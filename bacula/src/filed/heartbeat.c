/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2003-2014 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from many
   others, a complete list can be found in the file AUTHORS.

   You may use this file and others of this release according to the
   license defined in the LICENSE file, which includes the Affero General
   Public License, v3.0 ("AGPLv3") and some additional permissions and
   terms pursuant to its AGPLv3 Section 7.

   Bacula® is a registered trademark of Kern Sibbald.
*/
/*
 *  Bacula File Daemon heartbeat routines
 *    Listens for heartbeats coming from the SD
 *    If configured, sends heartbeats to Dir
 *
 *    Kern Sibbald, May MMIII
 *
 */

#include "bacula.h"
#include "filed.h"

#define WAIT_INTERVAL 5

extern "C" void *sd_heartbeat_thread(void *arg);
extern "C" void *dir_heartbeat_thread(void *arg);
extern bool no_signals;

/*
 * Listen on the SD socket for heartbeat signals.
 * Send heartbeats to the Director every HB_TIME
 *   seconds.
 */
extern "C" void *sd_heartbeat_thread(void *arg)
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
   jcr->hb_started = true;
   jcr->hb_dir_bsock = dir;
   dir->suppress_error_messages(true);
   sd->suppress_error_messages(true);

   /* Hang reading the socket to the SD, and every time we get
    *   a heartbeat or we get a wait timeout (5 seconds), we
    *   check to see if we need to send a heartbeat to the
    *   Director.
    */
   while (!sd->is_stop()) {
      n = sd->wait_data_intr(WAIT_INTERVAL);
      if (n < 0 || sd->is_stop()) {
         break;
      }
      if (me->heartbeat_interval) {
         now = time(NULL);
         if (now-last_heartbeat >= me->heartbeat_interval) {
            dir->signal(BNET_HEARTBEAT);
            if (dir->is_stop()) {
               break;
            }
            last_heartbeat = now;
         }
      }
      if (n == 1) {               /* input waiting */
         sd->recv();              /* read it -- probably heartbeat from sd */
         if (sd->is_stop()) {
            break;
         }
         if (sd->msglen <= 0) {
            Dmsg1(100, "Got BNET_SIG %d from SD\n", sd->msglen);
         } else {
            Dmsg2(100, "Got %d bytes from SD. MSG=%s\n", sd->msglen, sd->msg);
         }
      }
      Dmsg2(200, "wait_intr=%d stop=%d\n", n, sd->is_stop());
   }
   /*
    * Note, since sd and dir are local dupped sockets, this
    *  is one place where we can call destroy().
    */
   sd->destroy();
   dir->destroy();
   jcr->hb_bsock = NULL;
   jcr->hb_started = false;
   jcr->hb_dir_bsock = NULL;
   return NULL;
}

/* Startup the heartbeat thread -- see above */
void start_heartbeat_monitor(JCR *jcr)
{
   /*
    * If no signals are set, do not start the heartbeat because
    * it gives a constant stream of TIMEOUT_SIGNAL signals that
    * make debugging impossible.
    */
   if (!no_signals && (me->heartbeat_interval > 0)) {
      jcr->hb_bsock = NULL;
      jcr->hb_started = false;
      jcr->hb_dir_bsock = NULL;
      pthread_create(&jcr->heartbeat_id, NULL, sd_heartbeat_thread, (void *)jcr);
   }
}

/* Terminate the heartbeat thread. Used for both SD and DIR */
void stop_heartbeat_monitor(JCR *jcr)
{
   int cnt = 0;
   if (no_signals) {
      return;
   }
   /* Wait max 10 secs for heartbeat thread to start */
   while (!jcr->hb_started && cnt++ < 200) {
      bmicrosleep(0, 50000);         /* wait for start */
   }

   if (jcr->hb_started) {
      jcr->hb_bsock->set_timed_out();       /* set timed_out to terminate read */
      jcr->hb_bsock->set_terminated();      /* set to terminate read */
   }
   if (jcr->hb_dir_bsock) {
      jcr->hb_dir_bsock->set_timed_out();     /* set timed_out to terminate read */
      jcr->hb_dir_bsock->set_terminated();    /* set to terminate read */
   }
   if (jcr->hb_started) {
      Dmsg0(100, "Send kill to heartbeat id\n");
      pthread_kill(jcr->heartbeat_id, TIMEOUT_SIGNAL);  /* make heartbeat thread go away */
      bmicrosleep(0, 50000);
   }
   cnt = 0;
   /* Wait max 100 secs for heartbeat thread to stop */
   while (jcr->hb_started && cnt++ < 200) {
      pthread_kill(jcr->heartbeat_id, TIMEOUT_SIGNAL);  /* make heartbeat thread go away */
      bmicrosleep(0, 500000);
   }
}

/*
 * Thread for sending heartbeats to the Director when there
 *   is no SD monitoring needed -- e.g. restore and verify Vol
 *   both do their own read() on the SD socket.
 */
extern "C" void *dir_heartbeat_thread(void *arg)
{
   JCR *jcr = (JCR *)arg;
   BSOCK *dir;
   time_t last_heartbeat = time(NULL);

   pthread_detach(pthread_self());

   /* Get our own local copy */
   dir = dup_bsock(jcr->dir_bsock);

   jcr->hb_bsock = dir;
   jcr->hb_started = true;
   dir->suppress_error_messages(true);

   while (!dir->is_stop()) {
      time_t now, next;

      now = time(NULL);
      next = now - last_heartbeat;
      if (next >= me->heartbeat_interval) {
         dir->signal(BNET_HEARTBEAT);
         if (dir->is_stop()) {
            break;
         }
         last_heartbeat = now;
      }
      /* This should never happen, but it might ... */
      if (next <= 0) {
         next = 1;
      }
      bmicrosleep(next, 0);
   }
   dir->destroy();
   jcr->hb_bsock = NULL;
   jcr->hb_started = false;
   return NULL;
}

/*
 * Same as above but we don't listen to the SD
 */
void start_dir_heartbeat(JCR *jcr)
{
   if (!no_signals && (me->heartbeat_interval > 0)) {
      jcr->dir_bsock->set_locking();
      pthread_create(&jcr->heartbeat_id, NULL, dir_heartbeat_thread, (void *)jcr);
   }
}

void stop_dir_heartbeat(JCR *jcr)
{
   if (me->heartbeat_interval > 0) {
      stop_heartbeat_monitor(jcr);
   }
}
