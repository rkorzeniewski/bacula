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
static void *btimer_thread(void *arg);
static void stop_btimer(btimer_id wid);
static btimer_id btimer_start_common(uint32_t wait);

/* Static globals */
static pthread_mutex_t mutex;
static pthread_cond_t  timer;
static int quit;
static btimer_t *timer_chain = NULL;


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
   if ((stat = pthread_create(&wdid, NULL, btimer_thread, (void *)NULL)) != 0) {
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

   P(mutex);
   quit = TRUE;

   if ((stat = pthread_cond_signal(&timer)) != 0) {
      V(mutex);
      return stat;
   }
   V(mutex);
   return 0;
}


/* 
 * This is the actual watchdog thread.
 */
static void *btimer_thread(void *arg)
{
   struct timespec timeout;
   int stat;
   JCR *jcr;
   BSOCK *fd;
   btimer_t *wid;

   Dmsg0(200, "Start watchdog thread\n");
   pthread_detach(pthread_self());

   P(mutex);
   for ( ;!quit; ) {
      struct timeval tv;
      struct timezone tz;
      time_t timer_start, now;

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
      /* Note, this unlocks mutex during the sleep */
      stat = pthread_cond_timedwait(&timer, &mutex, &timeout);
      Dmsg1(200, "pthread_cond_timedwait stat=%d\n", stat);

      now = time(NULL);

      /* Walk child chain killing off any process overdue */
      for (wid = timer_chain; wid; wid=wid->next) {
	 int killed = FALSE;
	 /* First ask him politely to go away */
	 if (!wid->killed && now > (wid->start_time + wid->wait)) {
//          Dmsg1(000, "Watchdog sigterm pid=%d\n", wid->pid);
	    if (wid->type == TYPE_CHILD) {
	       kill(wid->pid, SIGTERM);
	       killed = TRUE;
	    } else {
               Dmsg1(200, "watchdog kill thread %d\n", wid->tid);
	       pthread_kill(wid->tid, TIMEOUT_SIGNAL);
	       wid->killed = TRUE;
	    }
	 }
	 /* If we asked a child to die, wait 3 seconds and slam him */
	 if (killed) {
	    btimer_t *wid1;
	    sleep(3);
	    for (wid1 = timer_chain; wid1; wid1=wid1->next) {
	       if (wid->type == TYPE_CHILD &&
		   !wid1->killed && now > (wid1->start_time + wid1->wait)) {
		  kill(wid1->pid, SIGKILL);
//                Dmsg1(000, "Watchdog killed pid=%d\n", wid->pid);
		  wid1->killed = TRUE;
	       }
	    }
	 }
      }
      
   } /* end of big for loop */

   V(mutex);
   Dmsg0(200, "End watchdog\n");
   return NULL;
}

/* 
 * Start a timer on a child process of pid, kill it after wait seconds.
 *   NOTE!  Granularity is SLEEP_TIME (i.e. 30 seconds)
 *
 *  Returns: btimer_id (pointer to btimer_t struct) on success
 *	     NULL on failure
 */
btimer_id start_child_timer(pid_t pid, uint32_t wait)
{
   btimer_t *wid;
   wid = btimer_start_common(wait);
   wid->pid = pid;
   wid->type = TYPE_CHILD;
   Dmsg2(200, "Start child timer 0x%x for %d secs.\n", wid, wait);
   return wid;
}

/* 
 * Start a timer on a thread. kill it after wait seconds.
 *   NOTE!  Granularity is SLEEP_TIME (i.e. 30 seconds)
 *
 *  Returns: btimer_id (pointer to btimer_t struct) on success
 *	     NULL on failure
 */
btimer_id start_thread_timer(pthread_t tid, uint32_t wait)
{
   btimer_t *wid;
   wid = btimer_start_common(wait);
   wid->tid = tid;
   wid->type = TYPE_PTHREAD;
   Dmsg2(200, "Start thread timer 0x%x for %d secs.\n", wid, wait);
   return wid;
}

static btimer_id btimer_start_common(uint32_t wait)
{
   btimer_id wid = (btimer_id)malloc(sizeof(btimer_t));

   P(mutex);
   /* Chain it into timer_chain as the first item */
   wid->prev = NULL;
   wid->next = timer_chain;
   if (timer_chain) {
      timer_chain->prev = wid;
   }
   timer_chain = wid;
   wid->start_time = time(NULL);
   wid->wait = wait;
   wid->killed = FALSE;
   V(mutex);
   return wid;
}

/*
 * Stop child timer
 */
void stop_child_timer(btimer_id wid)
{
   Dmsg2(200, "Stop child timer 0x%x for %d secs.\n", wid, wid->wait);
   stop_btimer(wid);	     
}

/*
 * Stop thread timer
 */
void stop_thread_timer(btimer_id wid)
{
   Dmsg2(200, "Stop thread timer 0x%x for %d secs.\n", wid, wid->wait);
   stop_btimer(wid);	     
}


/*
 * Stop btimer
 */
static void stop_btimer(btimer_id wid)
{
   if (wid == NULL) {
      Emsg0(M_ABORT, 0, _("NULL btimer_id.\n"));
   }
   P(mutex);
   /* Remove wid from timer_chain */
   if (!wid->prev) {		      /* if no prev */
      timer_chain = wid->next;	      /* set new head */
   } else {
      wid->prev->next = wid->next;    /* update prev */
   }
   if (wid->next) {
      wid->next->prev = wid->prev;    /* unlink it */
   }
   V(mutex);
   free(wid);
}
