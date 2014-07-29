/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2007-2011 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from many
   others, a complete list can be found in the file AUTHORS.

   You may use this file and others of this release according to the
   license defined in the LICENSE file, which includes the Affero General
   Public License, v3.0 ("AGPLv3") and some additional permissions and
   terms pursuant to its AGPLv3 Section 7.

   Bacula® is a registered trademark of Kern Sibbald.
*/

/*
 *  Label Page class
 *
 *   Kern Sibbald, February MMVII
 *
 */

#include "bat.h"
#include "label.h"
#include <QMessageBox>

labelPage::labelPage() : Pages()
{
   QString deflt("");
   m_closeable = false;
   showPage(deflt);
}

/*
 * An overload of the constructor to have a default storage show in the
 * combobox on start.  Used from context sensitive in storage class.
 */
labelPage::labelPage(QString &defString) : Pages()
{
   showPage(defString);
}

/*
 * moved the constructor code here for the overload.
 */
void labelPage::showPage(QString &defString)
{
   m_name = "Label";
   pgInitialize();
   setupUi(this);
   m_conn = m_console->notifyOff();
   QTreeWidgetItem* thisitem = mainWin->getFromHash(this);
   thisitem->setIcon(0,QIcon(QString::fromUtf8(":images/label.png")));

   storageCombo->addItems(m_console->storage_list);
   int index = storageCombo->findText(defString, Qt::MatchExactly);
   if (index != -1) {
      storageCombo->setCurrentIndex(index);
   }
   poolCombo->addItems(m_console->pool_list);
   connect(okButton, SIGNAL(pressed()), this, SLOT(okButtonPushed()));
   connect(cancelButton, SIGNAL(pressed()), this, SLOT(cancelButtonPushed()));
   connect(automountOnButton, SIGNAL(pressed()), this, SLOT(automountOnButtonPushed()));
   connect(automountOffButton, SIGNAL(pressed()), this, SLOT(automountOffButtonPushed()));
   dockPage();
   setCurrent();
   this->show();
}


void labelPage::okButtonPushed()
{
   QString scmd;
   if (volumeName->text().toUtf8().data()[0] == 0) {
      QMessageBox::warning(this, "No Volume name", "No Volume name given",
                           QMessageBox::Ok, QMessageBox::Ok);
      return;
   }
   this->hide();
   scmd = QString("label volume=\"%1\" pool=\"%2\" storage=\"%3\" slot=%4\n")
                  .arg(volumeName->text())
                  .arg(poolCombo->currentText())
                  .arg(storageCombo->currentText())
                  .arg(slotSpin->value());
   if (mainWin->m_commandDebug) {
      Pmsg1(000, "sending command : %s\n", scmd.toUtf8().data());
   }
   if (m_console) {
      m_console->write_dir(scmd.toUtf8().data());
      m_console->displayToPrompt(m_conn);
      m_console->notify(m_conn, true);
   } else {
      Pmsg0(000, "m_console==NULL !!!!!!\n");
   }
   closeStackPage();
   mainWin->resetFocus();
}

void labelPage::cancelButtonPushed()
{
   this->hide();
   if (m_console) {
      m_console->notify(m_conn, true);
   } else {
      Pmsg0(000, "m_console==NULL !!!!!!\n");
   }
   closeStackPage();
   mainWin->resetFocus();
}

/* turn automount on */
void labelPage::automountOnButtonPushed()
{
   QString cmd("automount on");
   consoleCommand(cmd);
}

/* turn automount off */
void labelPage::automountOffButtonPushed()
{
   QString cmd("automount off");
   consoleCommand(cmd);
}
