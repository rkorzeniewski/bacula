
#ifndef _SELECT_H_
#define _SELECT_H_

#include <QtGui>
#include "ui_select.h"
#include "console.h"

class selectDialog : public QDialog, public Ui::selectForm
{
   Q_OBJECT 

public:
   selectDialog(Console *console);

public slots:
   void accept();
   void reject();
   void index_change(int index);

private:
   Console *m_console;
   int m_index;

};

#endif /* _SELECT_H_ */
