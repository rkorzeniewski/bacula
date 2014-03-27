#ifndef _BAT_H_
#define _BAT_H_

/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2007-2014 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from many
   others, a complete list can be found in the file AUTHORS.

   You may use this file and others of this release according to the
   license defined in the LICENSE file, which includes the Affero General
   Public License, v3.0 ("AGPLv3") and some additional permissions and
   terms pursuant to its AGPLv3 Section 7.

   Bacula® is a registered trademark of Kern Sibbald.
*/
/*
 *   Kern Sibbald, January 2007
 */

#if defined(HAVE_WIN32)
#if !defined(_STAT_H)
#define _STAT_H       /* don't pull in MinGW stat.h */
#define _STAT_DEFINED /* don't pull in MinGW stat.h */
#endif
#endif

#if defined(HAVE_WIN32)
#if defined(HAVE_MINGW)
#include "mingwconfig.h"
#else
#include "winconfig.h"
#endif
#else
#include "config.h"
#endif
#define __CONFIG_H

#include <QtGui>
#include <QtCore>
#include "bacula.h"
#include "mainwin.h"
#include "bat_conf.h"
#include "jcr.h"
#include "console.h"

extern MainWin *mainWin;
extern QApplication *app;

bool isWin32Path(QString &fullPath);

#endif /* _BAT_H_ */
