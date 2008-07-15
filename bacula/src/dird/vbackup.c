/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2008-2008 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from
   many others, a complete list can be found in the file AUTHORS.
   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version two of the GNU General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

   Bacula® is a registered trademark of Kern Sibbald.
   The licensor of Bacula is the Free Software Foundation Europe
   (FSFE), Fiduciary Program, Sumatrastrasse 25, 8006 Zürich,
   Switzerland, email:ftf@fsfeurope.org.
*/
/*
 *
 *   Bacula Director -- vbackup.c -- responsible for doing virtual
 *     backup jobs or in other words, consolidation or synthetic
 *     backups.
 *
 *     Kern Sibbald, July MMVIII
 *
 *  Basic tasks done here:
 *     Open DB and create records for this job.
 *     Figure out what Jobs to copy.
 *     Open Message Channel with Storage daemon to tell him a job will be starting.
 *     Open connection with File daemon and pass him commands
 *       to do the backup.
 *     When the File daemon finishes the job, update the DB.
 *
 *   Version $Id: $
 */

#include "bacula.h"
#include "dird.h"
#include "ua.h"

static const int dbglevel = 10;

static char OKbootstrap[] = "3000 OK bootstrap\n";

static bool create_bootstrap_file(JCR *jcr, POOLMEM *jobids);
void vbackup_cleanup(JCR *jcr, int TermCode);

/* 
 * Called here before the job is run to do the job
 *   specific setup.
 */
bool do_vbackup_init(JCR *jcr)
{
   /* ***FIXME*** remove when implemented in job.c */
   if (!jcr->rpool_source) {
      jcr->rpool_source = get_pool_memory(PM_MESSAGE);
      pm_strcpy(jcr->rpool_source, _("unknown source"));
   }
   
   if (!get_or_create_fileset_record(jcr)) {
      Dmsg1(dbglevel, "JobId=%d no FileSet\n", (int)jcr->JobId);
      return false;
   }

   apply_pool_overrides(jcr);

   if (!allow_duplicate_job(jcr)) {
      return false;
   }

   /*
    * Note, at this point, pool is the pool for this job.  We
    *  transfer it to rpool (read pool), and a bit later,
    *  pool will be changed to point to the write pool, 
    *  which comes from pool->NextPool.
    */
   jcr->rpool = jcr->pool;            /* save read pool */
   pm_strcpy(jcr->rpool_source, jcr->pool_source);


   Dmsg2(dbglevel, "Read pool=%s (From %s)\n", jcr->rpool->name(), jcr->rpool_source);

   POOLMEM *jobids = get_pool_memory(PM_FNAME);
   db_accurate_get_jobids(jcr, jcr->db, &jcr->jr, jobids);
   Dmsg1(000, "Accurate jobids=%s\n", jobids);
   if (*jobids == 0) {
      free_pool_memory(jobids);
      Jmsg(jcr, M_FATAL, 0, _("Cannot find previous JobIds.\n"));
      return false;
   }

   if (!create_bootstrap_file(jcr, jobids)) {
      Jmsg(jcr, M_FATAL, 0, _("Could not get or create the FileSet record.\n"));
      free_pool_memory(jobids);
      return false;
   }
   free_pool_memory(jobids);

   /*
    * If the original backup pool has a NextPool, make sure a 
    *  record exists in the database. Note, in this case, we
    *  will be backing up from pool to pool->NextPool.
    */
   if (jcr->pool->NextPool) {
      jcr->jr.PoolId = get_or_create_pool_record(jcr, jcr->pool->NextPool->name());
      if (jcr->jr.PoolId == 0) {
         return false;
      }
   }
   /* ***FIXME*** this is probably not needed */
   if (!set_migration_wstorage(jcr, jcr->pool)) {
      return false;
   }
   pm_strcpy(jcr->pool_source, _("Job Pool's NextPool resource"));

   Dmsg2(dbglevel, "Write pool=%s read rpool=%s\n", jcr->pool->name(), jcr->rpool->name());

   create_clones(jcr);

   return true;
}

/*
 * Do a backup of the specified FileSet
 *
 *  Returns:  false on failure
 *            true  on success
 */
