/*
 *  Bacula low level File I/O routines.  This routine simulates
 *    open(), read(), write(), and close(), but using native routines.
 *    I.e. on Windows, we use Windows APIs.
 *
 *     Kern Sibbald May MMIII
 */
/*
   Copyright (C) 2000-2003 Kern Sibbald and John Walker

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

#ifndef __BFILE_H
#define __BFILE_H

#ifdef HAVE_CYGWIN

#include <windows.h>
#include "winapi.h"

#define BF_CLOSED 0
#define BF_READ   1		      /* BackupRead */
#define BF_WRITE  2		      /* BackupWrite */

/* In bfile.c */

/* Basic low level I/O file packet */
typedef struct s_bfile {
#ifdef xxx
   int use_win_api;		      /* set if using WinAPI */
#endif
   int use_backup_api;		      /* set if using BackupRead/Write */
   int mode;			      /* set if file is open */
   HANDLE fh;			      /* Win32 file handle */
   int fid;			      /* fd if doing Unix style */
   LPVOID lpContext;		      /* BackupRead/Write context */
   POOLMEM *errmsg;		      /* error message buffer */
   DWORD rw_bytes;		      /* Bytes read or written */
   DWORD lerror;		      /* Last error code */
} BFILE;

HANDLE bget_handle(BFILE *bfd);

#else	/* Linux/Unix systems */

/* Basic low level I/O file packet */
typedef struct s_bfile {
   int fid;			      /* file id on Unix */
   int berrno;
} BFILE;

#endif

void binit(BFILE *bfd);
int is_bopen(BFILE *bfd);
int is_win32_data(BFILE *bfd);
char *berror(BFILE *bfd);
int bopen(BFILE *bfd, const char *fname, int flags, mode_t mode);
int bclose(BFILE *bfd);
ssize_t bread(BFILE *bfd, void *buf, size_t count);
ssize_t bwrite(BFILE *bfd, void *buf, size_t count);
off_t blseek(BFILE *bfd, off_t offset, int whence);

#endif /* __BFILE_H */
