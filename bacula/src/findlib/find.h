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

#ifdef HAVE_DIRENT_H
#include <dirent.h>
#define NAMELEN(dirent) (strlen((dirent)->d_name))
#endif
#include <sys/file.h>
#include <utime.h>

#define MODE_RALL (S_IRUSR|S_IRGRP|S_IROTH)

#ifdef HAVE_FNMATCH
#include <fnmatch.h>
#else
#include "lib/fnmatch.h"
#endif

#include "save-cwd.h"

/* 
 * Status codes returned by create_file()
 */
#define CF_SKIP       1               /* skip file (not newer or something) */
#define CF_ERROR      2               /* error creating file */
#define CF_EXTRACT    3               /* file created, data to extract */
#define CF_CREATED    4               /* file created, no data to extract */

/* 
 *  NOTE!!! These go on the tape, so don't change them. If 
 *  need be, add to them.
 */
#define FT_LNKSAVED   1               /* hard link to file already saved */  
#define FT_REGE       2               /* Regular file but empty */
#define FT_REG        3               /* Regular file */
#define FT_LNK        4               /* Soft Link */
#define FT_DIR        5               /* Directory */
#define FT_SPEC       6               /* Special file -- chr, blk, fifo, sock */
#define FT_NOACCESS   7               /* Not able to access */
#define FT_NOFOLLOW   8               /* Could not follow link */
#define FT_NOSTAT     9               /* Could not stat file */
#define FT_NOCHG     10               /* Incremental option, file not changed */
#define FT_DIRNOCHG  11               /* Incremental option, directory not changed */
#define FT_ISARCH    12               /* Trying to save archive file */
#define FT_NORECURSE 13               /* No recursion into directory */
#define FT_NOFSCHG   14               /* Different file system, prohibited */
#define FT_NOOPEN    15               /* Could not open directory */
#define FT_RAW       16               /* Raw block device */
#define FT_FIFO      17               /* Raw fifo device */

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
typedef struct ff {
   char *fname;                       /* filename */
   char *link;                        /* link if file linked */
   POOLMEM *sys_fname;                /* system filename */
   struct stat statp;                 /* stat packet */
   uint32_t FileIndex;                /* FileIndex of this file */
   uint32_t LinkFI;                   /* FileIndex of main hard linked file */
   struct f_link *linked;             /* Set if we are hard linked */
   int type;                          /* FT_ type from above */
   int fid;                           /* file id if opened */
   int flags;                         /* control flags */
   int ff_errno;                      /* errno */
   int incremental;                   /* do incremental save */
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
