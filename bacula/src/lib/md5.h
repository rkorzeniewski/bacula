/*
 * Bacula MD5 definitions
 *
 *  Kern Sibbald, 2001
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
