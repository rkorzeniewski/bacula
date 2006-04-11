/*
 * Bacula File Daemon specific configuration and defines
 *
 *     Kern Sibbald, Jan MMI
 *
 *   Version $Id$
 */
/*
   Copyright (C) 2001-2006 Kern Sibbald

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   version 2 as amended with additional clauses defined in the
   file LICENSE in the main source directory.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the 
   the file LICENSE for additional details.

 */

#define FILE_DAEMON 1
#include "filed_conf.h"
#include "findlib/find.h"
#include "jcr.h"
#include "acl.h"
#include "protos.h"                   /* file daemon prototypes */
#ifdef HAVE_LIBZ
#include <zlib.h>                     /* compression headers */
#else
#define uLongf uint32_t
#endif

extern const int win32_client;              /* Are we running on Windows? */

extern CLIENT *me;                    /* "Global" Client resource */
