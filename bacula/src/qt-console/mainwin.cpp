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
 *   Version $Id$
 *
 *  Main Window control for bat (qt-console)
 *
 *   Kern Sibbald, January MMVII
 *
 */ 

#include "bat.h"

MainWin::MainWin(QWidget *parent) : QMainWindow(parent)
{
   mainWin = this;
   setupUi(this);                     /* Setup UI defined by main.ui (designer) */

   m_console = new Console(stackedWidget);
   stackedWidget->setCurrentIndex(0);

   resetFocus();

   createConnections();

   this->show();

   readSettings();

   m_console->connect();
}

void MainWin::resetFocus()
{  
   lineEdit->setFocus();
}   


void MainWin::createConnections()
{
   /* Connect signals to slots */
   connect(lineEdit, SIGNAL(returnPressed()), this, SLOT(input_line()));
   connect(actionAbout_bat, SIGNAL(triggered()), this, SLOT(about()));

   connect(treeWidget, SIGNAL(itemActivated(QTreeWidgetItem *, int)), this, 
           SLOT(treeItemClicked(QTreeWidgetItem *, int)));
   connect(treeWidget, SIGNAL(itemClicked(QTreeWidgetItem *, int)), this, 
           SLOT(treeItemClicked(QTreeWidgetItem *, int)));
   connect(treeWidget, SIGNAL(itemPressed(QTreeWidgetItem *, int)), this, 
           SLOT(treeItemClicked(QTreeWidgetItem *, int)));
   connect(treeWidget, SIGNAL(itemDoubleClicked(QTreeWidgetItem *, int)), this, 
           SLOT(treeItemDoubleClicked(QTreeWidgetItem *, int)));

   connect(actionQuit, SIGNAL(triggered()), app, SLOT(closeAllWindows()));
   connect(actionConnect, SIGNAL(triggered()), m_console, SLOT(connect()));
   connect(actionStatusDir, SIGNAL(triggered()), m_console, SLOT(status_dir()));
   connect(actionSelectFont, SIGNAL(triggered()), m_console, SLOT(set_font()));
   connect(actionLabel, SIGNAL(triggered()), this,  SLOT(labelDialogClicked()));
   connect(actionRun, SIGNAL(triggered()), this,  SLOT(runDialogClicked()));
   connect(actionRestore, SIGNAL(triggered()), this,  SLOT(restoreDialogClicked()));
}

/* 
 * Reimplementation of QWidget closeEvent virtual function   
 */
void MainWin::closeEvent(QCloseEvent *event)
{
   writeSettings();
   m_console->writeSettings();
   m_console->terminate();
   event->accept();
}

void MainWin::writeSettings()
{
   QSettings settings("bacula.org", "bat");

   settings.beginGroup("MainWin");
   settings.setValue("winSize", size());
   settings.setValue("winPos", pos());
   settings.endGroup();
}

void MainWin::readSettings()
{ 
   QSettings settings("bacula.org", "bat");

   settings.beginGroup("MainWin");
   resize(settings.value("winSize", QSize(1041, 801)).toSize());
   move(settings.value("winPos", QPoint(200, 150)).toPoint());
   settings.endGroup();
}

void MainWin::treeItemClicked(QTreeWidgetItem *item, int column)
{
   (void)column;
   int index = item->text(1).toInt();
   if (index >= 0 && index < 4) {
      stackedWidget->setCurrentIndex(index);
   }
}

/*
 */
void MainWin::treeItemDoubleClicked(QTreeWidgetItem *item, int column)
{
   (void)column;
   int index = item->text(1).toInt();
   /* ***FIXME**** make this automatic */
   if (index >= 0 && index < 4) {
      stackedWidget->setCurrentIndex(index);
   }
}

void MainWin::labelDialogClicked() 
{
   new labelDialog(m_console);
}

void MainWin::runDialogClicked() 
{
   new runDialog(m_console);
}

void MainWin::restoreDialogClicked() 
{
   new prerestoreDialog(m_console);
}



/*
 * The user just finished typing a line in the command line edit box
 */
void MainWin::input_line()
{
   QString cmdStr = lineEdit->text();    /* Get the text */
   lineEdit->clear();                    /* clear the lineEdit box */
   if (m_console->is_connected()) {
      m_console->display_text(cmdStr + "\n");
      m_console->write_dir(cmdStr.toUtf8().data());         /* send to dir */
   } else {
      set_status("Director not connected. Click on connect button.");
   }
}


void MainWin::about()
{
   QMessageBox::about(this, tr("About bat"),
            tr("<br><h2>bat 0.1</h2>"
            "<p>Copyright &copy; " BYEAR " Free Software Foundation Europe e.V."
            "<p>The <b>bat</b> is an administrative console"
               " interface to the Director."));
}

void MainWin::set_statusf(const char *fmt, ...)
{
   va_list arg_ptr;
   char buf[1000];
   int len;
   va_start(arg_ptr, fmt);
   len = bvsnprintf(buf, sizeof(buf), fmt, arg_ptr);
   va_end(arg_ptr);
   set_status(buf);
// set_scroll_bar_to_end();
}

void MainWin::set_status_ready()
{
   set_status(" Ready");
// set_scroll_bar_to_end();
}

void MainWin::set_status(const char *buf)
{
   statusBar()->showMessage(buf);
// ready = false;
}
