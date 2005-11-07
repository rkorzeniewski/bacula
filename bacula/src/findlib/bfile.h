/*
 *  Bacula low level File I/O routines.  This routine simulates
 *    open(), read(), write(), and close(), but using native routines.
 *    I.e. on Windows, we use Windows APIs.
 *
 *     Kern Sibbald May MMIII
 */
/*
   Copyright (C) 2003-2005 Kern Sibbald

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   version 2 as amended with additional clauses defined in the
   file LICENSE in the main source directory.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the 
   the file LICENSE for additional details.

 */

#ifndef __BFILE_H
#define __BFILE_H

#ifdef HAVE_PYTHON
#undef _POSIX_C_SOURCE
#include <Python.h>
struct Python_IO {
   PyObject *fo;
   PyObject *fr;
   PyObject *fc;
};
#else
struct Python_IO {
};
#endif

#ifdef USE_WIN32STREAMEXTRACTION

/* this should physically correspond to WIN32_STREAM_ID
 * from winbase.h on Win32. We didn't inlcude cStreamName
 * as we don't use it and don't need it for a correct struct size.
 */

typedef struct _BWIN32_STREAM_ID {
        int32_t        dwStreamId;
        int32_t        dwStreamAttributes;
        int64_t        Size;
        int32_t        dwStreamNameSize;        
} BWIN32_STREAM_ID, *LPBWIN32_STREAM_ID ;


typedef struct _PROCESS_WIN32_BACKUPAPIBLOCK_CONTEXT {
        int64_t          liNextHeader;
        bool             bIsInData;
        BWIN32_STREAM_ID header_stream;        
} PROCESS_WIN32_BACKUPAPIBLOCK_CONTEXT;
#endif

/*  =======================================================
 *
 *   W I N D O W S
 *
 *  =======================================================
 */
#if defined(HAVE_CYGWIN) || defined(HAVE_WIN32)

#include <windows.h>
#include "../lib/winapi.h"

enum {
   BF_CLOSED,
   BF_READ,                           /* BackupRead */
   BF_WRITE                           /* BackupWrite */
};

/* In bfile.c */

/* Basic Win32 low level I/O file packet */
struct BFILE {
   int use_backup_api;                /* set if using BackupRead/Write */
   int mode;                          /* set if file is open */
   HANDLE fh;                         /* Win32 file handle */
   int fid;                           /* fd if doing Unix style */
   LPVOID lpContext;                  /* BackupRead/Write context */
   POOLMEM *errmsg;                   /* error message buffer */
   DWORD rw_bytes;                    /* Bytes read or written */
   DWORD lerror;                      /* Last error code */
   int berrno;                        /* errno */
   char *prog;                        /* reader/writer program if any */
   JCR *jcr;                          /* jcr for editing job codes */
   Python_IO pio;                     /* Python I/O routines */
#ifdef USE_WIN32STREAMEXTRACTION
   PROCESS_WIN32_BACKUPAPIBLOCK_CONTEXT win32DecompContext; /* context for decomposition of win32 backup streams */
   int use_backup_decomp;             /* set if using BackupRead Stream Decomposition */
#endif
};

HANDLE bget_handle(BFILE *bfd);

#else   /* Linux/Unix systems */

/*  =======================================================
 *
 *   U N I X
 *
 *  =======================================================
 */

/* Basic Unix low level I/O file packet */
struct BFILE {
   int fid;                           /* file id on Unix */
   int berrno;
   char *prog;                        /* reader/writer program if any */
   JCR *jcr;                          /* jcr for editing job codes */
   Python_IO pio;                     /* Python I/O routines */
#ifdef USE_WIN32STREAMEXTRACTION
   PROCESS_WIN32_BACKUPAPIBLOCK_CONTEXT win32DecompContext; /* context for decomposition of win32 backup streams */
   int use_backup_decomp;             /* set if using BackupRead Stream Decomposition */
#endif
};

#endif

void    binit(BFILE *bfd);
bool    is_bopen(BFILE *bfd);
bool    set_win32_backup(BFILE *bfd);
bool    set_portable_backup(BFILE *bfd);
bool    set_prog(BFILE *bfd, char *prog, JCR *jcr);
bool    have_win32_api();
bool    is_portable_backup(BFILE *bfd);
bool    is_stream_supported(int stream);
bool    is_win32_stream(int stream);
char   *xberror(BFILE *bfd);          /* DO NOT USE  -- use berrno class */
int     bopen(BFILE *bfd, const char *fname, int flags, mode_t mode);
#ifdef HAVE_DARWIN_OS
int     bopen_rsrc(BFILE *bfd, const char *fname, int flags, mode_t mode);
#endif
int     bclose(BFILE *bfd);
ssize_t bread(BFILE *bfd, void *buf, size_t count);
ssize_t bwrite(BFILE *bfd, void *buf, size_t count);
off_t   blseek(BFILE *bfd, off_t offset, int whence);
const char   *stream_to_ascii(int stream);

#ifdef USE_WIN32STREAMEXTRACTION
BOOL processWin32BackupAPIBlock (BFILE *bfd, void *pBuffer, size_t dwSize);
#endif

#endif /* __BFILE_H */
