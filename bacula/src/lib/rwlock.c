/*
 * Bacula Thread Read/Write locking code. It permits
 *  multiple readers but only one writer.  Note, however,
 *  that the writer thread is permitted to make multiple
 *  nested write lock calls.
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
   Copyright (C) 2000-2004 Kern Sibbald and John Walker

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

/*   
 * Initialize a read/write lock
 *
 *  Returns: 0 on success
 *	     errno on failure
 */
int rwl_init(brwlock_t *rwl)
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
int rwl_destroy(brwlock_t *rwl)
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
   brwlock_t *rwl = (brwlock_t *)arg;

   rwl->r_wait--;
   pthread_mutex_unlock(&rwl->mutex);
}

/*
 * Handle cleanup when the write lock condition variable wait
 * is released.
 */
static void rwl_write_release(void *arg)
{
   brwlock_t *rwl = (brwlock_t *)arg;

   rwl->w_wait--;
   pthread_mutex_unlock(&rwl->mutex);
}

/*
 * Lock for read access, wait until locked (or error).
 */
int rwl_readlock(brwlock_t *rwl)
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
int rwl_readtrylock(brwlock_t *rwl)
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
int rwl_readunlock(brwlock_t *rwl)
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
 *   Multiple nested write locking is permitted.
 */
int rwl_writelock(brwlock_t *rwl)
{
   int stat;
    
   if (rwl->valid != RWLOCK_VALID) {
      return EINVAL;
   }
   if ((stat = pthread_mutex_lock(&rwl->mutex)) != 0) {
      return stat;
   }
   if (rwl->w_active && pthread_equal(rwl->writer_id, pthread_self())) {
      rwl->w_active++;
      pthread_mutex_unlock(&rwl->mutex);
      return 0;
   }
   if (rwl->w_active || rwl->r_active > 0) {
      rwl->w_wait++;		      /* indicate that we are waiting */
      pthread_cleanup_push(rwl_write_release, (void *)rwl);
      while (rwl->w_active || rwl->r_active > 0) {
	 if ((stat = pthread_cond_wait(&rwl->write, &rwl->mutex)) != 0) {
	    break;		      /* error, bail out */
	 }
      }
      pthread_cleanup_pop(0);
      rwl->w_wait--;		      /* we are no longer waiting */
   }
   if (stat == 0) {
      rwl->w_active++;		      /* we are running */
      rwl->writer_id = pthread_self(); /* save writer thread's id */
   }
   pthread_mutex_unlock(&rwl->mutex);
   return stat;
}

/* 
 * Attempt to lock for write access, don't wait
 */
int rwl_writetrylock(brwlock_t *rwl)
{
   int stat, stat2;
    
   if (rwl->valid != RWLOCK_VALID) {
      return EINVAL;
   }
   if ((stat = pthread_mutex_lock(&rwl->mutex)) != 0) {
      return stat;
   }
   if (rwl->w_active && pthread_equal(rwl->writer_id, pthread_self())) {
      rwl->w_active++;
      pthread_mutex_unlock(&rwl->mutex);
      return 0;
   }
   if (rwl->w_active || rwl->r_active > 0) {
      stat = EBUSY;
   } else {
      rwl->w_active = 1;	      /* we are running */
      rwl->writer_id = pthread_self(); /* save writer thread's id */
   }
   stat2 = pthread_mutex_unlock(&rwl->mutex);
   return (stat == 0 ? stat2 : stat);
}
   
/* 
 * Unlock write lock
 *  Start any waiting writers in preference to waiting readers
 */
int rwl_writeunlock(brwlock_t *rwl)
{
   int stat, stat2;
    
   if (rwl->valid != RWLOCK_VALID) {
      return EINVAL;
   }
   if ((stat = pthread_mutex_lock(&rwl->mutex)) != 0) {
      return stat;
   }
   if (rwl->w_active <= 0) {
      Emsg0(M_ABORT, 0, "rwl_writeunlock called too many times.\n");
   }
   rwl->w_active--;
   if (!pthread_equal(pthread_self(), rwl->writer_id)) {
      Emsg0(M_ABORT, 0, "rwl_writeunlock by non-owner.\n");
   }
   if (rwl->w_active > 0) {
      stat = 0; 		      /* writers still active */
   } else {
      /* No more writers, awaken someone */
      if (rwl->r_wait > 0) {	     /* if readers waiting */
	 stat = pthread_cond_broadcast(&rwl->read);
      } else if (rwl->w_wait > 0) {
	 stat = pthread_cond_signal(&rwl->write);
      }
   }
   stat2 = pthread_mutex_unlock(&rwl->mutex);
   return (stat == 0 ? stat2 : stat);
}

