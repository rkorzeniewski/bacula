/*
 * Bacula job queue routines.
 *
 *  This code consists of three queues, the waiting_jobs
 *  queue, where jobs are initially queued, the ready_jobs
 *  queue, where jobs are placed when all the resources are
 *  allocated and they can immediately be run, and the
 *  running queue where jobs are placed when they are
 *  running.
 *
 *  Kern Sibbald, July MMIII
 *
 *   Version $Id$
 *
 *  This code was adapted from the Bacula workq, which was
 *    adapted from "Programming with POSIX Threads", by
 *    David R. Butenhof
 *
 */
/*
   Copyright (C) 2003-2004 Kern Sibbald and John Walker

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
extern "C" void *jobq_server(void *arg);
extern "C" void *sched_wait(void *arg);

static int   start_server(jobq_t *jq);

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
      Jmsg1(NULL, M_ERROR, 0, "pthread_attr_init: ERR=%s\n", strerror(stat));
      return stat;
   }
   if ((stat = pthread_attr_setdetachstate(&jq->attr, PTHREAD_CREATE_DETACHED)) != 0) {
      pthread_attr_destroy(&jq->attr);
      return stat;
   }
   if ((stat = pthread_mutex_init(&jq->mutex, NULL)) != 0) {
      Jmsg1(NULL, M_ERROR, 0, "pthread_mutex_init: ERR=%s\n", strerror(stat));
      pthread_attr_destroy(&jq->attr);
      return stat;
   }
   if ((stat = pthread_cond_init(&jq->work, NULL)) != 0) {
      Jmsg1(NULL, M_ERROR, 0, "pthread_cond_init: ERR=%s\n", strerror(stat));
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
   /* Initialize the job queues */
   jq->waiting_jobs = new dlist(item, &item->link);
   jq->running_jobs = new dlist(item, &item->link);
   jq->ready_jobs = new dlist(item, &item->link);
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
     Jmsg1(NULL, M_ERROR, 0, "pthread_mutex_lock: ERR=%s\n", strerror(stat));
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
           Jmsg1(NULL, M_ERROR, 0, "pthread_cond_broadcast: ERR=%s\n", strerror(stat));
	   pthread_mutex_unlock(&jq->mutex);
	   return stat;
	}
     }
     while (jq->num_workers > 0) {
	if ((stat = pthread_cond_wait(&jq->work, &jq->mutex)) != 0) {
           Jmsg1(NULL, M_ERROR, 0, "pthread_cond_wait: ERR=%s\n", strerror(stat));
	   pthread_mutex_unlock(&jq->mutex);
	   return stat;
	}
     }
  }
  if ((stat = pthread_mutex_unlock(&jq->mutex)) != 0) {
     Jmsg1(NULL, M_ERROR, 0, "pthread_mutex_unlock: ERR=%s\n", strerror(stat));
     return stat;
  }
  stat	= pthread_mutex_destroy(&jq->mutex);
  stat1 = pthread_cond_destroy(&jq->work);
  stat2 = pthread_attr_destroy(&jq->attr);
  delete jq->waiting_jobs;
  delete jq->running_jobs;
  delete jq->ready_jobs;
  return (stat != 0 ? stat : (stat1 != 0 ? stat1 : stat2));
}

struct wait_pkt {
   JCR *jcr;
   jobq_t *jq;
};

/*
 * Wait until schedule time arrives before starting. Normally
 *  this routine is only used for jobs started from the console
 *  for which the user explicitly specified a start time. Otherwise
 *  most jobs are put into the job queue only when their
 *  scheduled time arives.
 */
