/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2000-2014 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from many
   others, a complete list can be found in the file AUTHORS.

   You may use this file and others of this release according to the
   license defined in the LICENSE file, which includes the Affero General
   Public License, v3.0 ("AGPLv3") and some additional permissions and
   terms pursuant to its AGPLv3 Section 7.

   Bacula® is a registered trademark of Kern Sibbald.
*/
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
 *           Kern E. Sibbald
 *
 */

#include "bacula.h"
#define dbglvl DT_MEMORY|800

#ifdef HAVE_MALLOC_TRIM
extern "C" int malloc_trim (size_t pad);
#endif

struct s_pool_ctl {
   int32_t size;                      /* default size */
   int32_t max_allocated;             /* max allocated */
   int32_t max_used;                  /* max buffers used */
   int32_t in_use;                    /* number in use */
   struct abufhead *free_buf;         /* pointer to free buffers */
};

/* Bacula Name length plus extra */
#define NLEN (MAX_NAME_LENGTH+2)

/* #define STRESS_TEST_POOL */
#ifndef STRESS_TEST_POOL
/*
 * Define default Pool buffer sizes
 */
static struct s_pool_ctl pool_ctl[] = {
   {  256,  256, 0, 0, NULL },        /* PM_NOPOOL no pooling */
   {  NLEN, NLEN,0, 0, NULL },        /* PM_NAME Bacula name */
   {  256,  256, 0, 0, NULL },        /* PM_FNAME filename buffers */
   {  512,  512, 0, 0, NULL },        /* PM_MESSAGE message buffer */
   { 1024, 1024, 0, 0, NULL },        /* PM_EMSG error message buffer */
  {  4096, 4096, 0, 0, NULL }         /* PM_BSOCK message buffer */
};
#else

/* This is used ONLY when stress testing the code */
static struct s_pool_ctl pool_ctl[] = {
   {   20,   20, 0, 0, NULL },        /* PM_NOPOOL no pooling */
   {  NLEN, NLEN,0, 0, NULL },        /* PM_NAME Bacula name */
   {   20,   20, 0, 0, NULL },        /* PM_FNAME filename buffers */
   {   20,   20, 0, 0, NULL },        /* PM_MESSAGE message buffer */
   {   20,   20, 0, 0, NULL },        /* PM_EMSG error message buffer */
   {   20,   20, 0, 0, NULL }         /* PM_BSOCK message buffer */
};
#endif


/*  Memory allocation control structures and storage.  */
struct abufhead {
   int32_t ablen;                     /* Buffer length in bytes */
   int32_t pool;                      /* pool */
   struct abufhead *next;             /* pointer to next free buffer */
   int32_t bnet_size;                 /* dummy for bnet_send() */
};

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

#ifdef SMARTALLOC

#define HEAD_SIZE BALIGN(sizeof(struct abufhead))

POOLMEM *sm_get_pool_memory(const char *fname, int lineno, int pool)
{
   struct abufhead *buf;

   if (pool > PM_MAX) {
      Emsg2(M_ABORT, 0, _("MemPool index %d larger than max %d\n"), pool, PM_MAX);
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
      Dmsg3(dbglvl, "sm_get_pool_memory reuse %p to %s:%d\n", buf, fname, lineno);
      sm_new_owner(fname, lineno, (char *)buf);
      return (POOLMEM *)((char *)buf+HEAD_SIZE);
   }

   if ((buf = (struct abufhead *)sm_malloc(fname, lineno, pool_ctl[pool].size+HEAD_SIZE)) == NULL) {
      V(mutex);
      Emsg1(M_ABORT, 0, _("Out of memory requesting %d bytes\n"), pool_ctl[pool].size);
   }
   buf->ablen = pool_ctl[pool].size;
   buf->pool = pool;
   pool_ctl[pool].in_use++;
   if (pool_ctl[pool].in_use > pool_ctl[pool].max_used) {
      pool_ctl[pool].max_used = pool_ctl[pool].in_use;
   }
   V(mutex);
   Dmsg3(dbglvl, "sm_get_pool_memory give %p to %s:%d\n", buf, fname, lineno);
   return (POOLMEM *)((char *)buf+HEAD_SIZE);
}

