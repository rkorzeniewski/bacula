/*
 *  Create a file, and reset the modes
 *
 *    Kern Sibbald, November MM
 *
 *   Version $Id$
 *
 */
/*
   Copyright (C) 2000, 2001, 2002 Kern Sibbald and John Walker

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

#ifndef S_IRWXUGO
#define S_IRWXUGO (S_IRWXU | S_IRWXG | S_IRWXO)
#endif

#ifndef IS_CTG
#define IS_CTG(x) 0
#define O_CTG 0
#endif


/*
 * Create the file, or the directory
 *
 *  fname is the original filename  
 *  ofile is the output filename (may be in a different directory)
 *
 * Returns:  1 on success
 *	     0 on failure
 *
 *   Note, we create the file here, except for special files,
 *     we do not set the attributes because we want to first 
 *     write the file, then when the writing is done, set the
 *     attributes.
 *   So, we return with the file descriptor open for normal 
 *     files.
 *
 */
int create_file(void *jcr, char *fname, char *ofile, char *lname,
		int type, int stream, struct stat *statp, 
		char *attribsEx, int *ofd)
{
   int new_mode, parent_mode, mode;
   uid_t uid;
   gid_t gid;
   int stat = 0;
   int fnl, pnl;
   char *f, *p, savechr;

   *ofd = -1;
/*
 * new_mode = S_IRWXUGO & ~umask(0);
 */
   new_mode = statp->st_mode;
   Dmsg2(300, "newmode=%x file=%s\n", new_mode, ofile);
   parent_mode = S_IWUSR | S_IXUSR | new_mode;
   gid = statp->st_gid;
   uid = statp->st_uid;

   switch (type) {
   case FT_LNKSAVED:		      /* Hard linked, file already saved */
      Dmsg2(130, "Hard link %s => %s\n", ofile, lname);
      if (link(lname, ofile) != 0) {
         Jmsg3(jcr, M_ERROR, 0, "Could not hard link %s ==> %s: ERR=%s\n", 
	       ofile, lname, strerror(errno));
      }
      break;
   case FT_REGE:		      /* empty file */
   case FT_REG: 		      /* regular file */
      /* Separate pathname and filename */
      for (p=f=ofile; *p; p++) {
         if (*p == '/') {
	    f = p;		      /* possible filename */
	 }
      }
      if (*f == '/') {
	 f++;
      }

      fnl = p - f;
      if (fnl == 0) {
         Jmsg1(jcr, M_ERROR, 0, "Zero length filename: %s\n", fname);
	 return 0;
      }

      pnl = f - ofile - 1;    
      if (pnl <= 0) {
         Jmsg1(jcr, M_ERROR, 0, "Zero length path: %s\n", fname);
	 return 0;
      }
      savechr = ofile[pnl];
      ofile[pnl] = 0;		      /* terminate path */

      Dmsg1(50, "Make path %s\n", ofile);
      /*
       * If we need to make the directory, ensure that it is with
       * execute bit set (i.e. parent_mode), and preserve what already
       * exists. Normally, this should do nothing.
       */
      stat = !make_path(jcr, ofile, parent_mode, parent_mode, uid, gid, 1, NULL);
      if (stat == 0) {
         Dmsg1(0, "Could not make path. %s\n", ofile);
	 return 0;
      }
      
      ofile[pnl] = savechr;	      /* restore full name */
      Dmsg1(100, "Create file %s\n", ofile);
      mode =  O_WRONLY | O_CREAT | O_TRUNC;
      if (IS_CTG(statp->st_mode)) {
	 mode |= O_CTG; 	      /* set contiguous bit if needed */
      }
      Dmsg1(50, "Create file: %s\n", ofile);
      if ((*ofd = open(ofile, mode, S_IRUSR | S_IWUSR)) < 0) {
         Jmsg2(jcr, M_ERROR, 0, "Could not create %s: ERR=%s\n", ofile, strerror(errno));
	 return 0;
      }
      return 1;
   case FT_LNK:
      Dmsg2(130, "FT_LNK should restore: %s -> %s\n", ofile, lname);
      if (symlink(lname, ofile) != 0 && errno != EEXIST) {
         Jmsg3(jcr, M_ERROR, 0, "Could not symlink %s -> %s: ERR=%s\n", 
	    ofile, lname, strerror(errno));
      }
      return 0;
   case FT_DIR:
      Dmsg2(300, "Make dir mode=%o dir=%s\n", new_mode, ofile);
      if (make_path(jcr, ofile, new_mode, parent_mode, uid, gid, 0, NULL) != 0) {
         Jmsg1(jcr, M_ERROR, 0, "Could not make directory: %s\n", ofile);
      }
      return 0;
   case FT_SPEC:
      if (S_ISFIFO(statp->st_mode)) {
         Dmsg1(0, "Restore fifo: %s\n", ofile);
	 if (mkfifo(ofile, statp->st_mode) != 0) {
            Jmsg2(jcr, M_ERROR, 0, "Cannot make fifo %s: ERR=%s\n", ofile, strerror(errno));
	    return 0;
	 }
      } else {		
         Dmsg1(0, "Restore node: %s\n", ofile);
	 if (mknod(ofile, statp->st_mode, statp->st_rdev) != 0) {
            Jmsg2(jcr, M_ERROR, 0, "Cannot make node %s: ERR=%s\n", ofile, strerror(errno));
	    return 0;
	 }
      }       
      Dmsg1(0, "FT_SPEC %s\n", ofile);
      return 0;

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
      Jmsg2(jcr, M_ERROR, 0, "Original file %s not saved. Stat=%d\n", fname, type);
      return 0;
   default:
      Jmsg2(jcr, M_ERROR, 0, "Unknown file type %d; not restored: %s\n", type, fname);
      return 0;
   }

   return 0;
}
