/*
 *  Encode and decode standard Unix attributes and
 *   Extended attributes for Win32 and
 *   other non-Unix systems, or Unix systems with ACLs, ...
 *
 *    Kern Sibbald, October MMII
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
#include "jcr.h"

#ifdef HAVE_CYGWIN

/* Forward referenced subroutines */
static
int set_win32_attributes(void *jcr, char *fname, char *ofile, char *lname, 
			 int type, int stream, struct stat *statp,
			 char *attribsEx, BFILE *ofd);
void unix_name_to_win32(POOLMEM **win32_name, char *name);
void win_error(void *jcr, char *prefix, POOLMEM *ofile);
HANDLE bget_handle(BFILE *bfd);
#endif

/* For old systems that don't have lchown() use chown() */
#ifndef HAVE_LCHOWN
#define lchown chown
#endif

/*=============================================================*/
/*							       */
/*	       ***  A l l  S y s t e m s ***		       */
/*							       */
/*=============================================================*/


/* Encode a stat structure into a base64 character string */
void encode_stat(char *buf, struct stat *statp, uint32_t LinkFI)
{
   char *p = buf;
   /*
    * NOTE: we should use rdev as major and minor device if
    * it is a block or char device (S_ISCHR(statp->st_mode)
    * or S_ISBLK(statp->st_mode)).  In all other cases,
    * it is not used.	
    *
    */
   p += to_base64((int64_t)statp->st_dev, p);
   *p++ = ' ';                        /* separate fields with a space */
   p += to_base64((int64_t)statp->st_ino, p);
   *p++ = ' ';
   p += to_base64((int64_t)statp->st_mode, p);
   *p++ = ' ';
   p += to_base64((int64_t)statp->st_nlink, p);
   *p++ = ' ';
   p += to_base64((int64_t)statp->st_uid, p);
   *p++ = ' ';
   p += to_base64((int64_t)statp->st_gid, p);
   *p++ = ' ';
   p += to_base64((int64_t)statp->st_rdev, p);
   *p++ = ' ';
   p += to_base64((int64_t)statp->st_size, p);
   *p++ = ' ';
   p += to_base64((int64_t)statp->st_blksize, p);
   *p++ = ' ';
   p += to_base64((int64_t)statp->st_blocks, p);
   *p++ = ' ';
   p += to_base64((int64_t)statp->st_atime, p);
   *p++ = ' ';
   p += to_base64((int64_t)statp->st_mtime, p);
   *p++ = ' ';
   p += to_base64((int64_t)statp->st_ctime, p);
   *p++ = ' ';
   p += to_base64((int64_t)LinkFI, p);

/* FreeBSD function */
#ifdef HAVE_CHFLAGS
   *p++ = ' ';
   p += to_base64((int64_t)statp->st_flags, p);
#endif
   *p = 0;
   return;
}



/* Decode a stat packet from base64 characters */
void
decode_stat(char *buf, struct stat *statp, uint32_t *LinkFI) 
{
   char *p = buf;
   int64_t val;

   p += from_base64(&val, p);
   statp->st_dev = val;
   p++; 			      /* skip space */
   p += from_base64(&val, p);
   statp->st_ino = val;
   p++;
   p += from_base64(&val, p);
   statp->st_mode = val;
   p++;
   p += from_base64(&val, p);
   statp->st_nlink = val;
   p++;
   p += from_base64(&val, p);
   statp->st_uid = val;
   p++;
   p += from_base64(&val, p);
   statp->st_gid = val;
   p++;
   p += from_base64(&val, p);
   statp->st_rdev = val;
   p++;
   p += from_base64(&val, p);
   statp->st_size = val;
   p++;
   p += from_base64(&val, p);
   statp->st_blksize = val;
   p++;
   p += from_base64(&val, p);
   statp->st_blocks = val;
   p++;
   p += from_base64(&val, p);
   statp->st_atime = val;
   p++;
   p += from_base64(&val, p);
   statp->st_mtime = val;
   p++;
   p += from_base64(&val, p);
   statp->st_ctime = val;
   /* Optional FileIndex of hard linked file data */
   if (*p == ' ' || (*p != 0 && *(p+1) == ' ')) {
      p++;
      p += from_base64(&val, p);
      *LinkFI = (uint32_t)val;
  } else {
      *LinkFI = 0;
  }

/* FreeBSD user flags */
#ifdef HAVE_CHFLAGS
   if (*p == ' ' || (*p != 0 && *(p+1) == ' ')) {
      p++;
      p += from_base64(&val, p);
      statp->st_flags = (uint32_t)val;
  } else {
      statp->st_flags  = 0;
  }
#endif
}

