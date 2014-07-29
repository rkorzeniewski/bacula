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
 * Kern Sibbald, February MMVII
 */

#ifndef _RELABEL_H_
#define _RELABEL_H_

#include <QtGui>
#include "ui_relabel.h"
#include "console.h"

class relabelDialog : public QDialog, public Ui::relabelForm
{
   Q_OBJECT

public:
   relabelDialog(Console *console, QString &fromVolume);

private:
   int getDefs(QStringList &fieldlist);

private slots:
   void accept();
   void reject();

private:
   Console *m_console;
   QString m_fromVolume;
   int m_conn;
};

#endif /* _RELABEL_H_ */