extern "C"  
void *sched_wait(void *arg)
{
   JCR *jcr = ((wait_pkt *)arg)->jcr;
   jobq_t *jq = ((wait_pkt *)arg)->jq;

   Dmsg0(300, "Enter sched_wait.\n");
   free(arg);
   time_t wtime = jcr->sched_time - time(NULL);
   /* Wait until scheduled time arrives */
   if (wtime > 0 && verbose) {
      Jmsg(jcr, M_INFO, 0, _("Job %s waiting %d seconds for scheduled start time.\n"), 
	 jcr->Job, wtime);
      set_jcr_job_status(jcr, JS_WaitStartTime);
   }
   /* Check every 30 seconds if canceled */ 
   while (wtime > 0) {
      Dmsg2(300, "Waiting on sched time, jobid=%d secs=%d\n", jcr->JobId, wtime);
      if (wtime > 30) {
	 wtime = 30;
      }
      bmicrosleep(wtime, 0);
      if (job_canceled(jcr)) {
	 break;
      }
      wtime = jcr->sched_time - time(NULL);
   }
   P(jcr->mutex);		      /* lock jcr */
   jobq_add(jq, jcr);
   V(jcr->mutex);
   free_jcr(jcr);		      /* we are done with jcr */
   Dmsg0(300, "Exit sched_wait\n");
   return NULL;
}

/*
 *  Add a job to the queue
 *    jq is a queue that was created with jobq_init
 * 
 *  On entry jcr->mutex must be locked.
 *   
 */
int jobq_add(jobq_t *jq, JCR *jcr)
{
   int stat;
   jobq_item_t *item, *li;
   bool inserted = false;
   time_t wtime = jcr->sched_time - time(NULL);
   pthread_t id;
   wait_pkt *sched_pkt;
    
   Dmsg3(300, "jobq_add jobid=%d jcr=0x%x use_count=%d\n", jcr->JobId, jcr, jcr->use_count);
   if (jq->valid != JOBQ_VALID) {
      Jmsg0(jcr, M_ERROR, 0, "Jobq_add queue not initialized.\n");
      return EINVAL;
   }

   jcr->use_count++;		      /* mark jcr in use by us */

   Dmsg3(300, "jobq_add jobid=%d jcr=0x%x use_count=%d\n", jcr->JobId, jcr, jcr->use_count);
   if (!job_canceled(jcr) && wtime > 0) {
      set_thread_concurrency(jq->max_workers + 2);
      sched_pkt = (wait_pkt *)malloc(sizeof(wait_pkt));
      sched_pkt->jcr = jcr;
      sched_pkt->jq = jq;
      stat = pthread_create(&id, &jq->attr, sched_wait, (void *)sched_pkt);	   
      if (stat != 0) {		      /* thread not created */
         Jmsg1(jcr, M_ERROR, 0, "pthread_thread_create: ERR=%s\n", strerror(stat));
	 jcr->use_count--;	      /* release jcr */
      }
      return stat;
   }

   if ((stat = pthread_mutex_lock(&jq->mutex)) != 0) {
      Jmsg1(jcr, M_ERROR, 0, "pthread_mutex_lock: ERR=%s\n", strerror(stat));
      jcr->use_count--; 	      /* release jcr */
      return stat;
   }

   if ((item = (jobq_item_t *)malloc(sizeof(jobq_item_t))) == NULL) {
      jcr->use_count--; 	      /* release jcr */
      return ENOMEM;
   }
   item->jcr = jcr;

   if (job_canceled(jcr)) {
      /* Add job to ready queue so that it is canceled quickly */
      jq->ready_jobs->prepend(item);
      Dmsg1(300, "Prepended job=%d to ready queue\n", jcr->JobId);
   } else {
      /* Add this job to the wait queue in priority sorted order */
      foreach_dlist(li, jq->waiting_jobs) {
         Dmsg2(300, "waiting item jobid=%d priority=%d\n",
	    li->jcr->JobId, li->jcr->JobPriority);
	 if (li->jcr->JobPriority > jcr->JobPriority) {
	    jq->waiting_jobs->insert_before(item, li);
            Dmsg2(300, "insert_before jobid=%d before waiting job=%d\n", 
	       li->jcr->JobId, jcr->JobId);
	    inserted = true;
	    break;
	 }
      }
      /* If not jobs in wait queue, append it */
      if (!inserted) {
	 jq->waiting_jobs->append(item);
         Dmsg1(300, "Appended item jobid=%d to waiting queue\n", jcr->JobId);
      }
   }

   /* Ensure that at least one server looks at the queue. */
   stat = start_server(jq);

   pthread_mutex_unlock(&jq->mutex);
   Dmsg0(300, "Return jobq_add\n");
   return stat;
}

