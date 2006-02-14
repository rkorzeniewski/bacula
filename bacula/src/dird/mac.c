/*
 *
 *   Bacula Director -- mac.c -- responsible for doing
 *     migration, archive, and copy jobs.
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

static char OKbootstrap[] = "3000 OK bootstrap\n";

/* 
 * Called here before the job is run to do the job
 *   specific setup.
 */
bool do_mac_init(JCR *jcr)
{
   POOL_DBR pr;
   char *Name;
   const char *Type;

   switch(jcr->JobType) {
   case JT_MIGRATE:
      Type = "Migration";
      break;
   case JT_ARCHIVE:
      Type = "Archive";
      break;
   case JT_COPY:
      Type = "Copy";
      break;
   default:
      Type = "Unknown";
      break;
   }

   if (!get_or_create_fileset_record(jcr)) {
      return false;
   }

   /*
    * Find JobId of last job that ran.
    */
   Name = jcr->job->migration_job->hdr.name;
   Dmsg1(100, "find last jobid for: %s\n", NPRT(Name));
   jcr->target_jr.JobType = JT_BACKUP;
   if (!db_find_last_jobid(jcr, jcr->db, Name, &jcr->target_jr)) {
      Jmsg(jcr, M_FATAL, 0, 
           _("Previous job \"%s\" not found. ERR=%s\n"), Name,
           db_strerror(jcr->db));
      return false;
   }
   Dmsg1(100, "Last jobid=%d\n", jcr->target_jr.JobId);

   if (!db_get_job_record(jcr, jcr->db, &jcr->target_jr)) {
      Jmsg(jcr, M_FATAL, 0, _("Could not get job record for previous Job. ERR=%s"),
           db_strerror(jcr->db));
      return false;
   }
   if (jcr->target_jr.JobStatus != 'T') {
      Jmsg(jcr, M_FATAL, 0, _("Last Job %d did not terminate normally. JobStatus=%c\n"),
         jcr->target_jr.JobId, jcr->target_jr.JobStatus);
      return false;
   }
   Jmsg(jcr, M_INFO, 0, _("%s using JobId=%d Job=%s\n"),
      Type, jcr->target_jr.JobId, jcr->target_jr.Job);


   /*
    * Get the Pool record -- first apply any level defined pools
    */
   switch (jcr->target_jr.JobLevel) {
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
 * Do a Migration, Archive, or Copy of a previous job
 *
 *  Returns:  false on failure
 *            true  on success
 */
bool do_mac(JCR *jcr)
{
   POOL_DBR pr;
   POOL *pool;
   const char *Type;
   char ed1[100];
   BSOCK *sd;
   JOB *job, *tjob;
   JCR *tjcr;

   switch(jcr->JobType) {
   case JT_MIGRATE:
      Type = "Migration";
      break;
   case JT_ARCHIVE:
      Type = "Archive";
      break;
   case JT_COPY:
      Type = "Copy";
      break;
   default:
      Type = "Unknown";
      break;
   }


   Dmsg4(100, "Target: Name=%s JobId=%d Type=%c Level=%c\n",
      jcr->target_jr.Name, jcr->target_jr.JobId, 
      jcr->target_jr.JobType, jcr->target_jr.JobLevel);

   Dmsg4(100, "Current: Name=%s JobId=%d Type=%c Level=%c\n",
      jcr->jr.Name, jcr->jr.JobId, 
      jcr->jr.JobType, jcr->jr.JobLevel);

   LockRes();
   job = (JOB *)GetResWithName(R_JOB, jcr->jr.Name);
   tjob = (JOB *)GetResWithName(R_JOB, jcr->target_jr.Name);
   UnlockRes();
   if (!job || !tjob) {
      return false;
   }

   tjcr = jcr->target_jcr = new_jcr(sizeof(JCR), dird_free_jcr);
   memcpy(&tjcr->target_jr, &jcr->target_jr, sizeof(tjcr->target_jr));
   set_jcr_defaults(tjcr, tjob);

   if (!setup_job(tjcr)) {
      return false;
   }
   /* Set output PoolId and FileSetId. */
   tjcr->jr.PoolId = jcr->jr.PoolId;
   tjcr->jr.FileSetId = jcr->jr.FileSetId;

   /*
    * Get the PoolId used with the original job. Then
    *  find the pool name from the database record.
    */
   memset(&pr, 0, sizeof(pr));
   pr.PoolId = tjcr->target_jr.PoolId;
   if (!db_get_pool_record(jcr, jcr->db, &pr)) {
      char ed1[50];
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

   /* If pool storage specified, use it for restore */
   copy_storage(tjcr, pool->storage);

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
      tjcr->pool = jcr->pool = pool->NextPool;
      tjcr->jr.PoolId = jcr->jr.PoolId = pr.PoolId;
   }

   /* If pool storage specified, use it instead of job storage for backup */
   copy_storage(jcr, jcr->pool->storage);

   /* Print Job Start message */
   Jmsg(jcr, M_INFO, 0, _("Start %s JobId %s, Job=%s\n"),
        Type, edit_uint64(jcr->JobId, ed1), jcr->Job);

   set_jcr_job_status(jcr, JS_Running);
   set_jcr_job_status(jcr, JS_Running);
   Dmsg2(100, "JobId=%d JobLevel=%c\n", jcr->jr.JobId, jcr->jr.JobLevel);
   if (!db_update_job_start_record(jcr, jcr->db, &jcr->jr)) {
      Jmsg(jcr, M_FATAL, 0, "%s", db_strerror(jcr->db));
      return false;
   }

   if (!db_update_job_start_record(tjcr, tjcr->db, &tjcr->jr)) {
      Jmsg(jcr, M_FATAL, 0, "%s", db_strerror(tjcr->db));
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
   set_jcr_job_status(tjcr, JS_WaitSD);
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
      ((STORE *)tjcr->storage->first())->hdr.name,
      ((STORE *)jcr->storage->first())->hdr.name);
   if (!start_storage_daemon_job(jcr, tjcr->storage, jcr->storage)) {
      return false;
   }
   Dmsg0(150, "Storage daemon connection OK\n");

   if (!send_bootstrap_file(jcr, sd) ||
       !response(jcr, sd, OKbootstrap, "Bootstrap", DISPLAY_ERROR)) {
      return false;
   }


   /*
    * Now start a Storage daemon message thread
    */
   if (!start_storage_daemon_message_thread(jcr)) {
      return false;
   }

   if (!bnet_fsend(sd, "run")) {
      return false;
   }

   set_jcr_job_status(jcr, JS_Running);
   set_jcr_job_status(tjcr, JS_Running);

   /* Pickup Job termination data */
   /* Note, the SD stores in jcr->JobFiles/ReadBytes/JobBytes/Errors */
   wait_for_storage_daemon_termination(jcr);

   jcr->JobStatus = jcr->SDJobStatus;
   if (jcr->JobStatus == JS_Terminated) {
      mac_cleanup(jcr, jcr->JobStatus);
      return true;
   }
   return false;
}


/*
 * Release resources allocated during backup.
 */
void mac_cleanup(JCR *jcr, int TermCode)
{
   char sdt[MAX_TIME_LENGTH], edt[MAX_TIME_LENGTH];
   char ec1[30], ec2[30], ec3[30], ec4[30], elapsed[50];
   char term_code[100], sd_term_msg[100];
   const char *term_msg;
   int msg_type;
   MEDIA_DBR mr;
   double kbps;
   utime_t RunTime;
   const char *Type;
   JCR *tjcr = jcr->target_jcr;
   POOL_MEM query(PM_MESSAGE);

   switch(jcr->JobType) {
   case JT_MIGRATE:
      Type = "Migration";
      break;
   case JT_ARCHIVE:
      Type = "Archive";
      break;
   case JT_COPY:
      Type = "Copy";
      break;
   default:
      Type = "Unknown";
      break;
   }

   /* Ensure target is defined to avoid a lot of testing */
   if (!tjcr) {
      tjcr = jcr;
   }
   tjcr->JobFiles = jcr->JobFiles = jcr->SDJobFiles;
   tjcr->JobBytes = jcr->JobBytes = jcr->SDJobBytes;
   tjcr->VolSessionId = jcr->VolSessionId;
   tjcr->VolSessionTime = jcr->VolSessionTime;

   Dmsg2(100, "Enter mac_cleanup %d %c\n", TermCode, TermCode);
   dequeue_messages(jcr);             /* display any queued messages */
   memset(&mr, 0, sizeof(mr));
   set_jcr_job_status(jcr, TermCode);
   set_jcr_job_status(tjcr, TermCode);


   update_job_end_record(jcr);        /* update database */
   update_job_end_record(tjcr);

   Mmsg(query, "UPDATE Job SET StartTime='%s',EndTime='%s',"
               "JobTDate=%s WHERE JobId=%s", 
      jcr->target_jr.cStartTime, jcr->target_jr.cEndTime, 
      edit_uint64(jcr->target_jr.JobTDate, ec1),
      edit_uint64(tjcr->jr.JobId, ec2));
   db_sql_query(tjcr->db, query.c_str(), NULL, NULL);

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

   update_bootstrap_file(tjcr);

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
   bsnprintf(term_code, sizeof(term_code), term_msg, Type);
   bstrftimes(sdt, sizeof(sdt), jcr->jr.StartTime);
   bstrftimes(edt, sizeof(edt), jcr->jr.EndTime);
   RunTime = jcr->jr.EndTime - jcr->jr.StartTime;
   if (RunTime <= 0) {
      kbps = 0;
   } else {
      kbps = (double)jcr->jr.JobBytes / (1000 * RunTime);
   }
   if (!db_get_job_volume_names(tjcr, tjcr->db, tjcr->jr.JobId, &tjcr->VolumeName)) {
      /*
       * Note, if the job has erred, most likely it did not write any
       *  tape, so suppress this "error" message since in that case
       *  it is normal.  Or look at it the other way, only for a
       *  normal exit should we complain about this error.
       */
      if (jcr->JobStatus == JS_Terminated && jcr->jr.JobBytes) {
         Jmsg(jcr, M_ERROR, 0, "%s", db_strerror(tjcr->db));
      }
      tjcr->VolumeName[0] = 0;         /* none */
   }

   jobstatus_to_ascii(jcr->SDJobStatus, sd_term_msg, sizeof(sd_term_msg));

// bmicrosleep(15, 0);                /* for debugging SIGHUP */

   Jmsg(jcr, msg_type, 0, _("Bacula %s (%s): %s\n"
"  Old Backup JobId:       %u\n"
"  New Backup JobId:       %u\n"
"  JobId:                  %u\n"
"  Job:                    %s\n"
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
"  Last Volume Bytes:      %s\n"
"  SD Errors:              %d\n"
"  SD termination status:  %s\n"
"  Termination:            %s\n\n"),
   VERSION,
   LSMDATE,
        edt, 
        jcr->target_jr.JobId,
        tjcr->jr.JobId,
        jcr->jr.JobId,
        jcr->jr.Job,
        level_to_str(jcr->JobLevel), jcr->since,
        jcr->client->hdr.name,
        jcr->fileset->hdr.name, jcr->FSCreateTime,
        jcr->pool->hdr.name,
        sdt,
        edt,
        edit_utime(RunTime, elapsed, sizeof(elapsed)),
        jcr->JobPriority,
        edit_uint64_with_commas(jcr->SDJobFiles, ec2),
        edit_uint64_with_commas(jcr->SDJobBytes, ec3),
        edit_uint64_with_suffix(jcr->jr.JobBytes, ec4),
        (float)kbps,
        tjcr->VolumeName,
        jcr->VolSessionId,
        jcr->VolSessionTime,
        edit_uint64_with_commas(mr.VolBytes, ec1),
        jcr->SDErrors,
        sd_term_msg,
        term_code);

   Dmsg1(100, "Leave mac_cleanup() target_jcr=0x%x\n", jcr->target_jcr);
   if (jcr->target_jcr) {
      free_jcr(jcr->target_jcr);
   }
}
