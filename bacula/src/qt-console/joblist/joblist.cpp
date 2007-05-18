/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2000-2007 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from
   many others, a complete list can be found in the file AUTHORS.
   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version two of the GNU General Public
   License as published by the Free Software Foundation plus additions
   that are listed in the file LICENSE.

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
 *   Version $Id: joblist.h 4230 2007-02-21 20:07:37Z kerns $
 *
 *   Dirk Bartley, March 2007
 */
 
#include <QAbstractEventDispatcher>
#include <QTableWidgetItem>
#include "bat.h"
#include "joblist.h"
#include "restore.h"
#include "joblog/joblog.h"

/*
 * Constructor for the class
 */
JobList::JobList(QString &mediaName, QString &clientname,
         QTreeWidgetItem *parentTreeWidgetItem)
{
   setupUi(this);
   m_name = "Clients";
   m_mediaName = mediaName;
   m_clientName = clientname;
   pgInitialize(parentTreeWidgetItem);
   m_resultCount = 0;
   m_populated = false;
   m_closeable = false;
   if ((m_mediaName != "") || (m_clientName != "")) { m_closeable=true; }
   m_checkCurrentWidget = true;
   createConnections();

   /* Set Defaults for check and spin for limits */
   limitCheckBox->setCheckState(Qt::Checked);
   limitSpinBox->setValue(150);
   daysCheckBox->setCheckState(Qt::Unchecked);
   daysSpinBox->setValue(30);
}

/*
 * The Meat of the class.
 * This function will populate the QTableWidget, mp_tablewidget, with
 * QTableWidgetItems representing the results of a query for what jobs exist on
 * the media name passed from the constructor stored in m_mediaName.
 */
