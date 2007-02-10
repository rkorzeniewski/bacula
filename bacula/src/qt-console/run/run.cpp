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
   QDateTime dt;
   m_console = console;
   setupUi(this);
   jobCombo->addItems(console->job_list);
   filesetCombo->addItems(console->fileset_list);
   levelCombo->addItems(console->level_list);
   clientCombo->addItems(console->client_list);
   poolCombo->addItems(console->pool_list);
   storageCombo->addItems(console->storage_list);
   dateTimeEdit->setDateTime(dt.currentDateTime());
   this->show();
}

void runDialog::accept()
{
   char cmd[1000];

   this->hide();
   
   bsnprintf(cmd, sizeof(cmd),
             "run job=\"%s\" fileset=\"%s\" level=%s client=\"%s\" pool=\"%s\" "
             "when=\"%s\" storage=\"%s\" priority=\"%d\" yes\n",
             jobCombo->currentText().toUtf8().data(),
             filesetCombo->currentText().toUtf8().data(),
             levelCombo->currentText().toUtf8().data(),
             clientCombo->currentText().toUtf8().data(),
             poolCombo->currentText().toUtf8().data(),
//           dateTimeEdit->textFromDateTime(dateTimeEdit->dateTime()).toUtf8().data(),
             "",
             storageCombo->currentText().toUtf8().data(),
             prioritySpin->value());

// m_console->write(cmd);
   m_console->set_text(cmd);
   delete this;
}


void runDialog::reject()
{
   mainWin->set_status(" Canceled");
   this->hide();
   delete this;
}
