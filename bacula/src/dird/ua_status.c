/*
 *
 *   Bacula Director -- User Agent Status Command
 *
 *     Kern Sibbald, August MMI
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

extern char my_name[];
extern time_t daemon_start_time;
extern struct s_last_job last_job;

static void print_jobs_scheduled(UAContext *ua);
static void do_storage_status(UAContext *ua, STORE *store);
static void do_client_status(UAContext *ua, CLIENT *client);
static void do_director_status(UAContext *ua, char *cmd);
static void do_all_status(UAContext *ua, char *cmd);

/*
 * status command
 */
int statuscmd(UAContext *ua, char *cmd)
{
   STORE *store;
   CLIENT *client;
   int item, i;

   if (!open_db(ua)) {
      return 1;
   }
   Dmsg1(20, "status:%s:\n", cmd);

   for (i=1; i<ua->argc; i++) {
      if (strcasecmp(ua->argk[i], _("all")) == 0) {
	 do_all_status(ua, cmd);
	 return 1;
      } else if (strcasecmp(ua->argk[i], _("dir")) == 0 ||
                 strcasecmp(ua->argk[i], _("director")) == 0) {
	 do_director_status(ua, cmd);
	 return 1;
      } else if (strcasecmp(ua->argk[i], _("client")) == 0) {
	 client = get_client_resource(ua);
	 if (client) {
	    do_client_status(ua, client);
	 }
	 return 1;
      } else {
	 store = get_storage_resource(ua, cmd);
	 if (store) {
	    do_storage_status(ua, store);
	 }
	 return 1;
      }
   }
   /* If no args, ask for status type */
   if (ua->argc == 1) { 				   
      start_prompt(ua, _("Status available for:\n"));
      add_prompt(ua, _("Director"));
      add_prompt(ua, _("Storage"));
      add_prompt(ua, _("Client"));
      add_prompt(ua, _("All"));
      Dmsg0(20, "do_prompt: select daemon\n");
      if ((item=do_prompt(ua, _("Select daemon type for status"), cmd, MAX_NAME_LENGTH)) < 0) {
	 return 1;
      }
      Dmsg1(20, "item=%d\n", item);
      switch (item) { 
      case 0:			      /* Director */
	 do_director_status(ua, cmd);
	 break;
      case 1:
	 store = select_storage_resource(ua);
	 if (store) {
	    do_storage_status(ua, store);
	 }
	 break;
      case 2:
	 client = select_client_resource(ua);
	 if (client) {
	    do_client_status(ua, client);
	 }
	 break;
      case 3:
	 do_all_status(ua, cmd);
	 break;
      default:
	 break;
      }
   }
   return 1;
}

static void do_all_status(UAContext *ua, char *cmd)
{
   STORE *store, **unique_store;
   CLIENT *client, **unique_client;
   int i, j, found;

   do_director_status(ua, cmd);

   /* Count Storage items */
   LockRes();
   store = NULL;
   for (i=0; (store = (STORE *)GetNextRes(R_STORAGE, (RES *)store)); i++)
      { }
   unique_store = (STORE **) malloc(i * sizeof(STORE));
   /* Find Unique Storage address/port */	  
   store = (STORE *)GetNextRes(R_STORAGE, NULL);
   i = 0;
   unique_store[i++] = store;
   while ((store = (STORE *)GetNextRes(R_STORAGE, (RES *)store))) {
      found = 0;
      for (j=0; j<i; j++) {
	 if (strcmp(unique_store[j]->address, store->address) == 0 &&
	     unique_store[j]->SDport == store->SDport) {
	    found = 1;
	    break;
	 }
      }
      if (!found) {
	 unique_store[i++] = store;
         Dmsg2(40, "Stuffing: %s:%d\n", store->address, store->SDport);
      }
   }
   UnlockRes();

   /* Call each unique Storage daemon */
   for (j=0; j<i; j++) {
      do_storage_status(ua, unique_store[j]);
   }
   free(unique_store);

   /* Count Client items */
   LockRes();
   client = NULL;
   for (i=0; (client = (CLIENT *)GetNextRes(R_CLIENT, (RES *)client)); i++)
      { }
   unique_client = (CLIENT **) malloc(i * sizeof(CLIENT));
   /* Find Unique Client address/port */	 
   client = (CLIENT *)GetNextRes(R_CLIENT, NULL);
   i = 0;
   unique_client[i++] = client;
   while ((client = (CLIENT *)GetNextRes(R_CLIENT, (RES *)client))) {
      found = 0;
      for (j=0; j<i; j++) {
	 if (strcmp(unique_client[j]->address, client->address) == 0 &&
	     unique_client[j]->FDport == client->FDport) {
	    found = 1;
	    break;
	 }
      }
      if (!found) {
	 unique_client[i++] = client;
         Dmsg2(40, "Stuffing: %s:%d\n", client->address, client->FDport);
      }
   }
   UnlockRes();

   /* Call each unique File daemon */
   for (j=0; j<i; j++) {
      do_client_status(ua, unique_client[j]);
   }
   free(unique_client);
   
}

