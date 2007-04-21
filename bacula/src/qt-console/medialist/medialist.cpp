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
 *   Version $Id: medialist.cpp 4230 2007-02-21 20:07:37Z kerns $
 *
 *  MediaList Class
 *
 *   Dirk Bartley, March 2007
 *
 */ 

#include <QAbstractEventDispatcher>
#include <QMenu>
#include "bat.h"
#include "medialist.h"
#include "mediaedit/mediaedit.h"
#include "joblist/joblist.h"

MediaList::MediaList()
{
   setupUi(this);
   pgInitialize();

   /* mp_treeWidget, Storage Tree Tree Widget inherited from ui_medialist.h */
   createConnections();
   m_populated = false;
   m_checkcurwidget = true;
   m_closeable = false;
}

MediaList::~MediaList()
{
}

/*
 * The main meat of the class!!  The function that querries the director and 
 * creates the widgets with appropriate values.
 */
void MediaList::populateTree()
{
   QTreeWidgetItem *mediatreeitem, *pooltreeitem, *topItem;
   QString currentpool("");
   QString resultline;
   QStringList results;
   const char *query = 
      "SELECT Pool.Name AS Pool, Media.VolumeName AS Media, Media.MediaId AS Id, Media.VolStatus AS VolStatus,"
      " Media.Enabled AS Enabled, Media.VolBytes AS Bytes, Media.VolFiles AS FileCount, Media.VolJobs AS JobCount,"
      " Media.VolRetention AS VolumeRetention, Media.MediaType AS MediaType, Media.LastWritten AS LastWritten"
      " FROM Media, Pool"
      " WHERE Media.PoolId=Pool.PoolId"
      " ORDER BY Pool";
   QStringList headerlist = (QStringList()
      << "Pool Name" << "Volume Name" << "Media Id" << "Volume Status" << "Enabled"
      << "Volume Bytes" << "Volume Files" << "Volume Jobs" << "Volume Retention" 
      << "Media Type" << "Last Written");

   m_checkcurwidget = false;
   mp_treeWidget->clear();
   m_checkcurwidget = true;
   mp_treeWidget->setColumnCount(headerlist.count());
   topItem = new QTreeWidgetItem(mp_treeWidget);
   topItem->setText(0, "Pools");
   topItem->setData(0, Qt::UserRole, 0);
   topItem->setExpanded(true);

   mp_treeWidget->setHeaderLabels(headerlist);

   /* FIXME Make this a user configurable loggin action and dont use printf */
   //printf("MediaList query cmd : %s\n",query);
   if (m_console->sql_cmd(query, results)) {
      QString field;
      QStringList fieldlist;

      foreach (resultline, results) {
         fieldlist = resultline.split("\t");
         int index = 0;
         /* Iterate through fields in the record */
         foreach (field, fieldlist) {
            field = field.trimmed();  /* strip leading & trailing spaces */
            if (field != "") {
               /* The first field is the pool name */
               if (index == 0) {
                  /* If new pool name, create new Pool item */
                  if (currentpool != field) {
                     currentpool = field;
                     pooltreeitem = new QTreeWidgetItem(topItem);
                     pooltreeitem->setText(0, field);
                     pooltreeitem->setData(0, Qt::UserRole, 1);
                     pooltreeitem->setExpanded(true);
                  }
                  mediatreeitem = new QTreeWidgetItem(pooltreeitem);
                  mediatreeitem->setData(index, Qt::UserRole, 2);
               } else {
                  /* Put media fields under the pool tree item */
                  mediatreeitem->setData(index, Qt::UserRole, 2);
                  mediatreeitem->setText(index, field);
               }
            }
            index++;
         }
      }
   }
}

/*
 * Not being used currently,  Should this be kept for possible future use.
 */
void MediaList::createConnections()
{
   connect(mp_treeWidget, SIGNAL(itemPressed(QTreeWidgetItem *, int)), this,
                SLOT(treeItemClicked(QTreeWidgetItem *, int)));
}

/*
 * Not being used currently,  Should this be kept for possible future use.
 */
void MediaList::treeItemClicked(QTreeWidgetItem * /*item*/, int /*column*/)
{
}

/*
 * Called from the signal of the context sensitive menu!
 */
void MediaList::editMedia()
{
   MediaEdit* edit = new MediaEdit(m_console, m_currentlyselected);
   edit->show();
}

/*
 * Called from the signal of the context sensitive menu!
 */
void MediaList::showJobs()
{
   QString emptyclient("");
   mainWin->createPageJobList(m_currentlyselected, emptyclient);
}

/*
 * When the treeWidgetItem in the page selector tree is singleclicked, Make sure
 * The tree has been populated.
 */
void MediaList::PgSeltreeWidgetClicked()
{
   if (!m_populated) {
      populateTree();
      createContextMenu();
      m_populated=true;
   }
}

/*
 * Added to set the context menu policy based on currently active treeWidgetItem
 * signaled by currentItemChanged
 */
void MediaList::treeItemChanged(QTreeWidgetItem *currentwidgetitem, QTreeWidgetItem *previouswidgetitem )
{
   /* m_checkcurwidget checks to see if this is during a refresh, which will segfault */
   if (m_checkcurwidget) {
      /* The Previous item */
      if (previouswidgetitem) { /* avoid a segfault if first time */
         int treedepth = previouswidgetitem->data(0, Qt::UserRole).toInt();
         if (treedepth == 2){
            mp_treeWidget->removeAction(actionEditVolume);
            mp_treeWidget->removeAction(actionListJobsOnVolume);
         }
      }

      int treedepth = currentwidgetitem->data(0, Qt::UserRole).toInt();
      if (treedepth == 2){
         m_currentlyselected=currentwidgetitem->text(1);
         mp_treeWidget->addAction(actionEditVolume);
         mp_treeWidget->addAction(actionListJobsOnVolume);
      }
   }
}

/* 
 * Setup a context menu 
 * Made separate from populate so that it would not create context menu over and
 * over as the tree is repopulated.
 */
void MediaList::createContextMenu()
{
   mp_treeWidget->setContextMenuPolicy(Qt::ActionsContextMenu);
   /*mp_treeWidget->setContextMenuPolicy(Qt::NoContextMenu);*/
   mp_treeWidget->addAction(actionRefreshMediaList);
   connect(actionEditVolume, SIGNAL(triggered()), this, SLOT(editMedia()));
   connect(actionListJobsOnVolume, SIGNAL(triggered()), this, SLOT(showJobs()));
   mp_treeWidget->setContextMenuPolicy(Qt::ActionsContextMenu);
   connect(mp_treeWidget, SIGNAL(
           currentItemChanged(QTreeWidgetItem *, QTreeWidgetItem *)),
           this, SLOT(treeItemChanged(QTreeWidgetItem *, QTreeWidgetItem *)));
   /* connect to the action specific to this pages class */
   connect(actionRefreshMediaList, SIGNAL(triggered()), this,
                SLOT(populateTree()));
}

/*
 * Virtual function which is called when this page is visible on the stack
 */
void MediaList::currentStackItem()
{
   if(!m_populated) {
      populateTree();
      /* add context sensitive menu items specific to this classto the page
       * selector tree. m_m_contextActions is QList of QActions, so this is 
       * only done once with the first population. */
      m_contextActions.append(actionRefreshMediaList);
      /* Create the context menu for the medialist tree */
      createContextMenu();
      m_populated=true;
   }
}

/*
 * Virtual Function to return the name for the medialist tree widget
 */
void MediaList::treeWidgetName(QString &name)
{
   name = "Media";
}
