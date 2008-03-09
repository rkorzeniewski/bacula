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
 *   Version $Id: clientstat.cpp 5880 2007-11-09 01:20:40Z bartleyd2 $
 *
 *   Dirk Bartley, March 2007
 */
 
#include <QAbstractEventDispatcher>
#include <QTableWidgetItem>
#include "bat.h"
#include "clientstat.h"

/*
 *       .status client=XXX header
 *       .status client=XXX running
 *       .status client=XXX terminated
 */

/*
 * Constructor for the class
 */
ClientStat::ClientStat(QString &client)
{
   m_client = client;
   setupUi(this);
   m_name = tr("Director Status");
   m_closeable = true;
   pgInitialize();
   QTreeWidgetItem* thisitem = mainWin->getFromHash(this);
   thisitem->setIcon(0,QIcon(QString::fromUtf8(":images/status.png")));
   m_cursor = new QTextCursor(textEdit->document());

   readSettings();
   dockPage();
   m_timer = new QTimer(this);
   QWidget::connect(m_timer, SIGNAL(timeout()), this, SLOT(timerTriggered()));
   m_timer->start(mainWin->m_refreshStatusDirInterval*1000);

   createConnections();
}

void ClientStat::getFont()
{
   QFont font = textEdit->font();

   QString dirname;
   m_console->getDirResName(dirname);
   QSettings settings(dirname, "bat");
   settings.beginGroup("Console");
   font.setFamily(settings.value("consoleFont", "Courier").value<QString>());
   font.setPointSize(settings.value("consolePointSize", 10).toInt());
   font.setFixedPitch(settings.value("consoleFixedPitch", true).toBool());
   settings.endGroup();
   textEdit->setFont(font);
}

/*
 * Write the m_splitter settings in the destructor
 */
ClientStat::~ClientStat()
{
   writeSettings();
}

/*
 * Populate all tables and header widgets
 */
void ClientStat::populateAll()
{
   if (!m_console->preventInUseConnect())
       return;
   populateHeader();
   populateTerminated();
   populateRunning();
}

/*
 *  Timer is triggered, see if is current and repopulate.
 */
void ClientStat::timerTriggered()
{
   bool iscurrent = mainWin->stackedWidget->currentIndex() == mainWin->stackedWidget->indexOf(this);
   if (((isDocked() && iscurrent) || (!isDocked())) && mainWin->m_refreshStatusDir) {
      if (m_console->is_ready())
         populateAll();
   }
}

/*
 * Populate header text widget
 */
void ClientStat::populateHeader()
{
   QString command = QString(".status client=\"" + m_client + "\" header");
   QStringList results;
   textEdit->clear();

   if (m_console->dir_cmd(command, results)) {
      foreach (QString line, results) {
         line += "\n";
         textEdit->insertPlainText(line);
      }
   }
}

/*
 * Populate teminated table
 */
void ClientStat::populateTerminated()
{
   QString command = QString(".status client=\"" + m_client + "\" terminated");
   QStringList results;
   QBrush blackBrush(Qt::black);

   terminatedTable->clear();
   QStringList headerlist = (QStringList()
      << tr("Job Id") << tr("Job Level") << tr("Job Files")
      << tr("Job Bytes") << tr("Job Status") << tr("Job Time")
      << tr("Job Name"));
   QStringList flaglist = (QStringList()
      << "R" << "L" << "R" << "R" << "LC"
      << "L" << "L");

   terminatedTable->setColumnCount(headerlist.size());
   terminatedTable->setHorizontalHeaderLabels(headerlist);

   if (m_console->dir_cmd(command, results)) {
      int row = 0;
      QTableWidgetItem* p_tableitem;
      terminatedTable->setRowCount(results.size());
      foreach (QString line, results) {
         /* Iterate through the record returned from the query */
         QStringList fieldlist = line.split("\t");
         int column = 0;
         QString statusCode("");
         /* Iterate through fields in the record */
         foreach (QString field, fieldlist) {
            field = field.trimmed();  /* strip leading & trailing spaces */
            p_tableitem = new QTableWidgetItem(field, 1);
            p_tableitem->setForeground(blackBrush);
            p_tableitem->setFlags(0);
            if (flaglist[column].contains("R"))
               p_tableitem->setTextAlignment(Qt::AlignRight);
            if (flaglist[column].contains("C"))
               if (field == "OK")
                  p_tableitem->setBackground(Qt::green);
               else
                  p_tableitem->setBackground(Qt::red);
            terminatedTable->setItem(row, column, p_tableitem);
            column += 1;
         }
         row += 1;
      }
   }
   terminatedTable->resizeColumnsToContents();
   terminatedTable->resizeRowsToContents();
   terminatedTable->verticalHeader()->hide();
}

