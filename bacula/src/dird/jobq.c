/*
 * Bacula job queue routines.
 *
 *  Kern Sibbald, July MMIII
 *
 *   Version $Id$
 *
 *  This code was adapted from the Bacula workq, which was
 *    adapted from "Programming with POSIX Threads", by
 *    David R. Butenhof
 *
 * Example:
 *
 * static jobq_t jq;	define job queue
 *
 *  Initialize queue
 *  if ((stat = jobq_init(&jq, max_workers, job_thread)) != 0) {
 *     Emsg1(M_ABORT, 0, "Could not init job work queue: ERR=%s\n", strerror(errno));
 *   }
 *
 *  Add an item to the queue
 *  if ((stat = jobq_add(&jq, jcr)) != 0) {
 *      Emsg1(M_ABORT, 0, "Could not add job to queue: ERR=%s\n", strerror(errno));
 *   }
 *
 *  Terminate the queue
 *  jobq_destroy(jobq_t *jq);
 *
 */
/*
   Copyright (C) 2000-2003 Kern Sibbald and John Walker

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
#include "dird.h"

/* Forward referenced functions */
static void *jobq_server(void *arg);

/*   
 * Initialize a job queue
 *
 *  Returns: 0 on success
 *	     errno on failure
 */
int jobq_init(jobq_t *jq, int threads, void *(*engine)(void *arg))
{
   int stat;
   jobq_item_t *item = NULL;
			
   if ((stat = pthread_attr_init(&jq->attr)) != 0) {
      return stat;
   }
   if ((stat = pthread_attr_setdetachstate(&jq->attr, PTHREAD_CREATE_DETACHED)) != 0) {
      pthread_attr_destroy(&jq->attr);
      return stat;
   }
   if ((stat = pthread_mutex_init(&jq->mutex, NULL)) != 0) {
      pthread_attr_destroy(&jq->attr);
      return stat;
   }
   if ((stat = pthread_cond_init(&jq->work, NULL)) != 0) {
      pthread_mutex_destroy(&jq->mutex);
      pthread_attr_destroy(&jq->attr);
      return stat;
   }
   jq->quit = false;
   jq->max_workers = threads;	      /* max threads to create */
   jq->num_workers = 0; 	      /* no threads yet */
   jq->idle_workers = 0;	      /* no idle threads */
   jq->engine = engine; 	      /* routine to run */
   jq->valid = JOBQ_VALID; 
   jq->list.init(item, &item->link);
   return 0;
}

/*
 * Destroy the job queue
 *
 * Returns: 0 on success
 *	    errno on failure
 */
int jobq_destroy(jobq_t *jq)
{
   int stat, stat1, stat2;

  if (jq->valid != JOBQ_VALID) {
     return EINVAL;
  }
  if ((stat = pthread_mutex_lock(&jq->mutex)) != 0) {
     return stat;
  }
  jq->valid = 0;		      /* prevent any more operations */

  /* 
   * If any threads are active, wake them 
   */
  if (jq->num_workers > 0) {
     jq->quit = true;
     if (jq->idle_workers) {
	if ((stat = pthread_cond_broadcast(&jq->work)) != 0) {
	   pthread_mutex_unlock(&jq->mutex);
	   return stat;
	}
     }
     while (jq->num_workers > 0) {
	if ((stat = pthread_cond_wait(&jq->work, &jq->mutex)) != 0) {
	   pthread_mutex_unlock(&jq->mutex);
	   return stat;
	}
     }
  }
  if ((stat = pthread_mutex_unlock(&jq->mutex)) != 0) {
     return stat;
  }
  stat	= pthread_mutex_destroy(&jq->mutex);
  stat1 = pthread_cond_destroy(&jq->work);
  stat2 = pthread_attr_destroy(&jq->attr);
  jq->list.destroy();
  return (stat != 0 ? stat : (stat1 != 0 ? stat1 : stat2));
}


/*
 *  Add a job to the queue
 *    jq is a queue that was created with jobq_init
 *   
 */
int jobq_add(jobq_t *jq, JCR *jcr)
{
   int stat;
   jobq_item_t *item, *li;
   pthread_t id;
   bool inserted = false;
    
   Dmsg0(200, "jobq_add\n");
   if (jq->valid != JOBQ_VALID) {
      return EINVAL;
   }

   if ((item = (jobq_item_t *)malloc(sizeof(jobq_item_t))) == NULL) {
      return ENOMEM;
   }
   item->jcr = jcr;
   if ((stat = pthread_mutex_lock(&jq->mutex)) != 0) {
      free(item);
      return stat;
   }

   Dmsg0(200, "add item to queue\n");
   for (li=NULL; (li=(jobq_item_t *)jq->list.next(li)); ) {
      if (li->jcr->JobPriority < jcr->JobPriority) {
	 jq->list.insert_before(item, li);
	 inserted = true;
      }
   }
   if (!inserted) {
      jq->list.append(item);
   }

   /* if any threads are idle, wake one */
   if (jq->idle_workers > 0) {
      Dmsg0(200, "Signal worker\n");
      if ((stat = pthread_cond_signal(&jq->work)) != 0) {
	 pthread_mutex_unlock(&jq->mutex);
	 return stat;
      }
   } else if (jq->num_workers < jq->max_workers) {
      Dmsg0(200, "Create worker thread\n");
      /* No idle threads so create a new one */
      set_thread_concurrency(jq->max_workers + 1);
      if ((stat = pthread_create(&id, &jq->attr, jobq_server, (void *)jq)) != 0) {
	 pthread_mutex_unlock(&jq->mutex);
	 return stat;
      }
      jq->num_workers++;
   }
   pthread_mutex_unlock(&jq->mutex);
   Dmsg0(200, "Return jobq_add\n");
   return stat;
}

