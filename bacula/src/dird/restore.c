/*
 *
 *   Bacula Director -- restore.c -- responsible for restoring files
 *
 *     Kern Sibbald, November MM
 *
 *    This routine is run as a separate thread.  There may be more
 *    work to be done to make it totally reentrant!!!!
 * 
 * Current implementation is Catalog verification only (i.e. no
 *  verification versus tape).
 *
 *  Basic tasks done here:
 *     Open DB
 *     Open Message Channel with Storage daemon to tell him a job will be starting.
 *     Open connection with File daemon and pass him commands
 *	 to do the restore.
 *     Update the DB according to what files where restored????
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

/* Commands sent to File daemon */
static char restorecmd[]   = "restore where=%s\n";
static char storaddr[]     = "storage address=%s port=%d\n";
static char sessioncmd[]   = "session %s %ld %ld %ld %ld %ld %ld\n";  

/* Responses received from File daemon */
static char OKrestore[]   = "2000 OK restore\n";
static char OKstore[]     = "2000 OK storage\n";
static char OKsession[]   = "2000 OK session\n";
static char OKbootstrap[] = "2000 OK bootstrap\n";

/* Forward referenced functions */
static void restore_cleanup(JCR *jcr, int status);
static int send_bootstrap_file(JCR *jcr);

/* External functions */

/* 
 * Do a restore of the specified files
 *    
 *  Returns:  0 on failure
 *	      1 on success
 */
int do_restore(JCR *jcr) 
{
   char dt[MAX_TIME_LENGTH];
   BSOCK   *fd;
   JOB_DBR rjr; 		      /* restore job record */


   if (!get_or_create_client_record(jcr)) {
      restore_cleanup(jcr, JS_ErrorTerminated);
      return 0;
   }

   memset(&rjr, 0, sizeof(rjr));
   jcr->jr.Level = 'F';            /* Full restore */
   jcr->jr.StartTime = jcr->start_time;
   if (!db_update_job_start_record(jcr->db, &jcr->jr)) {
      Jmsg(jcr, M_ERROR, 0, "%s", db_strerror(jcr->db));
      restore_cleanup(jcr, JS_ErrorTerminated);
      return 0;
   }
   Dmsg0(20, "Updated job start record\n");
   jcr->fname = (char *) get_pool_memory(PM_FNAME);

   Dmsg1(20, "RestoreJobId=%d\n", jcr->job->RestoreJobId);

   /* 
    * The following code is kept temporarily for compatibility.
    * It is the predecessor to the Bootstrap file.
    */
   if (!jcr->RestoreBootstrap) {
      /*
       * Find Job Record for Files to be restored
       */
      if (jcr->RestoreJobId != 0) {
	 rjr.JobId = jcr->RestoreJobId;     /* specified by UA */
      } else {
	 rjr.JobId = jcr->job->RestoreJobId; /* specified by Job Resource */
      }
      if (!db_get_job_record(jcr->db, &rjr)) {
         Jmsg2(jcr, M_FATAL, 0, _("Cannot get job record id=%d %s"), rjr.JobId,
	    db_strerror(jcr->db));
	 restore_cleanup(jcr, JS_ErrorTerminated);
	 return 0;
      }

      /*
       * Now find the Volumes we will need for the Restore
       */
      jcr->VolumeName[0] = 0;
      if (!db_get_job_volume_names(jcr->db, rjr.JobId, jcr->VolumeName) ||
	   jcr->VolumeName[0] == 0) {
         Jmsg(jcr, M_FATAL, 0, _("Cannot find Volume Name for restore Job %d. %s"), 
	    rjr.JobId, db_strerror(jcr->db));
	 restore_cleanup(jcr, JS_ErrorTerminated);
	 return 0;
      }
      Dmsg1(20, "Got job Volume Names: %s\n", jcr->VolumeName);
   }
      

   /* Print Job Start message */
   bstrftime(dt, sizeof(dt), jcr->start_time);
   Jmsg(jcr, M_INFO, 0, _("%s Start Restore Job %s Name=%s, Client=%s, FileSet=%s\n"), 
      dt, jcr->Job, jcr->job->hdr.name, jcr->client->hdr.name, 
      jcr->fileset->hdr.name);

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
      restore_cleanup(jcr, JS_ErrorTerminated);
      return 0;
   }
   /*
    * Now start a job with the Storage daemon
    */
   if (!start_storage_daemon_job(jcr)) {
      restore_cleanup(jcr, JS_ErrorTerminated);
      return 0;
   }
   /*
    * Now start a Storage daemon message thread
    */
   if (!start_storage_daemon_message_thread(jcr)) {
      restore_cleanup(jcr, JS_ErrorTerminated);
      return 0;
   }
   Dmsg0(50, "Storage daemon connection OK\n");

   /* 
    * Start conversation with File daemon  
    */
   if (!connect_to_file_daemon(jcr, 10, FDConnectTimeout, 1)) {
      restore_cleanup(jcr, JS_ErrorTerminated);
      return 0;
   }

   fd = jcr->file_bsock;
   jcr->JobStatus = JS_Running;

   if (!send_include_list(jcr)) {
      restore_cleanup(jcr, JS_ErrorTerminated);
      return 0;
   }

   if (!send_exclude_list(jcr)) {
      restore_cleanup(jcr, JS_ErrorTerminated);
      return 0;
   }


   /* 
    * send Storage daemon address to the File daemon,
    *	then wait for File daemon to make connection
    *	with Storage daemon.
    */
   jcr->JobStatus = JS_Blocked;
   if (jcr->store->SDDport == 0) {
      jcr->store->SDDport = jcr->store->SDport;
   }
   bnet_fsend(fd, storaddr, jcr->store->address, jcr->store->SDDport);
   Dmsg1(6, "dird>filed: %s\n", fd->msg);
   if (!response(fd, OKstore, "Storage")) {
      restore_cleanup(jcr, JS_ErrorTerminated);
      return 0;
   }
   jcr->JobStatus = JS_Running;

   /* 
    * Send the bootstrap file -- what Volumes/files to restore
    */
   if (!send_bootstrap_file(jcr)) {
      restore_cleanup(jcr, JS_ErrorTerminated);
      return 0;
   }

   /* 
    * The following code is deprecated	 
    */
   if (!jcr->RestoreBootstrap) {
      /*
       * Pass the VolSessionId, VolSessionTime, Start and
       * end File and Blocks on the session command.
       */
      bnet_fsend(fd, sessioncmd, 
		jcr->VolumeName,
		rjr.VolSessionId, rjr.VolSessionTime, 
		rjr.StartFile, rjr.EndFile, rjr.StartBlock, 
		rjr.EndBlock);
      if (!response(fd, OKsession, "Session")) {
	 restore_cleanup(jcr, JS_ErrorTerminated);
	 return 0;
      }
   }

   /* Send restore command */
   if (jcr->RestoreWhere) {
      bnet_fsend(fd, restorecmd, jcr->RestoreWhere);
   } else {
      bnet_fsend(fd, restorecmd, 
                 jcr->job->RestoreWhere ? jcr->job->RestoreWhere : "");
   }
   if (!response(fd, OKrestore, "Restore")) {
      restore_cleanup(jcr, JS_ErrorTerminated);
      return 0;
   }

   /* Wait for Job Termination */
   /*** ****FIXME**** get job termination status */
   Dmsg0(20, "wait for job termination\n");
   while (bget_msg(fd, 0) >  0) {
      Dmsg1(0, "dird<filed: %s\n", fd->msg);
   }

   restore_cleanup(jcr, JS_Terminated);

   return 1;
}

