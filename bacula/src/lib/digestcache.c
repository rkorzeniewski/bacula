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

#include "digestcache.h"

/* We need a HashDatabase library support available */
#if defined(HAVE_TOKYOCABINET)

#define DCHInfoData     "DCHInfoData"
#define DCHInvalidDate  "DCHInvalidDate"

/*
 * constructor
 */
digestcache::digestcache (const char * file){ init (file); }

/*
 * constructor
 */
digestcache::digestcache (void){ init (NULL); }

/**
 * initializes a memory of the object
 *
 * in:
 *    file - a name of the digest cache file to create
 * out: void
 */
void digestcache::init (const char * file)
{
   char * dfs;

   /* initialize a mutex */
   pthread_mutex_init(&mutex, NULL );

   if (file) {
      dfile = bstrdup(file);
      dfs = (char*)malloc(PATH_MAX);
      snprintf(dfs,PATH_MAX,"%s.snap",file);
      dfilesnap = bstrdup(dfs);
      free(dfs);
      snap=0;
   } else {
      dfile = NULL;
      dfilesnap = NULL;
   }

   invalid_date = 0;
   memcache = 0;
   openrefnr = 0;
}

/*
 *
 */
digestcache::~digestcache ()
{
   closedigestfile();
   pthread_mutex_destroy(&mutex);
}

/*
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
   DC_ENOTOPEN,         // digest cache not open
};
*/
static const char* DCStatusStrings[] =
{
   "Successful",
   "Generic Error",
   "Object Exist",
   "Object Not exist",
   "Error, no digest file set",
   "Error, no digest database available",
   "Error, cannot create dstore object",
   "Error, cannot set a memcache",
   "Error, cannot open a digest database",
   "Error, invlid parameter supplied",
   "Error, Digest Cache not open",
};
const char * digestcache::strdcstatus (DCStatus status)
{
   switch (status){
   case DC_OK:
      return DCStatusStrings[0];
   case DC_ERROR:
      return DCStatusStrings[1];
   case DC_EXIST:
      return DCStatusStrings[2];
   case DC_NONEXIST:
      return DCStatusStrings[3];
   case DC_ENODFILE:
      return DCStatusStrings[4];
   case DC_ENODSTORE:
      return DCStatusStrings[5];
   case DC_ECREATEDSTORE:
      return DCStatusStrings[6];
   case DC_ESETMEMCACHE:
      return DCStatusStrings[7];
   case DC_EOPENDSTORE:
      return DCStatusStrings[8];
   case DC_EINVAL:
      return DCStatusStrings[9];
   case DC_ENOTOPEN:
      return DCStatusStrings[10];
   }
   /* invalid status supplied */
   return NULL;
}

/*
 * open perform a data check and opens a digest file if required
 */
DCStatus digestcache::open (){

   DCStatus out = DC_OK;

   lock();
   if (openrefnr > 0){
      openrefnr++;
   } else
   if ((out = opencache()) == DC_OK){
      openrefnr = 1;
   }
   unlock();

   return out;
}

/*
 * open perform a data check and opens a digest file if required
 */
DCStatus digestcache::opencache (){

   DCStatus out;

   /* check if class is initialized properly */
   if (!dfile){
      return DC_ENODFILE;
   } else {
      /* open a digest file when required */
      out = opendigestfile();
   }

   return out;
}

/*
 * general close function, perform a real close only when no more
 * threads is using it based on openrefne counter
 *
 */
DCStatus digestcache::close (){

   DCStatus out = DC_OK;

   lock();
   if (openrefnr > 1){
      openrefnr--;
   } else
   if (openrefnr == 1){
      openrefnr = 0;
      out = closecache();
   }
   unlock();

   return out;
}

/*
 * general close function, perform a real close only when no more
 * threads is using it based on openrefne counter
 *
 */
DCStatus digestcache::closecache (){

   DCStatus out;

   if (!dfile){
      return DC_ENODFILE;
   } else
   if (!dstore){
      return DC_ENODSTORE;
   } else {
      if (snap){
         closesnapshot();
      }
      out = closedigestfile ();
   }
   unlock();

   return out;
}

