/*
 *
 *   Bacula Director Job processing routines
 *
 *     Kern Sibbald, October MM
 *
 *    Version $Id$
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
static void job_thread(void *arg);

/* Exported subroutines */
void run_job(JCR *jcr);
void init_job_server(int max_workers);


/* Imported subroutines */
extern void term_scheduler();
extern void term_ua_server();
extern int do_backup(JCR *jcr);
extern int do_restore(JCR *jcr);
extern int do_verify(JCR *jcr);
extern void backup_cleanup(void);
extern void start_UA_server(int port);

/* Queue of jobs to be run */
static workq_t job_wq;		      /* our job work queue */


void init_job_server(int max_workers)
{
   int stat;

   if ((stat = workq_init(&job_wq, max_workers, job_thread)) != 0) {
      Emsg1(M_ABORT, 0, _("Could not init job work queue: ERR=%s\n"), strerror(stat));
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

   init_msg(jcr, jcr->msgs);
   create_unique_job_name(jcr, jcr->job->hdr.name);
   jcr->jr.SchedTime = jcr->sched_time;
   jcr->jr.StartTime = jcr->start_time;
   jcr->jr.Type = jcr->JobType;
   jcr->jr.Level = jcr->JobLevel;
   strcpy(jcr->jr.Name, jcr->job->hdr.name);
   strcpy(jcr->jr.Job, jcr->Job);

   /* Initialize termination condition variable */
   if ((errstat = pthread_cond_init(&jcr->term_wait, NULL)) != 0) {
      Jmsg1(jcr, M_FATAL, 0, _("Unable to init job cond variable: ERR=%s\n"), strerror(errstat));
      jcr->JobStatus = JS_ErrorTerminated;
      free_jcr(jcr);
      return;
   }

   /*
    * Open database
    */
   Dmsg0(50, "Open database\n");
   jcr->db=db_init_database(jcr->catalog->db_name, jcr->catalog->db_user,
			    jcr->catalog->db_password);
   if (!db_open_database(jcr->db)) {
      Jmsg(jcr, M_FATAL, 0, "%s", db_strerror(jcr->db));
      db_close_database(jcr->db);
      jcr->JobStatus = JS_ErrorTerminated;
      free_jcr(jcr);
      return;
   }
   Dmsg0(50, "DB opened\n");

   /*
    * Create Job record  
    */
   jcr->jr.JobStatus = jcr->JobStatus;
   if (!db_create_job_record(jcr->db, &jcr->jr)) {
      Jmsg(jcr, M_FATAL, 0, "%s", db_strerror(jcr->db));
      db_close_database(jcr->db);
      jcr->JobStatus = JS_ErrorTerminated;
      free_jcr(jcr);
      return;
   }
   jcr->JobId = jcr->jr.JobId;
   ASSERT(jcr->jr.JobId > 0);

   Dmsg4(30, "Created job record JobId=%d Name=%s Type=%c Level=%c\n", 
       jcr->JobId, jcr->Job, jcr->jr.Type, jcr->jr.Level);
   Dmsg0(200, "Add jrc to work queue\n");


   /* Queue the job to be run */
   if ((stat = workq_add(&job_wq, (void *)jcr)) != 0) {
      Emsg1(M_ABORT, 0, _("Could not add job to work queue: ERR=%s\n"), strerror(stat));
   }
   Dmsg0(200, "Done run_job()\n");
}

/* 
 * This is the engine called by workq_add() when we were pulled 	       
 *  from the work queue.
 *  At this point, we are running in our own thread 
 */
static void job_thread(void *arg)
{
   time_t now;
   JCR *jcr = (JCR *)arg;

   time(&now);

   Dmsg0(100, "=====Start Job=========\n");
   jcr->start_time = now;	      /* set the real start time */
   if (jcr->job->MaxStartDelay != 0 && jcr->job->MaxStartDelay <
       (btime_t)(jcr->start_time - jcr->sched_time)) {
      Jmsg(jcr, M_FATAL, 0, _("Job cancelled because max delay time exceeded.\n"));
      jcr->JobStatus = JS_ErrorTerminated;
      update_job_end_record(jcr);
   } else {

      /* Run Job */
      jcr->JobStatus = JS_Running;

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
	    /* No actual job */
	    do_autoprune(jcr);
	    break;
	 default:
            Dmsg1(0, "Unimplemented job type: %d\n", jcr->JobType);
	    break;
	 }
   }
   Dmsg0(50, "Before free jcr\n");
   free_jcr(jcr);
   Dmsg0(50, "======== End Job ==========\n");
}

/*
 * Get or create a Client record for this Job
 */
int get_or_create_client_record(JCR *jcr)
{
   CLIENT_DBR cr;

   memset(&cr, 0, sizeof(cr));
   strcpy(cr.Name, jcr->client->hdr.name);
   cr.AutoPrune = jcr->client->AutoPrune;
   cr.FileRetention = jcr->client->FileRetention;
   cr.JobRetention = jcr->client->JobRetention;
   if (jcr->client_name) {
      free_pool_memory(jcr->client_name);
   }
   jcr->client_name = get_memory(strlen(jcr->client->hdr.name) + 1);
   strcpy(jcr->client_name, jcr->client->hdr.name);
   if (!db_create_client_record(jcr->db, &cr)) {
      Jmsg(jcr, M_FATAL, 0, _("Could not create Client record. %s"), 
	 db_strerror(jcr->db));
      return 0;
   }
   jcr->jr.ClientId = cr.ClientId;
   Dmsg2(9, "Created Client %s record %d\n", jcr->client->hdr.name, 
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
   if (!db_update_job_end_record(jcr->db, &jcr->jr)) {
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
   time(&now);
   while (now == last_start_time) {
      sleep(1);
      time(&now);
   }
   last_start_time = now;
   V(mutex);			      /* allow creation of jobs */
   jcr->start_time = now;
   /* Form Unique JobName */
   localtime_r(&now, &tm);
   strftime(dt, sizeof(dt), "%Y-%m-%d.%H:%M:%S", &tm); 
   strncpy(name, base_name, sizeof(name));
   name[sizeof(name)-22] = 0;	       /* truncate if too long */
   sprintf(jcr->Job, "%s.%s", name, dt); /* add date & time */
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

   if (jcr->file_bsock) {
      Dmsg0(200, "Close File bsock\n");
      bnet_close(jcr->file_bsock);
   }
   if (jcr->store_bsock) {
      Dmsg0(200, "Close Store bsock\n");
      bnet_close(jcr->store_bsock);
   }
   if (jcr->fname) {  
      Dmsg0(200, "Free JCR fname\n");
      free_pool_memory(jcr->fname);
   }
   if (jcr->stime) {
      Dmsg0(200, "Free JCR stime\n");
      free_pool_memory(jcr->stime);
   }
   if (jcr->db) {
      Dmsg0(200, "Close DB\n");
      db_close_database(jcr->db);
   }
   if (jcr->RestoreWhere) {
      free(jcr->RestoreWhere);
   }
   if (jcr->RestoreBootstrap) {
      free(jcr->RestoreBootstrap);
   }
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
   jcr->store = job->storage;
   jcr->client = job->client;
   if (jcr->client_name) {
      free_pool_memory(jcr->client_name);
   }
   jcr->client_name = get_memory(strlen(jcr->client->hdr.name) + 1);
   strcpy(jcr->client_name, jcr->client->hdr.name);
   jcr->pool = job->pool;
   jcr->catalog = job->client->catalog;
   jcr->fileset = job->fs;
   jcr->msgs = job->messages; 
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
      default:
	 break;
      }
   }
}
