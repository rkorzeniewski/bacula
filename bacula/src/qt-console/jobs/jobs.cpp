/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2007-2008 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from
   many others, a complete list can be found in the file AUTHORS.
   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version two of the GNU General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

   Bacula® is a registered trademark of John Walker.
   The licensor of Bacula is the Free Software Foundation Europe
   (FSFE), Fiduciary Program, Sumatrastrasse 25, 8006 Zürich,
   Switzerland, email:ftf@fsfeurope.org.
*/
 
/*
 *   Version $Id$
 *
 *  Jobs Class
 *
 *   Dirk Bartley, March 2007
 *
 */ 

#include "bat.h"
#include "jobs/jobs.h"
#include "run/run.h"

Jobs::Jobs()
{
   setupUi(this);
   m_name = tr("Jobs");
   pgInitialize();
   QTreeWidgetItem* thisitem = mainWin->getFromHash(this);
   thisitem->setIcon(0,QIcon(QString::fromUtf8(":images/run.png")));

   /* tableWidget, Storage Tree Tree Widget inherited from ui_client.h */
   m_populated = false;
   m_checkcurwidget = true;
   m_closeable = false;
   /* add context sensitive menu items specific to this classto the page
    * selector tree. m_contextActions is QList of QActions */
   m_contextActions.append(actionRefreshJobs);
   createContextMenu();
   dockPage();
}

Jobs::~Jobs()
{
}

/*
 * The main meat of the class!!  The function that querries the director and 
 * creates the widgets with appropriate values.
 */
void Jobs::populateTable()
{
   QTableWidgetItem *tableItem;
   QBrush blackBrush(Qt::black);

   if (!m_console->preventInUseConnect())
      return;
   m_checkcurwidget = false;
   tableWidget->clear();
   m_checkcurwidget = true;
   QStringList headerlist = (QStringList() << tr("Job Name") << tr("Pool") << tr("Messages")
      << tr("Client") << tr("Storage") << tr("Where") << tr("Level") << tr("Type") 
      << tr("FileSet") 
      << tr("Catalog") << tr("Enabled"));

   m_typeIndex = headerlist.indexOf(tr("Type"));

   tableWidget->setColumnCount(headerlist.count());
   tableWidget->setHorizontalHeaderLabels(headerlist);
   tableWidget->setRowCount(m_console->job_list.count());
   tableWidget->verticalHeader()->hide();
   int row = 0;


   foreach (QString jobName, m_console->job_list){
      job_defaults job_defs;
      job_defs.job_name = jobName;
      if (m_console->get_job_defaults(job_defs)) {

        for (int column=0; column<headerlist.count(); column++) {
            tableItem = new QTableWidgetItem(1);
            if (column == 0) 
               tableItem->setText(job_defs.job_name);
            if (column == 1) 
               tableItem->setText(job_defs.pool_name);
            if (column == 2) 
               tableItem->setText(job_defs.messages_name);
            if (column == 3) 
               tableItem->setText(job_defs.client_name);
            if (column == 4) 
               tableItem->setText(job_defs.store_name);
            if (column == 5) 
               tableItem->setText(job_defs.where);
            if (column == 6) 
               tableItem->setText(job_defs.level);
            if (column == 7) 
               tableItem->setText(job_defs.type);
            if (column == 8) 
               tableItem->setText(job_defs.fileset_name);
            if (column == 9) 
               tableItem->setText(job_defs.catalog_name);
            if (column == 10) 
               if (job_defs.enabled)
                  tableItem->setText("Yes");
               else
                  tableItem->setText("No");
            /* tableItem->setFlags(Qt::ItemIsSelectable); */
            tableItem->setForeground(blackBrush);
            tableWidget->setItem(row, column, tableItem);
         }
      }
      row++;
   }
   /* Resize the columns */
   for(int cnter=0; cnter<headerlist.size(); cnter++) {
      tableWidget->resizeColumnToContents(cnter);
   }
}

/*
 * When the treeWidgetItem in the page selector tree is singleclicked, Make sure
 * The tree has been populated.
 */
void Jobs::PgSeltreeWidgetClicked()
{
   if(!m_populated) {
      populateTable();
      m_populated=true;
   }
}

/*
 * Added to set the context menu policy based on currently active tableWidgetItem
 * signaled by currentItemChanged
 */
