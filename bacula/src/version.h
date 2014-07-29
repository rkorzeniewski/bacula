
#undef  VERSION
#define VERSION "7.0.5"
#define BDATE   "26 July 2014"
#define LSMDATE "26Jul14"

#define PROG_COPYRIGHT "Copyright (C) %d-2014 Free Software Foundation Europe e.V.\n"
#define BYEAR "2014"       /* year for copyright messages in progs */

/*
 * Versions of packages needed to build Bacula components
 */
#define DEPKGS_QT_VERSION  "01Jan13"
#define DEPKGS_VERSION     "29Feb12"
#define BQT4_VERSION       "4.8.4"


/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2000-2014 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from many
   others, a complete list can be found in the file AUTHORS.

   You may use this file and others of this release according to the
   license defined in the LICENSE file, which includes the Affero General
   Public License, v3.0 ("AGPLv3") and some additional permissions and
   terms pursuant to its AGPLv3 Section 7.

   Bacula® is a registered trademark of Kern Sibbald.
*/

/* Shared object library versions */

/* Uncomment to overwrite default value from VERSION */
/* #define LIBBAC_LT_RELEASE     "5.1.0" */
/* #define LIBBACCFG_LT_RELEASE  "5.1.0" */
/* #define LIBBACPY_LT_RELEASE   "5.1.0" */
/* #define LIBBACSQL_LT_RELEASE  "5.1.0" */
/* #define LIBBACCATS_LT_RELEASE  "5.1.0" */
/* #define LIBBACFIND_LT_RELEASE "5.1.0" */


/* Debug flags */
#undef  DEBUG
#define DEBUG 1
#define TRACEBACK 1
#define TRACE_FILE 1

/* If this is set stdout will not be closed on startup */
/* #define DEVELOPER 1 */

/* adjust DEVELOPER_MODE for status command */
#ifdef DEVELOPER
# define DEVELOPER_MODE 1
#else
# define DEVELOPER_MODE 0
#endif

/*
 * SMCHECK does orphaned buffer checking (memory leaks)
 *  it can always be turned on, but has some minor performance
 *  penalties.
 */
#ifdef DEVELOPER
# define SMCHECK
#endif

/*
 * _USE_LOCKMGR does lock/unlock mutex tracking (dead lock)
 *   it can always be turned on, but we advise to use it only
 *   for debug
 */
# ifndef _USE_LOCKMGR
#  define _USE_LOCKMGR
# endif /* _USE_LOCKMGR */
/*
 * Enable priority management with the lock manager
 *
 * Note, turning this on will cause the Bacula SD to abort if
 *  mutexes are executed out of order, which could lead to a
 *  deadlock.  However, note that this is not necessarily a
 *  deadlock, so turn this on only for debugging.
 */
#define USE_LOCKMGR_PRIORITY

/*
 * Enable thread verification before kill
 *
 * Note, this extra check have a high cost when using
 * dozens of thread, so turn this only for debugging.
 */
/* #define USE_LOCKMGR_SAFEKILL */

#if !HAVE_LINUX_OS && !HAVE_SUN_OS && !HAVE_DARWIN_OS && !HAVE_FREEBSD_OS
# undef _USE_LOCKMGR
#endif

/*
 * USE_VTAPE is a dummy tape driver. This is useful to
 *  run regress test.
 */
#ifdef HAVE_LINUX_OS
# define USE_VTAPE
#endif

/*
 * USE_FTP is a ftp driver for the FD using curl.
 */
// #define USE_FTP

/*
 * for fastest speed but you must have a UPS to avoid unwanted shutdowns
 */
//#define SQLITE3_INIT_QUERY "PRAGMA synchronous = OFF"

/*
 * for more safety, but is 30 times slower than above
 */
#define SQLITE3_INIT_QUERY "PRAGMA synchronous = NORMAL"

/*
 * This should always be on. It enables data encryption code
 *  providing it is configured.
 */
#define DATA_ENCRYPTION 1

/*
 * This uses a Bacula specific bsnprintf rather than the sys lib
 *  version because it is much more secure. It should always be
 *  on.
 */
#define USE_BSNPRINTF 1

/* Debug flags not normally turned on */

/* #define TRACE_JCR_CHAIN 1 */
/* #define TRACE_RES 1 */
/* #define DEBUG_MEMSET 1 */
/* #define DEBUG_MUTEX 1 */
/* #define DEBUG_BLOCK_CHECKSUM 1 */
#define BEEF 0

/*
 * Set SMALLOC_SANITY_CHECK to zero to turn off, otherwise
 *  it is the maximum memory malloced before Bacula will
 *  abort.  Except for debug situations, this should be zero
 */
#define SMALLOC_SANITY_CHECK 0  /* 500000000  0.5 GB max */


/* Check if header of tape block is zero before writing */
/* #define DEBUG_BLOCK_ZEROING 1 */

/* #define FULL_DEBUG 1 */   /* normally on for testing only */

/* Turn this on ONLY if you want all Dmsg() to append to the
 *   trace file. Implemented mainly for Win32 ...
 */
/*  #define SEND_DMSG_TO_FILE 1 */


/* The following are turned on for performance testing */
/*
 * If you turn on the NO_ATTRIBUTES_TEST and rebuild, the SD
 *  will receive the attributes from the FD, will write them
 *  to disk, then when the data is written to tape, it will
 *  read back the attributes, but they will not be sent to
 *  the Director. So this will eliminate: 1. the comm time
 *  to send the attributes to the Director. 2. the time it
 *  takes the Director to put them in the catalog database.
 */
/* #define NO_ATTRIBUTES_TEST 1 */

/*
* If you turn on NO_TAPE_WRITE_TEST and rebuild, the SD
*  will do all normal actions, but will not write to the
*  Volume.  Note, this means a lot of functions such as
*  labeling will not work, so you must use it only when
*  Bacula is going to append to a Volume. This will eliminate
*  the time it takes to write to the Volume (not the time
*  it takes to do any positioning).
*/
/* #define NO_TAPE_WRITE_TEST 1 */

/*
 * If you turn on FD_NO_SEND_TEST and rebuild, the FD will
 *  not send any attributes or data to the SD. This will
 *  eliminate the comm time sending to the SD.
 */
/* #define FD_NO_SEND_TEST 1 */
