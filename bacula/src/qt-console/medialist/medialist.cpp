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

MediaList::MediaList(QStackedWidget *parent, Console *console, QTreeWidgetItem *treeItem, int indexseq)
{
   SetPassedValues(parent, treeItem, indexseq );
   setupUi(this);

   /* mp_treeWidget, Storage Tree Tree Widget inherited from ui_medialist.h */
   mp_console = console;
   createConnections();
   m_populated = false;
   m_checkcurwidget = true;
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
      "SELECT p.Name,m.VolumeName,m.MediaId,m.VolStatus,m.Enabled,m.VolBytes,"
      "m.VolFiles,m.VolRetention,m.MediaType,m.LastWritten"
      " FROM Media m,Pool p"
      " WHERE m.PoolId=p.PoolId"
      " ORDER BY p.Name";
   QStringList headerlist = (QStringList()
      << "Pool Name" << "Volume Name" << "Media Id" << "Volume Status" << "Enabled"
      << "Volume Bytes" << "Volume Files" << "Volume Retention" 
      << "Media Type" << "Last Written");

   m_checkcurwidget = false;
   mp_treeWidget->clear();
   m_checkcurwidget = true;
   mp_treeWidget->setColumnCount(9);
   topItem = new QTreeWidgetItem(mp_treeWidget);
   topItem->setText(0, "Pools");
   topItem->setData(0, Qt::UserRole, 0);
   topItem->setExpanded( true );

   mp_treeWidget->setHeaderLabels(headerlist);

   if (mp_console->sql_cmd(query, results)) {
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
   connect(mp_treeWidget, SIGNAL(itemDoubleClicked(QTreeWidgetItem *, int)), this,
                SLOT(treeItemDoubleClicked(QTreeWidgetItem *, int)));
}

/*
 * Not being used currently,  Should this be kept for possible future use.
 */
void MediaList::treeItemClicked(QTreeWidgetItem * /*item*/, int /*column*/)
{
}

/*
 * Not being used currently,  Should this be kept for possible future use.
 */
void MediaList::treeItemDoubleClicked(QTreeWidgetItem * /*item*/, int /*column*/)
{
}

/*
 * Called from the signal of the context sensitive menu!
 */
void MediaList::editMedia()
{
   MediaEdit* edit = new MediaEdit(mp_console, m_currentlyselected);
   edit->show();
}

/*
 * Called from the signal of the context sensitive menu!
 */
void MediaList::showJobs()
{
   JobList* joblist = new JobList(mp_console, m_currentlyselected);
   joblist->show();
}

/*
 * When the treeWidgetItem in the page selector tree is singleclicked, Make sure
 * The tree has been populated.
 */
void MediaList::PgSeltreeWidgetClicked()
{
   if(!m_populated) {
      populateTree();
      createContextMenu();
      m_populated=true;
   }
}

/*
 * When the treeWidgetItem in the page selector tree is doubleclicked, Use that
 * As a signal to repopulate from a query of the database.
 * Should this be from a context sensitive menu in either or both of the page selector
 * or This widnow ???
 */
void MediaList::PgSeltreeWidgetDoubleClicked()
{
   populateTree();
}

/*
 * Added to set the context menu policy based on currently active treeWidgetItem
 * signaled by currentItemChanged
 */
void MediaList::treeItemChanged(QTreeWidgetItem *currentwidgetitem, QTreeWidgetItem *) /*previouswidgetitem*/
{
   if ( m_checkcurwidget ) {
      int treedepth = currentwidgetitem->data(0, Qt::UserRole).toInt();
      if (treedepth == 2){
         mp_treeWidget->setContextMenuPolicy(Qt::ActionsContextMenu);
         m_currentlyselected=currentwidgetitem->text(1);
      } else {
         mp_treeWidget->setContextMenuPolicy(Qt::NoContextMenu);
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
   QAction *editAction = new QAction("Edit Properties", mp_treeWidget);
   QAction *listAction = new QAction("List Jobs On Media", mp_treeWidget);
   mp_treeWidget->addAction(editAction);
   mp_treeWidget->addAction(listAction);
   connect(editAction, SIGNAL(triggered()), this, SLOT(editMedia()));
   connect(listAction, SIGNAL(triggered()), this, SLOT(showJobs()));
   mp_treeWidget->setContextMenuPolicy(Qt::ActionsContextMenu);
   connect(mp_treeWidget, SIGNAL(
           currentItemChanged(QTreeWidgetItem *, QTreeWidgetItem *)),
           this, SLOT(treeItemChanged(QTreeWidgetItem *, QTreeWidgetItem *)));
   /* connect to the action specific to this pages class */
   connect(actionRepopulateMediaTree, SIGNAL(triggered()), this,
                SLOT(populateTree()));
}

/*
 * Virtual function which is called when this page is visible on the stack
 */
void MediaList::currentStackItem()
{
   if(!m_populated) {
      populateTree();
      m_contextActions.append(actionRepopulateMediaTree);
      createContextMenu();
      m_populated=true;
   }
}
