/*
 *
 *   Bacula UA authentication. Provides authentication with
 *     the Director.
 *
 *     Kern Sibbald, June MMI
 *
 *    This routine runs as a thread and must be thread reentrant.
 *
 *  Basic tasks done here:
 *
 */
/*
   Copyright (C) 2001-2004 Kern Sibbald and John Walker

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA.
 */

#include "bacula.h"
#include "console_conf.h"
#include "jcr.h"


void senditf(const char *fmt, ...);
void sendit(const char *buf);

/* Commands sent to Director */
static char hello[]    = "Hello %s calling\n";

/* Response from Director */
static char OKhello[]   = "1000 OK:";

/* Forward referenced functions */

/*
 * Authenticate Director
 */
int authenticate_director(JCR *jcr, DIRRES *director, CONRES *cons)
{
   BSOCK *dir = jcr->dir_bsock;
   int tls_local_need = BNET_TLS_NONE;
   int tls_remote_need = BNET_TLS_NONE;
   char bashed_name[MAX_NAME_LENGTH];
   bool auth_success = false;
   char *password;
#ifdef HAVE_TLS
   TLS_CONTEXT *tls_ctx = NULL;
#endif /* HAVE_TLS */

   /*
    * Send my name to the Director then do authentication
    */
   if (cons) {
      bstrncpy(bashed_name, cons->hdr.name, sizeof(bashed_name));
      bash_spaces(bashed_name);
      password = cons->password;
#ifdef HAVE_TLS
      /* TLS Requirement */
      if (cons->tls_enable) {
         if (cons->tls_require) {
            tls_local_need = BNET_TLS_REQUIRED;
         } else {
            tls_local_need = BNET_TLS_OK;
         }
      }

      tls_ctx = cons->tls_ctx;
#endif /* HAVE_TLS */
   } else {
      bstrncpy(bashed_name, "*UserAgent*", sizeof(bashed_name));
      password = director->password;
#ifdef HAVE_TLS
      /* TLS Requirement */
      if (director->tls_enable) {
         if (director->tls_require) {
            tls_local_need = BNET_TLS_REQUIRED;
         } else {
            tls_local_need = BNET_TLS_OK;
         }
      }

      tls_ctx = director->tls_ctx;
#endif /* HAVE_TLS */
   }

   
   /* Timeout Hello after 5 mins */
   btimer_t *tid = start_bsock_timer(dir, 60 * 5);
   bnet_fsend(dir, hello, bashed_name);

   if (!cram_md5_get_auth(dir, password, &tls_remote_need) ||
       !cram_md5_auth(dir, password, tls_local_need)) {
      auth_success = false;
      goto auth_done;
   } else {
      auth_success = true;
      /* Continue on, soldier ... */
   }

   /* Verify that the remote host is willing to meet our TLS requirements */
   if (tls_remote_need < tls_local_need && tls_local_need != BNET_TLS_OK && tls_remote_need != BNET_TLS_OK) {
      sendit(_("Authorization problem:"
             " Remote server did not advertise required TLS support.\n"));
      auth_success = false;
      goto auth_done;
   }

   /* Verify that we are willing to meet the remote host's requirements */
   if (tls_remote_need > tls_local_need && tls_local_need != BNET_TLS_OK && tls_remote_need != BNET_TLS_OK) {
      sendit(_("Authorization problem:"
             " Remote server requires TLS.\n"));
      auth_success = false;
      goto auth_done;
   }

#ifdef HAVE_TLS
   /* Is TLS Enabled? */
   if (tls_local_need >= BNET_TLS_OK && tls_remote_need >= BNET_TLS_OK) {
      /* Engage TLS! Full Speed Ahead! */
      if (!bnet_tls_client(tls_ctx, dir)) {
         sendit(_("TLS negotiation failed\n"));
         auth_success = false;
         goto auth_done;
      }
      auth_success = true;
   }
#endif /* HAVE_TLS */

/* Authorization Completed */
auth_done:
   if (!auth_success) {
      stop_bsock_timer(tid);
      sendit( _("Director authorization problem.\n"
             "Most likely the passwords do not agree.\n"
             "If you are using TLS, there may have been a certificate validation error during the TLS handshake.\n"
             "Please see http://www.bacula.org/rel-manual/faq.html#AuthorizationErrors for help.\n"));
      return 0;
   }

   /*
    * It's possible that the TLS connection will
    * be dropped here if an invalid client certificate was presented
    */
   Dmsg1(6, ">dird: %s", dir->msg);
   if (bnet_recv(dir) <= 0) {
      stop_bsock_timer(tid);
      senditf(_("Bad response to Hello command: ERR=%s\n"),
         bnet_strerror(dir));
      senditf(_("If you are using TLS, it is possible that your client"
              " certificate was not accepted. Check the server messages.\n"));
      return 0;
   }
   Dmsg1(10, "<dird: %s", dir->msg);
   stop_bsock_timer(tid);
   if (strncmp(dir->msg, OKhello, sizeof(OKhello)-1) != 0) {
      sendit(_("Director rejected Hello command\n"));
      return 0;
   } else {
      sendit(dir->msg);
   }
   return 1;
}
