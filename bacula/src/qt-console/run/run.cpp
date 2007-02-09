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
 *  Run Dialog class
 *
 *   Kern Sibbald, February MMVI
 *
 */ 

#include "bat.h"
#include "run.h"

runDialog::runDialog(Console *console)
{
   setupUi(this);
   storageCombo->addItems(console->storage_list);
   poolCombo->addItems(console->pool_list);
   this->show();
}

void runDialog::accept()
{
   printf("Storage=%s\n"
          "Pool=%s\n", 
           storageCombo->currentText().toUtf8().data(),
           poolCombo->currentText().toUtf8().data());
   this->hide();
   delete this;

#ifdef xxx
   volume  = get_entry_text(label_dialog, "label_entry_volume");

   slot    = get_spin_text(label_dialog, "label_slot");

   if (!pool || !storage || !volume || !(*volume)) {
      set_status_ready();
      return;
   }

   bsnprintf(cmd, sizeof(cmd),
             "label volume=\"%s\" pool=\"%s\" storage=\"%s\" slot=%s\n", 
             volume, pool, storage, slot);
   write_director(cmd);
   set_text(cmd, strlen(cmd));
#endif
}

void runDialog::reject()
{
   printf("Rejected\n");
   this->hide();
   delete this;
}
