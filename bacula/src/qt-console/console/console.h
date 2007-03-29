#ifndef _CONSOLE_H_
#define _CONSOLE_H_
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
 *   Kern Sibbald, January 2007
 */

#include <QtGui>
#include "pages.h"
#include "ui_console.h"
#include "restore.h"
#include "select.h"

#ifndef MAX_NAME_LENGTH
#define MAX_NAME_LENGTH 128
#endif

/*
 * Structure for obtaining the defaults for a job
 */
struct job_defaults {
   QString job_name;
   QString pool_name;
   QString messages_name;
   QString client_name;
   QString store_name;
   QString where;
   QString level;
   QString type;
   QString fileset_name;
   QString catalog_name;
   bool enabled;
};

class DIRRES;
class BSOCK;
class JCR;
class CONRES;

class Console : public Pages, public Ui::ConsoleForm
{
   Q_OBJECT 

public:
   Console(QStackedWidget *parent);
   ~Console();
   void display_text(const char *buf);
   void display_text(const QString buf);
   void display_textf(const char *fmt, ...);
   void update_cursor(void);
   void write_dir(const char *buf);
   bool dir_cmd(const char *cmd, QStringList &results);
   bool authenticate_director(JCR *jcr, DIRRES *director, CONRES *cons);
   bool is_connected() { return m_sock != NULL; };
   const QFont get_font();
   void writeSettings();
   void readSettings();
   char *msg();
   void notify(bool enable);
   QStringList get_list(char *cmd);
   bool get_job_defaults(struct job_defaults &);
   void terminate();
   void beginNewCommand();
   void displayToPrompt();
   void discardToPrompt();
   void setDirRes(DIRRES *dir);

   QStringList job_list;
   QStringList client_list;
   QStringList fileset_list;
   QStringList messages_list;
   QStringList pool_list;
   QStringList storage_list;
   QStringList type_list;
   QStringList level_list;


public slots:
   void connect();                     
   void read_dir(int fd);
   int read(void);
   int write(const char *msg);
   int write(QString msg);
   void status_dir(void);
   void set_font(void);
   void poll_messages(void);

private:
   QTextEdit *m_textEdit;
   DIRRES *m_dir;
   BSOCK *m_sock;   
   bool m_at_prompt;
   QSocketNotifier *m_notifier;
   QTextCursor *m_cursor;
   bool m_api_set;
   bool m_messages_pending;
   QTimer *m_timer;
};

#endif /* _CONSOLE_H_ */
