/*
 * General routines for handling the various checksum supported.
 *
 *  Written by Preben 'Peppe' Guldberg, December MMIV
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

#include "bacula.h"
#include "filed.h"

/* return 0 on success, otherwise some handler specific error code. */
int chksum_init(CHKSUM *chksum, int flags)
{
   int status = 0;

   chksum->type = CHKSUM_NONE;
   bstrncpy(chksum->name, "NONE", sizeof(chksum->name));
   chksum->updated = false;
   if (flags & CHKSUM_MD5) {
      chksum->length = 16;
      MD5Init(&chksum->context.md5);
      chksum->type = CHKSUM_MD5;
      bstrncpy(chksum->name, "MD5", sizeof(chksum->name));
   } else if (flags * CHKSUM_SHA1) {
      chksum->length = 20;
      status = SHA1Init(&chksum->context.sha1);
      if (status == 0) {
	 chksum->type = CHKSUM_SHA1;
         bstrncpy(chksum->name, "SHA1", sizeof(chksum->name));
      }
   }
   return status;
}

/* return 0 on success, otherwise some handler specific error code. */
int chksum_update(CHKSUM *chksum, void *buf, unsigned len)
{
   int status;
   switch (chksum->type) {
   case CHKSUM_NONE:
      return 0;
   case CHKSUM_MD5:
      MD5Update(&chksum->context.md5, (unsigned char *)buf, len);
      chksum->updated = true;
      return 0;
   case CHKSUM_SHA1:
      status = SHA1Update(&chksum->context.sha1, (uint8_t *)buf, len);
      if (status == 0) {
	 chksum->updated = true;
      }
      return status;
   default:
      return -1;
   }
}

/* return 0 on success, otherwise some handler specific error code. */
int chksum_final(CHKSUM *chksum)
{
   switch (chksum->type) {
   case CHKSUM_NONE:
      return 0;
   case CHKSUM_MD5:
      MD5Final(chksum->signature, &chksum->context.md5);
      return 0;
   case CHKSUM_SHA1:
      return SHA1Final(&chksum->context.sha1, chksum->signature);
   default:
      return -1;
   }
}
