/*
 * File types as returned by find_files()
 *
 *     Kern Sibbald MIM
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

#ifndef __FILES_H
#define __FILES_H

#include "jcr.h"
#include "bfile.h"

#ifdef HAVE_DIRENT_H
#include <dirent.h>
#define NAMELEN(dirent) (strlen((dirent)->d_name))
#endif

#include <sys/file.h>
#if HAVE_UTIME_H
#include <utime.h>
#else
struct utimbuf {
    long actime;
    long modtime;
};
#endif

#define MODE_RALL (S_IRUSR|S_IRGRP|S_IROTH)

#ifdef HAVE_FNMATCH
#include <fnmatch.h>
#else
#include "lib/fnmatch.h"
#endif

#include "save-cwd.h"

#ifndef HAVE_READDIR_R
int readdir_r(DIR *dirp, struct dirent *entry, struct dirent **result);
#endif


/* 
 * Status codes returned by create_file()
 */
#define CF_SKIP       1               /* skip file (not newer or something) */
#define CF_ERROR      2               /* error creating file */
#define CF_EXTRACT    3               /* file created, data to extract */
#define CF_CREATED    4               /* file created, no data to extract */


/* Options saved in "flag" of ff packet */
#define FO_MD5          0x001         /* Do MD5 checksum */
#define FO_GZIP         0x002         /* Do Zlib compression */
#define FO_NO_RECURSION 0x004         /* no recursion in directories */
#define FO_MULTIFS      0x008         /* multiple file systems */
#define FO_SPARSE       0x010         /* do sparse file checking */
#define FO_IF_NEWER     0x020         /* replace if newer */
#define FO_NOREPLACE    0x040         /* never replace */
#define FO_READFIFO     0x080         /* read data from fifo */
#define FO_SHA1         0x100         /* Do SHA1 checksum */

/*
 * Options saved in "options" of include list
 * These are now directly jammed into ff->flags, so the above
 *   FO_xxx options may be used
 *
 * ***FIXME*** replace all OPT_xxx with FO_xxx or vise-versa 
 */
#define OPT_compute_MD5       0x01    /* compute MD5 of file's data */
#define OPT_GZIP_compression  0x02    /* use GZIP compression */
#define OPT_no_recursion      0x04    /* no recursion in directories */
#define OPT_multifs           0x08    /* multiple file systems */
#define OPT_sparse            0x10    /* do sparse file checking */
#define OPT_replace_if_newer  0x20    /* replace file if newer */
#define OPT_never_replace     0x40    /* never replace */
#define OPT_read_fifo         0x80    /* read data from fifo (named pipe) */
#define OPT_compute_SHA1     0x100    /* compute SHA1 of file's data */


struct s_included_file {
   struct s_included_file *next;
   int options;                       /* backup options */
   int level;                         /* compression level */
   int len;                           /* length of fname */
   int pattern;                       /* set if pattern */
   char VerifyOpts[20];               /* Options for verify */
   char fname[1];
};

struct s_excluded_file {
   struct s_excluded_file *next;
   int len;
   char fname[1];
};



/*
 * Definition of the find_files packet passed as the
 * first argument to the find_files callback subroutine.
 */
typedef struct s_ff {
   char *fname;                       /* filename */
   char *link;                        /* link if file linked */
   POOLMEM *sys_fname;                /* system filename */
   struct stat statp;                 /* stat packet */
   uint32_t FileIndex;                /* FileIndex of this file */
   uint32_t LinkFI;                   /* FileIndex of main hard linked file */
   struct f_link *linked;             /* Set if we are hard linked */
   int type;                          /* FT_ type from above */
   int flags;                         /* control flags */
   int ff_errno;                      /* errno */
   int incremental;                   /* do incremental save */
   BFILE bfd;                         /* Bacula file descriptor */
   time_t save_time;                  /* start of incremental time */
   int mtime_only;                    /* incremental on mtime_only */
   int dereference;                   /* follow links */
   int GZIP_level;                    /* compression level */
   int atime_preserve;                /* preserve access times */
   int null_output_device;            /* using null output device */
   char VerifyOpts[20];
   struct s_included_file *included_files_list;
   struct s_excluded_file *excluded_files_list;
   struct s_excluded_file *excluded_paths_list;

   struct f_link *linklist;           /* hard linked files */
} FF_PKT;


#include "protos.h"

#endif /* __FILES_H */
