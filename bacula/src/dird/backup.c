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
#include "ua.h"

/* Commands sent to File daemon */
static char backupcmd[] = "backup\n";
static char storaddr[]  = "storage address=%s port=%d\n";
static char levelcmd[]  = "level = %s%s\n";

/* Responses received from File daemon */
static char OKbackup[] = "2000 OK backup\n";
static char OKstore[]  = "2000 OK storage\n";
static char OKlevel[]  = "2000 OK level\n";

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
      backup_cleanup(jcr, JS_ErrorTerminated, since);
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
   } else {
      Jmsg(jcr, M_WARNING, 0, _("FileSet MD5 signature not found.\n"));
   }
   if (!db_create_fileset_record(jcr->db, &fsr)) {
      Jmsg(jcr, M_ERROR, 0, _("Could not create FileSet record. %s"), 
	 db_strerror(jcr->db));
      backup_cleanup(jcr, JS_ErrorTerminated, since);
      return 0;
   }   
   jcr->jr.FileSetId = fsr.FileSetId;
   Dmsg2(9, "Created FileSet %s record %d\n", jcr->fileset->hdr.name, 
      jcr->jr.FileSetId);


   /* Look up the last
    * FULL backup job to get the time/date for a 
    * differential or incremental save.
    */
   jcr->stime = (char *) get_pool_memory(PM_MESSAGE);
   jcr->stime[0] = 0;
   since[0] = 0;
   switch (jcr->level) {
      case L_DIFFERENTIAL:
      case L_INCREMENTAL:
	 /* Look up start time of last job */
	 jcr->jr.JobId = 0;
	 if (!db_find_job_start_time(jcr->db, &jcr->jr, jcr->stime)) {
            Jmsg(jcr, M_INFO, 0, _("Last FULL backup time not found. Doing FULL backup.\n"));
	    jcr->level = L_FULL;
	    jcr->jr.Level = L_FULL;
	 } else {
            strcpy(since, ", since=");
	    strcat(since, jcr->stime);
	 }
         Dmsg1(15, "Last start time = %s\n", jcr->stime);
	 break;
   }

   jcr->jr.JobId = jcr->JobId;
   jcr->jr.StartTime = jcr->start_time;
   if (!db_update_job_start_record(jcr->db, &jcr->jr)) {
      Jmsg(jcr, M_ERROR, 0, "%s", db_strerror(jcr->db));
      backup_cleanup(jcr, JS_ErrorTerminated, since);
      return 0;
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
   while (!db_get_pool_record(jcr->db, &pr)) { /* get by Name */
      /* Try to create the pool */
      if (create_pool(jcr->db, jcr->pool) < 0) {
         Jmsg(jcr, M_FATAL, 0, _("Pool %s not in database. %s"), pr.Name, 
	    db_strerror(jcr->db));
	 backup_cleanup(jcr, JS_ErrorTerminated, since);
	 return 0;
      } else {
         Jmsg(jcr, M_INFO, 0, _("Pool %s created in database.\n"), pr.Name);
      }
   }
   jcr->PoolId = pr.PoolId;		  /****FIXME**** this can go away */
   jcr->jr.PoolId = pr.PoolId;

#ifdef needed
   /* NOTE, THIS IS NOW DONE BY THE STORAGE DAEMON
    *
    * Find at least one Volume associated with this Pool
    *  It must be marked Append, and be of the correct Media Type
    *  for the storage type.
    */
   memset(&mr, 0, sizeof(mr));
   mr.PoolId = pr.PoolId;
   strcpy(mr.VolStatus, "Append");
   strcpy(mr.MediaType, jcr->store->media_type);
   if (!db_find_next_volume(jcr->db, 1, &mr)) {
      if (!newVolume(jcr)) {
         Jmsg(jcr, M_FATAL, 0, _("No writable %s media in Pool %s.\n\
      Please use the Console program to add available Volumes.\n"), mr.MediaType, pr.Name);
	 backup_cleanup(jcr, JS_ErrorTerminated, since);
	 return 0;
      }
   }
#endif

   /*
    * Open a message channel connection with the Storage
    * daemon. This is to let him know that our client
    * will be contacting him for a backup  session.
    *
    */
   Dmsg0(10, "Open connection with storage daemon\n");
   jcr->JobStatus = JS_Blocked;
   /*
    * Start conversation with Storage daemon  
    */
   if (!connect_to_storage_daemon(jcr, 10, SDConnectTimeout, 1)) {
      backup_cleanup(jcr, JS_ErrorTerminated, since);
      return 0;
   }
   /*
    * Now start a job with the Storage daemon
    */
   if (!start_storage_daemon_job(jcr)) {
      backup_cleanup(jcr, JS_ErrorTerminated, since);
      return 0;
   }
   /*
    * Now start a Storage daemon message thread
    */
   if (!start_storage_daemon_message_thread(jcr)) {
      backup_cleanup(jcr, JS_ErrorTerminated, since);
      return 0;
   }

   Dmsg0(50, "Storage daemon connection OK\n");

   if (!connect_to_file_daemon(jcr, 10, FDConnectTimeout, 1)) {
      backup_cleanup(jcr, JS_ErrorTerminated, since);
      return 0;
   }

   jcr->JobStatus = JS_Running;
   fd = jcr->file_bsock;

   if (!send_include_list(jcr)) {
      backup_cleanup(jcr, JS_ErrorTerminated, since);
      return 0;
   }

   if (!send_exclude_list(jcr)) {
      backup_cleanup(jcr, JS_ErrorTerminated, since);
      return 0;
   }

   /* 
    * send Storage daemon address to the File daemon
    */
   if (jcr->store->SDDport == 0) {
      jcr->store->SDDport = jcr->store->SDport;
   }
   bnet_fsend(fd, storaddr, jcr->store->address, jcr->store->SDDport);
   if (!response(fd, OKstore, "Storage")) {
      backup_cleanup(jcr, JS_ErrorTerminated, since);
      return 0;
   }

   /* 
    * Send Level command to File daemon
    */
   switch (jcr->level) {
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
         Emsg1(M_FATAL, 0, _("Unimplemented backup level %d\n"), jcr->level);
	 backup_cleanup(jcr, JS_ErrorTerminated, since);
	 return 0;
   }
   Dmsg1(20, ">filed: %s", fd->msg);
   if (!response(fd, OKlevel, "Level")) {
      backup_cleanup(jcr, JS_ErrorTerminated, since);
      return 0;
   }

   /* Send backup command */
   bnet_fsend(fd, backupcmd);
   if (!response(fd, OKbackup, "backup")) {
      backup_cleanup(jcr, JS_ErrorTerminated, since);
      return 0;
   }

   /* Pickup Job termination data */	    
   stat = wait_for_job_termination(jcr);
   backup_cleanup(jcr, stat, since);
   return 1;
}

