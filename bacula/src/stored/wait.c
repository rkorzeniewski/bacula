/*
 *  Subroutines to handle waiting for operator intervention
 *   or waiting for a Device to be released
 *
 *  Code for wait_for_sysop() pulled from askdir.c
 *
 *   Kern Sibbald, March 2005
 *
 *   Version $Id$
 */
/*
   Copyright (C) 2000-2005 Kern Sibbald

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

#include "bacula.h"                   /* pull in global headers */
#include "stored.h"                   /* pull in Storage Deamon headers */

static bool double_jcr_wait_time(JCR *jcr);


/*
 * Wait for SysOp to mount a tape on a specific device
 *
 *   Returns: status from pthread_cond_timedwait() 
 */
int wait_for_sysop(DCR *dcr)
{
   struct timeval tv;
   struct timezone tz;
   struct timespec timeout;
   time_t last_heartbeat = 0;
   time_t first_start = time(NULL);
   int stat = 0;
   int add_wait;
   bool unmounted;
   DEVICE *dev = dcr->dev;
   JCR *jcr = dcr->jcr;

   P(dev->mutex);
   unmounted = (dev->dev_blocked == BST_UNMOUNTED) ||
		(dev->dev_blocked == BST_UNMOUNTED_WAITING_FOR_SYSOP);

   dev->poll = false;
   /*
    * Wait requested time (dev->rem_wait_sec).	However, we also wake up every
    *	 HB_TIME seconds and send a heartbeat to the FD and the Director
    *	 to keep stateful firewalls from closing them down while waiting
    *	 for the operator.
    */
   add_wait = dev->rem_wait_sec;
   if (me->heartbeat_interval && add_wait > me->heartbeat_interval) {
      add_wait = me->heartbeat_interval;
   }
   /* If the user did not unmount the tape and we are polling, ensure
    *  that we poll at the correct interval.
    */
   if (!unmounted && dev->vol_poll_interval && add_wait > dev->vol_poll_interval) {
      add_wait = dev->vol_poll_interval;
   }

   if (!unmounted) {
      dev->dev_prev_blocked = dev->dev_blocked;
      dev->dev_blocked = BST_WAITING_FOR_SYSOP; /* indicate waiting for mount */
   }

   for ( ; !job_canceled(jcr); ) {
      time_t now, start;

      gettimeofday(&tv, &tz);
      timeout.tv_nsec = tv.tv_usec * 1000;
      timeout.tv_sec = tv.tv_sec + add_wait;

      Dmsg3(400, "I'm going to sleep on device %s. HB=%d wait=%d\n", dev->print_name(),
	 (int)me->heartbeat_interval, dev->wait_sec);
      start = time(NULL);
      /* Wait required time */
      stat = pthread_cond_timedwait(&dev->wait_next_vol, &dev->mutex, &timeout);
      Dmsg1(400, "Wokeup from sleep on device stat=%d\n", stat);

      now = time(NULL);
      dev->rem_wait_sec -= (now - start);

      /* Note, this always triggers the first time. We want that. */
      if (me->heartbeat_interval) {
	 if (now - last_heartbeat >= me->heartbeat_interval) {
	    /* send heartbeats */
	    if (jcr->file_bsock) {
	       bnet_sig(jcr->file_bsock, BNET_HEARTBEAT);
               Dmsg0(400, "Send heartbeat to FD.\n");
	    }
	    if (jcr->dir_bsock) {
	       bnet_sig(jcr->dir_bsock, BNET_HEARTBEAT);
	    }
	    last_heartbeat = now;
	 }
      }

      /*
       * Check if user unmounted the device while we were waiting
       */
      unmounted = (dev->dev_blocked == BST_UNMOUNTED) ||
		   (dev->dev_blocked == BST_UNMOUNTED_WAITING_FOR_SYSOP);

      if (stat != ETIMEDOUT) {	   /* we blocked the device */
	 break; 		   /* on error return */
      }
      if (dev->rem_wait_sec <= 0) {  /* on exceeding wait time return */
         Dmsg0(400, "Exceed wait time.\n");
	 break;
      }

      if (!unmounted && dev->vol_poll_interval &&
	  (now - first_start >= dev->vol_poll_interval)) {
         Dmsg1(400, "In wait blocked=%s\n", edit_blocked_reason(dev));
	 dev->poll = true;	      /* returning a poll event */
	 break;
      }
      /*
       * Check if user mounted the device while we were waiting
       */
      if (dev->dev_blocked == BST_MOUNT) {   /* mount request ? */
	 stat = 0;
	 break;
      }

      add_wait = dev->wait_sec - (now - start);
      if (add_wait < 0) {
	 add_wait = 0;
      }
      if (me->heartbeat_interval && add_wait > me->heartbeat_interval) {
	 add_wait = me->heartbeat_interval;
      }
   }

   if (!unmounted) {
      dev->dev_blocked = dev->dev_prev_blocked;    /* restore entry state */
   }
   V(dev->mutex);
   return stat;
}


