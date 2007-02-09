
#ifndef _RUN_H_
#define _RUN_H_

#include <QtGui>
#include "ui_run.h"
#include "console.h"

class runDialog : public QDialog, public Ui::runForm
{
   Q_OBJECT 

public:
   runDialog(Console *console);

public slots:
   void accept();
   void reject();

private:

};

#endif /* _RUN_H_ */
