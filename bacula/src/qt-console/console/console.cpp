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

#include <QAbstractEventDispatcher>
#include "bat.h"
#include "console.h"
#include "restore.h"

Console::Console(QStackedWidget *parent)
{
   QFont font;
   QTreeWidgetItem *item, *topItem;
   QTreeWidget *treeWidget = mainWin->treeWidget;

   setupUi(this);
   parent->addWidget(this);
   m_sock = NULL;
   m_at_prompt = false;
   m_textEdit = textEdit;   /* our console screen */
   m_cursor = new QTextCursor(m_textEdit->document());
   mainWin->actionConnect->setIcon(QIcon(QString::fromUtf8("images/disconnected.png")));

   bRestore *restore = new bRestore(parent);
   restore->setupUi(restore);
   parent->addWidget(restore);

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
   topItem->setIcon(0, QIcon(QString::fromUtf8("images/server.png")));
   item = new QTreeWidgetItem(topItem);
   m_consoleItem = item;
   item->setText(0, "Console");
   item->setText(1, "0");
   QBrush redBrush(Qt::red);
   item->setForeground(0, redBrush);
   item = new QTreeWidgetItem(topItem);
   item->setText(0, "Restore");
   item->setText(1, "1");
   treeWidget->expandItem(topItem);

   readSettings();

}

/*
 * Connect to Director. If there are more than one, put up
 * a modal dialog so that the user chooses one.
 */
void Console::connect()
{
   JCR jcr;

   m_textEdit = textEdit;   /* our console screen */

   if (!m_dir) {          
      mainWin->set_status(" No Director found.");
      return;
   }
   if (m_sock) {
      mainWin->set_status(" Already connected.");
      return;
   }

   memset(&jcr, 0, sizeof(jcr));

   mainWin->set_statusf(_(" Connecting to Director %s:%d"), m_dir->address, m_dir->DIRport);
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
      mainWin->set_status("Connection failed");
      return;
   } else {
      /* Update page selector to green to indicate that Console is connected */
      mainWin->actionConnect->setIcon(QIcon(QString::fromUtf8("images/connected.png")));
      QBrush greenBrush(Qt::green);
      m_consoleItem->setForeground(0, greenBrush);
   }

   jcr.dir_bsock = m_sock;

   if (!authenticate_director(&jcr, m_dir, cons)) {
      set_text(m_sock->msg);
      return;
   }

   /* Give GUI a chance */
   app->processEvents();

   mainWin->set_status(_(" Initializing ..."));

   /* Set up input notifier */
   m_notifier = new QSocketNotifier(m_sock->fd, QSocketNotifier::Read, 0);
   QObject::connect(m_notifier, SIGNAL(activated(int)), this, SLOT(read_dir(int)));

   job_list = get_list(".jobs");
   client_list = get_list(".clients");
   fileset_list = get_list(".filesets");
   messages_list = get_list(".messages");
   pool_list = get_list(".pools");
   storage_list = get_list(".storage");
   type_list = get_list(".types");
   level_list = get_list(".levels");

   mainWin->set_status(_(" Connected"));
   return;
}


/*  
 * Send a command to the director, and read all the resulting
 *  output into a list.
 */
QStringList Console::get_list(char *cmd)
{
   QStringList list;
   int stat;

   setEnabled(false);
   write(cmd);
   while ((stat = read()) > 0) {
      strip_trailing_junk(msg());
      list << msg();
   }
   setEnabled(true);
   list.sort();
   return list;
}


void Console::writeSettings()
{
   QFont font = get_font();

   QSettings settings("bacula.org", "bat");
   settings.beginGroup("Console");
   settings.setValue("consoleFont", font.family());
   settings.setValue("consolePointSize", font.pointSize());
   settings.setValue("consoleFixedPitch", font.fixedPitch());
   settings.endGroup();
}

void Console::readSettings()
{ 
   QFont font = get_font();

   QSettings settings("bacula.org", "bat");
   settings.beginGroup("Console");
   font.setFamily(settings.value("consoleFont", "Courier").value<QString>());
   font.setPointSize(settings.value("consolePointSize", 10).toInt());
   font.setFixedPitch(settings.value("consoleFixedPitch", true).toBool());
   settings.endGroup();
   m_textEdit->setFont(font);
}

