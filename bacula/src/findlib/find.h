/*
 * File types as returned by find_files()
 *
 *     Kern Sibbald MIM
 */
/*
   Copyright (C) 2000, 2001 Kern Sibbald and John Walker

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef __FILES_H
#define __FILES_H

#ifdef HAVE_FNMATCH
#include <fnmatch.h>
#else
#include "lib/fnmatch.h"
#endif


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

/* FileSet options */     
#define FO_MD5      0x1               /* Do MD5 checksum */
#define FO_GZIP     0x2               /* Do Zlib compression */


#define OPT_compute_MD5       0x01    /* compute MD5 of file's data */
#define OPT_GZIP_compression  0x02    /* use GZIP compression */
#define OPT_no_recursion      0x04    /* no recursion in directories */
#define OPT_multifs           0x08    /* multiple file systems */


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
   struct stat statp;                 /* stat packet */
   int type;                          /* FT_ type from above */
   int flags;                         /* control flags */
   int ff_errno;                      /* errno */
   int incremental;                   /* do incremental save */
   time_t save_time;                  /* start of incremental time */
   int no_recursion;                  /* do not recurse into sub directories */
   int mtime_only;                    /* incremental on mtime_only */
   int dereference;                   /* follow links */
   int compute_MD5;                   /* compute MD5 checksum */
   int GZIP_compression;              /* compress the file */
   int GZIP_level;                    /* compression level */
   int one_file_system;               /* do not traverse file systems */
   int atime_preserve;                /* preserve access times */
   int null_output_device;            /* using null output device */
   char VerifyOpts[20];
   struct s_included_file *included_files_list;
   struct s_excluded_file *excluded_files_list;
   struct s_excluded_file *excluded_paths_list;

   struct f_link *linklist;           /* hard linked files */
} FF_PKT;

/* From find.c */
FF_PKT *init_find_files();
void set_find_options(FF_PKT *ff, int incremental, time_t mtime);
int find_files(FF_PKT *ff, int sub(FF_PKT *ff_pkt, void *hpkt), void *pkt);
void term_find_files(FF_PKT *ff);

/* From match.c */
void init_include_exclude_files(FF_PKT *ff);
void term_include_exclude_files(FF_PKT *ff);
void add_fname_to_include_list(FF_PKT *ff, int prefixed, char *fname);
void add_fname_to_exclude_list(FF_PKT *ff, char *fname);
struct s_included_file *get_next_included_file(
    FF_PKT *ff, struct s_included_file *inc);
int file_is_excluded(FF_PKT *ff, char *file);
int file_is_included(FF_PKT *ff, char *file);

#endif /* __FILES_H */
