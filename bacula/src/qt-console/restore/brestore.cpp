/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2007-2009 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from
   many others, a complete list can be found in the file AUTHORS.
   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

   Bacula® is a registered trademark of Kern Sibbald.
   The licensor of Bacula is the Free Software Foundation Europe
   (FSFE), Fiduciary Program, Sumatrastrasse 25, 8006 Zürich,
   Switzerland, email:ftf@fsfeurope.org.
*/
 
/*
 *   Version $Id$
 *
 *  bRestore Class  (Eric's brestore)
 *
 *   Kern Sibbald, January MMVII
 *
 */ 

#include "bat.h"
#include "restore.h"

bRestore::bRestore()
{
   m_name = tr("bRestore");
   m_client = "";
   setupUi(this);
   pgInitialize();
   QTreeWidgetItem* thisitem = mainWin->getFromHash(this);
   thisitem->setIcon(0, QIcon(QString::fromUtf8(":images/browse.png")));
   dockPage();
   m_populated = false;
}

void bRestore::setClient()
{
   Pmsg0(000, "Repopulating client table\n");
   // Select the same client, don't touch
   if (m_client == ClientList->currentText()) {
      return;
   }
   m_client = ClientList->currentText();
   FileList->clearContents();
   FileRevisions->clearContents();
   JobList->clear();
   JobList->setEnabled(true);
   LocationEntry->clear();

   if (ClientList->currentIndex() < 1) {
      return;
   }

   JobList->addItem("Job list for " + m_client);

   QString jobQuery =
      "SELECT Job.Jobid AS JobId, Job.StartTime AS StartTime,"
      " Job.Level AS Level,"
      " Job.Name AS Name"
      " FROM Job JOIN Client USING (ClientId)"
      " WHERE"
      " Job.JobStatus IN ('T','W') AND Job.Type='B' AND"
      " Client.Name='" + m_client + "' ORDER BY StartTime DESC" ;

   QString job;
   QStringList results;
   if (m_console->sql_cmd(jobQuery, results)) {
      QString field;
      QStringList fieldlist;

      /* Iterate through the record returned from the query */
      foreach (QString resultline, results) {
         fieldlist = resultline.split("\t");
         job = fieldlist[1] + " " + fieldlist[3] + "(" + fieldlist[2] + ")";
         JobList->addItem(job, QVariant(fieldlist[0]));
      }
   }
}


void bRestore::setJob()
{
   if (JobList->currentIndex() < 1) {
      return ;
   }
   QStringList results;
   QVariant tmp = JobList->itemData(JobList->currentIndex(), Qt::UserRole);
   m_jobids = tmp.toString(); 
   QString cmd = ".bvfs_update jobid=" + m_jobids;
   m_console->dir_cmd(cmd, results);
   displayFiles("/");
   Pmsg0(000, "update done\n");
}

void bRestore::displayFiles(uint64_t pathid)
{

}

void bRestore::displayFiles(QString path)
{
   QString q = ".bvfs_lsdir jobid=" + m_jobids + " path=" + path;
   QStringList results;
   if (m_console->dir_cmd(q, results)) {
      QString field;
      QStringList fieldlist;

      foreach (QString resultline, results) {
         fieldlist = resultline.split("\t");
      }
   }
}

void bRestore::PgSeltreeWidgetClicked()
{
   if(!m_populated) {
      setupPage();
   }
   if (!isOnceDocked()) {
      dockPage();
   }
}

void bRestore::setupPage()
{
   Pmsg0(000, "Setup page\n");
   ClientList->addItem("Client list");
   ClientList->addItems(m_console->client_list);
   connect(ClientList, SIGNAL(currentIndexChanged(int)), this, SLOT(setClient()));
   connect(JobList, SIGNAL(currentIndexChanged(int)), this, SLOT(setJob()));
   m_populated = true;
}

bRestore::~bRestore()
{
}
