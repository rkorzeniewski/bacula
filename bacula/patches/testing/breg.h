/*
 * Bacula BREGEXP Structure definition for FileDaemon
 * Eric Bollengier March 2007
 * Version $Id$
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


#ifndef __BREG_H_
#define __BREG_H_ 1

//#undef HAVE_REGEX_H

#ifndef HAVE_REGEX_H
#include "bregex.h"
#else
#include <regex.h>
#endif

/* Usage:
 *
 * #include "lib/breg.h"
 * 
 * BREGEXP *breg = new_bregexp("!/prod!/test!");
 * char *filename = breg->replace("/prod/data.dat");
 *   or
 * char *filename = breg->result;
 * free_bregexp(breg);
 */

/*
 * Structure for BREGEXP ressource
 */
class BREGEXP {
public:
   POOLMEM *result;		/* match result */
   char *replace(const char *fname);
   void debug();

   /* private */
   POOLMEM *expr;		/* search epression */
   POOLMEM *subst;		/* substitution */
   regex_t preg;		/* regex_t result of regcomp() */
   struct re_registers regs;	/* contains match */

   int *_regs_match;
   
   char *return_fname(const char *fname, int len); /* return fname as result */
   char *edit_subst(const char *fname, struct re_registers *regs);
   int compute_dest_len(const char *fname, struct re_registers *regs);
   bool extract_regexp(const char *motif);
};

/* create new BREGEXP and compile regex_t */
BREGEXP *new_bregexp(const char *motif); 

/* launch each bregexp on filename */
int run_bregexp(alist *bregexps, const char *fname); 

/* free BREGEXP (and all POOLMEM) */
void free_bregexp(BREGEXP *script);

/* foreach_alist free RUNSCRIPT */
void free_bregexps(alist *bregexps); /* you have to free alist */

#endif /* __BREG_H_ */