/**
 * perform a global lock of digest cache
 */
inline void digestcache::lock (void){ pthread_mutex_lock (&mutex); }

/**
 * perform a global unlock of digest cache
 */
inline void digestcache::unlock (void){ pthread_mutex_unlock (&mutex); }

/*
 *
 */
DCStatus digestcache::opendigestfile (void){

   int err;
   uint64_t bnum;
   uint64_t rnum;
   uint64_t fsize;

   /* we have no file to open */
   if (!dfile){
      return DC_ENODFILE;
   }

   /* create a new database object */
   dstore = tchdbnew ();
   if (!dstore){
      return DC_ECREATEDSTORE;
   }

   /* set debug output of the TC */
   const char *ebuf = getenv("TCDBGFD");
   int dbgfd = 0;
   if (ebuf){
      dbgfd = tcatoix ( ebuf );
   }
   if (dbgfd > 0){
      tchdbsetdbgfd ( dstore, dbgfd );
   }

   /* set a mmaped memory buffer which is faster */
   if (memcache > 0){
      char ed1[50];
      if (tchdbsetxmsiz (dstore, memcache)) {
         Dmsg2(120, "extra mapped memory of %s set to %sB\n", dfile, edit_uint64(memcache, ed1));
      } else {
         Dmsg2(10, "Error setting extra mapped memory of %s to %sB\n", dfile,
            edit_uint64(memcache, ed1));
         return DC_ESETMEMCACHE;
      }
   }

   /* allow multithread access */
   tchdbsetmutex (dstore);

   err = access (dfile, F_OK);
   if (err){
      /* file do not exist */
      Dmsg0 (120,"Creating New DCache index database\n");
      /* apow=6 => 64bytes record aligment */
      tchdbtune (dstore, 16777216, 6, -1, HDBTLARGE);
      err = tchdbopen (dstore, dfile, HDBOWRITER|HDBOCREAT|HDBOREADER);
      if (!err){
         /* error while opening a database file */
         err = tchdbecode (dstore);
         Dmsg1 (120,"Error opening DCache index database. Err=%s\n", tchdberrmsg(err));
         tchdbclose (dstore);
         return DC_EOPENDSTORE;
      }
      chmod (dfile, 0640);
   } else {
      /* already available digest cache file */
      err = tchdbopen ( dstore, dfile, HDBOWRITER|HDBOREADER );
      if ( !err ){
         /* error while opening a database file */
         err = tchdbecode ( dstore );
         Dmsg1(120,"Error creating DCache index database. Err=%s\n", tchdberrmsg(err));
         tchdbclose ( dstore );
         return DC_EOPENDSTORE;
      }
      bnum = tchdbbnum (dstore);
      rnum = tchdbrnum (dstore);
      fsize = tchdbfsiz (dstore);
      Dmsg3(100,"DCache index database: fsize=%lld bnum=%lld rnum=%lld\n",fsize,bnum,rnum);
      if (bnum < rnum){
         /* optimize database */
         Dmsg0 (10,"Optimizing DigestCache starting ...\n");
         tchdboptimize (dstore, 0, 6, -1, HDBTLARGE);
         bnum = tchdbbnum (dstore);
         rnum = tchdbrnum (dstore);
         fsize = tchdbfsiz (dstore);
         Dmsg3(100,"DCache index database after optimization: fsize=%lld bnum=%lld rnum=%lld\n",
            fsize,bnum,rnum);
         Dmsg0 (10,"Optimizing DigestCache finish\n");
      }
   }

   /* open ref nr is one */
   openrefnr = 1;

   return DC_OK;
}

/*
 *
 */
