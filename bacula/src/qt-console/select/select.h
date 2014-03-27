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

#ifndef _SELECT_H_
#define _SELECT_H_

#include <QtGui>
#include "ui_select.h"
#include "console.h"

class selectDialog : public QDialog, public Ui::selectForm
{
   Q_OBJECT

public:
   selectDialog(Console *console, int conn);

public slots:
   void accept();
   void reject();
   void index_change(int index);

private:
   Console *m_console;
   int m_index;
   int m_conn;
};

class yesnoPopUp : public QDialog
{
   Q_OBJECT

public:
   yesnoPopUp(Console *console, int conn);

};


#endif /* _SELECT_H_ */