/*
 *  NOTE! This is no longer really needed as the Storage
 *	  daemon now passes this information directly
 *	  back to us.	
 */
static int wait_for_job_termination(JCR *jcr)
{
   int32_t n = 0;
   BSOCK *fd = jcr->file_bsock;

   jcr->JobStatus = JS_WaitFD;
   /* Wait for Client to terminate */
   while ((n = bget_msg(fd, 0)) > 0 && !job_cancelled(jcr)) {
      /* get and discard Client output */
   }
   bnet_sig(fd, BNET_TERMINATE);      /* tell Client we are terminating */
   if (n < 0) {
      Jmsg(jcr, M_FATAL, 0, _("<filed: network error during BACKUP command. ERR=%s\n"),
	  bnet_strerror(fd));
   }

   /* Now wait for Storage daemon to terminate our message thread */
   P(jcr->mutex);
   jcr->JobStatus = JS_WaitSD;
   while (!jcr->msg_thread_done && !job_cancelled(jcr)) {
      struct timeval tv;
      struct timezone tz;
      struct timespec timeout;

      gettimeofday(&tv, &tz);
      timeout.tv_nsec = 0;
      timeout.tv_sec = tv.tv_sec + 10; /* wait 10 seconds */
      Dmsg0(300, "I'm waiting for message thread termination.\n");
      pthread_cond_timedwait(&jcr->term_wait, &jcr->mutex, &timeout);
   }
   V(jcr->mutex);
   if (n < 0) { 				    
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
   char ec1[30], ec2[30], ec3[30];
   char term_code[100];
   char *term_msg;
   int msg_type;
   MEDIA_DBR mr;

   Dmsg0(100, "Enter backup_cleanup()\n");
   memset(&mr, 0, sizeof(mr));
   jcr->JobStatus = TermCode;

   update_job_end_record(jcr);	      /* update database */
   
   if (!db_get_job_record(jcr->db, &jcr->jr)) {
      Jmsg(jcr, M_WARNING, 0, _("Error getting job record for stats: %s"), 
	 db_strerror(jcr->db));
   }

   strcpy(mr.VolumeName, jcr->VolumeName);
   if (!db_get_media_record(jcr->db, &mr)) {
      Jmsg(jcr, M_WARNING, 0, _("Error getting Media record for stats: %s"), 
	 db_strerror(jcr->db));
   }

      
   msg_type = M_INFO;		      /* by default INFO message */
   switch (TermCode) {
      case JS_Terminated:
         term_msg = _("Backup OK");
	 break;
      case JS_Errored:
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
         sprintf(term_code, _("Inappropriate term code: %c\n"), TermCode);
	 break;
   }
   bstrftime(sdt, sizeof(sdt), jcr->jr.StartTime);
   bstrftime(edt, sizeof(edt), jcr->jr.EndTime);
   if (!db_get_job_volume_names(jcr->db, jcr->jr.JobId, jcr->VolumeName)) {
      jcr->VolumeName[0] = 0;	      /* none */
   }

   Jmsg(jcr, msg_type, 0, _("%s\n\
JobId:                  %d\n\
Job:                    %s\n\
FileSet:                %s\n\
Backup Level:           %s%s\n\
Client:                 %s\n\
Start time:             %s\n\
End time:               %s\n\
Bytes Written:          %s\n\
Files Written:          %s\n\
Volume names(s):        %s\n\
Volume Session Id:      %d\n\
Volume Session Time:    %d\n\
Volume Bytes:           %s\n\
Termination:            %s\n\n"),
	edt,
	jcr->jr.JobId,
	jcr->jr.Job,
	jcr->fileset->hdr.name,
	level_to_str(jcr->level), since,
	jcr->client->hdr.name,
	sdt,
	edt,
	edit_uint64_with_commas(jcr->jr.JobBytes, ec1),
	edit_uint64_with_commas(jcr->jr.JobFiles, ec2),
	jcr->VolumeName,
	jcr->VolSessionId,
	jcr->VolSessionTime,
	edit_uint64_with_commas(mr.VolBytes, ec3),
	term_msg);

   Dmsg0(100, "Leave backup_cleanup()\n");
}
