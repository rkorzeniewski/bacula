#ifndef _MEDIAINFO_H_
#define _MEDIAINFO_H_
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
 */

#include <QtGui>
#include "ui_mediainfo.h"
#include "console.h"
#include "pages.h"

class MediaInfo : public Pages, public Ui::mediaInfoForm
{
   Q_OBJECT

public:
   MediaInfo(QTreeWidgetItem *parentWidget, QString &mediaId);

private slots:
   void pruneVol();
   void purgeVol();
   void deleteVol();
   void editVol();
   void showInfoForJob(QTableWidgetItem * item);

private:
   void populateForm();
   QString m_mediaName;
   QString m_mediaId;
};

#endif /* _MEDIAINFO_H_ */
