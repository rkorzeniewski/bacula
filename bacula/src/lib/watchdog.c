/*
 * Bacula thread watchdog routine. General routine that monitors
 *  the daemon and signals a thread if it is blocked on a BSOCK
 *  too long. This prevents catastropic long waits -- generally
 *  due to Windows "hanging" the app.
 *
 *  Kern Sibbald, January MMII
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
#include "jcr.h"

/* Exported globals */
time_t watchdog_time;		      /* this has granularity of SLEEP_TIME */


#define TIMEOUT_SIGNAL SIGUSR2
#define SLEEP_TIME 30		      /* examine things every 30 seconds */

/* Forward referenced functions */
static void *watchdog_thread(void *arg);

/* Static globals */
static pthread_mutex_t mutex;
static pthread_cond_t  timer;
static int quit;


/*
 * Timeout signal comes here
 */
static void timeout_handler(int sig)
{
   return;			      /* thus interrupting the function */
}


/*   
 * Start watchdog thread
 *
 *  Returns: 0 on success
 *	     errno on failure
 */
int start_watchdog(void)
{
   int stat;
   pthread_t wdid;
   struct sigaction sigtimer;
			
   sigtimer.sa_flags = 0;
   sigtimer.sa_handler = timeout_handler;
   sigfillset(&sigtimer.sa_mask);
   sigaction(TIMEOUT_SIGNAL, &sigtimer, NULL);
   watchdog_time = time(NULL);
   if ((stat = pthread_mutex_init(&mutex, NULL)) != 0) {
      return stat;
   }
   if ((stat = pthread_cond_init(&timer, NULL)) != 0) {
      pthread_mutex_destroy(&mutex);
      return stat;
   }
   quit = FALSE;
   if ((stat = pthread_create(&wdid, NULL, watchdog_thread, (void *)NULL)) != 0) {
      pthread_mutex_destroy(&mutex);
      pthread_cond_destroy(&timer);
      return stat;
   }
   return 0;
}

/*
 * Terminate the watchdog thread
 *
 * Returns: 0 on success
 *	    errno on failure
 */
int stop_watchdog(void)
{
   int stat;

   if ((stat = pthread_mutex_lock(&mutex)) != 0) {
      return stat;
   }
   quit = TRUE;

   if ((stat = pthread_cond_signal(&timer)) != 0) {
      pthread_mutex_unlock(&mutex);
      return stat;
   }
   if ((stat = pthread_mutex_unlock(&mutex)) != 0) {
      return stat;
   }
   return 0;
}


/* 
 * This is the actual watchdog thread.
 */
static void *watchdog_thread(void *arg)
{
   struct timespec timeout;
   int stat;
   JCR *jcr;
   BSOCK *fd;

   Dmsg0(200, "Start watchdog thread\n");
   pthread_detach(pthread_self());

   if ((stat = pthread_mutex_lock(&mutex)) != 0) {
      return NULL;
   }

   for ( ;!quit; ) {
      struct timeval tv;
      struct timezone tz;
      time_t timer_start;

      Dmsg0(200, "Top of for loop\n");

      watchdog_time = time(NULL);     /* update timer */

      /* Walk through all JCRs checking if any one is 
       * blocked for more than specified max time.
       */
      lock_jcr_chain();
      for (jcr=NULL; (jcr=get_next_jcr(jcr)); ) {
	 free_locked_jcr(jcr);
	 if (jcr->JobId == 0) {
	    continue;
	 }
	 fd = jcr->store_bsock;
	 if (fd) {
	    timer_start = fd->timer_start;
	    if (timer_start && (watchdog_time - timer_start) > fd->timeout) {
	       fd->timed_out = TRUE;
	       Jmsg(jcr, M_ERROR, 0, _(
"Watchdog sending kill after %d secs to thread stalled reading Storage daemon.\n"),
		    watchdog_time - timer_start);
	       pthread_kill(jcr->my_thread_id, TIMEOUT_SIGNAL);
	    }
	 }
	 fd = jcr->file_bsock;
	 if (fd) {
	    timer_start = fd->timer_start;
	    if (timer_start && (watchdog_time - timer_start) > fd->timeout) {
	       fd->timed_out = TRUE;
	       Jmsg(jcr, M_ERROR, 0, _(
"Watchdog sending kill after %d secs to thread stalled reading File daemon.\n"),
		    watchdog_time - timer_start);
	       pthread_kill(jcr->my_thread_id, TIMEOUT_SIGNAL);
	    }
	 }
	 fd = jcr->dir_bsock;
	 if (fd) {
	    timer_start = fd->timer_start;
	    if (timer_start && (watchdog_time - timer_start) > fd->timeout) {
	       fd->timed_out = TRUE;
	       Jmsg(jcr, M_ERROR, 0, _(
"Watchdog sending kill after %d secs to thread stalled reading Director.\n"),
		    watchdog_time - timer_start);
	       pthread_kill(jcr->my_thread_id, TIMEOUT_SIGNAL);
	    }
	 }

      }
      unlock_jcr_chain();

      gettimeofday(&tv, &tz);
      timeout.tv_nsec = 0;
      timeout.tv_sec = tv.tv_sec + SLEEP_TIME;

      Dmsg1(200, "pthread_cond_timedwait sec=%d\n", timeout.tv_sec);
      stat = pthread_cond_timedwait(&timer, &mutex, &timeout);
      Dmsg1(200, "pthread_cond_timedwait stat=%d\n", stat);
      
   } /* end of big for loop */

   pthread_mutex_unlock(&mutex);      /* for good form */
   Dmsg0(200, "End watchdog\n");
   return NULL;
}