DCStatus digestcache::opensnapshot(void){

   int err;

   /* we have no file to open */
   if (!dfilesnap){
      return DC_ENODFILE;
   }

   /* create a new database object */
   dstoresnap = tchdbnew ();
   if (!dstoresnap){
      return DC_ECREATEDSTORE;
   }

   /* set debug output of the TC */
   const char *ebuf = getenv("TCDBGFD");
   int dbgfd = 0;
   if (ebuf){
      dbgfd = tcatoix(ebuf);
   }
   if (dbgfd > 0){
      tchdbsetdbgfd(dstoresnap, dbgfd);
   }

   /* set a mmaped memory buffer half size of main database */
   if (memcache > 0){
      char ed1[50];
      if (tchdbsetxmsiz (dstoresnap, memcache/2)) {
         Dmsg2(120, "extra mapped memory of %s set to %sB\n", dfile, edit_uint64(memcache/2, ed1));
      } else {
         Dmsg2(10, "Error setting extra mapped memory of %s to %sB\n", dfile,
            edit_uint64(memcache/2, ed1));
         return DC_ESETMEMCACHE;
      }
   }

   /* allow multithread access */
   tchdbsetmutex (dstoresnap);

   err = access (dfilesnap, F_OK);
   if (!err){
      unlink(dfilesnap);
   }
   Dmsg0 (120,"Creating New DCache snapshot database\n");
   /* apow=6 => 64bytes record aligment */
   //tchdbtune (dstoresnap, 16777216, 6, -1, HDBTLARGE);
   err = tchdbopen (dstoresnap, dfilesnap, HDBOWRITER|HDBOCREAT|HDBOREADER);
   if (!err){
      /* error while opening a database file */
      err = tchdbecode (dstoresnap);
      Dmsg1 (120,"Error opening DCache index database. Err=%s\n", tchdberrmsg(err));
      tchdbclose (dstoresnap);
      return DC_EOPENDSTORE;
   }
   chmod (dfilesnap, 0640);

   snap = 1;

   return DC_OK;
}

/*
 *
 */
DCStatus digestcache::closedigestfile(){

   if (dstore){
      tchdbdel (dstore);
      dstore = NULL;
      openrefnr = 0;
      return DC_OK;
   } else {
      return DC_EINVAL;
   }
}

/*
 * for proper function require a lock
 */
DCStatus digestcache::closesnapshot(){

   if (dstoresnap){
      // delete a snapshot database and file
      tchdbdel (dstoresnap);
      dstoresnap = NULL;
      snap = 0;
      if (dfilesnap){
         unlink(dfilesnap);
      }
   }

   return DC_OK;
}

/*
 * 
 */
DCStatus digestcache::snapshot(){

   DCStatus out = DC_OK;

   lock();
   if (openrefnr){
      if (snap > 0){
         snap++;
      } else
      if ((out = opensnapshot()) == DC_OK){
         snap = 1;
      }
   } else {
      out = DC_ENOTOPEN;
   }
   unlock();

   return out;
}

DCStatus digestcache::commit(){

   DCStatus out = DC_OK;
   char * key;
   int klen;
   int vlen;
   char * retval;

   if (dstoresnap){
      switch (snap){
      case 0:
         // no snapshot, strange, try to clean up
         closesnapshot();
         break;
      case 1:
         // move snapshot data into main cache
         if (!tchdbiterinit(dstoresnap)){
            return DC_ERROR;
         }
         // we have an iterator 
         while ((key = (char*)tchdbiternext(dstoresnap, &klen)) != NULL){
            // we have a next key in snapshot to move into main database
            retval = (char*) tchdbget (dstoresnap, key, klen, &vlen);
            if (!retval){
               return DC_ERROR;
            }
            if (!tchdbput (dstore, key, klen, retval, vlen)){
               // error
               return DC_ERROR;
            }
         }
         // delete a snapshot
         closesnapshot();
      default:
         snap--;
      }
   }

   return out;
   
}

/*
 * add a key=>value to the cache dstore
 */
DCStatus digestcache::put_key_val (const void * key, int keylen, const void * value, int valuelen){

   bool out;

   if (!dstore){
      return DC_ENODSTORE;
   }

   if (snap){
      // we have a snapshot vailable, so put a data into snapshot
      out = tchdbput (dstoresnap, key, keylen, value, valuelen);
   } else {
      out = tchdbput (dstore, key, keylen, value, valuelen);
   }

   if (!out){
      return DC_ERROR;
   }
   return DC_OK;
}

/*
 * set_info
 * in:
 *
 * Generic Value: utime_t(8) + offset(8) + char[](variable)
 *
 */