static void do_director_status(UAContext *ua, char *cmd)
{
   JCR *jcr;
   int njobs = 0;
   char *msg;
   char dt[MAX_TIME_LENGTH], b1[30], b2[30];
   int pool_mem = FALSE;

   Dmsg0(200, "Doing status\n");
   bsendmsg(ua, "%s Version: " VERSION " (" BDATE ")\n", my_name);
   bstrftime(dt, sizeof(dt), daemon_start_time);
   bsendmsg(ua, _("Daemon started %s, %d Job%s run.\n"), dt, last_job.NumJobs,
        last_job.NumJobs == 1 ? "" : "s");
   if (last_job.NumJobs > 0) {
      char termstat[30];

      bstrftime(dt, sizeof(dt), last_job.end_time);
      bsendmsg(ua, _("Last Job %s finished at %s\n"), last_job.Job, dt);
      jobstatus_to_ascii(last_job.JobStatus, termstat, sizeof(termstat));
	   
      bsendmsg(ua, _("  Files=%s Bytes=%s Termination Status=%s\n"), 
	   edit_uint64_with_commas(last_job.JobFiles, b1),
	   edit_uint64_with_commas(last_job.JobBytes, b2),
	   termstat);
   }
   lock_jcr_chain();
   for (jcr=NULL; (jcr=get_next_jcr(jcr)); njobs++) {
      if (jcr->JobId == 0) {	  /* this is us */
	 bstrftime(dt, sizeof(dt), jcr->start_time);
         bsendmsg(ua, _("Console connected at %s\n"), dt);
	 free_locked_jcr(jcr);
	 njobs--;
	 continue;
      }
      switch (jcr->JobStatus) {
	 case JS_Created:
            msg = _("is waiting execution");
	    break;
	 case JS_Running:
            msg = _("is running");
	    break;
	 case JS_Blocked:
            msg = _("is blocked");
	    break;
	 case JS_Terminated:
            msg = _("has terminated");
	    break;
	 case JS_ErrorTerminated:
            msg = _("has erred");
	    break;
	 case JS_Cancelled:
            msg = _("has been canceled");
	    break;
	 case JS_WaitFD:
	    msg = (char *) get_pool_memory(PM_FNAME);
            Mmsg(&msg, _("is waiting on Client %s"), jcr->client->hdr.name);
	    pool_mem = TRUE;
	    break;
	 case JS_WaitSD:
	    msg = (char *) get_pool_memory(PM_FNAME);
            Mmsg(&msg, _("is waiting on Storage %s"), jcr->store->hdr.name);
	    pool_mem = TRUE;
	    break;
	 case JS_WaitStoreRes:
            msg = _("is waiting on max Storage jobs");
	    break;
	 case JS_WaitClientRes:
            msg = _("is waiting on max Client jobs");
	    break;
	 case JS_WaitJobRes:
            msg = _("is waiting on max Job jobs");
	    break;
	 case JS_WaitMaxJobs:
            msg = _("is waiting on max total jobs");
	    break;

	 default:
	    msg = (char *) get_pool_memory(PM_FNAME);
            Mmsg(&msg, _("is in unknown state %c"), jcr->JobStatus);
	    pool_mem = TRUE;
	    break;
      }
      switch (jcr->SDJobStatus) {
	 case JS_WaitMount:
	    if (pool_mem) {
	       free_pool_memory(msg);
	       pool_mem = FALSE;
	    }
            msg = _("is waiting for a mount request");
	    break;
	 case JS_WaitMedia:
	    if (pool_mem) {
	       free_pool_memory(msg);
	       pool_mem = FALSE;
	    }
            msg = _("is waiting for an appendable Volume");
	    break;
	 case JS_WaitFD:
	    if (!pool_mem) {
	       msg = (char *) get_pool_memory(PM_FNAME);
	       pool_mem = TRUE;
	    }
            Mmsg(&msg, _("is waiting for Client %s to connect to Storage %s"),
		 jcr->client->hdr.name, jcr->store->hdr.name);
	    break;

      }
      bsendmsg(ua, _("JobId %d Job %s %s.\n"), jcr->JobId, jcr->Job, msg);
      if (pool_mem) {
	 free_pool_memory(msg);
	 pool_mem = FALSE;
      }
      free_locked_jcr(jcr);
   }
   unlock_jcr_chain();

   if (njobs == 0) {
      bsendmsg(ua, _("No jobs are running.\n"));
   }
   print_jobs_scheduled(ua);
   bsendmsg(ua, "====\n");
}

