/*
 * Main routine for finding files on a file system.
 *  The heart of the work is done in find_one.c
 *
 *  Kern E. Sibbald, MM
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

/* Imported functions */
int find_one_file(FF_PKT *ff, int handle_file(FF_PKT *ff_pkt, void *hpkt), 
	       void *pkt, char *p, dev_t parent_device, int top_level);
void term_find_one(FF_PKT *ff);

size_t name_max;	       /* filename max length */
size_t path_max;	       /* path name max length */


/* ****FIXME**** debug until stable */
#undef bmalloc
#define bmalloc(x) sm_malloc(__FILE__, __LINE__, x)


/* 
 * Initialize the find files "global" variables
 */
FF_PKT *init_find_files()
{
  FF_PKT *ff;	 

  ff = (FF_PKT *) bmalloc(sizeof(FF_PKT));
  memset(ff, 0, sizeof(FF_PKT));

  ff->sys_fname = get_pool_memory(PM_FNAME);

  init_include_exclude_files(ff);	    /* init lists */
  ff->mtime_only = 1;
  ff->one_file_system = 1;

   /* Get system path and filename maximum lengths */
   path_max = pathconf(".", _PC_PATH_MAX);
   if (path_max < 1024) {
      path_max = 1024;
   }

   name_max = pathconf(".", _PC_NAME_MAX);
   if (name_max < 1024) {
      name_max = 1024;
   }
   path_max++;			      /* add for EOS */
   name_max++;			      /* add for EOS */

  Dmsg1(100, "init_find_files ff=%p\n", ff);
  return ff;
}

/* 
 * Set find_files options. For the moment, we only
 * provide for full/incremental saves, and setting
 * of save_time. For additional options, see above
 */
void
set_find_options(FF_PKT *ff, int incremental, time_t save_time)
{
  Dmsg0(100, "Enter set_find_options()\n");
  ff->incremental = incremental;
  ff->save_time = save_time;
  Dmsg0(100, "Leave set_find_options()\n");
}

/* 
 * Find all specified files (determined by calls to 
 * name_add()
 * This routine calls the (handle_file) subroutine with all
 * sorts of good information for the final disposition of
 * the file.
 * 
 * Call this subroutine with a callback subroutine as the first
 * argument and a packet as the second argument, this packet
 * will be passed back to the callback subroutine as the last
 * argument.
 *
 * The callback subroutine gets called with:
 *  arg1 -- the FF_PKT containing filename, link, stat, ftype, flags, etc
 *  arg2 -- the user supplied packet
 *
 */
int
find_files(FF_PKT *ff, int callback(FF_PKT *ff_pkt, void *hpkt), void *his_pkt) 
{
   char *file;
   struct s_included_file *inc = NULL;

   while ((inc = get_next_included_file(ff, inc))) {
      file = inc->fname;
      strcpy(ff->VerifyOpts, inc->VerifyOpts); /* Copy options for this file */
      Dmsg1(50, "find_files: file=%s\n", file);
      if (!file_is_excluded(ff, file)) {
	 if (!find_one_file(ff, callback, his_pkt, file, (dev_t)-1, 1)) {
	    return 0;		       /* error return */
	 }
      }
   }
   return 1;
}

/*
 * Terminate find_files() and release
 * all allocated memory   
 */
void
term_find_files(FF_PKT *ff)
{
  term_include_exclude_files(ff);
  free_pool_memory(ff->sys_fname);
  term_find_one(ff);
  free(ff);
  return;
}