/* Get nonpool memory of size requested */
POOLMEM *sm_get_memory(const char *fname, int lineno, int32_t size)
{
   struct abufhead *buf;
   int pool = 0;

   if ((buf = (struct abufhead *)sm_malloc(fname, lineno, size+HEAD_SIZE)) == NULL) {
      Emsg1(M_ABORT, 0, _("Out of memory requesting %d bytes\n"), size);
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
int32_t sm_sizeof_pool_memory(const char *fname, int lineno, POOLMEM *obuf)
{
   char *cp = (char *)obuf;

   if (obuf == NULL) {
      Emsg0(M_ABORT, 0, _("obuf is NULL\n"));
   }
   cp -= HEAD_SIZE;
   return ((struct abufhead *)cp)->ablen;
}

/* Realloc pool memory buffer */
POOLMEM *sm_realloc_pool_memory(const char *fname, int lineno, POOLMEM *obuf, int32_t size)
{
   char *cp = (char *)obuf;
   void *buf;
   int pool;

   ASSERT(obuf);
   P(mutex);
   cp -= HEAD_SIZE;
   buf = sm_realloc(fname, lineno, cp, size+HEAD_SIZE);
   if (buf == NULL) {
      V(mutex);
      Emsg1(M_ABORT, 0, _("Out of memory requesting %d bytes\n"), size);
   }
   ((struct abufhead *)buf)->ablen = size;
   pool = ((struct abufhead *)buf)->pool;
   if (size > pool_ctl[pool].max_allocated) {
      pool_ctl[pool].max_allocated = size;
   }
   V(mutex);
   return (POOLMEM *)(((char *)buf)+HEAD_SIZE);
}

POOLMEM *sm_check_pool_memory_size(const char *fname, int lineno, POOLMEM *obuf, int32_t size)
{
   ASSERT(obuf);
   if (size <= sizeof_pool_memory(obuf)) {
      return obuf;
   }
   return realloc_pool_memory(obuf, size);
}

/* Free a memory buffer */
void sm_free_pool_memory(const char *fname, int lineno, POOLMEM *obuf)
{
   struct abufhead *buf;
   int pool;

   ASSERT(obuf);
   P(mutex);
   buf = (struct abufhead *)((char *)obuf - HEAD_SIZE);
   pool = buf->pool;
   pool_ctl[pool].in_use--;
   if (pool == 0) {
      free((char *)buf);              /* free nonpooled memory */
   } else {                           /* otherwise link it to the free pool chain */

   /* Disabled because it hangs in #5507 */
#ifdef xDEBUG
      struct abufhead *next;
      /* Don't let him free the same buffer twice */
      for (next=pool_ctl[pool].free_buf; next; next=next->next) {
         if (next == buf) {
            Dmsg4(dbglvl, "free_pool_memory %p pool=%d from %s:%d\n", buf, pool, fname, lineno);
            Dmsg4(dbglvl, "bad free_pool_memory %p pool=%d from %s:%d\n", buf, pool, fname, lineno);
            V(mutex);                 /* unblock the pool */
            ASSERT(next != buf);      /* attempt to free twice */
         }
      }
#endif
      buf->next = pool_ctl[pool].free_buf;
      pool_ctl[pool].free_buf = buf;
   }
   Dmsg4(dbglvl, "free_pool_memory %p pool=%d from %s:%d\n", buf, pool, fname, lineno);
   V(mutex);
}

#else

/* =========  NO SMARTALLOC  =========================================  */

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
      Emsg1(M_ABORT, 0, _("Out of memory requesting %d bytes\n"), pool_ctl[pool].size);
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
POOLMEM *get_memory(int32_t size)
{
   struct abufhead *buf;
   int pool = 0;

   if ((buf=malloc(size+HEAD_SIZE)) == NULL) {
      Emsg1(M_ABORT, 0, _("Out of memory requesting %d bytes\n"), size);
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
int32_t sizeof_pool_memory(POOLMEM *obuf)
{
   char *cp = (char *)obuf;

   ASSERT(obuf);
   cp -= HEAD_SIZE;
   return ((struct abufhead *)cp)->ablen;
}

/* Realloc pool memory buffer */
POOLMEM *realloc_pool_memory(POOLMEM *obuf, int32_t size)
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
      Emsg1(M_ABORT, 0, _("Out of memory requesting %d bytes\n"), size);
   }
   ((struct abufhead *)buf)->ablen = size;
   pool = ((struct abufhead *)buf)->pool;
   if (size > pool_ctl[pool].max_allocated) {
      pool_ctl[pool].max_allocated = size;
   }
   V(mutex);
   return (POOLMEM *)(((char *)buf)+HEAD_SIZE);
}

POOLMEM *check_pool_memory_size(POOLMEM *obuf, int32_t size)
{
   ASSERT(obuf);
   if (size <= sizeof_pool_memory(obuf)) {
      return obuf;
   }
   return realloc_pool_memory(obuf, size);
}

/* Free a memory buffer */
void free_pool_memory(POOLMEM *obuf)
{
   struct abufhead *buf;
   int pool;

   ASSERT(obuf);
   P(mutex);
   buf = (struct abufhead *)((char *)obuf - HEAD_SIZE);
   pool = buf->pool;
   pool_ctl[pool].in_use--;
   if (pool == 0) {
      free((char *)buf);              /* free nonpooled memory */
   } else {                           /* otherwise link it to the free pool chain */
#ifdef DEBUG
      struct abufhead *next;
      /* Don't let him free the same buffer twice */
      for (next=pool_ctl[pool].free_buf; next; next=next->next) {
         if (next == buf) {
            V(mutex);
            ASSERT(next != buf);  /* attempt to free twice */
         }
      }
#endif
      buf->next = pool_ctl[pool].free_buf;
      pool_ctl[pool].free_buf = buf;
   }
   Dmsg2(dbglvl, "free_pool_memory %p pool=%d\n", buf, pool);
   V(mutex);
}
#endif /* SMARTALLOC */

/*
 * Clean up memory pool periodically
 *
 */
static time_t last_garbage_collection = 0;
const int garbage_interval = 24 * 60 * 60;  /* garbage collect every 24 hours */

void garbage_collect_memory_pool()
{
   time_t now;

   Dmsg0(200, "garbage collect memory pool\n");
   P(mutex);
   if (last_garbage_collection == 0) {
      last_garbage_collection = time(NULL);
      V(mutex);
      return;
   }
   now = time(NULL);
   if (now >= last_garbage_collection + garbage_interval) {
      last_garbage_collection = now;
      V(mutex);
      garbage_collect_memory();
   } else {
      V(mutex);
   }
}

/* Release all freed pooled memory */
void close_memory_pool()
{
   struct abufhead *buf, *next;
   int count = 0;
   uint64_t bytes = 0;
   char ed1[50];

   sm_check(__FILE__, __LINE__, false);
   P(mutex);
   for (int i=1; i<=PM_MAX; i++) {
      buf = pool_ctl[i].free_buf;
      while (buf) {
         next = buf->next;
         count++;
         bytes += sizeof_pool_memory((char *)buf);
         free((char *)buf);
         buf = next;
      }
      pool_ctl[i].free_buf = NULL;
   }
   Dmsg2(DT_MEMORY|001, "Freed mem_pool count=%d size=%s\n", count, edit_uint64_with_commas(bytes, ed1));
   if (chk_dbglvl(DT_MEMORY|1)) {
      print_memory_pool_stats();
   }
   V(mutex);

}

/*
 * Garbage collect and trim memory if possible
 *  This should be called after all big memory usages
 *  if possible.
 */
void garbage_collect_memory()
{
   close_memory_pool();         /* release free chain */
#ifdef HAVE_MALLOC_TRIM
   P(mutex);
   malloc_trim(8192);
   V(mutex);
#endif
}

#ifdef DEBUG
static const char *pool_name(int pool)
{
   static const char *name[] = {"NoPool", "NAME  ", "FNAME ", "MSG   ", "EMSG  ", "BSOCK "};
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
   Pmsg0(-1, "Pool   Maxsize  Maxused  Inuse\n");
   for (int i=0; i<=PM_MAX; i++)
      Pmsg4(-1, "%5s  %7d  %7d  %5d\n", pool_name(i), pool_ctl[i].max_allocated,
         pool_ctl[i].max_used, pool_ctl[i].in_use);

   Pmsg0(-1, "\n");
}

#else
void print_memory_pool_stats() {}
#endif /* DEBUG */

/*
 * Concatenate a string (str) onto a pool memory buffer pm
 *   Returns: length of concatenated string
 */
int pm_strcat(POOLMEM **pm, const char *str)
{
   int pmlen = strlen(*pm);
   int len;

   if (!str) str = "";

   len = strlen(str) + 1;
   *pm = check_pool_memory_size(*pm, pmlen + len);
   memcpy(*pm+pmlen, str, len);
   return pmlen + len - 1;
}

int pm_strcat(POOLMEM *&pm, const char *str)
{
   int pmlen = strlen(pm);
   int len;

   if (!str) str = "";

   len = strlen(str) + 1;
   pm = check_pool_memory_size(pm, pmlen + len);
   memcpy(pm+pmlen, str, len);
   return pmlen + len - 1;
}

int pm_strcat(POOLMEM *&pm, POOL_MEM &str)
{
   int pmlen = strlen(pm);
   int len = strlen(str.c_str()) + 1;

   pm = check_pool_memory_size(pm, pmlen + len);
   memcpy(pm+pmlen, str.c_str(), len);
   return pmlen + len - 1;
}

int pm_strcat(POOL_MEM &pm, const char *str)
{
   int pmlen = strlen(pm.c_str());
   int len;

   if (!str) str = "";

   len = strlen(str) + 1;
   pm.check_size(pmlen + len);
   memcpy(pm.c_str()+pmlen, str, len);
   return pmlen + len - 1;
}

/*
 * Copy a string (str) into a pool memory buffer pm
 *   Returns: length of string copied
 */
int pm_strcpy(POOLMEM **pm, const char *str)
{
   int len;

   if (!str) str = "";

   len = strlen(str) + 1;
   *pm = check_pool_memory_size(*pm, len);
   memcpy(*pm, str, len);
   return len - 1;
}

int pm_strcpy(POOLMEM *&pm, const char *str)
{
   int len;

   if (!str) str = "";

   len = strlen(str) + 1;
   pm = check_pool_memory_size(pm, len);
   memcpy(pm, str, len);
   return len - 1;
}

int pm_strcpy(POOLMEM *&pm, POOL_MEM &str)
{
   int len = strlen(str.c_str()) + 1;

   pm = check_pool_memory_size(pm, len);
   memcpy(pm, str.c_str(), len);
   return len - 1;
}

int pm_strcpy(POOL_MEM &pm, const char *str)
{
   int len;

   if (!str) str = "";

   len = strlen(str) + 1;
   pm.check_size(len);
   memcpy(pm.c_str(), str, len);
   return len - 1;
}

/*
 * Copy data into a pool memory buffer pm
 *   Returns: length of data copied
 */
int pm_memcpy(POOLMEM **pm, const char *data, int32_t n)
{
   *pm = check_pool_memory_size(*pm, n);
   memcpy(*pm, data, n);
   return n;
}

int pm_memcpy(POOLMEM *&pm, const char *data, int32_t n)
{
   pm = check_pool_memory_size(pm, n);
   memcpy(pm, data, n);
   return n;
}

int pm_memcpy(POOLMEM *&pm, POOL_MEM &data, int32_t n)
{
   pm = check_pool_memory_size(pm, n);
   memcpy(pm, data.c_str(), n);
   return n;
}

int pm_memcpy(POOL_MEM &pm, const char *data, int32_t n)
{
   pm.check_size(n);
   memcpy(pm.c_str(), data, n);
   return n;
}

/* ==============  CLASS POOL_MEM   ============== */

/* Return the size of a memory buffer */
int32_t POOL_MEM::max_size()
{
   int32_t size;
   char *cp = mem;
   cp -= HEAD_SIZE;
   size = ((struct abufhead *)cp)->ablen;
   Dmsg1(900, "max_size=%d\n", size);
   return size;
}

void POOL_MEM::realloc_pm(int32_t size)
{
   char *cp = mem;
   char *buf;
   int pool;

   P(mutex);
   cp -= HEAD_SIZE;
   buf = (char *)realloc(cp, size+HEAD_SIZE);
   if (buf == NULL) {
      V(mutex);
      Emsg1(M_ABORT, 0, _("Out of memory requesting %d bytes\n"), size);
   }
   Dmsg2(900, "Old buf=%p new buf=%p\n", cp, buf);
   ((struct abufhead *)buf)->ablen = size;
   pool = ((struct abufhead *)buf)->pool;
   if (size > pool_ctl[pool].max_allocated) {
      pool_ctl[pool].max_allocated = size;
   }
   mem = buf+HEAD_SIZE;
   V(mutex);
   Dmsg3(900, "Old buf=%p new buf=%p mem=%p\n", cp, buf, mem);
}

int POOL_MEM::strcat(const char *str)
{
   int pmlen = strlen(mem);
   int len;

   if (!str) str = "";

   len = strlen(str) + 1;
   check_size(pmlen + len);
   memcpy(mem+pmlen, str, len);
   return pmlen + len - 1;
}

int POOL_MEM::strcpy(const char *str)
{
   int len;

   if (!str) str = "";

   len = strlen(str) + 1;
   check_size(len);
   memcpy(mem, str, len);
   return len - 1;
}
