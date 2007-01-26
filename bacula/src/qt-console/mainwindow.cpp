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
 *  Main Window control for qt-console
 *
 *   Kern Sibbald, January MMVI
 *
 */ 

#include "mainwindow.h"

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
   setupUi(this);                     /* Setup UI defined by main.ui (designer) */
   stackedWidget->setCurrentIndex(0);
   /* Dummy message ***FIXME*** remove a bit later */
   textEdit->setPlainText("Hello Baculites\nThis is the main console window.");
   /* Connect command line edit to input_line */
   connect(lineEdit, SIGNAL(returnPressed()), this, SLOT(input_line()));
   lineEdit->setFocus();
}

/*
 * The user just finished typing a line in the command line edit box
 */
void MainWindow::input_line()
{
   QString cmdStr = lineEdit->text();    /* Get the text */
   lineEdit->clear();                    /* clear the lineEdit box */
   textEdit->append(cmdStr);             /* append text on screen */
}
