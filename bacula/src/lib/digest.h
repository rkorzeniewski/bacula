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
 * DIGEST is a class which implements SHA Digest handling in area of
 * storage, computation and digest transformation for both binary
 * and text forms.
 *
 * The main author of Bacula Deduplication code is Radosław Korzeniewski
 * 
 *  (c) Radosław Korzeniewski, MMXIV
 *  radoslaw@korzeniewski.net
 *
 */

#ifndef _DIGEST_H
#define _DIGEST_H 1

#include <iostream>
#include "bacula.h"

#if defined(HAVE_OPENSSL)
#include <openssl/sha.h>

enum DigestType {
   DIGEST_256,
   DIGEST_384,
   DIGEST_512,
};

#define DIGEST_256_LEN   SHA512_DIGEST_LENGTH
#define DIGEST_384_LEN   SHA512_DIGEST_LENGTH
#define DIGEST_512_LEN   SHA512_DIGEST_LENGTH
#define DIGEST_MAX_LEN   DIGEST_512_LEN

class digest : public SMARTALLOC {
   DigestType type;
   unsigned char digest_bin [DIGEST_MAX_LEN];
   char digest_text [DIGEST_MAX_LEN * 2 + 1];
   void init(DigestType t);

public:
   digest ();
   digest (DigestType t);
   digest (const char * digt);
   digest (DigestType t, const char * digt);
   ~digest () {};
   void render_text ();
   void compute (const unsigned char * buf, unsigned long len);
   inline unsigned char * get_digest () { return digest_bin; };
   inline char * get_digest_text () { return digest_text; };
   char * get_render_text ();
   bool operator== (const digest &dig) const;
   bool operator!= (const digest &dig) const;
   digest& operator= (const digest &dig);
   digest& operator= (const char *digt);
   inline operator char*() { return digest_text; };
   inline int len() const {
         return   (type == DIGEST_256) ? DIGEST_256_LEN :
                  (type == DIGEST_384) ? DIGEST_384_LEN :
                  (type == DIGEST_512) ? DIGEST_512_LEN : DIGEST_MAX_LEN; }
};

std::ostream & operator<< (std::ostream & out, digest & dig);

#endif /* HAVE_OPENSSL */
#endif /* _DIGEST_H */