DCStatus digestcache::set_info (const char * info){

   const char * key = DCHInfoData;
   DCStatus out;

   if (!info){
      return DC_EINVAL;
   }

   lock();
   out = put_key_val (key, strlen(key), info, strlen(info));
   unlock ();

   return out;
}

/*
 *
 */
DCStatus digestcache::set_memcache ( uint64_t memsize ){

   memcache = memsize;
   return DC_OK;
}

/*
 * set_invalid_time
 * in:
 *
 * Generic Value: utime_t(8) + offset(8) + char[](variable)
 *
 */
DCStatus digestcache::set_invalid_time ( const utime_t time ){

   const char * key = DCHInvalidDate;
   DCStatus out;

   lock();
   out = put_key_val (key, strlen(key), &time, sizeof(utime_t));
   unlock ();

   return out;
}


/**
 * put - puts a new digest into a cache
 *
 *    KEY: size + digest
 *    VAL: utime_t + offset + char[]
 *
 * in:
 *    size - block size    \ this is a KEY
 *    dig - digest         /
 *    time - current time                 \
 *    file - file name parameter to save  | this is a value
 *    offset - offset in the file         /
 * out:
 *    DC_OK - on successful operation
 *    DC_ERROR - on any error
 */
DCStatus digestcache::put (uint32_t size, digest * dig, const utime_t time, const char * file,
                              const uint64_t offset){

   char * key;
   int len;
   char * val;
   int vlen = sizeof(utime_t) + sizeof(uint64_t) + 1;    /* minimal size of the value */
   utime_t * ptime;
   uint64_t * poffset;
   char * pfile;
   DCStatus out;

   len = produce_key_len (size, dig);
   key = produce_key (size, dig);
   if (!key){
      return DC_ERROR;
   }

   if (file){
      vlen += strlen(file);
   }

   val = (char*) malloc (vlen);
   memset (val, 0, vlen);

   ptime = (utime_t*)val;
   *ptime = time;                /* save a time */

   poffset = (uint64_t*)(&ptime[1]);
   *poffset = offset;            /* save offset */

   if (file){
      pfile = (char*)(&poffset[1]);
      strcpy (pfile, file);         /* save file name */
   }

   lock();
   out = put_key_val (key, len, val, vlen);
   unlock ();

   free (key);
   free (val);

   return out;
}

/**
 * put - puts a new digest into a cache
 *
 *    KEY: size + digest
 *    VAL: (utime_t) + offset + char[]
 *          where utime_t is a current time - default
 * in:
 *    size - block size
 *    dig - digest
 *    file - file name parameter to save
 *    offset - offset in the file
 * out:
 *    DC_OK - on successful operation
 *    DC_ERROR - on any error
 */
DCStatus digestcache::put (uint32_t size, digest * dig, const char * file, const uint64_t offset){

   utime_t curtime = btime_to_utime(get_current_btime());

   return put (size, dig, curtime, file, offset);
}

/*
 *
 */
DCStatus digestcache::remove_key (const char * key, int keylen){

   bool out;

   if (!dstore){
      return DC_ENODSTORE;
   }

   out = tchdbout (dstore, key, keylen);

   if (!out){
      return DC_ERROR;
   }

   return DC_OK;
}

/*
 *
 */
DCStatus digestcache::remove (uint32_t size, digest * dig){

   char * key;
   int len;
   DCStatus out;

   len = produce_key_len (size,dig);
   key = produce_key (size, dig);
   if (!key){
      return DC_ERROR;
   }

   lock();
   out = remove_key (key, len);
   unlock();
   free (key);

   return out;
}

/*
 * check if a key exist in cache
 */
