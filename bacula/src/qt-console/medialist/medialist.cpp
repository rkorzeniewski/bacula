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

MediaList::MediaList(QStackedWidget *parent, Console *console)
{
   setupUi(this);
   m_parent=parent;
//   AddTostack();
   m_poollist = new QStringList();

   m_treeWidget = treeWidget;   /* our medialist screen */
   m_console = console;
   createConnections();
   m_popupmedia="";
}

void MediaList::populateTree()
{
   int stat;
   QTreeWidgetItem *mediatreeitem, *treeitem, *topItem;

   m_treeWidget->clear();
   m_treeWidget->setColumnCount(3);
   topItem = new QTreeWidgetItem(m_treeWidget);
   topItem->setText(0, "Pools");
   topItem->setData(0, Qt::UserRole, 0);
   topItem->setExpanded( true );
   //topItem->setSizeHint(0,QSize(1050,50));

   /* Start with a list of pools */
   m_poollist->clear();
   QString *scmd = new QString(".pools\n");
   m_console->write_dir(scmd->toUtf8().data());
   while ((stat=m_console->read()) > 0) {
      m_poollist->append(m_console->msg());
   }
   for ( QStringList::Iterator poolitem = m_poollist->begin(); poolitem != m_poollist->end(); ++poolitem ) {
      treeitem = new QTreeWidgetItem(topItem);
      m_console->setTreeItem(treeitem);
      poolitem->replace(QRegExp("\n"), "");
      treeitem->setText(0, poolitem->toUtf8().data());
      treeitem->setData(0, Qt::UserRole, 1);
      treeitem->setExpanded( true );

      /* iterate through the media in the pool */
      QString *mcmd = new QString("sqlquery\n");
      m_console->write_dir(mcmd->toUtf8().data());
      while ((stat=m_console->read()) > 0) { }
      QString *mcmd2 = new QString("select m.volumename, m.mediaid, m.mediatype from media m, pool p where p.name = '");
      mcmd2->append(*poolitem);
      mcmd2->append("';\n");
      m_console->write_dir(mcmd2->toUtf8().data());
      QString *mediaread = new QString();
      while ((stat=m_console->read()) > 0) {
	 *mediaread += m_console->msg();
      }
      m_console->discardToPrompt();
      QStringList sqllinelist = mediaread->split("\n");
      /* a regex todetermine if mediareadline is a line of interest. */
      QRegExp regex("^\\|.*\\|$");
      int recordcnter=0;
      QStringList *headerlist = new QStringList();
      /* Iterate through lines retuned */
      for ( QStringList::Iterator mediareadline = sqllinelist.begin(); mediareadline != sqllinelist.end(); ++mediareadline ) {
	 if ( regex.indexIn(*mediareadline) >= 0 ){
	    QStringList recorditemlist = mediareadline->split("|");
	    int recorditemcnter=0;
	    /* Iterate through items in the record */
	    for ( QStringList::Iterator mediarecorditem = recorditemlist.begin(); mediarecorditem != recorditemlist.end(); ++mediarecorditem ) {
	       QString trimmeditem = mediarecorditem->trimmed();
	       if( trimmeditem != "" ){
		  if ( recordcnter == 0 ){
		     headerlist->append(trimmeditem);
		  } else {
		     if ( recorditemcnter == 0 ){
			mediatreeitem = new QTreeWidgetItem(treeitem);
		     }
		     mediatreeitem->setData(recorditemcnter, Qt::UserRole, 2);
		     mediatreeitem->setText(recorditemcnter, trimmeditem.toUtf8().data());
		  }
		  recorditemcnter+=1;
	       }
	    }
	    recordcnter+=1;
	 }
      }
      m_treeWidget->setHeaderLabels(*headerlist);
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
	 m_popupmedia = text;
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
   MediaEdit* edit = new MediaEdit(m_console, m_popupmedia);
   edit->show();
}

void MediaList::showJobs()
{
   JobList* joblist = new JobList(m_console, m_popupmedia);
   joblist->show();
}
