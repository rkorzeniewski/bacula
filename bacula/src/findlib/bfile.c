/*
 *  Bacula low level File I/O routines.  This routine simulates
 *    open(), read(), write(), and close(), but using native routines.
 *    I.e. on Windows, we use Windows APIs.
 *
 *    Kern Sibbald, April MMIII
 *
 *   Version $Id$
 *
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

#include "bacula.h"
#include "find.h"

#ifdef HAVE_CYGWIN

void unix_name_to_win32(POOLMEM **win32_name, char *name);
extern "C" HANDLE get_osfhandle(int fd);


void binit(BFILE *bfd)
{
   bfd->fid = -1;
   bfd->mode = BF_CLOSED;
   bfd->use_win_api = p_BackupRead && p_BackupWrite;
   bfd->errmsg = NULL;
   bfd->lpContext = NULL;
   bfd->lerror = 0;
}

HANDLE bget_handle(BFILE *bfd)
{
   if (!bfd->use_win_api) {
      return get_osfhandle(bfd->fid);
   }
   return bfd->fh;
}

int bopen(BFILE *bfd, const char *fname, int flags, mode_t mode)
{
   POOLMEM *win32_fname;

   if (!bfd->use_win_api) {
      bfd->fid = open(fname, flags, mode);
      if (bfd->fid >= 0) {
	 bfd->mode = BF_READ;	      /* not important if BF_READ or BF_WRITE */
      }
      return bfd->fid;
   }

   /* Convert to Windows path format */
   win32_fname = get_pool_memory(PM_FNAME);
   unix_name_to_win32(&win32_fname, (char *)fname);

   if (flags & O_WRONLY) {	      /* creating */
      bfd->fh = CreateFile(win32_fname,
		   FILE_ALL_ACCESS|WRITE_DAC|ACCESS_SYSTEM_SECURITY,	/* access */
		   0,
		   NULL,				   /* SecurityAttributes */
		   CREATE_ALWAYS,			   /* CreationDisposition */
		   FILE_ATTRIBUTE_NORMAL|FILE_FLAG_BACKUP_SEMANTICS,   /* Flags and attributes */
		   NULL);				   /* TemplateFile */

      if (bfd->fh == INVALID_HANDLE_VALUE) {
	 bfd->lerror = GetLastError();
	 bfd->mode = BF_CLOSED;
      } else {
	 bfd->mode = BF_WRITE;
      }
   } else {
      bfd->fh = CreateFile(win32_fname,
		   GENERIC_READ|ACCESS_SYSTEM_SECURITY,       /* access */
		   FILE_SHARE_READ,			    /* shared mode */
		   NULL,				   /* SecurityAttributes */
		   OPEN_EXISTING,			   /* CreationDisposition */
		   FILE_FLAG_BACKUP_SEMANTICS,	/* Flags and attributes */
		   NULL);				   /* TemplateFile */

      if (bfd->fh == INVALID_HANDLE_VALUE) {
	 bfd->lerror = GetLastError();
	 bfd->mode = BF_CLOSED;
      } else {
	 bfd->mode = BF_READ;
      }
   }
   bfd->errmsg = NULL;
   bfd->lpContext = NULL;
   return bfd->mode == BF_CLOSED ? -1 : 1;
}

/* 
 * Returns  0 on success
 *	   -1 on error
 */
