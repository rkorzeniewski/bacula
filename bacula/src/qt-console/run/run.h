
#ifndef _RUN_H_
#define _RUN_H_

#include <QtGui>
#include "ui_run.h"
#include "ui_runcmd.h"
#include "console.h"

class runPage : public Pages, public Ui::runForm
{
   Q_OBJECT 

public:
   runPage();

public slots:
   void okButtonPushed();
   void cancelButtonPushed();
   void job_name_change(int index);

private:
};

class runCmdPage : public Pages, public Ui::runCmdForm
{
   Q_OBJECT 

public:
   runCmdPage();

public slots:
   void okButtonPushed();
   void cancelButtonPushed();

private:
   void fill();
   QString m_dtformat;
};


#endif /* _RUN_H_ */