static void do_storage_status(UAContext *ua, STORE *store)
{
   BSOCK *sd;

   ua->jcr->store = store;
   /* Try connecting for up to 15 seconds */
   bsendmsg(ua, _("Connecting to Storage daemon %s at %s:%d\n"), 
      store->hdr.name, store->address, store->SDport);
   if (!connect_to_storage_daemon(ua->jcr, 1, 15, 0)) {
      bsendmsg(ua, _("\nFailed to connect to Storage daemon %s.\n====\n"),
	 store->hdr.name);
      return;
   }
   Dmsg0(20, _("Connected to storage daemon\n"));
   sd = ua->jcr->store_bsock;
   bnet_fsend(sd, "status");
   while (bnet_recv(sd) >= 0) {
      bsendmsg(ua, "%s", sd->msg);
   }
   bnet_sig(sd, BNET_TERMINATE);
   bnet_close(sd);
   ua->jcr->store_bsock = NULL;
   return;  
}
   
static void do_client_status(UAContext *ua, CLIENT *client)
{
   BSOCK *fd;

   /* Connect to File daemon */

   ua->jcr->client = client;
   /* Release any old dummy key */
   if (ua->jcr->sd_auth_key) {
      free(ua->jcr->sd_auth_key);
   }
   /* Create a new dummy SD auth key */
   ua->jcr->sd_auth_key = bstrdup("dummy");

   /* Try to connect for 15 seconds */
   bsendmsg(ua, _("Connecting to Client %s at %s:%d\n"), 
      client->hdr.name, client->address, client->FDport);
   if (!connect_to_file_daemon(ua->jcr, 1, 15, 0)) {
      bsendmsg(ua, _("Failed to connect to Client %s.\n====\n"),
	 client->hdr.name);
      return;
   }
   Dmsg0(20, _("Connected to file daemon\n"));
   fd = ua->jcr->file_bsock;
   bnet_fsend(fd, "status");
   while (bnet_recv(fd) >= 0) {
      bsendmsg(ua, "%s", fd->msg);
   }
   bnet_sig(fd, BNET_TERMINATE);
   bnet_close(fd);
   ua->jcr->file_bsock = NULL;

   return;  
}

static void prt_runhdr(UAContext *ua)
{
   bsendmsg(ua, _("Level          Type     Scheduled          Name\n"));
   bsendmsg(ua, _("=================================================================\n"));
}

