/*
 * General header file configurations that apply to
 * all daemons.  System dependent stuff goes here.
 *
 *   Version $Id$
 */
/*
   Copyright (C) 2000, 2001, 2002 Kern Sibbald and John Walker

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of
   the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA.

 */


#ifndef _BACONFIG_H
#define _BACONFIG_H 1

/* Bacula common configuration defines */

#define TRUE  1
#define FALSE 0

#ifndef ETIME
#define ETIME ETIMEDOUT
#endif

#ifdef PROTOTYPES
# define __PROTO(p)     p
#else
# define __PROTO(p)     ()
#endif

#ifdef DEBUG
#define ASSERT(x) if (!(x)) { \
   char *jcr = NULL; \
   Emsg1(M_ERROR, 0, "Failed ASSERT: %s\n", #x); \
   jcr[0] = 0; }
#else
#define ASSERT(x)
#endif

/* Allow printing of NULL pointers */
#define NPRT(x) (x)?(x):"*None*" 

#ifdef ENABLE_NLS
#include <libintl.h>
#define _(s) gettext((s))
#define N_(s) (s)
#else
#define _(s) (s)
#define N_(s) (s)
#define textdomain(x)
#endif


/* This should go away! ****FIXME***** */
#define MAXSTRING 500

/* Maximum length to edit time/date */
#define MAX_TIME_LENGTH 30

/* Maximum Name length including EOS */
#define MAX_NAME_LENGTH 128

/* Maximume number of user entered command args */
#define MAX_CMD_ARGS 30

/* All tape operations MUST be a multiple of this */
#define TAPE_BSIZE 1024
#if !defined(DEV_BSIZE) && defined(BSIZE)
#define DEV_BSIZE BSIZE
#endif

#ifndef DEV_BSIZE
#define DEV_BSIZE 512
#endif

/* Maximum number of bytes that you can push into a
 * socket.
 */
#define MAX_NETWORK_BUFFER_SIZE (32 * 1024)

/* Stream definitions.  Once defined these must NEVER
 *   change as they go on the storage media.
 */
#define STREAM_UNIX_ATTRIBUTES   1    /* Generic Unix attributes */
#define STREAM_FILE_DATA         2    /* Standard uncompressed data */
#define STREAM_MD5_SIGNATURE     3    /* MD5 signature for the file */
#define STREAM_GZIP_DATA         4    /* GZip compressed file data */
#define STREAM_WIN32_ATTRIBUTES  5    /* Windows attributes (superset of Unix) */
#define STREAM_SPARSE_DATA       6    /* Sparse data stream */
#define STREAM_SPARSE_GZIP_DATA  7
#define STREAM_PROGRAM_NAMES     8    /* program names for program data */
#define STREAM_PROGRAM_DATA      9    /* Data needing program */
#define STREAM_SHA1_SIGNATURE   10    /* SHA1 signature for the file */
#define STREAM_WIN32_DATA       11    /* Win32 BackupRead data */
#define STREAM_WIN32_GZIP_DATA  12    /* Gzipped Win32 BackupRead data */

/*
 * Internal code for Signature types
 */
#define NO_SIG   0
#define MD5_SIG  1
#define SHA1_SIG 2

/* Size of File Address stored in STREAM_SPARSE_DATA. Do NOT change! */
#define SPARSE_FADDR_SIZE (sizeof(uint64_t))


/* This is for dumb compilers/libraries like Solaris. Linux GCC
 * does it correctly, so it might be worthwhile
 * to remove the isascii(c) with ifdefs on such
 * "smart" systems.
 */
#define B_ISSPACE(c) (isascii((int)(c)) && isspace((int)(c)))
#define B_ISALPHA(c) (isascii((int)(c)) && isalpha((int)(c)))
#define B_ISUPPER(c) (isascii((int)(c)) && isupper((int)(c)))
#define B_ISDIGIT(c) (isascii((int)(c)) && isdigit((int)(c)))


typedef void (HANDLER)();
typedef int (INTHANDLER)();

#ifdef SETPGRP_VOID
# define SETPGRP_ARGS(x, y) /* No arguments */
#else
# define SETPGRP_ARGS(x, y) (x, y)
#endif

#ifndef S_ISLNK
#define S_ISLNK(m) (((m) & S_IFM) == S_IFLNK)
#endif

/* Added by KES to deal with Win32 systems */
#ifndef S_ISWIN32
#define S_ISWIN32 020000
#endif

#ifndef INADDR_NONE
#define INADDR_NONE ((unsigned long) -1)
#endif

