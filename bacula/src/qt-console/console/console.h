#ifndef _CONSOLE_H_
#define _CONSOLE_H_

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
 *   Kern Sibbald, January 2007
 */

#include <QtGui>
#include "ui_console.h"
#include "restore.h"

#ifndef MAX_NAME_LENGTH
#define MAX_NAME_LENGTH 128
#endif

struct job_defaults {
   char job_name[MAX_NAME_LENGTH];
   char pool_name[MAX_NAME_LENGTH];
   char messages_name[MAX_NAME_LENGTH];
   char client_name[MAX_NAME_LENGTH];
   char store_name[MAX_NAME_LENGTH];
   char where[MAX_NAME_LENGTH];
   char level[MAX_NAME_LENGTH];
   char type[MAX_NAME_LENGTH];
   char fileset_name[MAX_NAME_LENGTH];
   char catalog_name[MAX_NAME_LENGTH];
   bool enabled;
};

class DIRRES;
class BSOCK;
class JCR;
class CONRES;

class Console : public QWidget, public Ui::ConsoleForm
{
   Q_OBJECT 

public:
   Console(QStackedWidget *parent);
   void set_text(const char *buf);
   void set_text(const QString buf);
   void set_textf(const char *fmt, ...);
   void update_cursor(void);
   void write_dir(const char *buf);
   bool authenticate_director(JCR *jcr, DIRRES *director, CONRES *cons);
   bool is_connected() { return m_sock != NULL; };
   const QFont get_font();
   void writeSettings();
   void readSettings();
   char *msg();
   void setEnabled(bool enable) { m_notifier->setEnabled(enable); };
   QStringList get_list(char *cmd);
   bool get_job_defaults(struct job_defaults &);

   QStringList job_list;
   QStringList client_list;
   QStringList fileset_list;
   QStringList messages_list;
   QStringList pool_list;
   QStringList storage_list;
   QStringList type_list;
   QStringList level_list;

public slots:
   void connect(void);
   void read_dir(int fd);
   int read(void);
   int write(const char *msg);
   void status_dir(void);
   void set_font(void);

private:
   QTextEdit *m_textEdit;
   DIRRES *m_dir;
   BSOCK *m_sock;   
   bool m_at_prompt;
   QSocketNotifier *m_notifier;
   QTextCursor *m_cursor;
   QTreeWidgetItem *m_consoleItem;
};

#endif /* _CONSOLE_H_ */
