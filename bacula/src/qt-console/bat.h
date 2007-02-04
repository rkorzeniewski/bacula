
#ifndef _BAT_H_
#define _BAT_H_

#include "mainwin.h"
#include "config.h"
#include "bacula.h"
#include "bat_conf.h"
#include "jcr.h"
#include "console.h"
#include "qstd.h"

using namespace qstd;

extern MainWin *mainWin;
extern QApplication *app;

void set_textf(const char *fmt, ...);
void set_text(const char *buf);

int bvsnprintf(char *str, int32_t size, const char *format, va_list ap);

#endif /* _BAT_H_ */
