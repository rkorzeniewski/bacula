/*
 *
 *   Bacula Director Job processing routines
 *
 *     Kern Sibbald, October MM
 *
 *    Version $Id$
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
static void *job_thread(void *arg);

/* Exported subroutines */


/* Imported subroutines */
extern void term_scheduler();
extern void term_ua_server();
extern int do_backup(JCR *jcr);
extern int do_admin(JCR *jcr);
extern int do_restore(JCR *jcr);
extern int do_verify(JCR *jcr);

jobq_t	job_queue;

void init_job_server(int max_workers)
{
   int stat;
   if ((stat = jobq_init(&job_queue, max_workers, job_thread)) != 0) {
      Emsg1(M_ABORT, 0, _("Could not init job queue: ERR=%s\n"), strerror(stat));
   }
   return;
}

/*
 * Run a job -- typically called by the scheduler, but may also
 *		be called by the UA (Console program).
 *
 */
void run_job(JCR *jcr)
{
   int stat, errstat;

   sm_check(__FILE__, __LINE__, True);
   init_msg(jcr, jcr->messages);
   create_unique_job_name(jcr, jcr->job->hdr.name);
   set_jcr_job_status(jcr, JS_Created);
   jcr->jr.SchedTime = jcr->sched_time;
   jcr->jr.StartTime = jcr->start_time;
   jcr->jr.EndTime = 0; 	      /* perhaps rescheduled, clear it */
   jcr->jr.Type = jcr->JobType;
   jcr->jr.Level = jcr->JobLevel;
   jcr->jr.JobStatus = jcr->JobStatus;
   bstrncpy(jcr->jr.Name, jcr->job->hdr.name, sizeof(jcr->jr.Name));
   bstrncpy(jcr->jr.Job, jcr->Job, sizeof(jcr->jr.Job));

   /* Initialize termination condition variable */
   if ((errstat = pthread_cond_init(&jcr->term_wait, NULL)) != 0) {
      Jmsg1(jcr, M_FATAL, 0, _("Unable to init job cond variable: ERR=%s\n"), strerror(errstat));
      set_jcr_job_status(jcr, JS_ErrorTerminated);
      free_jcr(jcr);
      return;
   }

   /*
    * Open database
    */
   Dmsg0(50, "Open database\n");
   jcr->db=db_init_database(jcr, jcr->catalog->db_name, jcr->catalog->db_user,
			    jcr->catalog->db_password, jcr->catalog->db_address,
			    jcr->catalog->db_port, jcr->catalog->db_socket);
   if (!jcr->db || !db_open_database(jcr, jcr->db)) {
      Jmsg(jcr, M_FATAL, 0, _("Could not open database \"%s\".\n"),
		 jcr->catalog->db_name);
      if (jcr->db) {
         Jmsg(jcr, M_FATAL, 0, "%s", db_strerror(jcr->db));
      }
      set_jcr_job_status(jcr, JS_ErrorTerminated);
      free_jcr(jcr);
      return;
   }
   Dmsg0(50, "DB opened\n");

   /*
    * Create Job record  
    */
   jcr->jr.JobStatus = jcr->JobStatus;
   if (!db_create_job_record(jcr, jcr->db, &jcr->jr)) {
      Jmsg(jcr, M_FATAL, 0, "%s", db_strerror(jcr->db));
      set_jcr_job_status(jcr, JS_ErrorTerminated);
      free_jcr(jcr);
      return;
   }
   jcr->JobId = jcr->jr.JobId;
   ASSERT(jcr->jr.JobId > 0);

   Dmsg4(50, "Created job record JobId=%d Name=%s Type=%c Level=%c\n", 
       jcr->JobId, jcr->Job, jcr->jr.Type, jcr->jr.Level);
   Dmsg0(200, "Add jrc to work queue\n");

   /* Queue the job to be run */
   if ((stat = jobq_add(&job_queue, jcr)) != 0) {
      Emsg1(M_ABORT, 0, _("Could not add job queue: ERR=%s\n"), strerror(stat));
   }
   Dmsg0(100, "Done run_job()\n");
}

/* 
 * This is the engine called by job_add() when we were pulled		     
 *  from the work queue.
 *  At this point, we are running in our own thread and all
 *    necessary resources are allocated -- see jobq.c
 */
