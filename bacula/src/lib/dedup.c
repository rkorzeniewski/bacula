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
 * Digest Cache is a fast key->value storage used for information caching
 *
 * The main author of Bacula Deduplication code is Radoslaw Korzeniewski
 * 
 *  (c) Radosław Korzeniewski, MMXIV
 *  radoslaw@korzeniewski.net
 * 
 */

#include "digest.h"
#include "bacula.h"

#if defined (DEDUP_ENABLE)

/**
 * it produces a length of the key based on key source data
 * 
 * in:
 *    size - block size
 *    dig - digest
 * out:
 *    computed length of the produced key
 */
int produce_key_len (uint32_t size, digest * dig){

   int len = sizeof(size);

   if (dig){
      len += dig->len();
   }
   return len;
}

/**
 * it produces a key by connecting a 32bit size with digest 
 * function is alocating a memory which should be freed when not needed
 */
char * produce_key (uint32_t size, digest * dig){

   uint32_t * buf;
   int len = produce_key_len (size,dig);

   buf = (uint32_t*) malloc (len);
   memset (buf, 0, len);
   buf[0] = size;
   if (dig){
      memcpy (&buf[1], dig->get_digest(), dig->len());
   }

   return (char*)buf;
}

/**
 * it produces a length of the key based on key source data
 * 
 * in:
 *    size - block size
 *    dig - digest
 * out:
 *    computed length of the produced key
 */
int produce_keyser_len (uint32_t size, digest * dig) { return produce_key_len (size,dig); }

/**
 * it produces a key by connecting a 32bit size with digest 
 * function is alocating a memory which should be freed when not needed
 */
char * produce_keyser (uint32_t size, digest * dig) {

   uint32_t * buf;
   int len = produce_keyser_len (size,dig);

   buf = (uint32_t*) malloc (len);
   memset (buf, 0, len);

   /* serialize */
   ser_declare;
   ser_begin (buf, len);
   ser_uint32 (size);
   if (dig){
      ser_bytes (dig->get_digest(), dig->len());
   }
   ser_end (buf, len);

   return (char*)buf;
}

#endif /* DEDUP_ENABLE */
