
#ifndef _RESTORE_H_
#define _RESTORE_H_

#include <QtGui>
#include "ui_brestore.h"

class bRestore : public QWidget, public Ui::bRestoreForm
{
   Q_OBJECT 

public:
   bRestore(QStackedWidget *parent);

public slots:

private:

};

#endif /* _RESTORE_H_ */
