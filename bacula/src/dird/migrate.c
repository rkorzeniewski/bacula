/*
 *
 *   Bacula Director -- migrate.c -- responsible for doing
 *     migration jobs.
 *
 *     Kern Sibbald, September MMIV
 *
 *  Basic tasks done here:
 *     Open DB and create records for this job.
 *     Open Message Channel with Storage daemon to tell him a job will be starting.
 *     Open connection with Storage daemon and pass him commands
 *       to do the backup.
 *     When the Storage daemon finishes the job, update the DB.
 *
 *   Version $Id$
 */
/*
   Copyright (C) 2004-2006 Kern Sibbald

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   version 2 as amended with additional clauses defined in the
   file LICENSE in the main source directory.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the 
   the file LICENSE for additional details.

 */

#include "bacula.h"
#include "dird.h"
#include "ua.h"
#ifndef HAVE_REGEX_H
#include "lib/bregex.h"
#else
#include <regex.h>
#endif

static char OKbootstrap[] = "3000 OK bootstrap\n";
static bool get_job_to_migrate(JCR *jcr);
struct idpkt;
static bool regex_find_jobids(JCR *jcr, idpkt *ids, const char *query1,
                 const char *query2, const char *type);
static void start_migration_job(JCR *jcr);

/* 
 * Called here before the job is run to do the job
 *   specific setup.
 */
bool do_migration_init(JCR *jcr)
{
   POOL_DBR pr;

   /* If we find a job to migrate it is previous_jr.JobId */
   if (!get_job_to_migrate(jcr)) {
      return false;
   }

   if (jcr->previous_jr.JobId == 0) {
      return true;                    /* no work */
   }

   if (!get_or_create_fileset_record(jcr)) {
      return false;
   }

   /*
    * Get the Pool record -- first apply any level defined pools
    */
   switch (jcr->previous_jr.JobLevel) {
   case L_FULL:
      if (jcr->full_pool) {
         jcr->pool = jcr->full_pool;
      }
      break;
   case L_INCREMENTAL:
      if (jcr->inc_pool) {
         jcr->pool = jcr->inc_pool;
      }
      break;
   case L_DIFFERENTIAL:
      if (jcr->dif_pool) {
         jcr->pool = jcr->dif_pool;
      }
      break;
   }
   memset(&pr, 0, sizeof(pr));
   bstrncpy(pr.Name, jcr->pool->hdr.name, sizeof(pr.Name));

   while (!db_get_pool_record(jcr, jcr->db, &pr)) { /* get by Name */
      /* Try to create the pool */
      if (create_pool(jcr, jcr->db, jcr->pool, POOL_OP_CREATE) < 0) {
         Jmsg(jcr, M_FATAL, 0, _("Pool %s not in database. %s"), pr.Name,
            db_strerror(jcr->db));
         return false;
      } else {
         Jmsg(jcr, M_INFO, 0, _("Pool %s created in database.\n"), pr.Name);
      }
   }
   jcr->jr.PoolId = pr.PoolId;

   /* If pool storage specified, use it instead of job storage */
   copy_storage(jcr, jcr->pool->storage);

   if (!jcr->storage) {
      Jmsg(jcr, M_FATAL, 0, _("No Storage specification found in Job or Pool.\n"));
      return false;
   }

   if (!create_restore_bootstrap_file(jcr)) {
      return false;
   }
   return true;
}

/*
 * Do a Migration of a previous job
 *
 *  Returns:  false on failure
 *            true  on success
 */
