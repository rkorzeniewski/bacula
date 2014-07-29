/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2000-2014 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from many
   others, a complete list can be found in the file AUTHORS.

   You may use this file and others of this release according to the
   license defined in the LICENSE file, which includes the Affero General
   Public License, v3.0 ("AGPLv3") and some additional permissions and
   terms pursuant to its AGPLv3 Section 7.

   Bacula® is a registered trademark of Kern Sibbald.
*/
/*
 * Storage daemon specific defines and includes
 *
 */

#ifndef __STORED_H_
#define __STORED_H_

#define STORAGE_DAEMON 1

/* Set to debug Lock() and Unlock() only */
#define DEV_DEBUG_LOCK

/*
 * Set to define all SD locks except Lock()
 *  currently this does not work. More locks
 *  must be converted.
 */
#define SD_DEBUG_LOCK
#ifdef SD_DEBUG_LOCK
const int sd_dbglvl = 300;
#else
const int sd_dbglvl = 300;
#endif

#ifdef HAVE_MTIO_H
#include <mtio.h>
#else
# ifdef HAVE_SYS_MTIO_H
# include <sys/mtio.h>
# else
#   ifdef HAVE_SYS_TAPE_H
#   include <sys/tape.h>
#   else
    /* Needed for Mac 10.6 (Snow Leopard) */
#   include "lib/bmtio.h"
#   endif
# endif
#endif
#include "lock.h"
#include "block.h"
#include "record.h"
#include "dev.h"
#include "file_dev.h"
#include "tape_dev.h"
#include "stored_conf.h"
#include "bsr.h"
#include "jcr.h"
#include "vol_mgr.h"
#include "reserve.h"
#include "protos.h"
#ifdef HAVE_LIBZ
#include <zlib.h>                     /* compression headers */
#else
#define uLongf uint32_t
#endif
#ifdef HAVE_FNMATCH
#include <fnmatch.h>
#else
#include "lib/fnmatch.h"
#endif
#ifdef HAVE_DIRENT_H
#include <dirent.h>
#define NAMELEN(dirent) (strlen((dirent)->d_name))
#endif
#ifndef HAVE_READDIR_R
int readdir_r(DIR *dirp, struct dirent *entry, struct dirent **result);
#endif

#include "file_dev.h"
#include "tape_dev.h"
#include "vtape_dev.h"
#include "sd_plugins.h"

/* Daemon globals from stored.c */
extern STORES *me;                    /* "Global" daemon resource */
extern bool forge_on;                 /* proceed inspite of I/O errors */
extern pthread_mutex_t device_release_mutex;
extern pthread_cond_t wait_device_release; /* wait for any device to be released */

#endif /* __STORED_H_ */