bool do_vbackup(JCR *jcr)
{
   char ed1[100];
   BSOCK *sd;

   /* Print Job Start message */
   Jmsg(jcr, M_INFO, 0, _("Start Vbackup JobId %s, Job=%s\n"),
        edit_uint64(jcr->JobId, ed1), jcr->Job);

   /*
    * Open a message channel connection with the Storage
    * daemon. This is to let him know that our client
    * will be contacting him for a backup  session.
    *
    */
   Dmsg0(110, "Open connection with storage daemon\n");
   set_jcr_job_status(jcr, JS_WaitSD);
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
      ((STORE *)jcr->rstorage->first())->name(),
      ((STORE *)jcr->wstorage->first())->name());
   if (((STORE *)jcr->rstorage->first())->name() == ((STORE *)jcr->wstorage->first())->name()) {
      Jmsg(jcr, M_FATAL, 0, _("Read storage \"%s\" same as write storage.\n"),
           ((STORE *)jcr->rstorage->first())->name());
      return false;
   }
   if (!start_storage_daemon_job(jcr, jcr->rstorage, jcr->wstorage)) {
      return false;
   }
   Dmsg0(150, "Storage daemon connection OK\n");

   if (!send_bootstrap_file(jcr, sd) ||
       !response(jcr, sd, OKbootstrap, "Bootstrap", DISPLAY_ERROR)) {
      return false;
   }

   /*    
    * We re-update the job start record so that the start
    *  time is set after the run before job.  This avoids 
    *  that any files created by the run before job will
    *  be saved twice.  They will be backed up in the current
    *  job, but not in the next one unless they are changed.
    *  Without this, they will be backed up in this job and
    *  in the next job run because in that case, their date 
    *   is after the start of this run.
    */
   jcr->start_time = time(NULL);
   jcr->jr.StartTime = jcr->start_time;
   jcr->jr.JobTDate = jcr->start_time;
   set_jcr_job_status(jcr, JS_Running);

   /* Update job start record for this migration control job */
   if (!db_update_job_start_record(jcr, jcr->db, &jcr->jr)) {
      Jmsg(jcr, M_FATAL, 0, "%s", db_strerror(jcr->db));
      return false;
   }

   /*
    * Start the job prior to starting the message thread below
    * to avoid two threads from using the BSOCK structure at
    * the same time.
    */
   if (!sd->fsend("run")) {
      return false;
   }

   /*
    * Now start a Storage daemon message thread
    */
   if (!start_storage_daemon_message_thread(jcr)) {
      return false;
   }


   set_jcr_job_status(jcr, JS_Running);

   /* Pickup Job termination data */
   /* Note, the SD stores in jcr->JobFiles/ReadBytes/JobBytes/Errors */
   wait_for_storage_daemon_termination(jcr);
   set_jcr_job_status(jcr, jcr->SDJobStatus);
   db_write_batch_file_records(jcr);    /* used by bulk batch file insert */
   if (jcr->JobStatus != JS_Terminated) {
      return false;
   }

   vbackup_cleanup(jcr, jcr->JobStatus);
   return true;
}


/*
 * Release resources allocated during backup.
 */