bool do_migration(JCR *jcr)
{
   POOL_DBR pr;
   POOL *pool;
   char ed1[100];
   BSOCK *sd;
   JOB *job, *prev_job;
   JCR *prev_jcr;

   if (jcr->previous_jr.JobId == 0) {
      set_jcr_job_status(jcr, JS_Terminated);
      migration_cleanup(jcr, jcr->JobStatus);
      return true;                    /* no work */
   }

   Dmsg4(000, "Previous:: Name=%s JobId=%d Type=%c Level=%c\n",
      jcr->previous_jr.Name, jcr->previous_jr.JobId, 
      jcr->previous_jr.JobType, jcr->previous_jr.JobLevel);

   Dmsg4(000, "Current: Name=%s JobId=%d Type=%c Level=%c\n",
      jcr->jr.Name, jcr->jr.JobId, 
      jcr->jr.JobType, jcr->jr.JobLevel);

   LockRes();
   job = (JOB *)GetResWithName(R_JOB, jcr->jr.Name);
   prev_job = (JOB *)GetResWithName(R_JOB, jcr->previous_jr.Name);
   UnlockRes();
   if (!job || !prev_job) {
      return false;
   }

   /* 
    *  prev_jcr is the new Job that corresponds to the original
    *  job. It "runs" at the same time as the current 
    *  migration job and becomes a new backup job that replaces
    *  the original backup job.  Most operations on the current
    *  migration jcr are also done on the prev_jcr.
    */
   prev_jcr = jcr->previous_jcr = new_jcr(sizeof(JCR), dird_free_jcr);
   memcpy(&prev_jcr->previous_jr, &jcr->previous_jr, sizeof(prev_jcr->previous_jr));

   /* Turn the prev_jcr into a "real" job */
   set_jcr_defaults(prev_jcr, prev_job);
   if (!setup_job(prev_jcr)) {
      return false;
   }
   /* Set output PoolId and FileSetId. */
   prev_jcr->jr.PoolId = jcr->jr.PoolId;
   prev_jcr->jr.FileSetId = jcr->jr.FileSetId;

   /*
    * Get the PoolId used with the original job. Then
    *  find the pool name from the database record.
    */
   memset(&pr, 0, sizeof(pr));
   pr.PoolId = prev_jcr->previous_jr.PoolId;
   if (!db_get_pool_record(jcr, jcr->db, &pr)) {
      Jmsg(jcr, M_FATAL, 0, _("Pool for JobId %s not in database. ERR=%s\n"),
            edit_int64(pr.PoolId, ed1), db_strerror(jcr->db));
         return false;
   }
   /* Get the pool resource corresponding to the original job */
   pool = (POOL *)GetResWithName(R_POOL, pr.Name);
   if (!pool) {
      Jmsg(jcr, M_FATAL, 0, _("Pool resource \"%s\" not found.\n"), pr.Name);
      return false;
   }

   /* Check Migration time and High/Low water marks */
   /* ***FIXME*** */

   /* If pool storage specified, use it for restore */
   copy_storage(prev_jcr, pool->storage);

   /* If the original backup pool has a NextPool, make sure a 
    *  record exists in the database.
    */
   if (pool->NextPool) {
      memset(&pr, 0, sizeof(pr));
      bstrncpy(pr.Name, pool->NextPool->hdr.name, sizeof(pr.Name));

      while (!db_get_pool_record(jcr, jcr->db, &pr)) { /* get by Name */
         /* Try to create the pool */
         if (create_pool(jcr, jcr->db, pool->NextPool, POOL_OP_CREATE) < 0) {
            Jmsg(jcr, M_FATAL, 0, _("Pool \"%s\" not in database. %s"), pr.Name,
               db_strerror(jcr->db));
            return false;
         } else {
            Jmsg(jcr, M_INFO, 0, _("Pool \"%s\" created in database.\n"), pr.Name);
         }
      }
      /*
       * put the "NextPool" resource pointer in our jcr so that we
       * can pull the Storage reference from it.
       */
      prev_jcr->pool = jcr->pool = pool->NextPool;
      prev_jcr->jr.PoolId = jcr->jr.PoolId = pr.PoolId;
   }

   /* If pool storage specified, use it instead of job storage for backup */
   copy_storage(jcr, jcr->pool->storage);

   /* Print Job Start message */
   Jmsg(jcr, M_INFO, 0, _("Start Migration JobId %s, Job=%s\n"),
        edit_uint64(jcr->JobId, ed1), jcr->Job);

   set_jcr_job_status(jcr, JS_Running);
   set_jcr_job_status(prev_jcr, JS_Running);
   Dmsg2(000, "JobId=%d JobLevel=%c\n", jcr->jr.JobId, jcr->jr.JobLevel);
   if (!db_update_job_start_record(jcr, jcr->db, &jcr->jr)) {
      Jmsg(jcr, M_FATAL, 0, "%s", db_strerror(jcr->db));
      return false;
   }

   if (!db_update_job_start_record(prev_jcr, prev_jcr->db, &prev_jcr->jr)) {
      Jmsg(jcr, M_FATAL, 0, "%s", db_strerror(prev_jcr->db));
      return false;
   }


   /*
    * Open a message channel connection with the Storage
    * daemon. This is to let him know that our client
    * will be contacting him for a backup  session.
    *
    */
   Dmsg0(110, "Open connection with storage daemon\n");
   set_jcr_job_status(jcr, JS_WaitSD);
   set_jcr_job_status(prev_jcr, JS_WaitSD);
   /*
    * Start conversation with Storage daemon
    */
   if (!connect_to_storage_daemon(jcr, 10, SDConnectTimeout, 1)) {
      return false;
   }
   sd = jcr->store_bsock;
   /*
    * Now start a job with the Storage daemon
    */
   Dmsg2(000, "Read store=%s, write store=%s\n", 
      ((STORE *)prev_jcr->storage->first())->hdr.name,
      ((STORE *)jcr->storage->first())->hdr.name);
   if (!start_storage_daemon_job(jcr, prev_jcr->storage, jcr->storage)) {
      return false;
   }
   Dmsg0(150, "Storage daemon connection OK\n");

   if (!send_bootstrap_file(jcr, sd) ||
       !response(jcr, sd, OKbootstrap, "Bootstrap", DISPLAY_ERROR)) {
      return false;
   }

   if (!bnet_fsend(sd, "run")) {
      return false;
   }

   /*
    * Now start a Storage daemon message thread
    */
   if (!start_storage_daemon_message_thread(jcr)) {
      return false;
   }


   set_jcr_job_status(jcr, JS_Running);
   set_jcr_job_status(prev_jcr, JS_Running);

   /* Pickup Job termination data */
   /* Note, the SD stores in jcr->JobFiles/ReadBytes/JobBytes/Errors */
   wait_for_storage_daemon_termination(jcr);

   set_jcr_job_status(jcr, jcr->SDJobStatus);
   if (jcr->JobStatus == JS_Terminated) {
      migration_cleanup(jcr, jcr->JobStatus);
      return true;
   }
   return false;
}

