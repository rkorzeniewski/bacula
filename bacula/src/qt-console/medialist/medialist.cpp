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

   m_treeWidget = treeWidget;   /* our Storage Tree Tree Widget */
   m_console = console;
   createConnections();
   m_populated = false;
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

   m_treeWidget->clear();
   m_treeWidget->setColumnCount(9);
   topItem = new QTreeWidgetItem(m_treeWidget);
   topItem->setText(0, "Pools");
   topItem->setData(0, Qt::UserRole, 0);
   topItem->setExpanded( true );

#ifdef xxx
#include <QSize>
*****    FIXME   *****
//how to get the size of a column to be larger
//topItem->setSizeHint(0,QSize(1050,50));
#endif

   m_treeWidget->setHeaderLabels(headerlist);

   /* 
    * Setup a context menu 
    */
   QAction *editAction = new QAction("Edit Properties", m_treeWidget);
   QAction *listAction = new QAction("List Jobs On Media", m_treeWidget);
   m_treeWidget->addAction(editAction);
   m_treeWidget->addAction(listAction);
   connect(editAction, SIGNAL(triggered()), this, SLOT(editMedia()));
   connect(listAction, SIGNAL(triggered()), this, SLOT(showJobs()));
   m_treeWidget->setContextMenuPolicy(Qt::ActionsContextMenu);
   connect(m_treeWidget, SIGNAL(
           currentItemChanged(QTreeWidgetItem *, QTreeWidgetItem *)),
           this, SLOT(treeItemChanged(QTreeWidgetItem *, QTreeWidgetItem *)));

   if (m_console->sql_cmd(query, results)) {
      QString field;
      QStringList fieldlist;
      QRegExp regex("^Using Catalog");

      foreach (resultline, results) {
         fieldlist = resultline.split("\t");
         if (regex.indexIn(resultline) < 0) {
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
}

/*
 * Not being used currently,  Should this be kept for possible future use.
 */
void MediaList::createConnections()
{
   connect(treeWidget, SIGNAL(itemPressed(QTreeWidgetItem *, int)), this,
                SLOT(treeItemClicked(QTreeWidgetItem *, int)));
   connect(treeWidget, SIGNAL(itemDoubleClicked(QTreeWidgetItem *, int)), this,
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
   /* ***FIXME*** make sure a valid tree item is selected -- check currentItem
 *    ??? Should this be a check in the database for the existence of m_popuptext??*/
   MediaEdit* edit = new MediaEdit(m_console, m_popuptext);
   edit->show();
}

/*
 * Called from the signal of the context sensitive menu!
 */
void MediaList::showJobs()
{
   JobList* joblist = new JobList(m_console, m_popuptext);
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
 */
void MediaList::treeItemChanged(QTreeWidgetItem *currentwidgetitem, QTreeWidgetItem *) /*previouswidgetitem*/
{
   int treedepth = currentwidgetitem->data(0, Qt::UserRole).toInt();
   if (treedepth == 2){
      m_treeWidget->setContextMenuPolicy(Qt::ActionsContextMenu);
   } else {
      m_treeWidget->setContextMenuPolicy(Qt::NoContextMenu);
   }
}

