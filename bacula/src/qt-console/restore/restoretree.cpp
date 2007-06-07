/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2007-2007 Free Software Foundation Europe e.V.

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
 *   Version $Id: restore.cpp 4945 2007-05-31 01:24:28Z bartleyd2 $
 *
 *  Restore Class 
 *
 *   Kern Sibbald, February MMVII
 *
 */ 

#include "bat.h"
#include "restoretree.h"
#include "pages.h"

restoreTree::restoreTree()
{
   setupUi(this);
   m_name = "Version Browser";
   pgInitialize();
   QTreeWidgetItem* thisitem = mainWin->getFromHash(this);
   thisitem->setIcon(0,QIcon(QString::fromUtf8(":images/browse.png")));

   m_closeable = true;
   m_populated = false;

   readSettings();
   dockPage();
   m_winRegExpDrive.setPattern("^[a-z]:/$");
   m_winRegExpPath.setPattern("^[a-z]:/");
   m_slashregex.setPattern("/");
   m_debugCnt = 0;
   m_debugTrap = true;
}

restoreTree::~restoreTree()
{
   writeSettings();
}

/*
 * Called from the constructor to set up the page widgets and connections.
 */
void restoreTree::setupPage()
{
   connect(refreshButton, SIGNAL(pressed()), this, SLOT(refreshButtonPushed()));
   connect(jobCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(jobComboChanged(int)));
   connect(directoryTree, SIGNAL(currentItemChanged(QTreeWidgetItem *, QTreeWidgetItem *)),
           this, SLOT(directoryItemChanged(QTreeWidgetItem *, QTreeWidgetItem *)));
   connect(fileTable, SIGNAL(currentItemChanged(QTableWidgetItem *, QTableWidgetItem *)),
           this, SLOT(fileItemChanged(QTableWidgetItem *, QTableWidgetItem *)));
   connect(directoryTree, SIGNAL(itemExpanded(QTreeWidgetItem *)),
           this, SLOT(directoryItemExpanded(QTreeWidgetItem *)));

   QStringList titles;
   titles << "Directories";
   directoryTree->setHeaderLabels(titles);
   clientCombo->addItems(m_console->client_list);
   fileSetCombo->addItem("Any");
   fileSetCombo->addItems(m_console->fileset_list);
   jobCombo->addItems(m_console->job_list);
}

/*
 * When refresh button is pushed, perform a query getting the directories and
 * use parseDirectory and addDirectory to populate the directory tree with items.
 */
void restoreTree::populateDirectoryTree()
{
   m_slashTrap = false;
   m_dirPaths.clear();
   directoryTree->clear();
   fileTable->clear();
   versionTable->clear();
   QString cmd =
      "SELECT DISTINCT Path.Path FROM Path"
      " LEFT OUTER JOIN File ON (File.PathId=Path.PathId)"
      " LEFT OUTER JOIN Job ON (File.JobId=Job.JobId)"
      " LEFT OUTER JOIN Client ON (Job.ClientId=Client.ClientId)"
      " LEFT OUTER JOIN FileSet ON (Job.FileSetId=FileSet.FileSetId) WHERE";
   m_condition = " Job.name = '" + jobCombo->itemText(jobCombo->currentIndex()) + "'";
   int clientIndex = clientCombo->currentIndex();
   if ((clientIndex >= 0) && (clientCombo->itemText(clientIndex) != "Any")) {
      m_condition.append(" AND Client.Name='" + clientCombo->itemText(clientIndex) + "'");
   }
   int fileSetIndex = fileSetCombo->currentIndex();
   if ((fileSetIndex >= 0) && (fileSetCombo->itemText(fileSetIndex) != "Any")) {
      m_condition.append(" AND FileSet.FileSet='" + fileSetCombo->itemText(fileSetIndex) + "'");
   }
   cmd += m_condition;
   if (mainWin->m_sqlDebug) {
      Pmsg1(000, "Query cmd : %s\n",cmd.toUtf8().data());
   }
   QStringList directories;
   if (m_console->sql_cmd(cmd, directories)) {
      if (mainWin->m_miscDebug) {
         Pmsg1(000, "Done with query %i directories\n", directories.count());
      }
      foreach(QString directory, directories) {
         m_debugCnt += 1;
         parseDirectory(directory);
      }
   }
}