struct idpkt {
   POOLMEM *list;
   uint32_t count;
};

/*
 * Callback handler make list of DB Ids
 */
static int dbid_handler(void *ctx, int num_fields, char **row)
{
   idpkt *ids = (idpkt *)ctx;

   Dmsg3(000, "count=%d Ids=%p %s\n", ids->count, ids->list, ids->list);
   if (ids->count == 0) {
      ids->list[0] = 0;
   } else {
      pm_strcat(ids->list, ",");
   }
   pm_strcat(ids->list, row[0]);
   ids->count++;
   return 0;
}


struct uitem {
   dlink link;   
   char *item;
};

static int item_compare(void *item1, void *item2)
{
   uitem *i1 = (uitem *)item1;
   uitem *i2 = (uitem *)item2;
   return strcmp(i1->item, i2->item);
}

static int unique_name_handler(void *ctx, int num_fields, char **row)
{
   dlist *list = (dlist *)ctx;

   uitem *new_item = (uitem *)malloc(sizeof(uitem));
   uitem *item;
   
   memset(new_item, 0, sizeof(uitem));
   new_item->item = bstrdup(row[0]);
   Dmsg1(000, "Item=%s\n", row[0]);
   item = (uitem *)list->binary_insert((void *)new_item, item_compare);
   if (item != new_item) {            /* already in list */
      free(new_item->item);
      free((char *)new_item);
      return 0;
   }
   return 0;
}

