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
#include <QMenu>
#include "content.h"
#include "label/label.h"
#include "mount/mount.h"
#include "util/fmtwidgetitem.h"
#include "status/storstat.h"

Content::Content(QString storage)
{
   setupUi(this);
   pgInitialize(tr("Storage Content"));
   QTreeWidgetItem* thisitem = mainWin->getFromHash(this);
   thisitem->setIcon(0,QIcon(QString::fromUtf8(":images/package-x-generic.png")));

   m_populated = false;
   m_firstpopulation = true;
   m_checkcurwidget = true;
   m_closeable = true;
   m_currentStorage = storage;

   populateContent();

   dockPage();
   setCurrent();
}

/*
 * The main meat of the class!!  The function that querries the director and 
 * creates the widgets with appropriate values.
 */
void Content::populateContent()
{
   char buf[200];
   time_t t;
   struct tm tm;

   m_populated = true;
   m_firstpopulation = false;
   Freeze frz(*tableContent); /* disable updating*/
   tableContent->clearContents();

   QStringList results_all;
   QString cmd("status slots drive=0 storage=\"" + m_currentStorage + "\"");

   m_console->dir_cmd(cmd, results_all);
   // skip non list messages
   QStringList results = results_all.filter(QRegExp("[0-9]+\\|"));
   Pmsg1(0, "count=%i\n", results.size());
   tableContent->setRowCount(results.size());
   int row = results.size();

//   tableContent->setColumnCount(8);
   int col, i;
   foreach (QString resultline, results){
      QStringList fieldlist = resultline.split("|");
      col=0;
      i=0;

      if (fieldlist.size() < 9)
         continue; /* some fields missing, ignore row */

      Pmsg2(0, "row=%i line=%s\n", row, resultline.toUtf8().data());

      TableItemFormatter slotitem(*tableContent, row--);

      slotitem.setNumericFld(col++, fieldlist[i++]); // Slot

      if (fieldlist[i++] != "") {                     // skip "Real Slot"
         slotitem.setTextFld(col++,  fieldlist[i++]); // Volume
         slotitem.setBytesFld(col++, fieldlist[i++]); // Bytes
         slotitem.setVolStatusFld(col++, fieldlist[i++]); // Status
         slotitem.setTextFld(col++,  fieldlist[i++]);     // MediaType
         slotitem.setTextFld(col++,  fieldlist[i++]);     // Pool

         if (fieldlist[i].toInt() > 0) {
            t =  fieldlist[i++].toInt();
            localtime_r(&t, &tm);         
            strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
            slotitem.setTextFld(col++, QString(buf));           // LastWritten
            
            t =  fieldlist[i++].toInt();
            localtime_r(&t, &tm);         
            strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
            slotitem.setTextFld(col++, QString(buf));         // Expire
         }
      }
   }

   tableContent->resizeColumnsToContents();
   tableContent->verticalHeader()->hide();

   tableContent->sortByColumn(0, Qt::AscendingOrder);
   tableContent->setSortingEnabled(true);

   int rcnt = tableContent->rowCount();
   int ccnt = tableContent->columnCount();
   for(int r=0; r < rcnt; r++) {
      for(int c=0; c < ccnt; c++) {
         QTableWidgetItem* item = tableContent->item(r, c);
         if (item) {
            item->setFlags(Qt::ItemFlags(item->flags() & (~Qt::ItemIsEditable)));
         }
      }
   }

   tableTray->verticalHeader()->hide();
   tableDrive->verticalHeader()->hide();
}

/*
 * Virtual function which is called when this page is visible on the stack
 */
void Content::currentStackItem()
{
   if(!m_populated) {
      populateContent();
   }
}

/*
 *  Functions to respond to local context sensitive menu sending console
 *  commands If I could figure out how to make these one function passing a
 *  string, Yaaaaaa
 */
void Content::consoleStatusStorage()
{
   QString cmd("status storage=");
   cmd += m_currentStorage;
   consoleCommand(cmd);
}

/* Label Media populating current storage by default */
void Content::consoleLabelStorage()
{
   new labelPage(m_currentStorage);
}

void Content::treeItemChanged(QTreeWidgetItem *, QTreeWidgetItem *)
{

}

/* Mount currently selected storage */
void Content::consoleMountStorage()
{
   if (m_currentAutoChanger == 0){
      /* no autochanger, just execute the command in the console */
      QString cmd("mount storage=");
      cmd += m_currentStorage;
      consoleCommand(cmd);
   } else {
      setConsoleCurrent();
      /* if this storage is an autochanger, lets ask for the slot */
      new mountDialog(m_console, m_currentStorage);
   }
}

/* Unmount Currently selected storage */
void Content::consoleUnMountStorage()
{
   QString cmd("umount storage=");
   cmd += m_currentStorage;
   consoleCommand(cmd);
}

/* Update Slots */
void Content::consoleUpdateSlots()
{
   QString cmd("update slots storage=");
   cmd += m_currentStorage;
   consoleCommand(cmd);
}

/* Update Slots Scan*/
void Content::consoleUpdateSlotsScan()
{
   QString cmd("update slots scan storage=");
   cmd += m_currentStorage;
   consoleCommand(cmd);
}

/* Release a tape in the drive */
void Content::consoleRelease()
{
   QString cmd("release storage=");
   cmd += m_currentStorage;
   consoleCommand(cmd);
}

/*
 *  Open a status storage window
 */
void Content::statusStorageWindow()
{
   /* if one exists, then just set it current */
   bool found = false;
   foreach(Pages *page, mainWin->m_pagehash) {
      if (mainWin->currentConsole() == page->console()) {
         if (page->name() == tr("Storage Status %1").arg(m_currentStorage)) {
            found = true;
            page->setCurrent();
         }
      }
   }
   if (!found) {
      QTreeWidgetItem *parentItem = mainWin->getFromHash(this);
      new StorStat(m_currentStorage, parentItem);
   }
}
