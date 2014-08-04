/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2003-2014 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from many
   others, a complete list can be found in the file AUTHORS.

   You may use this file and others of this release according to the
   license defined in the LICENSE file, which includes the Affero General
   Public License, v3.0 ("AGPLv3") and some additional permissions and
   terms pursuant to its AGPLv3 Section 7.

   Bacula® is a registered trademark of Kern Sibbald.
*/
/*
 *  Bacula low level File I/O routines.  This routine simulates
 *    open(), read(), write(), and close(), but using native routines.
 *    I.e. on Windows, we use Windows APIs.
 *
 *    Kern Sibbald, April MMIII
 *
 */

#include "bacula.h"
#include "find.h"

const int dbglvl = 200;

int       (*plugin_bopen)(BFILE *bfd, const char *fname, uint64_t flags, mode_t mode) = NULL;
int       (*plugin_bclose)(BFILE *bfd) = NULL;
ssize_t   (*plugin_bread)(BFILE *bfd, void *buf, size_t count) = NULL;
ssize_t   (*plugin_bwrite)(BFILE *bfd, void *buf, size_t count) = NULL;
boffset_t (*plugin_blseek)(BFILE *bfd, boffset_t offset, int whence) = NULL;


#ifdef HAVE_DARWIN_OS
#include <sys/paths.h>
#endif

#if !defined(HAVE_FDATASYNC)
#define fdatasync(fd)
#endif

#ifdef HAVE_WIN32
void pause_msg(const char *file, const char *func, int line, const char *msg)
{
   char buf[1000];
   if (msg) {
      bsnprintf(buf, sizeof(buf), "%s:%s:%d %s", file, func, line, msg);
   } else {
      bsnprintf(buf, sizeof(buf), "%s:%s:%d", file, func, line);
   }
   MessageBox(NULL, buf, "Pause", MB_OK);
}

/*
 * The following code was contributed by Graham Keeling from his
 *  burp project.  August 2014
 */
static int encrypt_bopen(BFILE *bfd, const char *fname, uint64_t flags, mode_t mode)
{
   ULONG ulFlags = 0;
   POOLMEM *win32_fname;
   POOLMEM *win32_fname_wchar;

   bfd->mode = BF_CLOSED;
   bfd->fid = -1;

   if (!(p_OpenEncryptedFileRawA || p_OpenEncryptedFileRawW)) {
      Dmsg0(50, "No OpenEncryptedFileRawA and no OpenEncryptedFileRawW APIs!!!\n");
      return -1;
   }

   /* Convert to Windows path format */
   win32_fname = get_pool_memory(PM_FNAME);
   win32_fname_wchar = get_pool_memory(PM_FNAME);

   unix_name_to_win32(&win32_fname, (char *)fname);

   if (p_CreateFileW && p_MultiByteToWideChar) {
      make_win32_path_UTF8_2_wchar(&win32_fname_wchar, fname);
   }

   if ((flags & O_CREAT) || (flags & O_WRONLY)) {
      ulFlags = CREATE_FOR_IMPORT | OVERWRITE_HIDDEN;
      if (bfd->fattrs & FILE_ATTRIBUTE_DIRECTORY) {
         mkdir(fname, 0777);
         ulFlags |= CREATE_FOR_DIR;
      }
      bfd->mode = BF_WRITE;
   } else {
      /* Open existing for read */
      ulFlags = CREATE_FOR_EXPORT;
      bfd->mode = BF_READ;
   }

   if (p_OpenEncryptedFileRawW && p_MultiByteToWideChar) {
      /* unicode open */
      if (p_OpenEncryptedFileRawW((LPCWSTR)win32_fname_wchar,
                   ulFlags, &(bfd->pvContext))) {
         bfd->mode = BF_CLOSED;
      }
   } else {
      /* ascii open */
      if (p_OpenEncryptedFileRawA(win32_fname, ulFlags, &(bfd->pvContext))) {
         bfd->mode = BF_CLOSED;
      }
   }
   free_pool_memory(win32_fname_wchar);
   free_pool_memory(win32_fname);
   bfd->fid = (bfd->mode == BF_CLOSED) ? -1 : 0;
   return bfd->mode==BF_CLOSED ? -1: 1;
}