static void prt_runtime(UAContext *ua, JOB *job, int level, time_t runtime)
{
   char dt[MAX_TIME_LENGTH];	   

   bstrftime(dt, sizeof(dt), runtime);
   bsendmsg(ua, _("%-14s %-8s %-18s %s\n"), 
      level_to_str(level), job_type_to_str(job->JobType), dt, job->hdr.name);
}

/*	    
 * Find all jobs to be run this hour
 * and the next hour.
 */
static void print_jobs_scheduled(UAContext *ua)
{
   time_t now, runtime, tomorrow;
   RUN *run;
   JOB *job;
   SCHED *sched;
   struct tm tm;
   int mday, wday, month, wpos, tmday, twday, tmonth, twpos, i, hour;
   int tod, tom;
   int found;
   int hdr_printed = FALSE;
   int level;

   Dmsg0(200, "enter find_runs()\n");

   now = time(NULL);
   localtime_r(&now, &tm);
   mday = tm.tm_mday - 1;
   wday = tm.tm_wday;
   month = tm.tm_mon;
   wpos = (tm.tm_mday - 1) / 7;

   tomorrow = now + 60 * 60 * 24;
   localtime_r(&tomorrow, &tm);
   tmday = tm.tm_mday - 1;
   twday = tm.tm_wday;
   tmonth = tm.tm_mon;
   twpos  = (tm.tm_mday - 1) / 7;

   /* Loop through all jobs */
   LockRes();
   for (job=NULL; (job=(JOB *)GetNextRes(R_JOB, (RES *)job)); ) {
      level = job->level;   
      sched = job->schedule;
      if (sched == NULL) {	      /* scheduled? */
	 continue;		      /* no, skip this job */
      }
      for (run=sched->run; run; run=run->next) {
	 if (run->level) {
	    level = run->level;
	 }
	 /* 
	  * Find runs in next 24 hours
	  */
	 tod = (bit_is_set(mday, run->mday) || bit_is_set(wday, run->wday)) && 
		bit_is_set(month, run->month) && bit_is_set(wpos, run->wpos);

	 tom = (bit_is_set(tmday, run->mday) || bit_is_set(twday, run->wday)) &&
		bit_is_set(tmonth, run->month) && bit_is_set(wpos, run->wpos);

         Dmsg2(200, "tod=%d tom=%d\n", tod, tom);
	 found = FALSE;
	 if (tod) {
	    /* find time (time_t) job is to be run */
	    localtime_r(&now, &tm);
	    hour = 0;
	    for (i=tm.tm_hour; i < 24; i++) {
	       if (bit_is_set(i, run->hour)) {
		  tm.tm_hour = i;
		  tm.tm_min = run->minute;
		  tm.tm_sec = 0;
		  runtime = mktime(&tm);
		  if (runtime > now) {
		     if (!hdr_printed) {
			hdr_printed = TRUE;
			prt_runhdr(ua);
		     }
		     prt_runtime(ua, job, level, runtime);
		     found = TRUE;
		     break;
		  }
	       }
	    }
	 }

//      Dmsg2(200, "runtime=%d now=%d\n", runtime, now);
	if (!found && tom) {
	    localtime_r(&tomorrow, &tm);
	    hour = 0;
	    for (i=0; i < 24; i++) {
	       if (bit_is_set(i, run->hour)) {
		  hour = i;
		  break;
	       }
	    }
	    tm.tm_hour = hour;
	    tm.tm_min = run->minute;
	    tm.tm_sec = 0;
	    runtime = mktime(&tm);
            Dmsg2(200, "truntime=%d now=%d\n", runtime, now);
	    if (runtime < tomorrow) {
	       if (!hdr_printed) {
		  hdr_printed = TRUE;
		  prt_runhdr(ua);
	       }
	       prt_runtime(ua, job, level, runtime);
	    }
	 }
      }  
   }
   UnlockRes();
   Dmsg0(200, "Leave find_runs()\n");
}
