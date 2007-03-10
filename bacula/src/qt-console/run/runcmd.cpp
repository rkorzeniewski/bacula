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
   QDateTime dt;

   m_console = console;
   m_console->notify(false);
   setupUi(this);
   this->show();
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
