#ifndef _JOB_H_
#define _JOB_H_
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

#include <QtGui>
#include "ui_job.h"
#include "console.h"

class Job : public Pages, public Ui::JobForm
{
   Q_OBJECT

public:
   Job(QString &jobId, QTreeWidgetItem *parentTreeWidgetItem);

public slots:
   void populateAll();
   void deleteJob();
   void cancelJob();
   void showInfoVolume(QListWidgetItem *);
   void rerun();

private slots:

private:
   void updateRunInfo();
   void populateText();
   void populateForm();
   void populateVolumes();
   void getFont();
   QTextCursor *m_cursor;
   QString m_jobId;
   QString m_client;
   QTimer *m_timer;
};

#endif /* _JOB_H_ */
