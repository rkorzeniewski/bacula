/*
 *
 *   Bacula Director -- backup.c -- responsible for doing backup jobs
 *
 *     Kern Sibbald, March MM
 *
 *    This routine is called as a thread. It may not yet be totally
 *	thread reentrant!!!
 *
 *  Basic tasks done here:
 *     Open DB and create records for this job.
 *     Open Message Channel with Storage daemon to tell him a job will be starting.
 *     Open connection with File daemon and pass him commands
 *	 to do the backup.
 *     When the File daemon finishes the job, update the DB.
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
#include "ua.h"

/* Commands sent to File daemon */
static char backupcmd[] = "backup\n";
static char storaddr[]  = "storage address=%s port=%d\n";
static char levelcmd[]  = "level = %s%s\n";

/* Responses received from File daemon */
static char OKbackup[]  = "2000 OK backup\n";
static char OKstore[]   = "2000 OK storage\n";
static char OKlevel[]   = "2000 OK level\n";
static char EndBackup[] = "2801 End Backup Job TermCode=%d JobFiles=%u ReadBytes=%" lld " JobBytes=%" lld "\n";


/* Forward referenced functions */
static void backup_cleanup(JCR *jcr, int TermCode, char *since);
static int wait_for_job_termination(JCR *jcr);		     

/* External functions */

/* 
 * Do a backup of the specified FileSet
 *    
 *  Returns:  0 on failure
 *	      1 on success
 */
