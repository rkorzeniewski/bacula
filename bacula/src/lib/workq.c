/*
 * Bacula work queue routines. Permits passing work to
 *  multiple threads.
 *
 *  Kern Sibbald, January MMI
 *
 *   Version $Id$
 *
 *  This code adapted from "Programming with POSIX Threads", by
 *    David R. Butenhof
 *
 * Example:
 *
 * static workq_t job_wq;    define work queue
 *
 *  Initialize queue
 *  if ((stat = workq_init(&job_wq, max_workers, job_thread)) != 0) {
 *     Emsg1(M_ABORT, 0, "Could not init job work queue: ERR=%s\n", strerror(errno));
 *   }
 *
 *  Add an item to the queue
 *  if ((stat = workq_add(&job_wq, (void *)jcr)) != 0) {
 *      Emsg1(M_ABORT, 0, "Could not add job to work queue: ERR=%s\n", strerror(errno));
 *   }
 *
 *  Terminate the queue
 *  workq_destroy(workq_t *wq);
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

/* Forward referenced functions */
static void *workq_server(void *arg);

/*   
 * Initialize a work queue
 *
 *  Returns: 0 on success
 *	     errno on failure
 */
int workq_init(workq_t *wq, int threads, void (*engine)(void *arg))
{
   int stat;
			
   if ((stat = pthread_attr_init(&wq->attr)) != 0) {
      return stat;
   }
   if ((stat = pthread_attr_setdetachstate(&wq->attr, PTHREAD_CREATE_DETACHED)) != 0) {
      pthread_attr_destroy(&wq->attr);
      return stat;
   }
   if ((stat = pthread_mutex_init(&wq->mutex, NULL)) != 0) {
      pthread_attr_destroy(&wq->attr);
      return stat;
   }
   if ((stat = pthread_cond_init(&wq->work, NULL)) != 0) {
      pthread_mutex_destroy(&wq->mutex);
      pthread_attr_destroy(&wq->attr);
      return stat;
   }
   wq->quit = 0;
   wq->first = wq->last = NULL;
   wq->max_workers = threads;	      /* max threads to create */
   wq->num_workers = 0; 	      /* no threads yet */
   wq->idle_workers = 0;	      /* no idle threads */
   wq->engine = engine; 	      /* routine to run */
   wq->valid = WORKQ_VALID; 
   return 0;
}

/*
 * Destroy a work queue
 *
 * Returns: 0 on success
 *	    errno on failure
 */
int workq_destroy(workq_t *wq)
{
   int stat, stat1, stat2;

  if (wq->valid != WORKQ_VALID) {
     return EINVAL;
  }
  if ((stat = pthread_mutex_lock(&wq->mutex)) != 0) {
     return stat;
  }
  wq->valid = 0;		      /* prevent any more operations */

  /* 
   * If any threads are active, wake them 
   */
  if (wq->num_workers > 0) {
     wq->quit = 1;
     if (wq->idle_workers) {
	if ((stat = pthread_cond_broadcast(&wq->work)) != 0) {
	   pthread_mutex_unlock(&wq->mutex);
	   return stat;
	}
     }
     while (wq->num_workers > 0) {
	if ((stat = pthread_cond_wait(&wq->work, &wq->mutex)) != 0) {
	   pthread_mutex_unlock(&wq->mutex);
	   return stat;
	}
     }
  }
  if ((stat = pthread_mutex_unlock(&wq->mutex)) != 0) {
     return stat;
  }
  stat	= pthread_mutex_destroy(&wq->mutex);
  stat1 = pthread_cond_destroy(&wq->work);
  stat2 = pthread_attr_destroy(&wq->attr);
  return (stat != 0 ? stat : (stat1 != 0 ? stat1 : stat2));
}


/*
 *  Add work to a queue
 */