static int encrypt_bclose(BFILE *bfd)
{
   if (p_CloseEncryptedFileRaw) {
      p_CloseEncryptedFileRaw(bfd->pvContext);
   }
   bfd->pvContext = NULL;
   bfd->mode = BF_CLOSED;
   bfd->fattrs = 0;
   bfd->fid = -1;
   return 0;
}

#endif

/* ===============================================================
 *
 *            U N I X   AND   W I N D O W S
 *
 * ===============================================================
 */

bool is_win32_stream(int stream)
{
   switch (stream) {
   case STREAM_WIN32_DATA:
   case STREAM_WIN32_GZIP_DATA:
   case STREAM_WIN32_COMPRESSED_DATA:
   case STREAM_ENCRYPTED_WIN32_DATA:
   case STREAM_ENCRYPTED_WIN32_GZIP_DATA:
   case STREAM_ENCRYPTED_WIN32_COMPRESSED_DATA:
      return true;
   }
   return false;
}

const char *stream_to_ascii(int stream)
{
   static char buf[20];

   switch (stream & STREAMMASK_TYPE) {
   case STREAM_UNIX_ATTRIBUTES:
      return _("Unix attributes");
   case STREAM_FILE_DATA:
      return _("File data");
   case STREAM_MD5_DIGEST:
      return _("MD5 digest");
   case STREAM_GZIP_DATA:
      return _("GZIP data");
   case STREAM_COMPRESSED_DATA:
      return _("Compressed data");
   case STREAM_UNIX_ATTRIBUTES_EX:
      return _("Extended attributes");
   case STREAM_SPARSE_DATA:
      return _("Sparse data");
   case STREAM_SPARSE_GZIP_DATA:
      return _("GZIP sparse data");
   case STREAM_SPARSE_COMPRESSED_DATA:
      return _("Compressed sparse data");
   case STREAM_PROGRAM_NAMES:
      return _("Program names");
   case STREAM_PROGRAM_DATA:
      return _("Program data");
   case STREAM_SHA1_DIGEST:
      return _("SHA1 digest");
   case STREAM_WIN32_DATA:
      return _("Win32 data");
   case STREAM_WIN32_GZIP_DATA:
      return _("Win32 GZIP data");
   case STREAM_WIN32_COMPRESSED_DATA:
      return _("Win32 compressed data");
   case STREAM_MACOS_FORK_DATA:
      return _("MacOS Fork data");
   case STREAM_HFSPLUS_ATTRIBUTES:
      return _("HFS+ attribs");
   case STREAM_UNIX_ACCESS_ACL:
      return _("Standard Unix ACL attribs");
   case STREAM_UNIX_DEFAULT_ACL:
      return _("Default Unix ACL attribs");
   case STREAM_SHA256_DIGEST:
      return _("SHA256 digest");
   case STREAM_SHA512_DIGEST:
      return _("SHA512 digest");
   case STREAM_SIGNED_DIGEST:
      return _("Signed digest");
   case STREAM_ENCRYPTED_FILE_DATA:
      return _("Encrypted File data");
   case STREAM_ENCRYPTED_WIN32_DATA:
      return _("Encrypted Win32 data");
   case STREAM_ENCRYPTED_SESSION_DATA:
      return _("Encrypted session data");
   case STREAM_ENCRYPTED_FILE_GZIP_DATA:
      return _("Encrypted GZIP data");
   case STREAM_ENCRYPTED_FILE_COMPRESSED_DATA:
      return _("Encrypted compressed data");
   case STREAM_ENCRYPTED_WIN32_GZIP_DATA:
      return _("Encrypted Win32 GZIP data");
   case STREAM_ENCRYPTED_WIN32_COMPRESSED_DATA:
      return _("Encrypted Win32 Compressed data");
   case STREAM_ENCRYPTED_MACOS_FORK_DATA:
      return _("Encrypted MacOS fork data");
   case STREAM_PLUGIN_NAME:
      return _("Plugin Name");
   case STREAM_PLUGIN_DATA:
      return _("Plugin Data");
   case STREAM_RESTORE_OBJECT:
      return _("Restore Object");
   case STREAM_ACL_AIX_TEXT:
      return _("AIX Specific ACL attribs");
   case STREAM_ACL_DARWIN_ACCESS_ACL:
      return _("Darwin Specific ACL attribs");
   case STREAM_ACL_FREEBSD_DEFAULT_ACL:
      return _("FreeBSD Specific Default ACL attribs");
   case STREAM_ACL_FREEBSD_ACCESS_ACL:
      return _("FreeBSD Specific Access ACL attribs");
   case STREAM_ACL_HPUX_ACL_ENTRY:
      return _("HPUX Specific ACL attribs");
   case STREAM_ACL_IRIX_DEFAULT_ACL:
      return _("Irix Specific Default ACL attribs");
   case STREAM_ACL_IRIX_ACCESS_ACL:
      return _("Irix Specific Access ACL attribs");
   case STREAM_ACL_LINUX_DEFAULT_ACL:
      return _("Linux Specific Default ACL attribs");
   case STREAM_ACL_LINUX_ACCESS_ACL:
      return _("Linux Specific Access ACL attribs");
   case STREAM_ACL_TRU64_DEFAULT_ACL:
      return _("TRU64 Specific Default ACL attribs");
   case STREAM_ACL_TRU64_ACCESS_ACL:
      return _("TRU64 Specific Access ACL attribs");
   case STREAM_ACL_SOLARIS_ACLENT:
      return _("Solaris Specific POSIX ACL attribs");
   case STREAM_ACL_SOLARIS_ACE:
      return _("Solaris Specific NFSv4/ZFS ACL attribs");
   case STREAM_ACL_AFS_TEXT:
      return _("AFS Specific ACL attribs");
   case STREAM_ACL_AIX_AIXC:
      return _("AIX Specific POSIX ACL attribs");
   case STREAM_ACL_AIX_NFS4:
      return _("AIX Specific NFSv4 ACL attribs");
   case STREAM_ACL_FREEBSD_NFS4_ACL:
      return _("FreeBSD Specific NFSv4/ZFS ACL attribs");
   case STREAM_ACL_HURD_DEFAULT_ACL:
      return _("GNU Hurd Specific Default ACL attribs");
   case STREAM_ACL_HURD_ACCESS_ACL:
      return _("GNU Hurd Specific Access ACL attribs");
   case STREAM_XATTR_HURD:
      return _("GNU Hurd Specific Extended attribs");
   case STREAM_XATTR_IRIX:
      return _("IRIX Specific Extended attribs");
   case STREAM_XATTR_TRU64:
      return _("TRU64 Specific Extended attribs");
   case STREAM_XATTR_AIX:
      return _("AIX Specific Extended attribs");
   case STREAM_XATTR_OPENBSD:
      return _("OpenBSD Specific Extended attribs");
   case STREAM_XATTR_SOLARIS_SYS:
      return _("Solaris Specific Extensible attribs or System Extended attribs");
   case STREAM_XATTR_SOLARIS:
      return _("Solaris Specific Extended attribs");
   case STREAM_XATTR_DARWIN:
      return _("Darwin Specific Extended attribs");
   case STREAM_XATTR_FREEBSD:
      return _("FreeBSD Specific Extended attribs");
   case STREAM_XATTR_LINUX:
      return _("Linux Specific Extended attribs");
   case STREAM_XATTR_NETBSD:
      return _("NetBSD Specific Extended attribs");
   default:
      sprintf(buf, "%d", stream);
      return (const char *)buf;
   }
}

