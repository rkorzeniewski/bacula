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
 *  Run Command Dialog class
 *
 *  This is called when a Run Command signal is received from the
 *    Director. We parse the Director's output and throw up a 
 *    dialog box.  This happens, for example, after the user finishes
 *    selecting files to be restored. The Director will then submit a
 *    run command, that causes this page to be popped up.
 *
 *   Kern Sibbald, March MMVII
 *
 *  $Id: $
 */ 

#include "bat.h"
#include "restoretreerun.h"

/*
 * Setup all the combo boxes and display the dialog
 */
restoreTreeRunPage::restoreTreeRunPage(QString &table, QString &client, QList<int> &jobs, QTreeWidgetItem* parentItem)
{
   m_tempTable = table;
   m_jobList = jobs;
   m_client = client;
   m_name = "Restore Tree Run";
   pgInitialize(parentItem);
   setupUi(this);
   QTreeWidgetItem* thisitem = mainWin->getFromHash(this);
   thisitem->setIcon(0,QIcon(QString::fromUtf8(":images/restore.png")));
   m_console->notify(false);

   fill();
   m_console->discardToPrompt();

   connect(okButton, SIGNAL(pressed()), this, SLOT(okButtonPushed()));
   connect(cancelButton, SIGNAL(pressed()), this, SLOT(cancelButtonPushed()));
   dockPage();
   setCurrent();
   this->show();
}

void restoreTreeRunPage::fill()
{
   QDateTime dt;
   clientCombo->addItems(m_console->client_list);
   replaceCombo->addItems(QStringList() << "never" << "always" << "ifnewer" << "ifolder");
   replaceCombo->setCurrentIndex(replaceCombo->findText("never", Qt::MatchExactly));
   dateTimeEdit->setDisplayFormat(mainWin->m_dtformat);
   dateTimeEdit->setDateTime(dt.currentDateTime());
}

void restoreTreeRunPage::okButtonPushed()
{
   QString jobOption = " jobid=\"";
   bool first = true;
   foreach (int job, m_jobList) {
      if (first) first = false;
      else jobOption += ",";
      jobOption += QString("%1").arg(job);
   }
   jobOption += "\"";
   QString cmd = QString("restore");
   cmd += " client=\"" + m_client + "\""
          + jobOption + 
          " file=\"?" + m_tempTable + "\" yes";
   if (mainWin->m_commandDebug)
      Pmsg1(000, "preRestore command \'%s\'\n", cmd.toUtf8().data());
   consoleCommand(cmd);
   mainWin->resetFocus();
   closeStackPage();

/*   QString cmd(".mod");
   cmd += " restoreclient=\"" + clientCombo->currentText() + "\"";
   cmd += " replace=\"" + replaceCombo->currentText() + "\"";
   cmd += " when=\"" + dateTimeEdit->dateTime().toString(mainWin->m_dtformat) + "\"";
   cmd += " bootstrap=\"" + bootstrap->text() + "\"";
   cmd += " where=\"" + where->text() + "\"";
   QString pri;
   QTextStream(&pri) << " priority=\"" << prioritySpin->value() << "\"";
   cmd += pri;
   cmd += " yes\n"; */
}


void restoreTreeRunPage::cancelButtonPushed()
{
   m_console->displayToPrompt();
   m_console->write_dir(".");
   m_console->displayToPrompt();
   mainWin->set_status(" Canceled");
   this->hide();
   m_console->notify(true);
   closeStackPage();
   mainWin->resetFocus();
}