#ifdef TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#ifndef O_BINARY
#define O_BINARY 0
#endif

#ifndef O_NOFOLLOW
#define O_NOFOLLOW 0
#endif

#ifndef MODE_RW
#define MODE_RW 0666
#endif

#ifdef DEBUG_MUTEX
extern void _p(char *file, int line, pthread_mutex_t *m);
extern void _v(char *file, int line, pthread_mutex_t *m);

#define P(x) _p(__FILE__, __LINE__, &(x))
#define V(x) _v(__FILE__, __LINE__, &(x))

#else

/* These probably should be subroutines */
#define P(x) \
   do { int errstat; if ((errstat=pthread_mutex_lock(&(x)))) \
      e_msg(__FILE__, __LINE__, M_ABORT, 0, "Mutex lock failure. ERR=%s\n",\
           strerror(errstat)); \
   } while(0)

#define V(x) \
   do { int errstat; if ((errstat=pthread_mutex_unlock(&(x)))) \
         e_msg(__FILE__, __LINE__, M_ABORT, 0, "Mutex unlock failure. ERR=%s\n",\
           strerror(errstat)); \
   } while(0)

#endif /* DEBUG_MUTEX */

/* These probably should be subroutines */
#define Pw(x) \
   do { int errstat; if ((errstat=rwl_writelock(&(x)))) \
      e_msg(__FILE__, __LINE__, M_ABORT, 0, "Write lock lock failure. ERR=%s\n",\
           strerror(errstat)); \
   } while(0)

#define Vw(x) \
   do { int errstat; if ((errstat=rwl_writeunlock(&(x)))) \
         e_msg(__FILE__, __LINE__, M_ABORT, 0, "Write lock unlock failure. ERR=%s\n",\
           strerror(errstat)); \
   } while(0)


/*
 * The digit following Dmsg and Emsg indicates the number of substitutions in
 * the message string. We need to do this kludge because non-GNU compilers
 * do not handle varargs #defines.
 */
/* Debug Messages that are printed */
#ifdef DEBUG
#define Dmsg0(lvl, msg)             d_msg(__FILE__, __LINE__, lvl, msg)
#define Dmsg1(lvl, msg, a1)         d_msg(__FILE__, __LINE__, lvl, msg, a1)
#define Dmsg2(lvl, msg, a1, a2)     d_msg(__FILE__, __LINE__, lvl, msg, a1, a2)
#define Dmsg3(lvl, msg, a1, a2, a3) d_msg(__FILE__, __LINE__, lvl, msg, a1, a2, a3)
#define Dmsg4(lvl, msg, arg1, arg2, arg3, arg4) d_msg(__FILE__, __LINE__, lvl, msg, arg1, arg2, arg3, arg4)
#define Dmsg5(lvl, msg, a1, a2, a3, a4, a5) d_msg(__FILE__, __LINE__, lvl, msg, a1, a2, a3, a4, a5)
#define Dmsg6(lvl, msg, a1, a2, a3, a4, a5, a6) d_msg(__FILE__, __LINE__, lvl, msg, a1, a2, a3, a4, a5, a6)
#define Dmsg7(lvl, msg, a1, a2, a3, a4, a5, a6, a7) d_msg(__FILE__, __LINE__, lvl, msg, a1, a2, a3, a4, a5, a6, a7)
#define Dmsg8(lvl, msg, a1, a2, a3, a4, a5, a6, a7, a8) d_msg(__FILE__, __LINE__, lvl, msg, a1, a2, a3, a4, a5, a6, a7, a8)
#define Dmsg9(lvl,msg,a1,a2,a3,a4,a5,a6,a7,a8,a9) d_msg(__FILE__,__LINE__,lvl,msg,a1,a2,a3,a4,a5,a6,a7,a8,a9)
#define Dmsg10(lvl,msg,a1,a2,a3,a4,a5,a6,a7,a8,a9,a10) d_msg(__FILE__,__LINE__,lvl,msg,a1,a2,a3,a4,a5,a6,a7,a8,a9,a10)
#define Dmsg11(lvl,msg,a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11) d_msg(__FILE__,__LINE__,lvl,msg,a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11)
#define Dmsg12(lvl,msg,a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12) d_msg(__FILE__,__LINE__,lvl,msg,a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12)
#define Dmsg13(lvl,msg,a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13) d_msg(__FILE__,__LINE__,lvl,msg,a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13)
#else
#define Dmsg0(lvl, msg)
#define Dmsg1(lvl, msg, a1)
#define Dmsg2(lvl, msg, a1, a2)
#define Dmsg3(lvl, msg, a1, a2, a3)
#define Dmsg4(lvl, msg, arg1, arg2, arg3, arg4)
#define Dmsg5(lvl, msg, a1, a2, a3, a4, a5)
#define Dmsg6(lvl, msg, a1, a2, a3, a4, a5, a6)
#define Dmsg7(lvl, msg, a1, a2, a3, a4, a5, a6, a7)
#define Dmsg8(lvl, msg, a1, a2, a3, a4, a5, a6, a7, a8)
#define Dmsg11(lvl,msg,a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11)
#define Dmsg12(lvl,msg,a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12)
#define Dmsg13(lvl,msg,a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13)
#endif /* DEBUG */

