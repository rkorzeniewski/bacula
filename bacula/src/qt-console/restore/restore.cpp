/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2007-2007 Free Software Foundation Europe e.V.

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
 *   Version $Id$
 *
 *  Restore Class 
 *
 *   Kern Sibbald, February MMVII
 *
 */ 

#include "bat.h"
#include "restore.h"

restoreDialog::restoreDialog(Console *console )
{
   QStringList titles;

   m_console = console;
   m_console->notify(false);          /* this should already be off */

   setupUi(this);
   connect(fileWidget, SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)), 
           this, SLOT(fileDoubleClicked(QTreeWidgetItem *, int)));
   connect(directoryWidget, SIGNAL(
           currentItemChanged(QTreeWidgetItem *, QTreeWidgetItem *)),
           this, SLOT(directoryItemChanged(QTreeWidgetItem *, QTreeWidgetItem *)));
   connect(upButton, SIGNAL(pressed()), this, SLOT(upButtonPushed()));
   connect(markButton, SIGNAL(pressed()), this, SLOT(markButtonPushed()));
   connect(unmarkButton, SIGNAL(pressed()), this, SLOT(unmarkButtonPushed()));
   setFont(m_console->get_font());
   m_console->displayToPrompt();


   titles << "Mark" << "File" << "Mode" << "User" << "Group" << "Size" << "Date";
   fileWidget->setHeaderLabels(titles);

   get_cwd();
   fillDirectory();
   this->show();
}

/*
 * Fill the fileWidget box with the contents of the current directory
 */
void restoreDialog::fillDirectory()
{
   char modes[20], user[20], group[20], size[20], date[30];
   char marked[10];
   int pnl, fnl;
   POOLMEM *file = get_pool_memory(PM_FNAME);
   POOLMEM *path = get_pool_memory(PM_FNAME);

   fileWidget->clear();
   m_console->write_dir("dir");
   QList<QTreeWidgetItem *> treeItemList;
   QStringList item;
   while (m_console->read() > 0) {
      char *p = m_console->msg();
      char *l;
      strip_trailing_junk(p);
      if (*p == '$' || !*p) {
         continue;
      }
      l = p;
      skip_nonspaces(&p);             /* permissions */
      *p++ = 0;
      bstrncpy(modes, l, sizeof(modes));
      skip_spaces(&p);
      skip_nonspaces(&p);             /* link count */
      *p++ = 0;
      skip_spaces(&p);
      l = p;
      skip_nonspaces(&p);             /* user */
      *p++ = 0;
      skip_spaces(&p);
      bstrncpy(user, l, sizeof(user));
      l = p;
      skip_nonspaces(&p);             /* group */
      *p++ = 0;
      bstrncpy(group, l, sizeof(group));
      skip_spaces(&p);
      l = p;
      skip_nonspaces(&p);             /* size */
      *p++ = 0;
      bstrncpy(size, l, sizeof(size));
      skip_spaces(&p);
      l = p;
      skip_nonspaces(&p);             /* date/time */
      skip_spaces(&p);
      skip_nonspaces(&p);
      *p++ = 0;
      bstrncpy(date, l, sizeof(date));
      skip_spaces(&p);
      if (*p == '*') {
         bstrncpy(marked, "*", sizeof(marked));
         p++;
      } else {
         bstrncpy(marked, " ", sizeof(marked));
      }
      split_path_and_filename(p, &path, &pnl, &file, &fnl);
      item.clear();
      item << marked << file << modes << user << group << size << date;
      if (item[1].endsWith("/")) {
         addDirectory(item[1]);
       }
      QTreeWidgetItem *ti = new QTreeWidgetItem((QTreeWidget *)0, item);
      ti->setTextAlignment(5, Qt::AlignRight); /* right align size */
      treeItemList.append(ti);
   }
   fileWidget->clear();
   fileWidget->insertTopLevelItems(0, treeItemList);
   for (int i=0; i<7; i++) {
      fileWidget->resizeColumnToContents(i);
   }

   free_pool_memory(file);
   free_pool_memory(path);
}

/*
 * Function called from fill directory when a directory is found to see if this
 * directory exists in the directory pane and then add it to the directory pane
 */
void restoreDialog::addDirectory(QString &newdir)
{
   QString fullpath ;

   /* if this is the base dir, strip off the leading "/" */
   if (m_cwd == "/"){
      fullpath = newdir;
   } else {
      fullpath = m_cwd + newdir;
   }
   /* is it already existent ?? */
   if (!m_dirPaths.contains(fullpath)) {
      QTreeWidgetItem *item = NULL;
      if (m_dirPaths.empty()) {
         /* this is the base widget */
         item = new QTreeWidgetItem(directoryWidget);
         item->setText(0, newdir.toUtf8().data());
      } else {
         QTreeWidgetItem *parent = m_dirPaths.value(m_cwd);
         if (parent) {
            /* new directories to add */
            item = new QTreeWidgetItem(parent);
            item->setText(0, newdir.toUtf8().data());
            directoryWidget->expandItem(parent);
         }
      }
      /* insert into both forward and reverse hash */
      m_dirPaths.insert(fullpath, item);
      m_dirTreeItems.insert(item, fullpath);
   }
}

/*
 * Executed when the tree item in the directory pane is changed.  This will
 * allow us to populate the file pane and make this the cwd.
 */