void JobList::populateTable()
{
   QStringList results;
   QString resultline;
   QBrush blackBrush(Qt::black);

   if (!m_console->preventInUseConnect())
       return;

   /* Can't do this in constructor because not neccesarily conected in constructor */
   if (!m_populated) {
      clientsComboBox->addItem("Any");
      clientsComboBox->addItems(m_console->client_list);
      int clientIndex = clientsComboBox->findText(m_clientName, Qt::MatchExactly);
      if (clientIndex != -1)
         clientsComboBox->setCurrentIndex(clientIndex);

      /* Not m_console->volume_list will query database */
      QString query("SELECT VolumeName AS Media FROM Media ORDER BY Media");
      QStringList results, volumeList;
      if (m_console->sql_cmd(query, results)) {
         QString field;
         QStringList fieldlist;
         /* Iterate through the lines of results. */
         foreach (QString resultline, results) {
            fieldlist = resultline.split("\t");
            volumeList.append(fieldlist[0]);
         } /* foreach resultline */
      } /* if results from query */
      volumeComboBox->addItem("Any");
      volumeComboBox->addItems(volumeList);
      int volumeIndex = volumeComboBox->findText(m_mediaName, Qt::MatchExactly);
      if (volumeIndex != -1) {
         volumeComboBox->setCurrentIndex(volumeIndex);
      }
      jobComboBox->addItem("Any");
      jobComboBox->addItems(m_console->job_list);
      levelComboBox->addItem("Any");
      levelComboBox->addItems( QStringList() << "F" << "D" << "I");
      purgedComboBox->addItem("Any");
      purgedComboBox->addItems( QStringList() << "0" << "1");
      statusComboBox->addItem("Any");
      QString statusQuery("SELECT JobStatusLong FROM Status");
      QStringList statusResults, statusLongList;
      if (m_console->sql_cmd(statusQuery, statusResults)) {
         QString field;
         QStringList fieldlist;
         /* Iterate through the lines of results. */
         foreach (QString resultline, statusResults) {
            fieldlist = resultline.split("\t");
            statusLongList.append(fieldlist[0]);
         } /* foreach resultline */
      } /* if results from statusquery */
      statusComboBox->addItems(statusLongList);
   }

   /* Set up query */
   QString query("");
   int volumeIndex = volumeComboBox->currentIndex();
   if (volumeIndex != -1)
      m_mediaName = volumeComboBox->itemText(volumeIndex);
   query += "SELECT DISTINCT Job.Jobid AS Id, Job.Name AS JobName, Client.Name AS Client,"
            " Job.Starttime AS JobStart, Job.Type AS JobType,"
            " Job.Level AS BackupLevel, Job.Jobfiles AS FileCount,"
            " Job.JobBytes AS Bytes,"
            " Job.JobStatus AS Status, Status.JobStatusLong AS StatusLong,"
            " Job.PurgedFiles AS Purged"
            " FROM Job,Client,Status";
   if (m_mediaName != "Any") {
      query += ",JobMedia,Media";
   }
   query += " WHERE Client.ClientId=Job.ClientId AND Job.JobStatus=Status.JobStatus";
   if (m_mediaName != "Any") {
      query += " AND JobMedia.JobId=Job.JobId AND JobMedia.MediaId=Media.MediaId"
               " AND Media.VolumeName='" + m_mediaName + "'";
   }
   int clientIndex = clientsComboBox->currentIndex();
   if (clientIndex != -1)
      m_clientName = clientsComboBox->itemText(clientIndex);
   if (m_clientName != "Any") {
      query += " AND Client.Name='" + m_clientName + "'";
   }
   int jobIndex = jobComboBox->currentIndex();
   if ((jobIndex != -1) && (jobComboBox->itemText(jobIndex) != "Any")) {
      query += " AND Job.Name='" + jobComboBox->itemText(jobIndex) + "'";
   }
   int levelIndex = levelComboBox->currentIndex();
   if ((levelIndex != -1) && (levelComboBox->itemText(levelIndex) != "Any")) {
      query += " AND Job.Level='" + levelComboBox->itemText(levelIndex) + "'";
   }
   int statusIndex = statusComboBox->currentIndex();
   if ((statusIndex != -1) && (statusComboBox->itemText(statusIndex) != "Any")) {
      query += " AND Status.JobStatusLong='" + statusComboBox->itemText(statusIndex) + "'";
   }
   int purgedIndex = purgedComboBox->currentIndex();
   if ((purgedIndex != -1) && (purgedComboBox->itemText(purgedIndex) != "Any")) {
      query += " AND Job.PurgedFiles='" + purgedComboBox->itemText(purgedIndex) + "'";
   }
   /* If Limit check box For limit by days is checked  */
   if (daysCheckBox->checkState() == Qt::Checked) {
      QDateTime stamp = QDateTime::currentDateTime().addDays(-daysSpinBox->value());
      QString since = stamp.toString(Qt::ISODate);
      query += " AND Job.Starttime>'" + since + "'";
   }
   /* Descending */
   query += " ORDER BY Job.Starttime DESC, Job.JobId DESC";
   /* If Limit check box for limit records returned is checked  */
   if (limitCheckBox->checkState() == Qt::Checked) {
      QString limit;
      limit.setNum(limitSpinBox->value());
      query += " LIMIT " + limit;
   }

   /* Set up the Header for the table */
   QStringList headerlist = (QStringList()
      << "Job Id" << "Job Name" << "Client" << "Job Starttime" << "Job Type" 
      << "Job Level" << "Job Files" << "Job Bytes" << "Job Status"  << "Purged" );
   m_purgedIndex = headerlist.indexOf("Purged");
   m_typeIndex = headerlist.indexOf("Job Type");
   statusIndex = headerlist.indexOf("Job Status");

   /* Initialize the QTableWidget */
   m_checkCurrentWidget = false;
   mp_tableWidget->clear();
   m_checkCurrentWidget = true;
   mp_tableWidget->setColumnCount(headerlist.size());
   mp_tableWidget->setHorizontalHeaderLabels(headerlist);

   /*  This could be a user preference debug message?? */
   //printf("Query cmd : %s\n",query.toUtf8().data());
   if (m_console->sql_cmd(query, results)) {
      m_resultCount = results.count();

      QTableWidgetItem* p_tableitem;
      QString field;
      QStringList fieldlist;
      mp_tableWidget->setRowCount(results.size());

      int row = 0;
      /* Iterate through the record returned from the query */
      foreach (resultline, results) {
         fieldlist = resultline.split("\t");
         int column = 0;
         bool statusIndexDone = false;
         QString statusCode("");
         /* Iterate through fields in the record */
         foreach (field, fieldlist) {
            field = field.trimmed();  /* strip leading & trailing spaces */
            if ((column == statusIndex) && (!statusIndexDone)){
               statusIndexDone = true;
               statusCode = field;
            } else {
               p_tableitem = new QTableWidgetItem(field,1);
               p_tableitem->setFlags(0);
               p_tableitem->setForeground(blackBrush);
               mp_tableWidget->setItem(row, column, p_tableitem);
               if (column == statusIndex)
                  setStatusColor(p_tableitem, statusCode);
               column++;
            }
         }
         row++;
      }
   } 
   /* Resize the columns */
   mp_tableWidget->resizeColumnsToContents();
   mp_tableWidget->resizeRowsToContents();
   mp_tableWidget->verticalHeader()->hide();
   if ((m_mediaName != "Any") && (m_resultCount == 0)){
      /* for context sensitive searches, let the user know if there were no
       * results */
      QMessageBox::warning(this, tr("Bat"),
          tr("The Jobs query returned no results.\n"
         "Press OK to continue?"), QMessageBox::Ok );
   }
}