/*
 *  Remove a job from the job queue. Used only by cancel_job().
 *    jq is a queue that was created with jobq_init
 *    work_item is an element of work
 *
 *   Note, it is "removed" from the job queue.
 *    If you want to cancel it, you need to provide some external means
 *    of doing so (e.g. pthread_kill()).
 */
int jobq_remove(jobq_t *jq, JCR *jcr)
{
   int stat;
   bool found = false;
   jobq_item_t *item;
    
   Dmsg2(300, "jobq_remove jobid=%d jcr=0x%x\n", jcr->JobId, jcr);
   if (jq->valid != JOBQ_VALID) {
      return EINVAL;
   }

   if ((stat = pthread_mutex_lock(&jq->mutex)) != 0) {
      Jmsg1(NULL, M_ERROR, 0, "pthread_mutex_lock: ERR=%s\n", strerror(stat));
      return stat;
   }

   foreach_dlist(item, jq->waiting_jobs) {
      if (jcr == item->jcr) {
	 found = true;
	 break;
      }
   }
   if (!found) {
      pthread_mutex_unlock(&jq->mutex);
      Dmsg2(300, "jobq_remove jobid=%d jcr=0x%x not in wait queue\n", jcr->JobId, jcr);
      return EINVAL;
   }

   /* Move item to be the first on the list */
   jq->waiting_jobs->remove(item);
   jq->ready_jobs->prepend(item);
   Dmsg2(300, "jobq_remove jobid=%d jcr=0x%x moved to ready queue\n", jcr->JobId, jcr);
   
   stat = start_server(jq);

   pthread_mutex_unlock(&jq->mutex);
   Dmsg0(300, "Return jobq_remove\n");
   return stat;
}


/*
 * Start the server thread if it isn't already running
 */
static int start_server(jobq_t *jq)
{
   int stat = 0;
   pthread_t id;

   /* if any threads are idle, wake one */
   if (jq->idle_workers > 0) {
      Dmsg0(300, "Signal worker to wake up\n");
      if ((stat = pthread_cond_signal(&jq->work)) != 0) {
         Jmsg1(NULL, M_ERROR, 0, "pthread_cond_signal: ERR=%s\n", strerror(stat));
	 return stat;
      }
   } else if (jq->num_workers < jq->max_workers) {
      Dmsg0(300, "Create worker thread\n");
      /* No idle threads so create a new one */
      set_thread_concurrency(jq->max_workers + 1);
      if ((stat = pthread_create(&id, &jq->attr, jobq_server, (void *)jq)) != 0) {
         Jmsg1(NULL, M_ERROR, 0, "pthread_create: ERR=%s\n", strerror(stat));
	 return stat;
      }
   }
   return stat;
}


/* 
 * This is the worker thread that serves the job queue.
 * When all the resources are acquired for the job, 
 *  it will call the user's engine.
 */
