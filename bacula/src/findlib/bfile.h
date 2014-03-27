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
 *     Kern Sibbald May MMIII
 */

#ifndef __BFILE_H
#define __BFILE_H


/* this should physically correspond to WIN32_STREAM_ID
 * from winbase.h on Win32. We didn't inlcude cStreamName
 * as we don't use it and don't need it for a correct struct size.
 */

#define WIN32_BACKUP_DATA 1

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

/*  =======================================================
 *
 *   W I N D O W S
 *
 *  =======================================================
 */
#if defined(HAVE_WIN32)

enum {
   BF_CLOSED,
   BF_READ,                           /* BackupRead */
   BF_WRITE                           /* BackupWrite */
};

/* In bfile.c */

/* Basic Win32 low level I/O file packet */
struct BFILE {
   bool use_backup_api;               /* set if using BackupRead/Write */
   int mode;                          /* set if file is open */
   HANDLE fh;                         /* Win32 file handle */
   int fid;                           /* fd if doing Unix style */
   LPVOID lpContext;                  /* BackupRead/Write context */
   POOLMEM *errmsg;                   /* error message buffer */
   DWORD rw_bytes;                    /* Bytes read or written */
   DWORD lerror;                      /* Last error code */
   int berrno;                        /* errno */
   int block;                         /* Count of read/writes */
   uint64_t total_bytes;              /* bytes written */
   boffset_t offset;                  /* Delta offset */
   JCR *jcr;                          /* jcr for editing job codes */
   PROCESS_WIN32_BACKUPAPIBLOCK_CONTEXT win32DecompContext; /* context for decomposition of win32 backup streams */
   int use_backup_decomp;             /* set if using BackupRead Stream Decomposition */
   bool reparse_point;                /* set if reparse point */
   bool cmd_plugin;                   /* set if we have a command plugin */
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
   int berrno;                        /* errno */
   int32_t lerror;                    /* not used - simplies Win32 builds */
   int block;                         /* Count of read/writes */
   uint64_t m_flags;                  /* open flags */
   uint64_t total_bytes;              /* bytes written */
   boffset_t offset;                  /* Delta offset */
   JCR *jcr;                          /* jcr for editing job codes */
   PROCESS_WIN32_BACKUPAPIBLOCK_CONTEXT win32DecompContext; /* context for decomposition of win32 backup streams */
   int use_backup_decomp;             /* set if using BackupRead Stream Decomposition */
   bool reparse_point;                /* not used in Unix */
   bool cmd_plugin;                   /* set if we have a command plugin */
};

#endif

void    binit(BFILE *bfd);
bool    is_bopen(BFILE *bfd);
bool    set_win32_backup(BFILE *bfd);
bool    set_portable_backup(BFILE *bfd);
bool    set_cmd_plugin(BFILE *bfd, JCR *jcr);
bool    have_win32_api();
bool    is_portable_backup(BFILE *bfd);
bool    is_restore_stream_supported(int stream);
bool    is_win32_stream(int stream);
int     bopen(BFILE *bfd, const char *fname, uint64_t flags, mode_t mode);
int     bopen_rsrc(BFILE *bfd, const char *fname, uint64_t flags, mode_t mode);
int     bclose(BFILE *bfd);
ssize_t bread(BFILE *bfd, void *buf, size_t count);
ssize_t bwrite(BFILE *bfd, void *buf, size_t count);
boffset_t blseek(BFILE *bfd, boffset_t offset, int whence);
const char   *stream_to_ascii(int stream);

bool processWin32BackupAPIBlock (BFILE *bfd, void *pBuffer, ssize_t dwSize);

#endif /* __BFILE_H */