/* Get Job names in Pool */
const char *sql_job =
   "SELECT DISTINCT Job.Name from Job,Pool"
   " WHERE Pool.Name='%s' AND Job.PoolId=Pool.PoolId";

/* Get JobIds from regex'ed Job names */
const char *sql_jobids_from_job =
   "SELECT DISTINCT Job.JobId FROM Job,Pool"
   " WHERE Job.Name='%s' AND Pool.Name='%s' AND Job.PoolId=Pool.PoolId"
   " ORDER by Job.StartTime";

/* Get Client names in Pool */
const char *sql_client =
   "SELECT DISTINCT Client.Name from Client,Pool,Job"
   " WHERE Pool.Name='%s' AND Job.ClientId=Client.ClientId AND"
   " Job.PoolId=Pool.PoolId";

/* Get JobIds from regex'ed Client names */
const char *sql_jobids_from_client =
   "SELECT DISTINCT Job.JobId FROM Job,Pool"
   " WHERE Client.Name='%s' AND Pool.Name='%s' AND Job.PoolId=Pool.PoolId"
   " AND Job.ClientId=Client.ClientId "
   " ORDER by Job.StartTime";

/* Get Volume names in Pool */
const char *sql_vol = 
   "SELECT DISTINCT VolumeName FROM Media,Pool WHERE"
   " VolStatus in ('Full','Used','Error') AND"
   " Media.PoolId=Pool.PoolId AND Pool.Name='%s'";

/* Get JobIds from regex'ed Volume names */
const char *sql_jobids_from_vol =
   "SELECT DISTINCT Job.JobId FROM Media,JobMedia,Job"
   " WHERE Media.VolumeName='%s' AND Media.MediaId=JobMedia.MediaId"
   " AND JobMedia.JobId=Job.JobId" 
   " ORDER by Job.StartTime";
   




const char *sql_smallest_vol = 
   "SELECT MediaId FROM Media,Pool WHERE"
   " VolStatus in ('Full','Used','Error') AND"
   " Media.PoolId=Pool.PoolId AND Pool.Name='%s'"
   " ORDER BY VolBytes ASC LIMIT 1";

const char *sql_oldest_vol = 
   "SELECT MediaId FROM Media,Pool WHERE"
   " VolStatus in ('Full','Used','Error') AND"
   " Media.PoolId=Pool.PoolId AND Pool.Name='%s'"
   " ORDER BY LastWritten ASC LIMIT 1";

const char *sql_jobids_from_mediaid =
   "SELECT DISTINCT Job.JobId FROM JobMedia,Job"
   " WHERE JobMedia.JobId=Job.JobId AND JobMedia.MediaId=%s"
   " ORDER by Job.StartTime";

const char *sql_pool_bytes =
   "SELECT SUM(VolBytes) FROM Media,Pool WHERE"
   " VolStatus in ('Full','Used','Error','Append') AND"
   " Media.PoolId=Pool.PoolId AND Pool.Name='%s'";

const char *sql_vol_bytes =
   "SELECT MediaId FROM Media,Pool WHERE"
   " VolStatus in ('Full','Used','Error') AND"
   " Media.PoolId=Pool.PoolId AND Pool.Name='%s' AND"
   " VolBytes<%s ORDER BY LastWritten ASC LIMIT 1";


const char *sql_ujobid =
   "SELECT DISTINCT Job.Job from Client,Pool,Media,Job,JobMedia "
   " WHERE Media.PoolId=Pool.PoolId AND Pool.Name='%s' AND"
   " JobMedia.JobId=Job.JobId AND Job.PoolId=Media.PoolId";



/*
 * Returns: false on error
 *          true  if OK and jcr->previous_jr filled in
 */
