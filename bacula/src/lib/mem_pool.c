/*
 *  Bacula memory pool routines. 
 *
 *  The idea behind these routines is that there will be
 *  pools of memory that are pre-allocated for quick
 *  access. The pools will have a fixed memory size on allocation
 *  but if need be, the size can be increased. This is 
 *  particularly useful for filename
 *  buffers where 256 bytes should be sufficient in 99.99%
 *  of the cases, but when it isn't we want to be able to
 *  increase the size.
 *
 *  A major advantage of the pool memory aside from the speed
 *  is that the buffer carrys around its size, so to ensure that
 *  there is enough memory, simply call the check_pool_memory_size()
 *  with the desired size and it will adjust only if necessary.
 *
 *	     Kern E. Sibbald
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

struct s_pool_ctl {
   size_t size; 		      /* default size */
   size_t max_size;		      /* max allocated */
   size_t max_used;		      /* max buffers used */
   size_t in_use;		      /* number in use */
   struct abufhead *free_buf;	      /* pointer to free buffers */
};

static struct s_pool_ctl pool_ctl[] = {
   {  256,  256, 0, 0, NULL },	      /* PM_NOPOOL no pooling */
   {  256,  256, 0, 0, NULL },	      /* PM_FNAME filename buffers */
   {  512,  512, 0, 0, NULL },	      /* PM_MESSAGE message buffer */
   { 1024, 1024, 0, 0, NULL }	      /* PM_EMSG error message buffer */
};

/*  Memory allocation control structures and storage.  */
struct abufhead {
   size_t ablen;		      /* Buffer length in bytes */
   int32_t pool;		      /* pool */
   struct abufhead *next;	      /* pointer to next free buffer */
};

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;


#ifdef SMARTALLOC

#define HEAD_SIZE BALIGN(sizeof(struct abufhead))

extern POOLMEM *sm_malloc(char *fname, int lineno, int nbytes);

POOLMEM *sm_get_pool_memory(char *fname, int lineno, int pool)
{
   struct abufhead *buf;

   sm_check(fname, lineno, True);
   if (pool > PM_MAX) {
      Emsg2(M_ABORT, 0, "MemPool index %d larger than max %d\n", pool, PM_MAX);
   }
   P(mutex);
   if (pool_ctl[pool].free_buf) {
      buf = pool_ctl[pool].free_buf;
      pool_ctl[pool].free_buf = buf->next;
      pool_ctl[pool].in_use++;
      if (pool_ctl[pool].in_use > pool_ctl[pool].max_used) {
	 pool_ctl[pool].max_used = pool_ctl[pool].in_use;
      }
      V(mutex);
      Dmsg3(150, "sm_get_pool_memory reuse %x to %s:%d\n", buf, fname, lineno);
      sm_new_owner(fname, lineno, (char *)buf);
      return (POOLMEM *)((char *)buf+HEAD_SIZE);
   }
      
   if ((buf = (struct abufhead *) sm_malloc(fname, lineno, pool_ctl[pool].size+HEAD_SIZE)) == NULL) {
      V(mutex);
      Emsg1(M_ABORT, 0, "Out of memory requesting %d bytes\n", pool_ctl[pool].size);
   }
   buf->ablen = pool_ctl[pool].size;
   buf->pool = pool;
   pool_ctl[pool].in_use++;
   if (pool_ctl[pool].in_use > pool_ctl[pool].max_used) {
      pool_ctl[pool].max_used = pool_ctl[pool].in_use;
   }
   V(mutex);
   Dmsg3(150, "sm_get_pool_memory give %x to %s:%d\n", buf, fname, lineno);
   return (POOLMEM *)((char *)buf+HEAD_SIZE);
}