#ifdef TRACE_FILE
#define Tmsg0(lvl, msg)             t_msg(__FILE__, __LINE__, lvl, msg)
#define Tmsg1(lvl, msg, a1)         t_msg(__FILE__, __LINE__, lvl, msg, a1)
#define Tmsg2(lvl, msg, a1, a2)     t_msg(__FILE__, __LINE__, lvl, msg, a1, a2)
#define Tmsg3(lvl, msg, a1, a2, a3) t_msg(__FILE__, __LINE__, lvl, msg, a1, a2, a3)
#define Tmsg4(lvl, msg, arg1, arg2, arg3, arg4) t_msg(__FILE__, __LINE__, lvl, msg, arg1, arg2, arg3, arg4)
#define Tmsg5(lvl, msg, a1, a2, a3, a4, a5) t_msg(__FILE__, __LINE__, lvl, msg, a1, a2, a3, a4, a5)
#define Tmsg6(lvl, msg, a1, a2, a3, a4, a5, a6) t_msg(__FILE__, __LINE__, lvl, msg, a1, a2, a3, a4, a5, a6)
#define Tmsg7(lvl, msg, a1, a2, a3, a4, a5, a6, a7) t_msg(__FILE__, __LINE__, lvl, msg, a1, a2, a3, a4, a5, a6, a7)
#define Tmsg8(lvl, msg, a1, a2, a3, a4, a5, a6, a7, a8) t_msg(__FILE__, __LINE__, lvl, msg, a1, a2, a3, a4, a5, a6, a7, a8)
#define Tmsg9(lvl,msg,a1,a2,a3,a4,a5,a6,a7,a8,a9) t_msg(__FILE__,__LINE__,lvl,msg,a1,a2,a3,a4,a5,a6,a7,a8,a9)
#define Tmsg10(lvl,msg,a1,a2,a3,a4,a5,a6,a7,a8,a9,a10) t_msg(__FILE__,__LINE__,lvl,msg,a1,a2,a3,a4,a5,a6,a7,a8,a9,a10)
#define Tmsg11(lvl,msg,a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11) t_msg(__FILE__,__LINE__,lvl,msg,a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11)
#define Tmsg12(lvl,msg,a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12) t_msg(__FILE__,__LINE__,lvl,msg,a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12)
#define Tmsg13(lvl,msg,a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13) t_msg(__FILE__,__LINE__,lvl,msg,a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13)
#else
#define Tmsg0(lvl, msg)
#define Tmsg1(lvl, msg, a1)
#define Tmsg2(lvl, msg, a1, a2)
#define Tmsg3(lvl, msg, a1, a2, a3)
#define Tmsg4(lvl, msg, arg1, arg2, arg3, arg4)
#define Tmsg5(lvl, msg, a1, a2, a3, a4, a5)
#define Tmsg6(lvl, msg, a1, a2, a3, a4, a5, a6)
#define Tmsg7(lvl, msg, a1, a2, a3, a4, a5, a6, a7)
#define Tmsg8(lvl, msg, a1, a2, a3, a4, a5, a6, a7, a8)
#define Tmsg11(lvl,msg,a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11)
#define Tmsg12(lvl,msg,a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12)
#define Tmsg13(lvl,msg,a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13)
#endif /* TRACE_FILE */



