/*
 *   Job control and execution for Storage Daemon
 *
 *   Version $Id$
 *
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
#include "stored.h"

/* Imported variables */
extern uint32_t VolSessionTime;

/* Imported functions */
extern uint32_t newVolSessionId();

/* Forward referenced functions */
static int use_device_cmd(JCR *jcr);

/* Requests from the Director daemon */
static char jobcmd[]     = "JobId=%d job=%127s job_name=%127s client_name=%127s \
type=%d level=%d FileSet=%127s Allow=";
static char use_device[] = "use device=%s media_type=%s pool_name=%s pool_type=%s\n";

/* Responses sent to Director daemon */
static char OKjob[]     = "3000 OK Job SDid=%u SDtime=%u Authorization=%s\n";
static char OK_device[] = "3000 OK use device\n";
static char NO_device[] = "3904 Device %s not available\n";
static char BAD_use[]   = "3903 Bad use command: %s\n";
static char BAD_job[]   = "3905 Bad Job command: %s\n";



/*
 * Director requests us to start a job
 * Basic tasks done here:
 *  - We pickup the JobId to be run from the Director.
 *  - We pickup the device, media, and pool from the Director
 *  - Wait for a connection from the File Daemon (FD)
 *  - Accept commands from the FD (i.e. run the job)
 *  - Return when the connection is terminated or
 *    there is an error.
 */
int job_cmd(JCR *jcr)
{
   int JobId, errstat;
   char auth_key[100];
   BSOCK *dir = jcr->dir_bsock;
   POOLMEM *job_name, *client_name, *job, *fileset_name;
   int JobType, level;
   struct timeval tv;
   struct timezone tz;
   struct timespec timeout;

   /*
    * Get JobId and permissions from Director
    */

   Dmsg1(30, "Job_cmd: %s\n", dir->msg);
   job = get_memory(dir->msglen);
   job_name = get_memory(dir->msglen);
   client_name = get_memory(dir->msglen);
   fileset_name = get_memory(dir->msglen);
   if (sscanf(dir->msg, jobcmd, &JobId, job, job_name, client_name,
	      &JobType, &level, fileset_name) != 7) {
      bnet_fsend(dir, BAD_job, dir->msg);
      Emsg1(M_FATAL, 0, _("Bad Job Command from Director: %s\n"), dir->msg);
      free_memory(job);
      free_memory(job_name);
      free_memory(client_name);
      free_memory(fileset_name);
      jcr->JobStatus = JS_ErrorTerminated;
      return 0;
   }
   jcr->JobId = JobId;
   jcr->VolSessionId = newVolSessionId();
   jcr->VolSessionTime = VolSessionTime;
   strcpy(jcr->Job, job);
   unbash_spaces(job_name);
   jcr->job_name = get_memory(strlen(job_name) + 1);
   strcpy(jcr->job_name, job_name);
   unbash_spaces(client_name);
   jcr->client_name = get_memory(strlen(client_name) + 1);
   strcpy(jcr->client_name, client_name);
   unbash_spaces(fileset_name);
   jcr->fileset_name = get_memory(strlen(fileset_name) + 1);
   strcpy(jcr->fileset_name, fileset_name);
   jcr->JobType = JobType;
   jcr->JobLevel = level;
   free_memory(job);
   free_memory(job_name);
   free_memory(client_name);
   free_memory(fileset_name);

   /* Initialize FD start condition variable */
   if ((errstat = pthread_cond_init(&jcr->job_start_wait, NULL)) != 0) {
      Emsg1(M_FATAL, 0, _("Unable to init job cond variable: ERR=%s\n"), strerror(errstat));
      jcr->JobStatus = JS_ErrorTerminated;
      return 0;
   }
   jcr->authenticated = FALSE;

   /*
    * Pass back an authorization key for the File daemon
    */
   gettimeofday(&tv, &tz);
   srandom(tv.tv_usec + tv.tv_sec);
   sprintf(auth_key, "%ld", (long)random());
   bnet_fsend(dir, OKjob, jcr->VolSessionId, jcr->VolSessionTime, auth_key);
   Dmsg1(10, ">dird: %s", dir->msg);
   jcr->sd_auth_key = bstrdup(auth_key);

   /*
    * Wait for the device, media, and pool information
    */
   if (!use_device_cmd(jcr)) {
      jcr->JobStatus = JS_ErrorTerminated;
      return 0;
   }

   jcr->JobStatus = JS_WaitFD;	      /* wait for FD to connect */
   dir_send_job_status(jcr);

   gettimeofday(&tv, &tz);
   timeout.tv_nsec = tv.tv_usec * 1000; 
   timeout.tv_sec = tv.tv_sec + 30 * 60;	/* wait 30 minutes */


   Dmsg1(200, "%s waiting on job_start_wait\n", jcr->Job);
   P(jcr->mutex);
   for ( ;!job_cancelled(jcr); ) {
      errstat = pthread_cond_timedwait(&jcr->job_start_wait, &jcr->mutex, &timeout);
      if (errstat == 0 || errstat == ETIMEDOUT) {
	 break;
      }
   }
   V(jcr->mutex);

   if (jcr->authenticated && !job_cancelled(jcr)) {
      run_job(jcr);		      /* Run the job */
   }
   return 0;
}

/*
 * This entry point is only called if we have a separate
 *   Storage Daemon Data port. Otherwise, the connection
 *   is made to the main port, and if it is a File daemon
 *   calling, handl_filed_connection() is called directly.
 */
