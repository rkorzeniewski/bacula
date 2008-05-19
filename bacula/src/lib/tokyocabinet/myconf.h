/*************************************************************************************************
 * System-dependent configurations of Tokyo Cabinet
 *                                                      Copyright (C) 2006-2008 Mikio Hirabayashi
 * This file is part of Tokyo Cabinet.
 * Tokyo Cabinet is free software; you can redistribute it and/or modify it under the terms of
 * the GNU Lesser General Public License as published by the Free Software Foundation; either
 * version 2.1 of the License or any later version.  Tokyo Cabinet is distributed in the hope
 * that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 * You should have received a copy of the GNU Lesser General Public License along with Tokyo
 * Cabinet; if not, write to the Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307 USA.
 *************************************************************************************************/


#ifndef _MYCONF_H                        // duplication check
#define _MYCONF_H



/*************************************************************************************************
 * system discrimination
 *************************************************************************************************/


#if defined(__linux__)

#define _SYS_LINUX_
#define ESTSYSNAME  "Linux"

#elif defined(__FreeBSD__)

#define _SYS_FREEBSD_
#define ESTSYSNAME  "FreeBSD"

#elif defined(__NetBSD__)

#define _SYS_NETBSD_
#define ESTSYSNAME  "NetBSD"

#elif defined(__OpenBSD__)

#define _SYS_OPENBSD_
#define ESTSYSNAME  "OpenBSD"

#elif defined(__sun__)

#define _SYS_SUNOS_
#define ESTSYSNAME  "SunOS"

#elif defined(__hpux)

#define _SYS_HPUX_
#define ESTSYSNAME  "HP-UX"

#elif defined(__osf)

#define _SYS_TRU64_
#define ESTSYSNAME  "Tru64"

#elif defined(_AIX)

#define _SYS_AIX_
#define ESTSYSNAME  "AIX"

#elif defined(__APPLE__) && defined(__MACH__)

#define _SYS_MACOSX_
#define ESTSYSNAME  "Mac OS X"

#elif defined(_MSC_VER)

#define _SYS_MSVC_
#define ESTSYSNAME  "Windows (VC++)"

#elif defined(_WIN32)

#define _SYS_MINGW_
#define ESTSYSNAME  "Windows (MinGW)"

#elif defined(__CYGWIN__)

#define _SYS_CYGWIN_
#define ESTSYSNAME  "Windows (Cygwin)"

#else

#define _SYS_GENERIC_
#define ESTSYSNAME  "Generic"

#endif



/*************************************************************************************************
 * common settings
 *************************************************************************************************/


#if defined(NDEBUG)
#define TCDODEBUG(TC_expr) \
  do { \
  } while(false)
#else
#define TCDODEBUG(TC_expr) \
  do { \
    TC_expr; \
  } while(false)
#endif

#define TCSWAB16(TC_num) \
  ( \
   ((TC_num & 0x00ffU) << 8) | \
   ((TC_num & 0xff00U) >> 8) \
  )

#define TCSWAB32(TC_num) \
  ( \
   ((TC_num & 0x000000ffUL) << 24) | \
   ((TC_num & 0x0000ff00UL) << 8) | \
   ((TC_num & 0x00ff0000UL) >> 8) | \
   ((TC_num & 0xff000000UL) >> 24) \
  )

#define TCSWAB64(TC_num) \
  ( \
   ((TC_num & 0x00000000000000ffULL) << 56) | \
   ((TC_num & 0x000000000000ff00ULL) << 40) | \
   ((TC_num & 0x0000000000ff0000ULL) << 24) | \
   ((TC_num & 0x00000000ff000000ULL) << 8) | \
   ((TC_num & 0x000000ff00000000ULL) >> 8) | \
   ((TC_num & 0x0000ff0000000000ULL) >> 24) | \
   ((TC_num & 0x00ff000000000000ULL) >> 40) | \
   ((TC_num & 0xff00000000000000ULL) >> 56) \
  )

#if defined(_MYBIGEND) || defined(_MYSWAB)
#define TCBIGEND       1
#define TCHTOIS(TC_num)   TCSWAB16(TC_num)
#define TCHTOIL(TC_num)   TCSWAB32(TC_num)
#define TCHTOILL(TC_num)  TCSWAB64(TC_num)
#define TCITOHS(TC_num)   TCSWAB16(TC_num)
#define TCITOHL(TC_num)   TCSWAB32(TC_num)
#define TCITOHLL(TC_num)  TCSWAB64(TC_num)
#else
#define TCBIGEND       0
#define TCHTOIS(TC_num)   (TC_num)
#define TCHTOIL(TC_num)   (TC_num)
#define TCHTOILL(TC_num)  (TC_num)
#define TCITOHS(TC_num)   (TC_num)
#define TCITOHL(TC_num)   (TC_num)
#define TCITOHLL(TC_num)  (TC_num)
#endif