void vbackup_cleanup(JCR *jcr, int TermCode)
{
   char sdt[50], edt[50], schedt[50];
   char ec1[30], ec2[30], ec3[30], ec4[30], ec5[30], compress[50];
   char ec6[30], ec7[30], ec8[30], elapsed[50];
   char term_code[100], fd_term_msg[100], sd_term_msg[100];
   const char *term_msg;
   int msg_type = M_INFO;
   MEDIA_DBR mr;
   CLIENT_DBR cr;
   double kbps, compression;
   utime_t RunTime;

   Dmsg2(100, "Enter backup_cleanup %d %c\n", TermCode, TermCode);
   memset(&mr, 0, sizeof(mr));
   memset(&cr, 0, sizeof(cr));

   update_job_end(jcr, TermCode);

   if (!db_get_job_record(jcr, jcr->db, &jcr->jr)) {
      Jmsg(jcr, M_WARNING, 0, _("Error getting Job record for Job report: ERR=%s"),
         db_strerror(jcr->db));
      set_jcr_job_status(jcr, JS_ErrorTerminated);
   }

   bstrncpy(cr.Name, jcr->client->name(), sizeof(cr.Name));
   if (!db_get_client_record(jcr, jcr->db, &cr)) {
      Jmsg(jcr, M_WARNING, 0, _("Error getting Client record for Job report: ERR=%s"),
         db_strerror(jcr->db));
   }

   bstrncpy(mr.VolumeName, jcr->VolumeName, sizeof(mr.VolumeName));
   if (!db_get_media_record(jcr, jcr->db, &mr)) {
      Jmsg(jcr, M_WARNING, 0, _("Error getting Media record for Volume \"%s\": ERR=%s"),
         mr.VolumeName, db_strerror(jcr->db));
      set_jcr_job_status(jcr, JS_ErrorTerminated);
   }

   update_bootstrap_file(jcr);

   switch (jcr->JobStatus) {
      case JS_Terminated:
         if (jcr->Errors || jcr->SDErrors) {
            term_msg = _("Backup OK -- with warnings");
         } else {
            term_msg = _("Backup OK");
         }
         break;
      case JS_FatalError:
      case JS_ErrorTerminated:
         term_msg = _("*** Backup Error ***");
         msg_type = M_ERROR;          /* Generate error message */
         if (jcr->store_bsock) {
            jcr->store_bsock->signal(BNET_TERMINATE);
            if (jcr->SD_msg_chan) {
               pthread_cancel(jcr->SD_msg_chan);
            }
         }
         break;
      case JS_Canceled:
         term_msg = _("Backup Canceled");
         if (jcr->store_bsock) {
            jcr->store_bsock->signal(BNET_TERMINATE);
            if (jcr->SD_msg_chan) {
               pthread_cancel(jcr->SD_msg_chan);
            }
         }
         break;
      default:
         term_msg = term_code;
         sprintf(term_code, _("Inappropriate term code: %c\n"), jcr->JobStatus);
         break;
   }
   bstrftimes(schedt, sizeof(schedt), jcr->jr.SchedTime);
   bstrftimes(sdt, sizeof(sdt), jcr->jr.StartTime);
   bstrftimes(edt, sizeof(edt), jcr->jr.EndTime);
   RunTime = jcr->jr.EndTime - jcr->jr.StartTime;
   if (RunTime <= 0) {
      kbps = 0;
   } else {
      kbps = ((double)jcr->jr.JobBytes) / (1000.0 * (double)RunTime);
   }
   if (!db_get_job_volume_names(jcr, jcr->db, jcr->jr.JobId, &jcr->VolumeName)) {
      /*
       * Note, if the job has erred, most likely it did not write any
       *  tape, so suppress this "error" message since in that case
       *  it is normal.  Or look at it the other way, only for a
       *  normal exit should we complain about this error.
       */
      if (jcr->JobStatus == JS_Terminated && jcr->jr.JobBytes) {
         Jmsg(jcr, M_ERROR, 0, "%s", db_strerror(jcr->db));
      }
      jcr->VolumeName[0] = 0;         /* none */
   }

   if (jcr->ReadBytes == 0) {
      bstrncpy(compress, "None", sizeof(compress));
   } else {
      compression = (double)100 - 100.0 * ((double)jcr->JobBytes / (double)jcr->ReadBytes);
      if (compression < 0.5) {
         bstrncpy(compress, "None", sizeof(compress));
      } else {
         bsnprintf(compress, sizeof(compress), "%.1f %%", compression);
      }
   }
   jobstatus_to_ascii(jcr->FDJobStatus, fd_term_msg, sizeof(fd_term_msg));
   jobstatus_to_ascii(jcr->SDJobStatus, sd_term_msg, sizeof(sd_term_msg));

// bmicrosleep(15, 0);                /* for debugging SIGHUP */

   Jmsg(jcr, msg_type, 0, _("Bacula %s %s (%s): %s\n"
"  Build OS:               %s %s %s\n"
"  JobId:                  %d\n"
"  Job:                    %s\n"
"  Backup Level:           %s%s\n"
"  Client:                 \"%s\" %s\n"
"  FileSet:                \"%s\" %s\n"
"  Pool:                   \"%s\" (From %s)\n"
"  Catalog:                \"%s\" (From %s)\n"
"  Storage:                \"%s\" (From %s)\n"
"  Scheduled time:         %s\n"
"  Start time:             %s\n"
"  End time:               %s\n"
"  Elapsed time:           %s\n"
"  Priority:               %d\n"
"  FD Files Written:       %s\n"
"  SD Files Written:       %s\n"
"  FD Bytes Written:       %s (%sB)\n"
"  SD Bytes Written:       %s (%sB)\n"
"  Rate:                   %.1f KB/s\n"
"  Software Compression:   %s\n"
"  VSS:                    %s\n"
"  Encryption:             %s\n"
"  Accurate:               %s\n"
"  Volume name(s):         %s\n"
"  Volume Session Id:      %d\n"
"  Volume Session Time:    %d\n"
"  Last Volume Bytes:      %s (%sB)\n"
"  Non-fatal FD errors:    %d\n"
"  SD Errors:              %d\n"
"  FD termination status:  %s\n"
"  SD termination status:  %s\n"
"  Termination:            %s\n\n"),
        my_name, VERSION, LSMDATE, edt,
        HOST_OS, DISTNAME, DISTVER,
        jcr->jr.JobId,
        jcr->jr.Job,
        level_to_str(jcr->JobLevel), jcr->since,
        jcr->client->name(), cr.Uname,
        jcr->fileset->name(), jcr->FSCreateTime,
        jcr->pool->name(), jcr->pool_source,
        jcr->catalog->name(), jcr->catalog_source,
        jcr->wstore->name(), jcr->wstore_source,
        schedt,
        sdt,
        edt,
        edit_utime(RunTime, elapsed, sizeof(elapsed)),
        jcr->JobPriority,
        edit_uint64_with_commas(jcr->jr.JobFiles, ec1),
        edit_uint64_with_commas(jcr->SDJobFiles, ec2),
        edit_uint64_with_commas(jcr->jr.JobBytes, ec3),
        edit_uint64_with_suffix(jcr->jr.JobBytes, ec4),
        edit_uint64_with_commas(jcr->SDJobBytes, ec5),
        edit_uint64_with_suffix(jcr->SDJobBytes, ec6),
        kbps,
        compress,
        jcr->VSS?_("yes"):_("no"),
        jcr->Encrypt?_("yes"):_("no"),
        jcr->accurate?_("yes"):_("no"),
        jcr->VolumeName,
        jcr->VolSessionId,
        jcr->VolSessionTime,
        edit_uint64_with_commas(mr.VolBytes, ec7),
        edit_uint64_with_suffix(mr.VolBytes, ec8),
        jcr->Errors,
        jcr->SDErrors,
        fd_term_msg,
        sd_term_msg,
        term_msg);

   Dmsg0(100, "Leave vbackup_cleanup()\n");
}