int do_backup(JCR *jcr) 
{
   char since[MAXSTRING];
   int stat;
   BSOCK   *fd;
   POOL_DBR pr;
   FILESET_DBR fsr;

   since[0] = 0;

   if (!get_or_create_client_record(jcr)) {
      Jmsg(jcr, M_ERROR, 0, _("Could not get/create Client record. ERR=%s\n"), 
	 db_strerror(jcr->db));
      goto bail_out;
   }

   /*
    * Get or Create FileSet record
    */
   memset(&fsr, 0, sizeof(fsr));
   strcpy(fsr.FileSet, jcr->fileset->hdr.name);
   if (jcr->fileset->have_MD5) {
      struct MD5Context md5c;
      unsigned char signature[16];
      memcpy(&md5c, &jcr->fileset->md5c, sizeof(md5c));
      MD5Final(signature, &md5c);
      bin_to_base64(fsr.MD5, (char *)signature, 16); /* encode 16 bytes */
      strcpy(jcr->fileset->MD5, fsr.MD5);
   } else {
      Jmsg(jcr, M_WARNING, 0, _("FileSet MD5 signature not found.\n"));
   }
   if (!db_create_fileset_record(jcr, jcr->db, &fsr)) {
      Jmsg(jcr, M_ERROR, 0, _("Could not create FileSet record. ERR=%s\n"), 
	 db_strerror(jcr->db));
      goto bail_out;
   }   
   jcr->jr.FileSetId = fsr.FileSetId;
   Dmsg2(119, "Created FileSet %s record %d\n", jcr->fileset->hdr.name, 
      jcr->jr.FileSetId);

   /* Look up the last
    * FULL backup job to get the time/date for a 
    * differential or incremental save.
    */
   jcr->stime = get_pool_memory(PM_MESSAGE);
   jcr->stime[0] = 0;
   since[0] = 0;
   switch (jcr->JobLevel) {
      case L_DIFFERENTIAL:
      case L_INCREMENTAL:
	 /* Look up start time of last job */
	 jcr->jr.JobId = 0;
	 if (!db_find_job_start_time(jcr, jcr->db, &jcr->jr, &jcr->stime)) {
            Jmsg(jcr, M_INFO, 0, _("Last FULL backup time not found. Doing FULL backup.\n"));
	    jcr->JobLevel = jcr->jr.Level = L_FULL;
	 } else {
            strcpy(since, ", since=");
	    bstrncat(since, jcr->stime, sizeof(since));
	 }
         Dmsg1(115, "Last start time = %s\n", jcr->stime);
	 break;
   }

   jcr->jr.JobId = jcr->JobId;
   jcr->jr.StartTime = jcr->start_time;
   if (!db_update_job_start_record(jcr, jcr->db, &jcr->jr)) {
      Jmsg(jcr, M_ERROR, 0, "%s", db_strerror(jcr->db));
      goto bail_out;
   }

   jcr->fname = (char *) get_pool_memory(PM_FNAME);

   /* Print Job Start message */
   Jmsg(jcr, M_INFO, 0, _("Start Backup JobId %d, Job=%s\n"),
	jcr->JobId, jcr->Job);

   /* 
    * Get the Pool record  
    */
   memset(&pr, 0, sizeof(pr));
   strcpy(pr.Name, jcr->pool->hdr.name);
   while (!db_get_pool_record(jcr, jcr->db, &pr)) { /* get by Name */
      /* Try to create the pool */
      if (create_pool(jcr, jcr->db, jcr->pool) < 0) {
         Jmsg(jcr, M_FATAL, 0, _("Pool %s not in database. %s"), pr.Name, 
	    db_strerror(jcr->db));
	 goto bail_out;
      } else {
         Jmsg(jcr, M_INFO, 0, _("Pool %s created in database.\n"), pr.Name);
      }
   }
   jcr->PoolId = pr.PoolId;		  /****FIXME**** this can go away */
   jcr->jr.PoolId = pr.PoolId;

   /*
    * Open a message channel connection with the Storage
    * daemon. This is to let him know that our client
    * will be contacting him for a backup  session.
    *
    */
   Dmsg0(110, "Open connection with storage daemon\n");
   set_jcr_job_status(jcr, JS_Blocked);
   /*
    * Start conversation with Storage daemon  
    */
   if (!connect_to_storage_daemon(jcr, 10, SDConnectTimeout, 1)) {
      goto bail_out;
   }
   /*
    * Now start a job with the Storage daemon
    */
   if (!start_storage_daemon_job(jcr)) {
      goto bail_out;
   }
   /*
    * Now start a Storage daemon message thread
    */
   if (!start_storage_daemon_message_thread(jcr)) {
      goto bail_out;
   }
   Dmsg0(150, "Storage daemon connection OK\n");

   set_jcr_job_status(jcr, JS_Blocked);
   if (!connect_to_file_daemon(jcr, 10, FDConnectTimeout, 1)) {
      goto bail_out;
   }

   set_jcr_job_status(jcr, JS_Running);
   fd = jcr->file_bsock;

   if (!send_include_list(jcr)) {
      goto bail_out;
   }

   if (!send_exclude_list(jcr)) {
      goto bail_out;
   }

   /* 
    * send Storage daemon address to the File daemon
    */
   if (jcr->store->SDDport == 0) {
      jcr->store->SDDport = jcr->store->SDport;
   }
   bnet_fsend(fd, storaddr, jcr->store->address, jcr->store->SDDport);
   if (!response(fd, OKstore, "Storage")) {
      goto bail_out;
   }

   /* 
    * Send Level command to File daemon
    */
   switch (jcr->JobLevel) {
      case L_FULL:
         bnet_fsend(fd, levelcmd, "full", " ");
	 break;
      case L_DIFFERENTIAL:
      case L_INCREMENTAL:
         bnet_fsend(fd, levelcmd, "since ", jcr->stime);
	 free_pool_memory(jcr->stime);
	 jcr->stime = NULL;
	 break;
      case L_SINCE:
      default:
         Jmsg2(jcr, M_FATAL, 0, _("Unimplemented backup level %d %c\n"), 
	    jcr->JobLevel, jcr->JobLevel);
	 goto bail_out;
   }
   Dmsg1(120, ">filed: %s", fd->msg);
   if (!response(fd, OKlevel, "Level")) {
      goto bail_out;
   }

   /* Send backup command */
   bnet_fsend(fd, backupcmd);
   if (!response(fd, OKbackup, "backup")) {
      goto bail_out;
   }

   /* Pickup Job termination data */	    
   stat = wait_for_job_termination(jcr);
   backup_cleanup(jcr, stat, since);
   return 1;

bail_out:
   if (jcr->stime) {
      free_pool_memory(jcr->stime);
      jcr->stime = NULL;
   }
   backup_cleanup(jcr, JS_ErrorTerminated, since);
   return 0;

}

/*
 * Here we wait for the File daemon to signal termination,
 *   then we wait for the Storage daemon.  When both
 *   are done, we return the job status.
 */
