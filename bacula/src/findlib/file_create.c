/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2000-2014 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from
   many others, a complete list can be found in the file AUTHORS.
   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

   Bacula® is a registered trademark of Kern Sibbald.
   The licensor of Bacula is the Free Software Foundation Europe
   (FSFE), Fiduciary Program, Sumatrastrasse 25, 8006 Zürich,
   Switzerland, email:ftf@fsfeurope.org.
*/
/*
 *  Create a file, and reset the modes
 *
 *    Kern Sibbald, November MM
 *
 */

#include "bacula.h"
#include "find.h"

#ifndef S_IRWXUGO
#define S_IRWXUGO (S_IRWXU | S_IRWXG | S_IRWXO)
#endif

#ifndef IS_CTG
#define IS_CTG(x) 0
#define O_CTG 0
#endif

#ifndef O_EXCL
#define O_EXCL 0
#endif
#ifndef O_NOFOLLOW
#define O_NOFOLLOW 0
#endif

static int separate_path_and_file(JCR *jcr, char *fname, char *ofile);
static int path_already_seen(JCR *jcr, char *path, int pnl);


/*
 * Create the file, or the directory
 *
 *  fname is the original filename
 *  ofile is the output filename (may be in a different directory)
 *
 * Returns:  CF_SKIP     if file should be skipped
 *           CF_ERROR    on error
 *           CF_EXTRACT  file created and data to restore
 *           CF_CREATED  file created no data to restore
 *
 *   Note, we create the file here, except for special files,
 *     we do not set the attributes because we want to first
 *     write the file, then when the writing is done, set the
 *     attributes.
 *   So, we return with the file descriptor open for normal
 *     files.
 *
 */
