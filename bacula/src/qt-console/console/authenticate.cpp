/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2001-2007 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from
   many others, a complete list can be found in the file AUTHORS.
   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version two of the GNU General Public
   License as published by the Free Software Foundation plus additions
   that are listed in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

   Bacula® is a registered trademark of John Walker.
   The licensor of Bacula is the Free Software Foundation Europe
   (FSFE), Fiduciary Program, Sumatrastrasse 25, 8006 Zürich,
   Switzerland, email:ftf@fsfeurope.org.
*/

/*
 *
 *   Bacula UA authentication. Provides authentication with
 *     the Director.
 *
 *     Kern Sibbald, June MMI   adapted to bat, Jan MMVI
 *
 *     Version $Id$
 *
 */


#include "bat.h"


/* Commands sent to Director */
static char hello[]    = "Hello %s calling\n";

/* Response from Director */
static char OKhello[]   = "1000 OK:";

/* Forward referenced functions */

/*
 * Authenticate Director
 */
bool Console::authenticate_director(JCR *jcr, DIRRES *director, CONRES *cons)
{
   BSOCK *dir = jcr->dir_bsock;
   int tls_local_need = BNET_TLS_NONE;
   int tls_remote_need = BNET_TLS_NONE;
   int compatible = true;
   char bashed_name[MAX_NAME_LENGTH];
   char *password;
   TLS_CONTEXT *tls_ctx = NULL;

   /*
    * Send my name to the Director then do authentication
    */
   if (cons) {
      bstrncpy(bashed_name, cons->hdr.name, sizeof(bashed_name));
      bash_spaces(bashed_name);
      password = cons->password;
      /* TLS Requirement */
      if (cons->tls_enable) {
         if (cons->tls_require) {
            tls_local_need = BNET_TLS_REQUIRED;
         } else {
            tls_local_need = BNET_TLS_OK;
         }
      }

      tls_ctx = cons->tls_ctx;
   } else {
      bstrncpy(bashed_name, "*UserAgent*", sizeof(bashed_name));
      password = director->password;
      /* TLS Requirement */
      if (director->tls_enable) {
         if (director->tls_require) {
            tls_local_need = BNET_TLS_REQUIRED;
         } else {
            tls_local_need = BNET_TLS_OK;
         }
      }

      tls_ctx = director->tls_ctx;
   }
   /* Timeout Hello after 30 secs */
   btimer_t *tid = start_bsock_timer(dir, 30);
   dir->fsend(hello, bashed_name);

   /* respond to Dir challenge */
   if (!cram_md5_respond(dir, password, &tls_remote_need, &compatible) ||
       /* Now challenge dir */
       !cram_md5_challenge(dir, password, tls_local_need, compatible)) {
      display_textf(_("Director authorization problem at \"%s:%d\"\n"),
         dir->host(), dir->port());
      goto bail_out;
   }

   /* Verify that the remote host is willing to meet our TLS requirements */
   if (tls_remote_need < tls_local_need && tls_local_need != BNET_TLS_OK && tls_remote_need != BNET_TLS_OK) {
      display_textf(_("Authorization problem:"
             " Remote server at \"%s:%d\" did not advertise required TLS support.\n"),
             dir->host(), dir->port());
      goto bail_out;
   }

   /* Verify that we are willing to meet the remote host's requirements */
   if (tls_remote_need > tls_local_need && tls_local_need != BNET_TLS_OK && tls_remote_need != BNET_TLS_OK) {
      display_textf(_("Authorization problem with Director at \"%s:%d\":"
                     " Remote server requires TLS.\n"),
                     dir->host(), dir->port());

      goto bail_out;
   }

   /* Is TLS Enabled? */
   if (have_tls) {
      if (tls_local_need >= BNET_TLS_OK && tls_remote_need >= BNET_TLS_OK) {
         /* Engage TLS! Full Speed Ahead! */
         if (!bnet_tls_client(tls_ctx, dir, NULL)) {
            display_textf(_("TLS negotiation failed with Director at \"%s:%d\"\n"),
               dir->host(), dir->port());
            goto bail_out;
         }
      }
   }

   Dmsg1(6, ">dird: %s", dir->msg);
   if (dir->recv() <= 0) {
      stop_bsock_timer(tid);
      display_textf(_("Bad response to Hello command: ERR=%s\n"
                      "The Director at \"%s:%d\" is probably not running.\n"),
                    dir->bstrerror(), dir->host(), dir->port());
      return false;
   }

  stop_bsock_timer(tid);
   Dmsg1(10, "<dird: %s", dir->msg);
   if (strncmp(dir->msg, OKhello, sizeof(OKhello)-1) != 0) {
      display_textf(_("Director at \"%s:%d\" rejected Hello command\n"),
         dir->host(), dir->port());
      return false;
   } else {
      display_text(dir->msg);
   }
   return true;

bail_out:
   stop_bsock_timer(tid);
   display_textf(_("Authorization problem with Director at \"%s:%d\"\n"
             "Most likely the passwords do not agree.\n"
             "If you are using TLS, there may have been a certificate validation error during the TLS handshake.\n"
             "Please see http://www.bacula.org/rel-manual/faq.html#AuthorizationErrors for help.\n"), 
             dir->host(), dir->port());
   return false;
}