/*
 * Wait for Device to be released
 *
 */
bool wait_for_device(DCR *dcr, const char *msg, bool first)
{
   struct timeval tv;
   struct timezone tz;
   struct timespec timeout;
// time_t last_heartbeat = 0;
   int stat = 0;
   int add_wait;
   DEVICE *dev = dcr->dev;
   JCR *jcr = dcr->jcr;
   bool ok = false;

   P(device_release_mutex);

   if (first) {
      Jmsg(jcr, M_MOUNT, 0, msg);
   }

   /*
    * Wait requested time (dev->rem_wait_sec).	However, we also wake up every
    *	 HB_TIME seconds and send a heartbeat to the FD and the Director
    *	 to keep stateful firewalls from closing them down while waiting
    *	 for the operator.
    */
   add_wait = jcr->rem_wait_sec;
   if (me->heartbeat_interval && add_wait > me->heartbeat_interval) {
      add_wait = me->heartbeat_interval;
   }

   for ( ; !job_canceled(jcr); ) {
      time_t now, start;

      gettimeofday(&tv, &tz);
      timeout.tv_nsec = tv.tv_usec * 1000;
      timeout.tv_sec = tv.tv_sec + add_wait;

      Dmsg3(000, "I'm going to sleep on device %s. HB=%d wait=%d\n", dev->print_name(),
	 (int)me->heartbeat_interval, dev->wait_sec);
      start = time(NULL);
      /* Wait required time */
      stat = pthread_cond_timedwait(&wait_device_release, &device_release_mutex, &timeout);
      Dmsg1(000, "Wokeup from sleep on device stat=%d\n", stat);

      now = time(NULL);
      jcr->rem_wait_sec -= (now - start);

#ifdef needed
      /* Note, this always triggers the first time. We want that. */
      if (me->heartbeat_interval) {
	 if (now - last_heartbeat >= me->heartbeat_interval) {
	    /* send heartbeats */
	    if (jcr->file_bsock) {
	       bnet_sig(jcr->file_bsock, BNET_HEARTBEAT);
               Dmsg0(400, "Send heartbeat to FD.\n");
	    }
	    if (jcr->dir_bsock) {
	       bnet_sig(jcr->dir_bsock, BNET_HEARTBEAT);
	    }
	    last_heartbeat = now;
	 }
      }
#endif

      if (stat != ETIMEDOUT) {	   /* if someone woke us up */
	 ok = true;
	 break; 		   /* allow caller to examine device */
      }
      if (jcr->rem_wait_sec <= 0) {  /* on exceeding wait time return */
         Dmsg0(400, "Exceed wait time.\n");
	 if (!double_jcr_wait_time(jcr)) {
	    break;		   /* give up */
	 }
	 Jmsg(jcr, M_MOUNT, 0, msg);
      }

      add_wait = jcr->wait_sec - (now - start);
      if (add_wait < 0) {
	 add_wait = 0;
      }
      if (me->heartbeat_interval && add_wait > me->heartbeat_interval) {
	 add_wait = me->heartbeat_interval;
      }
   }

   V(device_release_mutex);
   Dmsg1(000, "Return from wait_device ok=%d\n", ok);
   return ok;
}

/*
 * The jcr timers are used for waiting on any device
 *
 * Returns: true if time doubled
 *	    false if max time expired
 */
static bool double_jcr_wait_time(JCR *jcr)
{
   jcr->wait_sec *= 2;		     /* double wait time */
   if (jcr->wait_sec > jcr->max_wait) {   /* but not longer than maxtime */
      jcr->wait_sec = jcr->max_wait;
   }
   jcr->num_wait++;
   jcr->rem_wait_sec = jcr->wait_sec;
   if (jcr->num_wait >= jcr->max_num_wait) {
      return false;
   }
   return true;
}
