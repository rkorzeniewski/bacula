/*
 *
 *   Bacula scheduler
 *     It looks at what jobs are to be run and when
 *     and waits around until it is time to 
 *     fire them up.
 *
 *     Kern Sibbald, May MM, revised December MMIII
 *
 *   Version $Id$
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


/* Forward referenced subroutines */
static void find_runs();
static void add_job(JOB *job, RUN *run, time_t now, time_t runtime);

/* Imported subroutines */

/* Imported variables */

/* Local variables */
struct job_item {  
   RUN *run;
   JOB *job;
   time_t runtime;
   dlink link;			      /* link for list */
};		

/* List of jobs to be run. They were scheduled in this hour or the next */
static dlist *jobs_to_run;		 /* list of jobs to be run */

/* Time interval in secs to sleep if nothing to be run */
#define NEXT_CHECK_SECS 60

/*********************************************************************
 *
 *	   Main Bacula Scheduler
 *
 */
JCR *wait_for_next_job(char *one_shot_job_to_run)
{
   JCR *jcr;
   JOB *job;
   RUN *run;
   time_t now, runtime, nexttime;
   static bool first = true;
   char dt[MAX_TIME_LENGTH];
   job_item *next_job = NULL;

   Dmsg0(200, "Enter wait_for_next_job\n");
   if (first) {
      first = false;
      /* Create scheduled jobs list */
      jobs_to_run = new dlist(next_job, &next_job->link);
      if (one_shot_job_to_run) {	    /* one shot */
	 job = (JOB *)GetResWithName(R_JOB, one_shot_job_to_run);
	 if (!job) {
            Emsg1(M_ABORT, 0, _("Job %s not found\n"), one_shot_job_to_run);
	 }
         Dmsg1(5, "Found one_shot_job_to_run %s\n", one_shot_job_to_run);
	 jcr = new_jcr(sizeof(JCR), dird_free_jcr);
	 set_jcr_defaults(jcr, job);
	 return jcr;
      }
   }
   /* Wait until we have something in the
    * next hour or so.
    */
   while (jobs_to_run->size() == 0) {
      find_runs();
      if (jobs_to_run->size() > 0) {
	 break;
      }
      bmicrosleep(NEXT_CHECK_SECS, 0); /* recheck once per minute */
   }

   /* 
    * Sort through what is to be run in the next
    * two hours to find the first job to be run,
    * then wait around until it is time.
    *
    */
   time(&now);
   nexttime = now + 60 * 60 * 24;     /* a much later time */
   bstrftime(dt, sizeof(dt), now);
   Dmsg2(400, "jobs=%d. Now is %s\n", jobs_to_run->size(), dt);
   next_job = NULL;
   for (job_item *je=NULL; (je=(job_item *)jobs_to_run->next(je)); ) {
      runtime = je->runtime;
      if (runtime > 0 && runtime < nexttime) { /* find minimum time job */
	 nexttime = runtime;
	 next_job = je;
      }

#define xxxx_debug
#ifdef	xxxx_debug
      if (runtime > 0) {
	 bstrftime(dt, sizeof(dt), next_job->runtime);	
         Dmsg2(100, "    %s run %s\n", dt, next_job->job->hdr.name);
      }
#endif
   }
   if (!next_job) {		   /* we really should have something now */
      Emsg0(M_ABORT, 0, _("Scheduler logic error\n"));
   }

   /* Now wait for the time to run the job */
   for (;;) {
      time_t twait;
      now = time(NULL);
      twait = nexttime - now;
      if (twait <= 0) { 	      /* time to run it */
	 break;
      }
      bmicrosleep(twait, 0);
   }
   run = next_job->run; 	      /* pick up needed values */
   job = next_job->job;
   jobs_to_run->remove(next_job);	 /* remove from list */
   run->last_run = now; 	      /* mark as run */

   jcr = new_jcr(sizeof(JCR), dird_free_jcr);
   ASSERT(job);
   set_jcr_defaults(jcr, job);
   if (run->level) {
      jcr->JobLevel = run->level;	 /* override run level */
   }
   if (run->pool) {
      jcr->pool = run->pool;	      /* override pool */
   }
   if (run->storage) {
      jcr->store = run->storage;      /* override storage */
   }
   if (run->msgs) {
      jcr->messages = run->msgs;      /* override messages */
   }
   if (run->Priority) {
      jcr->JobPriority = run->Priority;
   }
   Dmsg0(200, "Leave wait_for_next_job()\n");
   return jcr;
}


