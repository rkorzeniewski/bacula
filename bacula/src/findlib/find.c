/*
 * Main routine for finding files on a file system.
 *  The heart of the work is done in find_one.c
 *
 *  Kern E. Sibbald, MM
 */
/*
   Copyright (C) 2000-2004 Kern Sibbald and John Walker

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


int32_t name_max;              /* filename max length */
int32_t path_max;              /* path name max length */


/* ****FIXME**** debug until stable */
#undef bmalloc
#define bmalloc(x) sm_malloc(__FILE__, __LINE__, x)
static void set_options(FF_PKT *ff, const char *opts);
static int our_callback(FF_PKT *ff, void *hpkt);
static bool accept_file(FF_PKT *ff);


/* 
 * Initialize the find files "global" variables
 */
FF_PKT *init_find_files()
{
  FF_PKT *ff;    

  ff = (FF_PKT *)bmalloc(sizeof(FF_PKT));
  memset(ff, 0, sizeof(FF_PKT));

  ff->sys_fname = get_pool_memory(PM_FNAME);

  init_include_exclude_files(ff);           /* init lists */

   /* Get system path and filename maximum lengths */
   path_max = pathconf(".", _PC_PATH_MAX);
   if (path_max < 1024) {
      path_max = 1024;
   }

   name_max = pathconf(".", _PC_NAME_MAX);
   if (name_max < 1024) {
      name_max = 1024;
   }
   path_max++;                        /* add for EOS */
   name_max++;                        /* add for EOS */

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
 * Find all specified files (determined by calls to name_add()
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
find_files(JCR *jcr, FF_PKT *ff, int callback(FF_PKT *ff_pkt, void *hpkt), void *his_pkt) 
{
   ff->callback = callback;

   /* This is the new way */
   findFILESET *fileset = ff->fileset;
   if (fileset) {
      int i, j;
      for (i=0; i<fileset->include_list.size(); i++) {
         findINCEXE *incexe = (findINCEXE *)fileset->include_list.get(i);
         fileset->incexe = incexe;
         /*
          * By setting all options, we in effect or the global options
          *   which is what we want.
          */
         for (j=0; j<incexe->opts_list.size(); j++) {
            findFOPTS *fo = (findFOPTS *)incexe->opts_list.get(j);
            Dmsg1(400, "Find global options O %s\n", fo->opts);
            set_options(ff, fo->opts);
         }
         for (j=0; j<incexe->name_list.size(); j++) {
            Dmsg1(400, "F %s\n", (char *)incexe->name_list.get(j));
            char *fname = (char *)incexe->name_list.get(j);
            if (!find_one_file(jcr, ff, our_callback, his_pkt, fname, (dev_t)-1, 1)) {
               return 0;                  /* error return */
            }
         }
      }
   } else {
      struct s_included_file *inc = NULL;

      /* This is the old deprecated way */
      while (!job_canceled(jcr) && (inc = get_next_included_file(ff, inc))) {
         /* Copy options for this file */
         bstrncpy(ff->VerifyOpts, inc->VerifyOpts, sizeof(ff->VerifyOpts)); 
         Dmsg1(50, "find_files: file=%s\n", inc->fname);
         if (!file_is_excluded(ff, inc->fname)) {
            if (!find_one_file(jcr, ff, callback, his_pkt, inc->fname, (dev_t)-1, 1)) {
               return 0;                  /* error return */
            }
         }
      }
   }
   return 1;
}

static bool accept_file(FF_PKT *ff)
{
   int i, j, k;
   findFILESET *fileset = ff->fileset;
   findINCEXE *incexe = fileset->incexe;

   for (j=0; j<incexe->opts_list.size(); j++) {
      findFOPTS *fo = (findFOPTS *)incexe->opts_list.get(j);
      for (k=0; k<fo->wild.size(); k++) {
         
      }
   }

   for (i=0; i<fileset->exclude_list.size(); i++) {
      findINCEXE *incexe = (findINCEXE *)fileset->exclude_list.get(i);
      for (j=0; j<incexe->opts_list.size(); j++) {
         findFOPTS *fo = (findFOPTS *)incexe->opts_list.get(j);
         for (k=0; k<fo->wild.size(); k++) {
            Dmsg1(400, "W %s\n", (char *)fo->wild.get(k));
            if (fnmatch((char *)fo->wild.get(k), ff->fname, FNM_PATHNAME) == 0) {
               Dmsg1(000, "Reject wild: %s\n", ff->fname);
               return false;          /* reject file */
            }
         }
      }
      for (j=0; j<incexe->name_list.size(); j++) {
         Dmsg1(400, "F %s\n", (char *)incexe->name_list.get(j));
         if (fnmatch((char *)incexe->name_list.get(j), ff->fname, FNM_PATHNAME) == 0) {
            Dmsg1(000, "Reject: %s\n", ff->fname);
            return false;          /* reject file */
         }
      }
   }
   Dmsg1(000, "Accept: %s\n", ff->fname);
   return true;
}

/*
 * The code comes here for each file examined.
 * We filter the files, then call the user's callback if
 *    the file is included. 
 */
static int our_callback(FF_PKT *ff, void *hpkt)
{
   switch (ff->type) {
   case FT_NOACCESS:
   case FT_NOFOLLOW:
   case FT_NOSTAT:
   case FT_NOCHG:
   case FT_ISARCH:
   case FT_NORECURSE:
   case FT_NOFSCHG:
   case FT_NOOPEN:
      Dmsg1(000, "File=%s\n", ff->fname);
      return ff->callback(ff, hpkt);

   /* These items can be filtered */
   case FT_LNKSAVED:
   case FT_REGE:
   case FT_REG:
   case FT_LNK:
   case FT_DIRBEGIN:
   case FT_DIREND:
   case FT_SPEC:
      if (accept_file(ff)) {
         return ff->callback(ff, hpkt);
      } else {
         return 0;
      }
   }    
   return 0;
}


/*
 * As an optimization, we should do this during
 *  "compile" time in filed/job.c, and keep only a bit mask
 *  and the Verify options.
 */
static void set_options(FF_PKT *ff, const char *opts)
{
   int j;
   const char *p;

   for (p=opts; *p; p++) {
      switch (*p) {
      case 'a':                 /* alway replace */
      case '0':                 /* no option */
         break;
      case 'e':
         ff->flags |= FO_EXCLUDE;
         break;
      case 'f':
         ff->flags |= FO_MULTIFS;
         break;
      case 'h':                 /* no recursion */
         ff->flags |= FO_NO_RECURSION;
         break;
      case 'M':                 /* MD5 */
         ff->flags |= FO_MD5;
         break;
      case 'n':
         ff->flags |= FO_NOREPLACE;
         break;
      case 'p':                 /* use portable data format */
         ff->flags |= FO_PORTABLE;
         break;
      case 'r':                 /* read fifo */
         ff->flags |= FO_READFIFO;
         break;
      case 'S':
         ff->flags |= FO_SHA1;
         break;
      case 's':
         ff->flags |= FO_SPARSE;
         break;
      case 'm':
         ff->flags |= FO_MTIMEONLY;
         break;
      case 'k':
         ff->flags |= FO_KEEPATIME;
         break;
      case 'V':                  /* verify options */
         /* Copy Verify Options */
         for (j=0; *p && *p != ':'; p++) {
            ff->VerifyOpts[j] = *p;
            if (j < (int)sizeof(ff->VerifyOpts) - 1) {
               j++;
            }
         }
         ff->VerifyOpts[j] = 0;
         break;
      case 'w':
         ff->flags |= FO_IF_NEWER;
         break;
      case 'Z':                 /* gzip compression */
         ff->flags |= FO_GZIP;
         ff->GZIP_level = *++p - '0';
         Dmsg1(200, "Compression level=%d\n", ff->GZIP_level);
         break;
      default:
         Emsg1(M_ERROR, 0, "Unknown include/exclude option: %c\n", *p);
         break;
      }
   }
}


/*
 * Terminate find_files() and release
 * all allocated memory   
 */
int
term_find_files(FF_PKT *ff)
{
  int hard_links;

  term_include_exclude_files(ff);
  free_pool_memory(ff->sys_fname);
  hard_links = term_find_one(ff);
  free(ff);
  return hard_links;
}