int bclose(BFILE *bfd)
{ 
   int stat = 0;
   if (!bfd->use_win_api) {
      int stat = close(bfd->fid);
      bfd->fid = -1;
      bfd->mode = BF_CLOSED;
      return stat;
   }
   if (bfd->errmsg) {
      free_pool_memory(bfd->errmsg);
      bfd->errmsg = NULL;
   }
   if (bfd->mode == BF_CLOSED) {
      return 0;
   }
   if (bfd->mode == BF_READ) {
      BYTE buf[10];
      if (!bfd->lpContext && !p_BackupRead(bfd->fh,   
	      buf,		      /* buffer */
	      (DWORD)0, 	      /* bytes to read */
	      &bfd->rw_bytes,	      /* bytes read */
	      1,		      /* Abort */
	      1,		      /* ProcessSecurity */
	      &bfd->lpContext)) {     /* Read context */
	 stat = -1;
      } 
   } else {
      BYTE buf[10];
      if (!bfd->lpContext && !p_BackupWrite(bfd->fh,   
	      buf,		      /* buffer */
	      (DWORD)0, 	      /* bytes to read */
	      &bfd->rw_bytes,	      /* bytes written */
	      1,		      /* Abort */
	      1,		      /* ProcessSecurity */
	      &bfd->lpContext)) {     /* Write context */
	 stat = -1;
      } 
   }
   if (!CloseHandle(bfd->fh)) {
      stat = -1;
   }
   bfd->mode = BF_CLOSED;
   bfd->lpContext = NULL;
   return stat;
}

/*
 * Generate error message 
 */
char *berror(BFILE *bfd)
{
   LPTSTR msg;

   if (!bfd->use_win_api) {
      return strerror(errno);
   }
   FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|
		 FORMAT_MESSAGE_FROM_SYSTEM,
		 NULL,
		 bfd->lerror,
		 0,
		 (LPTSTR)&msg,
		 0,
		 NULL);
   strip_trailing_junk(msg);
   if (!bfd->errmsg) {
      bfd->errmsg = get_pool_memory(PM_FNAME);
   }
   pm_strcpy(&bfd->errmsg, msg);
   LocalFree(msg);
   return bfd->errmsg;
}

/* Returns: bytes read on success
 *	     0	       on EOF
 *	    -1	       on error
 */
ssize_t bread(BFILE *bfd, void *buf, size_t count)
{
   if (!bfd->use_win_api) {
      return read(bfd->fid, buf, count);
   }

   bfd->rw_bytes = 0;
   if (!p_BackupRead(bfd->fh, 
	(BYTE *)buf,
	count,
	&bfd->rw_bytes,
	0,			     /* no Abort */
	1,			     /* Process Security */
	&bfd->lpContext)) {	     /* Context */
      bfd->lerror = GetLastError();
      return -1;
   }
   return (ssize_t)bfd->rw_bytes;
}

ssize_t bwrite(BFILE *bfd, void *buf, size_t count)
{
   if (!bfd->use_win_api) {
      return write(bfd->fid, buf, count);
   }

   bfd->rw_bytes = 0;
   if (!p_BackupWrite(bfd->fh,
	(BYTE *)buf,
	count,
	&bfd->rw_bytes,
	0,			     /* No abort */
	1,			     /* Process Security */
	&bfd->lpContext)) {	     /* Context */
      bfd->lerror = GetLastError();
      return -1;
   }
   return (ssize_t)bfd->rw_bytes;
}

int is_bopen(BFILE *bfd)
{
   return bfd->mode != BF_CLOSED;
}

off_t blseek(BFILE *bfd, off_t offset, int whence)
{
   if (!bfd->use_win_api) {
      return lseek(bfd->fid, offset, whence);
   }
   return -1;
}

#else  /* Unix systems */

/* ===============================================================
 * 
 *	      U N I X
 *
 * ===============================================================
 */
void binit(BFILE *bfd)
{
   bfd->fid = -1;
}

int bopen(BFILE *bfd, const char *fname, int flags, mode_t mode)
{
   return bfd->fid = open(fname, flags, mode);
}

int bclose(BFILE *bfd)
{ 
   int stat = close(bfd->fid);
   bfd->fid = -1;
   return stat;
}

ssize_t bread(BFILE *bfd, void *buf, size_t count)
{
   return read(bfd->fid, buf, count);
}

ssize_t bwrite(BFILE *bfd, void *buf, size_t count)
{
   return write(bfd->fid, buf, count);
}

int is_bopen(BFILE *bfd)
{
   return bfd->fid >= 0;
}

off_t blseek(BFILE *bfd, off_t offset, int whence)
{
   return lseek(bfd->fid, offset, whence);
}

char *berror(BFILE *bfd)
{
    return strerror(errno);
}

#endif