/*
 * Release resources allocated during restore.
 *
 */
static void restore_cleanup(JCR *jcr, int status) 
{
   char dt[MAX_TIME_LENGTH];

   Dmsg0(20, "In restore_cleanup\n");
   jcr->JobStatus = status;

   update_job_end_record(jcr);

   bstrftime(dt, sizeof(dt), jcr->jr.EndTime);
   Jmsg(jcr, M_INFO, 0, _("%s End Restore Job %s.\n\n"),
      dt, jcr->Job);

   Dmsg0(20, "Leaving restore_cleanup\n");
}

static int send_bootstrap_file(JCR *jcr)
{
   FILE *bs;
   char buf[1000];
   BSOCK *fd = jcr->file_bsock;
   char *bootstrap = "bootstrap\n";

   Dmsg1(400, "send_bootstrap_file: %s\n", jcr->RestoreBootstrap);
   if (!jcr->RestoreBootstrap) {
      return 1;
   }
   bs = fopen(jcr->RestoreBootstrap, "r");
   if (!bs) {
      Jmsg(jcr, M_FATAL, 0, _("Could not open bootstrap file %s: ERR=%s\n"), 
	 jcr->RestoreBootstrap, strerror(errno));
      jcr->JobStatus = JS_ErrorTerminated;
      return 0;
   }
   strcpy(fd->msg, bootstrap);	
   fd->msglen = strlen(fd->msg);
   bnet_send(fd);
   while (fgets(buf, sizeof(buf), bs)) {
      fd->msglen = Mmsg(&fd->msg, "%s", buf);
      bnet_send(fd);	   
   }
   bnet_sig(fd, BNET_EOF);
   fclose(bs);
   if (!response(fd, OKbootstrap, "Bootstrap")) {
      jcr->JobStatus = JS_ErrorTerminated;
      return 0;
   }
   return 1;
}
