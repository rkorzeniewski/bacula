/*
 * Bacula Thread Read/Write locking code. It permits
 *  multiple readers but only one writer.		  
 *
 *  Kern Sibbald, January MMI
 *
 *   Version $Id$
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

#include "bacula.h"

#ifdef REALLY_IMPLEMENTED

/*   
 * Initialize a read/write lock
 *
 *  Returns: 0 on success
 *	     errno on failure
 */
int rwl_init(rwlock_t *rwl)
{
   int stat;
			
   rwl->r_active = rwl->w_active = 0;
   rwl->r_wait = rwl->w_wait = 0;
   if ((stat = pthread_mutex_init(&rwl->mutex, NULL)) != 0) {
      return stat;
   }
   if ((stat = pthread_cond_init(&rwl->read, NULL)) != 0) {
      pthread_mutex_destroy(&rwl->mutex);
      return stat;
   }
   if ((stat = pthread_cond_init(&rwl->write, NULL)) != 0) {
      pthread_cond_destroy(&rwl->read);
      pthread_mutex_destroy(&rwl->mutex);
      return stat;
   }
   rwl->valid = RWLOCK_VALID;
   return 0;
}

/*
 * Destroy a read/write lock
 *
 * Returns: 0 on success
 *	    errno on failure
 */
int rwl_destroy(rwlock_t *rwl)
{
   int stat, stat1, stat2;

  if (rwl->valid != RWLOCK_VALID) {
     return EINVAL;
  }
  if ((stat = pthread_mutex_lock(&rwl->mutex)) != 0) {
     return stat;
  }

  /* 
   * If any threads are active, report EBUSY
   */
  if (rwl->r_active > 0 || rwl->w_active) {
     pthread_mutex_unlock(&rwl->mutex);
     return EBUSY;
  }

  /*
   * If any threads are waiting, report EBUSY
   */
  if (rwl->r_wait > 0 || rwl->w_wait > 0) { 
     pthread_mutex_unlock(&rwl->mutex);
     return EBUSY;
  }

  rwl->valid = 0;
  if ((stat = pthread_mutex_unlock(&rwl->mutex)) != 0) {
     return stat;
  }
  stat	= pthread_mutex_destroy(&rwl->mutex);
  stat1 = pthread_cond_destroy(&rwl->read);
  stat2 = pthread_cond_destroy(&rwl->write);
  return (stat != 0 ? stat : (stat1 != 0 ? stat1 : stat2));
}

/*
 * Handle cleanup when the read lock condition variable
 * wait is released.
 */
static void rwl_read_release(void *arg)
{
   rwlock_t *rwl = (rwlock_t *)arg;

   rwl->r_wait--;
   pthread_mutex_unlock(&rwl->mutex);
}

/*
 * Handle cleanup when the write lock condition variable wait
 * is released.
 */
static void rwl_write_release(void *arg)
{
   rwlock_t *rwl = (rwlock_t *)arg;

   rwl->w_wait--;
   pthread_mutex_unlock(&rwl->mutex);
}

/*
 * Lock for read access, wait until locked (or error).
 */
int rwl_readlock(rwlock_t *rwl)
{
   int stat;
    
   if (rwl->valid != RWLOCK_VALID) {
      return EINVAL;
   }
   if ((stat = pthread_mutex_lock(&rwl->mutex)) != 0) {
      return stat;
   }
   if (rwl->w_active) {
      rwl->r_wait++;		      /* indicate that we are waiting */
      pthread_cleanup_push(rwl_read_release, (void *)rwl);
      while (rwl->w_active) {
	 stat = pthread_cond_wait(&rwl->read, &rwl->mutex);
	 if (stat != 0) {
	    break;		      /* error, bail out */
	 }
      }
      pthread_cleanup_pop(0);
      rwl->r_wait--;		      /* we are no longer waiting */
   }
   if (stat == 0) {
      rwl->r_active++;		      /* we are running */
   }
   pthread_mutex_unlock(&rwl->mutex);
   return stat;
}

/* 
 * Attempt to lock for read access, don't wait
 */
int rwl_readtrylock(rwlock_t *rwl)
{
   int stat, stat2;
    
   if (rwl->valid != RWLOCK_VALID) {
      return EINVAL;
   }
   if ((stat = pthread_mutex_lock(&rwl->mutex)) != 0) {
      return stat;
   }
   if (rwl->w_active) {
      stat = EBUSY;
   } else {
      rwl->r_active++;		      /* we are running */
   }
   stat2 = pthread_mutex_unlock(&rwl->mutex);
   return (stat == 0 ? stat2 : stat);
}
   
/* 
 * Unlock read lock
 */
int rwl_readunlock(rwlock_t *rwl)
{
   int stat, stat2;
    
   if (rwl->valid != RWLOCK_VALID) {
      return EINVAL;
   }
   if ((stat = pthread_mutex_lock(&rwl->mutex)) != 0) {
      return stat;
   }
   rwl->r_active--;
   if (rwl->r_active == 0 && rwl->w_wait > 0) { /* if writers waiting */
      stat = pthread_cond_signal(&rwl->write);
   }
   stat2 = pthread_mutex_unlock(&rwl->mutex);
   return (stat == 0 ? stat2 : stat);
}


/*
 * Lock for write access, wait until locked (or error).
 */
int rwl_writelock(rwlock_t *rwl)
{
   int stat;
    
   if (rwl->valid != RWLOCK_VALID) {
      return EINVAL;
   }
   if ((stat = pthread_mutex_lock(&rwl->mutex)) != 0) {
      return stat;
   }
   if (rwl->w_active || rwl->r_active > 0) {
      rwl->w_wait++;		      /* indicate that we are waiting */
      pthread_cleanup_push(rwl_write_release, (void *)rwl);
      while (rwl->w_active || rwl->r_active > 0) {
	 stat = pthread_cond_wait(&rwl->write, &rwl->mutex);
	 if (stat != 0) {
	    break;		      /* error, bail out */
	 }
      }
      pthread_cleanup_pop(0);
      rwl->w_wait--;		      /* we are no longer waiting */
   }
   if (stat == 0) {
      rwl->w_active = 1;	      /* we are running */
   }
   pthread_mutex_unlock(&rwl->mutex);
   return stat;
}

/* 
 * Attempt to lock for write access, don't wait
 */
int rwl_writetrylock(rwlock_t *rwl)
{
   int stat, stat2;
    
   if (rwl->valid != RWLOCK_VALID) {
      return EINVAL;
   }
   if ((stat = pthread_mutex_lock(&rwl->mutex)) != 0) {
      return stat;
   }
   if (rwl->w_active || rwl->r_active > 0) {
      stat = EBUSY;
   } else {
      rwl->w_active = 1;	      /* we are running */
   }
   stat2 = pthread_mutex_unlock(&rwl->mutex);
   return (stat == 0 ? stat2 : stat);
}
   
/* 
 * Unlock write lock
 *  Start any waiting writers in preference to waiting readers
 */
int rwl_writeunlock(rwlock_t *rwl)
{
   int stat, stat2;
    
   if (rwl->valid != RWLOCK_VALID) {
      return EINVAL;
   }
   if ((stat = pthread_mutex_lock(&rwl->mutex)) != 0) {
      return stat;
   }
   rwl->w_active = 0;
   if (rwl->w_wait > 0) {	      /* if writers waiting */
      stat = pthread_cond_signal(&rwl->write);
   } else if (rwl->r_wait > 0) {
      stat = pthread_cond_broadcast(&rwl->read);
   }
   stat2 = pthread_mutex_unlock(&rwl->mutex);
   return (stat == 0 ? stat2 : stat);
}

#endif
