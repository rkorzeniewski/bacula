/*
 * Manipulation routines for Job Control Records
 *
 *  Kern E. Sibbald, December 2000
 *
 *  Version $Id$
 *
 *  These routines are thread safe.
 *
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
#include "jcr.h"

struct s_last_job last_job;	      /* last job run by this daemon */

static JCR *jobs = NULL;	      /* pointer to JCR chain */
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

/*
 * Create a Job Control Record and link it into JCR chain
 * Returns newly allocated JCR
 * Note, since each daemon has a different JCR, he passes
 *  us the size.
 */
JCR *new_jcr(int size, JCR_free_HANDLER *daemon_free_jcr)
{
   JCR *jcr;

   Dmsg0(200, "Enter new_jcr\n");
   jcr = (JCR *) malloc(size);
   memset(jcr, 0, size);
   jcr->my_thread_id = pthread_self();
   jcr->sched_time = time(NULL);
   jcr->daemon_free_jcr = daemon_free_jcr;    /* plug daemon free routine */
   jcr->use_count = 1;
   pthread_mutex_init(&(jcr->mutex), NULL);
   jcr->JobStatus = JS_Created;       /* ready to run */
   jcr->VolumeName = get_pool_memory(PM_FNAME);
   jcr->VolumeName[0] = 0;
   jcr->errmsg = get_pool_memory(PM_MESSAGE);
   jcr->errmsg[0] = 0;

   P(mutex);
   jcr->prev = NULL;
   jcr->next = jobs;
   if (jobs) {
      jobs->prev = jcr;
   }
   jobs = jcr;
   V(mutex);
   return jcr;
}


/*
 * Remove a JCR from the chain
 * NOTE! The chain must be locked prior to calling
 *	 this routine.
 */
static void remove_jcr(JCR *jcr)
{
   Dmsg0(150, "Enter remove_jcr\n");
   if (!jcr) {
      Emsg0(M_ABORT, 0, "NULL jcr.\n");
   }
   if (!jcr->prev) {		      /* if no prev */
      jobs = jcr->next; 	      /* set new head */
   } else {
      jcr->prev->next = jcr->next;    /* update prev */
   }
   if (jcr->next) {
      jcr->next->prev = jcr->prev;
   }
   Dmsg0(150, "Leave remove_jcr\n");
}

/*
 * Free stuff common to all JCRs
 */
static void free_common_jcr(JCR *jcr)
{
   /* Keep some statistics */
   switch (jcr->JobType) {
      case JT_BACKUP:
      case JT_VERIFY:
      case JT_RESTORE:
	 last_job.NumJobs++;
	 last_job.JobType = jcr->JobType;
	 last_job.JobId = jcr->JobId;
	 last_job.VolSessionId = jcr->VolSessionId;
	 last_job.VolSessionTime = jcr->VolSessionTime;
	 strcpy(last_job.Job, jcr->Job);
	 last_job.JobFiles = jcr->JobFiles;
	 last_job.JobBytes = jcr->JobBytes;
	 last_job.JobStatus = jcr->JobStatus;
	 last_job.start_time = jcr->start_time;
	 last_job.end_time = time(NULL);
	 break;
      default:
	 break;
   }
   pthread_mutex_destroy(&jcr->mutex);

   close_msg(jcr);		      /* close messages for this job */

   /* do this after closing messages */
   if (jcr->client_name) {
      free_pool_memory(jcr->client_name);
      jcr->client_name = NULL;
   }

   if (jcr->sd_auth_key) {
      Dmsg0(200, "Free JCR sd_auth_key\n");
      free(jcr->sd_auth_key);
      jcr->sd_auth_key = NULL;
   }
   if (jcr->VolumeName) {
      free_pool_memory(jcr->VolumeName);
      jcr->VolumeName = NULL;
   }

   if (jcr->dir_bsock) {
      bnet_close(jcr->dir_bsock);
      jcr->dir_bsock = NULL;
   }
   if (jcr->errmsg) {
      free_pool_memory(jcr->errmsg);
      jcr->errmsg = NULL;
   }
   free(jcr);
}

/* 
 * Global routine to free a jcr
 */
