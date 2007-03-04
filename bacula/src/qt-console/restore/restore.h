#ifndef _RESTORE_H_
#define _RESTORE_H_

/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2007-2007 Free Software Foundation Europe e.V.

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
 *   Version $Id$
 *
 *  Kern Sibbald, February 2007
 */

#include <QtGui>
#include "ui_brestore.h"
#include "ui_restore.h"
#include "ui_prerestore.h"

class Console;

/*
 * The pre-restore dialog selects the Job/Client to be restored
 * It really could use considerable enhancement.
 */
class prerestoreDialog : public QDialog, public Ui::prerestoreForm
{
   Q_OBJECT 

public:
   prerestoreDialog(Console *parent);

public slots:
   void accept();
   void reject();
   void job_name_change(int index);

private:
   Console *m_console;

};

/*  
 * The restore dialog is brought up once we are in the Bacula
 * restore tree routines.  It handles putting up a GUI tree
 * representation of the files to be restored.
 */
class restoreDialog : public QDialog, public Ui::restoreForm
{
   Q_OBJECT 

public:
   restoreDialog(Console *parent);
   void fillDirectory(const char *path);

private slots:
   void accept();
   void reject();
   void fileDoubleClicked(QTreeWidgetItem *item, int column);

private:
   Console *m_console;
   QString m_fname;

};


class bRestore : public QWidget, public Ui::bRestoreForm
{
   Q_OBJECT 

public:
   bRestore(QStackedWidget *parent);

public slots:

private:

};



#endif /* _RESTORE_H_ */