/*
 * This callback routine is responsible for inserting the
 *  items it gets into the bootstrap structure. For each JobId selected
 *  this routine is called once for each file. We do not allow
 *  duplicate filenames, but instead keep the info from the most
 *  recent file entered (i.e. the JobIds are assumed to be sorted)
 *
 *   See uar_sel_files in sql_cmds.c for query that calls us.
 *      row[0]=Path, row[1]=Filename, row[2]=FileIndex
 *      row[3]=JobId row[4]=LStat
 */
int insert_bootstrap_handler(void *ctx, int num_fields, char **row)
{
   JobId_t JobId;
   int FileIndex;
   RBSR *bsr = (RBSR *)ctx;

   JobId = str_to_int64(row[3]);
   FileIndex = str_to_int64(row[2]);
   add_findex(bsr, JobId, FileIndex);
   return 0;
}


static bool create_bootstrap_file(JCR *jcr, POOLMEM *jobids)
{
   RESTORE_CTX rx;
   UAContext *ua;

   memset(&rx, 0, sizeof(rx));
   rx.bsr = new_bsr();
   ua = new_ua_context(jcr);
   rx.JobIds = jobids;

#define new_get_file_list
#ifdef new_get_file_list
   if (!db_get_file_list(jcr, ua->db, jobids, insert_bootstrap_handler, (void *)rx.bsr)) {
      Jmsg(jcr, M_ERROR, 0, "%s", db_strerror(ua->db));
   }
#else
   char *p;
   JobId_t JobId, last_JobId = 0;
   rx.query = get_pool_memory(PM_MESSAGE);
   for (p=rx.JobIds; get_next_jobid_from_list(&p, &JobId) > 0; ) {
      char ed1[50];

      if (JobId == last_JobId) {
         continue;                    /* eliminate duplicate JobIds */
      }
      last_JobId = JobId;
      /*
       * Find files for this JobId and insert them in the tree
       */
      Mmsg(rx.query, uar_sel_files, edit_int64(JobId, ed1));
      Dmsg1(000, "uar_sel_files=%s\n", rx.query);
      if (!db_sql_query(ua->db, rx.query, insert_bootstrap_handler, (void *)rx.bsr)) {
         Jmsg(jcr, M_ERROR, 0, "%s", db_strerror(ua->db));
      }
      free_pool_memory(rx.query);
      rx.query = NULL;
   }
#endif

   complete_bsr(ua, rx.bsr);
   Dmsg0(000, "Print bsr\n");
   print_bsr(ua, rx.bsr);

   jcr->ExpectedFiles = write_bsr_file(ua, rx);
   Dmsg1(000, "Found %d files to consolidate.\n", jcr->ExpectedFiles);
   if (jcr->ExpectedFiles == 0) {
      free_ua_context(ua);
      free_bsr(rx.bsr);
      return false;
   }
   free_ua_context(ua);
   free_bsr(rx.bsr);
   return true;
}
