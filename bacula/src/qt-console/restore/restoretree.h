#ifndef _RESTORETREE_H_
#define _RESTORETREE_H_

/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2007-2007 Free Software Foundation Europe e.V.

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

   Bacula® is a registered trademark of John Walker.
   The licensor of Bacula is the Free Software Foundation Europe
   (FSFE), Fiduciary Program, Sumatrastrasse 25, 8006 Zürich,
   Switzerland, email:ftf@fsfeurope.org.
*/
/*
 *   Version $Id: restore.h 4945 2007-05-31 01:24:28Z bartleyd2 $
 *
 *  Kern Sibbald, February 2007
 */

#include <QtGui>
#include "pages.h"
#include "ui_restoretree.h"

/*  
 * A restore tree to view files in the catalog
 */
class restoreTree : public Pages, public Ui::restoreTreeForm
{
   Q_OBJECT 

public:
   restoreTree();
   ~restoreTree();
   virtual void currentStackItem();

private slots:
   void refreshButtonPushed();
   void jobComboChanged(int);
   void directoryItemChanged(QTreeWidgetItem *, QTreeWidgetItem *);
   void fileItemChanged(QTableWidgetItem *,QTableWidgetItem *);
   void directoryItemExpanded(QTreeWidgetItem *);

private:
   void populateDirectoryTree();
   void parseDirectory(QString &dir_in);
   bool addDirectory(QString &, QString &);
   void setupPage();
   void writeSettings();
   void readSettings();
   bool m_populated;
   QRegExp m_winRegExpDrive;
   QRegExp m_winRegExpPath;
   QRegExp m_slashregex;
   bool m_slashTrap;
   //QString m_jobCondition;
   QHash<QString, QTreeWidgetItem *> m_dirPaths;
   QString m_condition;
   int m_resultCount;
   int m_debugCnt;
   bool m_debugTrap;
};

#endif /* _RESTORETREE_H_ */
