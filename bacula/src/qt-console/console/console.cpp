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
 *  Console Class
 *
 *   Kern Sibbald, January MMVII
 *
 */ 

#include <QAbstractEventDispatcher>
#include "bat.h"
#include "console.h"

Console::Console(QStackedWidget *parent)
{
   QFont font;
   m_parent=parent;
   (void)parent;

   setupUi(this);
   m_sock = NULL;
   m_at_prompt = false;
   m_textEdit = textEdit;   /* our console screen */
   m_cursor = new QTextCursor(m_textEdit->document());
   mainWin->actionConnect->setIcon(QIcon(QString::fromUtf8("images/disconnected.png")));

   readSettings();
   /* Check for messages every 5 seconds */
// m_timer = new QTimer(this);
// QWidget::connect(m_timer, SIGNAL(timeout()), this, SLOT(poll_messages()));
// m_timer->start(5000);

}

void Console::poll_messages()
{
   m_messages_pending = true;
}

/* Terminate any open socket */
void Console::terminate()
{
   if (m_sock) {
      m_sock->close();
      m_sock = NULL;
   }
// m_timer->stop();
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
      mainWin->set_status("No Director found.");
      return;
   }
   if (m_sock) {
      mainWin->set_status("Already connected.");
      return;
   }

   memset(&jcr, 0, sizeof(jcr));

   mainWin->set_statusf(_("Connecting to Director %s:%d"), m_dir->address, m_dir->DIRport);
   display_textf(_("Connecting to Director %s:%d\n\n"), m_dir->address, m_dir->DIRport);

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
      m_treeItem->setForeground(0, greenBrush);
   }

   jcr.dir_bsock = m_sock;

   if (!authenticate_director(&jcr, m_dir, cons)) {
      display_text(m_sock->msg);
      return;
   }

   /* Give GUI a chance */
   app->processEvents();

   mainWin->set_status(_("Initializing ..."));

   /* Set up input notifier */
   m_notifier = new QSocketNotifier(m_sock->fd, QSocketNotifier::Read, 0);
   QObject::connect(m_notifier, SIGNAL(activated(int)), this, SLOT(read_dir(int)));

   write(".api 1");
   discardToPrompt();

   beginNewCommand();
   job_list = get_list(".jobs");
   client_list = get_list(".clients");
   fileset_list = get_list(".filesets");
   messages_list = get_list(".messages");
   pool_list = get_list(".pools");
   storage_list = get_list(".storage");
   type_list = get_list(".types");
   level_list = get_list(".levels");

   mainWin->set_status(_("Connected"));
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

   notify(false);
   write(cmd);
   while ((stat = read()) > 0) {
      strip_trailing_junk(msg());
      list << msg();
   }
   notify(true);
   return list;
}

/*  
 * Send a job name to the director, and read all the resulting
 *  defaults. 
 */
bool Console::get_job_defaults(struct job_defaults &job_defs)
{
   QString scmd;
   int stat;
   char *def;

   notify(false);
   beginNewCommand();
   scmd = QString(".defaults job=\"%1\"").arg(job_defs.job_name);
   write(scmd);
   while ((stat = read()) > 0) {
      def = strchr(msg(), '=');
      if (!def) {
         continue;
      }
      /* Pointer to default value */
      *def++ = 0;
      strip_trailing_junk(def);

      if (strcmp(msg(), "job") == 0) {
         if (strcmp(def, job_defs.job_name.toUtf8().data()) != 0) {
            goto bail_out;
         }
         continue;
      }
      if (strcmp(msg(), "pool") == 0) {
         job_defs.pool_name = def;
         continue;
      }
      if (strcmp(msg(), "messages") == 0) {
         job_defs.messages_name = def;
         continue;
      }
      if (strcmp(msg(), "client") == 0) {
         job_defs.client_name = def;
         continue;
      }
      if (strcmp(msg(), "storage") == 0) {
         job_defs.store_name = def;
         continue;
      }
      if (strcmp(msg(), "where") == 0) {
         job_defs.where = def;
         continue;
      }
      if (strcmp(msg(), "level") == 0) {
         job_defs.level = def;
         continue;
      }
      if (strcmp(msg(), "type") == 0) {
         job_defs.type = def;
         continue;
      }
      if (strcmp(msg(), "fileset") == 0) {
         job_defs.fileset_name = def;
         continue;
      }
      if (strcmp(msg(), "catalog") == 0) {
         job_defs.catalog_name = def;
         continue;
      }
      if (strcmp(msg(), "enabled") == 0) {
         job_defs.enabled = *def == '1' ? true : false;
         continue;
      }
   }

#ifdef xxx
   bsnprintf(cmd, sizeof(cmd), "job=%s pool=%s client=%s storage=%s where=%s\n"
      "level=%s type=%s fileset=%s catalog=%s enabled=%d\n",
      job_defs.job_name.toUtf8().data(), job_defs.pool_name.toUtf8().data(), 
      job_defs.client_name.toUtf8().data(), 
      job_defs.pool_name.toUtf8().data(), job_defs.messages_name.toUtf8().data(), 
      job_defs.store_name.toUtf8().data(),
      job_defs.where.toUtf8().data(), job_defs.level.toUtf8().data(), 
      job_defs.type.toUtf8().data(), job_defs.fileset_name.toUtf8().data(),
      job_defs.catalog_name.toUtf8().data(), job_defs.enabled);
#endif

   notify(true);
   return true;

bail_out:
   notify(true);
   return false;
}


