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
#include <math.h>
#include "mediaview.h"
#include "mediaedit/mediaedit.h"
#include "mediainfo/mediainfo.h"
#include "joblist/joblist.h"
#include "relabel/relabel.h"
#include "run/run.h"
#include "util/fmtwidgetitem.h"

MediaView::MediaView()
{
   setupUi(this);
   m_name = tr("Media");
   pgInitialize();
   QTreeWidgetItem* thisitem = mainWin->getFromHash(this);
   thisitem->setIcon(0,QIcon(QString::fromUtf8(":images/cartridge.png")));

   /* mp_treeWidget, Storage Tree Tree Widget inherited from ui_medialist.h */
   m_populated = false;
   m_checkcurwidget = true;
   m_closeable = false;
}

MediaView::~MediaView()
{
}

/*
 * The main meat of the class!!  The function that querries the director and 
 * creates the widgets with appropriate values.
 */
void MediaView::populateTable()
{
   m_populated = true;

   Freeze frz(*m_tableMedia); /* disable updating*/

}

/*
 * Called from the signal of the context sensitive menu!
 */
void MediaView::editVolume()
{
   MediaEdit* edit = new MediaEdit(mainWin->getFromHash(this), m_currentVolumeId);
   connect(edit, SIGNAL(destroyed()), this, SLOT(populateTable()));
}

/*
 * Called from the signal of the context sensitive menu!
 */
void MediaView::showJobs()
{
   QTreeWidgetItem *parentItem = mainWin->getFromHash(this);
   mainWin->createPageJobList(m_currentVolumeName, "", "", "", parentItem);
}

/*
 * Called from the signal of the context sensitive menu!
 */
void MediaView::viewVolume()
{
   QTreeWidgetItem *parentItem = mainWin->getFromHash(this);
   MediaInfo* view = new MediaInfo(parentItem, m_currentVolumeName);
   connect(view, SIGNAL(destroyed()), this, SLOT(populateTable()));

}

/*
 * When the treeWidgetItem in the page selector tree is singleclicked, Make sure
 * The tree has been populated.
 */
void MediaView::PgSeltreeWidgetClicked()
{
   if (!m_populated) {
      populateTable();
   }
   dockPage();
}

/*
 * Virtual function which is called when this page is visible on the stack
 */
void MediaView::currentStackItem()
{
   if(!m_populated) {
      populateTable();
   }
}

/*
 * Called from the signal of the context sensitive menu to delete a volume!
 */
void MediaView::deleteVolume()
{
   if (QMessageBox::warning(this, "Bat",
      tr("Are you sure you want to delete??  !!!.\n"
"This delete command is used to delete a Volume record and all associated catalog"
" records that were created. This command operates only on the Catalog"
" database and has no effect on the actual data written to a Volume. This"
" command can be dangerous and we strongly recommend that you do not use"
" it unless you know what you are doing.  All Jobs and all associated"
" records (File and JobMedia) will be deleted from the catalog."
      "Press OK to proceed with delete operation.?"),
      QMessageBox::Ok | QMessageBox::Cancel)
      == QMessageBox::Cancel) { return; }

   QString cmd("delete volume=");
   cmd += m_currentVolumeName;
   consoleCommand(cmd);
}

/*
 * Called from the signal of the context sensitive menu to purge!
 */
void MediaView::purgeVolume()
{
   if (QMessageBox::warning(this, "Bat",
      tr("Are you sure you want to purge ??  !!!.\n"
"The Purge command will delete associated Catalog database records from Jobs and"
" Volumes without considering the retention period. Purge  works only on the"
" Catalog database and does not affect data written to Volumes. This command can"
" be dangerous because you can delete catalog records associated with current"
" backups of files, and we recommend that you do not use it unless you know what"
" you are doing.\n"
      "Press OK to proceed with the purge operation?"),
      QMessageBox::Ok | QMessageBox::Cancel)
      == QMessageBox::Cancel) { return; }

   QString cmd("purge volume=");
   cmd += m_currentVolumeName;
   consoleCommand(cmd);
   populateTable();
}

/*
 * Called from the signal of the context sensitive menu to prune!
 */
void MediaView::pruneVolume()
{
   new prunePage(m_currentVolumeName, "");
}

// /*
//  * Called from the signal of the context sensitive menu to relabel!
//  */
// void MediaView::relabelVolume()
// {
//    setConsoleCurrent();
//    new relabelDialog(m_console, m_currentVolumeName);
// }
// 
// /*
//  * Called from the signal of the context sensitive menu to purge!
//  */
// void MediaView::allVolumesFromPool()
// {
//    QString cmd = "update volume AllFromPool=" + m_currentVolumeName;
//    consoleCommand(cmd);
//    populateTable();
// }
// 
// void MediaView::allVolumes()
// {
//    QString cmd = "update volume allfrompools";
//    consoleCommand(cmd);
//    populateTable();
// }
// 
//  /*
//   * Called from the signal of the context sensitive menu to purge!
//   */
//  void MediaView::volumeFromPool()
//  {
//     QTreeWidgetItem *currentItem = mp_treeWidget->currentItem();
//     QTreeWidgetItem *parent = currentItem->parent();
//     QString pool = parent->text(0);
//     QString cmd;
//     cmd = "update volume=" + m_currentVolumeName + " frompool=" + pool;
//     consoleCommand(cmd);
//     populateTable();
//  }
//  
