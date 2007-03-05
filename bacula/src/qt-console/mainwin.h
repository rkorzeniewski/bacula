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
 * qt-console main window class definition.
 *
 *  Written by Kern Sibbald, January MMVII
 */

#ifndef _MAINWIN_H_
#define _MAINWIN_H_

#include <QtGui>
#include "ui_main.h"
#include "label/label.h"
#include "run/run.h"
#include "restore/restore.h"

class Console;

class MainWin : public QMainWindow, public Ui::MainForm    
{
   Q_OBJECT

public:
   MainWin(QWidget *parent = 0);
   void set_statusf(const char *fmt, ...);
   void set_status_ready();
   void set_status(const char *buf);
   void writeSettings();
   void readSettings();
   void resetFocus();

public slots:
   void input_line();
   void about();
   void treeItemClicked(QTreeWidgetItem *item, int column);
   void treeItemDoubleClicked(QTreeWidgetItem *item, int column);
   void labelDialogClicked();
   void runDialogClicked();
   void restoreDialogClicked();

protected:
   void closeEvent(QCloseEvent *event);

private:
   void createConnections(); 

private:
   QString m_UserInput;
   Console *m_console;
};

#endif /* _MAINWIN_H_ */