#ifdef DEBUG
void b_free_jcr(char *file, int line, JCR *jcr)
{
   Dmsg3(200, "Enter free_jcr 0x%x from %s:%d\n", jcr, file, line);

#else

void free_jcr(JCR *jcr)
{
   Dmsg1(200, "Enter free_jcr 0x%x\n", jcr);

#endif

   P(mutex);
   jcr->use_count--;		      /* decrement use count */
   Dmsg2(200, "Decrement jcr 0x%x use_count=%d\n", jcr, jcr->use_count);
   if (jcr->use_count > 0) {	      /* if in use */
      V(mutex);
      Dmsg2(200, "jcr 0x%x use_count=%d\n", jcr, jcr->use_count);
      return;
   }
   remove_jcr(jcr);
   V(mutex);

   if (jcr->daemon_free_jcr) {
      jcr->daemon_free_jcr(jcr);      /* call daemon free routine */
   }
   free_common_jcr(jcr);
   Dmsg0(200, "Exit free_jcr\n");
}


/* 
 * Global routine to free a jcr
 *  JCR chain is already locked
 */
void free_locked_jcr(JCR *jcr)
{
   jcr->use_count--;		      /* decrement use count */
   Dmsg2(200, "Decrement jcr 0x%x use_count=%d\n", jcr, jcr->use_count);
   if (jcr->use_count > 0) {	      /* if in use */
      return;
   }
   remove_jcr(jcr);
   jcr->daemon_free_jcr(jcr);	      /* call daemon free routine */
   free_common_jcr(jcr);
}




/*
 * Given a JobId, find the JCR	    
 *   Returns: jcr on success
 *	      NULL on failure
 */
JCR *get_jcr_by_id(uint32_t JobId)
{
   JCR *jcr;	   

   P(mutex);
   for (jcr = jobs; jcr; jcr=jcr->next) {
      if (jcr->JobId == JobId) {
	 jcr->use_count++;
         Dmsg2(200, "Increment jcr 0x%x use_count=%d\n", jcr, jcr->use_count);
	 break;
      }
   }
   V(mutex);
   return jcr; 
}

/*
 * Given a SessionId and SessionTime, find the JCR	
 *   Returns: jcr on success
 *	      NULL on failure
 */
JCR *get_jcr_by_session(uint32_t SessionId, uint32_t SessionTime)
{
   JCR *jcr;	   

   P(mutex);
   for (jcr = jobs; jcr; jcr=jcr->next) {
      if (jcr->VolSessionId == SessionId && 
	  jcr->VolSessionTime == SessionTime) {
	 jcr->use_count++;
         Dmsg2(200, "Increment jcr 0x%x use_count=%d\n", jcr, jcr->use_count);
	 break;
      }
   }
   V(mutex);
   return jcr; 
}


/*
 * Given a Job, find the JCR	  
 *  compares on the number of characters in Job
 *  thus allowing partial matches.
 *   Returns: jcr on success
 *	      NULL on failure
 */
JCR *get_jcr_by_partial_name(char *Job)
{
   JCR *jcr;	   
   int len;

   P(mutex);
   len = strlen(Job);
   for (jcr = jobs; jcr; jcr=jcr->next) {
      if (strncmp(Job, jcr->Job, len) == 0) {
	 jcr->use_count++;
         Dmsg2(200, "Increment jcr 0x%x use_count=%d\n", jcr, jcr->use_count);
	 break;
      }
   }
   V(mutex);
   return jcr; 
}


/*
 * Given a Job, find the JCR	  
 *  requires an exact match of names.
 *   Returns: jcr on success
 *	      NULL on failure
 */
JCR *get_jcr_by_full_name(char *Job)
{
   JCR *jcr;	   

   P(mutex);
   for (jcr = jobs; jcr; jcr=jcr->next) {
      if (strcmp(jcr->Job, Job) == 0) {
	 jcr->use_count++;
         Dmsg2(200, "Increment jcr 0x%x use_count=%d\n", jcr, jcr->use_count);
	 break;
      }
   }
   V(mutex);
   return jcr; 
}

void set_jcr_job_status(JCR *jcr, int JobStatus)
{
   /*
    * For a set of errors, ... keep the current status
    *   so it isn't lost. For all others, set it.
    */
   switch (jcr->JobStatus) {
   case JS_ErrorTerminated:
   case JS_Error:
   case JS_FatalError:
   case JS_Differences:
   case JS_Cancelled:
      break;
   default:
      jcr->JobStatus = JobStatus;
   }
}

/* 
 * Lock the chain
 */
void lock_jcr_chain()
{
   P(mutex);
}

/*
 * Unlock the chain
 */
void unlock_jcr_chain()
{
   V(mutex);
}


JCR *get_next_jcr(JCR *jcr)
{
   JCR *rjcr;

   if (jcr == NULL) {
      rjcr = jobs;
   } else {
      rjcr = jcr->next;
   }
   if (rjcr) {
      rjcr->use_count++;
      Dmsg1(200, "Increment jcr use_count=%d\n", rjcr->use_count);
   }
   return rjcr;
}
