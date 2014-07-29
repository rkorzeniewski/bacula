/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2005-2014 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from many
   others, a complete list can be found in the file AUTHORS.

   You may use this file and others of this release according to the
   license defined in the LICENSE file, which includes the Affero General
   Public License, v3.0 ("AGPLv3") and some additional permissions and
   terms pursuant to its AGPLv3 Section 7.

   Bacula® is a registered trademark of Kern Sibbald.
*/
/*
 * openssl.h OpenSSL support functions
 *
 * Author: Landon Fuller <landonf@opendarwin.org>
 *
 * Version $Id$
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

#ifndef __OPENSSL_H_
#define __OPENSSL_H_

#ifdef HAVE_OPENSSL
void             openssl_post_errors     (int code, const char *errstring);
void             openssl_post_errors     (JCR *jcr, int code, const char *errstring);
int              openssl_init_threads    (void);
void             openssl_cleanup_threads (void);
int              openssl_seed_prng       (void);
int              openssl_save_prng       (void);
#endif /* HAVE_OPENSSL */

#endif /* __OPENSSL_H_ */
