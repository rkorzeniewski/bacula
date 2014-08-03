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
 * Digest Cache a fast key->value storage public API
 *
 * The main author of Bacula Deduplication code is Radoslaw Korzeniewski
 * 
 *  (c) Radosław Korzeniewski, MMXIV
 *  radoslaw@korzeniewski.net
 * 
 */

#ifndef _DIGESTCACHE_H
#define _DIGESTCACHE_H 1

#include <iostream>
#include "lib/digest.h"
#include "bacula.h"

/* We need a HashDatabase library support available */
#if defined(HAVE_TOKYOCABINET)
#if defined(HAVE_TCUTIL_H)
#include <tcutil.h>
#endif /* HAVE_TCUTIL_H */
#if defined(HAVE_TCHDB_H)
#include <tchdb.h>
#endif /* HAVE_TCHDB_H */

/**
 * Status enum used for return codes
 */
enum _DCStatus {
   DC_OK = 0,
   DC_ERROR,            // generic error
   DC_EXIST,
   DC_NONEXIST,
   DC_ENODFILE,         // no digest file set
   DC_ENODSTORE,        // no digest database available
   DC_ECREATEDSTORE,    // cannot create dstore object
   DC_ESETMEMCACHE,     // cannot set a memcache
   DC_EOPENDSTORE,      // cannot open a digest database
   DC_EINVAL,           // invlid parameter supplied
};
typedef enum _DCStatus DCStatus;


/*
 * Digest Cache
 */
class digestcache : public SMARTALLOC {
   TCHDB * dstore;
   pthread_mutex_t mutex;
   char * dfile;
   utime_t invalid_date;
   uint64_t memcache;
   int openrefnr;

   void init (const char * file);
   DCStatus closedigestfile ();
   DCStatus opendigestfile ();
   DCStatus put_key_val (const void * key, int keylen, const void * value, int valuelen);
   DCStatus remove_key (const char * key, int keylen);
   DCStatus check_key (const char * key, int keylen);
   char * get_val (const char * key, int keylen, int * vlen);
   /*
    * public interface functions perform a locking
    */
   inline void lock ();
   inline void unlock ();

public:
   digestcache ();
   digestcache (const char * file);
   ~digestcache ();

   /* setup methods */
   DCStatus set_memcache (uint64_t memsize);
   /* generic methods */
   DCStatus open();
   DCStatus close();
   DCStatus clearall ();
   const char * strdcstatus (DCStatus status);
   /* information handling */
   DCStatus set_info (const char * info);
   DCStatus get_info (char ** info);
   /* validation handling */
   DCStatus set_invalid_time (const utime_t time);
   DCStatus get_invalid_time (const utime_t time);
   /* KEY: size(4B) + digest(variable); VAL: utime_t(8B) + offset(8B) + char[](variable) */
   /* standard methods */
   DCStatus remove (uint32_t size, digest * dig);
   DCStatus check (uint32_t size, digest * dig);
   /* put data */
   DCStatus put (uint32_t size, digest * dig, const char * file, const uint64_t offset);
   DCStatus put (uint32_t size, digest * dig, const utime_t time, const char * file, const uint64_t offset);
   /* get data */
   DCStatus get (uint32_t size, digest * dig, char ** file, uint64_t * offset);
   DCStatus get (uint32_t size, digest * dig, utime_t * time, char ** file, uint64_t * offset);
};

#endif /* HAVE_TOKYOCABINET */
#endif /* _DIGESTCACHE_H */