DCStatus digestcache::check_key (const char * key, int keylen){

   char * retval = NULL;
   int vlen;
   DCStatus out;
   utime_t * ptime;

   if (snap){
      if (!dstoresnap){
         return DC_ENODSTORE;
      }
      retval = (char*) tchdbget (dstoresnap, key, keylen, &vlen);
   }
   if (!dstore){
         return DC_ENODSTORE;
   }
   if (!retval){
      retval = (char*) tchdbget (dstore, key, keylen, &vlen);
   }

   if (retval){
      if (invalid_date && vlen > (int)(sizeof(utime_t))){
         /* check if key is valid for invalid date */
         ptime = (utime_t*)retval;
         if (ptime[0] < invalid_date){
           /* all keys with creation time less then invalid_date are invalid */
           out = DC_NONEXIST;
         } else {
           out = DC_EXIST;
         }
      } else {
        out = DC_EXIST;
      }
      tcfree ( retval );
   } else {
      out = DC_NONEXIST;
   }

   return out;
}

/*
 * check -
 * input:
 *    digest
 *    size
 * out:
 *    DC_EXIST - when digest found in database
 *    DC_NONEXIST - when digest not in database
 *    DC_ERROR - in case of any error
 */
DCStatus digestcache::check (uint32_t size, digest * dig){

   char * key;
   int len;
   DCStatus out;

   len = produce_key_len (size, dig);
   key = produce_key (size, dig);
   if (!key){
      return DC_ERROR;
   }

   lock();
   out = check_key (key, len);
   unlock();
   free (key);

   return out;
}

/*
 *
 */
char * digestcache::get_val (const char * key, int keylen, int * vlen){

   char * retval;

   if (!dstore || !vlen || !key){
      return NULL;
   }

   if (snap){
      // we have a snapshot active, so check on snapshot data first
      retval = (char*) tchdbget (dstoresnap, key, keylen, vlen);
      if (retval){
         return retval;
      }
   }

   retval = (char*) tchdbget (dstore, key, keylen, vlen);

   return retval;
}

/*
 * get_info
 * in:
 *    **info
 * out:
 *    **info
 */
DCStatus digestcache::get_info ( char ** info ){

   int vlen;
   const char * key = DCHInfoData;
   char * val;

   if (!info){
      return DC_EINVAL;
   }

   lock();
   val = get_val ( key, strlen(key), &vlen );
   unlock();

   if (val){
      *info = (char*) malloc (vlen+1);
      memcpy (*info, val, vlen);
      (*info)[vlen] = '\0';
      tcfree(val);
   } else {
      *info = NULL;
      return DC_NONEXIST;
   }

   return DC_OK;
}

/*
 * get
 *    KEY: size + digest
 *    VAL: utime_t + offset + char[]
 */
DCStatus digestcache::get (uint32_t size, digest * dig, utime_t * time, char ** file, uint64_t * offset){

   int vlen;
   char * key;
   int len;
   char * val;
   int slen;
   utime_t * ptime;
   uint64_t * poffset;
   char * pfile;

   if (!time || !file || !offset){
      return DC_EINVAL;
   }

   len = produce_key_len (size, dig);
   key = produce_key (size, dig);
   if (!key){
      return DC_ERROR;
   }

   lock();
   val = get_val (key, len, &vlen);
   unlock();

   free (key);

   if (val){
      if (vlen > (int)(sizeof(utime_t) + sizeof(uint64_t))){
         ptime = (utime_t*)val;
         *time = ptime[0];       /* recover time */

         poffset = (uint64_t*)(&ptime[1]);
         *offset = poffset[0];   /* recover offset */

         pfile = (char*)(&poffset[1]);
         slen = vlen - (sizeof(utime_t) + sizeof(uint64_t));
         *file = (char*) malloc (slen);
         memset (*file, 0, slen);
         memcpy (*file, (void*)(&pfile[0]), slen);
      } else {
         return DC_ERROR;
      }
      tcfree(val);
   } else {
      return DC_NONEXIST;
   }

   return DC_OK;
}

/*
 * get
 *    KEY: size + digest
 *    VAL: utime_t + offset + char[]
 */
DCStatus digestcache::get (uint32_t size, digest * dig, char ** file, uint64_t * offset){

   utime_t time;

   return get (size, dig, &time, file, offset);
}

/*
 *
 */
DCStatus digestcache::clearall (void){

   bool out;

   lock();
   out = tchdbvanish(dstore);
   unlock();

   if (!out){
      return DC_ERROR;
   }

   return DC_OK;
}

