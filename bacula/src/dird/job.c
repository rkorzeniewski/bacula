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
static char *edit_run_codes(JCR *jcr, char *omsg, char *imsg);
static void release_resource_locks(JCR *jcr);
static int acquire_resource_locks(JCR *jcr);
#ifdef USE_SEMAPHORE
static void backoff_resource_locks(JCR *jcr, int count);
#endif

/* Exported subroutines */
void run_job(JCR *jcr);


/* Imported subroutines */
extern void term_scheduler();
extern void term_ua_server();
extern int do_backup(JCR *jcr);
extern int do_restore(JCR *jcr);
extern int do_verify(JCR *jcr);
extern void backup_cleanup(void);

#ifdef USE_SEMAPHORE
static semlock_t job_lock;
static pthread_mutex_t mutex;
static pthread_cond_t  resource_wait;
static int waiting = 0; 	      /* count of waiting threads */
#else
/* Queue of jobs to be run */
workq_t job_wq; 		  /* our job work queue */
#endif

void init_job_server(int max_workers)
{
   int stat;
#ifdef USE_SEMAPHORE
   if ((stat = sem_init(&job_lock, max_workers)) != 0) {
      Emsg1(M_ABORT, 0, _("Could not init job lock: ERR=%s\n"), strerror(stat));
   }
   if ((stat = pthread_mutex_init(&mutex, NULL)) != 0) {
      Emsg1(M_ABORT, 0, _("Could not init resource mutex: ERR=%s\n"), strerror(stat));
   }
   if ((stat = pthread_cond_init(&resource_wait, NULL)) != 0) {
      Emsg1(M_ABORT, 0, _("Could not init resource wait: ERR=%s\n"), strerror(stat));
   }

#else
   if ((stat = workq_init(&job_wq, max_workers, job_thread)) != 0) {
      Emsg1(M_ABORT, 0, _("Could not init job work queue: ERR=%s\n"), strerror(stat));
   }
#endif
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
#ifdef USE_SEMAPHORE
   pthread_t tid;
#else
   workq_ele_t *work_item;
#endif

   sm_check(__FILE__, __LINE__, True);
   init_msg(jcr, jcr->messages);
   create_unique_job_name(jcr, jcr->job->hdr.name);
   jcr->jr.SchedTime = jcr->sched_time;
   jcr->jr.StartTime = jcr->start_time;
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
   if (!db_open_database(jcr, jcr->db)) {
      Jmsg(jcr, M_FATAL, 0, "%s", db_strerror(jcr->db));
      db_close_database(jcr, jcr->db);
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
      db_close_database(jcr, jcr->db);
      set_jcr_job_status(jcr, JS_ErrorTerminated);
      free_jcr(jcr);
      return;
   }
   jcr->JobId = jcr->jr.JobId;
   ASSERT(jcr->jr.JobId > 0);

   Dmsg4(30, "Created job record JobId=%d Name=%s Type=%c Level=%c\n", 
       jcr->JobId, jcr->Job, jcr->jr.Type, jcr->jr.Level);
   Dmsg0(200, "Add jrc to work queue\n");

#ifdef USE_SEMAPHORE
  if ((stat = pthread_create(&tid, NULL, job_thread, (void *)jcr)) != 0) {
      Emsg1(M_ABORT, 0, _("Unable to create job thread: ERR=%s\n"), strerror(stat));
   }
#else
   /* Queue the job to be run */
   if ((stat = workq_add(&job_wq, (void *)jcr, &work_item, 0)) != 0) {
      Emsg1(M_ABORT, 0, _("Could not add job to work queue: ERR=%s\n"), strerror(stat));
   }
   jcr->work_item = work_item;
#endif
   Dmsg0(200, "Done run_job()\n");
}

/* 
 * This is the engine called by workq_add() when we were pulled 	       
 *  from the work queue.
 *  At this point, we are running in our own thread 
 */
