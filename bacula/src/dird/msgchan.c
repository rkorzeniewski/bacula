/*
 *
 *   Bacula Director -- msgchan.c -- handles the message channel
 *    to the Storage daemon and the File daemon.
 *
 *     Kern Sibbald, August MM
 *
 *    This routine runs as a thread and must be thread reentrant.
 *
 *  Basic tasks done here:
 *    Open a message channel with the Storage daemon
 *	to authenticate ourself and to pass the JobId.
 *    Create a thread to interact with the Storage daemon
 *	who returns a job status and requests Catalog services, etc.
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
#include "dird.h"

/* Commands sent to Storage daemon */
static char jobcmd[]     = "JobId=%d job=%s job_name=%s client_name=%s Allow=%s Session=%s\n";
static char use_device[] = "use device=%s media_type=%s pool_name=%s pool_type=%s\n";

/* Response from Storage daemon */
static char OKjob[]      = "3000 OK Job SDid=%d SDtime=%d Authorization=%100s\n";
static char OK_device[]  = "3000 OK use device\n";

/* Storage Daemon requests */
static char Job_start[]  = "3010 Job %127s start\n";
static char Job_end[]	 = 
   "3099 Job %127s end JobStatus=%d JobFiles=%d JobBytes=%" lld "\n";
static char Job_status[] = "3012 Job %127s jobstatus %d\n";

/* Forward referenced functions */
static void *msg_thread(void *arg);

/*
 * Establish a message channel connection with the Storage daemon
 * and perform authentication. 
 */
int connect_to_storage_daemon(JCR *jcr, int retry_interval,    
			      int max_retry_time, int verbose)
{
   BSOCK *sd;

   /*
    *  Open message channel with the Storage daemon   
    */
   Dmsg2(200, "bnet_connect to Storage daemon %s:%d\n", jcr->store->address,
      jcr->store->SDport);
   sd = bnet_connect(jcr, retry_interval, max_retry_time,
          _("Storage daemon"), jcr->store->address, 
	  NULL, jcr->store->SDport, verbose);
   if (sd == NULL) {
      return 0;
   }
   sd->res = (RES *)jcr->store;        /* save pointer to other end */
   jcr->store_bsock = sd;

   if (!authenticate_storage_daemon(jcr)) {
      return 0;
   }
   return 1;
}

/*
 * Start a job with the Storage daemon
 */
int start_storage_daemon_job(JCR *jcr)
{
   int status;
   STORE *storage;
   BSOCK *sd;
   char auth_key[100];		      /* max 17 chars */
   char *device_name, *pool_name, *pool_type, *media_type;
   int device_name_len, pool_name_len, pool_type_len, media_type_len;

   storage = jcr->store;
   sd = jcr->store_bsock;
   /*
    * Now send JobId and permissions, and get back the authorization key.
    */
   bash_spaces(jcr->job->hdr.name);
   bash_spaces(jcr->client->hdr.name);
   bnet_fsend(sd, jobcmd, jcr->JobId, jcr->Job, jcr->job->hdr.name, 
              jcr->client->hdr.name, "append", "*");
   unbash_spaces(jcr->job->hdr.name);
   unbash_spaces(jcr->client->hdr.name);
   if (bnet_recv(sd) > 0) {
       Dmsg1(10, "<stored: %s", sd->msg);
       if (sscanf(sd->msg, OKjob, &jcr->VolSessionId, 
		  &jcr->VolSessionTime, &auth_key) != 3) {
          Jmsg(jcr, M_FATAL, 0, _("Storage daemon rejected Job command: %s\n"), sd->msg);
	  return 0;
       } else {
	  jcr->sd_auth_key = bstrdup(auth_key);
          Dmsg1(50, "sd_auth_key=%s\n", jcr->sd_auth_key);
       }
   } else {
      Jmsg(jcr, M_FATAL, 0, _("<stored: bad response to Job command: %s\n"),
	 bnet_strerror(sd));
      return 0;
   }

   /*
    * Send use device = xxx media = yyy pool = zzz
    */
   device_name_len = strlen(storage->dev_name) + 1;
   media_type_len = strlen(storage->media_type) + 1;
   pool_type_len = strlen(jcr->pool->pool_type) + 1;
   pool_name_len = strlen(jcr->pool->hdr.name) + 1;
   device_name = (char *) get_memory(device_name_len);
   pool_name = (char *) get_memory(pool_name_len);
   pool_type = (char *) get_memory(pool_type_len);
   media_type = (char *) get_memory(media_type_len);
   memcpy(device_name, storage->dev_name, device_name_len);
   memcpy(media_type, storage->media_type, media_type_len);
   memcpy(pool_type, jcr->pool->pool_type, pool_type_len);
   memcpy(pool_name, jcr->pool->hdr.name, pool_name_len);
   bash_spaces(device_name);
   bash_spaces(media_type);
   bash_spaces(pool_type);
   bash_spaces(pool_name);
   sd->msg = (char *) check_pool_memory_size(sd->msg, sizeof(device_name) +
      device_name_len + media_type_len + pool_type_len + pool_name_len);
   bnet_fsend(sd, use_device, device_name, media_type, pool_name, pool_type);
   Dmsg1(10, ">stored: %s", sd->msg);
   status = response(sd, OK_device, "Use Device");

   free_memory(device_name);
   free_memory(media_type);
   free_memory(pool_name);
   free_memory(pool_type);

   return status;
}