/*
 * Function to parse a directory into all possible subdirectories, then add to
 * The tree.
 */
void restoreTree::parseDirectory(QString &dir_in)
{
   if (m_debugCnt > 2)
      m_debugTrap = false;
   /* Clean up the directory string remove some funny char after last '/' */
   QRegExp rgx("[^/]$");
   int lastslash = rgx.indexIn(dir_in);
   dir_in.replace(lastslash, dir_in.length()-lastslash, "");
   if ((mainWin->m_miscDebug) && (m_debugTrap))
      Pmsg1(000, "parsing %s\n", dir_in.toUtf8().data());

   /* split and add if not in yet */
   QString direct, path;
   int index;
   bool done = false;
   QStringList pathAfter, dirAfter;
   /* start from the end, turn /etc/somedir/subdir/ into /etc/somedir and subdir/ 
    * if not added into tree, then try /etc/ and somedir/ if not added, then try
    * / and etc/ .  That should succeed, then add the ones that failed in reverse */
   while (((index = m_slashregex.lastIndexIn(dir_in, -2)) != -1) && (!done)) {
      direct = path = dir_in;
      path.replace(index+1,dir_in.length()-index-1,"");
      direct.replace(0,index+1,"");
      if ((mainWin->m_miscDebug) && (m_debugTrap)) {
         QString msg = QString("length = \"%1\" index = \"%2\" Adding \"%3\" \"%4\"\n")
                    .arg(dir_in.length())
                    .arg(index)
                    .arg(path)
                    .arg(direct);
         Pmsg0(000, msg.toUtf8().data());
      }
      if (addDirectory(path, direct)) done = true;
      else {
         if ((mainWin->m_miscDebug) && (m_debugTrap))
            Pmsg0(000, "Saving for later\n");
         pathAfter.prepend(path);
         dirAfter.prepend(direct);
      }
      dir_in = path;
   }
   for (int k=0; k<pathAfter.count(); k++) {
      if (addDirectory(pathAfter[k], dirAfter[k]))
         if ((mainWin->m_miscDebug) && (m_debugTrap))
            Pmsg2(000, "Adding After %s %s\n", pathAfter[k].toUtf8().data(), dirAfter[k].toUtf8().data());
      else
         if ((mainWin->m_miscDebug) && (m_debugTrap))
            Pmsg2(000, "Error Adding %s %s\n", pathAfter[k].toUtf8().data(), dirAfter[k].toUtf8().data());
   }
}

/*
 * Function called from fill directory when a directory is found to see if this
 * directory exists in the directory pane and then add it to the directory pane
 */
bool restoreTree::addDirectory(QString &m_cwd, QString &newdirr)
{
   QString newdir = newdirr;
   QString fullpath = m_cwd + newdirr;
   bool ok = true, added = false;

   if ((mainWin->m_miscDebug) && (m_debugTrap)) {
      QString msg = QString("In addDirectory cwd \"%1\" newdir \"%2\"\n")
                    .arg(m_cwd)
                    .arg(newdir);
      Pmsg0(000, msg.toUtf8().data());
   }

   if (!m_slashTrap) {
      /* add unix '/' directory first */
      if (m_dirPaths.empty() && (m_winRegExpPath.indexIn(fullpath,0) == -1)) {
         m_slashTrap = true;
         QTreeWidgetItem *item = new QTreeWidgetItem(directoryTree);
         item->setIcon(0, QIcon(QString::fromUtf8(":images/folder.png")));
         QString text("/");
         item->setText(0, text.toUtf8().data());
         item->setData(0, Qt::UserRole, QVariant(text));
         if ((mainWin->m_miscDebug) && (m_debugTrap)) {
            Pmsg1(000, "Pre Inserting %s\n",text.toUtf8().data());
         }
         m_dirPaths.insert(text, item);
      }
      /* no need to check for windows drive if unix */
      if (m_winRegExpDrive.indexIn(m_cwd, 0) == 0) {
         /* this is a windows drive add the base widget */
         QTreeWidgetItem *item = item = new QTreeWidgetItem(directoryTree);
         item->setIcon(0, QIcon(QString::fromUtf8(":images/folder.png")));
         item->setText(0, m_cwd);
         item->setData(0, Qt::UserRole, QVariant(fullpath));
         if ((mainWin->m_miscDebug) && (m_debugTrap)) {
            Pmsg0(000, "Added Base \"letter\":/\n");
         }
         m_dirPaths.insert(m_cwd, item);
      }
   }
 
   /* is it already existent ?? */
   if (!m_dirPaths.contains(fullpath)) {
      QTreeWidgetItem *item = NULL;
      QTreeWidgetItem *parent = m_dirPaths.value(m_cwd);
      if (parent) {
         /* new directories to add */
         item = new QTreeWidgetItem(parent);
         item->setText(0, newdir.toUtf8().data());
         item->setData(0, Qt::UserRole, QVariant(fullpath));
      } else {
         ok = false;
         if ((mainWin->m_miscDebug) && (m_debugTrap)) {
            QString msg = QString("In else of if parent cwd \"%1\" newdir \"%2\"\n")
                 .arg(m_cwd)
                 .arg(newdir);
            Pmsg0(000, msg.toUtf8().data());
         }
      }
      /* insert into hash */
      if (ok) {
         if ((mainWin->m_miscDebug) && (m_debugTrap)) {
            Pmsg1(000, "Inserting %s\n",fullpath.toUtf8().data());
         }
         m_dirPaths.insert(fullpath, item);
         added = true;
      }
   }
   return added;
}

