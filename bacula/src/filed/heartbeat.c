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

/* 
 * Listen on the SD socket for heartbeat signals.
 * Send heartbeats to the Director every HB_TIME
 *   seconds.
 */
static void *heartbeat_thread(void *arg)
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

   jcr->duped_sd = sd;

   /* Hang reading the socket to the SD, and every time we get
    *	a heartbeat, we simply send it on to the Director to
    *	keep him alive.
    */
   for ( ; !is_bnet_stop(sd); ) {
      n = bnet_wait_data_intr(sd, 60);
      if (me->heartbeat_interval) {
	 now = time(NULL);
	 if (now-last_heartbeat >= me->heartbeat_interval) {
	    bnet_sig(dir, BNET_HEARTBEAT);
	    last_heartbeat = now;
	 }
      }
      if (n == 1) {		      /* input waiting */
	 bnet_recv(sd); 	      /* read it -- probably heartbeat from sd */
/*       Dmsg1(000, "Got %d from SD\n", sd->msglen); */
      }
   }
   bnet_close(sd);
   bnet_close(dir);
   jcr->duped_sd = NULL;
   return NULL;
}

/* Startup the heartbeat thread -- see above */
void start_heartbeat_monitor(JCR *jcr)
{
   jcr->duped_sd = NULL;
   pthread_create(&jcr->heartbeat_id, NULL, heartbeat_thread, (void *)jcr);
}

/* Terminate the heartbeat thread */
void stop_heartbeat_monitor(JCR *jcr) 
{
   /* Wait for heartbeat thread to start */
   while (jcr->duped_sd == NULL) {
      bmicrosleep(0, 50);	      /* avoid race */
   }
   jcr->duped_sd->timed_out = 1;      /* set timed_out to terminate read */
   jcr->duped_sd->terminated = 1;     /* set to terminate read */

   /* Wait for heartbeat thread to stop */
   while (jcr->duped_sd) {
      pthread_kill(jcr->heartbeat_id, TIMEOUT_SIGNAL);	/* make heartbeat thread go away */
      bmicrosleep(0, 20);
   }
}

/*
 * Same as above but we don't listen to the SD
 */
void start_dir_heartbeat(JCR *jcr)
{
   /* ***FIXME*** implement */
}

void stop_dir_heartbeat(JCR *jcr)
{
   /* ***FIXME*** implement */
}