/* 
 * Start a thread to handle Storage daemon messages and
 *  Catalog requests.
 */
int start_storage_daemon_message_thread(JCR *jcr)
{
   int status;
   pthread_t thid;

   P(jcr->mutex);
   jcr->use_count++;		      /* mark in use by msg thread */
   V(jcr->mutex);
   set_thread_concurrency(4);
   if ((status=pthread_create(&thid, NULL, msg_thread, (void *)jcr)) != 0) {
      Emsg1(M_ABORT, 0, _("Cannot create message thread: %s\n"), strerror(status));
   }	     
   jcr->SD_msg_chan = thid;
   return 1;
}

static void msg_thread_cleanup(void *arg)
{
   JCR *jcr = (JCR *)arg;
   Dmsg0(200, "End msg_thread\n");
   P(jcr->mutex);
   jcr->msg_thread_done = TRUE;
   pthread_cond_broadcast(&jcr->term_wait); /* wakeup any waiting threads */
   V(jcr->mutex);
   free_jcr(jcr);		      /* release jcr */
}

/*
 * Handle the message channel (i.e. requests from the
 *  Storage daemon).
 * Note, we are running in a separate thread.
 */
static void *msg_thread(void *arg)
{
   JCR *jcr = (JCR *)arg;
   BSOCK *sd;
   int JobStatus;
   char Job[MAX_NAME_LENGTH];
   uint32_t JobFiles;
   uint64_t JobBytes;
   int stat;

   pthread_cleanup_push(msg_thread_cleanup, arg);
   Dmsg0(200, "msg_thread\n");
   sd = jcr->store_bsock;
   pthread_detach(pthread_self());

   /* Read the Storage daemon's output.
    */
   Dmsg0(200, "Start msg_thread loop\n");
   while ((stat=bget_msg(sd, 0)) > 0) {
      Dmsg1(200, "<stored: %s", sd->msg);
      if (sscanf(sd->msg, Job_start, &Job) == 1) {
	 continue;
      }
      if (sscanf(sd->msg, Job_end, &Job, &JobStatus, &JobFiles,
		 &JobBytes) == 4) {
	 jcr->SDJobStatus = JobStatus; /* termination status */
	 jcr->JobFiles = JobFiles;
	 jcr->JobBytes = JobBytes;
	 break;
      }     
      if (sscanf(sd->msg, Job_status, &Job, &JobStatus) == 2) {
	 jcr->SDJobStatus = JobStatus; /* current status */
	 continue;
      }
   }
   if (stat < 0) {		     
      jcr->SDJobStatus = JS_ErrorTerminated;
   }
   pthread_cleanup_pop(1);
   return NULL;
}