void restoreDialog::directoryItemChanged(QTreeWidgetItem *currentitem,
                                         QTreeWidgetItem * /*previousitem*/)
{
   QString fullpath = m_dirTreeItems.value(currentitem);
   if (fullpath != ""){
      cwd(fullpath.toUtf8().data());
      fillDirectory();
   }
}

void restoreDialog::accept()
{
   this->hide();
   m_console->write("done");
   m_console->notify(true);
   delete this;
   mainWin->resetFocus();
}


void restoreDialog::reject()
{
   this->hide();
   m_console->write("quit");
   mainWin->set_status("Canceled");
   delete this;
   m_console->notify(true);
   mainWin->resetFocus();
}

void restoreDialog::fileDoubleClicked(QTreeWidgetItem *item, int column)
{
   char cmd[1000];
   if (column == 0) {                 /* mark/unmark */
      if (item->text(0) == "*") {
         bsnprintf(cmd, sizeof(cmd), "unmark \"%s\"", item->text(1).toUtf8().data());
         item->setText(0, " ");
      } else {
         bsnprintf(cmd, sizeof(cmd), "mark \"%s\"", item->text(1).toUtf8().data());
         item->setText(0, "*");
      }
      m_console->write_dir(cmd);
      if (m_console->read() > 0) {
         strip_trailing_junk(m_console->msg());
         statusLine->setText(m_console->msg());
      }
      m_console->displayToPrompt();
      return;
   }    
   /* 
    * Double clicking other than column 0 means to decend into
    *  the directory -- or nothing if it is not a directory.
    */
   if (item->text(1).endsWith("/")) {
      QString fullpath = m_cwd + item->text(1);
      QTreeWidgetItem *item = m_dirPaths.value(fullpath);
      if (item) {
         directoryWidget->setCurrentItem(item);
      }
   }
}

/*
 * If up button pushed, making the parent tree widget current will call fill
 * directory.
 */
void restoreDialog::upButtonPushed()
{
   cwd("..");
   QTreeWidgetItem *item = m_dirPaths.value(m_cwd);
   if (item) {
      directoryWidget->setCurrentItem(item);
   }
}

/*
 * Mark selected items
 */
void restoreDialog::markButtonPushed()
{
   QList<QTreeWidgetItem *> treeItemList = fileWidget->selectedItems();
   QTreeWidgetItem *item;
   char cmd[1000];
   foreach (item, treeItemList) {
      bsnprintf(cmd, sizeof(cmd), "mark \"%s\"", item->text(1).toUtf8().data());
      item->setText(0, "*");
      m_console->write_dir(cmd);
      if (m_console->read() > 0) {
         strip_trailing_junk(m_console->msg());
         statusLine->setText(m_console->msg());
      }
      Dmsg1(100, "cmd=%s\n", cmd);
      m_console->discardToPrompt();
   }
}

/*
 * Unmark selected items
 */
void restoreDialog::unmarkButtonPushed()
{
   QList<QTreeWidgetItem *> treeItemList = fileWidget->selectedItems();
   QTreeWidgetItem *item;
   char cmd[1000];
   foreach (item, treeItemList) {
      bsnprintf(cmd, sizeof(cmd), "unmark \"%s\"", item->text(1).toUtf8().data());
      item->setText(0, " ");
      m_console->write_dir(cmd);
      if (m_console->read() > 0) {
         strip_trailing_junk(m_console->msg());
         statusLine->setText(m_console->msg());
      }
      Dmsg1(100, "cmd=%s\n", cmd);
      m_console->discardToPrompt();
   }
}

/*
 * Change current working directory 
 */
bool restoreDialog::cwd(const char *dir)
{
   int stat;
   char cd_cmd[MAXSTRING];

   bsnprintf(cd_cmd, sizeof(cd_cmd), "cd \"%s\"", dir);
   Dmsg2(100, "dir=%s cmd=%s\n", dir, cd_cmd);
   m_console->write_dir(cd_cmd);
   lineEdit->clear();
   if ((stat = m_console->read()) > 0) {
      m_cwd = m_console->msg();
      lineEdit->insert(m_cwd);
      Dmsg2(100, "cwd=%s msg=%s\n", m_cwd.toUtf8().data(), m_console->msg());
   } else {
      Dmsg1(000, "stat=%d\n", stat);
      QMessageBox::critical(this, "Error", "cd command failed", QMessageBox::Ok);
   }
   m_console->discardToPrompt();
   return true;  /* ***FIXME*** return real status */
}

/*
 * Return cwd when in tree restore mode 
 */
char *restoreDialog::get_cwd()
{
   int stat;
   m_console->write_dir(".pwd");
   Dmsg0(100, "send: .pwd\n");
   if ((stat = m_console->read()) > 0) {
      m_cwd = m_console->msg();
      Dmsg2(100, "cwd=%s msg=%s\n", m_cwd.toUtf8().data(), m_console->msg());
   } else {
      Dmsg1(000, "stat=%d\n", stat);
      QMessageBox::critical(this, "Error", ".pwd command failed", QMessageBox::Ok);
      Dmsg1(000, "stat=%d\n", stat);
   }
   m_console->discardToPrompt(); 
   return m_cwd.toUtf8().data();
}
