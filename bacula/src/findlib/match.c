/*
 *  Routines used to keep and match include and exclude
 *   filename/pathname patterns.
 *
 *   Kern E. Sibbald, December MMI
 *
 */
/*
   Copyright (C) 2001-2003 Kern Sibbald and John Walker

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

#include <pwd.h>
#include <grp.h>
#include <sys/types.h>

#ifndef FNM_LEADING_DIR
#define FNM_LEADING_DIR 0
#endif

#undef bmalloc
#define bmalloc(x) sm_malloc(__FILE__, __LINE__, x)

#ifdef HAVE_CYGWIN
static int win32_client = 1;
#else
static int win32_client = 0;
#endif

       
/*
 * Initialize structures for filename matching
 */
void init_include_exclude_files(FF_PKT *ff)
{
}

/*
 * Done doing filename matching, release all 
 *  resources used.
 */
void term_include_exclude_files(FF_PKT *ff)
{
   struct s_included_file *inc, *next_inc;
   struct s_excluded_file *exc, *next_exc;

   for (inc=ff->included_files_list; inc; ) {
      next_inc = inc->next;
      free(inc);
      inc = next_inc;
   }

   for (exc=ff->excluded_files_list; exc; ) {
      next_exc = exc->next;
      free(exc);
      exc = next_exc;
   }

   for (exc=ff->excluded_paths_list; exc; ) {
      next_exc = exc->next;
      free(exc);
      exc = next_exc;
   }
   
}

/*
 * Add a filename to list of included files
 */
void add_fname_to_include_list(FF_PKT *ff, int prefixed, char *fname)
{
   int len, j;
   struct s_included_file *inc;
   char *p;

   len = strlen(fname);

   inc =(struct s_included_file *)bmalloc(sizeof(struct s_included_file) + len + 1);
   inc->options = 0;
   inc->VerifyOpts[0] = 'V'; 
   inc->VerifyOpts[1] = ':';
   inc->VerifyOpts[2] = 0;

   /* prefixed = preceded with options */
   if (prefixed) {
      for (p=fname; *p && *p != ' '; p++) {
	 switch (*p) {
            case 'a':                 /* alway replace */
            case '0':                 /* no option */
	       break;
            case 'f':
	       inc->options |= OPT_multifs;
	       break;
            case 'h':                 /* no recursion */
	       inc->options |= OPT_no_recursion;
	       break;
            case 'M':                 /* MD5 */
	       inc->options |= OPT_compute_MD5;
	       break;
            case 'S':
	       inc->options |= OPT_compute_SHA1;
	       break;
            case 'n':
	       inc->options |= OPT_never_replace;
	       break;
            case 'r':                 /* read fifo */
	       inc->options |= OPT_read_fifo;
	       break;
            case 's':
	       inc->options |= OPT_sparse;
	       break;
            case 'V':                  /* verify options */
	       /* Copy Verify Options */
               for (j=0; *p && *p != ':'; p++) {
		  inc->VerifyOpts[j] = *p;
		  if (j < (int)sizeof(inc->VerifyOpts) - 1) {
		     j++;
		  }
	       }
	       inc->VerifyOpts[j] = 0;
	       break;
            case 'w':
	       inc->options |= OPT_replace_if_newer;
	       break;
            case 'Z':                 /* gzip compression */
	       inc->options |= OPT_GZIP_compression;
               inc->level = *++p - '0';
               Dmsg1(200, "Compression level=%d\n", inc->level);
	       break;
	    default:
               Emsg1(M_ERROR, 0, "Unknown include/exclude option: %c\n", *p);
	       break;
	 }
      }
      /* Skip past space(s) */
      for ( ; *p == ' '; p++)
	 {}
   } else {
      p = fname;
   }

   strcpy(inc->fname, p);		  
   p = inc->fname;
   len = strlen(p);
   /* Zap trailing slashes.  */
   p += len - 1;
   while (p > inc->fname && *p == '/') {
      *p-- = 0;
      len--;
   }
   inc->len = len;
   /* Check for wild cards */
   inc->pattern = 0;
   for (p=inc->fname; *p; p++) {
      if (*p == '*' || *p == '[' || *p == '?') {
	 inc->pattern = 1;
	 break;
      }
   }
#ifdef HAVE_CYGWIN
   /* Convert any \'s into /'s */
   for (p=inc->fname; *p; p++) {
      if (*p == '\\') {
         *p = '/';
      }
   }
#endif
   inc->next = NULL;
   /* Chain this one on the end of the list */
   if (!ff->included_files_list) {
      /* First one, so set head */
      ff->included_files_list = inc;
   } else {
      struct s_included_file *next;
      /* Walk to end of list */
      for (next=ff->included_files_list; next->next; next=next->next)
	 { }
      next->next = inc;
   }  
   Dmsg1(50, "add_fname_to_include fname=%s\n", inc->fname);
}

