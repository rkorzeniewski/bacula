/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2000-2014 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from many
   others, a complete list can be found in the file AUTHORS.

   You may use this file and others of this release according to the
   license defined in the LICENSE file, which includes the Affero General
   Public License, v3.0 ("AGPLv3") and some additional permissions and
   terms pursuant to its AGPLv3 Section 7.

   Bacula® is a registered trademark of Kern Sibbald.
*/
/*
 * Authenticate Director who is attempting to connect.
 *
 *   Kern Sibbald, October 2000
 *
 */

#include "bacula.h"
#include "filed.h"

extern CLIENT *me;                 /* my resource */

const int dbglvl = 50;

/* Version at end of Hello
 *   prior to 10Mar08 no version
 *   1 10Mar08
 *   2 13Mar09 - added the ability to restore from multiple storages
 *   3 03Sep10 - added the restore object command for vss plugin 4.0
 *   4 25Nov10 - added bandwidth command 5.1
 *   5 01Jan14 - added SD Calls Client and api version to status command
 *
 *   Next version should be 1000
 */
#define FD_VERSION 5

static char hello_sd[]  = "Hello Bacula SD: Start Job %s %d\n";

static char hello_dir[]  = "2000 OK Hello %d\n";
static char Dir_sorry[] = "2999 Authentication failed.\n";
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

/*********************************************************************
 *
 */
