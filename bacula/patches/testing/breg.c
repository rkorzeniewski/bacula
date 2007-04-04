/*
 * Manipulation routines for BREGEXP list
 *
 *  Eric Bollengier, March 2007
 *
 *  Version $Id$
 *
 */
/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2006-2006 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from
   many others, a complete list can be found in the file AUTHORS.
   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version two of the GNU General Public
   License as published by the Free Software Foundation plus additions
   that are listed in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

   Bacula® is a registered trademark of John Walker.
   The licensor of Bacula is the Free Software Foundation Europe
   (FSFE), Fiduciary Program, Sumatrastrasse 25, 8006 Zürich,
   Switzerland, email:ftf@fsfeurope.org.
*/


#include "bacula.h"

#include "breg.h"
#include "mem_pool.h"

BREGEXP *new_bregexp(const char *motif)
{
   Dmsg0(500, "bregexp: creating new bregexp object\n");
   BREGEXP *self = (BREGEXP *)malloc(sizeof(BREGEXP));
   memset(self, 0, sizeof(BREGEXP));
   
   if (!self->extract_regexp(motif)) {
      Dmsg0(100, "bregexp: extract_regexp error\n");
      free_bregexp(self);
      return NULL;
   }

   self->result = get_pool_memory(PM_FNAME);
   self->result[0] = '\0';

#ifdef HAVE_REGEX_H
   /* TODO: que devient cette memoire... */
   self->_regs_match = (int *) malloc (2*RE_NREGS * sizeof(int));

   self->regs.num_regs = RE_NREGS;
   self->regs.start = self->_regs_match;
   self->regs.end   = self->_regs_match+(RE_NREGS * sizeof(int));
#endif 

   return self;
}

void free_bregexp(BREGEXP *self)
{
   Dmsg0(500, "bregexp: freeing BREGEXP object\n");

   if (self->expr) {
      free(self->expr);
   }
   if (self->result) {
      free_pool_memory(self->result);
   }
   if (self->_regs_match) {
      free(self->_regs_match);
   }

   regfree(&self->preg);
   free(self);
}

/* Free a bregexps alist
 */
void free_bregexps(alist *bregexps)
{
   Dmsg0(500, "bregexp: freeing all BREGEXP object\n");

   BREGEXP *elt;
   foreach_alist(elt, bregexps) {
      free_bregexp(elt);
   }
}

/* Apply all regexps to fname
 */
bool apply_bregexps(const char *fname, alist *bregexps, char **result)
{
   BREGEXP *elt;
   bool ok=false;

   char *ret = (char *) fname;
   foreach_alist(elt, bregexps) {
      ret = elt->replace(ret);
      ok = ok || elt->success;
   }
   Dmsg2(500, "bregexp: fname=%s ret=%s\n", fname, ret);

   *result = ret;
   return ok;
}

/* return an alist of BREGEXP or return NULL if it's not a 
 * where=!tmp!opt!ig,!temp!opt!i
 */
alist *get_bregexps(const char *where)
{
   char *p = (char *)where;
   alist *list = New(alist(10, not_owned_by_alist));
   BREGEXP *reg;

   reg = new_bregexp(p);

   while(reg) {
      p = reg->eor;
      list->append(reg);
      reg = new_bregexp(p);
   }

   if (list->size()) {
      return list;      
   } else {
      delete list;
      return NULL;
   }
}

bool BREGEXP::extract_regexp(const char *motif)
{
   if ( !motif ) {
      return false;
   }
   
   char sep = motif[0];

   if (!(sep == '!' || 
	 sep == ':' || 
	 sep == ';' || 
	 sep == '|' || 
	 sep == ',' || 
	 sep == '&' || 
	 sep == '%' || 
	 sep == '=' || 
	 sep == '~' ||
	 sep == '#'   )) 
   {
      return false;
   }

   char *search = (char *) motif + 1;
   int options = REG_EXTENDED | REG_NEWLINE;
   bool ok = false;

   /* extract 1st part */
   char *dest = expr = bstrdup(motif);

   while (*search && !ok) {
      if (search[0] == '\\' && search[1] == sep) {
	 *dest++ = *++search;       /* we skip separator */ 

      } else if (search[0] == '\\' && search[1] == '\\') {
	 *dest++ = *++search;	    /* we skip the second \ */

      } else if (*search == sep) {  /* we found end of expression */
	 *dest++ = '\0';

	 if (subst) {	        /* already have found motif */
	    ok = true;

	 } else {
	    *dest++ = *++search; /* we skip separator */ 
	    subst = dest;	 /* get replaced string */
	 }

      } else {
	 *dest++ = *search++;
      }
   }
   *dest = '\0';		/* in case of */
   
   if (!ok || !subst) {
      /* bad regexp */
      return false;
   }

   ok = false;
   /* find options */
   while (*search && !ok) {
      if (*search == 'i') {
	 options |= REG_ICASE;

      } else if (*search == 'g') {
	      /* recherche multiple*/

      } else if (*search == sep) {
	 /* skip separator */

      } else {			/* end of options */
	 ok = true;
      }
      search++;
   }

   int rc = regcomp(&preg, expr, options);
   if (rc != 0) {
      char prbuf[500];
      regerror(rc, &preg, prbuf, sizeof(prbuf));
      Dmsg1(100, "bregexp: compile error: %s\n", prbuf);
      return false;
   }

   eor = search;		/* useful to find the next regexp in where */

   return true;
}