void Console::set_font()
{
   bool ok;
   QFont font = QFontDialog::getFont(&ok, QFont(m_textEdit->font()), this);
   if (ok) {
      m_textEdit->setFont(font);
   }
}

const QFont Console::get_font()
{
   return m_textEdit->font();
}


void Console::status_dir()
{
   write_dir("status dir\n");
}

void Console::set_textf(const char *fmt, ...)
{
   va_list arg_ptr;
   char buf[1000];
   int len;
   va_start(arg_ptr, fmt);
   len = bvsnprintf(buf, sizeof(buf), fmt, arg_ptr);
   va_end(arg_ptr);
   set_text(buf);
}

void Console::set_text(const QString buf)
{
   m_cursor->movePosition(QTextCursor::End);
   m_cursor->insertText(buf);
}


void Console::set_text(const char *buf)
{
   m_cursor->movePosition(QTextCursor::End);
   m_cursor->insertText(buf);
}

/* Position cursor to end of screen */
void Console::update_cursor()
{
   QApplication::restoreOverrideCursor();
   m_textEdit->moveCursor(QTextCursor::End);
   m_textEdit->ensureCursorVisible();
}

char *Console::msg()
{
   if (m_sock) {
      return m_sock->msg;
   }
   return NULL;
}

/* Send a command to the Director */
void Console::write_dir(const char *msg)
{
   if (m_sock) {
      m_at_prompt = false;
      mainWin->set_status(_(" Processing command ..."));
      QApplication::setOverrideCursor(Qt::WaitCursor);
      m_sock->msglen = strlen(msg);
      pm_strcpy(&m_sock->msg, msg);
      bnet_send(m_sock);
   } else {
      mainWin->set_status(" Director not connected. Click on connect button.");
      mainWin->actionConnect->setIcon(QIcon(QString::fromUtf8("images/disconnected.png")));
      QBrush redBrush(Qt::red);
      m_consoleItem->setForeground(0, redBrush);
   }
}

int Console::write(const char *msg)
{
   m_sock->msglen = strlen(msg);
   pm_strcpy(&m_sock->msg, msg);
   return bnet_send(m_sock);
}

/* 
 * Blocking read from director */
int Console::read()
{
   int stat;
   if (m_sock) {
      for (;;) {
         stat = bnet_wait_data_intr(m_sock, 1);
         if (stat > 0) {
            break;
         } 
         app->processEvents();
         if (stat < 0) {
            return BNET_ERROR;
         }
      }
      return bnet_recv(m_sock);
   } 
   return BNET_HARDEOF;
}

/* Called by signal when the Director has output for us */
void Console::read_dir(int fd)
{
   int stat;
   (void)fd;

   if (!m_sock) {
      return;
   }
   stat = bnet_recv(m_sock);
   if (stat >= 0) {
      if (m_at_prompt) {
         set_text("\n");
         m_at_prompt = false;
      }
      set_text(m_sock->msg);
      return;
   }
   if (is_bnet_stop(m_sock)) {         /* error or term request */
      bnet_close(m_sock);
      m_sock = NULL;
      mainWin->actionConnect->setIcon(QIcon(QString::fromUtf8("images/disconnected.png")));
      QBrush redBrush(Qt::red);
      m_consoleItem->setForeground(0, redBrush);
      m_notifier->setEnabled(false);
      delete m_notifier;
      m_notifier = NULL;
      mainWin->set_status(_(" Director disconnected."));
      QApplication::restoreOverrideCursor();
      return;
   }
   /* Must be a signal -- either do something or ignore it */
   if (m_sock->msglen == BNET_PROMPT) {
      m_at_prompt = true;
      mainWin->set_status(_(" At prompt waiting for input ..."));
      update_cursor();
   }
   if (m_sock->msglen == BNET_EOD) {
      mainWin->set_status_ready();
      update_cursor();
   }
   return;
}