/*
 * Set file modes, permissions and times
 *
 *  fname is the original filename  
 *  ofile is the output filename (may be in a different directory)
 *
 * Returns:  1 on success
 *	     0 on failure
 */
int set_attributes(void *jcr, char *fname, char *ofile, char *lname, 
		   int type, int stream, struct stat *statp,
		   char *attribsEx, BFILE *ofd)
{
   struct utimbuf ut;	 
   mode_t old_mask;
   int stat = 1;

#ifdef HAVE_CYGWIN
   if (set_win32_attributes(jcr, fname, ofile, lname, type, stream,
			    statp, attribsEx, ofd)) {
      return 1;
   }
   /*
    * If Windows stuff failed, e.g. attempt to restore Unix file
    *  to Windows, simply fall through and we will do it the	 
    *  universal way.
    */
#endif

   old_mask = umask(0);
   if (is_bopen(ofd)) {
      bclose(ofd);		      /* first close file */
   }

   ut.actime = statp->st_atime;
   ut.modtime = statp->st_mtime;

   /* ***FIXME**** optimize -- don't do if already correct */
   /* 
    * For link, change owner of link using lchown, but don't
    *	try to do a chmod as that will update the file behind it.
    */
   if (type == FT_LNK) {
      /* Change owner of link, not of real file */
      if (lchown(ofile, statp->st_uid, statp->st_gid) < 0) {
         Jmsg2(jcr, M_WARNING, 0, "Unable to set file owner %s: ERR=%s\n",
	    ofile, strerror(errno));
	 stat = 0;
      }
   } else {
      if (chown(ofile, statp->st_uid, statp->st_gid) < 0) {
         Jmsg2(jcr, M_WARNING, 0, "Unable to set file owner %s: ERR=%s\n",
	    ofile, strerror(errno));
	 stat = 0;
      }
      if (chmod(ofile, statp->st_mode) < 0) {
         Jmsg2(jcr, M_WARNING, 0, "Unable to set file modes %s: ERR=%s\n",
	    ofile, strerror(errno));
	 stat = 0;
      }

      /* FreeBSD user flags */
#ifdef HAVE_CHFLAGS
      if (chflags(ofile, statp->st_flags) < 0) {
         Jmsg2(jcr, M_WARNING, 0, "Unable to set file flags %s: ERR=%s\n",
	    ofile, strerror(errno));
	 stat = 0;
      }
#endif
      /*
       * Reset file times.
       */
      if (utime(ofile, &ut) < 0) {
         Jmsg2(jcr, M_ERROR, 0, "Unable to set file times %s: ERR=%s\n",
	    ofile, strerror(errno));
	 stat = 0;
      }
   }
   umask(old_mask);
   return stat;
}


/*=============================================================*/
/*							       */
/*		   * * *  U n i x * * * *		       */
/*							       */
/*=============================================================*/

#ifndef HAVE_CYGWIN
    
/*
 * If you have a Unix system with extended attributes (e.g.
 *  ACLs for Solaris, do it here.
 */
int encode_attribsEx(void *jcr, char *attribsEx, FF_PKT *ff_pkt)
{
   *attribsEx = 0;		      /* no extended attributes */
   return STREAM_UNIX_ATTRIBUTES;
}

void SetServicePrivileges(void *jcr)
 { }


#endif



/*=============================================================*/
/*							       */
/*		   * * *  W i n 3 2 * * * *		       */
/*							       */
/*=============================================================*/

#ifdef HAVE_CYGWIN

int NoGetFileAttributesEx = 0;