static bool get_job_to_migrate(JCR *jcr)
{
   char ed1[30];
   POOL_MEM query(PM_MESSAGE);
   JobId_t JobId;
   int stat;
   char *p;
   idpkt ids;

   ids.list = get_pool_memory(PM_MESSAGE);
   Dmsg1(000, "list=%p\n", ids.list);
   ids.list[0] = 0;
   ids.count = 0;

   if (jcr->MigrateJobId != 0) {
      Dmsg1(000, "previous jobid=%u\n", jcr->MigrateJobId);
      edit_uint64(jcr->MigrateJobId, ids.list);
      ids.count = 1;
   } else {
      switch (jcr->job->selection_type) {
      case MT_JOB:
         if (!regex_find_jobids(jcr, &ids, sql_job, sql_jobids_from_job, "Job")) {
            goto bail_out;
         } 
         break;
      case MT_CLIENT:
         if (!regex_find_jobids(jcr, &ids, sql_client, 
              sql_jobids_from_client, "Client")) {
            goto bail_out;
         } 
         break;
      case MT_VOLUME:
         if (!regex_find_jobids(jcr, &ids, sql_vol, 
             sql_jobids_from_vol, "Volume")) {
            goto bail_out;
         } 
         break;
      case MT_SQLQUERY:
         if (!jcr->job->selection_pattern) {
            Jmsg(jcr, M_FATAL, 0, _("No Migration SQL selection pattern specified.\n"));
            goto bail_out;
         }
         Dmsg1(000, "SQL=%s\n", jcr->job->selection_pattern);
         if (!db_sql_query(jcr->db, jcr->job->selection_pattern,
              dbid_handler, (void *)&ids)) {
            Jmsg(jcr, M_FATAL, 0,
                 _("SQL failed. ERR=%s\n"), db_strerror(jcr->db));
            goto bail_out;
         }
         break;


/***** Below not implemented yet *********/
      case MT_SMALLEST_VOL:
         Mmsg(query, sql_smallest_vol, jcr->pool->hdr.name);
//       Mmsg(query2, sql_jobids_from_mediaid, JobIds);
//       Dmsg1(000, "Smallest Vol Jobids=%s\n", JobIds);
         break;
      case MT_OLDEST_VOL:
         Mmsg(query, sql_oldest_vol, jcr->pool->hdr.name);
//       Mmsg(query2, sql_jobids_from_mediaid, JobIds);
//       Dmsg1(000, "Oldest Vol Jobids=%s\n", JobIds);
         break;
      case MT_POOL_OCCUPANCY:
         Mmsg(query, sql_pool_bytes, jcr->pool->hdr.name);
//       Dmsg1(000, "Pool Occupancy Jobids=%s\n", JobIds);
         break;
      case MT_POOL_TIME:
         Dmsg0(000, "Pool time not implemented\n");
         break;
      default:
         Jmsg(jcr, M_FATAL, 0, _("Unknown Migration Selection Type.\n"));
         goto bail_out;
      }
   }

   p = ids.list;
   JobId = 0;
   stat = get_next_jobid_from_list(&p, &JobId);
   Dmsg2(000, "get_next_jobid stat=%d JobId=%u\n", stat, JobId);
   jcr->MigrateJobId = JobId;
   start_migration_job(jcr);
   if (stat < 0) {
      Jmsg(jcr, M_FATAL, 0, _("Invalid JobId found.\n"));
      goto bail_out;
   } else if (stat == 0) {
      Jmsg(jcr, M_INFO, 0, _("No JobIds found to migrate.\n"));
      goto ok_out;
   }
   
   jcr->previous_jr.JobId = JobId;
   Dmsg1(000, "Previous jobid=%d\n", jcr->previous_jr.JobId);

   if (!db_get_job_record(jcr, jcr->db, &jcr->previous_jr)) {
      Jmsg(jcr, M_FATAL, 0, _("Could not get job record for JobId %s to migrate. ERR=%s"),
           edit_int64(jcr->previous_jr.JobId, ed1),
           db_strerror(jcr->db));
      goto bail_out;
   }
   Jmsg(jcr, M_INFO, 0, _("Migration using JobId=%d Job=%s\n"),
      jcr->previous_jr.JobId, jcr->previous_jr.Job);

ok_out:
   free_pool_memory(ids.list);
   return true;

bail_out:
   free_pool_memory(ids.list);
   return false;
}

