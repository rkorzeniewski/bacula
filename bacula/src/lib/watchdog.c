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
   Copyright (C) 2000-2004 Kern Sibbald and John Walker

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
time_t watchdog_time = 0;	      /* this has granularity of SLEEP_TIME */

#define SLEEP_TIME 1		      /* examine things every second */

/* Forward referenced functions */
static void *watchdog_thread(void *arg);

/* Static globals */
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  timer = PTHREAD_COND_INITIALIZER;
static bool quit;
static bool wd_is_init = false;

/* Forward referenced callback functions */
static pthread_t wd_tid;

/* Static globals */
static dlist *wd_queue;
static dlist *wd_inactive;

/*   
 * Start watchdog thread
 *
 *  Returns: 0 on success
 *	     errno on failure
 */
int start_watchdog(void)
{
   int stat;
   watchdog_t *dummy = NULL;

   if (wd_is_init) {
      return 0;
   }
   Dmsg0(200, "Initialising NicB-hacked watchdog thread\n");
   watchdog_time = time(NULL);
   quit = false;

   wd_queue = new dlist(wd_queue, &dummy->link);
   wd_inactive = new dlist(wd_inactive, &dummy->link);

   if ((stat = pthread_create(&wd_tid, NULL, watchdog_thread, NULL)) != 0) {
      return stat;
   }
   wd_is_init = true;
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
   watchdog_t *p;    

   if (!wd_is_init) {
      return 0;
   }

   Dmsg0(200, "Sending stop signal to NicB-hacked watchdog thread\n");
   P(mutex);
   quit = true;
   stat = pthread_cond_signal(&timer);
   V(mutex);

   wd_is_init = false;

   stat = pthread_join(wd_tid, NULL);

   foreach_dlist(p, wd_queue) {
      if (p->destructor != NULL) {
	 p->destructor(p);
      }
      free(p);
   }
   delete wd_queue;
   wd_queue = NULL;

   foreach_dlist(p, wd_inactive) {
      if (p->destructor != NULL) {
	 p->destructor(p);
      }
      free(p);
   }
   delete wd_inactive;
   wd_inactive = NULL;

   return stat;
}

watchdog_t *watchdog_new(void)
{
   watchdog_t *wd = (watchdog_t *) malloc(sizeof(watchdog_t));

   if (!wd_is_init) {
      Emsg0(M_ABORT, 0, "BUG! watchdog_new called before start_watchdog\n");
   }

   if (wd == NULL) {
      return NULL;
   }
   wd->one_shot = true;
   wd->interval = 0;
   wd->callback = NULL;
   wd->destructor = NULL;
   wd->data = NULL;

   return wd;
}

bool register_watchdog(watchdog_t *wd)
{
   if (!wd_is_init) {
      Emsg0(M_ABORT, 0, "BUG! register_watchdog called before start_watchdog\n");
   }
   if (wd->callback == NULL) {
      Emsg1(M_ABORT, 0, "BUG! Watchdog %p has NULL callback\n", wd);
   }
   if (wd->interval == 0) {
      Emsg1(M_ABORT, 0, "BUG! Watchdog %p has zero interval\n", wd);
   }

   P(mutex);
   wd->next_fire = watchdog_time + wd->interval;
   wd_queue->append(wd);
   Dmsg3(200, "Registered watchdog %p, interval %d%s\n",
         wd, wd->interval, wd->one_shot ? " one shot" : "");
   V(mutex);

   return false;
}

bool unregister_watchdog_unlocked(watchdog_t *wd)
{
   watchdog_t *p;

   if (!wd_is_init) {
      Emsg0(M_ABORT, 0, "BUG! unregister_watchdog_unlocked called before start_watchdog\n");
   }

   foreach_dlist(p, wd_queue) {
      if (wd == p) {
	 wd_queue->remove(wd);
         Dmsg1(200, "Unregistered watchdog %p\n", wd);
	 return true;
      }
   }

   foreach_dlist(p, wd_inactive) {
      if (wd == p) {
	 wd_inactive->remove(wd);
         Dmsg1(200, "Unregistered inactive watchdog %p\n", wd);
	 return true;
      }
   }

   Dmsg1(200, "Failed to unregister watchdog %p\n", wd);
   return false;
}

bool unregister_watchdog(watchdog_t *wd)
{
   bool ret;

   if (!wd_is_init) {
      Emsg0(M_ABORT, 0, "BUG! unregister_watchdog called before start_watchdog\n");
   }

   P(mutex);
   ret = unregister_watchdog_unlocked(wd);
   V(mutex);

   return ret;
}

static void *watchdog_thread(void *arg)
{
   Dmsg0(200, "NicB-reworked watchdog thread entered\n");

   while (true) {
      watchdog_t *p;

      P(mutex);
      if (quit) {
	 V(mutex);
	 break;
      }

      watchdog_time = time(NULL);

      foreach_dlist(p, wd_queue) {
	 if (p->next_fire < watchdog_time) {
	    /* Run the callback */
	    p->callback(p);

            /* Reschedule (or move to inactive list if it's a one-shot timer) */
	    if (p->one_shot) {
	       wd_queue->remove(p);
	       wd_inactive->append(p);
	    } else {
	       p->next_fire = watchdog_time + p->interval;
	    }
	 }
      }
      V(mutex);
      bmicrosleep(SLEEP_TIME, 0);
   }

   Dmsg0(200, "NicB-reworked watchdog thread exited\n");
   return NULL;
}