#ifndef HAVE_REGEX_H
 #define BREGEX_CAST unsigned
#else
 #define BREGEX_CAST const
#endif

/* return regexp->result */
char *BREGEXP::replace(const char *fname)
{
   success = false;		/* use this.success to known if it's ok */
   int flen = strlen(fname);
   int rc = re_search(&preg, (BREGEX_CAST char*) fname, flen, 0, flen, &regs);

   if (rc < 0) {
      Dmsg0(500, "bregexp: regex mismatch\n");
      return return_fname(fname, flen);
   }

   int len = compute_dest_len(fname, &regs);

   if (len) {
      result = check_pool_memory_size(result, len);
      edit_subst(fname, &regs);
      success = true;
      Dmsg2(500, "bregexp: len = %i, result_len = %i\n", len, strlen(result));

   } else {			/* error in substitution */
      Dmsg0(100, "bregexp: error in substitution\n");
      return return_fname(fname, flen);
   }

   return result;
} 

char *BREGEXP::return_fname(const char *fname, int len)
{
   result = check_pool_memory_size(result, len+1);
   strcpy(result,fname);
   return result;
}

int BREGEXP::compute_dest_len(const char *fname, struct re_registers *regs)
{
   int len=0;
   char *p;
   char *psubst = subst;
   int no;

   if (!fname || !regs) {
      return 0;
   }

   /* match failed ? */
   if (regs->start[0] < 0) {
      return 0;
   }

   for (p = psubst++; *p ; p = psubst++) {
      /* match $1 \1 back references */
      if ((*p == '$' || *p == '\\') && ('0' <= *psubst && *psubst <= '9')) {
	 no = *psubst++ - '0';

	 /* we check if the back reference exists */
	 /* references can not match if we are using (..)? */

	 if (regs->start[no] >= 0 && regs->end[no] >= 0) { 
	    len += regs->end[no] - regs->start[no];
	 }
	 
      } else {
	 len++;
      }
   }

   /* $0 is replaced by subst */
   len -= regs->end[0] - regs->start[0];
   len += strlen(fname) + 1;

   return len;
}

char *BREGEXP::edit_subst(const char *fname, struct re_registers *regs)
{
   int i;
   char *p;
   char *psubst = subst;
   int no;
   int len;

   /* il faut recopier fname dans dest
    *  on recopie le debut fname -> regs->start[0]
    */
   
   for (i = 0; i < regs->start[0] ; i++) {
      result[i] = fname[i];
   }

   /* on recopie le motif de remplacement (avec tous les $x) */

   for (p = psubst++; *p ; p = psubst++) {
      /* match $1 \1 back references */
      if ((*p == '$' || *p == '\\') && ('0' <= *psubst && *psubst <= '9')) {
	 no = *psubst++ - '0';

	 /* have a back reference ? */
	 if (regs->start[no] >= 0 && regs->end[no] >= 0) {
	    len = regs->end[no] - regs->start[no];
	    bstrncpy(result + i, fname + regs->start[no], len + 1);
	    i += len ;
	 }

      } else {
	 result[i++] = *p;
      }
   }

   /* we copy what is out of the match */
   strcpy(result + i, fname + regs->end[0]);

   return result;
}

/* escape sep char and \
 * dest must be long enough (src*2+1)
 * return end of the string */
char *bregexp_escape_string(char *dest, char *src, char sep)
{
   char *ret = dest;
   while (*src)
   {
      if (*src == sep) {
	 *dest++ = '\\';
      } else if (*src == '\\') {
	 *dest++ = '\\';
      }
      *dest++ = *src++;
   }
   *dest = '\0';

   return ret; 
}

/* build a regexp string with user arguments
 * don't forget to free ret 
 */
char *bregexp_build_where(char *strip_prefix, 
                          char *add_prefix, 
                          char *add_suffix)
{
   /* strip_prefix = !strip_prefix!!         4 bytes
    * add_prefix   = !^!add_prefix!          5 bytes
    * add_suffix   = !([^/])$!$1.add_suffix! 14 bytes
    */
   int len=0;
   char sep = '!';
   int str_size = (strlen(strip_prefix) + 4 +
		   strlen(add_prefix)   + 5 +
		   strlen(add_suffix)   + 14) * 2  + 1;

   POOLMEM *ret = get_memory(str_size);
   POOLMEM *str_tmp = get_memory(str_size);

   *str_tmp = *ret = '\0';
   
   if (strip_prefix) {
      len += bsnprintf(ret, str_size - len, "!%s!!",
		       bregexp_escape_string(str_tmp, strip_prefix, sep));
   }

   if (add_suffix) {
      if (len) ret[len++] = ',';

      len += bsnprintf(ret + len,  str_size - len, "!([^/])$!$1%s!",
		       bregexp_escape_string(str_tmp, add_suffix, sep));
   }

   if (add_prefix) {
      if (len) ret[len++] = ',';

      len += bsnprintf(ret + len, str_size - len, "!^!%s!", 
		       bregexp_escape_string(str_tmp, add_prefix, sep));
   }

   return ret;
}


void BREGEXP::debug()
{
   printf("expr=[%s]\n", expr);
   printf("subst=[%s]\n", subst);
   printf("result=%s\n", result?result:"(null)");
}
