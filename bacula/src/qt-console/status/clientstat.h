#ifndef _CLIENTSTAT_H_
#define _CLIENTSTAT_H_
/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2007-2014 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from many
   others, a complete list can be found in the file AUTHORS.

   You may use this file and others of this release according to the
   license defined in the LICENSE file, which includes the Affero General
   Public License, v3.0 ("AGPLv3") and some additional permissions and
   terms pursuant to its AGPLv3 Section 7.

   Bacula® is a registered trademark of Kern Sibbald.
*/
/*
 *   Version $Id: clientstat.h 5372 2007-08-17 12:17:04Z kerns $
 *
 *   Dirk Bartley, March 2007
 */

#include <QtGui>
#include "ui_clientstat.h"
#include "console.h"
#include "pages.h"

class ClientStat : public Pages, public Ui::ClientStatForm
{
   Q_OBJECT

public:
   ClientStat(QString&, QTreeWidgetItem*);
   ~ClientStat();
   virtual void PgSeltreeWidgetClicked();
   virtual void currentStackItem();

public slots:
   void populateHeader();
   void populateTerminated();
   void populateRunning();
   void populateAll();
   void populateCurrentTab(int);

private slots:
   void timerTriggered();

private:
   void createConnections();
   void writeSettings();
   void readSettings();
   bool m_populated;
   QTextCursor *m_cursor;
   void getFont();
   QString m_groupText, m_splitText;
   QTimer *m_timer;
   QString m_client;
};

#endif /* _CLIENTSTAT_H_ */
