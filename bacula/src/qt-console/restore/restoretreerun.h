
#ifndef _RESTORETREERUN_H_
#define _RESTORETREERUN_H_

#include <QtGui>
#include "ui_restoretreerun.h"
#include "console.h"

class restoreTreeRunPage : public Pages, public Ui::restoreTreeRunForm
{
   Q_OBJECT 

public:
   restoreTreeRunPage(QString &, QString &, QList<int> &, QTreeWidgetItem*);

public slots:
   void okButtonPushed();
   void cancelButtonPushed();

private:
   void fill();
   QString m_tempTable;
   QString m_client;
   QList<int> m_jobList;
};

#endif /* _RESTORETREERUN_H_ */