void JobList::setStatusColor(QTableWidgetItem *item, QString &field)
{
   QString greenchars("TCR");
   QString redchars("BEf");
   QString yellowchars("eDAFSMmsjdctp");
   if (greenchars.contains(field, Qt::CaseSensitive)) {
      item->setBackground(Qt::green);
   } else if (redchars.contains(field, Qt::CaseSensitive)) {
      item->setBackground(Qt::red);
   } else if (yellowchars.contains(field, Qt::CaseSensitive)){ 
      item->setBackground(Qt::yellow);
   }
}

/*
 * When the treeWidgetItem in the page selector tree is singleclicked, Make sure
 * The tree has been populated.
 */
void JobList::PgSeltreeWidgetClicked()
{
   populateTable();
   if (!m_populated) {
      m_populated=true;
   }
}

/*
 *  Virtual function override of pages function which is called when this page
 *  is visible on the stack
 */
void JobList::currentStackItem()
{
   if (!m_populated) {
      populateTable();
      m_contextActions.append(actionRefreshJobList);
      m_populated=true;
   }
}

/*
 * Virtual Function to return the name for the medialist tree widget
 */
void JobList::treeWidgetName(QString &desc)
{
   if ((m_mediaName == "") && (m_clientName == "")) {
      desc = "Jobs";
   } else {
      desc = "Jobs ";
      if (m_mediaName != "" ) {
         desc += "on Volume " + m_mediaName;
      }
      if (m_clientName != "" ) {
         desc += "of Client " + m_clientName;
      }
   }
}

/*
 * This functions much line tableItemChanged for trees like the page selector,
 * but I will do much less here
 */
void JobList::tableItemChanged(QTableWidgetItem *currentItem, QTableWidgetItem * /*previousItem*/)
{
   if (m_checkCurrentWidget) {
      int row = currentItem->row();
      QTableWidgetItem* jobitem = mp_tableWidget->item(row, 0);
      m_currentJob = jobitem->text();
      jobitem = mp_tableWidget->item(row, m_purgedIndex);
      QString purged = jobitem->text();
      mp_tableWidget->removeAction(actionPurgeFiles);
      if (purged == "0") {
         mp_tableWidget->addAction(actionPurgeFiles);
      }
      jobitem = mp_tableWidget->item(row, m_typeIndex);
      QString status = jobitem->text();
      mp_tableWidget->removeAction(actionRestoreFromJob);
      mp_tableWidget->removeAction(actionRestoreFromTime);
      if (status == "B") {
         mp_tableWidget->addAction(actionRestoreFromJob);
         mp_tableWidget->addAction(actionRestoreFromTime);
      }
   }
}

/*
 * Function to create connections for context sensitive menu for this and
 * the page selector
 */
