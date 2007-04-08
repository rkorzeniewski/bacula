#ifndef _PAGES_H_
#define _PAGES_H_
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
 *   Version $Id: batstack.h 4230 2007-02-21 20:07:37Z kerns $
 *
 *   Dirk Bartley, March 2007
 */

#include <QtGui>
#include <QList>

/*
 *  The Pages Class
 *
 *  This class is inherited by all widget windows which are on the stack
 *  It is for the purpos of having a conistant set of functions and properties
 *  in all of the subclasses to accomplish tasks such as pulling a window out
 *  of or into the stack.  It also provides virtual functions placed called
 *  from in mainwin so that subclasses can contain functions to allow them
 *  to populate thier screens at the time of first viewing, (when clicked) as
 *  opposed to  the first creation of the console connection.  After all the 
 *  console is not actually connected until after the page selector tree has been
 *  populated.
 */

class Pages : public QWidget
{
public:
   void dockPage();
   void undockPage();
   void togglePageDocking();
   bool isDocked();
   QStackedWidget *m_parent;
   QTreeWidgetItem *m_treeItem;
   QList<QAction*> m_contextActions;
   void SetPassedValues(QStackedWidget*, QTreeWidgetItem*, int );
   virtual void PgSeltreeWidgetClicked();
   virtual void PgSeltreeWidgetDoubleClicked();
   virtual void currentStackItem();

public slots:
   /* closeEvent is a virtual function inherited from QWidget */
   virtual void closeEvent(QCloseEvent* event);

private:
   bool m_docked;
};

#endif /* _PAGES_H_ */
