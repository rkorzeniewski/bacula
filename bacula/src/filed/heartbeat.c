/*
 *  Bacula File Daemon heartbeat routines
 *    Listens for heartbeats coming from the SD
 *    If configured, sends heartbeats to Dir
 *
 *    Kern Sibbald, May MMIII
 *
 *   Version $Id$
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
#include "filed.h"

#ifdef HAVE_CYGWIN
/* pthread_kill() dies on Cygwin, so disable it */
#define pthread_kill(x, y)
/* Use shorter wait interval on Cygwin because no kill */
#define WAIT_INTERVAL 10
 
#else	/* Unix systems */
#define WAIT_INTERVAL 60
#endif

/* 
 * Listen on the SD socket for heartbeat signals.
 * Send heartbeats to the Director every HB_TIME
 *   seconds.
 */
static void *sd_heartbeat_thread(void *arg)
{
   int32_t n;
   JCR *jcr = (JCR *)arg;
   BSOCK *sd, *dir;
   time_t last_heartbeat = time(NULL);
   time_t now;

   pthread_detach(pthread_self());

   /* Get our own local copy */
   sd = dup_bsock(jcr->store_bsock);
   dir = dup_bsock(jcr->dir_bsock);

   jcr->hb_bsock = sd;

   /* Hang reading the socket to the SD, and every time we get
    *	a heartbeat or we get a wait timeout (1 minute), we
    *	check to see if we need to send a heartbeat to the
    *	Directory.
    */
   for ( ; !is_bnet_stop(sd); ) {
      n = bnet_wait_data_intr(sd, WAIT_INTERVAL);
      if (me->heartbeat_interval) {
	 now = time(NULL);
	 if (now-last_heartbeat >= me->heartbeat_interval) {
	    bnet_sig(dir, BNET_HEARTBEAT);
	    last_heartbeat = now;
	 }
      }
      if (n == 1) {		      /* input waiting */
	 bnet_recv(sd); 	      /* read it -- probably heartbeat from sd */
         Dmsg1(100, "Got %d from SD\n", sd->msglen);     
      }
   }
   bnet_close(sd);
   bnet_close(dir);
   jcr->hb_bsock = NULL;
   return NULL;
}

/* Startup the heartbeat thread -- see above */
void start_heartbeat_monitor(JCR *jcr)
{
   jcr->hb_bsock = NULL;
   pthread_create(&jcr->heartbeat_id, NULL, sd_heartbeat_thread, (void *)jcr);
}

/* Terminate the heartbeat thread. Used for both SD and DIR */
void stop_heartbeat_monitor(JCR *jcr) 
{
   int cnt = 0;
   /* Wait max 10 secs for heartbeat thread to start */
   while (jcr->hb_bsock == NULL && cnt++ < 200) {
      bmicrosleep(0, 50);	      /* avoid race */
   }

   if (jcr->hb_bsock) {
      jcr->hb_bsock->timed_out = 1;   /* set timed_out to terminate read */
      jcr->hb_bsock->terminated = 1;  /* set to terminate read */
   }
   cnt = 0;
   /* Wait max 100 secs for heartbeat thread to stop */
   while (jcr->hb_bsock && cnt++ < 200) {
      /* Naturally, Cygwin 1.3.20 craps out on the following */
      pthread_kill(jcr->heartbeat_id, TIMEOUT_SIGNAL);	/* make heartbeat thread go away */
      bmicrosleep(0, 500);
   }
}

/*
 * Thread for sending heartbeats to the Director when there
 *   is no SD monitoring needed -- e.g. restore and verify Vol
 *   both do their own read() on the SD socket.
 */
static void *dir_heartbeat_thread(void *arg)
{
   JCR *jcr = (JCR *)arg;
   BSOCK *dir;
   time_t last_heartbeat = time(NULL);

   pthread_detach(pthread_self());

   /* Get our own local copy */
   dir = dup_bsock(jcr->dir_bsock);

   jcr->hb_bsock = dir;

   for ( ; !is_bnet_stop(dir); ) {
      time_t now, next;

      now = time(NULL);
      next = now - last_heartbeat;
      if (next >= me->heartbeat_interval) {
	 bnet_sig(dir, BNET_HEARTBEAT);
	 last_heartbeat = now;
      }
      bmicrosleep(next, 0);
   }
   bnet_close(dir);
   jcr->hb_bsock = NULL;
   return NULL;
}
/*
 * Same as above but we don't listen to the SD
 */
void start_dir_heartbeat(JCR *jcr)
{
   if (me->heartbeat_interval) {
      pthread_create(&jcr->heartbeat_id, NULL, dir_heartbeat_thread, (void *)jcr);
   }
}

void stop_dir_heartbeat(JCR *jcr)
{
   if (me->heartbeat_interval) {
      stop_heartbeat_monitor(jcr);
   }
}
