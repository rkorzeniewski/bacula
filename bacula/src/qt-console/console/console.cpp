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
 *  Console Class
 *
 *   Kern Sibbald, January MMVI
 *
 */ 

#include "bat.h"
#include "console.h"

Console::Console()
{
   QTreeWidgetItem *item, *topItem;
   QTreeWidget *treeWidget = mainWin->treeWidget;

   m_sock = NULL;
   m_textEdit = mainWin->textEdit;   /* our console screen */

   /* Just take the first Director */
   LockRes();
   m_dir = (DIRRES *)GetNextRes(R_DIRECTOR, NULL);
   UnlockRes();

   /* Dummy setup of treeWidget */
   treeWidget->clear();
   treeWidget->setColumnCount(1);
   treeWidget->setHeaderLabel("Selection");
   topItem = new QTreeWidgetItem(treeWidget);
   topItem->setText(0, m_dir->name());
   item = new QTreeWidgetItem(topItem);
   item->setText(0, "Console");
   item->setText(1, "0");
   item = new QTreeWidgetItem(topItem);
   item->setText(0, "Restore");
   item->setText(1, "1");
   treeWidget->expandItem(topItem);
}

/*
 * Connect to Director. If there are more than one, put up
 * a modal dialog so that the user chooses one.
 */
void Console::connect()
{
   JCR jcr;

   m_textEdit = mainWin->textEdit;   /* our console screen */

   if (!m_dir) {          
      set_text("No Director to connect to.\n");
      return;
   }
   if (m_sock) {
      set_text("Already connected.\n");
      return;
   }

   memset(&jcr, 0, sizeof(jcr));

   set_statusf(_(" Connecting to Director %s:%d"), m_dir->address, m_dir->DIRport);
   set_textf(_("Connecting to Director %s:%d\n\n"), m_dir->address, m_dir->DIRport);

   /* Give GUI a chance */
   app->processEvents();
   
   LockRes();
   /* If cons==NULL, default console will be used */
   CONRES *cons = (CONRES *)GetNextRes(R_CONSOLE, (RES *)NULL);
   UnlockRes();

   m_sock = bnet_connect(NULL, 5, 15, _("Director daemon"), m_dir->address,
                          NULL, m_dir->DIRport, 0);
   if (m_sock == NULL) {
      set_text("Connection failed\n");
      return;
   }

   jcr.dir_bsock = m_sock;

   if (!authenticate_director(&jcr, m_dir, cons)) {
      set_text(m_sock->msg);
      return;
   }

   /* Give GUI a chance */
   app->processEvents();

   set_status(_(" Initializing ..."));

   bnet_fsend(m_sock, "autodisplay on");

   /* Read and display all initial messages */
   while (bnet_recv(m_sock) > 0) {
      set_text(m_sock->msg);
   }

   /* Give GUI a chance */
   app->processEvents();

   /* Query Directory for .jobs, .clients, .filesets, .msgs,
    *  .pools, .storage, .types, .levels, ...
    */

   set_status(_(" Connected"));
   set_text("Connected\n");

   return;
}

void Console::set_textf(const char *fmt, ...)
{
   va_list arg_ptr;
   char buf[1000];
   int len;
   va_start(arg_ptr, fmt);
   len = bvsnprintf(buf, sizeof(buf), fmt, arg_ptr);
   va_end(arg_ptr);
   m_textEdit->append(buf);
}

void Console::set_text(const char *buf)
{
   m_textEdit->append(buf);
}

void Console::set_statusf(const char *fmt, ...)
{
   va_list arg_ptr;
   char buf[1000];
   int len;
   va_start(arg_ptr, fmt);
   len = bvsnprintf(buf, sizeof(buf), fmt, arg_ptr);
   va_end(arg_ptr);
   set_status(buf);
}

void Console::set_status_ready()
{
   mainWin->statusBar()->showMessage("Ready");
// ready = true;
}

void Console::set_status(const char *buf)
{
   mainWin->statusBar()->showMessage(buf);
   set_text(buf);
// ready = false;
}


#ifdef xxx
void write_director(const gchar *msg)
{
   if (UA_sock) {
      at_prompt = false;
      set_status(_(" Processing command ..."));
      UA_sock->msglen = strlen(msg);
      pm_strcpy(&UA_sock->msg, msg);
      bnet_send(UA_sock);
   }
   if (strcmp(msg, ".quit") == 0 || strcmp(msg, ".exit") == 0) {
      disconnect_from_director((gpointer)NULL);
      gtk_main_quit();
   }
}

extern "C"
void read_director(gpointer data, gint fd, GdkInputCondition condition)
{
   int stat;

   if (!UA_sock || UA_sock->fd != fd) {
      return;
   }
   stat = bnet_recv(UA_sock);
   if (stat >= 0) {
      if (at_prompt) {
         set_text("\n", 1);
         at_prompt = false;
      }
      set_text(UA_sock->msg, UA_sock->msglen);
      return;
   }
   if (is_bnet_stop(UA_sock)) {         /* error or term request */
      gtk_main_quit();
      return;
   }
   /* Must be a signal -- either do something or ignore it */
   if (UA_sock->msglen == BNET_PROMPT) {
      at_prompt = true;
      set_status(_(" At prompt waiting for input ..."));
   }
   if (UA_sock->msglen == BNET_EOD) {
      set_status_ready();
   }
   return;
}

static gint tag;

void start_director_reader(gpointer data)
{

   if (director_reader_running || !UA_sock) {
      return;
   }
   tag = gdk_input_add(UA_sock->fd, GDK_INPUT_READ, read_director, NULL);
   director_reader_running = true;
}

void stop_director_reader(gpointer data)
{
   if (!director_reader_running) {
      return;
   }
   gdk_input_remove(tag);
   director_reader_running = false;
}
#endif