static void *job_thread(void *arg)
{
   JCR *jcr = (JCR *)arg;

   pthread_detach(pthread_self());
   sm_check(__FILE__, __LINE__, True);

   for ( ;; ) {

      Dmsg0(200, "=====Start Job=========\n");
      jcr->start_time = time(NULL);	 /* set the real start time */
      set_jcr_job_status(jcr, JS_Running);

      if (job_canceled(jcr)) {
	 update_job_end_record(jcr);
      } else if (jcr->job->MaxStartDelay != 0 && jcr->job->MaxStartDelay <
	  (utime_t)(jcr->start_time - jcr->sched_time)) {
         Jmsg(jcr, M_FATAL, 0, _("Job canceled because max start delay time exceeded.\n"));
	 set_jcr_job_status(jcr, JS_Canceled);
	 update_job_end_record(jcr);
      } else {

	 /* Run Job */
	 if (jcr->job->RunBeforeJob) {
	    POOLMEM *before = get_pool_memory(PM_FNAME);
	    int status;
	    BPIPE *bpipe;
	    char line[MAXSTRING];
	    
            before = edit_job_codes(jcr, before, jcr->job->RunBeforeJob, "");
            bpipe = open_bpipe(before, 0, "r");
	    free_pool_memory(before);
	    while (fgets(line, sizeof(line), bpipe->rfd)) {
               Jmsg(jcr, M_INFO, 0, _("RunBefore: %s"), line);
	    }
	    status = close_bpipe(bpipe);
	    if (status != 0) {
               Jmsg(jcr, M_FATAL, 0, _("RunBeforeJob returned non-zero status=%d\n"),
		  status);
	       set_jcr_job_status(jcr, JS_FatalError);
	       update_job_end_record(jcr);
	       goto bail_out;
	    }
	 }
	 switch (jcr->JobType) {
	 case JT_BACKUP:
	    do_backup(jcr);
	    if (jcr->JobStatus == JS_Terminated) {
	       do_autoprune(jcr);
	    }
	    break;
	 case JT_VERIFY:
	    do_verify(jcr);
	    if (jcr->JobStatus == JS_Terminated) {
	       do_autoprune(jcr);
	    }
	    break;
	 case JT_RESTORE:
	    do_restore(jcr);
	    if (jcr->JobStatus == JS_Terminated) {
	       do_autoprune(jcr);
	    }
	    break;
	 case JT_ADMIN:
	    do_admin(jcr);
	    if (jcr->JobStatus == JS_Terminated) {
	       do_autoprune(jcr);
	    }
	    break;
	 default:
            Pmsg1(0, "Unimplemented job type: %d\n", jcr->JobType);
	    break;
	 }
	 if (jcr->job->RunAfterJob) {
	    POOLMEM *after = get_pool_memory(PM_FNAME);
	    int status;
	    BPIPE *bpipe;
	    char line[MAXSTRING];
	    
            after = edit_job_codes(jcr, after, jcr->job->RunAfterJob, "");
            bpipe = open_bpipe(after, 0, "r");
	    free_pool_memory(after);
	    while (fgets(line, sizeof(line), bpipe->rfd)) {
               Jmsg(jcr, M_INFO, 0, _("RunAfter: %s"), line);
	    }
	    status = close_bpipe(bpipe);
	    if (status != 0) {
               Jmsg(jcr, M_FATAL, 0, _("RunAfterJob returned non-zero status=%d\n"),
		  status);
	       set_jcr_job_status(jcr, JS_FatalError);
	       update_job_end_record(jcr);
	    }
	 }
      }
bail_out:
      break;
   }

   Dmsg0(50, "======== End Job ==========\n");
   sm_check(__FILE__, __LINE__, True);
   return NULL;
}


/*
 * Get or create a Client record for this Job
 */
int get_or_create_client_record(JCR *jcr)
{
   CLIENT_DBR cr;

   memset(&cr, 0, sizeof(cr));
   bstrncpy(cr.Name, jcr->client->hdr.name, sizeof(cr.Name));
   cr.AutoPrune = jcr->client->AutoPrune;
   cr.FileRetention = jcr->client->FileRetention;
   cr.JobRetention = jcr->client->JobRetention;
   if (!jcr->client_name) {
      jcr->client_name = get_pool_memory(PM_NAME);
   }
   pm_strcpy(&jcr->client_name, jcr->client->hdr.name);
   if (!db_create_client_record(jcr, jcr->db, &cr)) {
      Jmsg(jcr, M_FATAL, 0, _("Could not create Client record. ERR=%s\n"), 
	 db_strerror(jcr->db));
      return 0;
   }
   jcr->jr.ClientId = cr.ClientId;
   if (cr.Uname[0]) {
      if (!jcr->client_uname) {
	 jcr->client_uname = get_pool_memory(PM_NAME);
      }
      pm_strcpy(&jcr->client_uname, cr.Uname);
   }
   Dmsg2(100, "Created Client %s record %d\n", jcr->client->hdr.name, 
      jcr->jr.ClientId);
   return 1;
}


/*
 * Write status and such in DB
 */
void update_job_end_record(JCR *jcr)
{
   if (jcr->jr.EndTime == 0) {
      jcr->jr.EndTime = time(NULL);
   }
   jcr->end_time = jcr->jr.EndTime;
   jcr->jr.JobId = jcr->JobId;
   jcr->jr.JobStatus = jcr->JobStatus;
   jcr->jr.JobFiles = jcr->JobFiles;
   jcr->jr.JobBytes = jcr->JobBytes;
   jcr->jr.VolSessionId = jcr->VolSessionId;
   jcr->jr.VolSessionTime = jcr->VolSessionTime;
   if (!db_update_job_end_record(jcr, jcr->db, &jcr->jr)) {
      Jmsg(jcr, M_WARNING, 0, _("Error updating job record. %s"), 
	 db_strerror(jcr->db));
   }
}

