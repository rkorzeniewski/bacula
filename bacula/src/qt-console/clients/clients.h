#ifndef _CLIENTS_H_
#define _CLIENTS_H_
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
 *   Version $Id$
 *
 *   Dirk Bartley, March 2007
 */

#include <QtGui>
#include "ui_clients.h"
#include "console.h"
#include "pages.h"

class Clients : public Pages, public Ui::ClientForm
{
   Q_OBJECT

public:
   Clients();
   ~Clients();
   virtual void PgSeltreeWidgetClicked();
   virtual void currentStackItem();

public slots:
   void tableItemChanged(QTableWidgetItem *, QTableWidgetItem *);

private slots:
   void populateTable();
   void showJobs();
   void consoleStatusClient();
   void statusClientWindow();
   void consolePurgeJobs();
   void prune();

private:
   void createContextMenu();
   void settingsOpenStatus(QString& client);
   QString m_currentlyselected;
   bool m_populated;
   bool m_firstpopulation;
   bool m_checkcurwidget;
};

#endif /* _CLIENTS_H_ */
