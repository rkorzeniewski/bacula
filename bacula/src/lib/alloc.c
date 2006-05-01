/*

        Error checking memory allocator

     Version $Id$
*/

/*
   Copyright (C) 2000-2006 Kern Sibbald

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   version 2 as amended with additional clauses defined in the
   file LICENSE in the main source directory.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the 
   the file LICENSE for additional details.

 */

#ifdef NEEDED

#include <stdio.h>
#include <stdlib.h>

#ifdef TESTERR
#undef NULL
#define NULL  buf
#endif

/*LINTLIBRARY*/

#define V        (void)

#ifdef SMARTALLOC

extern void *sm_malloc();

/*  SM_ALLOC  --  Allocate buffer and signal on error  */

void *sm_alloc(char *fname, int lineno, unsigned int nbytes)
{
   void *buf;

   if ((buf = sm_malloc(fname, lineno, nbytes)) != NULL) {
      return buf;
   }
   V fprintf(stderr, "\nBoom!!!  Memory capacity exceeded.\n");
   V fprintf(stderr, "  Requested %u bytes at line %d of %s.\n",
      nbytes, lineno, fname);
   abort();
   /*NOTREACHED*/
}
#else

/*  ALLOC  --  Allocate buffer and signal on error  */

void *alloc(unsigned int nbytes)
{
   void *buf;

   if ((buf = malloc(nbytes)) != NULL) {
      return buf;
   }
   V fprintf(stderr, "\nBoom!!!  Memory capacity exceeded.\n");
   V fprintf(stderr, "  Requested %u bytes.\n", nbytes);
   abort();
   /*NOTREACHED*/
}
#endif
#endif
