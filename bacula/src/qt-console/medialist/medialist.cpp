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
 *   Kern Sibbald, January MMVI
 *
 */ 

#include <QAbstractEventDispatcher>
#include "bat.h"
#include "medialist.h"
#include "mediaedit/mediaedit.h"
#include "joblist/joblist.h"
#include <QMenu>
//#include <QSize>

MediaList::MediaList(QStackedWidget *parent, Console *console, QTreeWidgetItem *treeItem)
{
   setupUi(this);
   m_parent=parent;

   m_treeWidget = treeWidget;   /* our Storage Tree Tree Widget */
   m_console = console;
   m_treeItem = treeItem;
   createConnections();
   m_populated=false;
   m_headerlist = new QStringList();
   m_popupmedia = new QString("");
   m_poollist = new QStringList();
   m_cmd = new QString("select m.volumename, m.mediaid, m.mediatype, p.name from media m, pool p ORDER BY p.name");
}

MediaList::~MediaList()
{
   delete m_headerlist;
   delete m_popupmedia;
   delete m_poollist;
   delete m_cmd;
}

void MediaList::populateTree()
{
   QTreeWidgetItem *mediatreeitem, *pooltreeitem, *topItem;

   m_treeWidget->clear();
   m_treeWidget->setColumnCount(3);
   topItem = new QTreeWidgetItem(m_treeWidget);
   topItem->setText(0, "Pools");
   topItem->setData(0, Qt::UserRole, 0);
   topItem->setExpanded( true );
   //topItem->setSizeHint(0,QSize(1050,50));

   /* Start with a list of pools */
   m_poollist->clear();
   QStringList *results=m_console->dosql(m_cmd);
   int recordcounter=0;
   m_headerlist->append("Volume Name");
   m_headerlist->append("Media Id");
   m_headerlist->append("Type");
   m_treeWidget->setHeaderLabels(*m_headerlist);
   QString currentpool("");
   for ( QStringList::Iterator resultline = results->begin(); resultline != results->end(); ++resultline ) {
      QStringList recorditemlist = resultline->split("\t");
      int recorditemcnter=0;
      /* Iterate through items in the record */
      for ( QStringList::Iterator mediarecorditem = recorditemlist.begin(); mediarecorditem != recorditemlist.end(); ++mediarecorditem ) {
	 QString trimmeditem = mediarecorditem->trimmed();
	 if( trimmeditem != "" ){
	    if ( recorditemcnter == 0 ){
	       if ( currentpool != trimmeditem.toUtf8().data() ){
		  currentpool = trimmeditem.toUtf8().data();
		  pooltreeitem = new QTreeWidgetItem(topItem);
		  pooltreeitem->setText(0, trimmeditem.toUtf8().data());
		  pooltreeitem->setData(0, Qt::UserRole, 1);
		  pooltreeitem->setExpanded( true );
	       }
	       mediatreeitem = new QTreeWidgetItem(pooltreeitem);
	    }
	    mediatreeitem->setData(recorditemcnter, Qt::UserRole, 2);
	    mediatreeitem->setText(recorditemcnter, trimmeditem.toUtf8().data());
	    recorditemcnter+=1;
	 }
      }
      recordcounter+=1;
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
   QString text = item->text(0);
   switch (treedepth){
      case 1:
	 break;
      case 2:
	 /* Can't figure out how to make a right button do this --- Qt::LeftButton, Qt::RightButton, Qt::MidButton */
	 *m_popupmedia = text;
	 QMenu *popup = new QMenu( m_treeWidget );
	 connect(popup->addAction("Edit Properties"), SIGNAL(triggered()), this, SLOT(editMedia()));
	 connect(popup->addAction("Show Jobs On Media"), SIGNAL(triggered()), this, SLOT(showJobs()));
	 popup->exec(QCursor::pos());
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
   MediaEdit* edit = new MediaEdit(m_console, *m_popupmedia);
   edit->show();
}

void MediaList::showJobs()
{
   JobList* joblist = new JobList(m_console, *m_popupmedia);
   joblist->show();
}

void MediaList::PgSeltreeWidgetClicked()
{
   if( ! m_populated ){
      populateTree();
      m_populated=true;
   }
}

void MediaList::PgSeltreeWidgetDoubleClicked()
{
   populateTree();
}