int workq_add(workq_t *wq, void *element)
{
   int stat;
   workq_ele_t *item;
   pthread_t id;
    
   Dmsg0(200, "workq_add\n");
   if (wq->valid != WORKQ_VALID) {
      return EINVAL;
   }

   if ((item = (workq_ele_t *) malloc(sizeof(workq_ele_t))) == NULL) {
      return ENOMEM;
   }
   item->data = element;
   item->next = NULL;
   if ((stat = pthread_mutex_lock(&wq->mutex)) != 0) {
      free(item);
      return stat;
   }

   Dmsg0(200, "add item to queue\n");
   /* Add the new item to the end of the queue */
   if (wq->first == NULL) {
      wq->first = item;
   } else {
      wq->last->next = item;
   }
   wq->last = item;

   /* if any threads are idle, wake one */
   if (wq->idle_workers > 0) {
      Dmsg0(200, "Signal worker\n");
      if ((stat = pthread_cond_signal(&wq->work)) != 0) {
	 pthread_mutex_unlock(&wq->mutex);
	 return stat;
      }
   } else if (wq->num_workers < wq->max_workers) {
      Dmsg0(200, "Create worker thread\n");
      /* No idle threads so create a new one */
      set_thread_concurrency(wq->max_workers + 1);
      if ((stat = pthread_create(&id, &wq->attr, workq_server, (void *)wq)) != 0) {
	 pthread_mutex_unlock(&wq->mutex);
	 return stat;
      }
      wq->num_workers++;
   }
   pthread_mutex_unlock(&wq->mutex);
   Dmsg0(200, "Return workq_add\n");
   return stat;
}

/* 
 * This is the worker thread that serves the work queue.
 * In due course, it will call the user's engine.
 */
static void *workq_server(void *arg)
{
   struct timespec timeout;
   workq_t *wq = (workq_t *)arg;
   workq_ele_t *we;
   int stat, timedout;

   Dmsg0(200, "Start workq_server\n");
   if ((stat = pthread_mutex_lock(&wq->mutex)) != 0) {
      return NULL;
   }

   for (;;) {
      struct timeval tv;
      struct timezone tz;

      Dmsg0(200, "Top of for loop\n");
      timedout = 0;
      Dmsg0(200, "gettimeofday()\n");
      gettimeofday(&tv, &tz);
      timeout.tv_nsec = 0;
      timeout.tv_sec = tv.tv_sec + 2;

      while (wq->first == NULL && !wq->quit) {
	 /*
	  * Wait 2 seconds, then if no more work, exit
	  */
         Dmsg0(200, "pthread_cond_timedwait()\n");
#ifdef xxxxxxxxxxxxxxxx_was_HAVE_CYGWIN
	 /* CYGWIN dies with a page fault the second
	  * time that pthread_cond_timedwait() is called
	  * so fake it out.
	  */
	 pthread_mutex_lock(&wq->mutex);
	 stat = ETIMEDOUT;
#else
	 stat = pthread_cond_timedwait(&wq->work, &wq->mutex, &timeout);
#endif
         Dmsg1(200, "timedwait=%d\n", stat);
	 if (stat == ETIMEDOUT) {
	    timedout = 1;
	    break;
	 } else if (stat != 0) {
            /* This shouldn't happen */
            Dmsg0(200, "This shouldn't happen\n");
	    wq->num_workers--;
	    pthread_mutex_unlock(&wq->mutex);
	    return NULL;
	 }
      } 
      we = wq->first;
      if (we != NULL) {
	 wq->first = we->next;
	 if (wq->last == we) {
	    wq->last = NULL;
	 }
	 if ((stat = pthread_mutex_unlock(&wq->mutex)) != 0) {
	    return NULL;
	 }
         /* Call user's routine here */
         Dmsg0(200, "Calling user engine.\n");
	 wq->engine(we->data);
         Dmsg0(200, "Back from user engine.\n");
	 free(we);		      /* release work entry */
         Dmsg0(200, "relock mutex\n"); 
	 if ((stat = pthread_mutex_lock(&wq->mutex)) != 0) {
	    return NULL;
	 }
         Dmsg0(200, "Done lock mutex\n");
      }
      /*
       * If no more work request, and we are asked to quit, then do it
       */
      if (wq->first == NULL && wq->quit) {
	 wq->num_workers--;
	 if (wq->num_workers == 0) {
            Dmsg0(200, "Wake up destroy routine\n");
	    /* Wake up destroy routine if he is waiting */
	    pthread_cond_broadcast(&wq->work);
	 }
         Dmsg0(200, "Unlock mutex\n");
	 pthread_mutex_unlock(&wq->mutex);
         Dmsg0(200, "Return from workq_server\n");
	 return NULL;
      }
      Dmsg0(200, "Check for work request\n");
      /* 
       * If no more work requests, and we waited long enough, quit
       */
      Dmsg1(200, "wq->first==NULL = %d\n", wq->first==NULL);
      Dmsg1(200, "timedout=%d\n", timedout);
      if (wq->first == NULL && timedout) {
         Dmsg0(200, "break big loop\n");
	 wq->num_workers--;
	 break;
      }
      Dmsg0(200, "Loop again\n");
   } /* end of big for loop */

   Dmsg0(200, "unlock mutex\n");
   pthread_mutex_unlock(&wq->mutex);
   Dmsg0(200, "End workq_server\n");
   return NULL;
}