/*
 * Populate running table
 */
void ClientStat::populateRunning()
{
   QString command = QString(".status client=\"" + m_client + "\" running");
   Pmsg1(000, "Clients running cmd : %s\n",command.toUtf8().data());
   QStringList results;
   QBrush blackBrush(Qt::black);

   runningTable->clear();
   QStringList headerlist = (QStringList()
      << tr("Job Id") << tr("Job Level") << tr("Job Data") << tr("Job Info"));

   runningTable->setColumnCount(headerlist.size());
   runningTable->setHorizontalHeaderLabels(headerlist);

   if (m_console->dir_cmd(command, results)) {
      int row = 0;
      QTableWidgetItem* p_tableitem;
      runningTable->setRowCount(results.size());
      foreach (QString line, results) {
         /* Iterate through the record returned from the query */
         QStringList fieldlist = line.split("\t");
         int column = 0;
         QString statusCode("");
         /* Iterate through fields in the record */
         foreach (QString field, fieldlist) {
            field = field.trimmed();  /* strip leading & trailing spaces */
            p_tableitem = new QTableWidgetItem(field, 1);
            p_tableitem->setForeground(blackBrush);
            p_tableitem->setFlags(0);
            runningTable->setItem(row, column, p_tableitem);
            column += 1;
         }
         row += 1;
      }
   }
   runningTable->resizeColumnsToContents();
   runningTable->resizeRowsToContents();
   runningTable->verticalHeader()->hide();
}

/*
 * When the treeWidgetItem in the page selector tree is singleclicked, Make sure
 * The tree has been populated.
 */
void ClientStat::PgSeltreeWidgetClicked()
{
   if (!m_populated) {
      populateAll();
      m_populated=true;
   }
}

/*
 *  Virtual function override of pages function which is called when this page
 *  is visible on the stack
 */
void ClientStat::currentStackItem()
{
   populateAll();
   if (!m_populated) {
      m_populated=true;
   }
}

/*
 * Function to create connections for context sensitive menu for this and
 * the page selector
 */
void ClientStat::createConnections()
{
   connect(actionRefresh, SIGNAL(triggered()), this,
                   SLOT(populateAll()));
   connect(actionCancelRunning, SIGNAL(triggered()), this,
                   SLOT(consoleCancelJob()));

   runningTable->setContextMenuPolicy(Qt::ActionsContextMenu);
   runningTable->addAction(actionRefresh);
   runningTable->addAction(actionCancelRunning);
}

/*
 * Save user settings associated with this page
 */
void ClientStat::writeSettings()
{
   QSettings settings(m_console->m_dir->name(), "bat");
   settings.beginGroup(m_groupText);
   settings.setValue(m_splitText, splitter->saveState());
   settings.endGroup();
}

/*
 * Read and restore user settings associated with this page
 */
void ClientStat::readSettings()
{
   m_groupText = "ClientStatPage";
   m_splitText = "splitterSizes_0";
   QSettings settings(m_console->m_dir->name(), "bat");
   settings.beginGroup(m_groupText);
   splitter->restoreState(settings.value(m_splitText).toByteArray());
   settings.endGroup();
}

/*
 * Cancel a running job
 */
void ClientStat::consoleCancelJob()
{
   int currentrow = runningTable->currentRow();
   QTableWidgetItem *item = runningTable->item(currentrow, 0);
   if (item) {
      QString text = item->text();
      QString cmd("cancel jobid=");
      cmd += text;
      consoleCommand(cmd);
   }
}