int encode_attribsEx(void *jcr, char *attribsEx, FF_PKT *ff_pkt)
{
   char *p = attribsEx;
   WIN32_FILE_ATTRIBUTE_DATA atts;
   ULARGE_INTEGER li;

   attribsEx[0] = 0;		      /* no extended attributes */

   if (NoGetFileAttributesEx) {
      return STREAM_UNIX_ATTRIBUTES;
   }

   unix_name_to_win32(&ff_pkt->sys_fname, ff_pkt->fname);
   if (!GetFileAttributesEx(ff_pkt->sys_fname, GetFileExInfoStandard,
			    (LPVOID)&atts)) {
      win_error(jcr, "GetFileAttributesEx:", ff_pkt->sys_fname);
      return STREAM_WIN32_ATTRIBUTES;
   }

   p += to_base64((uint64_t)atts.dwFileAttributes, p);
   *p++ = ' ';                        /* separate fields with a space */
   li.LowPart = atts.ftCreationTime.dwLowDateTime;
   li.HighPart = atts.ftCreationTime.dwHighDateTime;
   p += to_base64((uint64_t)li.QuadPart, p);
   *p++ = ' ';
   li.LowPart = atts.ftLastAccessTime.dwLowDateTime;
   li.HighPart = atts.ftLastAccessTime.dwHighDateTime;
   p += to_base64((uint64_t)li.QuadPart, p);
   *p++ = ' ';
   li.LowPart = atts.ftLastWriteTime.dwLowDateTime;
   li.HighPart = atts.ftLastWriteTime.dwHighDateTime;
   p += to_base64((uint64_t)li.QuadPart, p);
   *p++ = ' ';
   p += to_base64((uint64_t)atts.nFileSizeHigh, p);
   *p++ = ' ';
   p += to_base64((uint64_t)atts.nFileSizeLow, p);
   *p = 0;
   return STREAM_WIN32_ATTRIBUTES;
}

/* Define attributes that are legal to set with SetFileAttributes() */
#define SET_ATTRS ( \
         FILE_ATTRIBUTE_ARCHIVE| \
         FILE_ATTRIBUTE_HIDDEN| \
         FILE_ATTRIBUTE_NORMAL| \
         FILE_ATTRIBUTE_NOT_CONTENT_INDEXED| \
         FILE_ATTRIBUTE_OFFLINE| \
         FILE_ATTRIBUTE_READONLY| \
         FILE_ATTRIBUTE_SYSTEM| \
	 FILE_ATTRIBUTE_TEMPORARY)


/*
 * Set Extended File Attributes for Win32
 *
 *  fname is the original filename  
 *  ofile is the output filename (may be in a different directory)
 *
 * Returns:  1 on success
 *	     0 on failure
 */