int create_file(JCR *jcr, ATTR *attr, BFILE *bfd, int replace)
{
   mode_t new_mode, parent_mode;
   int flags;
   uid_t uid;
   gid_t gid;
   int pnl;
   bool exists = false;
   struct stat mstatp;

   bfd->reparse_point = false;
   set_portable_backup(bfd);

   new_mode = attr->statp.st_mode;
   Dmsg3(200, "type=%d newmode=%x file=%s\n", attr->type, new_mode, attr->ofname);
   parent_mode = S_IWUSR | S_IXUSR | new_mode;
   gid = attr->statp.st_gid;
   uid = attr->statp.st_uid;

   Dmsg2(400, "Replace=%c %d\n", (char)replace, replace);
   if (lstat(attr->ofname, &mstatp) == 0) {
      exists = true;
      switch (replace) {
      case REPLACE_IFNEWER:
         if (attr->statp.st_mtime <= mstatp.st_mtime) {
            Qmsg(jcr, M_SKIPPED, 0, _("File skipped. Not newer: %s\n"), attr->ofname);
            return CF_SKIP;
         }
         break;

      case REPLACE_IFOLDER:
         if (attr->statp.st_mtime >= mstatp.st_mtime) {
            Qmsg(jcr, M_SKIPPED, 0, _("File skipped. Not older: %s\n"), attr->ofname);
            return CF_SKIP;
         }
         break;

      case REPLACE_NEVER:
         /* Set attributes if we created this directory */
         if (attr->type == FT_DIREND && path_list_lookup(jcr, attr->ofname)) {
            break;
         }
         Qmsg(jcr, M_SKIPPED, 0, _("File skipped. Already exists: %s\n"), attr->ofname);
         return CF_SKIP;

      case REPLACE_ALWAYS:
         break;
      }
   }
   switch (attr->type) {
   case FT_RAW:                       /* raw device to be written */
   case FT_FIFO:                      /* FIFO to be written to */
   case FT_LNKSAVED:                  /* Hard linked, file already saved */
   case FT_LNK:
   case FT_SPEC:                      /* fifo, ... to be backed up */
   case FT_REGE:                      /* empty file */
   case FT_REG:                       /* regular file */
      /*
       * Note, we do not delete FT_RAW because these are device files
       *  or FIFOs that should already exist. If we blow it away,
       *  we may blow away a FIFO that is being used to read the
       *  restore data, or we may blow away a partition definition.
       */
      if (exists && attr->type != FT_RAW && attr->type != FT_FIFO) {
         /* Get rid of old copy */
         Dmsg1(400, "unlink %s\n", attr->ofname);
         if (unlink(attr->ofname) == -1) {
            berrno be;
            Qmsg(jcr, M_ERROR, 0, _("File %s already exists and could not be replaced. ERR=%s.\n"),
               attr->ofname, be.bstrerror());
            return CF_ERROR;
         }
      }
      /*
       * Here we do some preliminary work for all the above
       *   types to create the path to the file if it does
       *   not already exist.  Below, we will split to
       *   do the file type specific work
       */
      pnl = separate_path_and_file(jcr, attr->fname, attr->ofname);
      if (pnl < 0) {
         return CF_ERROR;
      }

      /*
       * If path length is <= 0 we are making a file in the root
       *  directory. Assume that the directory already exists.
       */
      if (pnl > 0) {
         char savechr;
         savechr = attr->ofname[pnl];
         attr->ofname[pnl] = 0;                 /* terminate path */

         if (!path_already_seen(jcr, attr->ofname, pnl)) {
            Dmsg1(400, "Make path %s\n", attr->ofname);
            /*
             * If we need to make the directory, ensure that it is with
             * execute bit set (i.e. parent_mode), and preserve what already
             * exists. Normally, this should do nothing.
             */
            if (!makepath(attr, attr->ofname, parent_mode, parent_mode, uid, gid, 1)) {
               Dmsg1(10, "Could not make path. %s\n", attr->ofname);
               attr->ofname[pnl] = savechr;     /* restore full name */
               return CF_ERROR;
            }
         }
         attr->ofname[pnl] = savechr;           /* restore full name */
      }

      /* Now we do the specific work for each file type */
      switch(attr->type) {
      case FT_REGE:
      case FT_REG:
         Dmsg1(100, "Create=%s\n", attr->ofname);
         flags =  O_WRONLY | O_CREAT | O_BINARY | O_EXCL;
         if (IS_CTG(attr->statp.st_mode)) {
            flags |= O_CTG;              /* set contiguous bit if needed */
         }
         if (is_bopen(bfd)) {
            Qmsg1(jcr, M_ERROR, 0, _("bpkt already open fid=%d\n"), bfd->fid);
            bclose(bfd);
         }


         if ((bopen(bfd, attr->ofname, flags, S_IRUSR | S_IWUSR)) < 0) {
            berrno be;
            be.set_errno(bfd->berrno);
            Qmsg2(jcr, M_ERROR, 0, _("Could not create %s: ERR=%s\n"),
                  attr->ofname, be.bstrerror());
            Dmsg2(100,"Could not create %s: ERR=%s\n", attr->ofname, be.bstrerror());
            return CF_ERROR;
         }
         return CF_EXTRACT;

      case FT_RAW:                    /* Bacula raw device e.g. /dev/sda1 */
      case FT_FIFO:                   /* Bacula fifo to save data */
      case FT_SPEC:
         if (S_ISFIFO(attr->statp.st_mode)) {
            Dmsg1(400, "Restore fifo: %s\n", attr->ofname);
            if (mkfifo(attr->ofname, attr->statp.st_mode) != 0 && errno != EEXIST) {
               berrno be;
               Qmsg2(jcr, M_ERROR, 0, _("Cannot make fifo %s: ERR=%s\n"),
                     attr->ofname, be.bstrerror());
               return CF_ERROR;
            }
         } else if (S_ISSOCK(attr->statp.st_mode)) {
             Dmsg1(200, "Skipping restore of socket: %s\n", attr->ofname);
#ifdef S_IFDOOR     // Solaris high speed RPC mechanism
         } else if (S_ISDOOR(attr->statp.st_mode)) {
             Dmsg1(200, "Skipping restore of door file: %s\n", attr->ofname);
#endif
#ifdef S_IFPORT     // Solaris event port for handling AIO
         } else if (S_ISPORT(attr->statp.st_mode)) {
             Dmsg1(200, "Skipping restore of event port file: %s\n", attr->ofname);
#endif
         } else {
            Dmsg1(400, "Restore node: %s\n", attr->ofname);
            if (mknod(attr->ofname, attr->statp.st_mode, attr->statp.st_rdev) != 0 && errno != EEXIST) {
               berrno be;
               Qmsg2(jcr, M_ERROR, 0, _("Cannot make node %s: ERR=%s\n"),
                     attr->ofname, be.bstrerror());
               return CF_ERROR;
            }
         }
         /*
          * Here we are going to attempt to restore to a FIFO, which
          *   means that the FIFO must already exist, AND there must
          *   be some process already attempting to read from the
          *   FIFO, so we open it write-only.
          */
         if (attr->type == FT_RAW || attr->type == FT_FIFO) {
            btimer_t *tid;
            Dmsg1(400, "FT_RAW|FT_FIFO %s\n", attr->ofname);
            flags =  O_WRONLY | O_BINARY;
            /* Timeout open() in 60 seconds */
            if (attr->type == FT_FIFO) {
               Dmsg0(400, "Set FIFO timer\n");
               tid = start_thread_timer(jcr, pthread_self(), 60);
            } else {
               tid = NULL;
            }
            if (is_bopen(bfd)) {
               Qmsg1(jcr, M_ERROR, 0, _("bpkt already open fid=%d\n"), bfd->fid);
            }
            Dmsg2(400, "open %s flags=0x%x\n", attr->ofname, flags);
            if ((bopen(bfd, attr->ofname, flags, 0)) < 0) {
               berrno be;
               be.set_errno(bfd->berrno);
               Qmsg2(jcr, M_ERROR, 0, _("Could not open %s: ERR=%s\n"),
                     attr->ofname, be.bstrerror());
               Dmsg2(400, "Could not open %s: ERR=%s\n", attr->ofname, be.bstrerror());
               stop_thread_timer(tid);
               return CF_ERROR;
            }
            stop_thread_timer(tid);
            return CF_EXTRACT;
         }
         Dmsg1(400, "FT_SPEC %s\n", attr->ofname);
         return CF_CREATED;

      case FT_LNK:
         Dmsg2(130, "FT_LNK should restore: %s -> %s\n", attr->ofname, attr->olname);
         if (symlink(attr->olname, attr->ofname) != 0 && errno != EEXIST) {
            berrno be;
            Qmsg3(jcr, M_ERROR, 0, _("Could not symlink %s -> %s: ERR=%s\n"),
                  attr->ofname, attr->olname, be.bstrerror());
            return CF_ERROR;
         }
         return CF_CREATED;

      case FT_LNKSAVED:                  /* Hard linked, file already saved */
         Dmsg2(130, "Hard link %s => %s\n", attr->ofname, attr->olname);
         if (link(attr->olname, attr->ofname) != 0) {
            berrno be;
#ifdef HAVE_CHFLAGS
            struct stat s;
           /*
            * If using BSD user flags, maybe has a file flag
            * preventing this. So attempt to disable, retry link,
            * and reset flags.
            * Note that BSD securelevel may prevent disabling flag.
            */
            if (stat(attr->olname, &s) == 0 && s.st_flags != 0) {
               if (chflags(attr->olname, 0) == 0) {
                  if (link(attr->olname, attr->ofname) != 0) {
                     /* restore original file flags even when linking failed */
                     if (chflags(attr->olname, s.st_flags) < 0) {
                        Qmsg2(jcr, M_ERROR, 0, _("Could not restore file flags for file %s: ERR=%s\n"),
                              attr->olname, be.bstrerror());
                     }
#endif /* HAVE_CHFLAGS */
            Qmsg3(jcr, M_ERROR, 0, _("Could not hard link %s -> %s: ERR=%s\n"),
                  attr->ofname, attr->olname, be.bstrerror());
            Dmsg3(200, "Could not hard link %s -> %s: ERR=%s\n",
                  attr->ofname, attr->olname, be.bstrerror());
            return CF_ERROR;
#ifdef HAVE_CHFLAGS
                  }
                  /* finally restore original file flags */
                  if (chflags(attr->olname, s.st_flags) < 0) {
                     Qmsg2(jcr, M_ERROR, 0, _("Could not restore file flags for file %s: ERR=%s\n"),
                            attr->olname, be.bstrerror());
                  }
               } else {
                 Qmsg2(jcr, M_ERROR, 0, _("Could not reset file flags for file %s: ERR=%s\n"),
                       attr->olname, be.bstrerror());
               }
            } else {
              Qmsg3(jcr, M_ERROR, 0, _("Could not hard link %s -> %s: ERR=%s\n"),
                    attr->ofname, attr->olname, be.bstrerror());
              return CF_ERROR;
            }
#endif /* HAVE_CHFLAGS */

         }
         return CF_CREATED;
      } /* End inner switch */

   case FT_REPARSE:
   case FT_JUNCTION:
      bfd->reparse_point = true;
      /* Fall through wanted */
   case FT_DIRBEGIN:
   case FT_DIREND:
      Dmsg2(200, "Make dir mode=%o dir=%s\n", new_mode, attr->ofname);
      if (!makepath(attr, attr->ofname, new_mode, parent_mode, uid, gid, 0)) {
         return CF_ERROR;
      }
      /*
       * If we are using the Win32 Backup API, we open the
       *   directory so that the security info will be read
       *   and saved.
       */
      if (!is_portable_backup(bfd)) {
         if (is_bopen(bfd)) {
            Qmsg1(jcr, M_ERROR, 0, _("bpkt already open fid=%d\n"), bfd->fid);
         }
         if ((bopen(bfd, attr->ofname, O_WRONLY|O_BINARY, 0)) < 0) {
            berrno be;
            be.set_errno(bfd->berrno);
            Qmsg2(jcr, M_ERROR, 0, _("Could not open %s: ERR=%s\n"),
                  attr->ofname, be.bstrerror());
            return CF_ERROR;
         }
         return CF_EXTRACT;
      } else {
         return CF_CREATED;
      }

   case FT_DELETED:
      Qmsg2(jcr, M_INFO, 0, _("Original file %s have been deleted: type=%d\n"), attr->fname, attr->type);
      break;
   /* The following should not occur */
   case FT_NOACCESS:
   case FT_NOFOLLOW:
   case FT_NOSTAT:
   case FT_DIRNOCHG:
   case FT_NOCHG:
   case FT_ISARCH:
   case FT_NORECURSE:
   case FT_NOFSCHG:
   case FT_NOOPEN:
      Qmsg2(jcr, M_ERROR, 0, _("Original file %s not saved: type=%d\n"), attr->fname, attr->type);
      break;
   default:
      Qmsg2(jcr, M_ERROR, 0, _("Unknown file type %d; not restored: %s\n"), attr->type, attr->fname);
      Pmsg2(000, "Unknown file type %d; not restored: %s\n", attr->type, attr->fname);
      break;
   }
   return CF_ERROR;
}

