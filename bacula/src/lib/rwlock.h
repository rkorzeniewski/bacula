/*
 * Bacula Thread Read/Write locking code. It permits
 *  multiple readers but only one writer.                 
 *
 *  Kern Sibbald, January MMI
 *
 *  This code adapted from "Programming with POSIX Threads", by
 *    David R. Butenhof
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

#ifndef __RWLOCK_H 
#define __RWLOCK_H 1

typedef struct rwlock_tag {
   pthread_mutex_t   mutex;
   pthread_cond_t    read;            /* wait for read */
   pthread_cond_t    write;           /* wait for write */
   int               valid;           /* set when valid */
   int               r_active;        /* readers active */
   int               w_active;        /* writers active */
   int               r_wait;          /* readers waiting */
   int               w_wait;          /* writers waiting */
} rwlock_t;

#define RWLOCK_VALID  0xfacade

#define RWL_INIIALIZER \
   {PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER, \
    PTHREAD_COND_INITIALIZER, RWLOCK_VALID, 0, 0, 0, 0}

/* 
 * read/write lock prototypes
 */
extern int rwl_init(rwlock_t *wrlock);
extern int rwl_destroy(rwlock_t *rwlock);
extern int rwl_readlock(rwlock_t *rwlock);
extern int rwl_readtrylock(rwlock_t *rwlock);
extern int rwl_readunlock(rwlock_t *rwlock);
extern int rwl_writelock(rwlock_t *rwlock);
extern int rwl_writetrylock(rwlock_t *rwlock);
extern int rwl_writeunlock(rwlock_t *rwlock);

#endif /* __RWLOCK_H */