#if defined(_MYNOZLIB)
#define TCUSEZLIB      0
#else
#define TCUSEZLIB      1
#endif

#if defined(_MYNOPTHREAD)
#define TCUSEPTHREAD   0
#else
#define TCUSEPTHREAD   1
#endif

#if defined(_MYMICROYIELD)
#define TCMICROYIELD   1
#else
#define TCMICROYIELD   0
#endif

#define sizeof(a)      ((int)sizeof(a))

int _tc_dummyfunc(void);
int _tc_dummyfuncv(int a, ...);



/*************************************************************************************************
 * general headers
 *************************************************************************************************/


#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <limits.h>
#include <locale.h>
#include <math.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <time.h>

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/times.h>
#include <fcntl.h>
#include <dirent.h>
#include <regex.h>
#include <glob.h>

#if TCUSEPTHREAD
#include <pthread.h>
#endif



/*************************************************************************************************
 * notation of filesystems
 *************************************************************************************************/


#define MYPATHCHR       '/'
#define MYPATHSTR       "/"
#define MYEXTCHR        '.'
#define MYEXTSTR        "."
#define MYCDIRSTR       "."
#define MYPDIRSTR       ".."



/*************************************************************************************************
 * for ZLIB
 *************************************************************************************************/


enum {
  _TCZMZLIB,
  _TCZMRAW,
  _TCZMGZIP
};


extern char *(*_tc_deflate)(const char *, int, int *, int);

extern char *(*_tc_inflate)(const char *, int, int *, int);

extern unsigned int (*_tc_getcrc)(const char *, int);



/*************************************************************************************************
 * for POSIX thread disability
 *************************************************************************************************/


#if ! TCUSEPTHREAD

#define pthread_once_t                   int
#undef PTHREAD_ONCE_INIT
#define PTHREAD_ONCE_INIT                0
#define pthread_once(TC_a, TC_b)         _tc_dummyfuncv((int)(TC_a), (TC_b))

#define pthread_mutexattr_t              int
#undef PTHREAD_MUTEX_RECURSIVE
#define PTHREAD_MUTEX_RECURSIVE          0
#define pthread_mutexattr_init(TC_a)     _tc_dummyfuncv((int)(TC_a))
#define pthread_mutexattr_destroy(TC_a)  _tc_dummyfuncv((int)(TC_a))
#define pthread_mutexattr_settype(TC_a, TC_b)  _tc_dummyfuncv((int)(TC_a), (TC_b))

#define pthread_mutex_t                  int
#undef PTHREAD_MUTEX_INITIALIZER
#define PTHREAD_MUTEX_INITIALIZER        0
#define pthread_mutex_init(TC_a, TC_b)   _tc_dummyfuncv((int)(TC_a), (TC_b))
#define pthread_mutex_destroy(TC_a)      _tc_dummyfuncv((int)(TC_a))
#define pthread_mutex_lock(TC_a)         _tc_dummyfuncv((int)(TC_a))
#define pthread_mutex_unlock(TC_a)       _tc_dummyfuncv((int)(TC_a))

#define pthread_rwlock_t                 int
#undef PTHREAD_RWLOCK_INITIALIZER
#define PTHREAD_RWLOCK_INITIALIZER       0
#define pthread_rwlock_init(TC_a, TC_b)  _tc_dummyfuncv((int)(TC_a), (TC_b))
#define pthread_rwlock_destroy(TC_a)     _tc_dummyfuncv((int)(TC_a))
#define pthread_rwlock_rdlock(TC_a)      _tc_dummyfuncv((int)(TC_a))
#define pthread_rwlock_wrlock(TC_a)      _tc_dummyfuncv((int)(TC_a))
#define pthread_rwlock_unlock(TC_a)      _tc_dummyfuncv((int)(TC_a))

#define pthread_key_create(TC_a, TC_b)   _tc_dummyfuncv((int)(TC_a), (TC_b))
#define pthread_key_delete(TC_a)         _tc_dummyfuncv((int)(TC_a))
#define pthread_setspecific(TC_a, TC_b)  _tc_dummyfuncv((int)(TC_a))
#define pthread_getspecific(TC_a)        _tc_dummyfuncv((int)(TC_a))

#define pthread_create(TC_th, TC_attr, TC_func, TC_arg) \
  (*(TC_th) = 0, (TC_func)(TC_arg), 0)
#define pthread_join(TC_th, TC_rv) \
  (*(TC_rv) = NULL, 0)
#define pthread_detach(TC_th)         0

#endif

