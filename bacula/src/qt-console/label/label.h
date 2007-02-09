
#ifndef _LABEL_H_
#define _LABEL_H_

#include <QtGui>
#include "ui_label.h"
#include "console.h"

class labelDialog : public QDialog, public Ui::labelVolume
{
   Q_OBJECT 

public:
   labelDialog(Console *console);

public slots:
   void accept();
   void reject();

private:

};

#endif /* _LABEL_H_ */
