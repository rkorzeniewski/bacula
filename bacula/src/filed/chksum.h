/*
 * General routines for handling the various checksum supported.
 */

/*
   Copyright (C) 2004 Kern Sibbald

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

#ifndef _CHKSUM_H_
#define _CHKSUM_H_

#include "bacula.h"

/*
 * Link these to findlib options. Doing so allows for simpler handling of
 * signatures in the callers.
 * If multiple signatures are specified, the order in chksum_init() matters.
 * Still, spell out our own names in case we want to change the approach.
 */
#define CHKSUM_NONE     0
#define CHKSUM_MD5      FO_MD5
#define CHKSUM_SHA1     FO_SHA1

union chksumContext {
   MD5Context  md5;
   SHA1Context sha1;
};

struct CHKSUM {
   int            type;                /* One of CHKSUM_* above */
   char           name[5];             /* Big enough for NONE, MD5, SHA1, etc. */
   bool           updated;             /* True if updated by chksum_update() */
   chksumContext  context;             /* Context for the algorithm at hand */
   int            length;              /* Length of signature */
   unsigned char  signature[30];       /* Large enough for either signature */
};

int chksum_init(CHKSUM *chksum, int flags);
int chksum_update(CHKSUM *chksum, void *buf, unsigned len);
int chksum_final(CHKSUM *chksum);

#endif