/*
 *  Returns: > 0 index into path where last path char is.
 *           0  no path
 *           -1 filename is zero length
 */
static int separate_path_and_file(JCR *jcr, char *fname, char *ofile)
{
   char *f, *p, *q;
   int fnl, pnl;

   /* Separate pathname and filename */
   for (q=p=f=ofile; *p; p++) {
      if (IsPathSeparator(*p)) {
         f = q;                    /* possible filename */
      }
      q++;
   }

   if (IsPathSeparator(*f)) {
      f++;
   }
   *q = 0;                         /* terminate string */

   fnl = q - f;
   if (fnl == 0) {
      /* The filename length must not be zero here because we
       *  are dealing with a file (i.e. FT_REGE or FT_REG).
       */
      Jmsg1(jcr, M_ERROR, 0, _("Zero length filename: %s\n"), fname);
      return -1;
   }
   pnl = f - ofile - 1;
   return pnl;
}

/*
 * Primitive caching of path to prevent recreating a pathname
 *   each time as long as we remain in the same directory.
 */
static int path_already_seen(JCR *jcr, char *path, int pnl)
{
   if (!jcr->cached_path) {
      jcr->cached_path = get_pool_memory(PM_FNAME);
   }
   if (jcr->cached_pnl == pnl && strcmp(path, jcr->cached_path) == 0) {
      return 1;
   }
   pm_strcpy(jcr->cached_path, path);
   jcr->cached_pnl = pnl;
   return 0;
}