void JobList::createConnections()
{
   /* connect to the action specific to this pages class that shows up in the 
    * page selector tree */
   connect(actionRefreshJobList, SIGNAL(triggered()), this,
                SLOT(populateTable()));
   connect(refreshButton, SIGNAL(pressed()), this, SLOT(populateTable()));
   /* for the tableItemChanged to maintain m_currentJob */
   connect(mp_tableWidget, SIGNAL(
           currentItemChanged(QTableWidgetItem *, QTableWidgetItem *)),
           this, SLOT(tableItemChanged(QTableWidgetItem *, QTableWidgetItem *)));

   /* Do what is required for the local context sensitive menu */


   /* setContextMenuPolicy is required */
   mp_tableWidget->setContextMenuPolicy(Qt::ActionsContextMenu);

   /* Add Actions */
   mp_tableWidget->addAction(actionRefreshJobList);
   mp_tableWidget->addAction(actionLongListJob);
   mp_tableWidget->addAction(actionListJobid);
   mp_tableWidget->addAction(actionListFilesOnJob);
   mp_tableWidget->addAction(actionListJobMedia);
   mp_tableWidget->addAction(actionListVolumes);
   mp_tableWidget->addAction(actionDeleteJob);
   mp_tableWidget->addAction(actionPurgeFiles);
   mp_tableWidget->addAction(actionRestoreFromJob);
   mp_tableWidget->addAction(actionRestoreFromTime);
   mp_tableWidget->addAction(actionShowLogForJob);

   /* Make Connections */
   connect(actionLongListJob, SIGNAL(triggered()), this,
                SLOT(consoleLongListJob()));
   connect(actionListJobid, SIGNAL(triggered()), this,
                SLOT(consoleListJobid()));
   connect(actionListFilesOnJob, SIGNAL(triggered()), this,
                SLOT(consoleListFilesOnJob()));
   connect(actionListJobMedia, SIGNAL(triggered()), this,
                SLOT(consoleListJobMedia()));
   connect(actionListVolumes, SIGNAL(triggered()), this,
                SLOT(consoleListVolumes()));
   connect(actionDeleteJob, SIGNAL(triggered()), this,
                SLOT(consoleDeleteJob()));
   connect(actionPurgeFiles, SIGNAL(triggered()), this,
                SLOT(consolePurgeFiles()));
   connect(actionRestoreFromJob, SIGNAL(triggered()), this,
                SLOT(preRestoreFromJob()));
   connect(actionRestoreFromTime, SIGNAL(triggered()), this,
                SLOT(preRestoreFromTime()));
   connect(actionShowLogForJob, SIGNAL(triggered()), this,
                SLOT(showLogForJob()));
}

/*
 * Functions to respond to local context sensitive menu sending console commands
 * If I could figure out how to make these one function passing a string, Yaaaaaa
 */
void JobList::consoleLongListJob()
{
   QString cmd("llist jobid=");
   cmd += m_currentJob;
   consoleCommand(cmd);
}
void JobList::consoleListJobid()
{
   QString cmd("list jobid=");
   cmd += m_currentJob;
   consoleCommand(cmd);
}
void JobList::consoleListFilesOnJob()
{
   QString cmd("list files jobid=");
   cmd += m_currentJob;
   consoleCommand(cmd);
}
void JobList::consoleListJobMedia()
{
   QString cmd("list jobmedia jobid=");
   cmd += m_currentJob;
   consoleCommand(cmd);
}
void JobList::consoleListVolumes()
{
   QString cmd("list volumes jobid=");
   cmd += m_currentJob;
   consoleCommand(cmd);
}
void JobList::consoleDeleteJob()
{
   if (QMessageBox::warning(this, tr("Bat"),
      tr("Are you sure you want to delete??  !!!.\n"
"This delete command is used to delete a Job record and all associated catalog"
" records that were created. This command operates only on the Catalog"
" database and has no effect on the actual data written to a Volume. This"
" command can be dangerous and we strongly recommend that you do not use"
" it unless you know what you are doing.  The Job and all its associated"
" records (File and JobMedia) will be deleted from the catalog."
      "Press OK to proceed with delete operation.?"),
      QMessageBox::Ok | QMessageBox::Cancel)
      == QMessageBox::Cancel) { return; }

   QString cmd("delete job jobid=");
   cmd += m_currentJob;
   consoleCommand(cmd);
}
void JobList::consolePurgeFiles()
{
   if (QMessageBox::warning(this, tr("Bat"),
      tr("Are you sure you want to purge ??  !!!.\n"
"The Purge command will delete associated Catalog database records from Jobs and"
" Volumes without considering the retention period. Purge  works only on the"
" Catalog database and does not affect data written to Volumes. This command can"
" be dangerous because you can delete catalog records associated with current"
" backups of files, and we recommend that you do not use it unless you know what"
" you are doing.\n"
      "Press OK to proceed with the purge operation?"),
      QMessageBox::Ok | QMessageBox::Cancel)
      == QMessageBox::Cancel) { return; }

   QString cmd("purge files jobid=");
   cmd += m_currentJob;
   consoleCommand(cmd);
}

/*
 * Subroutine to call preRestore to restore from a select job
 */
void JobList::preRestoreFromJob()
{
   new prerestorePage(m_currentJob, R_JOBIDLIST);
}

/*
 * Subroutine to call preRestore to restore from a select job
 */
void JobList::preRestoreFromTime()
{
   new prerestorePage(m_currentJob, R_JOBDATETIME);
}

/*
 * Subroutine to call class to show the log in the database from that job
 */
void JobList::showLogForJob()
{
   QTreeWidgetItem* pageSelectorTreeWidgetItem = mainWin->getFromHash(this);
   new JobLog(m_currentJob, pageSelectorTreeWidgetItem);
}