/* Get nonpool memory of size requested */
POOLMEM *sm_get_memory(char *fname, int lineno, size_t size)
{
   struct abufhead *buf;
   int pool = 0;

   sm_check(fname, lineno, True);
   if ((buf = (struct abufhead *) sm_malloc(fname, lineno, size+HEAD_SIZE)) == NULL) {
      Emsg1(M_ABORT, 0, "Out of memory requesting %d bytes\n", size);
   }
   buf->ablen = size;
   buf->pool = pool;
   buf->next = NULL;
   pool_ctl[pool].in_use++;
   if (pool_ctl[pool].in_use > pool_ctl[pool].max_used)
      pool_ctl[pool].max_used = pool_ctl[pool].in_use;
   return (POOLMEM *)(((char *)buf)+HEAD_SIZE);
}


/* Return the size of a memory buffer */
size_t sm_sizeof_pool_memory(char *fname, int lineno, POOLMEM *obuf)
{
   char *cp = (char *)obuf;

   sm_check(fname, lineno, False);
   ASSERT(obuf);
   cp -= HEAD_SIZE;
   return ((struct abufhead *)cp)->ablen;
}

/* Realloc pool memory buffer */
POOLMEM *sm_realloc_pool_memory(char *fname, int lineno, POOLMEM *obuf, size_t size)
{
   char *cp = (char *)obuf;
   void *buf;
   int pool;

   sm_check(fname, lineno, False);
   ASSERT(obuf);
   P(mutex);
   cp -= HEAD_SIZE;
   buf = realloc(cp, size+HEAD_SIZE);
   if (buf == NULL) {
      V(mutex);
      Emsg1(M_ABORT, 0, "Out of memory requesting %d bytes\n", size);
   }
   ((struct abufhead *)buf)->ablen = size;
   pool = ((struct abufhead *)buf)->pool;
   if (size > pool_ctl[pool].max_size) {
      pool_ctl[pool].max_size = size;
   }
   V(mutex);
   sm_check(fname, lineno, False);
   return (POOLMEM *)(((char *)buf)+HEAD_SIZE);
}

POOLMEM *sm_check_pool_memory_size(char *fname, int lineno, POOLMEM *obuf, size_t size)
{
   sm_check(fname, lineno, False);
   ASSERT(obuf);
   if (size <= sizeof_pool_memory(obuf)) {
      return obuf;
   }
   return realloc_pool_memory(obuf, size);
}

#else

POOLMEM *get_pool_memory(int pool)
{
   struct abufhead *buf;

   P(mutex);
   if (pool_ctl[pool].free_buf) {
      buf = pool_ctl[pool].free_buf;
      pool_ctl[pool].free_buf = buf->next;
      V(mutex);
      return (POOLMEM *)((char *)buf+HEAD_SIZE);
   }
      
   if ((buf=malloc(pool_ctl[pool].size+HEAD_SIZE)) == NULL) {
      V(mutex);
      Emsg1(M_ABORT, 0, "Out of memory requesting %d bytes\n", pool_ctl[pool].size);
   }
   buf->ablen = pool_ctl[pool].size;
   buf->pool = pool;
   buf->next = NULL;
   pool_ctl[pool].in_use++;
   if (pool_ctl[pool].in_use > pool_ctl[pool].max_used) {
      pool_ctl[pool].max_used = pool_ctl[pool].in_use;
   }
   V(mutex);
   return (POOLMEM *)(((char *)buf)+HEAD_SIZE);
}

/* Get nonpool memory of size requested */
POOLMEM *get_memory(size_t size)
{
   struct abufhead *buf;
   int pool = 0;

   if ((buf=malloc(size+HEAD_SIZE)) == NULL) {
      Emsg1(M_ABORT, 0, "Out of memory requesting %d bytes\n", size);
   }
   buf->ablen = size;
   buf->pool = pool;
   buf->next = NULL;
   pool_ctl[pool].in_use++;
   if (pool_ctl[pool].in_use > pool_ctl[pool].max_used) {
      pool_ctl[pool].max_used = pool_ctl[pool].in_use;
   }
   return (POOLMEM *)(((char *)buf)+HEAD_SIZE);
}


