/*
 * Bacula MD5 definitions
 *
 *  Kern Sibbald, 2001
 *
 *   Version $Id$
 */
/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2001-2014 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from many
   others, a complete list can be found in the file AUTHORS.

   You may use this file and others of this release according to the
   license defined in the LICENSE file, which includes the Affero General
   Public License, v3.0 ("AGPLv3") and some additional permissions and
   terms pursuant to its AGPLv3 Section 7.

   Bacula® is a registered trademark of Kern Sibbald.
*/

#ifndef __BMD5_H
#define __BMD5_H

#define MD5HashSize 16

struct MD5Context {
   uint32_t buf[4];
   uint32_t bits[2];
   uint8_t  in[64];
};

typedef struct MD5Context MD5Context;

extern void MD5Init(struct MD5Context *ctx);
extern void MD5Update(struct MD5Context *ctx, unsigned char *buf, unsigned len);
extern void MD5Final(unsigned char digest[16], struct MD5Context *ctx);
extern void MD5Transform(uint32_t buf[4], uint32_t in[16]);

#endif /* !__BMD5_H */