static int wait_for_job_termination(JCR *jcr)
{
   int32_t n = 0;
   BSOCK *fd = jcr->file_bsock;
   int fd_ok = FALSE;

   set_jcr_job_status(jcr, JS_Running);
   /* Wait for Client to terminate */
   while ((n = bget_msg(fd, 0)) >= 0) {
      if (sscanf(fd->msg, EndBackup, &jcr->FDJobStatus, &jcr->JobFiles,
	  &jcr->ReadBytes, &jcr->JobBytes) == 4) {
	 fd_ok = TRUE;
	 set_jcr_job_status(jcr, jcr->FDJobStatus);
         Dmsg1(100, "FDStatus=%c\n", (char)jcr->JobStatus);
      }
      if (job_cancelled(jcr)) {
	 break;
      }
   }
   if (is_bnet_error(fd)) {
      Jmsg(jcr, M_FATAL, 0, _("<filed: network error during BACKUP command. ERR=%s\n"),
	  bnet_strerror(fd));
   }
   bnet_sig(fd, BNET_TERMINATE);   /* tell Client we are terminating */

   wait_for_storage_daemon_termination(jcr);

   /* Return the first error status we find FD or SD */
   if (fd_ok && jcr->JobStatus != JS_Terminated) {
      return jcr->JobStatus;
   }
   if (!fd_ok || is_bnet_error(fd)) {			       
      return JS_ErrorTerminated;
   }
   return jcr->SDJobStatus;
}

/*
 * Release resources allocated during backup.
 */
