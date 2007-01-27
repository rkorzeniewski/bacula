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
 * qt-console main window class definition.
 *
 *  Written by Kern Sibbald, January MMVI
 */

#ifndef _MAINWINDOW_H_
#define _MAINWINDOW_H_

#include <QtGui>
#include "ui_main.h"

class MainWindow : public QMainWindow, public Ui::MainForm    
{
   Q_OBJECT

public:
   MainWindow(QWidget *parent = 0);

public slots:
   void input_line();
   void about();
   void treeItemClicked(QTreeWidgetItem *item, int column);

private:
   QString m_UserInput;
};

#endif /* _MAINWINDOW_H_ */
