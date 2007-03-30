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
      << "Volume Name" << "Media Id" << "Volume Status" << "Enabled"
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

   if (m_console->sql_cmd(query, results)) {
      int recordcounter = 0;
      foreach (resultline, results) {
         QRegExp regex("^Using Catalog");
         if (regex.indexIn(resultline) < 0) {
            QStringList recorditemlist = resultline.split("\t");
            int recorditemcnter = 0;
            /* Iterate through items in the record */
            QString mediarecorditem;
            foreach (mediarecorditem, recorditemlist) {
               QString trimmeditem = mediarecorditem.trimmed();
               if (trimmeditem != "") {
                  if (recorditemcnter == 0) {
                     if (currentpool != trimmeditem) {
                        currentpool = trimmeditem;
                        pooltreeitem = new QTreeWidgetItem(topItem);
                        pooltreeitem->setText(0, trimmeditem);
                        pooltreeitem->setData(0, Qt::UserRole, 1);
                        pooltreeitem->setExpanded(true);
                     }
                     mediatreeitem = new QTreeWidgetItem(pooltreeitem);
                  } else {
                     mediatreeitem->setData(recorditemcnter-1, Qt::UserRole, 2);
                     mediatreeitem->setText(recorditemcnter-1, trimmeditem);
                  }
                  recorditemcnter++;
               }
            }
         }
         recordcounter++;
      }
   }
}

void MediaList::createConnections()
{
   connect(treeWidget, SIGNAL(itemPressed(QTreeWidgetItem *, int)), this,
                SLOT(treeItemClicked(QTreeWidgetItem *, int)));
   connect(treeWidget, SIGNAL(itemDoubleClicked(QTreeWidgetItem *, int)), this,
                SLOT(treeItemDoubleClicked(QTreeWidgetItem *, int)));
}

void MediaList::treeItemClicked(QTreeWidgetItem *item, int column)
{
   int treedepth = item->data(column, Qt::UserRole).toInt();
   switch (treedepth) {
   case 1:
      break;
   case 2:
      /* Can't figure out how to make a right button do this --- Qt::LeftButton, Qt::RightButton, Qt::MidButton */
      m_popuptext = item->text(0);
      QMenu *popup = new QMenu( m_treeWidget );
      connect(popup->addAction("Edit Properties"), SIGNAL(triggered()), this, SLOT(editMedia()));
      connect(popup->addAction("Show Jobs On Media"), SIGNAL(triggered()), this, SLOT(showJobs()));
      popup->exec(QCursor::pos());
      break;
   }
}

void MediaList::treeItemDoubleClicked(QTreeWidgetItem *item, int column)
{
   int treedepth = item->data(column, Qt::UserRole).toInt();
   if (treedepth >= 0) {
   }
}

void MediaList::editMedia()
{
   MediaEdit* edit = new MediaEdit(m_console, m_popuptext);
   edit->show();
}

void MediaList::showJobs()
{
   JobList* joblist = new JobList(m_console, m_popuptext);
   joblist->show();
}

void MediaList::PgSeltreeWidgetClicked()
{
   if(!m_populated) {
      populateTree();
      m_populated=true;
   }
}

void MediaList::PgSeltreeWidgetDoubleClicked()
{
   populateTree();
}
