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

   This file is based on GNU TAR source code. Except for a few key
   ideas and some key snippets of code, it has been rewritten for Bacula.

      Kern Sibbald, MM

   Many thanks to the TAR programmers.

 */

#include "bacula.h"
#include "find.h"
/*#include "system.h" */


extern size_t name_max; 	      /* filename max length */
extern size_t path_max; 	      /* path name max length */

#ifndef HAVE_READDIR_R
int readdir_r(DIR *dirp, struct dirent *entry, struct dirent **result);
#endif


/*
 * Structure for keeping track of hard linked files
 */
struct f_link {
    struct f_link *next;
    dev_t dev;
    ino_t ino;
    short linkcount;
    char name[1];
};


#if HAVE_UTIME_H
# include <utime.h>
#else
struct utimbuf
  {
    long actime;
    long modtime;
  };
#endif


/*
 * Find a single file.			      
 * handle_file is the callback for handling the file.
 * p is the filename
 * parent_device is the device we are currently on 
 * top_level is 1 when not recursing or 0 when 
 *  decending into a directory.
 */
int
find_one_file(FF_PKT *ff_pkt, int handle_file(FF_PKT *ff, void *hpkt), void *pkt, 
	       char *fname, dev_t parent_device, int top_level)
{
   struct utimbuf restore_times;
   int rtn_stat;

   ff_pkt->fname = ff_pkt->link = fname;
   if (ff_pkt->compute_MD5) {
      ff_pkt->flags |= FO_MD5;
   }
   if (ff_pkt->GZIP_compression) {
      ff_pkt->flags |= FO_GZIP;
   }

   if (lstat(fname, &ff_pkt->statp) != 0) {
       /* Cannot stat file */
       ff_pkt->type = FT_NOSTAT;
       ff_pkt->ff_errno = errno;
       return handle_file(ff_pkt, pkt);
   }

   Dmsg1(60, "File ----: %s\n", fname);
#ifdef DEBUG
   if (S_ISLNK(ff_pkt->statp.st_mode))
      Dmsg1(60, "Link-------------: %s \n", fname);
#endif

   /* Save current times of this directory in case we need to
    * reset them because the user doesn't want them changed.
    */
   restore_times.actime = ff_pkt->statp.st_atime;
   restore_times.modtime = ff_pkt->statp.st_mtime;


   /* 
    * If this is an Incremental backup, see if file was modified
    * since our last "save_time", presumably the last Full save
    * or Incremental.
    */
   if (ff_pkt->incremental && !S_ISDIR(ff_pkt->statp.st_mode)) {
      Dmsg1(100, "Non-directory incremental: %s\n", ff_pkt->fname);
      /* Not a directory */
      if (ff_pkt->statp.st_mtime < ff_pkt->save_time
	  && (ff_pkt->mtime_only || 
	      ff_pkt->statp.st_ctime < ff_pkt->save_time)) {
	 /* Incremental option, file not changed */
	 ff_pkt->type = FT_NOCHG;
         Dmsg1(100, "File not changed: %s\n", ff_pkt->fname);
         Dmsg4(200, "save_time=%d mtime=%d mtime_only=%d st_ctime=%d\n",
	    ff_pkt->save_time, ff_pkt->statp.st_mtime, 
	    ff_pkt->mtime_only, ff_pkt->statp.st_ctime);
	 return handle_file(ff_pkt, pkt);
      }
   }

#if xxxxxxx
   /* See if we are trying to dump the archive.  */
   if (ar_dev && ff_pkt->statp.st_dev == ar_dev && ff_pkt->statp.st_ino == ar_ino) {
       ff_pkt->type = FT_ISARCH;
       return handle_file(ff_pkt, pkt);
   }
#endif

   /* 
    * Handle hard linked files
    *
    * Maintain a list of hard linked files already backed up. This
    *  allows us to ensure that the data of each file gets backed 
    *  up only once.
    */
   if (ff_pkt->statp.st_nlink > 1
       && (S_ISREG(ff_pkt->statp.st_mode)
	   || S_ISCHR(ff_pkt->statp.st_mode)
	   || S_ISBLK(ff_pkt->statp.st_mode)
	   || S_ISFIFO(ff_pkt->statp.st_mode)
	   || S_ISSOCK(ff_pkt->statp.st_mode))) {

       struct f_link *lp;

       /* Search link list of hard linked files */
       for (lp = ff_pkt->linklist; lp; lp = lp->next)
	  if (lp->ino == ff_pkt->statp.st_ino && lp->dev == ff_pkt->statp.st_dev) {
	      ff_pkt->link = lp->name;
	      ff_pkt->type = FT_LNKSAVED;	/* Handle link, file already saved */
	      return handle_file(ff_pkt, pkt);
	  }

       /* File not previously dumped. Chain it into our list. */
       lp = (struct f_link *)bmalloc(sizeof (struct f_link) + strlen(fname));
       lp->ino = ff_pkt->statp.st_ino;
       lp->dev = ff_pkt->statp.st_dev;
       strcpy (lp->name, fname);
       lp->next = ff_pkt->linklist;
       ff_pkt->linklist = lp;
   }

   /* This is not a link to a previously dumped file, so dump it.  */
   if (S_ISREG(ff_pkt->statp.st_mode)) {
       off_t sizeleft;

       sizeleft = ff_pkt->statp.st_size;

       /* Don't bother opening empty, world readable files.  Also do not open
	  files when archive is meant for /dev/null.  */
       if (ff_pkt->null_output_device || (sizeleft == 0
	       && MODE_RALL == (MODE_RALL & ff_pkt->statp.st_mode))) {
	  ff_pkt->type = FT_REGE;
       } else {
	  ff_pkt->type = FT_REG;
       }
       return handle_file(ff_pkt, pkt);

   } else if (S_ISLNK(ff_pkt->statp.st_mode)) {
       int size;
       char *buffer = (char *)alloca(PATH_MAX + 1);

       size = readlink(fname, buffer, PATH_MAX + 1);
       if (size < 0) {
	   /* Could not follow link */				   
	   ff_pkt->type = FT_NOFOLLOW;
	   ff_pkt->ff_errno = errno;
	   return handle_file(ff_pkt, pkt);
       }
       buffer[size] = 0;
       ff_pkt->link = buffer;
       ff_pkt->type = FT_LNK;		/* got a real link */
       return handle_file(ff_pkt, pkt);

   } else if (S_ISDIR(ff_pkt->statp.st_mode)) {
       DIR *directory;
       struct dirent *entry, *result;
       char *namebuf;
       size_t namebuf_len;
       size_t len;
       int status;
       dev_t our_device = ff_pkt->statp.st_dev;

       if (access(fname, R_OK) == -1 && geteuid() != 0) {
	   /* Could not access() directory */
	   ff_pkt->type = FT_NOACCESS;
	   ff_pkt->ff_errno = errno;
	   return handle_file(ff_pkt, pkt);
       }

       /* Build a canonical directory name with a trailing slash. */
       len = strlen(fname);
       namebuf_len = len + DEFAULT_NAMEBUF_LEN;
       namebuf = (char *)bmalloc(namebuf_len + 2);
       bstrncpy(namebuf, fname, namebuf_len);
       while (len >= 1 && namebuf[len - 1] == '/')
	 len--;
       namebuf[len++] = '/';
       namebuf[len] = '\0';

       ff_pkt->link = namebuf;
       if (ff_pkt->incremental &&
	   (ff_pkt->statp.st_mtime < ff_pkt->save_time &&
	    ff_pkt->statp.st_ctime < ff_pkt->save_time)) {
	  /* Incremental option, directory entry not changed */
	  ff_pkt->type = FT_DIRNOCHG;
       } else {
	  ff_pkt->type = FT_DIR;
       }
       handle_file(ff_pkt, pkt);       /* handle directory entry */

       ff_pkt->link = ff_pkt->fname;     /* reset "link" */

       /* 
	* Do not decend into subdirectories (recurse) if the
	* user has turned it off for this directory.
	*/
       if (ff_pkt->no_recursion) {
	  free(namebuf);
	  /* No recursion into this directory */
	  ff_pkt->type = FT_NORECURSE;
	  return handle_file(ff_pkt, pkt);
       }

       /* 
	* See if we are crossing file systems, and
	* avoid doing so if the user only wants to dump one file system.
	*/
       if (ff_pkt->one_file_system && !top_level
	   && parent_device != ff_pkt->statp.st_dev) {
	  free(namebuf);
	  ff_pkt->type = FT_NOFSCHG;
	  return handle_file(ff_pkt, pkt);
       }

       /* 
	* Now process the files in this directory.
	*/
       errno = 0;
       if ((directory = opendir(fname)) == NULL) {
	  free(namebuf);
	  ff_pkt->type = FT_NOOPEN;
	  ff_pkt->ff_errno = errno;
	  return handle_file(ff_pkt, pkt);
       }

       /*
	* This would possibly run faster if we chdir to the directory
	* before traversing it.
	*/
       rtn_stat = 1;
       entry = (struct dirent *)malloc(sizeof(struct dirent) + name_max + 10);
       for ( ;; ) {
	   char *p;

	   status  = readdir_r(directory, entry, &result);
           Dmsg3(200, "readdir stat=%d result=%x name=%s\n", status, result,
	      entry->d_name);
	   if (status != 0 || result == NULL) {
	      break;
	   }
	   p = entry->d_name;
           /* Skip `.', `..', and excluded file names.  */
           if (p[0] == '\0' || (p[0] == '.' && (p[1] == '\0' ||
               (p[1] == '.' && p[2] == '\0')))) {
	      continue;
	   }

	   if ((int)NAMELEN(entry) + len >= namebuf_len) {
	       namebuf_len = len + NAMELEN(entry);
	       namebuf = (char *)brealloc(namebuf, namebuf_len + 2);
	   }
	   strcpy(namebuf + len, entry->d_name);
	   if (!file_is_excluded(ff_pkt, namebuf)) {
	      rtn_stat = find_one_file(ff_pkt, handle_file, pkt, namebuf, our_device, 0);
	   }
       }
       closedir(directory);
       free(namebuf);
       free(entry);
       if (ff_pkt->atime_preserve) {
	  utime(fname, &restore_times);
       }
       return rtn_stat;
   } /* end check for directory */

   /*
    * If it is explicitly mentioned (i.e. top_level) and is
    *  a block device, we do a raw backup of it.
    */
   if (top_level && S_ISBLK(ff_pkt->statp.st_mode)) {
      ff_pkt->type = FT_RAW;	      /* raw partition */
   } else {
      /* The only remaining types are special (character, ...) files */
      ff_pkt->type = FT_SPEC;
   }
   return handle_file(ff_pkt, pkt);
}

void term_find_one(FF_PKT *ff)
{
   struct f_link *lp, *lc;
  
   /* Free up list of hard linked files */
   for (lp = ff->linklist; lp;) {
      lc = lp;
      lp = lp->next;
      if (lc) {
	 free(lc);
      }
   }
   return;
}
