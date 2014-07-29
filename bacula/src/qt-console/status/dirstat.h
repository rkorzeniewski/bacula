#ifndef _DIRSTAT_H_
#define _DIRSTAT_H_
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
 *   Version $Id: dirstat.h 5372 2007-08-17 12:17:04Z kerns $
 *
 *   Dirk Bartley, March 2007
 */

#include <QtGui>
#include "ui_dirstat.h"
#include "console.h"
#include "pages.h"

class DirStat : public Pages, public Ui::DirStatForm
{
   Q_OBJECT

public:
   DirStat();
   ~DirStat();
   virtual void PgSeltreeWidgetClicked();
   virtual void currentStackItem();

public slots:
   void populateHeader();
   void populateTerminated();
   void populateScheduled();
   void populateRunning();
   void populateAll();

private slots:
   void timerTriggered();
   void consoleCancelJob();
   void consoleDisableJob();

private:
   void createConnections();
   void writeSettings();
   void readSettings();
   bool m_populated;
   QTextCursor *m_cursor;
   void getFont();
   QString m_groupText, m_splitText;
   QTimer *m_timer;
};

#endif /* _DIRSTAT_H_ */
