/*
 * openssl.h OpenSSL support functions
 *
 * Author: Landon Fuller <landonf@opendarwin.org>
 *
 * Version $Id$
 *
 * Copyright (C) 2005 Kern Sibbald
 *
 * This file was contributed to the Bacula project by Landon Fuller.
 *
 * Landon Fuller has been granted a perpetual, worldwide, non-exclusive,
 * no-charge, royalty-free, irrevocable copyright * license to reproduce,
 * prepare derivative works of, publicly display, publicly perform,
 * sublicense, and distribute the original work contributed by Landon Fuller
 * to the Bacula project in source or object form.
 *
 * If you wish to license these contributions under an alternate open source
 * license please contact Landon Fuller <landonf@opendarwin.org>.
 */
/*
   Copyright (C) 2005-2006 Kern Sibbald

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   version 2 as amended with additional clauses defined in the
   file LICENSE in the main source directory.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the 
   the file LICENSE for additional details.

 */

#ifndef __OPENSSL_H_
#define __OPENSSL_H_

#ifdef HAVE_OPENSSL
void             openssl_post_errors     (int code, const char *errstring);
int              openssl_init_threads    (void);
void             openssl_cleanup_threads (void);
int              openssl_seed_prng       (void);
int              openssl_save_prng       (void);
#endif /* HAVE_OPENSSL */

#endif /* __OPENSSL_H_ */