static void start_migration_job(JCR *jcr)
{
   UAContext *ua = new_ua_context(jcr);
   char ed1[50];
   ua->batch = true;
   Mmsg(ua->cmd, "run %s jobid=%s", jcr->job->hdr.name, 
        edit_uint64(jcr->MigrateJobId, ed1));
   Dmsg1(000, "=============== Migration cmd=%s\n", ua->cmd);
   parse_ua_args(ua);                 /* parse command */
// int stat = run_cmd(ua, ua->cmd);
   int stat = (int)jcr->MigrateJobId;
   if (stat == 0) {
      Jmsg(jcr, M_ERROR, 0, _("Could not start migration job.\n"));
   } else {
      Jmsg(jcr, M_INFO, 0, _("Migration JobId %d started.\n"), stat);
   }
   free_ua_context(ua);
}


static bool regex_find_jobids(JCR *jcr, idpkt *ids, const char *query1,
                 const char *query2, const char *type) {
   dlist *item_chain;
   uitem *item = NULL;
   uitem *last_item = NULL;
   regex_t preg;
   char prbuf[500];
   int rc;
   bool ok = false;
   POOL_MEM query(PM_MESSAGE);

   item_chain = New(dlist(item, &item->link));
   if (!jcr->job->selection_pattern) {
      Jmsg(jcr, M_FATAL, 0, _("No Migration %s selection pattern specified.\n"),
         type);
      goto bail_out;
   }
   Dmsg1(000, "regex=%s\n", jcr->job->selection_pattern);
   /* Compile regex expression */
   rc = regcomp(&preg, jcr->job->selection_pattern, REG_EXTENDED);
   if (rc != 0) {
      regerror(rc, &preg, prbuf, sizeof(prbuf));
      Jmsg(jcr, M_FATAL, 0, _("Could not compile regex pattern \"%s\" ERR=%s\n"),
           jcr->job->selection_pattern, prbuf);
      goto bail_out;
   }
   /* Basic query for names */
   Mmsg(query, query1, jcr->pool->hdr.name);
   Dmsg1(000, "query1=%s\n", query.c_str());
   if (!db_sql_query(jcr->db, query.c_str(), unique_name_handler, 
        (void *)item_chain)) {
      Jmsg(jcr, M_FATAL, 0,
           _("SQL to get %s failed. ERR=%s\n"), type, db_strerror(jcr->db));
      goto bail_out;
   }
   /* Now apply the regex to the names and remove any item not matched */
   foreach_dlist(item, item_chain) {
      const int nmatch = 30;
      regmatch_t pmatch[nmatch];
      if (last_item) {
         Dmsg1(000, "Remove item %s\n", last_item->item);
         free(last_item->item);
         item_chain->remove(last_item);
      }
      Dmsg1(000, "Jobitem=%s\n", item->item);
      rc = regexec(&preg, item->item, nmatch, pmatch,  0);
      if (rc == 0) {
         last_item = NULL;   /* keep this one */
      } else {   
         last_item = item;
      }
   }
   if (last_item) {
      free(last_item->item);
      Dmsg1(000, "Remove item %s\n", last_item->item);
      item_chain->remove(last_item);
   }
   regfree(&preg);
   /* 
    * At this point, we have a list of items in item_chain
    *  that have been matched by the regex, so now we need
    *  to look up their jobids.
    */
   ids->count = 0;
   foreach_dlist(item, item_chain) {
      Dmsg1(000, "Got Job: %s\n", item->item);
      Mmsg(query, query2, item->item, jcr->pool->hdr.name);
      Dmsg1(000, "query2=%s\n", query.c_str());
      if (!db_sql_query(jcr->db, query.c_str(), dbid_handler, (void *)ids)) {
         Jmsg(jcr, M_FATAL, 0,
              _("SQL failed. ERR=%s\n"), db_strerror(jcr->db));
         goto bail_out;
      }
   }
   if (ids->count == 0) {
      Jmsg(jcr, M_INFO, 0, _("No %ss found to migrate.\n"), type);
   }
   ok = true;
bail_out:
   Dmsg2(000, "Count=%d Jobids=%s\n", ids->count, ids->list);
   delete item_chain;
   Dmsg0(000, "After delete item_chain\n");
   return ok;
}