/**
 *  Convert a 64 bit little endian to a big endian
 */
void int64_LE2BE(int64_t* pBE, const int64_t v)
{
   /* convert little endian to big endian */
   if (htonl(1) != 1L) { /* no work if on little endian machine */
      memcpy(pBE, &v, sizeof(int64_t));
   } else {
      int i;
      uint8_t rv[sizeof(int64_t)];
      uint8_t *pv = (uint8_t *) &v;

      for (i = 0; i < 8; i++) {
         rv[i] = pv[7 - i];
      }
      memcpy(pBE, &rv, sizeof(int64_t));
   }
}

/**
 *  Convert a 32 bit little endian to a big endian
 */
void int32_LE2BE(int32_t* pBE, const int32_t v)
{
   /* convert little endian to big endian */
   if (htonl(1) != 1L) { /* no work if on little endian machine */
      memcpy(pBE, &v, sizeof(int32_t));
   } else {
      int i;
      uint8_t rv[sizeof(int32_t)];
      uint8_t *pv = (uint8_t *) &v;

      for (i = 0; i < 4; i++) {
         rv[i] = pv[3 - i];
      }
      memcpy(pBE, &rv, sizeof(int32_t));
   }
}


/*
 *  Read a BackupRead block and pull out the file data
 */
bool processWin32BackupAPIBlock (BFILE *bfd, void *pBuffer, ssize_t dwSize)
{
   /* pByte contains the buffer
      dwSize the len to be processed.  function assumes to be
      called in successive incremental order over the complete
      BackupRead stream beginning at pos 0 and ending at the end.
    */

   PROCESS_WIN32_BACKUPAPIBLOCK_CONTEXT* pContext = &(bfd->win32DecompContext);
   bool bContinue = false;
   int64_t dwDataOffset = 0;
   int64_t dwDataLen;

   /* Win32 Stream Header size without name of stream.
    * = sizeof (WIN32_STREAM_ID)- sizeof(WCHAR*);
    */
   int32_t dwSizeHeader = 20;

   do {
      if (pContext->liNextHeader >= dwSize) {
         dwDataLen = dwSize-dwDataOffset;
         bContinue = false; /* 1 iteration is enough */
      } else {
         dwDataLen = pContext->liNextHeader-dwDataOffset;
         bContinue = true; /* multiple iterations may be necessary */
      }

      /* flush */
      /* copy block of real DATA */
      if (pContext->bIsInData) {
         if (bwrite(bfd, ((char *)pBuffer)+dwDataOffset, dwDataLen) != (ssize_t)dwDataLen)
            return false;
      }

      if (pContext->liNextHeader < dwSize) {/* is a header in this block ? */
         int32_t dwOffsetTarget;
         int32_t dwOffsetSource;

         if (pContext->liNextHeader < 0) {
            /* start of header was before this block, so we
             * continue with the part in the current block
             */
            dwOffsetTarget = -pContext->liNextHeader;
            dwOffsetSource = 0;
         } else {
            /* start of header is inside of this block */
            dwOffsetTarget = 0;
            dwOffsetSource = pContext->liNextHeader;
         }

         int32_t dwHeaderPartLen = dwSizeHeader-dwOffsetTarget;
         bool bHeaderIsComplete;

         if (dwHeaderPartLen <= dwSize-dwOffsetSource) {
            /* header (or rest of header) is completely available
               in current block
             */
            bHeaderIsComplete = true;
         } else {
            /* header will continue in next block */
            bHeaderIsComplete = false;
            dwHeaderPartLen = dwSize-dwOffsetSource;
         }

         /* copy the available portion of header to persistent copy */
         memcpy(((char *)&pContext->header_stream)+dwOffsetTarget, ((char *)pBuffer)+dwOffsetSource, dwHeaderPartLen);

         /* recalculate position of next header */
         if (bHeaderIsComplete) {
            /* convert stream name size (32 bit little endian) to machine type */
            int32_t dwNameSize;
            int32_LE2BE (&dwNameSize, pContext->header_stream.dwStreamNameSize);
            dwDataOffset = dwNameSize+pContext->liNextHeader+dwSizeHeader;

            /* convert stream size (64 bit little endian) to machine type */
            int64_LE2BE (&(pContext->liNextHeader), pContext->header_stream.Size);
            pContext->liNextHeader += dwDataOffset;

            pContext->bIsInData = pContext->header_stream.dwStreamId == WIN32_BACKUP_DATA;
            if (dwDataOffset == dwSize)
               bContinue = false;
         } else {
            /* stop and continue with next block */
            bContinue = false;
            pContext->bIsInData = false;
         }
      }
   } while (bContinue);

   /* set "NextHeader" relative to the beginning of the next block */
   pContext->liNextHeader-= dwSize;

   return TRUE;
}




