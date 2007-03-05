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

restoreDialog::restoreDialog(Console *console)
{
   m_console = console;
  
   m_console->setEnabled(false);
   setupUi(this);
   connect(fileWidget, SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)), 
           this, SLOT(fileDoubleClicked(QTreeWidgetItem *, int)));
   setFont(m_console->get_font());
   m_console->displayToPrompt();
   fillDirectory();
   this->show();
}

/*
 * Fill the fileWidget box with the contents of the current directory
 */
void restoreDialog::fillDirectory()
{
   char cd_cmd[MAXSTRING];
   char modes[20], user[20], group[20], size[20], date[30];
   char marked[10];
   int pnl, fnl;
   POOLMEM *file = get_pool_memory(PM_FNAME);
   POOLMEM *path = get_pool_memory(PM_FNAME);
   QStringList titles;

   titles << "Mark" << "File" << "Mode" << "User" << "Group" << "Size" << "Date";
   fileWidget->setHeaderLabels(titles);

   char *dir = get_cwd();
   bsnprintf(cd_cmd, sizeof(cd_cmd), "cd \"%s\"\n", dir);
   Dmsg2(100, "dir=%s cmd=%s\n", dir, cd_cmd);
   m_console->write_dir(cd_cmd);
   m_console->discardToPrompt();

   m_console->write_dir("dir");
   QList<QTreeWidgetItem *> items;
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
      item << "" << file << modes << user << group << size << date;
      QTreeWidgetItem *ti = new QTreeWidgetItem((QTreeWidget *)0, item);
      ti->setTextAlignment(5, Qt::AlignRight); /* right align size */
      items.append(ti);
   }
   fileWidget->clear();
   fileWidget->insertTopLevelItems(0, items);

   free_pool_memory(file);
   free_pool_memory(path);
}

void restoreDialog::accept()
{
   this->hide();
   m_console->write("done");
   delete this;
   m_console->setEnabled(true);
   mainWin->resetFocus();
}


void restoreDialog::reject()
{
   this->hide();
   m_console->write("quit");
   mainWin->set_status("Canceled");
   delete this;
   m_console->setEnabled(true);
   mainWin->resetFocus();
}

void restoreDialog::fileDoubleClicked(QTreeWidgetItem *item, int column)
{
   char cmd[1000];
//   printf("cwd=%s Text=%s column=%d\n", m_cwd.toUtf8().data(), 
//          item->text(1).toUtf8().data(), column);
   if (column == 0) {                 /* mark/unmark */
      if (item->text(0) == "*") {
         bsnprintf(cmd, sizeof(cmd), "unmark \"%s\"\n", item->text(1).toUtf8().data());
         item->setText(0, " ");
      } else {
         bsnprintf(cmd, sizeof(cmd), "mark \"%s\"\n", item->text(1).toUtf8().data());
         item->setText(0, "*");
      }
      m_console->write(cmd);
//    printf("cmd=%s", cmd);
      m_console->displayToPrompt();
      return;
   }
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
   }
   m_console->displayToPrompt(); 
   return m_cwd.toUtf8().data();
}