void Jobs::tableItemChanged(QTableWidgetItem *currentwidgetitem, QTableWidgetItem *previouswidgetitem )
{
   /* m_checkcurwidget checks to see if this is during a refresh, which will segfault */
   if (m_checkcurwidget) {
      /* The Previous item */
      if (previouswidgetitem) { /* avoid a segfault if first time */
         foreach(QAction* jobAction, tableWidget->actions()) {
            tableWidget->removeAction(jobAction);
         }
      }
      int currentRow = currentwidgetitem->row();
      QTableWidgetItem *currentrowzeroitem = tableWidget->item(currentRow, 0);
      m_currentlyselected = currentrowzeroitem->text();
      QTableWidgetItem *currenttypeitem = tableWidget->item(currentRow, m_typeIndex);
      QString type = currenttypeitem->text();

      if (m_currentlyselected.length() != 0) {
         /* set a hold variable to the client name in case the context sensitive
          * menu is used */
         tableWidget->addAction(actionConsoleListFiles);
         tableWidget->addAction(actionConsoleListVolumes);
         tableWidget->addAction(actionConsoleListNextVolume);
         tableWidget->addAction(actionConsoleEnableJob);
         tableWidget->addAction(actionConsoleDisableJob);
         tableWidget->addAction(actionConsoleCancel);
         tableWidget->addAction(actionJobListQuery);
         if (type == tr("Backup"))
            tableWidget->addAction(actionRunJob);
      }
   }
}

/* 
 * Setup a context menu 
 * Made separate from populate so that it would not create context menu over and
 * over as the table is repopulated.
 */
void Jobs::createContextMenu()
{
   tableWidget->setContextMenuPolicy(Qt::ActionsContextMenu);
   tableWidget->addAction(actionRefreshJobs);
   connect(tableWidget, SIGNAL(
           currentItemChanged(QTableWidgetItem *, QTableWidgetItem *)),
           this, SLOT(tableItemChanged(QTableWidgetItem *, QTableWidgetItem *)));
   /* connect to the action specific to this pages class */
   connect(actionRefreshJobs, SIGNAL(triggered()), this,
                SLOT(populateTable()));
   connect(actionConsoleListFiles, SIGNAL(triggered()), this, SLOT(consoleListFiles()));
   connect(actionConsoleListVolumes, SIGNAL(triggered()), this, SLOT(consoleListVolume()));
   connect(actionConsoleListNextVolume, SIGNAL(triggered()), this, SLOT(consoleListNextVolume()));
   connect(actionConsoleEnableJob, SIGNAL(triggered()), this, SLOT(consoleEnable()));
   connect(actionConsoleDisableJob, SIGNAL(triggered()), this, SLOT(consoleDisable()));
   connect(actionConsoleCancel, SIGNAL(triggered()), this, SLOT(consoleCancel()));
   connect(actionJobListQuery, SIGNAL(triggered()), this, SLOT(listJobs()));
   connect(actionRunJob, SIGNAL(triggered()), this, SLOT(runJob()));
}

/*
 * Virtual function which is called when this page is visible on the stack
 */
void Jobs::currentStackItem()
{
   populateTable();
   if(!m_populated) {
      /* Create the context menu for the client table */
      m_populated=true;
   }
}

/*
 * The following functions are slots responding to users clicking on the context
 * sensitive menu
 */

void Jobs::consoleListFiles()
{
   QString cmd = "list files job=\"" + m_currentlyselected + "\"";
   if (mainWin->m_longList) { cmd.prepend("l"); }
   consoleCommand(cmd);
}

void Jobs::consoleListVolume()
{
   QString cmd = "list volumes job=\"" + m_currentlyselected + "\"";
   if (mainWin->m_longList) { cmd.prepend("l"); }
   consoleCommand(cmd);
}

void Jobs::consoleListNextVolume()
{
   QString cmd = "list nextvolume job=\"" + m_currentlyselected + "\"";
   if (mainWin->m_longList) { cmd.prepend("l"); }
   consoleCommand(cmd);
}

void Jobs::consoleEnable()
{
   QString cmd = "enable job=\"" + m_currentlyselected + "\"";
   consoleCommand(cmd);
}

void Jobs::consoleDisable()
{
   QString cmd = "disable job=\"" + m_currentlyselected + "\"";
   consoleCommand(cmd);
}

void Jobs::consoleCancel()
{
   QString cmd = "cancel job=\"" + m_currentlyselected + "\"";
   consoleCommand(cmd);
}

void Jobs::listJobs()
{
   QTreeWidgetItem *parentItem = mainWin->getFromHash(this);
   mainWin->createPageJobList("", "", m_currentlyselected, "", parentItem);
}

/*
 * Open a new job run page with the currentley selected "Backup" job 
 * defaulted In
 */
void Jobs::runJob()
{
   new runPage(m_currentlyselected);
}
