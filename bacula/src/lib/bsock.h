/*
 * Bacula Sock Structure definition
 *
 * Kern Sibbald, May MM
 *
 * Zero msglen from other end indicates soft eof (usually
 *   end of some binary data stream, but not end of conversation).
 *
 * Negative msglen, is special "signal" (no data follows).
 *   See below for SIGNAL codes.
 *
 *   Version $Id$
 */
/*
   Copyright (C) 2000, 2001, 2002 Kern Sibbald and John Walker

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

typedef struct s_bsock {
   uint64_t read_seqno;               /* read sequence number */
   uint32_t in_msg_no;                /* intput message number */
   uint32_t out_msg_no;               /* output message number */
   int fd;                            /* socket file descriptor */
   int32_t msglen;                    /* message length */
   int port;                          /* desired port */
   int errors;                        /* set if errors on socket */
   int b_errno;                       /* bsock errno */
   time_t timer_start;                /* time started read/write */
   int timed_out;                     /* timed out in read/write */
   int timeout;                       /* time out after this value */
   int terminated;                    /* set when BNET_TERMINATE arrives */
   int duped;                         /* set if duped BSOCK */
   POOLMEM *msg;                      /* message pool buffer */
   char *who;                         /* Name of daemon to which we are talking */
   char *host;                        /* Host name/IP */
   POOLMEM *errmsg;                   /* edited error message (to be implemented) */
   RES *res;                          /* Resource to which we are connected */
   struct s_bsock *next;              /* next BSOCK if duped */
} BSOCK;

/* Signal definitions for use in bnet_sig() */
#define BNET_EOF          0           /* Deprecated, use BNET_EOD */
#define BNET_EOD         -1           /* End of data stream, new data may follow */
#define BNET_EOD_POLL    -2           /* End of data and poll all in one */
#define BNET_STATUS      -3           /* Send full status */
#define BNET_TERMINATE   -4           /* Conversation terminated, doing close() */
#define BNET_POLL        -5           /* Poll request, I'm hanging on a read */
#define BNET_HEARTBEAT   -6           /* Heartbeat Response requested */
#define BNET_HB_RESPONSE -7           /* Only response permited to HB */
#define BNET_PROMPT      -8           /* Prompt for UA */

#define BNET_SETBUF_READ  1           /* Arg for bnet_set_buffer_size */
#define BNET_SETBUF_WRITE 2           /* Arg for bnet_set_buffer_size */
