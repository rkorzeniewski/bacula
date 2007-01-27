
#ifndef _CONSOLE_H_
#define _CONSOLE_H_

#include "mainwindow.h"
#include "config.h"
#include "bacula.h"
#include "console_conf.h"
#include "jcr.h"

extern MainWindow *mainWin;
extern QApplication *app;

class Console(QWidget *textEdit)
{
public:
   Console();
   bool connect();
// void setDirector()
// void write();
// void read();
   void set_text(const char *buf);
   void set_textf(const char *fmt, ...);
private:
   QWidget *m_textEdit;
   DIRRES *m_dir;
   BSOCK *m_sock;   
};



void set_textf(const char *fmt, ...);
void set_text(const char *buf);

int bvsnprintf(char *str, int32_t size, const char *format, va_list ap);

#endif /* _CONSOLE_H_ */
