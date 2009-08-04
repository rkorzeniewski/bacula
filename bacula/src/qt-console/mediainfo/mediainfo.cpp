/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2007-2009 Free Software Foundation Europe e.V.

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

   Bacula® is a registered trademark of Kern Sibbald.
   The licensor of Bacula is the Free Software Foundation Europe
   (FSFE), Fiduciary Program, Sumatrastrasse 25, 8006 Zürich,
   Switzerland, email:ftf@fsfeurope.org.
*/
 
#include "bat.h"
#include <QAbstractEventDispatcher>
#include <QTableWidgetItem>
#include <QMessageBox>
#include "mediainfo.h"

/*
 * A constructor 
 */
MediaInfo::MediaInfo(QTreeWidgetItem *parentWidget, QString &mediaName)
{
   setupUi(this);
   pgInitialize(tr("Media Info"), parentWidget);
   QTreeWidgetItem* thisitem = mainWin->getFromHash(this);
   thisitem->setIcon(0,QIcon(QString::fromUtf8(":images/cartridge-edit.png")));
   m_mediaName = mediaName;
   m_closeable = true;
   dockPage();
   setCurrent();
   populateForm();
}

// Need to use the fmtwidgetitem helper instead
static QString convertBytesSI(qint64 qfld)
{
   static const qint64 KB = Q_INT64_C(1000);
   static const qint64 MB = (KB * KB);
   static const qint64 GB = (MB * KB);
   static const qint64 TB = (GB * KB);
   static const qint64 PB = (TB * KB);
   static const qint64 EB = (PB * KB);

   /* note: division is integer, so to have some decimals we divide for a
      smaller unit (e.g. GB for a TB number and so on) */
   char suffix;
   if (qfld >= EB) {
      qfld /= PB; 
      suffix = 'E';
   }
   else if (qfld >= PB) {
      qfld /= TB; 
      suffix = 'P';
   }
   else if (qfld >= TB) {
      qfld /= GB; 
      suffix = 'T';
   }
   else if (qfld >= GB) {
      qfld /= MB;
      suffix = 'G';
   }
   else if (qfld >= MB) {
      qfld /= KB;
      suffix = 'M';
   }
   else if (qfld >= KB) {
      suffix = 'k'; /* SI uses lowercase k */
   }
   else  {
      /* plain bytes, no need to reformat */
      return QString("%1 B").arg(qfld); 
   }

   /* having divided for a smaller unit, now we can safely convert to double and
      use the extra room for decimals */
   return QString("%1 %2B").arg(qfld / 1000.0, 0, 'f', 2).arg(suffix);
}

/*
 * Populate the text in the window
 */
void MediaInfo::populateForm()
{
   QString stat;
   char buf[256];
   QString query = 
      "SELECT VolumeName, Pool.Name, MediaType, FirstWritten,"
      "LastWritten, VolMounts, VolBytes, Media.Enabled,"
      "Location.Location, VolStatus, RecyclePool.Name, Media.Recycle, "
      "VolReadTime, VolWriteTime, Media.VolUseDuration, Media.MaxVolJobs, "
      "Media.MaxVolFiles, Media.MaxVolBytes, Media.VolRetention,Slot,InChanger "
      "FROM Media JOIN Pool USING (PoolId) LEFT JOIN Pool AS RecyclePool "
      "ON (Media.RecyclePoolId = RecyclePool.PoolId) "
      "LEFT JOIN Location USING (LocationId) "
      "WHERE Media.VolumeName = '" + m_mediaName + "'";

   QStringList results;
   if (m_console->sql_cmd(query, results)) {
      QString resultline;
      QStringList fieldlist;

      foreach (resultline, results) { // should have only one result
         fieldlist = resultline.split("\t");
         QStringListIterator fld(fieldlist);
         label_VolumeName->setText(fld.next());
         label_Pool->setText(fld.next());
         label_MediaType->setText(fld.next());
         label_FirstWritten->setText(fld.next());
         label_LastWritten->setText(fld.next());
//         label_VolFiles->setText(fld.next());
         label_VolMounts->setText(fld.next());
         label_VolBytes->setText(convertBytesSI(fld.next().toULongLong()));
         label_Enabled->setPixmap(QPixmap(":/images/inflag" + fld.next() + ".png"));
         label_Location->setText(fld.next());
         label_VolStatus->setText(fld.next());
         label_RecyclePool->setText(fld.next());
         chkbox_Recycle->setCheckState(fld.next().toInt()?Qt::Checked:Qt::Unchecked);         
         label_VolReadTime->setText(fld.next());
         label_VolWriteTime->setText(fld.next());
         label_VolUseDuration->setText(fld.next());
         label_MaxVolJobs->setText(fld.next());
         label_MaxVolFiles->setText(fld.next());
         label_MaxVolBytes->setText(fld.next());
         label_VolRetention->setText(fld.next());
         
//         label_VolFiles->setText(fld.next());
//         label_VolErrors->setText(fld.next());

//         stat=fld.next();
//         label_Online->setPixmap(QPixmap(":/images/inflag" + stat + ".png"));
//         jobstatus_to_ascii_gui(stat[0].toAscii(), buf, sizeof(buf));
//         stat = buf;
//       
      }
   }
}