/*
 * Release resources allocated during backup.
 */
void migration_cleanup(JCR *jcr, int TermCode)
{
   char sdt[MAX_TIME_LENGTH], edt[MAX_TIME_LENGTH];
   char ec1[30], ec2[30], ec3[30], ec4[30], ec5[30], elapsed[50];
   char ec6[50], ec7[50], ec8[50];
   char term_code[100], sd_term_msg[100];
   const char *term_msg;
   int msg_type;
   MEDIA_DBR mr;
   double kbps;
   utime_t RunTime;
   JCR *prev_jcr = jcr->previous_jcr;
   POOL_MEM query(PM_MESSAGE);

   Dmsg2(100, "Enter migrate_cleanup %d %c\n", TermCode, TermCode);
   dequeue_messages(jcr);             /* display any queued messages */
   memset(&mr, 0, sizeof(mr));
   set_jcr_job_status(jcr, TermCode);
   update_job_end_record(jcr);        /* update database */

   /* Check if we actually did something */
   if (prev_jcr) {
      prev_jcr->JobFiles = jcr->JobFiles = jcr->SDJobFiles;
      prev_jcr->JobBytes = jcr->JobBytes = jcr->SDJobBytes;
      prev_jcr->VolSessionId = jcr->VolSessionId;
      prev_jcr->VolSessionTime = jcr->VolSessionTime;

      set_jcr_job_status(prev_jcr, TermCode);

      update_job_end_record(prev_jcr);

      Mmsg(query, "UPDATE Job SET StartTime='%s',EndTime='%s',"
                  "JobTDate=%s WHERE JobId=%s", 
         jcr->previous_jr.cStartTime, jcr->previous_jr.cEndTime, 
         edit_uint64(jcr->previous_jr.JobTDate, ec1),
         edit_uint64(prev_jcr->jr.JobId, ec2));
      db_sql_query(prev_jcr->db, query.c_str(), NULL, NULL);

      if (!db_get_job_record(jcr, jcr->db, &jcr->jr)) {
         Jmsg(jcr, M_WARNING, 0, _("Error getting job record for stats: %s"),
            db_strerror(jcr->db));
         set_jcr_job_status(jcr, JS_ErrorTerminated);
      }

      bstrncpy(mr.VolumeName, jcr->VolumeName, sizeof(mr.VolumeName));
      if (!db_get_media_record(jcr, jcr->db, &mr)) {
         Jmsg(jcr, M_WARNING, 0, _("Error getting Media record for Volume \"%s\": ERR=%s"),
            mr.VolumeName, db_strerror(jcr->db));
         set_jcr_job_status(jcr, JS_ErrorTerminated);
      }

      update_bootstrap_file(prev_jcr);

      if (!db_get_job_volume_names(prev_jcr, prev_jcr->db, prev_jcr->jr.JobId, &prev_jcr->VolumeName)) {
         /*
          * Note, if the job has erred, most likely it did not write any
          *  tape, so suppress this "error" message since in that case
          *  it is normal.  Or look at it the other way, only for a
          *  normal exit should we complain about this error.
          */
         if (jcr->JobStatus == JS_Terminated && jcr->jr.JobBytes) {
            Jmsg(jcr, M_ERROR, 0, "%s", db_strerror(prev_jcr->db));
         }
         prev_jcr->VolumeName[0] = 0;         /* none */
      }
  }

   msg_type = M_INFO;                 /* by default INFO message */
   switch (jcr->JobStatus) {
      case JS_Terminated:
         if (jcr->Errors || jcr->SDErrors) {
            term_msg = _("%s OK -- with warnings");
         } else {
            term_msg = _("%s OK");
         }
         break;
      case JS_FatalError:
      case JS_ErrorTerminated:
         term_msg = _("*** %s Error ***");
         msg_type = M_ERROR;          /* Generate error message */
         if (jcr->store_bsock) {
            bnet_sig(jcr->store_bsock, BNET_TERMINATE);
            if (jcr->SD_msg_chan) {
               pthread_cancel(jcr->SD_msg_chan);
            }
         }
         break;
      case JS_Canceled:
         term_msg = _("%s Canceled");
         if (jcr->store_bsock) {
            bnet_sig(jcr->store_bsock, BNET_TERMINATE);
            if (jcr->SD_msg_chan) {
               pthread_cancel(jcr->SD_msg_chan);
            }
         }
         break;
      default:
         term_msg = _("Inappropriate %s term code");
         break;
   }
   bsnprintf(term_code, sizeof(term_code), term_msg, "Migration");
   bstrftimes(sdt, sizeof(sdt), jcr->jr.StartTime);
   bstrftimes(edt, sizeof(edt), jcr->jr.EndTime);
   RunTime = jcr->jr.EndTime - jcr->jr.StartTime;
   if (RunTime <= 0) {
      kbps = 0;
   } else {
      kbps = (double)jcr->SDJobBytes / (1000 * RunTime);
   }


   jobstatus_to_ascii(jcr->SDJobStatus, sd_term_msg, sizeof(sd_term_msg));

   Jmsg(jcr, msg_type, 0, _("Bacula %s (%s): %s\n"
"  Prev Backup JobId:      %s\n"
"  New Backup JobId:       %s\n"
"  Migration JobId:        %s\n"
"  Migration Job:          %s\n"
"  Backup Level:           %s%s\n"
"  Client:                 %s\n"
"  FileSet:                \"%s\" %s\n"
"  Pool:                   \"%s\"\n"
"  Start time:             %s\n"
"  End time:               %s\n"
"  Elapsed time:           %s\n"
"  Priority:               %d\n"
"  SD Files Written:       %s\n"
"  SD Bytes Written:       %s (%sB)\n"
"  Rate:                   %.1f KB/s\n"
"  Volume name(s):         %s\n"
"  Volume Session Id:      %d\n"
"  Volume Session Time:    %d\n"
"  Last Volume Bytes:      %s (%sB)\n"
"  SD Errors:              %d\n"
"  SD termination status:  %s\n"
"  Termination:            %s\n\n"),
   VERSION,
   LSMDATE,
        edt, 
        prev_jcr ? edit_uint64(jcr->previous_jr.JobId, ec6) : "0", 
        prev_jcr ? edit_uint64(prev_jcr->jr.JobId, ec7) : "0",
        edit_uint64(jcr->jr.JobId, ec8),
        jcr->jr.Job,
        level_to_str(jcr->JobLevel), jcr->since,
        jcr->client->hdr.name,
        jcr->fileset->hdr.name, jcr->FSCreateTime,
        jcr->pool->hdr.name,
        sdt,
        edt,
        edit_utime(RunTime, elapsed, sizeof(elapsed)),
        jcr->JobPriority,
        edit_uint64_with_commas(jcr->SDJobFiles, ec1),
        edit_uint64_with_commas(jcr->SDJobBytes, ec2),
        edit_uint64_with_suffix(jcr->SDJobBytes, ec3),
        (float)kbps,
        prev_jcr ? prev_jcr->VolumeName : "",
        jcr->VolSessionId,
        jcr->VolSessionTime,
        edit_uint64_with_commas(mr.VolBytes, ec4),
        edit_uint64_with_suffix(mr.VolBytes, ec5),
        jcr->SDErrors,
        sd_term_msg,
        term_code);

   Dmsg1(100, "migrate_cleanup() previous_jcr=0x%x\n", jcr->previous_jcr);
   if (jcr->previous_jcr) {
      free_jcr(jcr->previous_jcr);
      jcr->previous_jcr = NULL;
   }
   Dmsg0(100, "Leave migrate_cleanup()\n");
}