static void backup_cleanup(JCR *jcr, int TermCode, char *since)
{
   char sdt[50], edt[50];
   char ec1[30], ec2[30], ec3[30], compress[50];
   char term_code[100], fd_term_msg[100], sd_term_msg[100];
   char *term_msg;
   int msg_type;
   MEDIA_DBR mr;
   double kbps, compression;
   utime_t RunTime;

   Dmsg0(100, "Enter backup_cleanup()\n");
   memset(&mr, 0, sizeof(mr));
   set_jcr_job_status(jcr, TermCode);

   update_job_end_record(jcr);	      /* update database */
   
   if (!db_get_job_record(jcr, jcr->db, &jcr->jr)) {
      Jmsg(jcr, M_WARNING, 0, _("Error getting job record for stats: %s"), 
	 db_strerror(jcr->db));
      set_jcr_job_status(jcr, JS_ErrorTerminated);
   }

   strcpy(mr.VolumeName, jcr->VolumeName);
   if (!db_get_media_record(jcr, jcr->db, &mr)) {
      Jmsg(jcr, M_WARNING, 0, _("Error getting Media record for stats: %s"), 
	 db_strerror(jcr->db));
      set_jcr_job_status(jcr, JS_ErrorTerminated);
   }

   /* Now update the bootstrap file if any */
   if (jcr->JobStatus == JS_Terminated && jcr->job->WriteBootstrap) {
      FILE *fd;
      BPIPE *bpipe = NULL;
      int got_pipe = 0;
      char *fname = jcr->job->WriteBootstrap;
      VOL_PARAMS *VolParams = NULL;
      int VolCount;

      if (*fname == '|') {
	 fname++;
	 got_pipe = 1;
         bpipe = open_bpipe(fname, 0, "w");
	 fd = bpipe ? bpipe->wfd : NULL;
      } else {
         fd = fopen(fname, jcr->JobLevel==L_FULL?"w+":"a+");
      }
      if (fd) {
	 VolCount = db_get_job_volume_parameters(jcr, jcr->db, jcr->JobId,
		    &VolParams);
	 if (VolCount == 0) {
            Jmsg(jcr, M_ERROR, 0, _("Could not get Job Volume Parameters. ERR=%s\n"),
		 db_strerror(jcr->db));
	 }
	 for (int i=0; i < VolCount; i++) {
	    /* Write the record */
            fprintf(fd, "Volume=\"%s\"\n", VolParams[i].VolumeName);
            fprintf(fd, "VolSessionId=%u\n", jcr->VolSessionId);
            fprintf(fd, "VolSessionTime=%u\n", jcr->VolSessionTime);
            fprintf(fd, "VolFile=%u-%u\n", VolParams[i].StartFile,
			 VolParams[i].EndFile);
            fprintf(fd, "VolBlock=%u-%u\n", VolParams[i].StartBlock,
			 VolParams[i].EndBlock);
            fprintf(fd, "FileIndex=%d-%d\n", VolParams[i].FirstIndex,
			 VolParams[i].LastIndex);
	 }
	 if (VolParams) {
	    free(VolParams);
	 }
	 if (got_pipe) {
	    close_bpipe(bpipe);
	 } else {
	    fclose(fd);
	 }
      } else {
         Jmsg(jcr, M_ERROR, 0, _("Could not open WriteBootstrap file:\n"
              "%s: ERR=%s\n"), fname, strerror(errno));
	 set_jcr_job_status(jcr, JS_ErrorTerminated);
      }
   }

   msg_type = M_INFO;		      /* by default INFO message */
   switch (jcr->JobStatus) {
      case JS_Terminated:
         term_msg = _("Backup OK");
	 break;
      case JS_FatalError:
      case JS_ErrorTerminated:
         term_msg = _("*** Backup Error ***"); 
	 msg_type = M_ERROR;	      /* Generate error message */
	 if (jcr->store_bsock) {
	    bnet_sig(jcr->store_bsock, BNET_TERMINATE);
	    pthread_cancel(jcr->SD_msg_chan);
	 }
	 break;
      case JS_Cancelled:
         term_msg = _("Backup Cancelled");
	 if (jcr->store_bsock) {
	    bnet_sig(jcr->store_bsock, BNET_TERMINATE);
	    pthread_cancel(jcr->SD_msg_chan);
	 }
	 break;
      default:
	 term_msg = term_code;
         sprintf(term_code, _("Inappropriate term code: %c\n"), jcr->JobStatus);
	 break;
   }
   bstrftime(sdt, sizeof(sdt), jcr->jr.StartTime);
   bstrftime(edt, sizeof(edt), jcr->jr.EndTime);
   RunTime = jcr->jr.EndTime - jcr->jr.StartTime;
   if (RunTime <= 0) {
      kbps = 0;
   } else {
      kbps = (double)jcr->jr.JobBytes / (1000 * RunTime);
   }
   if (!db_get_job_volume_names(jcr, jcr->db, jcr->jr.JobId, &jcr->VolumeName)) {
      /*
       * Note, if the job has erred, most likely it did not write any
       *  tape, so suppress this "error" message since in that case
       *  it is normal.  Or look at it the other way, only for a
       *  normal exit should we complain about this error.
       */
      if (jcr->JobStatus == JS_Terminated) {				    
         Jmsg(jcr, M_ERROR, 0, "%s", db_strerror(jcr->db));
      }
      jcr->VolumeName[0] = 0;	      /* none */
   }

   if (jcr->ReadBytes == 0) {
      strcpy(compress, "None");
   } else {
      compression = (double)100 - 100.0 * ((double)jcr->JobBytes / (double)jcr->ReadBytes);
      if (compression < 0.5) {
         strcpy(compress, "None");
      } else {
         sprintf(compress, "%.1f %%", (float)compression);
      }
   }
   jobstatus_to_ascii(jcr->FDJobStatus, fd_term_msg, sizeof(fd_term_msg));
   jobstatus_to_ascii(jcr->SDJobStatus, sd_term_msg, sizeof(sd_term_msg));

   Jmsg(jcr, msg_type, 0, _("Bacula " VERSION " (" LSMDATE "): %s\n\
JobId:                  %d\n\
Job:                    %s\n\
FileSet:                %s\n\
Backup Level:           %s%s\n\
Client:                 %s\n\
Start time:             %s\n\
End time:               %s\n\
Files Written:          %s\n\
Bytes Written:          %s\n\
Rate:                   %.1f KB/s\n\
Software Compression:   %s\n\
Volume names(s):        %s\n\
Volume Session Id:      %d\n\
Volume Session Time:    %d\n\
Last Volume Bytes:      %s\n\
FD termination status:  %s\n\
SD termination status:  %s\n\
Termination:            %s\n\n"),
	edt,
	jcr->jr.JobId,
	jcr->jr.Job,
	jcr->fileset->hdr.name,
	level_to_str(jcr->JobLevel), since,
	jcr->client->hdr.name,
	sdt,
	edt,
	edit_uint64_with_commas(jcr->jr.JobFiles, ec1),
	edit_uint64_with_commas(jcr->jr.JobBytes, ec2),
	(float)kbps,
	compress,
	jcr->VolumeName,
	jcr->VolSessionId,
	jcr->VolSessionTime,
	edit_uint64_with_commas(mr.VolBytes, ec3),
	fd_term_msg,
	sd_term_msg,
	term_msg);


   Dmsg0(100, "Leave backup_cleanup()\n");
}
