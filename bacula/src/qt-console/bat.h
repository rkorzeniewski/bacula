
#include "mainwindow.h"
#include "config.h"
#include "bacula.h"
#include "bat_conf.h"
#include "jcr.h"

extern MainWindow *mainWin;
extern QApplication *app;

void set_textf(const char *fmt, ...);
void set_text(const char *buf);

int bvsnprintf(char *str, int32_t size, const char *format, va_list ap);
