/*
 * Memory Pool prototypes
 *
 *  Kern Sibbald, 2000
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

#ifdef SMARTALLOC

#define get_pool_memory(pool) sm_get_pool_memory(__FILE__, __LINE__, pool)
extern POOLMEM *sm_get_pool_memory(char *file, int line, int pool);
#define get_memory(size) sm_get_memory(__FILE__, __LINE__, size)
extern POOLMEM *sm_get_memory(char *fname, int line, size_t size);

#else

extern POOLMEM *get_pool_memory(int pool);
extern POOLMEM *get_memory(size_t size);

#endif
 
#define free_memory(x) free_pool_memory(x)
extern void   free_pool_memory(POOLMEM *buf);
extern size_t sizeof_pool_memory(POOLMEM *buf);
extern POOLMEM	*realloc_pool_memory(POOLMEM *buf, size_t size);
extern POOLMEM	*check_pool_memory_size(POOLMEM *buf, size_t size);
extern void  close_memory_pool();
extern void  print_memory_pool_stats();

#define PM_NOPOOL  0		      /* nonpooled memory */
#define PM_FNAME   1		      /* file name buffer */
#define PM_MESSAGE 2		      /* daemon message */
#define PM_EMSG    3		      /* error message */
#define PM_MAX	   PM_EMSG	      /* Number of types */
