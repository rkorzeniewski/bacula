/*
 *
 *   Bacula scheduler
 *     It looks at what jobs are to be run and when
 *     and waits around until it is time to 
 *     fire them up.
 *
 *     Kern Sibbald, May MM
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
#include "dird.h"


/* Forward referenced subroutines */
static void find_runs();

/* Imported subroutines */

/* Imported variables */

/* Local variables */
typedef struct {
   RUN *run;
   JOB *job;
   time_t runtime;
} RUNJOB;

static int num_runjobs; 	      /* total jobs found by find_runs() */
static int rem_runjobs; 	      /* jobs remaining to be processed */
static int max_runjobs; 	      /* max jobs in runjobs array */
static RUNJOB *runjobs; 	      /* array of jobs to be run */


/*********************************************************************
 *
 *	   Main Bacula Scheduler
 *
 */
JCR *wait_for_next_job(char *job_to_run)
{
   JCR *jcr;
   JOB *job;
   RUN *run;
   time_t now, runtime, nexttime;
   int jobindex, i;
   static int first = TRUE;

   Dmsg0(200, "Enter wait_for_next_job\n");
   if (first) {
      first = FALSE;
      max_runjobs = 10;
      runjobs = (RUNJOB *) malloc(sizeof(RUNJOB) * max_runjobs);
      num_runjobs = 0;
      if (job_to_run) { 	      /* one shot */
	 job = (JOB *)GetResWithName(R_JOB, job_to_run);
	 if (!job) {
            Emsg1(M_ABORT, 0, _("Job %s not found\n"), job_to_run);
	 }
         Dmsg1(5, "Found job_to_run %s\n", job_to_run);
	 jcr = new_jcr(sizeof(JCR), dird_free_jcr);
	 set_jcr_defaults(jcr, job);
	 return jcr;
      }
      find_runs();
   }
   /* Wait until we have something in the
    * next hour or so.
    */
   while (num_runjobs == 0 || rem_runjobs == 0) {
      sleep(60);
      find_runs();
   }
   /* 
    * Sort through what is to be run in the next
    * two hours to find the first job to be run,
    * then wait around until it is time.
    *
    */
   time(&now);
   nexttime = now + 60 * 60 * 24;     /* a much later time */
   jobindex = -1;
   for (i=0; i<num_runjobs; i++) {
      runtime = runjobs[i].runtime;
      if (runtime > 0 && runtime < nexttime) { /* find minimum time job */
	 nexttime = runtime;
	 jobindex = i;
      }
   }
   if (jobindex < 0) {		      /* we really should have something now */
      Emsg0(M_ABORT, 0, _("Scheduler logic error\n"));
   }

   /* Now wait for the time to run the job */
   for (;;) {
      int twait;
      time(&now);
      twait = nexttime - now;
      if (twait <= 0)		      /* time to run it */
	 break;
      if (twait > 20)		      /* sleep max 20 seconds */
	 twait = 20;
      sleep(twait);
   }
   run = runjobs[jobindex].run;
   job = runjobs[jobindex].job;
   runjobs[jobindex].runtime = 0;     /* remove from list */
   run->last_run = now; 	      /* mark as run */
   rem_runjobs--;		      /* decrement count of remaining jobs */

   jcr = new_jcr(sizeof(JCR), dird_free_jcr);
   ASSERT(job);
   sm_check(__FILE__, __LINE__, False);
   set_jcr_defaults(jcr, job);
   if (run->level) {
      jcr->level = run->level;	      /* override run level */
   }
   if (run->pool) {
      jcr->pool = run->pool;	      /* override pool */
   }
   if (run->storage) {
      jcr->store = run->storage;      /* override storage */
   }
   if (run->msgs) {
      jcr->msgs = run->msgs;	      /* override messages */
   }
   Dmsg0(200, "Leave wait_for_next_job()\n");
   return jcr;
}


/*
 * Shutdown the scheduler  
 */
void term_scheduler()
{
   if (runjobs) {		      /* free allocated memory */
      free(runjobs);
      runjobs = NULL;
      max_runjobs = 0;
   }
}


/*	    
 * Find all jobs to be run this hour
 * and the next hour.
 */
static void find_runs()
{
   time_t now, runtime;
   RUN *run;
   JOB *job;
   SCHED *sched;
   struct tm tm;
   int hour, next_hour, minute, mday, wday, month;

   Dmsg0(200, "enter find_runs()\n");
   num_runjobs = 0;

   now = time(NULL);
   localtime_r(&now, &tm);
   
   hour = tm.tm_hour;
   next_hour = hour + 1;
   if (next_hour > 23)
      next_hour -= 24;
   minute = tm.tm_min;
   mday = tm.tm_mday - 1;
   wday = tm.tm_wday;
   month = tm.tm_mon;

   /* Loop through all jobs */
   LockRes();
   for (job=NULL; (job=(JOB *)GetNextRes(R_JOB, (RES *)job)); ) {
      sched = job->schedule;
      if (sched == NULL) {	      /* scheduled? */
	 continue;		      /* no, skip this job */
      }
      for (run=sched->run; run; run=run->next) {

	 if (now - run->last_run < 60 * 20)
	    continue;		      /* wait at least 20 minutes */

	 /* Find runs scheduled in this our or in the
	  * next hour (we may be one second before the next hour).
	  */
	 if ((bit_is_set(hour, run->hour) || bit_is_set(next_hour, run->hour)) &&
	     (bit_is_set(mday, run->mday) || bit_is_set(wday, run->wday)) && 
	     bit_is_set(month, run->month)) {

	    /* find time (time_t) job is to be run */
	    localtime_r(&now, &tm);
	    if (bit_is_set(next_hour, run->hour))
	       tm.tm_hour++;
	    if (tm.tm_hour > 23)
	       tm.tm_hour = 0;
	    tm.tm_min = run->minute;
	    tm.tm_sec = 0;
	    runtime = mktime(&tm);
	    if (runtime < (now - 5 * 60)) /* give 5 min grace to pickup straglers */
	       continue;
	    /* Make sure array is big enough */
	    if (num_runjobs == max_runjobs) {
	       max_runjobs += 10;
	       runjobs = (RUNJOB *) realloc(runjobs, sizeof(RUNJOB) * max_runjobs);
	       if (!runjobs)
                  Emsg0(M_ABORT, 0, _("Out of memory\n"));
	    }
	    /* accept to run this job */
	    runjobs[num_runjobs].run = run;
	    runjobs[num_runjobs].job = job;
	    runjobs[num_runjobs++].runtime = runtime; /* when to run it */

	 }
      }  
   }

   UnlockRes();
   rem_runjobs = num_runjobs;
   Dmsg0(200, "Leave find_runs()\n");
}