static
int set_win32_attributes(void *jcr, char *fname, char *ofile, char *lname, 
			 int type, int stream, struct stat *statp,
			 char *attribsEx, BFILE *ofd)
{
   char *p = attribsEx;
   int64_t val;
   WIN32_FILE_ATTRIBUTE_DATA atts;
   ULARGE_INTEGER li;
   int stat;
   POOLMEM *win32_ofile;

   if (!p || !*p) {		      /* we should have attributes */
      Dmsg2(100, "Attributes missing. of=%s ofd=%d\n", ofile, ofd->fid);
      if (is_bopen(ofd)) {
	 bclose(ofd);
      }
      return 0;
   } else {
      Dmsg2(100, "Attribs %s = %s\n", ofile, attribsEx);
   }

   p += from_base64(&val, p);
   atts.dwFileAttributes = val;
   p++; 			      /* skip space */
   p += from_base64(&val, p);
   li.QuadPart = val;
   atts.ftCreationTime.dwLowDateTime = li.LowPart;
   atts.ftCreationTime.dwHighDateTime = li.HighPart;
   p++; 			      /* skip space */
   p += from_base64(&val, p);
   li.QuadPart = val;
   atts.ftLastAccessTime.dwLowDateTime = li.LowPart;
   atts.ftLastAccessTime.dwHighDateTime = li.HighPart;
   p++; 			      /* skip space */
   p += from_base64(&val, p);
   li.QuadPart = val;
   atts.ftLastWriteTime.dwLowDateTime = li.LowPart;
   atts.ftLastWriteTime.dwHighDateTime = li.HighPart;
   p++;   
   p += from_base64(&val, p);
   atts.nFileSizeHigh = val;
   p++;
   p += from_base64(&val, p);
   atts.nFileSizeLow = val;

   /* At this point, we have reconstructed the WIN32_FILE_ATTRIBUTE_DATA pkt */

   /* Convert to Windows path format */
   win32_ofile = get_pool_memory(PM_FNAME);
   unix_name_to_win32(&win32_ofile, ofile);

   if (!is_bopen(ofd)) {
      Dmsg1(100, "File not open: %s\n", ofile);
      bopen(ofd, ofile, O_WRONLY|O_BINARY, 0);	 /* attempt to open the file */
   }

   if (is_bopen(ofd)) {
      Dmsg1(100, "SetFileTime %s\n", ofile);
      stat = SetFileTime(bget_handle(ofd),
			 &atts.ftCreationTime,
			 &atts.ftLastAccessTime,
			 &atts.ftLastWriteTime);
      if (stat != 1) {
         win_error(jcr, "SetFileTime:", win32_ofile);
      }
      bclose(ofd);
   }

   Dmsg1(100, "SetFileAtts %s\n", ofile);
   stat = SetFileAttributes(win32_ofile, atts.dwFileAttributes & SET_ATTRS);
   if (stat != 1) {
      win_error(jcr, "SetFileAttributes:", win32_ofile);
   }
   free_pool_memory(win32_ofile);
   return 1;
}

void win_error(void *vjcr, char *prefix, POOLMEM *win32_ofile)
{
   JCR *jcr = (JCR *)vjcr; 
   DWORD lerror = GetLastError();
   LPTSTR msg;
   FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|
		 FORMAT_MESSAGE_FROM_SYSTEM,
		 NULL,
		 lerror,
		 0,
		 (LPTSTR)&msg,
		 0,
		 NULL);
   Dmsg3(100, "Error in %s on file %s: ERR=%s\n", prefix, win32_ofile, msg);
   strip_trailing_junk(msg);
   Jmsg3(jcr, M_INFO, 0, _("Error in %s file %s: ERR=%s\n"), prefix, win32_ofile, msg);
   LocalFree(msg);
}

void win_error(void *vjcr, char *prefix, DWORD lerror)
{
   JCR *jcr = (JCR *)vjcr; 
   LPTSTR msg;
   FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|
		 FORMAT_MESSAGE_FROM_SYSTEM,
		 NULL,
		 lerror,
		 0,
		 (LPTSTR)&msg,
		 0,
		 NULL);
   strip_trailing_junk(msg);
   Jmsg2(jcr, M_INFO, 0, _("Error in %s: ERR=%s\n"), prefix, msg);
   LocalFree(msg);
}


/* Cygwin API definition */
extern "C" void cygwin_conv_to_win32_path(const char *path, char *win32_path);

void unix_name_to_win32(POOLMEM **win32_name, char *name)
{
   /* One extra byte should suffice, but we double it */
   *win32_name = check_pool_memory_size(*win32_name, 2*strlen(name)+1);
   cygwin_conv_to_win32_path(name, *win32_name);
}

/*
 * Setup privileges we think we will need.  We probably do not need
 *  the SE_SECURITY_NAME, but since nothing seems to be working,
 *  we get it hoping to fix the problems.
 */