/*
 * Virtual function which is called when this page is visible on the stack
 */
void restoreTree::currentStackItem()
{
   if(!m_populated) {
      if (!m_console->preventInUseConnect())
         return;
      setupPage();
      m_populated=true;
   }
}

/*
 * Populate the tree when refresh button pushed.
 */
void restoreTree::refreshButtonPushed()
{
   populateDirectoryTree();
}

/*
 * Set the values of non-job combo boxes to the job defaults
 */
void restoreTree::jobComboChanged(int)
{
   job_defaults job_defs;

   (void)index;
   job_defs.job_name = jobCombo->currentText();
   if (m_console->get_job_defaults(job_defs)) {
      fileSetCombo->setCurrentIndex(fileSetCombo->findText(job_defs.fileset_name, Qt::MatchExactly));
      clientCombo->setCurrentIndex(clientCombo->findText(job_defs.client_name, Qt::MatchExactly));
   }
}

/*
 * Function to populate the file list table
 */
void restoreTree::directoryItemChanged(QTreeWidgetItem *item, QTreeWidgetItem *)
{
   if (item == NULL)
      return;
   QBrush blackBrush(Qt::black);
   QString directory = item->data(0,Qt::UserRole).toString();
   directoryLabel->setText("Present Working Directory : " + directory);
   QString cmd =
      "SELECT DISTINCT Filename.Name"
      " FROM File LEFT OUTER JOIN Filename on (Filename.FilenameId=File.FilenameId)"
      " LEFT OUTER JOIN Path ON (Path.PathId=File.PathId)"
      " LEFT OUTER JOIN Job ON (File.JobId=Job.JobId)"
      " LEFT OUTER JOIN Client ON (Job.ClientId=Client.ClientId)"
      " LEFT OUTER JOIN FileSet ON (Job.FileSetId=FileSet.FileSetId)";
   cmd += " WHERE Path.Path='" + directory + "' AND Filename.Name!='' AND " + m_condition;

   QStringList headerlist = (QStringList() << "File Name");
   fileTable->clear();
   /* Also clear the version table here */
   versionTable->clear();
   versionTable->setRowCount(0);
   versionTable->setColumnCount(0);
   fileTable->setColumnCount(headerlist.size());
   fileTable->setHorizontalHeaderLabels(headerlist);

   if (mainWin->m_sqlDebug) {
      Pmsg1(000, "Query cmd : %s\n",cmd.toUtf8().data());
   }
   QStringList results;
   if (m_console->sql_cmd(cmd, results)) {
      m_resultCount = results.count();
   
      QTableWidgetItem* tableItem;
      QString field;
      QStringList fieldlist;
      fileTable->setRowCount(results.size());

      int row = 0;
      /* Iterate through the record returned from the query */
      foreach (QString resultline, results) {
         /* Iterate through fields in the record */
         int column = 0;
         fieldlist = resultline.split("\t");
         foreach (field, fieldlist) {
            field = field.trimmed();  /* strip leading & trailing spaces */
            tableItem = new QTableWidgetItem(field,1);
            tableItem->setFlags(0);
            tableItem->setForeground(blackBrush);
            tableItem->setData(Qt::UserRole,QVariant(directory));
            fileTable->setItem(row, column, tableItem);
            column++;
         }
         row++;
      }
      fileTable->setRowCount(row);
   }
   fileTable->resizeColumnsToContents();
   fileTable->resizeRowsToContents();
   fileTable->verticalHeader()->hide();
}