/*
 *  Remove a job from the job queue
 *    jq is a queue that was created with jobq_init
 *    work_item is an element of work
 *
 *   Note, it is "removed" by immediately calling a processing routine.
 *    if you want to cancel it, you need to provide some external means
 *    of doing so.
 */
int jobq_remove(jobq_t *jq, JCR *jcr)
{
   int stat;
   bool found = false;
   pthread_t id;
   jobq_item_t *item;
    
   Dmsg0(200, "jobq_remove\n");
   if (jq->valid != JOBQ_VALID) {
      return EINVAL;
   }

   if ((stat = pthread_mutex_lock(&jq->mutex)) != 0) {
      return stat;
   }

   for (item=NULL; (item=(jobq_item_t *)jq->list.next(item)); ) {
      if (jcr == item->jcr) {
	 found = true;
	 break;
      }
   }
   if (!found) {
      return EINVAL;
   }

   /* Move item to be the first on the list */
   jq->list.remove(item);
   jq->list.prepend(item);
   
   /* if any threads are idle, wake one */
   if (jq->idle_workers > 0) {
      Dmsg0(200, "Signal worker\n");
      if ((stat = pthread_cond_signal(&jq->work)) != 0) {
	 pthread_mutex_unlock(&jq->mutex);
	 return stat;
      }
   } else {
      Dmsg0(200, "Create worker thread\n");
      /* No idle threads so create a new one */
      set_thread_concurrency(jq->max_workers + 1);
      if ((stat = pthread_create(&id, &jq->attr, jobq_server, (void *)jq)) != 0) {
	 pthread_mutex_unlock(&jq->mutex);
	 return stat;
      }
      jq->num_workers++;
   }
   pthread_mutex_unlock(&jq->mutex);
   Dmsg0(200, "Return jobq_remove\n");
   return stat;
}


/* 
 * This is the worker thread that serves the job queue.
 * When all the resources are acquired for the job, 
 *  it will call the user's engine.
 */
static void *jobq_server(void *arg)
{
   struct timespec timeout;
   jobq_t *jq = (jobq_t *)arg;
   jobq_item_t *je;		      /* job entry in queue */
   int stat;
   bool timedout;

   Dmsg0(200, "Start jobq_server\n");
   if ((stat = pthread_mutex_lock(&jq->mutex)) != 0) {
      return NULL;
   }

   for (;;) {
      struct timeval tv;
      struct timezone tz;

      Dmsg0(200, "Top of for loop\n");
      timedout = false;
      Dmsg0(200, "gettimeofday()\n");
      gettimeofday(&tv, &tz);
      timeout.tv_nsec = 0;
      timeout.tv_sec = tv.tv_sec + 2;

      while (jq->list.empty() && !jq->quit) {
	 /*
	  * Wait 2 seconds, then if no more work, exit
	  */
         Dmsg0(200, "pthread_cond_timedwait()\n");
	 stat = pthread_cond_timedwait(&jq->work, &jq->mutex, &timeout);
         Dmsg1(200, "timedwait=%d\n", stat);
	 if (stat == ETIMEDOUT) {
	    timedout = true;
	    break;
	 } else if (stat != 0) {
            /* This shouldn't happen */
            Dmsg0(200, "This shouldn't happen\n");
	    jq->num_workers--;
	    pthread_mutex_unlock(&jq->mutex);
	    return NULL;
	 }
      } 
      je = (jobq_item_t *)jq->list.first();
      if (je != NULL) {
	 jq->list.remove(je);
	 if ((stat = pthread_mutex_unlock(&jq->mutex)) != 0) {
	    return NULL;
	 }
         /* Call user's routine here */
         Dmsg0(200, "Calling user engine.\n");
	 jq->engine(je->jcr);
         Dmsg0(200, "Back from user engine.\n");
	 free(je);		      /* release job entry */
         Dmsg0(200, "relock mutex\n"); 
	 if ((stat = pthread_mutex_lock(&jq->mutex)) != 0) {
	    return NULL;
	 }
         Dmsg0(200, "Done lock mutex\n");
      }
      /*
       * If no more work request, and we are asked to quit, then do it
       */
      if (jq->list.empty() && jq->quit) {
	 jq->num_workers--;
	 if (jq->num_workers == 0) {
            Dmsg0(200, "Wake up destroy routine\n");
	    /* Wake up destroy routine if he is waiting */
	    pthread_cond_broadcast(&jq->work);
	 }
         Dmsg0(200, "Unlock mutex\n");
	 pthread_mutex_unlock(&jq->mutex);
         Dmsg0(200, "Return from jobq_server\n");
	 return NULL;
      }
      Dmsg0(200, "Check for work request\n");
      /* 
       * If no more work requests, and we waited long enough, quit
       */
      Dmsg1(200, "jq empty = %d\n", jq->list.empty());
      Dmsg1(200, "timedout=%d\n", timedout);
      if (jq->list.empty() && timedout) {
         Dmsg0(200, "break big loop\n");
	 jq->num_workers--;
	 break;
      }
      Dmsg0(200, "Loop again\n");
   } /* end of big for loop */

   Dmsg0(200, "unlock mutex\n");
   pthread_mutex_unlock(&jq->mutex);
   Dmsg0(200, "End jobq_server\n");
   return NULL;
}