/* Return the size of a memory buffer */
size_t sizeof_pool_memory(POOLMEM *obuf)
{
   char *cp = (char *)obuf;

   ASSERT(obuf);
   cp -= HEAD_SIZE;
   return ((struct abufhead *)cp)->ablen;
}

/* Realloc pool memory buffer */
POOLMEM *realloc_pool_memory(POOLMEM *obuf, size_t size)
{
   char *cp = (char *)obuf;
   void *buf;
   int pool;

   ASSERT(obuf);
   P(mutex);
   cp -= HEAD_SIZE;
   buf = realloc(cp, size+HEAD_SIZE);
   if (buf == NULL) {
      V(mutex);
      Emsg1(M_ABORT, 0, "Out of memory requesting %d bytes\n", size);
   }
   ((struct abufhead *)buf)->ablen = size;
   pool = ((struct abufhead *)buf)->pool;
   if (size > pool_ctl[pool].max_size) {
      pool_ctl[pool].max_size = size;
   }
   V(mutex);
   return (POOLMEM *)(((char *)buf)+HEAD_SIZE);
}

POOLMEM *check_pool_memory_size(POOLMEM *obuf, size_t size)
{
   ASSERT(obuf);
   if (size <= sizeof_pool_memory(obuf)) {
      return obuf;
   }
   return realloc_pool_memory(obuf, size);
}

#endif /* SMARTALLOC */



/* Free a memory buffer */
void free_pool_memory(POOLMEM *obuf)
{
   struct abufhead *buf;
   int pool;

   sm_check(__FILE__, __LINE__, False);
   ASSERT(obuf);
   P(mutex);
   buf = (struct abufhead *)((char *)obuf - HEAD_SIZE);
   pool = buf->pool;
   pool_ctl[pool].in_use--;
   if (pool == 0) {
      free((char *)buf);	      /* free nonpooled memory */
   } else {			      /* otherwise link it to the free pool chain */
#ifdef DEBUG
      struct abufhead *next;
      /* Don't let him free the same buffer twice */
      for (next=pool_ctl[pool].free_buf; next; next=next->next) {
	 ASSERT(next != buf);  /* attempt to free twice */
      }
#endif
      buf->next = pool_ctl[pool].free_buf;
      pool_ctl[pool].free_buf = buf;
   }
   Dmsg2(150, "free_pool_memory %x pool=%d\n", buf, pool);
   V(mutex);
}



/* Release all pooled memory */
void close_memory_pool()
{
   struct abufhead *buf, *next;
   int i;

   sm_check(__FILE__, __LINE__, False);
   P(mutex);
   for (i=1; i<=PM_MAX; i++) {
      buf = pool_ctl[i].free_buf;
      while (buf) {
	 next = buf->next;
	 free((char *)buf);
	 buf = next;
      }
      pool_ctl[i].free_buf = NULL;
   }
   V(mutex);
}

#ifdef DEBUG

static char *pool_name(int pool)
{
   static char *name[] = {"NoPool", "FNAME ", "MSG   ", "EMSG  "};
   static char buf[30];

   if (pool >= 0 && pool <= PM_MAX) {
      return name[pool];
   }
   sprintf(buf, "%-6d", pool);
   return buf;
}
   
/* Print staticstics on memory pool usage   
 */ 
void print_memory_pool_stats()
{
   int i;

   Dmsg0(-1, "Pool   Maxsize  Maxused  Inuse\n");
   for (i=0; i<=PM_MAX; i++)
      Dmsg4(-1, "%5s  %7d  %7d  %5d\n", pool_name(i), pool_ctl[i].max_size,
	 pool_ctl[i].max_used, pool_ctl[i].in_use);

   Dmsg0(-1, "\n");
}

#else
void print_memory_pool_stats() {} 
#endif /* DEBUG */
