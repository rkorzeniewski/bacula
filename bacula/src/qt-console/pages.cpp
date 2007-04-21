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
 *   Version $Id: batstack.cpp 4230 2007-02-21 20:07:37Z kerns $
 *
 *   Dirk Bartley, March 2007
 */

#include "pages.h"
#include "bat.h"

/*
 * dockPage
 * This function is intended to be called from within the Pages class to pull
 * a window from floating to in the stack widget.
 */

void Pages::dockPage()
{
   /* These two lines are for making sure if it is being changed from a window
    * that it has the proper window flag and parent.
    */
   setWindowFlags(Qt::Widget);
//   setParent(m_parent);

   /* This was being done already */
   m_parent->addWidget(this);

   /* Set docked flag */
   m_docked = true;
}

/*
 * undockPage
 * This function is intended to be called from within the Pages class to put
 * a window from the stack widget to a floating window.
 */

void Pages::undockPage()
{
   /* Change from a stacked widget to a normal window */
   m_parent->removeWidget(this);
   setWindowFlags(Qt::Window);
   show();
   /* Clear docked flag */
   m_docked = false;
}

/*
 * This function is intended to be called with the subclasses.  When it is 
 * called the specific sublclass does not have to be known to Pages.  When it 
 * is called this function will change the page from it's current state of being
 * docked or undocked and change it to the other.
 */

void Pages::togglePageDocking()
{
   if (m_docked) {
      undockPage();
   } else {
      dockPage();
   }
}

/*
 * This function is because I wanted for some reason to keep it protected but still 
 * give any subclasses the ability to find out if it is currently stacked or not.
 */
bool Pages::isDocked()
{
   return m_docked;
}

/*
 * To keep m_closeable protected as well
 */
bool Pages::isCloseable()
{
   return m_closeable;
}

/*
 * When a window is closed, this slot is called.  The idea is to put it back in the
 * stack here, and it works.  I wanted to get it to the top of the stack so that the
 * user immediately sees where his window went.  Also, if he undocks the window, then
 * closes it with the tree item highlighted, it may be confusing that the highlighted
 * treewidgetitem is not the stack item in the front.
 */

void Pages::closeEvent(QCloseEvent* event)
{
   /* A Widget was closed, lets toggle it back into the window, and set it in front. */
   dockPage();

   /* is the tree widget item for "this" the current widget item */
   if (mainWin->treeWidget->currentItem() == mainWin->getFromHash(this))
      /* in case the current widget is the one which represents this, lets set the context
       * menu to undock */
      mainWin->setContextMenuDockText();
   else
      /* in case the current widget is not the one which represents this, lets set the
      * color back to black */
      mainWin->setTreeWidgetItemDockColor(this);

   /* this fixes my woes of getting the widget to show up on top when closed */
   event->ignore();

   /* put this widget on the top of the stack widget */
   m_parent->setCurrentWidget(this);

   /* Set the current tree widget item in the Page Selector window to the item 
    * which represents "this" */
   QTreeWidgetItem *item= mainWin->getFromHash(this);
   mainWin->treeWidget->setCurrentItem(item);
}

/*
 * The next three are virtual functions.  The idea here is that each subclass will have the
 * built in virtual function to override if the programmer wants to populate the window
 * when it it is first clicked.
 */
void Pages::PgSeltreeWidgetClicked()
{
}

void Pages::PgSeltreeWidgetDoubleClicked()
{
}

/*
 *  Virtual function which is called when this page is visible on the stack.
 *  This will be overridden by classes that want to populate if they are on the
 *  top.
 */
void Pages::currentStackItem()
{
}

/*
 * Function to close the stacked page and remove the widget from the
 * Page selector window
 */
void Pages::closeStackPage()
{
   /* First get the tree widget item and destroy it */
   QTreeWidgetItem *item=mainWin->getFromHash(this);
   /* remove the QTreeWidgetItem <-> page from the hash */
   mainWin->hashRemove(item, this);
   /* remove the item from the page selector by destroying it */
   delete item;
   /* remove this */
   delete this;
}

/*
 * Function to set members from the external mainwin
 */
void Pages::pgInitialize()
{
   m_parent = mainWin->stackedWidget;
   m_console = mainWin->currentConsole();
   QTreeWidgetItem *parentTreeWidgetItem = m_console->directorTreeItem();

   QTreeWidgetItem *item = new QTreeWidgetItem(parentTreeWidgetItem);
   QString name; 
   treeWidgetName(name);
   item->setText(0, name);
   mainWin->hashInsert(item, this);
}

/*
 * Virtual Function to return a name
 * All subclasses should override this function
 */
void Pages::treeWidgetName(QString &name)
{
   name = "Default Page Name";
}
