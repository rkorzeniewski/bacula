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

MediaList::MediaList(QStackedWidget *parent)
{
   setupUi(this);
   parent->addWidget(this);
   poollist = new QStringList();

   m_treeWidget = treeWidget;   /* our medialist screen */
}

void MediaList::DoDisplay(Console *console)
{
   int stat;
   QTreeWidgetItem *mediatreeitem, *treeitem, *topItem;

   m_console = console;

   m_treeWidget->clear();
   m_treeWidget->setColumnCount(3);
   topItem = new QTreeWidgetItem(m_treeWidget);
   topItem->setText(0, "Pools");

   /* Start with a list of pools */
   poollist->clear();
   QString *scmd = new QString(".pools\n");
   m_console->write_dir(scmd->toUtf8().data());
   while ((stat=m_console->read()) > 0) {
      poollist->append(m_console->msg());
   }
   for ( QStringList::Iterator poolitem = poollist->begin(); poolitem != poollist->end(); ++poolitem ) {
      treeitem = new QTreeWidgetItem(topItem);
      m_console->setTreeItem(treeitem);
      poolitem->replace(QRegExp("\n"), "");
      treeitem->setText(0, poolitem->toUtf8().data());

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
      QStringList sqllinelist = mediaread->split("\n");
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