/*
 * Takes base_name and appends (unique) current
 *   date and time to form unique job name.
 *
 *  Returns: unique job name in jcr->Job
 *    date/time in jcr->start_time
 */
void create_unique_job_name(JCR *jcr, char *base_name)
{
   /* Job start mutex */
   static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
   static time_t last_start_time = 0;
   time_t now;
   struct tm tm;
   char dt[MAX_TIME_LENGTH];
   char name[MAX_NAME_LENGTH];
   char *p;

   /* Guarantee unique start time -- maximum one per second, and
    * thus unique Job Name 
    */
   P(mutex);			      /* lock creation of jobs */
   now = time(NULL);
   while (now == last_start_time) {
      bmicrosleep(0, 500000);
      now = time(NULL);
   }
   last_start_time = now;
   V(mutex);			      /* allow creation of jobs */
   jcr->start_time = now;
   /* Form Unique JobName */
   localtime_r(&now, &tm);
   /* Use only characters that are permitted in Windows filenames */
   strftime(dt, sizeof(dt), "%Y-%m-%d_%H.%M.%S", &tm); 
   bstrncpy(name, base_name, sizeof(name));
   name[sizeof(name)-22] = 0;	       /* truncate if too long */
   bsnprintf(jcr->Job, sizeof(jcr->Job), "%s.%s", name, dt); /* add date & time */
   /* Convert spaces into underscores */
   for (p=jcr->Job; *p; p++) {
      if (*p == ' ') {
         *p = '_';
      }
   }
}

/*
 * Free the Job Control Record if no one is still using it.
 *  Called from main free_jcr() routine in src/lib/jcr.c so
 *  that we can do our Director specific cleanup of the jcr.
 */
void dird_free_jcr(JCR *jcr)
{
   Dmsg0(200, "Start dird free_jcr\n");

   if (jcr->sd_auth_key) {
      free(jcr->sd_auth_key);
      jcr->sd_auth_key = NULL;
   }
   if (jcr->where) {
      free(jcr->where);
      jcr->where = NULL;
   }
   if (jcr->file_bsock) {
      Dmsg0(200, "Close File bsock\n");
      bnet_close(jcr->file_bsock);
      jcr->file_bsock = NULL;
   }
   if (jcr->store_bsock) {
      Dmsg0(200, "Close Store bsock\n");
      bnet_close(jcr->store_bsock);
      jcr->store_bsock = NULL;
   }
   if (jcr->fname) {  
      Dmsg0(200, "Free JCR fname\n");
      free_pool_memory(jcr->fname);
      jcr->fname = NULL;
   }
   if (jcr->stime) {
      Dmsg0(200, "Free JCR stime\n");
      free_pool_memory(jcr->stime);
      jcr->stime = NULL;
   }
   if (jcr->RestoreBootstrap) {
      free(jcr->RestoreBootstrap);
      jcr->RestoreBootstrap = NULL;
   }
   if (jcr->client_uname) {
      free_pool_memory(jcr->client_uname);
      jcr->client_uname = NULL;
   }
   pthread_cond_destroy(&jcr->term_wait);
   Dmsg0(200, "End dird free_jcr\n");
}

/*
 * Set some defaults in the JCR necessary to
 * run. These items are pulled from the job
 * definition as defaults, but can be overridden
 * later either by the Run record in the Schedule resource,
 * or by the Console program.
 */
void set_jcr_defaults(JCR *jcr, JOB *job)
{
   jcr->job = job;
   jcr->JobType = job->JobType;
   jcr->JobLevel = job->level;
   jcr->JobPriority = job->Priority;
   jcr->store = job->storage;
   jcr->client = job->client;
   if (!jcr->client_name) {
      jcr->client_name = get_pool_memory(PM_NAME);
   }
   pm_strcpy(&jcr->client_name, jcr->client->hdr.name);
   jcr->pool = job->pool;
   jcr->catalog = job->client->catalog;
   jcr->fileset = job->fileset;
   jcr->messages = job->messages; 
   if (jcr->RestoreBootstrap) {
      free(jcr->RestoreBootstrap);
   }
   /* This can be overridden by Console program */
   if (job->RestoreBootstrap) {
      jcr->RestoreBootstrap = bstrdup(job->RestoreBootstrap);
   }
   /* If no default level given, set one */
   if (jcr->JobLevel == 0) {
      switch (jcr->JobType) {
      case JT_VERIFY:
	 jcr->JobLevel = L_VERIFY_CATALOG;
	 break;
      case JT_BACKUP:
	 jcr->JobLevel = L_INCREMENTAL;
	 break;
      case JT_RESTORE:
      case JT_ADMIN:
	 jcr->JobLevel = L_FULL;
	 break;
      default:
	 break;
      }
   }
}