#ifdef TEST_RWLOCK

#define THREADS     5
#define DATASIZE   15
#define ITERATIONS 10000

/*
 * Keep statics for each thread.
 */
typedef struct thread_tag {
   int thread_num;
   pthread_t thread_id;
   int writes;
   int reads;
   int interval;
} thread_t;

/* 
 * Read/write lock and shared data.
 */
typedef struct data_tag {
   brwlock_t lock;
   int data;
   int writes;
} data_t;

thread_t threads[THREADS];
data_t data[DATASIZE];

/* 
 * Thread start routine that uses read/write locks.
 */
void *thread_routine(void *arg)
{
   thread_t *self = (thread_t *)arg;
   int repeats = 0;
   int iteration;
   int element = 0;
   int status;

   for (iteration=0; iteration < ITERATIONS; iteration++) {
      /*
       * Each "self->interval" iterations, perform an
       * update operation (write lock instead of read
       * lock).
       */
      if ((iteration % self->interval) == 0) {
	 status = rwl_writelock(&data[element].lock);
	 if (status != 0) {
            Emsg1(M_ABORT, 0, "Write lock failed. ERR=%s\n", strerror(status));
	 }
	 data[element].data = self->thread_num;
	 data[element].writes++;
	 self->writes++;
	 status = rwl_writeunlock(&data[element].lock);
	 if (status != 0) {
            Emsg1(M_ABORT, 0, "Write unlock failed. ERR=%s\n", strerror(status));
	 }
      } else {
	 /*
	  * Look at the current data element to see whether
	  * the current thread last updated it. Count the
	  * times to report later.
	  */
	  status = rwl_readlock(&data[element].lock);
	  if (status != 0) {
             Emsg1(M_ABORT, 0, "Read lock failed. ERR=%s\n", strerror(status));
	  }
	  self->reads++;
	  if (data[element].data == self->thread_num)
	     repeats++;
	  status = rwl_readunlock(&data[element].lock);
	  if (status != 0) {
             Emsg1(M_ABORT, 0, "Read unlock failed. ERR=%s\n", strerror(status));
	  }
      }
      element++;
      if (element >= DATASIZE) {
	 element = 0;
      }
   }
   if (repeats > 0) {
      Pmsg2(000, "Thread %d found unchanged elements %d times\n",
	 self->thread_num, repeats);
   }
   return NULL;
}

int main (int argc, char *argv[])
{
    int count;
    int data_count;
    int status;
    unsigned int seed = 1;
    int thread_writes = 0;
    int data_writes = 0;

#ifdef sun
    /*
     * On Solaris 2.5, threads are not timesliced. To ensure
     * that our threads can run concurrently, we need to
     * increase the concurrency level to THREADS.
     */
    thr_setconcurrency (THREADS);
#endif

    /*
     * Initialize the shared data.
     */
    for (data_count = 0; data_count < DATASIZE; data_count++) {
	data[data_count].data = 0;
	data[data_count].writes = 0;
	status = rwl_init (&data[data_count].lock);
	if (status != 0) {
           Emsg1(M_ABORT, 0, "Init rwlock failed. ERR=%s\n", strerror(status));
	}
    }

    /*
     * Create THREADS threads to access shared data.
     */
    for (count = 0; count < THREADS; count++) {
	threads[count].thread_num = count + 1;
	threads[count].writes = 0;
	threads[count].reads = 0;
	threads[count].interval = rand_r (&seed) % 71;
	status = pthread_create (&threads[count].thread_id,
	    NULL, thread_routine, (void*)&threads[count]);
	if (status != 0) {
           Emsg1(M_ABORT, 0, "Create thread failed. ERR=%s\n", strerror(status));
	}
    }

    /*
     * Wait for all threads to complete, and collect
     * statistics.
     */
    for (count = 0; count < THREADS; count++) {
	status = pthread_join (threads[count].thread_id, NULL);
	if (status != 0) {
           Emsg1(M_ABORT, 0, "Join thread failed. ERR=%s\n", strerror(status));
	}
	thread_writes += threads[count].writes;
        printf ("%02d: interval %d, writes %d, reads %d\n",
	    count, threads[count].interval,
	    threads[count].writes, threads[count].reads);
    }

    /*
     * Collect statistics for the data.
     */
    for (data_count = 0; data_count < DATASIZE; data_count++) {
	data_writes += data[data_count].writes;
        printf ("data %02d: value %d, %d writes\n",
	    data_count, data[data_count].data, data[data_count].writes);
	rwl_destroy (&data[data_count].lock);
    }

    printf ("Total: %d thread writes, %d data writes\n",
	thread_writes, data_writes);
    return 0;
}