/*
 * Save user settings associated with this console
 */
void Console::writeSettings()
{
   QFont font = get_font();

   QSettings settings("bacula.org", "bat");
   /* ***FIXME*** make console name unique */
   settings.beginGroup("Console");
   settings.setValue("consoleFont", font.family());
   settings.setValue("consolePointSize", font.pointSize());
   settings.setValue("consoleFixedPitch", font.fixedPitch());
   settings.endGroup();
}

/*
 * Read and restore user settings associated with this console
 */
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

/*
 * Set the console textEdit font
 */
void Console::set_font()
{
   bool ok;
   QFont font = QFontDialog::getFont(&ok, QFont(m_textEdit->font()), this);
   if (ok) {
      m_textEdit->setFont(font);
   }
}

/*
 * Get the console text edit font
 */
const QFont Console::get_font()
{
   return m_textEdit->font();
}


void Console::status_dir()
{
   write_dir("status dir\n");
   displayToPrompt();
}

/*
 * Put text into the console window
 */
void Console::display_textf(const char *fmt, ...)
{
   va_list arg_ptr;
   char buf[1000];
   int len;
   va_start(arg_ptr, fmt);
   len = bvsnprintf(buf, sizeof(buf), fmt, arg_ptr);
   va_end(arg_ptr);
   display_text(buf);
}

void Console::display_text(const QString buf)
{
   m_cursor->movePosition(QTextCursor::End);
   m_cursor->insertText(buf);
}


void Console::display_text(const char *buf)
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

/* 
 * This should be moved into a bSocket class 
 */
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
      mainWin->set_status(_("Processing command ..."));
      QApplication::setOverrideCursor(Qt::WaitCursor);
      write(msg);
   } else {
      mainWin->set_status(" Director not connected. Click on connect button.");
      mainWin->actionConnect->setIcon(QIcon(QString::fromUtf8("images/disconnected.png")));
      QBrush redBrush(Qt::red);
      m_treeItem->setForeground(0, redBrush);
      m_at_prompt = false;
   }
}

int Console::write(const QString msg)
{
   return write(msg.toUtf8().data());
}

int Console::write(const char *msg)
{
   m_sock->msglen = strlen(msg);
   pm_strcpy(&m_sock->msg, msg);
   m_at_prompt = false;
   if (commDebug) Pmsg1(000, "send: %s\n", msg);
   return m_sock->send();
}

/*
 * Get to main command prompt 
 */
void Console::beginNewCommand()
{
   write(".\n");
   while (read() > 0) {
   }
   write(".\n");
   while (read() > 0) {
   }
   write(".\n");
   while (read() > 0) {
   }
   display_text("\n");
}

void Console::displayToPrompt()
{ 
   int stat;
   if (commDebug) Pmsg0(000, "DisplaytoPrompt\n");
   while (!m_at_prompt) {
      if ((stat=read()) > 0) {
         display_text(msg());
      }
   }
   if (commDebug) Pmsg1(000, "endDisplaytoPrompt=%d\n", stat);
}

void Console::discardToPrompt()
{ 
   int stat;
   if (commDebug) Pmsg0(000, "discardToPrompt\n");
   while (!m_at_prompt) {
      stat = read();
   }
   if (commDebug) Pmsg1(000, "endDisplayToPrompt=%d\n", stat);
}


