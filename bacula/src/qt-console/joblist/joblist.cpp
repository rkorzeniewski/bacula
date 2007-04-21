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


/*
 * Constructor for the class
 */
JobList::JobList(QString &medianame, QString &clientname)
{
   setupUi(this);
   m_medianame = medianame;
   m_clientname = clientname;
   pgInitialize();
   m_resultCount = 0;
   m_populated = false;
   m_closeable = false;
   /* connect to the action specific to this pages class */
   connect(actionRefreshJobList, SIGNAL(triggered()), this,
                SLOT(populateTable()));
}

/*
 * The Meat of the class.
 * This function will populate the QTableWidget, mp_tablewidget, with
 * QTableWidgetItems representing the results of a query for what jobs exist on
 * the media name passed from the constructor stored in m_medianame.
 */
void JobList::populateTable()
{
   QStringList results;
   QString resultline;

   /* Set up query QString and header QStringList */
   QString query("");
   query += "SELECT Job.Jobid AS Id, Job.Name AS JobName, Client.Name AS Client,"
            " Job.Starttime AS JobStart, Job.Type AS JobType,"
            " Job.Level AS BackupLevel, Job.Jobfiles AS FileCount,"
            " Job.JobBytes AS Bytes, Job.JobStatus AS Status"
            " FROM Job, JobMedia, Media, Client"
            " WHERE JobMedia.JobId=Job.JobId and JobMedia.MediaId=Media.MediaId"
            " and Client.ClientId=Job.ClientId";
   if (m_medianame != "") {
      query += " and Media.VolumeName='" + m_medianame + "'";
      m_closeable=true;
   }
   if (m_clientname != "") {
      query += " and Client.Name='" + m_clientname + "'";
      m_closeable=true;
   }
   query += " ORDER BY Job.Starttime";
   QStringList headerlist = (QStringList()
      << "Job Id" << "Job Name" << "Client" << "Job Starttime" << "Job Type" 
      << "Job Level" << "Job Files" << "Job Bytes" << "Job Status"  );

   /* Initialize the QTableWidget */
   mp_tableWidget->clear();
   mp_tableWidget->setColumnCount(headerlist.size());
   mp_tableWidget->setHorizontalHeaderLabels(headerlist);

    /*  This could be a user preference debug message?? */
    printf("Query cmd : %s\n",query.toUtf8().data());
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
         /* Iterate through fields in the record */
         foreach (field, fieldlist) {
            field = field.trimmed();  /* strip leading & trailing spaces */
            p_tableitem = new QTableWidgetItem(field,1);
            p_tableitem->setFlags(0);
            mp_tableWidget->setItem(row, column, p_tableitem);
            column++;
         }
         row++;
      }
   } 
   if ((m_medianame != "") && (m_resultCount == 0)){
      /* for context sensitive searches, let the user know if there were no
       * results */
      QMessageBox::warning(this, tr("Bat"),
          tr("The Jobs query returned no results.\n"
         "Press OK to continue?"), QMessageBox::Ok );
   }
}

/*
 * When the treeWidgetItem in the page selector tree is singleclicked, Make sure
 * The tree has been populated.
 */
void JobList::PgSeltreeWidgetClicked()
{
   if (!m_populated) {
      populateTable();
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
   if ((m_medianame == "") && (m_clientname == "")) {
      desc = "All Jobs";
   } else {
      desc = "Jobs ";
      if (m_medianame != "" ) {
         desc += "on Volume " + m_medianame;
      }
      if (m_clientname != "" ) {
         desc += "of Client " + m_clientname;
      }
   }
}