/* Messages that are printed (uses d_msg) */
#define Pmsg0(lvl, msg)             p_msg(__FILE__, __LINE__, lvl, msg)
#define Pmsg1(lvl, msg, a1)         p_msg(__FILE__, __LINE__, lvl, msg, a1)
#define Pmsg2(lvl, msg, a1, a2)     p_msg(__FILE__, __LINE__, lvl, msg, a1, a2)
#define Pmsg3(lvl, msg, a1, a2, a3) p_msg(__FILE__, __LINE__, lvl, msg, a1, a2, a3)
#define Pmsg4(lvl, msg, arg1, arg2, arg3, arg4) p_msg(__FILE__, __LINE__, lvl, msg, arg1, arg2, arg3, arg4)
#define Pmsg5(lvl, msg, a1, a2, a3, a4, a5) p_msg(__FILE__, __LINE__, lvl, msg, a1, a2, a3, a4, a5)
#define Pmsg6(lvl, msg, a1, a2, a3, a4, a5, a6) p_msg(__FILE__, __LINE__, lvl, msg, a1, a2, a3, a4, a5, a6)
#define Pmsg7(lvl, msg, a1, a2, a3, a4, a5, a6, a7) p_msg(__FILE__, __LINE__, lvl, msg, a1, a2, a3, a4, a5, a6, a7)
#define Pmsg8(lvl, msg, a1, a2, a3, a4, a5, a6, a7, a8) p_msg(__FILE__, __LINE__, lvl, msg, a1, a2, a3, a4, a5, a6, a7, a8)
#define Pmsg9(lvl,msg,a1,a2,a3,a4,a5,a6,a7,a8,a9) p_msg(__FILE__,__LINE__,lvl,msg,a1,a2,a3,a4,a5,a6,a7,a8,a9)
#define Pmsg10(lvl,msg,a1,a2,a3,a4,a5,a6,a7,a8,a9,a10) p_msg(__FILE__,__LINE__,lvl,msg,a1,a2,a3,a4,a5,a6,a7,a8,a9,a10)
#define Pmsg11(lvl,msg,a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11) p_msg(__FILE__,__LINE__,lvl,msg,a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11)
#define Pmsg12(lvl,msg,a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12) p_msg(__FILE__,__LINE__,lvl,msg,a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12)
#define Pmsg13(lvl,msg,a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13) p_msg(__FILE__,__LINE__,lvl,msg,a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13)
#define Pmsg14(lvl,msg,a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14) p_msg(__FILE__,__LINE__,lvl,msg,a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14)

       
/* Daemon Error Messages that are delivered according to the message resource */
#define Emsg0(typ, lvl, msg)             e_msg(__FILE__, __LINE__, typ, lvl, msg)
#define Emsg1(typ, lvl, msg, a1)         e_msg(__FILE__, __LINE__, typ, lvl, msg, a1)
#define Emsg2(typ, lvl, msg, a1, a2)     e_msg(__FILE__, __LINE__, typ, lvl, msg, a1, a2)
#define Emsg3(typ, lvl, msg, a1, a2, a3) e_msg(__FILE__, __LINE__, typ, lvl, msg, a1, a2, a3)
#define Emsg4(typ, lvl, msg, a1, a2, a3, a4) e_msg(__FILE__, __LINE__, typ, lvl, msg, a1, a2, a3, a4)
#define Emsg5(typ, lvl, msg, a1, a2, a3, a4, a5) e_msg(__FILE__, __LINE__, typ, lvl, msg, a1, a2, a3, a4, a5)
#define Emsg6(typ, lvl, msg, a1, a2, a3, a4, a5, a6) e_msg(__FILE__, __LINE__, typ, lvl, msg, a1, a2, a3, a4, a5, a6)

/* Job Error Messages that are delivered according to the message resource */
#define Jmsg0(jcr, typ, lvl, msg)             j_msg(__FILE__, __LINE__, jcr, typ, lvl, msg)
#define Jmsg1(jcr, typ, lvl, msg, a1)         j_msg(__FILE__, __LINE__, jcr, typ, lvl, msg, a1)
#define Jmsg2(jcr, typ, lvl, msg, a1, a2)     j_msg(__FILE__, __LINE__, jcr, typ, lvl, msg, a1, a2)
#define Jmsg3(jcr, typ, lvl, msg, a1, a2, a3) j_msg(__FILE__, __LINE__, jcr, typ, lvl, msg, a1, a2, a3)
#define Jmsg4(jcr, typ, lvl, msg, a1, a2, a3, a4) j_msg(__FILE__, __LINE__, jcr, typ, lvl, msg, a1, a2, a3, a4)
#define Jmsg5(jcr, typ, lvl, msg, a1, a2, a3, a4, a5) j_msg(__FILE__, __LINE__, jcr, typ, lvl, msg, a1, a2, a3, a4, a5)
#define Jmsg6(jcr, typ, lvl, msg, a1, a2, a3, a4, a5, a6) j_msg(__FILE__, __LINE__, jcr, typ, lvl, msg, a1, a2, a3, a4, a5, a6)


