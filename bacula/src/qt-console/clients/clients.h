#ifndef _CLIENTS_H_g
#define _CLIENTS_H_g
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
 *   Version $Id: clients.h 4230 2007-02-21 20:07:37Z kerns $
 *
 *   Dirk Bartley, March 2007
 */

#include <QtGui>
#include "ui_clients.h"
#include "console.h"
#include "pages.h"
//#include <qstringlist.h>

class Clients : public Pages, public Ui::ClientForm
{
   Q_OBJECT 

public:
   Clients(QStackedWidget *parent, Console *console);
   ~Clients();
   virtual void PgSeltreeWidgetClicked();
   virtual void currentStackItem();

public slots:
   void treeItemChanged(QTreeWidgetItem *, QTreeWidgetItem *);

private slots:
   void populateTree();

private:
   void createContextMenu();

private:
   Console *mp_console;
   QString m_currentlyselected;
   bool m_populated;
   bool m_checkcurwidget;
};

#endif /* _CLIENTS_H_g */