/* ===============================================================
 *
 *            U N I X
 *
 * ===============================================================
 */
/* Unix */
void binit(BFILE *bfd)
{
   memset(bfd, 0, sizeof(BFILE));
   bfd->fid = -1;
}

/* Unix */
bool have_win32_api()
{
   return false;                       /* no can do */
}

/*
 * Enables using the Backup API (win32_data).
 *   Returns true  if function worked
 *   Returns false if failed (i.e. do not have Backup API on this machine)
 */
/* Unix */
bool set_win32_backup(BFILE *bfd)
{
   return false;                       /* no can do */
}


/* Unix */
bool set_portable_backup(BFILE *bfd)
{
   return true;                        /* no problem */
}

/*
 * Return true  if we are writing in portable format
 * return false if not
 */
/* Unix */
bool is_portable_backup(BFILE *bfd)
{
   return true;                       /* portable by definition */
}

/* Unix */
bool set_prog(BFILE *bfd, char *prog, JCR *jcr)
{
   return false;
}

/* Unix */
bool set_cmd_plugin(BFILE *bfd, JCR *jcr)
{
   bfd->cmd_plugin = true;
   bfd->jcr = jcr;
   return true;
}

/*
 * This code is running on a non-Win32 machine
 */
/* Unix */
bool is_restore_stream_supported(int stream)
{
   /* No Win32 backup on this machine */
     switch (stream) {
#ifndef HAVE_LIBZ
   case STREAM_GZIP_DATA:
   case STREAM_SPARSE_GZIP_DATA:
   case STREAM_WIN32_GZIP_DATA:
#endif
#ifndef HAVE_LZO
   case STREAM_COMPRESSED_DATA:
   case STREAM_SPARSE_COMPRESSED_DATA:
   case STREAM_WIN32_COMPRESSED_DATA:
   case STREAM_ENCRYPTED_FILE_COMPRESSED_DATA:
   case STREAM_ENCRYPTED_WIN32_COMPRESSED_DATA:
#endif
#ifndef HAVE_DARWIN_OS
   case STREAM_MACOS_FORK_DATA:
   case STREAM_HFSPLUS_ATTRIBUTES:
#endif
      return false;

   /* Known streams */
#ifdef HAVE_LIBZ
   case STREAM_GZIP_DATA:
   case STREAM_SPARSE_GZIP_DATA:
   case STREAM_WIN32_GZIP_DATA:
#endif
#ifdef HAVE_LZO
   case STREAM_COMPRESSED_DATA:
   case STREAM_SPARSE_COMPRESSED_DATA:
   case STREAM_WIN32_COMPRESSED_DATA:
   case STREAM_ENCRYPTED_FILE_COMPRESSED_DATA:
   case STREAM_ENCRYPTED_WIN32_COMPRESSED_DATA:
#endif
   case STREAM_WIN32_DATA:
   case STREAM_UNIX_ATTRIBUTES:
   case STREAM_FILE_DATA:
   case STREAM_MD5_DIGEST:
   case STREAM_UNIX_ATTRIBUTES_EX:
   case STREAM_SPARSE_DATA:
   case STREAM_PROGRAM_NAMES:
   case STREAM_PROGRAM_DATA:
   case STREAM_SHA1_DIGEST:
#ifdef HAVE_SHA2
   case STREAM_SHA256_DIGEST:
   case STREAM_SHA512_DIGEST:
#endif
#ifdef HAVE_CRYPTO
   case STREAM_SIGNED_DIGEST:
   case STREAM_ENCRYPTED_FILE_DATA:
   case STREAM_ENCRYPTED_FILE_GZIP_DATA:
   case STREAM_ENCRYPTED_WIN32_DATA:
   case STREAM_ENCRYPTED_WIN32_GZIP_DATA:
#endif
#ifdef HAVE_DARWIN_OS
   case STREAM_MACOS_FORK_DATA:
   case STREAM_HFSPLUS_ATTRIBUTES:
#ifdef HAVE_CRYPTO
   case STREAM_ENCRYPTED_MACOS_FORK_DATA:
#endif /* HAVE_CRYPTO */
#endif /* HAVE_DARWIN_OS */
   case 0:   /* compatibility with old tapes */
      return true;

   }
   return false;
}