/* 
 * Blocking read from director
 */
int Console::read()
{
   int stat = 0;
   while (m_sock) {
      for (;;) {
         stat = bnet_wait_data_intr(m_sock, 1);
         if (stat > 0) {
            break;
         } 
         app->processEvents();
         if (m_api_set && m_messages_pending) {
            write_dir(".messages");
            m_messages_pending = false;
         }
      }
      stat = m_sock->recv();
      if (stat >= 0) {
         if (m_at_prompt) {
            display_text("\n");
            m_at_prompt = false;
         }
         if (commDebug) Pmsg1(000, "got: %s", m_sock->msg);
      }
      switch (m_sock->msglen) {
      case BNET_SERVER_READY:
         if (m_api_set && m_messages_pending) {
            write_dir(".messages");
            m_messages_pending = false;
         }
         m_at_prompt = true;
         continue;
      case BNET_MSGS_PENDING:
         if (commDebug) Pmsg0(000, "MSGS PENDING\n");
         m_messages_pending = true;
         continue;
      case BNET_CMD_OK:
         if (commDebug) Pmsg0(000, "CMD OK\n");
         m_at_prompt = false;
         continue;
      case BNET_CMD_BEGIN:
         if (commDebug) Pmsg0(000, "CMD BEGIN\n");
         m_at_prompt = false;
         continue;
      case BNET_PROMPT:
         if (commDebug) Pmsg0(000, "PROMPT\n");
         m_at_prompt = true;
         mainWin->set_status(_("At prompt waiting for input ..."));
         update_cursor();
         QApplication::restoreOverrideCursor();
         break;
      case BNET_CMD_FAILED:
         if (commDebug) Pmsg0(000, "CMD FAIL\n");
         mainWin->set_status(_("Command failed. At prompt waiting for input ..."));
         update_cursor();
         QApplication::restoreOverrideCursor();
         break;
      /* We should not get this one */
      case BNET_EOD:
         if (commDebug) Pmsg0(000, "EOD\n");
         mainWin->set_status_ready();
         update_cursor();
         QApplication::restoreOverrideCursor();
         if (!m_api_set) {
            break;
         }
         continue;
      case BNET_START_SELECT:
         new selectDialog(this);    
         break;
      case BNET_RUN_CMD:
         new runCmdDialog(this);
         break;
      case BNET_ERROR_MSG:
         m_sock->recv();              /* get the message */
         display_text(msg());
         QMessageBox::critical(this, "Error", msg(), QMessageBox::Ok);
         break;
      case BNET_WARNING_MSG:
         m_sock->recv();              /* get the message */
         display_text(msg());
         QMessageBox::critical(this, "Warning", msg(), QMessageBox::Ok);
         break;
      case BNET_INFO_MSG:
         m_sock->recv();              /* get the message */
         display_text(msg());
         mainWin->set_status(msg());
         break;
      }
      if (is_bnet_stop(m_sock)) {         /* error or term request */
         m_sock->close();
         m_sock = NULL;
         mainWin->actionConnect->setIcon(QIcon(QString::fromUtf8("images/disconnected.png")));
         QBrush redBrush(Qt::red);
         m_treeItem->setForeground(0, redBrush);
         m_notifier->setEnabled(false);
         delete m_notifier;
         m_notifier = NULL;
         mainWin->set_status(_("Director disconnected."));
         QApplication::restoreOverrideCursor();
         stat = BNET_HARDEOF;
      }
      break;
   } 
   return stat;
}

/* Called by signal when the Director has output for us */
void Console::read_dir(int fd)
{
   int stat;
   (void)fd;

   if (commDebug) Pmsg0(000, "read_dir\n");
   while ((stat = read()) >= 0) {
      display_text(msg());
   }
}

/*
 * When the notifier is enabled, read_dir() will automatically be
 * called by the Qt event loop when ever there is any output 
 * from the Directory, and read_dir() will then display it on
 * the console.
 *
 * When we are in a bat dialog, we want to control *all* output
 * from the Directory, so we set notify to off.
 *    m_console->notifiy(false);
 */
void Console::notify(bool enable) 
{ 
   m_notifier->setEnabled(enable);   
}

void Console::setTreeItem(QTreeWidgetItem *item)
{
   m_treeItem = item;
}

void Console::setDirRes(DIRRES *dir) 
{ 
   m_dir = dir;
}
