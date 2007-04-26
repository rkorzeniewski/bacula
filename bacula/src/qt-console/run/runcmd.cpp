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
 *  Run Command Dialog class
 *
 *  This is called when a Run Command signal is received from the
 *    Director. We parse the Director's output and throw up a 
 *    dialog box.
 *
 *   Kern Sibbald, March MMVII
 *
 *  $Id: $
 */ 

#include "bat.h"
#include "run.h"

/*
 * Setup all the combo boxes and display the dialog
 */
runCmdDialog::runCmdDialog(Console *console)
{
   m_console = console;
   m_console->notify(false);
   setupUi(this);
   fillRunDialog();
   this->show();
   m_console->discardToPrompt();
}

void runCmdDialog::fillRunDialog()
{
   QString item, val;
   QStringList items;
   QRegExp rx("^.*:\\s*(\\S.*$)");   /* Regex to get value */

   m_console->read();
   item = m_console->msg();
   items = item.split("\n");
   label->setText(items[0]);
   Dmsg1(000, "Title=%s\n", items[0].toUtf8().data());
   items.removeFirst();               /* remove title */
   foreach(item, items) {
      rx.indexIn(item);
      val = rx.cap(1);
      Dmsg1(000, "Item=%s\n", item.toUtf8().data());
      Dmsg1(000, "Value=%s\n", val.toUtf8().data());

      if (item.startsWith("JobName:")) {
         jobCombo->addItem(val);
         continue;
      }
      if (item.startsWith("Bootstrap:")) {
         bootstrap->setText(val);
         continue;
      }
      if (item.startsWith("Backup Client:")) {
         clientCombo->addItem(val);
         continue;
      }
      if (item.startsWith("Storage:")) {
         storageCombo->addItem(val);
         continue;
      }
      if (item.startsWith("Where:")) {
         where->setText(val);
         continue;
      }
      if (item.startsWith("When:")) {
         continue;
      }
      if (item.startsWith("Catalog:")) {
         catalogCombo->addItem(val);
         continue;
      }
      if (item.startsWith("Fileset:")) {
         filesetCombo->addItem(val);
         continue;
      }
      if (item.startsWith("Priority:")) {
//       prioritySpin->setValue(atoi(val));
         continue;
      }
   }
}

void runCmdDialog::accept()
{

   this->hide();
   
   m_console->write_dir("yes");
   m_console->displayToPrompt();
   m_console->notify(true);
   delete this;
   mainWin->resetFocus();
}


void runCmdDialog::reject()
{
   mainWin->set_status(" Canceled");
   this->hide();
   m_console->notify(true);
   delete this;
   mainWin->resetFocus();
}