/* Unix */
int bopen(BFILE *bfd, const char *fname, uint64_t flags, mode_t mode)
{
   if (bfd->cmd_plugin && plugin_bopen) {
      Dmsg1(400, "call plugin_bopen fname=%s\n", fname);
      bfd->fid = plugin_bopen(bfd, fname, flags, mode);
      Dmsg2(400, "Plugin bopen fid=%d file=%s\n", bfd->fid, fname);
      return bfd->fid;
   }

   /* Normal file open */
   Dmsg1(dbglvl, "open file %s\n", fname);

   /* We use fnctl to set O_NOATIME if requested to avoid open error */
   bfd->fid = open(fname, flags & ~O_NOATIME, mode);

   /* Set O_NOATIME if possible */
   if (bfd->fid != -1 && flags & O_NOATIME) {
      int oldflags = fcntl(bfd->fid, F_GETFL, 0);
      if (oldflags == -1) {
         bfd->berrno = errno;
         close(bfd->fid);
         bfd->fid = -1;
      } else {
         int ret = fcntl(bfd->fid, F_SETFL, oldflags | O_NOATIME);
        /* EPERM means setting O_NOATIME was not allowed  */
         if (ret == -1 && errno != EPERM) {
            bfd->berrno = errno;
            close(bfd->fid);
            bfd->fid = -1;
         }
      }
   }
   bfd->berrno = errno;
   bfd->m_flags = flags;
   bfd->block = 0;
   bfd->total_bytes = 0;
   Dmsg1(400, "Open file %d\n", bfd->fid);
   errno = bfd->berrno;

   bfd->win32DecompContext.bIsInData = false;
   bfd->win32DecompContext.liNextHeader = 0;

#if defined(HAVE_POSIX_FADVISE) && defined(POSIX_FADV_WILLNEED)
   if (bfd->fid != -1 && flags & O_RDONLY) {
      int stat = posix_fadvise(bfd->fid, 0, 0, POSIX_FADV_WILLNEED);
      Dmsg2(400, "Did posix_fadvise on %s stat=%d\n", fname, stat);
   }
#endif

   return bfd->fid;
}

