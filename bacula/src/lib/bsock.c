/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2007-2007 Free Software Foundation Europe e.V.

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
 * Network Utility Routines
 *
 *  by Kern Sibbald
 *
 *   Version $Id: bnet.c 3670 2006-11-21 16:13:58Z kerns $
 */


#include "bacula.h"
#include "jcr.h"
#include <netdb.h>

/*
 * Send a message over the network. The send consists of
 * two network packets. The first is sends a 32 bit integer containing
 * the length of the data packet which follows.
 *
 * Returns: false on failure
 *          true  on success
 */
bool BSOCK::send()
{
   int32_t rc;
   int32_t pktsiz;
   int32_t *hdr;

   if (errors || terminated || msglen > 1000000) {
      return false;
   }
   /* Compute total packet length */
   if (msglen <= 0) {
      pktsiz = sizeof(pktsiz);               /* signal, no data */
   } else {
      pktsiz = msglen + sizeof(pktsiz);      /* data */
   }
   /* Store packet length at head of message -- note, we
    *  have reserved an int32_t just before msg, so we can
    *  store there 
    */
   hdr = (int32_t *)(msg - (int)sizeof(pktsiz));
   *hdr = htonl(msglen);                     /* store signal/length */

   out_msg_no++;            /* increment message number */

   /* send data packet */
   timer_start = watchdog_time;  /* start timer */
   timed_out = 0;
   /* Full I/O done in one write */
   rc = write_nbytes(this, (char *)hdr, pktsiz);
   timer_start = 0;         /* clear timer */
   if (rc != pktsiz) {
      errors++;
      if (errno == 0) {
         b_errno = EIO;
      } else {
         b_errno = errno;
      }
      if (rc < 0) {
         if (!suppress_error_msgs) {
            Qmsg5(jcr, M_ERROR, 0,
                  _("Write error sending %d bytes to %s:%s:%d: ERR=%s\n"), 
                  msglen, who,
                  host, port, bnet_strerror(this));
         }
      } else {
         Qmsg5(jcr, M_ERROR, 0,
               _("Wrote %d bytes to %s:%s:%d, but only %d accepted.\n"),
               msglen, who, host, port, rc);
      }
      return false;
   }
   return true;
}

/*
 * Format and send a message
 *  Returns: false on error
 *           true  on success
 */
bool BSOCK::fsend(const char *fmt, ...)
{
   va_list arg_ptr;
   int maxlen;

   if (errors || terminated) {
      return false;
   }
   /* This probably won't work, but we vsnprintf, then if we
    * get a negative length or a length greater than our buffer
    * (depending on which library is used), the printf was truncated, so
    * get a bigger buffer and try again.
    */
   for (;;) {
      maxlen = sizeof_pool_memory(msg) - 1;
      va_start(arg_ptr, fmt);
      msglen = bvsnprintf(msg, maxlen, fmt, arg_ptr);
      va_end(arg_ptr);
      if (msglen > 0 && msglen < (maxlen - 5)) {
         break;
      }
      msg = realloc_pool_memory(msg, maxlen + maxlen / 2);
   }
   return send();
}
