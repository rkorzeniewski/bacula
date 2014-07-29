#ifndef _JOBLOG_H_
#define _JOBLOG_H_
/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2000-2014 Free Software Foundation Europe e.V.

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
#include "ui_joblog.h"
#include "console.h"

class JobLog : public Pages, public Ui::JobLogForm
{
   Q_OBJECT

public:
   JobLog(QString &jobId, QTreeWidgetItem *parentTreeWidgetItem);

public slots:

private slots:

private:
   void populateText();
   void getFont();
   QTextCursor *m_cursor;
   QString m_jobId;
};

#endif /* _JOBLOG_H_ */