extern "C"  
void *jobq_server(void *arg)
{
   struct timespec timeout;
   jobq_t *jq = (jobq_t *)arg;
   jobq_item_t *je;		      /* job entry in queue */
   int stat;
   bool timedout = false;
   bool work = true;

   Dmsg0(300, "Start jobq_server\n");
   if ((stat = pthread_mutex_lock(&jq->mutex)) != 0) {
      Jmsg1(NULL, M_ERROR, 0, "pthread_mutex_lock: ERR=%s\n", strerror(stat));
      return NULL;
   }
   jq->num_workers++;

   for (;;) {
      struct timeval tv;
      struct timezone tz;

      Dmsg0(300, "Top of for loop\n");
      if (!work && !jq->quit) {
	 gettimeofday(&tv, &tz);
	 timeout.tv_nsec = 0;
	 timeout.tv_sec = tv.tv_sec + 4;

	 while (!jq->quit) {
	    /*
	     * Wait 4 seconds, then if no more work, exit
	     */
            Dmsg0(300, "pthread_cond_timedwait()\n");
	    stat = pthread_cond_timedwait(&jq->work, &jq->mutex, &timeout);
	    if (stat == ETIMEDOUT) {
               Dmsg0(300, "timedwait timedout.\n");
	       timedout = true;
	       break;
	    } else if (stat != 0) {
               /* This shouldn't happen */
               Dmsg0(300, "This shouldn't happen\n");
	       jq->num_workers--;
	       pthread_mutex_unlock(&jq->mutex);
	       return NULL;
	    }
	    break;
	 } 
      }
      /* 
       * If anything is in the ready queue, run it
       */
      Dmsg0(300, "Checking ready queue.\n");
      while (!jq->ready_jobs->empty() && !jq->quit) {
	 JCR *jcr;
	 je = (jobq_item_t *)jq->ready_jobs->first(); 
	 jcr = je->jcr;
	 jq->ready_jobs->remove(je);
	 if (!jq->ready_jobs->empty()) {
            Dmsg0(300, "ready queue not empty start server\n");
	    if (start_server(jq) != 0) {
	       jq->num_workers--;
	       pthread_mutex_unlock(&jq->mutex);
	       return NULL;
	    }
	 }
	 jq->running_jobs->append(je);
         Dmsg1(300, "Took jobid=%d from ready and appended to run\n", jcr->JobId);
	 if ((stat = pthread_mutex_unlock(&jq->mutex)) != 0) {
            Jmsg1(NULL, M_ERROR, 0, "pthread_mutex_unlock: ERR=%s\n", strerror(stat));
	    jq->num_workers--;
	    return NULL;
	 }

         /* Call user's routine here */
         Dmsg1(300, "Calling user engine for jobid=%d\n", jcr->JobId);
	 jq->engine(je->jcr);

         Dmsg1(300, "Back from user engine jobid=%d.\n", jcr->JobId);
	 if ((stat = pthread_mutex_lock(&jq->mutex)) != 0) {
            Jmsg1(NULL, M_ERROR, 0, "pthread_mutex_lock: ERR=%s\n", strerror(stat));
	    jq->num_workers--;
	    free(je);		      /* release job entry */
	    return NULL;
	 }
         Dmsg0(200, "Done lock mutex after running job. Release locks.\n");
	 jq->running_jobs->remove(je);
	 /* 
	  * Release locks if acquired. Note, they will not have
	  *  been acquired for jobs canceled before they were
	  *  put into the ready queue.
	  */
	 if (jcr->acquired_resource_locks) {
	    jcr->store->NumConcurrentJobs--;
	    if (jcr->JobType == JT_RESTORE) {
	       jcr->store->MaxConcurrentJobs = jcr->saveMaxConcurrentJobs;  
	    }
	    jcr->client->NumConcurrentJobs--;
	    jcr->job->NumConcurrentJobs--;
	 }

	 if (jcr->job->RescheduleOnError && 
	     jcr->JobStatus != JS_Terminated &&
	     jcr->JobStatus != JS_Canceled && 
	     jcr->job->RescheduleTimes > 0 && 
	     jcr->reschedule_count < jcr->job->RescheduleTimes) {
	     char dt[50];

	     /*
	      * Reschedule this job by cleaning it up, but
	      *  reuse the same JobId if possible.
	      */
	    jcr->reschedule_count++;
	    jcr->sched_time = time(NULL) + jcr->job->RescheduleInterval;
            Dmsg2(300, "Rescheduled Job %s to re-run in %d seconds.\n", jcr->Job,
	       (int)jcr->job->RescheduleInterval);
	    bstrftime(dt, sizeof(dt), time(NULL));
            Jmsg(jcr, M_INFO, 0, _("Rescheduled Job %s at %s to re-run in %d seconds.\n"),
	       jcr->Job, dt, (int)jcr->job->RescheduleInterval);
	    jcr->JobStatus = JS_Created; /* force new status */
	    dird_free_jcr(jcr); 	 /* partial cleanup old stuff */
	    if (jcr->JobBytes == 0) {
               Dmsg1(300, "Requeue job=%d\n", jcr->JobId);
	       V(jq->mutex);
	       jobq_add(jq, jcr);     /* queue the job to run again */
	       P(jq->mutex);
	       free(je);	      /* free the job entry */
	       continue;	      /* look for another job to run */
	    }
	    /* 
	     * Something was actually backed up, so we cannot reuse
	     *	 the old JobId or there will be database record
	     *	 conflicts.  We now create a new job, copying the
	     *	 appropriate fields.
	     */
	    JCR *njcr = new_jcr(sizeof(JCR), dird_free_jcr);
	    set_jcr_defaults(njcr, jcr->job);
	    njcr->reschedule_count = jcr->reschedule_count;
	    njcr->JobLevel = jcr->JobLevel;
	    njcr->JobStatus = jcr->JobStatus;
	    njcr->pool = jcr->pool;
	    njcr->store = jcr->store;
	    njcr->messages = jcr->messages;
            Dmsg0(300, "Call to run new job\n");
	    V(jq->mutex);
            run_job(njcr);            /* This creates a "new" job */
            free_jcr(njcr);           /* release "new" jcr */
	    P(jq->mutex);
            Dmsg0(300, "Back from running new job.\n");
	 }
	 /* Clean up and release old jcr */
	 if (jcr->db) {
	    db_close_database(jcr, jcr->db);
	    jcr->db = NULL;
	 }
         Dmsg1(300, "====== Termination job=%d\n", jcr->JobId);
	 free_jcr(jcr);
	 free(je);		      /* release job entry */
      }
      /*
       * If any job in the wait queue can be run,
       *  move it to the ready queue
       */
      Dmsg0(300, "Done check ready, now check wait queue.\n");
      while (!jq->waiting_jobs->empty() && !jq->quit) {
	 int Priority;
	 je = (jobq_item_t *)jq->waiting_jobs->first(); 
	 jobq_item_t *re = (jobq_item_t *)jq->running_jobs->first();
	 if (re) {
	    Priority = re->jcr->JobPriority;
            Dmsg2(300, "JobId %d is running. Look for pri=%d\n", re->jcr->JobId, Priority);
	 } else {
	    Priority = je->jcr->JobPriority;
            Dmsg1(300, "No job running. Look for Job pri=%d\n", Priority);
	 }
	 /*
	  * Walk down the list of waiting jobs and attempt
	  *   to acquire the resources it needs.
	  */
	 for ( ; je;  ) {
	    /* je is current job item on the queue, jn is the next one */
	    JCR *jcr = je->jcr;
	    jobq_item_t *jn = (jobq_item_t *)jq->waiting_jobs->next(je);
            Dmsg3(300, "Examining Job=%d JobPri=%d want Pri=%d\n",
	       jcr->JobId, jcr->JobPriority, Priority);
	    /* Take only jobs of correct Priority */
	    if (jcr->JobPriority != Priority) {
	       set_jcr_job_status(jcr, JS_WaitPriority);
	       break;
	    }
	    if (jcr->JobType == JT_RESTORE) {
	       /* Let only one Restore job run at a time regardless of MaxConcurrentJobs */
	       if (jcr->store->NumConcurrentJobs == 0) {
		  jcr->store->NumConcurrentJobs++;
		  jcr->saveMaxConcurrentJobs = jcr->store->MaxConcurrentJobs;
		  jcr->store->MaxConcurrentJobs = 1;
	       } else {
		  set_jcr_job_status(jcr, JS_WaitStoreRes);
		  je = jn;
		  continue;
	       }
	    } else if (jcr->store->NumConcurrentJobs < jcr->store->MaxConcurrentJobs) {
	       jcr->store->NumConcurrentJobs++;
	    } else {
	       set_jcr_job_status(jcr, JS_WaitStoreRes);
	       je = jn;
	       continue;
	    }

	    if (jcr->client->NumConcurrentJobs < jcr->client->MaxConcurrentJobs) {
	       jcr->client->NumConcurrentJobs++;
	    } else {
	       /* Back out previous locks */
	       jcr->store->NumConcurrentJobs--;
	       if (jcr->JobType == JT_RESTORE) {
		  jcr->store->MaxConcurrentJobs = jcr->saveMaxConcurrentJobs;  
	       }
	       set_jcr_job_status(jcr, JS_WaitClientRes);
	       je = jn;
	       continue;
	    }
	    if (jcr->job->NumConcurrentJobs < jcr->job->MaxConcurrentJobs) {
	       jcr->job->NumConcurrentJobs++;
	    } else {
	       /* Back out previous locks */
	       jcr->store->NumConcurrentJobs--;
	       if (jcr->JobType == JT_RESTORE) {
		  jcr->store->MaxConcurrentJobs = jcr->saveMaxConcurrentJobs;  
	       }
	       jcr->client->NumConcurrentJobs--;
	       set_jcr_job_status(jcr, JS_WaitJobRes);
	       je = jn;
	       continue;
	    }
	    /* Got all locks, now remove it from wait queue and append it
	     *	 to the ready queue  
	     */
	    jcr->acquired_resource_locks = true;
	    jq->waiting_jobs->remove(je);
	    jq->ready_jobs->append(je);
            Dmsg1(300, "moved JobId=%d from wait to ready queue\n", je->jcr->JobId);
	    je = jn;
	 } /* end for loop */
	 break;
      } /* end while loop */
      Dmsg0(300, "Done checking wait queue.\n");
      /*
       * If no more ready work and we are asked to quit, then do it
       */
      if (jq->ready_jobs->empty() && jq->quit) {
	 jq->num_workers--;
	 if (jq->num_workers == 0) {
            Dmsg0(300, "Wake up destroy routine\n");
	    /* Wake up destroy routine if he is waiting */
	    pthread_cond_broadcast(&jq->work);
	 }
	 break;
      }
      Dmsg0(300, "Check for work request\n");
      /* 
       * If no more work requests, and we waited long enough, quit
       */
      Dmsg2(300, "timedout=%d read empty=%d\n", timedout,
	 jq->ready_jobs->empty());
      if (jq->ready_jobs->empty() && timedout) {
         Dmsg0(300, "break big loop\n");
	 jq->num_workers--;
	 break;
      }

      work = !jq->ready_jobs->empty() || !jq->waiting_jobs->empty();
      if (work) {
	 /*	     
          * If a job is waiting on a Resource, don't consume all
	  *   the CPU time looping looking for work, and even more
	  *   important, release the lock so that a job that has
	  *   terminated can give us the resource.
	  */
	 if ((stat = pthread_mutex_unlock(&jq->mutex)) != 0) {
            Jmsg1(NULL, M_ERROR, 0, "pthread_mutex_unlock: ERR=%s\n", strerror(stat));
	    jq->num_workers--;
	    return NULL;
	 }
	 bmicrosleep(2, 0);		 /* pause for 2 seconds */
	 if ((stat = pthread_mutex_lock(&jq->mutex)) != 0) {
            Jmsg1(NULL, M_ERROR, 0, "pthread_mutex_lock: ERR=%s\n", strerror(stat));
	    jq->num_workers--;
	    return NULL;
	 }
	 /* Recompute work as something may have changed in last 2 secs */
	 work = !jq->ready_jobs->empty() || !jq->waiting_jobs->empty();
      }
      Dmsg1(300, "Loop again. work=%d\n", work);
   } /* end of big for loop */

   Dmsg0(200, "unlock mutex\n");
   if ((stat = pthread_mutex_unlock(&jq->mutex)) != 0) {
      Jmsg1(NULL, M_ERROR, 0, "pthread_mutex_unlock: ERR=%s\n", strerror(stat));
   }
   Dmsg0(300, "End jobq_server\n");
   return NULL;
}