/*
 * We add an exclude name to either the exclude path
 *  list or the exclude filename list.
 */
void add_fname_to_exclude_list(FF_PKT *ff, char *fname)
{
   int len;
   struct s_excluded_file *exc, **list;

#ifdef HAVE_CYGWIN
   /* Convert any \'s into /'s */
   for (char *p=fname; *p; p++) {
      if (*p == '\\') {
         *p = '/';
      }
   }
#endif
   Dmsg1(20, "Add name to exclude: %s\n", fname);

   if (strchr(fname, '/')) {
      list = &ff->excluded_paths_list;
   } else {
      list = &ff->excluded_files_list;
   }
  
   len = strlen(fname);

   exc = (struct s_excluded_file *)bmalloc(sizeof(struct s_excluded_file) + len + 1);
   exc->next = *list;
   exc->len = len;
   strcpy(exc->fname, fname);		      
   *list = exc;
}


/*
 * Get next included file
 */
struct s_included_file *get_next_included_file(FF_PKT *ff, struct s_included_file *ainc)
{
   struct s_included_file *inc;

   if (ainc == NULL) { 
      inc = ff->included_files_list;
   } else {
      inc = ainc->next;
   }
   /*
    * copy inc_options for this file into the ff packet
    */
   if (inc) {
      ff->flags = inc->options;
      ff->GZIP_level = inc->level;
   }
   return inc;
}

/*
 * Walk through the included list to see if this
 *  file is included possibly with wild-cards.
 */

int file_is_included(FF_PKT *ff, char *file)
{
   struct s_included_file *inc = ff->included_files_list;
   int len;

   for ( ; inc; inc=inc->next ) {
      if (inc->pattern) {
	 if (fnmatch(inc->fname, file, FNM_LEADING_DIR) == 0) {
	    return 1;
	 }
	 continue;
      } 			    
      /*
       * No wild cards. We accept a match to the
       *  end of any component.
       */
      Dmsg2(900, "pat=%s file=%s\n", inc->fname, file);
      len = strlen(file);
      if (inc->len == len && strcmp(inc->fname, file) == 0) {
	 return 1;
      }
      if (inc->len < len && file[inc->len] == '/' && 
	  strncmp(inc->fname, file, inc->len) == 0) {
	 return 1;
      }
      if (inc->len == 1 && inc->fname[0] == '/') {
	 return 1;
      }
   }
   return 0;
}


/*
 * This is the workhorse of excluded_file().
 * Determine if the file is excluded or not.
 */
static int
file_in_excluded_list(struct s_excluded_file *exc, char *file)
{
   if (exc == NULL) {
      Dmsg0(900, "exc is NULL\n");
   }
   for ( ; exc; exc=exc->next ) {
      if (fnmatch(exc->fname, file, FNM_PATHNAME) == 0) {
         Dmsg2(900, "Match exc pat=%s: file=%s:\n", exc->fname, file);
	 return 1;
      }
      Dmsg2(900, "No match exc pat=%s: file=%s:\n", exc->fname, file);
   }
   return 0;
}


/*
 * Walk through the excluded lists to see if this
 *  file is excluded, or if it matches a component
 *  of an excluded directory.
 */

int file_is_excluded(FF_PKT *ff, char *file)
{
   char *p;

   if (win32_client && file[1] == ':') {
      file += 2;
   }

   if (file_in_excluded_list(ff->excluded_paths_list, file)) {
      return 1;
   }

   /* Try each component */
   for (p = file; *p; p++) {
      /* Match from the beginning of a component only */
      if ((p == file || (*p != '/' && *(p-1) == '/'))
	   && file_in_excluded_list(ff->excluded_files_list, p)) {
	 return 1;
      }
   }
   return 0;
}
