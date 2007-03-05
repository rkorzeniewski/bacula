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
 *  Run Dialog class
 *
 *   Kern Sibbald, February MMVII
 *
 *  $Id$
 */ 

#include "bat.h"
#include "run.h"

/*
 * Setup all the combo boxes and display the dialog
 */
runDialog::runDialog(Console *console)
{
   QDateTime dt;

   m_console = console;
   setupUi(this);
   m_console->beginNewCommand();
   jobCombo->addItems(console->job_list);
   filesetCombo->addItems(console->fileset_list);
   levelCombo->addItems(console->level_list);
   clientCombo->addItems(console->client_list);
   poolCombo->addItems(console->pool_list);
   storageCombo->addItems(console->storage_list);
   dateTimeEdit->setDateTime(dt.currentDateTime());
   job_name_change(0);
   connect(jobCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(job_name_change(int)));
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

   m_console->write_dir(cmd);
   m_console->display_text(cmd);
   m_console->displayToPrompt();
   delete this;
   mainWin->resetFocus();
}


void runDialog::reject()
{
   mainWin->set_status(" Canceled");
   this->hide();
   delete this;
   mainWin->resetFocus();
}

/*
 * Called here when the jobname combo box is changed.
 *  We load the default values for the new job in the
 *  other combo boxes.
 */
void runDialog::job_name_change(int index)
{
   job_defaults job_defs;

   (void)index;
   job_defs.job_name = jobCombo->currentText();
   if (m_console->get_job_defaults(job_defs)) {
      filesetCombo->setCurrentIndex(filesetCombo->findText(job_defs.fileset_name, Qt::MatchExactly));
      levelCombo->setCurrentIndex(levelCombo->findText(job_defs.level, Qt::MatchExactly));
      clientCombo->setCurrentIndex(clientCombo->findText(job_defs.client_name, Qt::MatchExactly));
      poolCombo->setCurrentIndex(poolCombo->findText(job_defs.pool_name, Qt::MatchExactly));
      storageCombo->setCurrentIndex(storageCombo->findText(job_defs.store_name, Qt::MatchExactly));
      typeCombo->clear();
      typeCombo->addItem(job_defs.type);
   }
}