static bool authenticate(int rcode, BSOCK *bs, JCR* jcr)
{
   POOLMEM *dirname = get_pool_memory(PM_MESSAGE);
   DIRRES *director = NULL;
   int tls_local_need = BNET_TLS_NONE;
   int tls_remote_need = BNET_TLS_NONE;
   int compatible = true;                 /* Want md5 compatible DIR */
   bool auth_success = false;
   alist *verify_list = NULL;
   btimer_t *tid = NULL;
   int dir_version = 0;

   if (rcode != R_DIRECTOR) {
      Dmsg1(dbglvl, "I only authenticate directors, not %d\n", rcode);
      Jmsg1(jcr, M_FATAL, 0, _("I only authenticate directors, not %d\n"), rcode);
      goto auth_fatal;
   }

   dirname = check_pool_memory_size(dirname, bs->msglen);

   if (sscanf(bs->msg, "Hello Director %s calling %d", dirname, &dir_version) != 2 &&
       sscanf(bs->msg, "Hello Director %s calling", dirname) != 1) {
      char addr[64];
      char *who = bs->get_peer(addr, sizeof(addr)) ? bs->who() : addr;
      bs->msg[100] = 0;
      Dmsg2(dbglvl, "Bad Hello command from Director at %s: %s\n",
            bs->who(), bs->msg);
      Jmsg2(jcr, M_FATAL, 0, _("Bad Hello command from Director at %s: %s\n"),
            who, bs->msg);
      goto auth_fatal;
   }
   unbash_spaces(dirname);
   foreach_res(director, R_DIRECTOR) {
      if (strcmp(director->hdr.name, dirname) == 0)
         break;
   }
   if (!director) {
      char addr[64];
      char *who = bs->get_peer(addr, sizeof(addr)) ? bs->who() : addr;
      Jmsg2(jcr, M_FATAL, 0, _("Connection from unknown Director %s at %s rejected.\n"),
            dirname, who);
      goto auth_fatal;
   }

   if (have_tls) {
      /* TLS Requirement */
      if (director->tls_enable) {
         if (director->tls_require) {
            tls_local_need = BNET_TLS_REQUIRED;
         } else {
            tls_local_need = BNET_TLS_OK;
         }
      }

      if (director->tls_authenticate) {
         tls_local_need = BNET_TLS_REQUIRED;
      }

      if (director->tls_verify_peer) {
         verify_list = director->tls_allowed_cns;
      }
   }

   tid = start_bsock_timer(bs, AUTH_TIMEOUT);
   /* Challenge the director */
   auth_success = cram_md5_challenge(bs, director->password, tls_local_need, compatible);
   if (job_canceled(jcr)) {
      auth_success = false;
      goto auth_fatal;                   /* quick exit */
   }
   if (auth_success) {
      auth_success = cram_md5_respond(bs, director->password, &tls_remote_need, &compatible);
      if (!auth_success) {
          char addr[64];
          char *who = bs->get_peer(addr, sizeof(addr)) ? bs->who() : addr;
          Dmsg1(dbglvl, "cram_get_auth respond failed for Director: %s\n", who);
      }
   } else {
       char addr[64];
       char *who = bs->get_peer(addr, sizeof(addr)) ? bs->who() : addr;
       Dmsg1(dbglvl, "cram_auth challenge failed for Director %s\n", who);
   }
   if (!auth_success) {
       Emsg1(M_FATAL, 0, _("Incorrect password given by Director at %s.\n"),
             bs->who());
       goto auth_fatal;
   }

   /* Verify that the remote host is willing to meet our TLS requirements */
   if (tls_remote_need < tls_local_need && tls_local_need != BNET_TLS_OK && tls_remote_need != BNET_TLS_OK) {
      Jmsg0(jcr, M_FATAL, 0, _("Authorization problem: Remote server did not"
           " advertize required TLS support.\n"));
      Dmsg2(dbglvl, "remote_need=%d local_need=%d\n", tls_remote_need, tls_local_need);
      auth_success = false;
      goto auth_fatal;
   }

   /* Verify that we are willing to meet the remote host's requirements */
   if (tls_remote_need > tls_local_need && tls_local_need != BNET_TLS_OK && tls_remote_need != BNET_TLS_OK) {
      Jmsg0(jcr, M_FATAL, 0, _("Authorization problem: Remote server requires TLS.\n"));
      Dmsg2(dbglvl, "remote_need=%d local_need=%d\n", tls_remote_need, tls_local_need);
      auth_success = false;
      goto auth_fatal;
   }

   if (tls_local_need >= BNET_TLS_OK && tls_remote_need >= BNET_TLS_OK) {
      /* Engage TLS! Full Speed Ahead! */
      if (!bnet_tls_server(director->tls_ctx, bs, verify_list)) {
         Jmsg0(jcr, M_FATAL, 0, _("TLS negotiation failed.\n"));
         auth_success = false;
         goto auth_fatal;
      }
      if (director->tls_authenticate) {         /* authentication only? */
         bs->free_tls();                        /* shutodown tls */
      }
   }

auth_fatal:
   if (tid) {
      stop_bsock_timer(tid);
      tid = NULL;
   }
   free_pool_memory(dirname);
   jcr->director = director;
   /* Single thread all failures to avoid DOS */
   if (!auth_success) {
      P(mutex);
      bmicrosleep(6, 0);
      V(mutex);
   }
   return auth_success;
}

/*
 * Inititiate the communications with the Director.
 * He has made a connection to our server.
 *
 * Basic tasks done here:
 *   We read Director's initial message and authorize him.
 *
 */
int authenticate_director(JCR *jcr)
{
   BSOCK *dir = jcr->dir_bsock;

   if (!authenticate(R_DIRECTOR, dir, jcr)) {
      dir->fsend("%s", Dir_sorry);
      Emsg0(M_FATAL, 0, _("Unable to authenticate Director\n"));
      return 0;
   }
   return dir->fsend(hello_dir, FD_VERSION);
}

/*
 * First prove our identity to the Storage daemon, then
 * make him prove his identity.
 */