#if TCUSEPTHREAD && TCMICROYIELD
#define TCTESTYIELD() \
  do { \
    static uint32_t cnt = 0; \
    if(((++cnt) & (0x20 - 1)) == 0){ \
      pthread_yield(); \
      if(cnt > 0x1000) cnt = (uint32_t)time(NULL) % 0x1000; \
    } \
  } while(false)
#undef assert
#define assert(TC_expr) \
  do { \
    if(!(TC_expr)){ \
      fprintf(stderr, "assertion failed: %s\n", #TC_expr); \
      abort(); \
    } \
    TCTESTYIELD(); \
  } while(false)
#else
#define TCTESTYIELD() \
  do { \
  } while(false);
#endif



/*************************************************************************************************
 * utilities for implementation
 *************************************************************************************************/


#define TCNUMBUFSIZ    32                // size of a buffer for a number

/* set a buffer for a variable length number */
#define TCSETVNUMBUF(TC_len, TC_buf, TC_num) \
  do { \
    int _TC_num = (TC_num); \
    if(_TC_num == 0){ \
      ((signed char *)(TC_buf))[0] = 0; \
      (TC_len) = 1; \
    } else { \
      (TC_len) = 0; \
      while(_TC_num > 0){ \
        int _TC_rem = _TC_num & 0x7f; \
        _TC_num >>= 7; \
        if(_TC_num > 0){ \
          ((signed char *)(TC_buf))[(TC_len)] = -_TC_rem - 1; \
        } else { \
          ((signed char *)(TC_buf))[(TC_len)] = _TC_rem; \
        } \
        (TC_len)++; \
      } \
    } \
  } while(false)

/* set a buffer for a variable length number of 64-bit */
#define TCSETVNUMBUF64(TC_len, TC_buf, TC_num) \
  do { \
    long long int _TC_num = (TC_num); \
    if(_TC_num == 0){ \
      ((signed char *)(TC_buf))[0] = 0; \
      (TC_len) = 1; \
    } else { \
      (TC_len) = 0; \
      while(_TC_num > 0){ \
        int _TC_rem = _TC_num & 0x7f; \
        _TC_num >>= 7; \
        if(_TC_num > 0){ \
          ((signed char *)(TC_buf))[(TC_len)] = -_TC_rem - 1; \
        } else { \
          ((signed char *)(TC_buf))[(TC_len)] = _TC_rem; \
        } \
        (TC_len)++; \
      } \
    } \
  } while(false)

/* read a variable length buffer */
#define TCREADVNUMBUF(TC_buf, TC_num, TC_step) \
  do { \
    TC_num = 0; \
    int _TC_base = 1; \
    int _TC_i = 0; \
    while(true){ \
      if(((signed char *)(TC_buf))[_TC_i] >= 0){ \
        TC_num += ((signed char *)(TC_buf))[_TC_i] * _TC_base; \
        break; \
      } \
      TC_num += _TC_base * (((signed char *)(TC_buf))[_TC_i] + 1) * -1; \
      _TC_base <<= 7; \
      _TC_i++; \
    } \
    (TC_step) = _TC_i + 1; \
  } while(false)

/* read a variable length buffer */
#define TCREADVNUMBUF64(TC_buf, TC_num, TC_step) \
  do { \
    TC_num = 0; \
    long long int _TC_base = 1; \
    int _TC_i = 0; \
    while(true){ \
      if(((signed char *)(TC_buf))[_TC_i] >= 0){ \
        TC_num += ((signed char *)(TC_buf))[_TC_i] * _TC_base; \
        break; \
      } \
      TC_num += _TC_base * (((signed char *)(TC_buf))[_TC_i] + 1) * -1; \
      _TC_base <<= 7; \
      _TC_i++; \
    } \
    (TC_step) = _TC_i + 1; \
  } while(false)

/* Compare keys of two records by lexical order. */
#define TCCMPLEXICAL(TC_rv, TC_aptr, TC_asiz, TC_bptr, TC_bsiz) \
  do { \
    (TC_rv) = 0; \
    int _TC_min = (TC_asiz) < (TC_bsiz) ? (TC_asiz) : (TC_bsiz); \
    for(int _TC_i = 0; _TC_i < _TC_min; _TC_i++){ \
      if(((unsigned char *)(TC_aptr))[_TC_i] != ((unsigned char *)(TC_bptr))[_TC_i]){ \
        (TC_rv) = ((unsigned char *)(TC_aptr))[_TC_i] - ((unsigned char *)(TC_bptr))[_TC_i]; \
        break; \
      } \
    } \
    if((TC_rv) == 0) (TC_rv) = (TC_asiz) - (TC_bsiz); \
  } while(false)



#endif                                   // duplication check


// END OF FILE