void SetServicePrivileges(void *jcr)
{
    HANDLE hToken;
    TOKEN_PRIVILEGES tkp, tkpPrevious;
    DWORD cbPrevious = sizeof(TOKEN_PRIVILEGES);
    DWORD lerror;
    // Get a token for this process. 
    if (!OpenProcessToken(GetCurrentProcess(), 
	    TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
       win_error(jcr, "OpenProcessToken", GetLastError());
       /* Forge on anyway */
    } 

#ifdef xxx
    // Get the LUID for the security privilege. 
    if (!LookupPrivilegeValue(NULL, SE_SECURITY_NAME,  &tkp.Privileges[0].Luid)) {
       win_error(jcr, "LookupPrivilegeValue", GetLastError());
    }

    tkp.PrivilegeCount = 1;
    tkp.Privileges[0].Attributes = 0;
    /* Get the privilege */
    AdjustTokenPrivileges(hToken, FALSE, &tkp, sizeof(TOKEN_PRIVILEGES),
			    &tkpPrevious, &cbPrevious);
    lerror = GetLastError();
    if (lerror != ERROR_SUCCESS) {
       win_error(jcr, "AdjustTokenPrivileges get SECURITY_NAME", lerror);
    } 

    tkpPrevious.PrivilegeCount = 1;
    tkpPrevious.Privileges[0].Attributes |= (SE_PRIVILEGE_ENABLED); 
       
    /* Set the security privilege for this process. */
    AdjustTokenPrivileges(hToken, FALSE, &tkpPrevious, sizeof(TOKEN_PRIVILEGES),
			    (PTOKEN_PRIVILEGES)NULL, (PDWORD)NULL);
    lerror = GetLastError();
    if (lerror != ERROR_SUCCESS) {
       win_error(jcr, "AdjustTokenPrivileges set SECURITY_NAME", lerror);
    } 
#endif

    // Get the LUID for the backup privilege. 
    if (!LookupPrivilegeValue(NULL, SE_BACKUP_NAME,  &tkp.Privileges[0].Luid)) {
       win_error(jcr, "LookupPrivilegeValue", GetLastError());
    }

    tkp.PrivilegeCount = 1;
    tkp.Privileges[0].Attributes = 0;
    /* Get the current privilege */
    AdjustTokenPrivileges(hToken, FALSE, &tkp, sizeof(TOKEN_PRIVILEGES),
			    &tkpPrevious, &cbPrevious);
    lerror = GetLastError();
    if (lerror != ERROR_SUCCESS) {
       win_error(jcr, "AdjustTokenPrivileges get BACKUP_NAME", lerror);
    } 

    tkpPrevious.PrivilegeCount = 1;
    tkpPrevious.Privileges[0].Attributes |= (SE_PRIVILEGE_ENABLED); 
       
    /* Set the backup privilege for this process. */
    AdjustTokenPrivileges(hToken, FALSE, &tkpPrevious, sizeof(TOKEN_PRIVILEGES),
			    (PTOKEN_PRIVILEGES)NULL, (PDWORD)NULL);  
    lerror = GetLastError();
    if (lerror != ERROR_SUCCESS) {
       win_error(jcr, "AdjustTokenPrivileges set BACKUP_NAME", lerror);
    } 
     
    // Get the LUID for the restore privilege. 
    if (!LookupPrivilegeValue(NULL, SE_RESTORE_NAME, &tkp.Privileges[0].Luid)) {
       win_error(jcr, "LookupPrivilegeValue", GetLastError());
    }

    tkp.PrivilegeCount = 1;
    tkp.Privileges[0].Attributes = 0;
    /* Get the privilege */
    AdjustTokenPrivileges(hToken, FALSE, &tkp, sizeof(TOKEN_PRIVILEGES),
			    &tkpPrevious, &cbPrevious);
    lerror = GetLastError();
    if (lerror != ERROR_SUCCESS) {
       win_error(jcr, "AdjustTokenPrivileges get RESTORE_NAME", lerror);
    } 

    tkpPrevious.PrivilegeCount = 1;
    tkpPrevious.Privileges[0].Attributes |= (SE_PRIVILEGE_ENABLED); 
       
    /* Set the security privilege for this process. */
    AdjustTokenPrivileges(hToken, FALSE, &tkpPrevious, sizeof(TOKEN_PRIVILEGES),
			    (PTOKEN_PRIVILEGES)NULL, (PDWORD)NULL);
    lerror = GetLastError();
    if (lerror != ERROR_SUCCESS) {
       win_error(jcr, "AdjustTokenPrivileges set RESTORE_NAME", lerror);
    } 
    CloseHandle(hToken);
}

//     MessageBox(NULL, "Get restore priv failed: AdjustTokePrivileges", "restore", MB_OK);

#endif	/* HAVE_CYGWIN */
