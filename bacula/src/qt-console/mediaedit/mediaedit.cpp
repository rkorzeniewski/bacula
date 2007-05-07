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
 *   Version $Id: batstack.h 4230 2007-02-21 20:07:37Z kerns $
 *
 *   Dirk Bartley, March 2007
 */
 
#include <QAbstractEventDispatcher>
#include <QTableWidgetItem>
#include <QMessageBox>
#include "bat.h"
#include "mediaedit.h"

/*
 * A constructor 
 */
MediaEdit::MediaEdit(Console *console, QString &mediaId)
{
   m_console = console;
   m_console->notify(false);
   m_pool = "";
   m_status = "";
   m_slot = 0;


   setupUi(this);

   /* The media's pool */
   poolCombo->addItems(console->pool_list);

   /* The media's Status */
   QStringList statusList = (QStringList() << "Full" << "Used" << "Append" << "Error" << "Purged" << "Recycled");
   statusCombo->addItems(statusList);

   /* Set up the query for the default values */
   QStringList FieldList = (QStringList()
      << "Media.VolumeName" << "Pool.Name" << "Media.VolStatus" << "Media.Slot" );
   QStringList AsList = (QStringList()
      << "VolumeName" << "PoolName" << "Status" << "Slot" );
   int i = 0;
   QString query("SELECT ");
   foreach (QString field, FieldList) {
      if (i != 0) {
         query += ", ";
      }
      query += field + " AS " + AsList[i];
      i += 1;
   }
   query += " FROM Media, Pool WHERE Media.PoolId=Pool.PoolId";
   query += " AND Media.MediaId='" + mediaId + "'";
   query += " ORDER BY Pool.Name";

   /* FIXME Make this a user configurable logging action and dont use printf */
   //printf("MediaList query cmd : %s\n",query.toUtf8().data());
   QStringList results;
   if (m_console->sql_cmd(query, results)) {
      QString field;
      QStringList fieldlist;

      /* Iterate through the lines of results, there should only be one. */
      foreach (QString resultline, results) {
         fieldlist = resultline.split("\t");
         i = 0;

         /* Iterate through fields in the record */
         foreach (field, fieldlist) {
            field = field.trimmed();  /* strip leading & trailing spaces */
            if (i == 0) {
               m_mediaName = field;
               volumeLabel->setText(QString("Volume : %1").arg(m_mediaName));
            } else if (i == 1) {
               m_pool = field;
            } else if (i == 2) {
               m_status = field;
            } else if (i == 3) {
               bool ok;
               m_slot = field.toInt(&ok, 10);
               if (!ok){ m_slot = 0; }
            }
            i++;
         } /* foreach field */
      } /* foreach resultline */
   } /* if results from query */

   if (m_mediaName != "") {
      int index;
      /* default value for pool */
      index = poolCombo->findText(m_pool, Qt::MatchExactly);
      if (index != -1) {
         poolCombo->setCurrentIndex(index);
      }

      /* default value for status */
      index = statusCombo->findText(m_status, Qt::MatchExactly);
      if (index != -1) {
         statusCombo->setCurrentIndex(index);
      }
      slotSpin->setValue(m_slot);

      this->show();
   } else {
      QMessageBox::warning(this, "No Volume name", "No Volume name given",
                           QMessageBox::Ok, QMessageBox::Ok);
      return;
   }

}

/*
 * Function to handle updating the record
 */
void MediaEdit::accept()
{
   QString scmd;
   this->hide();
   bool docmd = false;
   scmd = QString("update volume=\"%1\"")
                  .arg(m_mediaName);
   if (m_pool != poolCombo->currentText()) {
       scmd += " pool=\"" + poolCombo->currentText() + "\"";
       docmd = true;
   }
   if (m_status != statusCombo->currentText()) {
       scmd += " volstatus=\"" + statusCombo->currentText() + "\"";
       docmd = true;
   }
   if (m_slot != slotSpin->value()) {
       scmd += " slot=" + QString().setNum(slotSpin->value());
       docmd = true;
   }
   if (docmd) {
      /* FIXME Make this a user configurable logging action and dont use printf */
      //printf("sending command : %s\n",scmd.toUtf8().data());
      m_console->write_dir(scmd.toUtf8().data());
      m_console->displayToPrompt();
      m_console->notify(true);
    }
   delete this;
   mainWin->resetFocus();
}

void MediaEdit::reject()
{
   this->hide();
   m_console->notify(true);
   delete this;
   mainWin->resetFocus();
}
