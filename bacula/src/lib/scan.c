/*
 *   scan.c -- scanning routines for Bacula
 * 
 *    Kern Sibbald, MM  separated from util.c MMIII
 *
 *   Version $Id$
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
#include "jcr.h"
#include "findlib/find.h"


/* Strip any trailing junk from the command */
void strip_trailing_junk(char *cmd)
{
   char *p;
   p = cmd + strlen(cmd) - 1;

   /* strip trailing junk from command */
   while ((p >= cmd) && (*p == '\n' || *p == '\r' || *p == ' '))
      *p-- = 0;
}

/* Strip any trailing slashes from a directory path */
void strip_trailing_slashes(char *dir)
{
   char *p;
   p = dir + strlen(dir) - 1;

   /* strip trailing slashes */
   while ((p >= dir) && (*p == '/'))
      *p-- = 0;
}

/*
 * Skip spaces
 *  Returns: 0 on failure (EOF)             
 *           1 on success
 *           new address in passed parameter 
 */
bool skip_spaces(char **msg)
{
   char *p = *msg;
   if (!p) {
      return false;
   }
   while (*p && B_ISSPACE(*p)) {
      p++;
   }
   *msg = p;
   return *p ? true : false;
}

/*
 * Skip nonspaces
 *  Returns: 0 on failure (EOF)             
 *           1 on success
 *           new address in passed parameter 
 */
bool skip_nonspaces(char **msg)
{
   char *p = *msg;

   if (!p) {
      return false;
   }
   while (*p && !B_ISSPACE(*p)) {
      p++;
   }
   *msg = p;
   return *p ? true : false;
}

/* folded search for string - case insensitive */
int
fstrsch(char *a, char *b)   /* folded case search */
{
   register char *s1,*s2;
   register char c1, c2;

   s1=a;
   s2=b;
   while (*s1) {                      /* do it the fast way */
      if ((*s1++ | 0x20) != (*s2++ | 0x20))
         return 0;                    /* failed */
   }
   while (*a) {                       /* do it over the correct slow way */
      if (B_ISUPPER(c1 = *a)) {
         c1 = tolower((int)c1);
      }
      if (B_ISUPPER(c2 = *b)) {
         c2 = tolower((int)c2);
      }
      if (c1 != c2) {
         return 0;
      }
      a++;
      b++;
   }
   return 1;
}


/* 
 * Return next argument from command line.  Note, this
 * routine is destructive.
 */
char *next_arg(char **s)
{
   char *p, *q, *n;
   bool in_quote = false;

   /* skip past spaces to next arg */
   for (p=*s; *p && B_ISSPACE(*p); ) {
      p++;
   }    
   Dmsg1(400, "Next arg=%s\n", p);
   for (n = q = p; *p ; ) {
      if (*p == '\\') {
         p++;
         if (*p) {
            *q++ = *p++;
         } else {
            *q++ = *p;
         }
         continue;
      }
      if (*p == '"') {                  /* start or end of quote */
         if (in_quote) {
            p++;                        /* skip quote */
            in_quote = false;
            continue;
         }
         in_quote = true;
         p++;
         continue;
      }
      if (!in_quote && B_ISSPACE(*p)) {     /* end of field */
         p++;
         break;
      }
      *q++ = *p++;
   }
   *q = 0;
   *s = p;
   Dmsg2(400, "End arg=%s next=%s\n", n, p);
   return n;
}   

/*
 * This routine parses the input command line.
 * It makes a copy in args, then builds an
 *  argc, argv like list where
 *    
 *  argc = count of arguments
 *  argk[i] = argument keyword (part preceding =)
 *  argv[i] = argument value (part after =)
 *
 *  example:  arg1 arg2=abc arg3=
 *
 *  argc = c
 *  argk[0] = arg1
 *  argv[0] = NULL
 *  argk[1] = arg2
 *  argv[1] = abc
 *  argk[2] = arg3
 *  argv[2] = 
 */

int parse_args(POOLMEM *cmd, POOLMEM **args, int *argc, 
               char **argk, char **argv, int max_args) 
{
   char *p, *q, *n;

   pm_strcpy(args, cmd);
   strip_trailing_junk(*args);
   p = *args;
   *argc = 0;
   /* Pick up all arguments */
   while (*argc < max_args) {
      n = next_arg(&p);   
      if (*n) {
         argk[*argc] = n;
         argv[(*argc)++] = NULL;
      } else {
         break;
      }
   }
   /* Separate keyword and value */
   for (int i=0; i < *argc; i++) {
      p = strchr(argk[i], '=');
      if (p) {
         *p++ = 0;                    /* terminate keyword and point to value */
         /* Unquote quoted values */
         if (*p == '"') {
            for (n = q = ++p; *p && *p != '"'; ) {
               if (*p == '\\') {
                  p++;
               }
               *q++ = *p++;
            }
            *q = 0;                   /* terminate string */
            p = n;                    /* point to string */
         }
         if (strlen(p) > MAX_NAME_LENGTH-1) {
            p[MAX_NAME_LENGTH-1] = 0; /* truncate to max len */
         }
      }
      argv[i] = p;                    /* save ptr to value or NULL */
   }
#ifdef xxxx
   for (int i=0; i < *argc; i++) {
      Pmsg3(000, "Arg %d: kw=%s val=%s\n", i, argk[i], argv[i]?argv[i]:"NULL");
   }
#endif
   return 1;
}

/*
 * Given a full filename, split it into its path
 *  and filename parts. They are returned in pool memory
 *  in the arguments provided.
 */
void split_path_and_filename(const char *fname, POOLMEM **path, int *pnl,
        POOLMEM **file, int *fnl)
{
   const char *p, *f;
   int slen;
   int len = slen = strlen(fname);

   /*
    * Find path without the filename.  
    * I.e. everything after the last / is a "filename".
    * OK, maybe it is a directory name, but we treat it like
    * a filename. If we don't find a / then the whole name
    * must be a path name (e.g. c:).
    */
   p = fname;
   f = fname + len - 1;
   /* "strip" any trailing slashes */
   while (slen > 0 && *f == '/') {
      slen--;
      f--;
   }
   /* Walk back to last slash -- begin of filename */
   while (slen > 0 && *f != '/') {
      slen--;
      f--;
   }
   if (*f == '/') {                   /* did we find a slash? */
      f++;                            /* yes, point to filename */
   } else {                           /* no, whole thing must be path name */
      f = fname;
   }
   Dmsg2(200, "after strip len=%d f=%s\n", len, f);
   *fnl = fname - f + len;
   if (*fnl > 0) {
      *file = check_pool_memory_size(*file, *fnl+1);
      memcpy(*file, f, *fnl);    /* copy filename */
   }
   (*file)[*fnl] = 0;

   *pnl = f - fname;
   if (*pnl > 0) {
      *path = check_pool_memory_size(*path, *pnl+1);
      memcpy(*path, fname, *pnl);
   }
   (*path)[*pnl] = 0;

   Dmsg2(200, "pnl=%d fnl=%d\n", *pnl, *fnl);
   Dmsg3(200, "split fname=%s path=%s file=%s\n", fname, *path, *file);
}