#endif

#ifdef TEST_RW_TRY_LOCK
/*
 * brwlock_try_main.c
 *
 * Demonstrate use of non-blocking read-write locks.
 *
 * Special notes: On a Solaris system, call thr_setconcurrency()
 * to allow interleaved thread execution, since threads are not
 * timesliced.
 */
#include <pthread.h>
#include "rwlock.h"
#include "errors.h"

#define THREADS 	5
#define ITERATIONS	1000
#define DATASIZE	15

/*
 * Keep statistics for each thread.
 */
typedef struct thread_tag {
    int 	thread_num;
    pthread_t	thread_id;
    int 	r_collisions;
    int 	w_collisions;
    int 	updates;
    int 	interval;
} thread_t;

/*
 * Read-write lock and shared data
 */
typedef struct data_tag {
    brwlock_t	 lock;
    int 	data;
    int 	updates;
} data_t;

thread_t threads[THREADS];
data_t data[DATASIZE];

/*
 * Thread start routine that uses read-write locks
 */
void *thread_routine (void *arg)
{
    thread_t *self = (thread_t*)arg;
    int iteration;
    int element;
    int status;

    element = 0;			/* Current data element */

    for (iteration = 0; iteration < ITERATIONS; iteration++) {
	if ((iteration % self->interval) == 0) {
	    status = rwl_writetrylock (&data[element].lock);
	    if (status == EBUSY)
		self->w_collisions++;
	    else if (status == 0) {
		data[element].data++;
		data[element].updates++;
		self->updates++;
		rwl_writeunlock (&data[element].lock);
	    } else
                err_abort (status, "Try write lock");
	} else {
	    status = rwl_readtrylock (&data[element].lock);
	    if (status == EBUSY)
		self->r_collisions++;
	    else if (status != 0) {
                err_abort (status, "Try read lock");
	    } else {
		if (data[element].data != data[element].updates)
                    printf ("%d: data[%d] %d != %d\n",
			self->thread_num, element,
			data[element].data, data[element].updates);
		rwl_readunlock (&data[element].lock);
	    }
	}

	element++;
	if (element >= DATASIZE)
	    element = 0;
    }
    return NULL;
}

int main (int argc, char *argv[])
{
    int count, data_count;
    unsigned int seed = 1;
    int thread_updates = 0, data_updates = 0;
    int status;

#ifdef sun
    /*
     * On Solaris 2.5, threads are not timesliced. To ensure
     * that our threads can run concurrently, we need to
     * increase the concurrency level to THREADS.
     */
    DPRINTF (("Setting concurrency level to %d\n", THREADS));
    thr_setconcurrency (THREADS);
#endif

    /*
     * Initialize the shared data.
     */
    for (data_count = 0; data_count < DATASIZE; data_count++) {
	data[data_count].data = 0;
	data[data_count].updates = 0;
	rwl_init (&data[data_count].lock);
    }

    /*
     * Create THREADS threads to access shared data.
     */
    for (count = 0; count < THREADS; count++) {
	threads[count].thread_num = count;
	threads[count].r_collisions = 0;
	threads[count].w_collisions = 0;
	threads[count].updates = 0;
	threads[count].interval = rand_r (&seed) % ITERATIONS;
	status = pthread_create (&threads[count].thread_id,
	    NULL, thread_routine, (void*)&threads[count]);
	if (status != 0)
            err_abort (status, "Create thread");
    }

    /*
     * Wait for all threads to complete, and collect
     * statistics.
     */
    for (count = 0; count < THREADS; count++) {
	status = pthread_join (threads[count].thread_id, NULL);
	if (status != 0)
            err_abort (status, "Join thread");
	thread_updates += threads[count].updates;
        printf ("%02d: interval %d, updates %d, "
                "r_collisions %d, w_collisions %d\n",
	    count, threads[count].interval,
	    threads[count].updates,
	    threads[count].r_collisions, threads[count].w_collisions);
    }

    /*
     * Collect statistics for the data.
     */
    for (data_count = 0; data_count < DATASIZE; data_count++) {
	data_updates += data[data_count].updates;
        printf ("data %02d: value %d, %d updates\n",
	    data_count, data[data_count].data, data[data_count].updates);
	rwl_destroy (&data[data_count].lock);
    }

    return 0;
}

#endif
