/*
 * Storage daemon specific defines and includes
 *
 *  Version $Id$
 */
/*
   Copyright (C) 2000-2005 Kern Sibbald

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of
   the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA.

 */

#ifndef __STORED_H_
#define __STORED_H_

#define STORAGE_DAEMON 1

#ifdef HAVE_MTIO_H
#include <mtio.h>
#else
# ifdef HAVE_SYS_MTIO_H
# include <sys/mtio.h>
# else
#   ifdef HAVE_SYS_TAPE
#   include <sys/tape.h>
#   endif
# endif
#endif
#include "block.h"
#include "record.h"
#include "dev.h"
#include "stored_conf.h"
#include "bsr.h"
#include "jcr.h"
#include "protos.h"
#ifdef HAVE_LIBZ
#include <zlib.h>		      /* compression headers */
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

/* Daemon globals from stored.c */
extern STORES *me;                    /* "Global" daemon resource */
extern bool forge_on;		      /* proceed inspite of I/O errors */
extern pthread_mutex_t device_release_mutex;
extern pthread_cond_t wait_device_release; /* wait for any device to be released */			      

#ifdef debug_tracing
extern int _rewind_dev(char *file, int line, DEVICE *dev);
#define rewind_dev(d) _rewind_dev(__FILE__, __LINE__, (d))
#endif

#endif /* __STORED_H_ */