/* Memory Messages that are edited into a Pool Memory buffer */
#define Mmsg0(buf, msg)             m_msg(__FILE__, __LINE__, buf, msg)
#define Mmsg1(buf, msg, a1)         m_msg(__FILE__, __LINE__, buf, msg, a1)
#define Mmsg2(buf, msg, a1, a2)     m_msg(__FILE__, __LINE__, buf, msg, a1, a2)
#define Mmsg3(buf, msg, a1, a2, a3) m_msg(__FILE__, __LINE__, buf, msg, a1, a2, a3)
#define Mmsg4(buf, msg, a1, a2, a3, a4) m_msg(__FILE__, __LINE__, buf, msg, a1, a2, a3, a4)
#define Mmsg5(buf, msg, a1, a2, a3, a4, a5) m_msg(__FILE__, __LINE__, buf, msg, a1, a2, a3, a4, a5)
#define Mmsg6(buf, msg, a1, a2, a3, a4, a5, a6) m_msg(__FILE__, __LINE__, buf, msg, a1, a2, a3, a4, a5, a6)
#define Mmsg7(buf, msg, a1, a2, a3, a4, a5, a6, a7) m_msg(__FILE__, __LINE__, buf, msg, a1, a2, a3, a4, a5, a6)
#define Mmsg8(buf,msg,a1,a2,a3,a4,a5,a6,a7,a8) m_msg(__FILE__,__LINE__,buf,msg,a1,a2,a3,a4,a5,a6)
#define Mmsg11(buf,msg,a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11) m_msg(__FILE__,__LINE__,buf,msg,a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11)
#define Mmsg15(buf,msg,a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15) m_msg(__FILE__,__LINE__,buf,msg,a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15)

/* Edit message into Pool Memory buffer -- no __FILE__ and __LINE__ */
int  Mmsg(char **msgbuf, char *fmt,...);


struct JCR;
void d_msg(char *file, int line, int level, char *fmt,...);
void p_msg(char *file, int line, int level, char *fmt,...);
void e_msg(char *file, int line, int type, int level, char *fmt,...);
void j_msg(char *file, int line, JCR *jcr, int type, int level, char *fmt,...);
int  m_msg(char *file, int line, char **msgbuf, char *fmt,...);


/* Use our strdup with smartalloc */
#undef strdup
#define strdup(buf) bad_call_on_strdup_use_bstrdup(buf)

#ifdef DEBUG
#define bstrdup(str) strcpy((char *) b_malloc(__FILE__,__LINE__,strlen((str))+1),(str))
#else
#define bstrdup(str) strcpy((char *) bmalloc(strlen((str))+1),(str))
#endif

#ifdef DEBUG
#define bmalloc(size) b_malloc(__FILE__, __LINE__, (size))
#endif

#ifdef __alpha__
#define OSF 1
#endif

#ifdef HAVE_SUN_OS
   /* 
    * On Solaris 2.5, threads are not timesliced by default, so we need to
    * explictly increase the conncurrency level.
    */
#include <thread.h>
#define set_thread_concurrency(x)  thr_setconcurrency(x)
extern int thr_setconcurrency(int);
#define SunOS 1

#else


/* Not needed on most systems */
#define set_thread_concurrency(x)

#endif

#ifdef HAVE_DARWIN_OS
/* Apparently someone forgot to wrap getdomainname as a C function */
extern "C" int getdomainname(char *name, size_t len);

/* Darwin lib fnmatch() doesn't work, so use our own */
#undef HAVE_FNMATCH
#endif

#ifdef HAVE_CYGWIN
/* They don't really have it */
#undef HAVE_GETDOMAINNAME
#endif

#ifdef HAVE_AIX_OS
#endif
 
/* This probably should be done on a machine by machine basic, but it works */
#define ALIGN_SIZE (sizeof(double))
#define BALIGN(x) (((x) + ALIGN_SIZE - 1) & ~(ALIGN_SIZE -1))


/* Added by KES to deal with Win32 systems */
#ifndef S_ISWIN32
#define S_ISWIN32 020000
#endif

/*
 * Replace codes needed in both file routines and non-file routines
 * Job replace codes -- in "replace"   
 */
#define REPLACE_ALWAYS   'a'
#define REPLACE_IFNEWER  'w'
#define REPLACE_NEVER    'n'
#define REPLACE_IFOLDER  'o'

#endif /* _BACONFIG_H */