/*
 * Function to populate the version table
 */
void restoreTree::fileItemChanged(QTableWidgetItem *fileTableItem, QTableWidgetItem *)
{
   if (fileTableItem == NULL)
      return;
   QString file = fileTableItem->text();
   QString directory = fileTableItem->data(Qt::UserRole).toString();

   QBrush blackBrush(Qt::black);
   QString cmd = 
      "SELECT File.FileId, Job.JobId, Job.EndTime, File.Md5 "
      " FROM File"
      " LEFT OUTER JOIN Filename on (Filename.FilenameId=File.FilenameId)"
      " LEFT OUTER JOIN Path ON (Path.PathId=File.PathId)"
      " LEFT OUTER JOIN Job ON (File.JobId=Job.JobId)"
      " LEFT OUTER JOIN Client ON (Job.ClientId=Client.ClientId)"
      " LEFT OUTER JOIN FileSet ON (Job.FileSetId=FileSet.FileSetId)";
   cmd += " WHERE Filename.Name='" + file + "' AND Path.Path='" + directory + "' AND " + m_condition;

   QStringList headerlist = (QStringList() << "File Id" << "Job Id" << "End Time" << "Md5");
   versionTable->clear();
   versionTable->setColumnCount(headerlist.size());
   versionTable->setHorizontalHeaderLabels(headerlist);

   if (mainWin->m_sqlDebug) {
      Pmsg1(000, "Query cmd : %s\n",cmd.toUtf8().data());
   }
   QStringList results;
   if (m_console->sql_cmd(cmd, results)) {
      m_resultCount = results.count();
   
      QTableWidgetItem* tableItem;
      QString field;
      QStringList fieldlist;
      versionTable->setRowCount(results.size());

      int row = 0;
      /* Iterate through the record returned from the query */
      foreach (QString resultline, results) {
         fieldlist = resultline.split("\t");
         int column = 0;
         /* remove directory */
         if (fieldlist[0].trimmed() != "") {
            /* Iterate through fields in the record */
            foreach (field, fieldlist) {
               field = field.trimmed();  /* strip leading & trailing spaces */
               tableItem = new QTableWidgetItem(field,1);
               tableItem->setFlags(0);
               tableItem->setForeground(blackBrush);
               tableItem->setData(Qt::UserRole,QVariant(directory));
               versionTable->setItem(row, column, tableItem);
               column++;
            }
            row++;
         }
      }
   }
   versionTable->resizeColumnsToContents();
   versionTable->resizeRowsToContents();
   versionTable->verticalHeader()->hide();
}

/*
 * Save user settings associated with this page
 */
void restoreTree::writeSettings()
{
   QSettings settings(m_console->m_dir->name(), "bat");
   settings.beginGroup("RestoreTree");
   settings.setValue("splitterSizes", splitter->saveState());
   settings.endGroup();
}

/*
 * Read and restore user settings associated with this page
 */
void restoreTree::readSettings()
{
   QSettings settings(m_console->m_dir->name(), "bat");
   settings.beginGroup("RestoreTree");
   splitter->restoreState(settings.value("splitterSizes").toByteArray());
   settings.endGroup();
}

/*
 * This is a funcion to accomplish the one thing I struggled to figure out what
 * was taking so long.  It add the icons, but after the tree is made.
 */
void restoreTree::directoryItemExpanded(QTreeWidgetItem *item)
{
   int childCount = item->childCount();
   for (int i=0; i<childCount; i++) {
      QTreeWidgetItem *child = item->child(i);
      child->setIcon(0,QIcon(QString::fromUtf8(":images/folder.png")));
   }
}