int authenticate_storagedaemon(JCR *jcr)
{
   BSOCK *sd = jcr->store_bsock;
   int tls_local_need = BNET_TLS_NONE;
   int tls_remote_need = BNET_TLS_NONE;
   int compatible = true;
   bool auth_success = false;
   int sd_version = 0;

   btimer_t *tid = start_bsock_timer(sd, AUTH_TIMEOUT);

   /* TLS Requirement */
   if (have_tls && me->tls_enable) {
      if (me->tls_require) {
         tls_local_need = BNET_TLS_REQUIRED;
      } else {
         tls_local_need = BNET_TLS_OK;
      }
   }

   if (me->tls_authenticate) {
      tls_local_need = BNET_TLS_REQUIRED;
   }

   if (job_canceled(jcr)) {
      auth_success = false;     /* force quick exit */
      goto auth_fatal;
   }


   sd->fsend(hello_sd, jcr->Job, FD_VERSION);
   Dmsg1(100, "Send to SD: %s\n", sd->msg);

   /* Respond to SD challenge */
   Dmsg0(050, "==== respond to SD challenge\n");
   auth_success = cram_md5_respond(sd, jcr->sd_auth_key, &tls_remote_need, &compatible);
   if (job_canceled(jcr)) {
      auth_success = false;     /* force quick exit */
      goto auth_fatal;
   }
   if (!auth_success) {
      Dmsg1(dbglvl, "cram_respond failed for SD: %s\n", sd->who());
   } else {
      /* Now challenge him */
      Dmsg0(050, "==== Challenge SD\n");
      auth_success = cram_md5_challenge(sd, jcr->sd_auth_key, tls_local_need, compatible);
      if (!auth_success) {
         Dmsg1(dbglvl, "cram_challenge failed for SD: %s\n", sd->who());
      }
   }

   if (!auth_success) {
      Jmsg(jcr, M_FATAL, 0, _("Authorization key rejected by Storage daemon.\n"
       "Please see " MANUAL_AUTH_URL " for help.\n"));
      goto auth_fatal;
   } else {
      Dmsg0(050, "Authorization with SD is OK\n");
   }

   /* Verify that the remote host is willing to meet our TLS requirements */
   if (tls_remote_need < tls_local_need && tls_local_need != BNET_TLS_OK && tls_remote_need != BNET_TLS_OK) {
      Jmsg(jcr, M_FATAL, 0, _("Authorization problem: Remote server did not"
           " advertize required TLS support.\n"));
      Dmsg2(dbglvl, "remote_need=%d local_need=%d\n", tls_remote_need, tls_local_need);
      auth_success = false;
      goto auth_fatal;
   }

   /* Verify that we are willing to meet the remote host's requirements */
   if (tls_remote_need > tls_local_need && tls_local_need != BNET_TLS_OK && tls_remote_need != BNET_TLS_OK) {
      Jmsg(jcr, M_FATAL, 0, _("Authorization problem: Remote server requires TLS.\n"));
      Dmsg2(dbglvl, "remote_need=%d local_need=%d\n", tls_remote_need, tls_local_need);
      auth_success = false;
      goto auth_fatal;
   }

   if (tls_local_need >= BNET_TLS_OK && tls_remote_need >= BNET_TLS_OK) {
      /* Engage TLS! Full Speed Ahead! */
      if (!bnet_tls_client(me->tls_ctx, sd, NULL)) {
         Jmsg(jcr, M_FATAL, 0, _("TLS negotiation failed.\n"));
         auth_success = false;
         goto auth_fatal;
      }
      if (me->tls_authenticate) {           /* tls authentication only? */
         sd->free_tls();                    /* yes, shutdown tls */
      }
   }
   if (sd->recv() <= 0) {
      auth_success = false;
      goto auth_fatal;
   }
   sscanf(sd->msg, "3000 OK Hello %d", &sd_version);

   /* At this point, we have successfully connected */

auth_fatal:
   /* Destroy session key */
   memset(jcr->sd_auth_key, 0, strlen(jcr->sd_auth_key));
   stop_bsock_timer(tid);
   /* Single thread all failures to avoid DOS */
   if (!auth_success) {
      P(mutex);
      bmicrosleep(6, 0);
      V(mutex);
   }
   return auth_success;
}
