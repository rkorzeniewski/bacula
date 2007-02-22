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

#ifdef HAVE_WIN32
#define socketRead(fd, buf, len)  ::recv(fd, buf, len, 0)
#define socketWrite(fd, buf, len) ::send(fd, buf, len, 0)
#define socketClose(fd)           ::closesocket(fd)
#else
#define socketRead(fd, buf, len)  ::read(fd, buf, len)
#define socketWrite(fd, buf, len) ::write(fd, buf, len)
#define socketClose(fd)           ::close(fd)
#endif

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
            Qmsg5(m_jcr, M_ERROR, 0,
                  _("Write error sending %d bytes to %s:%s:%d: ERR=%s\n"), 
                  msglen, m_who,
                  m_host, m_port, bnet_strerror(this));
         }
      } else {
         Qmsg5(m_jcr, M_ERROR, 0,
               _("Wrote %d bytes to %s:%s:%d, but only %d accepted.\n"),
               msglen, m_who, m_host, m_port, rc);
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

/*
 * Receive a message from the other end. Each message consists of
 * two packets. The first is a header that contains the size
 * of the data that follows in the second packet.
 * Returns number of bytes read (may return zero)
 * Returns -1 on signal (BNET_SIGNAL)
 * Returns -2 on hard end of file (BNET_HARDEOF)
 * Returns -3 on error  (BNET_ERROR)
 *
 *  Unfortunately, it is a bit complicated because we have these
 *    four return types:
 *    1. Normal data
 *    2. Signal including end of data stream
 *    3. Hard end of file
 *    4. Error
 *  Using is_bnet_stop() and is_bnet_error() you can figure this all out.
 */
int32_t BSOCK::recv()
{
   int32_t nbytes;
   int32_t pktsiz;

   msg[0] = 0;
   msglen = 0;
   if (errors || terminated) {
      return BNET_HARDEOF;
   }

   read_seqno++;            /* bump sequence number */
   timer_start = watchdog_time;  /* set start wait time */
   timed_out = 0;
   /* get data size -- in int32_t */
   if ((nbytes = read_nbytes(this, (char *)&pktsiz, sizeof(int32_t))) <= 0) {
      timer_start = 0;      /* clear timer */
      /* probably pipe broken because client died */
      if (errno == 0) {
         b_errno = ENODATA;
      } else {
         b_errno = errno;
      }
      errors++;
      return BNET_HARDEOF;         /* assume hard EOF received */
   }
   timer_start = 0;         /* clear timer */
   if (nbytes != sizeof(int32_t)) {
      errors++;
      b_errno = EIO;
      Qmsg5(m_jcr, M_ERROR, 0, _("Read expected %d got %d from %s:%s:%d\n"),
            sizeof(int32_t), nbytes, m_who, m_host, m_port);
      return BNET_ERROR;
   }

   pktsiz = ntohl(pktsiz);         /* decode no. of bytes that follow */

   if (pktsiz == 0) {              /* No data transferred */
      timer_start = 0;      /* clear timer */
      in_msg_no++;
      msglen = 0;
      return 0;                    /* zero bytes read */
   }

   /* If signal or packet size too big */
   if (pktsiz < 0 || pktsiz > 1000000) {
      if (pktsiz > 0) {            /* if packet too big */
         Qmsg3(m_jcr, M_FATAL, 0,
               _("Packet size too big from \"%s:%s:%d. Terminating connection.\n"),
               m_who, m_host, m_port);
         pktsiz = BNET_TERMINATE;  /* hang up */
      }
      if (pktsiz == BNET_TERMINATE) {
         terminated = 1;
      }
      timer_start = 0;      /* clear timer */
      b_errno = ENODATA;
      msglen = pktsiz;      /* signal code */
      return BNET_SIGNAL;          /* signal */
   }

   /* Make sure the buffer is big enough + one byte for EOS */
   if (pktsiz >= (int32_t) sizeof_pool_memory(msg)) {
      msg = realloc_pool_memory(msg, pktsiz + 100);
   }

   timer_start = watchdog_time;  /* set start wait time */
   timed_out = 0;
   /* now read the actual data */
   if ((nbytes = read_nbytes(this, msg, pktsiz)) <= 0) {
      timer_start = 0;      /* clear timer */
      if (errno == 0) {
         b_errno = ENODATA;
      } else {
         b_errno = errno;
      }
      errors++;
      Qmsg4(m_jcr, M_ERROR, 0, _("Read error from %s:%s:%d: ERR=%s\n"),
            m_who, m_host, m_port, bnet_strerror(this));
      return BNET_ERROR;
   }
   timer_start = 0;         /* clear timer */
   in_msg_no++;
   msglen = nbytes;
   if (nbytes != pktsiz) {
      b_errno = EIO;
      errors++;
      Qmsg5(m_jcr, M_ERROR, 0, _("Read expected %d got %d from %s:%s:%d\n"),
            pktsiz, nbytes, m_who, m_host, m_port);
      return BNET_ERROR;
   }
   /* always add a zero by to properly terminate any
    * string that was send to us. Note, we ensured above that the
    * buffer is at least one byte longer than the message length.
    */
   msg[nbytes] = 0; /* terminate in case it is a string */
   sm_check(__FILE__, __LINE__, false);
   return nbytes;                  /* return actual length of message */
}


/*
 * Send a signal
 */
bool BSOCK::signal(int signal)
{
   msglen = signal;
   if (signal == BNET_TERMINATE) {
      suppress_error_msgs = true;
   }
   return send();
}

void BSOCK::close()
{
   BSOCK *bsock = this;
   BSOCK *next;

   for (; bsock; bsock = next) {
      next = bsock->m_next;           /* get possible pointer to next before destoryed */
      if (!bsock->duped) {
#ifdef HAVE_TLS
         /* Shutdown tls cleanly. */
         if (bsock->tls) {
            tls_bsock_shutdown(bsock);
            free_tls_connection(bsock->tls);
            bsock->tls = NULL;
         }
#endif /* HAVE_TLS */
         if (bsock->timed_out) {
            shutdown(bsock->fd, 2);   /* discard any pending I/O */
         }
         socketClose(bsock->fd);      /* normal close */
      }
      destroy();                      /* free the packet */
   }
   return;
}

void BSOCK::destroy()
{
   if (msg) {
      free_pool_memory(msg);
      msg = NULL;
   } else {
      ASSERT(1 == 0);              /* double close */
   }
   if (errmsg) {
      free_pool_memory(errmsg);
      errmsg = NULL;
   }
   if (m_who) {
      free(m_who);
      m_who = NULL;
   }
   if (m_host) {
      free(m_host);
      m_host = NULL;
   }
   free(this);
}
