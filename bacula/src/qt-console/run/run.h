
#ifndef _RUN_H_
#define _RUN_H_

#include <QtGui>
#include "ui_run.h"
#include "ui_runcmd.h"
#include "console.h"

class runDialog : public QDialog, public Ui::runForm
{
   Q_OBJECT 

public:
   runDialog(Console *console);

public slots:
   void accept();
   void reject();
   void job_name_change(int index);

private:
   Console *m_console;

};

class runCmdDialog : public QDialog, public Ui::runCmdForm
{
   Q_OBJECT 

public:
   runCmdDialog(Console *console);

public slots:
   void accept();
   void reject();

private:
   void fillRunDialog();

   Console *m_console;
};


#endif /* _RUN_H_ */
