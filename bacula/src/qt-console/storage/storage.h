#ifndef _STORAGE_H_
#define _STORAGE_H_
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
#include "ui_storage.h"
#include "console.h"
#include "pages.h"

class Storage : public Pages, public Ui::StorageForm
{
   Q_OBJECT

public:
   Storage();
   ~Storage();
   virtual void PgSeltreeWidgetClicked();
   virtual void currentStackItem();

public slots:
   void treeItemChanged(QTreeWidgetItem *, QTreeWidgetItem *);

private slots:
   void populateTree();
   void consoleStatusStorage();
   void consoleLabelStorage();
   void consoleMountStorage();
   void consoleUnMountStorage();
   void consoleUpdateSlots();
   void consoleUpdateSlotsScan();
   void consoleRelease();
   void statusStorageWindow();
   void contentWindow();

private:
   void createContextMenu();
   void mediaList(QTreeWidgetItem *parent, const QString &storageID);
   void settingsOpenStatus(QString& storage);
   QString m_currentStorage;
   bool m_currentAutoChanger;
   bool m_populated;
   bool m_firstpopulation;
   bool m_checkcurwidget;
   void writeExpandedSettings();
   QTreeWidgetItem *m_topItem;
};

void table_get_selection(QTableWidget *table, QString &sel);

#endif /* _STORAGE_H_ */
