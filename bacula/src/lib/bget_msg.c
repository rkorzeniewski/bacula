/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2001-2014 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from many
   others, a complete list can be found in the file AUTHORS.

   You may use this file and others of this release according to the
   license defined in the LICENSE file, which includes the Affero General
   Public License, v3.0 ("AGPLv3") and some additional permissions and
   terms pursuant to its AGPLv3 Section 7.

   Bacula® is a registered trademark of Kern Sibbald.
*/
/*
 *  Subroutines to receive network data and handle
 *   network signals for the FD and the SD.
 *
 *   Kern Sibbald, May MMI previously in src/stored/fdmsg.c
 *
 */

#include "bacula.h"                   /* pull in global headers */

static char OK_msg[]   = "2000 OK\n";
static char TERM_msg[] = "2999 Terminate\n";

#define msglvl 500

/*
 * This routine does a bnet_recv(), then if a signal was
 *   sent, it handles it.  The return codes are the same as
 *   bne_recv() except the BNET_SIGNAL messages that can
 *   be handled are done so without returning.
 *
 * Returns number of bytes read (may return zero)
 * Returns -1 on signal (BNET_SIGNAL)
 * Returns -2 on hard end of file (BNET_HARDEOF)
 * Returns -3 on error  (BNET_ERROR)
 */
int bget_msg(BSOCK *sock)
{
   int n;
   for ( ;; ) {
      n = sock->recv();
      if (n >= 0) {                  /* normal return */
         return n;
      }
      if (sock->is_stop()) {         /* error return */
         return n;
      }

      /* BNET_SIGNAL (-1) return from bnet_recv() => network signal */
      switch (sock->msglen) {
      case BNET_EOD:               /* end of data */
         Dmsg0(msglvl, "Got BNET_EOD\n");
         return n;
      case BNET_EOD_POLL:
         Dmsg0(msglvl, "Got BNET_EOD_POLL\n");
         if (sock->is_terminated()) {
            sock->fsend(TERM_msg);
         } else {
            sock->fsend(OK_msg); /* send response */
         }
         return n;                 /* end of data */
      case BNET_TERMINATE:
         Dmsg0(msglvl, "Got BNET_TERMINATE\n");
         sock->set_terminated();
         return n;
      case BNET_POLL:
         Dmsg0(msglvl, "Got BNET_POLL\n");
         if (sock->is_terminated()) {
            sock->fsend(TERM_msg);
         } else {
            sock->fsend(OK_msg); /* send response */
         }
         break;
      case BNET_HEARTBEAT:
      case BNET_HB_RESPONSE:
         break;
      case BNET_STATUS:
         /* *****FIXME***** Implement BNET_STATUS */
         Dmsg0(msglvl, "Got BNET_STATUS\n");
         sock->fsend(_("Status OK\n"));
         sock->signal(BNET_EOD);
         break;
      default:
         Emsg1(M_ERROR, 0, _("bget_msg: unknown signal %d\n"), sock->msglen);
         break;
      }
   }
}
