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
static void wd_lock();
static void wd_unlock();

/* Static globals */
static bool quit = false;;
static bool wd_is_init = false;
static brwlock_t lock;		      /* watchdog lock */

static pthread_t wd_tid;
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
   int errstat;

   if (wd_is_init) {
      return 0;
   }
   Dmsg0(200, "Initialising NicB-hacked watchdog thread\n");
   watchdog_time = time(NULL);

   if ((errstat=rwl_init(&lock)) != 0) {
      Emsg1(M_ABORT, 0, _("Unable to initialize watchdog lock. ERR=%s\n"), 
	    strerror(errstat));
   }
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

   quit = true; 		      /* notify watchdog thread to stop */
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
   rwl_destroy(&lock);

   return stat;
}

watchdog_t *new_watchdog(void)
{
   watchdog_t *wd = (watchdog_t *)malloc(sizeof(watchdog_t));

   if (!wd_is_init) {
      start_watchdog();
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

   wd_lock();
   wd->next_fire = watchdog_time + wd->interval;
   wd_queue->append(wd);
   Dmsg3(200, "Registered watchdog %p, interval %d%s\n",
         wd, wd->interval, wd->one_shot ? " one shot" : "");
   wd_unlock();

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

   wd_lock();
   ret = unregister_watchdog_unlocked(wd);
   wd_unlock();

   return ret;
}

static void *watchdog_thread(void *arg)
{
   Dmsg0(200, "NicB-reworked watchdog thread entered\n");

   while (!quit) {
      watchdog_t *p;

      /* 
       * We lock the jcr chain here because a good number of the
       *   callback routines lock the jcr chain. We need to lock
       *   it here *before* the watchdog lock because the SD message
       *   thread first locks the jcr chain, then when closing the
       *   job locks the watchdog chain. If the two thread do not
       *   lock in the same order, we get a deadlock -- each holds
       *   the other's needed lock.
       */
      lock_jcr_chain();
      wd_lock();
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
      wd_unlock();
      unlock_jcr_chain();
      bmicrosleep(SLEEP_TIME, 0);
   }

   Dmsg0(200, "NicB-reworked watchdog thread exited\n");
   return NULL;
}

/*
 * Watchdog lock, this can be called multiple times by the same
 *   thread without blocking, but must be unlocked the number of
 *   times it was locked.
 */
static void wd_lock()
{
   int errstat;
   if ((errstat=rwl_writelock(&lock)) != 0) {
      Emsg1(M_ABORT, 0, "rwl_writelock failure. ERR=%s\n",
	   strerror(errstat));
   }
}    

/*
 * Unlock the watchdog. This can be called multiple times by the
 *   same thread up to the number of times that thread called
 *   wd_ lock()/
 */
static void wd_unlock()
{
   int errstat;
   if ((errstat=rwl_writeunlock(&lock)) != 0) {
      Emsg1(M_ABORT, 0, "rwl_writeunlock failure. ERR=%s\n",
	   strerror(errstat));
   }
}    