/*
 * Shutdown the scheduler  
 */
void term_scheduler()
{
   /* Release all queued job entries to be run */
   for (void *je=NULL; (je=jobs_to_run->next(je)); ) {
      free(je);
   }
   delete jobs_to_run;
}

/*	    
 * Find all jobs to be run this hour and the next hour.
 */
static void find_runs()
{
   time_t now, next_hour, runtime;
   RUN *run;
   JOB *job;
   SCHED *sched;
   struct tm tm;
   int minute;
   int hour, mday, wday, month, wom, woy;
   /* Items corresponding to above at the next hour */
   int nh_hour, nh_mday, nh_wday, nh_month, nh_wom, nh_woy, nh_year;

   Dmsg0(200, "enter find_runs()\n");

   
   /* compute values for time now */
   now = time(NULL);
   localtime_r(&now, &tm);
   hour = tm.tm_hour;
   minute = tm.tm_min;
   mday = tm.tm_mday - 1;
   wday = tm.tm_wday;
   month = tm.tm_mon;
   wom = tm_wom(tm.tm_mday, tm.tm_wday);  /* get week of month */
   woy = tm_woy(now);			  /* get week of year */

   /* 
    * Compute values for next hour from now.
    * We do this to be sure we don't miss a job while
    * sleeping.
    */
   next_hour = now + 3600;  
   localtime_r(&next_hour, &tm);
   nh_hour = tm.tm_hour;
   minute = tm.tm_min;
   nh_mday = tm.tm_mday - 1;
   nh_wday = tm.tm_wday;
   nh_month = tm.tm_mon;
   nh_year  = tm.tm_year;
   nh_wom = tm_wom(tm.tm_mday, tm.tm_wday);  /* get week of month */
   nh_woy = tm_woy(now);		     /* get week of year */

   /* Loop through all jobs */
   LockRes();
   for (job=NULL; (job=(JOB *)GetNextRes(R_JOB, (RES *)job)); ) {
      sched = job->schedule;
      if (sched == NULL) {	      /* scheduled? */
	 continue;		      /* no, skip this job */
      }
      for (run=sched->run; run; run=run->next) {
	 bool run_now, run_nh;

	 /* Find runs scheduled between now and the next
	  * check time (typically 60 seconds)
	  */
	 run_now = bit_is_set(hour, run->hour) &&
	    (bit_is_set(mday, run->mday) || bit_is_set(wday, run->wday)) &&
	    bit_is_set(month, run->month) &&
	    bit_is_set(wom, run->wom) &&
	    bit_is_set(woy, run->woy);

	 run_nh = bit_is_set(nh_hour, run->hour) &&
	    (bit_is_set(nh_mday, run->mday) || bit_is_set(nh_wday, run->wday)) &&
	    bit_is_set(nh_month, run->month) &&
	    bit_is_set(nh_wom, run->wom) &&
	    bit_is_set(nh_woy, run->woy);

	 /* find time (time_t) job is to be run */
	 localtime_r(&now, &tm);      /* reset tm structure */
	 tm.tm_min = run->minute;     /* set run minute */
	 tm.tm_sec = 0; 	      /* zero secs */
	 if (run_now) {
	    runtime = mktime(&tm);
	    add_job(job, run, now, runtime);
	 }
	 /* If job is to be run in the next hour schedule it */
	 if (run_nh) {
	    /* Set correct values */
	    tm.tm_hour = nh_hour;
	    tm.tm_mday = nh_mday;
	    tm.tm_mon = nh_month;
	    tm.tm_year = nh_year;
	    runtime = mktime(&tm);
	    add_job(job, run, now, runtime);
	 }
      }  
   }
   UnlockRes();
   Dmsg0(200, "Leave find_runs()\n");
}

static void add_job(JOB *job, RUN *run, time_t now, time_t runtime)
{
   /*
    * Don't run any job that ran less than a minute ago, but
    *  do run any job scheduled less than a minute ago.
    */
   if ((runtime - run->last_run < 61) || (runtime+59 < now)) {
      return;
   }
   /* accept to run this job */
   job_item *je = (job_item *)malloc(sizeof(job_item));
   je->run = run;
   je->job = job;
   je->runtime = runtime;
   /* ***FIXME**** (enhancement) insert by sorted runtime */
   jobs_to_run->append(je);
}