#ifdef TEST_PROGRAM
using namespace std;
const char * D = "DE2F256064A0AF797747C2B97505DC0B9F3DF0DE4F489EAC731C23AE9CA9CC31";
const char * F = "46ecb1f919b3ec87a8cd7a83e79ff509f723ecb167161b498b2c2df678cec914";

int main (void){

   digestcache * cache;
   digest dig(D);
   DCStatus out;
//   struct rusage rus;

   cout << "create cache on /tmp/digestcache.tdbm" << endl;
   cache = New (digestcache ("/tmp/digestcache.tdbm"));
   if (!cache){
      cout << "error creating a cache!" << endl;
      exit(1);
   }

   cache->set_memcache (1024*1024*1024);

   cout << endl << "OPEN/CLOSE:" << endl;
   if ((out = cache->open()) == DC_OK){
      cout << "open for the first time: " << cache->strdcstatus(out) << endl;
   } else {
      cout << "open unsuccessful: " << cache->strdcstatus(out) << endl;
   }
   if ((out = cache->open()) == DC_OK){
      cout << "open for the second time: " << cache->strdcstatus(out) << endl;
   } else {
      cout << "second open unsuccessful: " << cache->strdcstatus(out) << endl;
   }
   if ((out = cache->close()) == DC_OK){
      cout << "close: " << cache->strdcstatus(out) << endl;
   } else {
      cout << "close unsuccessful: " << cache->strdcstatus(out) << endl;
   }

   cout << endl << "SNAPSHOT:" << endl;
   if ((out = cache->snapshot()) == DC_OK){
      cout << "snapshot for the first time: " << cache->strdcstatus(out) << endl;
   } else {
      cout << "snapshot unsuccessful: " << cache->strdcstatus(out) << endl;
   }

   cout << endl << "ADD:" << endl;
   cout << "add to cache ( 1024," << dig << "): ";
   if ((out = cache->put (1024, &dig, "", 0)) == DC_OK){
      cout << "success" << endl;
   } else {
      cout << "error" << endl;
   }
   cout << "add to cache (65536," << dig << "): ";
   if ((out = cache->put (65536, &dig, "", 0)) == DC_OK){
      cout << "success" << endl;
   } else {
      cout << "error" << endl;
   }
   dig = F;
   cout << "add to cache ( 2048," << dig << "): ";
   if ((out = cache->put (2048, &dig, "", 0)) == DC_OK){
      cout << "success" << endl;
   } else {
      cout << "error" << endl;
   }

   cout << endl << "SEARCH:" << endl;
   cout << "search in cache ( 2048," << dig << "): ";
   if ((out = cache->check (2048, &dig)) == DC_EXIST){
      cout << "found - good" << endl;
   } else {
      cout << "not found - not good" << endl;
   }
   cout << "search in cache (65536," << dig << "): ";
   if ((out = cache->check (65536, &dig)) == DC_EXIST){
      cout << "found - not good" << endl;
   } else {
      cout << "not found - good" << endl;
   }

/*
   if (getrusage(RUSAGE_SELF, &rus) < 0){
      cout << "resource usage checking error" << endl;
   }
   cout << "memory: " << rus.ru_maxrss << "kB" << endl;
*/

   cout << endl << "COMMIT:" << endl;
   if ((out = cache->commit()) == DC_OK){
      cout << "commit for the first time: " << cache->strdcstatus(out) << endl;
   } else {
      cout << "commit unsuccessful: " << cache->strdcstatus(out) << endl;
   }

   cout << endl << "Check CLOSE:" << endl;
   if ((out = cache->close()) == DC_OK){
      cout << "close: " << cache->strdcstatus(out) << endl;
   } else {
      cout << "open unsuccessful: " << cache->strdcstatus(out) << endl;
   }
   if ((out = cache->close()) != DC_OK){
      cout << "check close success (" << cache->strdcstatus(out) << ")" << endl;
   } else {
      cout << "next check close unsuccessful: " << cache->strdcstatus(out) << endl;
   }

   return 0;
}

#endif /* TEST_PROGRAM */
#endif /* HAVE_TOKYOCABINET */