void connection_from_filed(void *arg)
{
   BSOCK *fd = (BSOCK *)arg;
   char job_name[MAX_NAME_LENGTH];

   Dmsg0(200, "enter connection_from_filed\n");
   if (bnet_recv(fd) <= 0) {
      Emsg0(M_FATAL, 0, _("Unable to authenticate Client.\n"));
      return;
   }
   Dmsg1(100, "got: %s\n", fd->msg);

   if (sscanf(fd->msg, "Hello Start Job %127s\n", job_name) != 1) {
      Emsg1(M_FATAL, 0, _("Authentication failure: %s\n"), fd->msg);
      return;
   }
   handle_filed_connection(fd, job_name);
   return;
}

void handle_filed_connection(BSOCK *fd, char *job_name)
{
   JCR *jcr;

   if (!(jcr=get_jcr_by_full_name(job_name))) {
      Emsg1(M_FATAL, 0, _("Job name not found: %s\n"), job_name);
      return;
   }

   jcr->file_bsock = fd;

   Dmsg1(100, "Found Job %s\n", job_name);

   if (jcr->authenticated) {
      Pmsg2(000, "Hey!!!! JobId %d Job %s already authenticated.\n", 
	 jcr->JobId, jcr->Job);
   }
  
   /*
    * Authenticate the File daemon
    */
   if (jcr->authenticated || !authenticate_filed(jcr)) {
      Dmsg1(100, "Authentication failed Job %s\n", jcr->Job);
      Jmsg(jcr, M_FATAL, 0, _("Unable to authenticate File daemon\n"));
   } else {
      jcr->authenticated = TRUE;
      Dmsg1(100, "OK Authentication Job %s\n", jcr->Job);
   }

   P(jcr->mutex);
   if (!jcr->authenticated) {
      jcr->JobStatus = JS_ErrorTerminated;
   }
   pthread_cond_signal(&jcr->job_start_wait); /* wake waiting job */
   V(jcr->mutex);
   free_jcr(jcr);
   return;
}


/*  
 *   Use Device command from Director
 *   He tells is what Device Name to use, the Media Type, 
 *	the Pool Name, and the Pool Type.
 *
 *    Ensure that the device exists and is opened, then store
 *	the media and pool info in the JCR.
 */
static int use_device_cmd(JCR *jcr)
{
   POOLMEM *dev_name, *media_type, *pool_name, *pool_type;
   BSOCK *dir = jcr->dir_bsock;
   DEVRES *device;

   if (bnet_recv(dir) <= 0) {
      Emsg0(M_FATAL, 0, "No Device from Director\n");
      return 0;
   }
   
   Dmsg1(20, "Use device: %s", dir->msg);
   dev_name = get_memory(dir->msglen);
   media_type = get_memory(dir->msglen);
   pool_name = get_memory(dir->msglen);
   pool_type = get_memory(dir->msglen);
   if (sscanf(dir->msg, use_device, dev_name, media_type, pool_name, pool_type) == 4) {
      unbash_spaces(dev_name);
      unbash_spaces(media_type);
      unbash_spaces(pool_name);
      unbash_spaces(pool_type);
      device = NULL;
      LockRes();
      while ((device=(DEVRES *)GetNextRes(R_DEVICE, (RES *)device))) {
	 /* Find resource, and make sure we were able to open it */
	 if (strcmp(device->hdr.name, dev_name) == 0 && device->dev) {
            Dmsg1(20, "Found device %s\n", device->hdr.name);
	    jcr->pool_name = get_memory(strlen(pool_name) + 1);
	    strcpy(jcr->pool_name, pool_name);
	    jcr->pool_type = get_memory(strlen(pool_type) + 1);
	    strcpy(jcr->pool_type, pool_type);
	    jcr->media_type = get_memory(strlen(media_type) + 1);
	    strcpy(jcr->media_type, media_type);
	    jcr->dev_name = get_memory(strlen(dev_name) + 1);
	    strcpy(jcr->dev_name, dev_name);
	    jcr->device = device;
	    Dmsg4(20, use_device, dev_name, media_type, pool_name, pool_type);
	    free_memory(dev_name);
	    free_memory(media_type);
	    free_memory(pool_name);
	    free_memory(pool_type);
	    UnlockRes();
	    return bnet_fsend(dir, OK_device);
	 }
      }
      UnlockRes();
      Jmsg(jcr, M_FATAL, 0, _("Requested device %s not found. Cannot continue.\n"),
	   dev_name);
      bnet_fsend(dir, NO_device, dev_name);
   } else {
      Jmsg(jcr, M_FATAL, 0, _("store<dir: Bad Use Device command: %s\n"), dir->msg);
      bnet_fsend(dir, BAD_use, dir->msg);
   }

   free_memory(dev_name);
   free_memory(media_type);
   free_memory(pool_name);
   free_memory(pool_type);
   return 0;			      /* ERROR return */
}

/* 
 * Destroy the Job Control Record and associated
 * resources (sockets).
 */
void stored_free_jcr(JCR *jcr) 
{
   if (jcr->file_bsock) {
      bnet_close(jcr->file_bsock);
   }
   if (jcr->pool_name) {
      free_memory(jcr->pool_name);
   }
   if (jcr->pool_type) {
      free_memory(jcr->pool_type);
   }
   if (jcr->media_type) {
      free_memory(jcr->media_type);
   }
   if (jcr->dev_name) {
      free_memory(jcr->dev_name);
   }
   if (jcr->job_name) {
      free_pool_memory(jcr->job_name);
   }
   if (jcr->client_name) {
      free_memory(jcr->client_name);
      jcr->client_name = NULL;
   }
   if (jcr->fileset_name) {
      free_memory(jcr->fileset_name);
   }
   if (jcr->bsr) {
      free_bsr(jcr->bsr);
   }
   if (jcr->RestoreBootstrap) {
      unlink(jcr->RestoreBootstrap);
      free_pool_memory(jcr->RestoreBootstrap);
      jcr->RestoreBootstrap = NULL;
   }
		      
   return;
}