#ifdef HAVE_DARWIN_OS
/* Open the resource fork of a file. */
int bopen_rsrc(BFILE *bfd, const char *fname, uint64_t flags, mode_t mode)
{
   POOLMEM *rsrc_fname;

   rsrc_fname = get_pool_memory(PM_FNAME);
   pm_strcpy(rsrc_fname, fname);
   pm_strcat(rsrc_fname, _PATH_RSRCFORKSPEC);
   bopen(bfd, rsrc_fname, flags, mode);
   free_pool_memory(rsrc_fname);
   return bfd->fid;
}
#else
/* Unix */
int bopen_rsrc(BFILE *bfd, const char *fname, uint64_t flags, mode_t mode)
{
   return -1;
}
#endif


/* Unix */
int bclose(BFILE *bfd)
{
   int stat;

   Dmsg1(400, "Close file %d\n", bfd->fid);

   if (bfd->fid == -1) {
      return 0;
   }
   if (bfd->cmd_plugin && plugin_bclose) {
      stat = plugin_bclose(bfd);
      bfd->fid = -1;
      bfd->cmd_plugin = false;
   }

#if defined(HAVE_POSIX_FADVISE) && defined(POSIX_FADV_DONTNEED)
   if (bfd->m_flags & O_RDONLY) {
      fdatasync(bfd->fid);            /* sync the file */
      /* Tell OS we don't need it any more */
      posix_fadvise(bfd->fid, 0, 0, POSIX_FADV_DONTNEED);
   }
#endif

   /* Close normal file */
   stat = close(bfd->fid);
   bfd->berrno = errno;
   bfd->fid = -1;
   bfd->cmd_plugin = false;
   return stat;
}

/* Unix */
ssize_t bread(BFILE *bfd, void *buf, size_t count)
{
   ssize_t stat;

   if (bfd->cmd_plugin && plugin_bread) {
      return plugin_bread(bfd, buf, count);
   }

   stat = read(bfd->fid, buf, count);
   bfd->berrno = errno;
   bfd->block++;
   if (stat > 0) {
      bfd->total_bytes += stat;
   }
   return stat;
}

/* Unix */
ssize_t bwrite(BFILE *bfd, void *buf, size_t count)
{
   ssize_t stat;

   if (bfd->cmd_plugin && plugin_bwrite) {
      return plugin_bwrite(bfd, buf, count);
   }
   stat = write(bfd->fid, buf, count);
   bfd->berrno = errno;
   bfd->block++;
   if (stat > 0) {
      bfd->total_bytes += stat;
   }
   return stat;
}

/* Unix */
bool is_bopen(BFILE *bfd)
{
   return bfd->fid >= 0;
}

/* Unix */
boffset_t blseek(BFILE *bfd, boffset_t offset, int whence)
{
   boffset_t pos;

   if (bfd->cmd_plugin && plugin_bwrite) {
      return plugin_blseek(bfd, offset, whence);
   }
   pos = (boffset_t)lseek(bfd->fid, offset, whence);
   bfd->berrno = errno;
   return pos;
}
