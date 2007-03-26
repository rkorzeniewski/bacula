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
 *  Main Window control for bat (qt-console)
 *
 *   Kern Sibbald, January MMVII
 *
 */ 

#include "bat.h"
#include "medialist/medialist.h"

MainWin::MainWin(QWidget *parent) : QMainWindow(parent)
{

   mainWin = this;
   setupUi(this);                     /* Setup UI defined by main.ui (designer) */
   treeWidget->clear();
   treeWidget->setColumnCount(1);
   treeWidget->setHeaderLabel("Select Page");

   m_pages = 0;
   createPages();

   resetFocus();

   createConnections();

   this->show();

   readSettings();

   m_console->connect();
}

void MainWin::createPages()
{
   DIRRES *dir;
   QTreeWidgetItem *item, *topItem;

   /* Create console tree stacked widget item */
   m_console = new Console(stackedWidget);
   /* Console is special -> needs director*/
   /* Just take the first Director */
   LockRes();
   dir = (DIRRES *)GetNextRes(R_DIRECTOR, NULL);
   m_console->setDirRes(dir);
   UnlockRes();
   /* The top tree item representing the director */
   topItem = createTopPage(dir->name());
   topItem->setIcon(0, QIcon(QString::fromUtf8("images/server.png")));
   /* Create Tree Widget Item */
   item = createPage("Console", topItem);
   m_console->setTreeItem(item);
   /* Append to bstacklist */
   m_bstacklist.append(m_console);
   /* Set BatStack m_treeItem */
   m_console->SetBSTreeWidgetItem(item);
   QBrush redBrush(Qt::red);
   item->setForeground(0, redBrush);

   /* Now with the console created, on with the rest, these are easy */
   /* All should be
 * 1. create tree widget item
 * 2. create object passing pointer to tree widget item (modified constructors to pass QTreeWidget pointers)
 * 3. append to stacklist
 * And it can even be done in one line.
 * */ 

   /* brestore */
   m_bstacklist.append(new bRestore( stackedWidget, createPage("brestore", topItem) ));

   /* lastly for now, the medialist */
   m_bstacklist.append(new MediaList(stackedWidget, m_console, createPage("Storage Tree", topItem )));

   /* Iterate through and add to the stack */
   for ( QList<BatStack*>::iterator bstackItem = m_bstacklist.begin(); bstackItem != m_bstacklist.end(); ++bstackItem ) {
      (*bstackItem)->AddTostack();
   }

   treeWidget->expandItem(topItem);
   stackedWidget->setCurrentIndex(0);
}

/* Create a root Tree Widget */
QTreeWidgetItem *MainWin::createTopPage(char *name )
{
   QTreeWidgetItem *item = new QTreeWidgetItem(treeWidget);
   item->setText(0, name);
   return item;
}

/* Create A Tree Widget Item which will be associated with a Page in the stacked widget */
QTreeWidgetItem *MainWin::createPage(char *name, QTreeWidgetItem *parent)
{
   QTreeWidgetItem *item = new QTreeWidgetItem(parent);
   item->setText(0, name);
   return item;
}

/*
 * Handle up and down arrow keys for the command line
 *  history.
 */
void MainWin::keyPressEvent(QKeyEvent *event)
{
   if (m_cmd_history.size() == 0) {
      event->ignore();
      return;
   }
   switch (event->key()) {
   case Qt::Key_Down:
      if (m_cmd_last < 0 || m_cmd_last >= (m_cmd_history.size()-1)) {
         event->ignore();
         return;
      }
      m_cmd_last++;
      break;
   case Qt::Key_Up:
      if (m_cmd_last == 0) {
         event->ignore();
         return;
      }
      if (m_cmd_last < 0 || m_cmd_last > (m_cmd_history.size()-1)) {
         m_cmd_last = m_cmd_history.size() - 1;
      } else {
         m_cmd_last--;
      }
      break;
   default:
      event->ignore();
      return;
   }
   lineEdit->setText(m_cmd_history[m_cmd_last]);
}

void MainWin::createConnections()
{
   /* Connect signals to slots */
   connect(lineEdit, SIGNAL(returnPressed()), this, SLOT(input_line()));
   connect(actionAbout_bat, SIGNAL(triggered()), this, SLOT(about()));

/*   connect(treeWidget, SIGNAL(itemActivated(QTreeWidgetItem *, int)), this, 
           SLOT(treeItemClicked(QTreeWidgetItem *, int)));
   connect(treeWidget, SIGNAL(itemPressed(QTreeWidgetItem *, int)), this, 
           SLOT(treeItemClicked(QTreeWidgetItem *, int)));  Commented out because it was getting to clicked multiple times*/
   connect(treeWidget, SIGNAL(itemClicked(QTreeWidgetItem *, int)), this, 
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
   connect(actionPullWindowOut, SIGNAL(triggered()), this,  SLOT(pullWindowOutButton()));
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
   /* Iterate through and find the tree widget item clicked */
   column+=0;
   QList<BatStack*>::iterator bstackItem;
   bstackItem = m_bstacklist.begin();
   while ( bstackItem != m_bstacklist.end() ){
      if ( item == (*bstackItem)->m_treeItem ) {
	 int stackindex=stackedWidget->indexOf( *bstackItem );
	 if( stackindex >= 0 ){
	    stackedWidget->setCurrentIndex(stackindex);
	 }
         (*bstackItem)->PgSeltreeWidgetClicked();
      }
      ++bstackItem;
   }
}

/*
 */
void MainWin::treeItemDoubleClicked(QTreeWidgetItem *item, int column)
{
   /* Iterate through and find the tree widget item double clicked */
   column+=0;
   QList<BatStack*>::iterator bstackItem;
   bstackItem = m_bstacklist.begin();
   while ( bstackItem != m_bstacklist.end() ){
      if ( item == (*bstackItem)->m_treeItem ) {
	 /* This could be a call a virtual function ?? or just popup a popup menu for an action */
	 if ( (*bstackItem)->isStacked() == true ){
	    m_bstackpophold=*bstackItem;
	    QMenu *popup = new QMenu( treeWidget );
	    connect(popup->addAction("Pull Window Out"), SIGNAL(triggered()), this, SLOT(pullWindowOut()));
	    popup->exec(QCursor::pos());
	 } else {
	    (*bstackItem)->Togglestack();
	 }
         (*bstackItem)->PgSeltreeWidgetDoubleClicked();
      }
      ++bstackItem;
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
   m_cmd_history.append(cmdStr);
   m_cmd_last = -1;
}


void MainWin::about()
{
   QMessageBox::about(this, tr("About bat"),
            tr("<br><h2>bat 0.2, by Kern Sibbald</h2>"
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
}

void MainWin::set_status_ready()
{
   set_status(" Ready");
}

void MainWin::set_status(const char *buf)
{
   statusBar()->showMessage(buf);
}

void MainWin::pullWindowOut()
{
   m_bstackpophold->Togglestack();
}

void MainWin::pullWindowOutButton()
{
   int curindex = stackedWidget->currentIndex();
   QList<BatStack*>::iterator bstackItem;
   bstackItem = m_bstacklist.begin();
   bool done=false;
   while ( (bstackItem != m_bstacklist.end()) && not(done) ){
      if ( curindex == stackedWidget->indexOf( *bstackItem ) ){
	 (*bstackItem)->Togglestack();
	 done=true;
      }
      ++bstackItem;
   }
}