static void *job_thread(void *arg)
{
   JCR *jcr = (JCR *)arg;

   pthread_detach(pthread_self());
   sm_check(__FILE__, __LINE__, True);

   if (!acquire_resource_locks(jcr)) {
      set_jcr_job_status(jcr, JS_Canceled);
   }

   Dmsg0(200, "=====Start Job=========\n");
   jcr->start_time = time(NULL);      /* set the real start time */
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
	 
	 before = edit_run_codes(jcr, before, jcr->job->RunBeforeJob);
	 status = run_program(before, 0, NULL);
	 if (status != 0) {
            Jmsg(jcr, M_FATAL, 0, _("RunBeforeJob returned non-zero status=%d\n"),
	       status);
	    set_jcr_job_status(jcr, JS_FatalError);
	    update_job_end_record(jcr);
	    free_pool_memory(before);
	    goto bail_out;
	 }
	 free_pool_memory(before);
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
	    /* No actual job */
	    do_autoprune(jcr);
	    set_jcr_job_status(jcr, JS_Terminated);
	    break;
	 default:
            Pmsg1(0, "Unimplemented job type: %d\n", jcr->JobType);
	    break;
	 }
      if (jcr->job->RunAfterJob) {
	 POOLMEM *after = get_pool_memory(PM_FNAME);
	 int status;
      
	 after = edit_run_codes(jcr, after, jcr->job->RunAfterJob);
	 status = run_program(after, 0, NULL);
	 if (status != 0) {
            Jmsg(jcr, M_FATAL, 0, _("RunAfterJob returned non-zero status=%d\n"),
	       status);
	    set_jcr_job_status(jcr, JS_FatalError);
	    update_job_end_record(jcr);
	 }
	 free_pool_memory(after);
      }
   }
bail_out:
   release_resource_locks(jcr);
   Dmsg0(50, "Before free jcr\n");
   free_jcr(jcr);
   Dmsg0(50, "======== End Job ==========\n");
   sm_check(__FILE__, __LINE__, True);
   return NULL;
}

/*
 * Acquire the resources needed. These locks limit the
 *  number of jobs by each resource. We have limits on
 *  Jobs, Clients, Storage, and total jobs.
 */
static int acquire_resource_locks(JCR *jcr)
{
   time_t now = time(NULL);

   /* Wait until scheduled time arrives */
   while (jcr->sched_time > now) {
      Dmsg2(100, "Waiting on sched time, jobid=%d secs=%d\n", jcr->JobId,
	    jcr->sched_time - now);
      bmicrosleep(jcr->sched_time - now, 0);
      now = time(NULL);
   }


#ifdef USE_SEMAPHORE
   int stat;

   /* Initialize semaphores */
   if (jcr->store->sem.valid != SEMLOCK_VALID) {
      if ((stat = sem_init(&jcr->store->sem, jcr->store->MaxConcurrentJobs)) != 0) {
         Emsg1(M_ABORT, 0, _("Could not init Storage semaphore: ERR=%s\n"), strerror(stat));
      }
   }
   if (jcr->client->sem.valid != SEMLOCK_VALID) {
      if ((stat = sem_init(&jcr->client->sem, jcr->client->MaxConcurrentJobs)) != 0) {
         Emsg1(M_ABORT, 0, _("Could not init Client semaphore: ERR=%s\n"), strerror(stat));
      }
   }
   if (jcr->job->sem.valid != SEMLOCK_VALID) {
      if ((stat = sem_init(&jcr->job->sem, jcr->job->MaxConcurrentJobs)) != 0) {
         Emsg1(M_ABORT, 0, _("Could not init Job semaphore: ERR=%s\n"), strerror(stat));
      }
   }

   for ( ;; ) {
      /* Acquire semaphore */
      set_jcr_job_status(jcr, JS_WaitJobRes);
      if ((stat = sem_lock(&jcr->job->sem)) != 0) {
         Emsg1(M_ABORT, 0, _("Could not acquire Job max jobs lock: ERR=%s\n"), strerror(stat));
      }
      set_jcr_job_status(jcr, JS_WaitClientRes);
      if ((stat = sem_trylock(&jcr->client->sem)) != 0) {
	 if (stat == EBUSY) {
	    backoff_resource_locks(jcr, 1);
	    goto wait;
	 } else {
            Emsg1(M_ABORT, 0, _("Could not acquire Client max jobs lock: ERR=%s\n"), strerror(stat));
	 }
      }
      set_jcr_job_status(jcr, JS_WaitStoreRes);
      if ((stat = sem_trylock(&jcr->store->sem)) != 0) {
	 if (stat == EBUSY) {
	    backoff_resource_locks(jcr, 2);
	    goto wait;
	 } else {
            Emsg1(M_ABORT, 0, _("Could not acquire Storage max jobs lock: ERR=%s\n"), strerror(stat));
	 }
      }
      set_jcr_job_status(jcr, JS_WaitMaxJobs);
      if ((stat = sem_trylock(&job_lock)) != 0) {
	 if (stat == EBUSY) {
	    backoff_resource_locks(jcr, 3);
	    goto wait;
	 } else {
            Emsg1(M_ABORT, 0, _("Could not acquire max jobs lock: ERR=%s\n"), strerror(stat));
	 }
      }
      break;

wait:
      P(mutex);
      /*
       * Wait for a resource to be released either by backoff or
       *  by a job terminating.
       */
      waiting++;
      pthread_cond_wait(&resource_wait, &mutex);
      waiting--;
      V(mutex);
      /* Try again */
   }
#endif
   return 1;
}

#ifdef USE_SEMAPHORE
/*
 * We could not get all the resource locks because 
 *  too many jobs are running, so release any locks
 *  we did acquire, giving others a chance to use them
 *  while we wait.
 */
static void backoff_resource_locks(JCR *jcr, int count)
{
   P(mutex);
   switch (count) {
   case 3:
      sem_unlock(&jcr->store->sem);
      /* Fall through wanted */
   case 2:
      sem_unlock(&jcr->client->sem);
      /* Fall through wanted */
   case 1:
      sem_unlock(&jcr->job->sem);
      break;
   }
   /*
    * Since we released a lock, if there are any threads
    *  waiting, wake them up so that they can try again.
    */
   if (waiting > 0) {
      pthread_cond_broadcast(&resource_wait);
   }
   V(mutex);
}
#endif

/*
 * This is called at the end of the job to release
 *   any resource limits on the number of jobs. If
 *   there are any other jobs waiting, we wake them
 *   up so that they can try again.
 */
static void release_resource_locks(JCR *jcr)
{
#ifdef USE_SEMAPHORE
   P(mutex);
   sem_unlock(&jcr->store->sem);
   sem_unlock(&jcr->client->sem);
   sem_unlock(&jcr->job->sem);
   sem_unlock(&job_lock);
   if (waiting > 0) {
      pthread_cond_broadcast(&resource_wait);
   }
   V(mutex);
#endif
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
   if (jcr->client_name) {
      free_pool_memory(jcr->client_name);
   }
   jcr->client_name = get_memory(strlen(jcr->client->hdr.name) + 1);
   strcpy(jcr->client_name, jcr->client->hdr.name);
   if (!db_create_client_record(jcr, jcr->db, &cr)) {
      Jmsg(jcr, M_FATAL, 0, _("Could not create Client record. ERR=%s\n"), 
	 db_strerror(jcr->db));
      return 0;
   }
   jcr->jr.ClientId = cr.ClientId;
   if (cr.Uname[0]) {
      if (jcr->client_uname) {
	 free_pool_memory(jcr->client_uname);
      }
      jcr->client_uname = get_memory(strlen(cr.Uname) + 1);
      strcpy(jcr->client_uname, cr.Uname);
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
      db_close_database(jcr, jcr->db);
   }
   if (jcr->RestoreWhere) {
      free(jcr->RestoreWhere);
   }
   if (jcr->RestoreBootstrap) {
      free(jcr->RestoreBootstrap);
   }
   if (jcr->client_uname) {
      free_pool_memory(jcr->client_uname);
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
      default:
	 break;
      }
   }
}

/*
 * Edit codes into Run command
 *  %% = %
 *  %c = Client's name
 *  %d = Director's name
 *  %i = JobId
 *  %e = Job Exit
 *  %j = Job
 *  %l = Job Level
 *  %n = Job name
 *  %t = Job type
 *
 *  omsg = edited output message
 *  imsg = input string containing edit codes (%x)
 *
 */
static char *edit_run_codes(JCR *jcr, char *omsg, char *imsg) 
{
   char *p;
   const char *str;
   char add[20];

   *omsg = 0;
   Dmsg1(200, "edit_run_codes: %s\n", imsg);
   for (p=imsg; *p; p++) {
      if (*p == '%') {
	 switch (*++p) {
         case '%':
            str = "%";
	    break;
         case 'c':
	    str = jcr->client_name;
	    if (!str) {
               str = "";
	    }
	    break;
         case 'd':
	    str = my_name;
	    break;
         case 'e':
	    str = job_status_to_str(jcr->JobStatus);
	    break;
         case 'i':
            sprintf(add, "%d", jcr->JobId);
	    str = add;
	    break;
         case 'j':                    /* Job */
	    str = jcr->Job;
	    break;
         case 'l':
	    str = job_level_to_str(jcr->JobLevel);
	    break;
         case 'n':
	    str = jcr->job->hdr.name;
	    break;
         case 't':
	    str = job_type_to_str(jcr->JobType);
	    break;
	 default:
            add[0] = '%';
	    add[1] = *p;
	    add[2] = 0;
	    str = add;
	    break;
	 }
      } else {
	 add[0] = *p;
	 add[1] = 0;
	 str = add;
      }
      Dmsg1(200, "add_str %s\n", str);
      pm_strcat(&omsg, (char *)str);
      Dmsg1(200, "omsg=%s\n", omsg);
   }
   return omsg;
}
