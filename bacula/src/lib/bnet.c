/*
 * Network Utility Routines
 *
 *  by Kern Sibbald
 *
 * Adapted and enhanced for Bacula, originally written 
 * for inclusion in the Apcupsd package
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


#include "bacula.h"

extern time_t watchdog_time;

#ifndef   INADDR_NONE
#define   INADDR_NONE	 -1
#endif

#ifndef ENODATA 		      /* not defined on BSD systems */
#define ENODATA EPIPE
#endif


/*
 * Read a nbytes from the network.
 * It is possible that the total bytes require in several
 * read requests
 */

static int32_t read_nbytes(BSOCK *bsock, char *ptr, int32_t nbytes)
{
   int32_t nleft, nread;
      
   nleft = nbytes;
   while (nleft > 0) {
      do {
	 errno = 0;
	 nread = read(bsock->fd, ptr, nleft);	 
      } while (!bsock->timed_out && nread == -1 && (errno == EINTR || errno == EAGAIN));
      if (nread <= 0) {
	 return nread;		     /* error, or EOF */
      }
      nleft -= nread;
      ptr += nread;
   }
   return nbytes - nleft;	     /* return >= 0 */
}

/*
 * Write nbytes to the network.
 * It may require several writes.
 */

static int32_t write_nbytes(BSOCK *bsock, char *ptr, int32_t nbytes)
{
   int32_t nleft, nwritten;

   nleft = nbytes;
   while (nleft > 0) {
      do {
	 errno = 0;
	 nwritten = write(bsock->fd, ptr, nleft);
      } while (!bsock->timed_out && nwritten == -1 && (errno == EINTR || errno == EAGAIN));
      if (nwritten <= 0) {
	 return nwritten;	     /* error */
      }
      nleft -= nwritten;
      ptr += nwritten;
   }
   return nbytes-nleft;
}

/* 
 * Receive a message from the other end. Each message consists of
 * two packets. The first is a header that contains the size
 * of the data that follows in the second packet.
 * Returns number of bytes read
 * Returns 0 on end of file
 * Returns -1 on hard end of file (i.e. network connection close)
 * Returns -2 on error
 */
/* EXTPROTO */
int32_t 
bnet_recv(BSOCK *bsock)
{
   int32_t nbytes;
   int32_t pktsiz;

   if (bsock->errors) {
      return -2;
   }

   bsock->read_seqno++; 	      /* bump sequence number */
   bsock->timer_start = watchdog_time;	/* set start wait time */
   bsock->timed_out = 0;
   /* get data size -- in int32_t */
   if ((nbytes = read_nbytes(bsock, (char *)&pktsiz, sizeof(int32_t))) <= 0) {
      bsock->timer_start = 0;	      /* clear timer */
      /* probably pipe broken because client died */
      if (errno == 0) {
	 bsock->b_errno = ENODATA;
      } else {
	 bsock->b_errno = errno;
      }
      bsock->errors++;
      return -1;		      /* assume hard EOF received */
   }
   bsock->timer_start = 0;	      /* clear timer */
   if (nbytes != sizeof(int32_t)) {
      bsock->errors++;
      bsock->b_errno = EIO;
      Emsg3(M_ERROR, 0, "Read %d expected %d from %s\n", nbytes, sizeof(int32_t),
	    bsock->who);
      return -2;
   }

   pktsiz = ntohl(pktsiz);	      /* decode no. of bytes that follow */

   if (pktsiz <= 0) {
      bsock->b_errno = ENODATA;
      bsock->msglen = pktsiz;	      /* return size */
      return 0; 		      /* soft EOF */
   }
   /* For big packet size, something went wrong */
   if (pktsiz > 10000000) {
      bsock->b_errno = ENODATA;
      bsock->msglen = pktsiz;	      /* return size */
      return 0; 		      /* soft EOF */
   }

   /* Make sure the buffer is big enough + one byte for EOS */
   if (pktsiz >= (int32_t)sizeof_pool_memory(bsock->msg)) {
      bsock->msg = (char *) realloc_pool_memory(bsock->msg, pktsiz + 100);
   }

   bsock->timer_start = watchdog_time;	/* set start wait time */
   bsock->timed_out = 0;
   /* now read the actual data */
   if ((nbytes = read_nbytes(bsock, bsock->msg, pktsiz)) <=  0) {
      bsock->timer_start = 0;	      /* clear timer */
      if (errno == 0) {
	 bsock->b_errno = ENODATA;
      } else {
	 bsock->b_errno = errno;
      }
      bsock->errors++;
      Emsg4(M_ERROR, 0, "Read error from %s:%s:%d: ERR=%s\n", 
	    bsock->who, bsock->host, bsock->port, bnet_strerror(bsock));
      return -2;
   }
   bsock->timer_start = 0;	      /* clear timer */
   bsock->in_msg_no++;
   bsock->msglen = nbytes;
   if (nbytes != pktsiz) {
      bsock->b_errno = EIO;
      bsock->errors++;
      Emsg5(M_ERROR, 0, "Read expected %d got %d from %s:%s:%d\n", pktsiz, nbytes,
	    bsock->who, bsock->host, bsock->port);
      return -2;
   }
   /* always add a zero by to properly terminate any
    * string that was send to us. Note, we ensured above that the
    * buffer is atleast one byte longer than the message length.
    */
   bsock->msg[nbytes] = 0;	      /* terminate in case it is a string */
   sm_check(__FILE__, __LINE__, False);
   return nbytes;		      /* return actual length of message */
}

/*
 * Send a message over the network. The send consists of
 * two network packets. The first is sends a 32 bit integer containing
 * the length of the data packet which follows.
 *
 * Returns: 0 on failure
 *	    1 on success
 */
int
bnet_send(BSOCK *bsock)
{
   int32_t rc;
   int32_t pktsiz;

   if (bsock->errors) {
      return 0;
   }
   pktsiz = htonl((int32_t)bsock->msglen);
   /* send int32_t containing size of data packet */
   bsock->timer_start = watchdog_time; /* start timer */
   bsock->timed_out = 0;	       
   rc = write_nbytes(bsock, (char *)&pktsiz, sizeof(int32_t));
   bsock->timer_start = 0;	      /* clear timer */
   if (rc != sizeof(int32_t)) {
      bsock->errors++;
      if (errno == 0) {
	 bsock->b_errno = EIO;
      } else {
	 bsock->b_errno = errno;
      }
      if (rc < 0) {
	 /****FIXME***** use Mmsg */
         Emsg4(M_ERROR, 0, "Write error sending to %s:%s:%d: ERR=%s\n", 
	       bsock->who, bsock->host, bsock->port,  bnet_strerror(bsock));
      } else {
         Emsg5(M_ERROR, 0, "Wrote %d bytes to %s:%s:%d, but only %d accepted.\n", 
	       bsock->who, bsock->host, bsock->port, bsock->msglen, rc);
      }
      return 0;
   }

   bsock->out_msg_no++; 	      /* increment message number */
   if (bsock->msglen <= 0) {	      /* length only? */
      return 1; 		      /* yes, no data */
   }

   /* send data packet */
   bsock->timer_start = watchdog_time; /* start timer */
   bsock->timed_out = 0;	       
   rc = write_nbytes(bsock, bsock->msg, bsock->msglen);
   bsock->timer_start = 0;	      /* clear timer */
   if (rc != bsock->msglen) {
      bsock->errors++;
      if (errno == 0) {
	 bsock->b_errno = EIO;
      } else {
	 bsock->b_errno = errno;
      }
      if (rc < 0) {
	 /************FIXME********* use Pmsg() **/
         Emsg4(M_ERROR, 0, "Write error sending to %s:%s:%d: ERR=%s\n", 
	       bsock->who, bsock->host, bsock->port,  bnet_strerror(bsock));
      } else {
         Emsg5(M_ERROR, 0, "Wrote %d bytes to %s:%s:%d, but only %d accepted.\n", 
	       bsock->who, bsock->host, bsock->port, bsock->msglen, rc);
      }
      return 0;
   }
   return 1;
}

/*
 * Wait for a specified time for data to appear on
 * the BSOCK connection.
 *
 *   Returns: 1 if data available
 *	      0 if timeout
 *	     -1 if error
 */
int 
bnet_wait_data(BSOCK *bsock, int sec)
{
   fd_set fdset;
   struct timeval tv;

   FD_ZERO(&fdset);
   FD_SET(bsock->fd, &fdset);
   tv.tv_sec = sec;
   tv.tv_usec = 0;
   for ( ;; ) {
      switch(select(bsock->fd + 1, &fdset, NULL, NULL, &tv)) {
	 case 0:			 /* timeout */
	    bsock->b_errno = 0;
	    return 0;
	 case -1:
	    bsock->b_errno = errno;
	    if (errno == EINTR || errno == EAGAIN) {
	       continue;
	    }
	    return -1;			/* error return */
	 default:
	    bsock->b_errno = 0;
	    return 1;
      }
   }
}

/*
 * Convert a hostname or dotted IP address into   
 * a s_addr.  We handle only IPv4.
 */
static uint32_t *bget_host_ip(char *host)
{
   struct in_addr inaddr;
   uint32_t *addr_list; 	      /* this really should be struct in_addr */
   struct hostent *hp;
   char **p;
   int i;

   if ((inaddr.s_addr = inet_addr(host)) != INADDR_NONE) {
      addr_list = (uint32_t *) malloc(sizeof(uint32_t) * 2);
      addr_list[0] = inaddr.s_addr;
      addr_list[1] = (uint32_t) -1;
   } else {
      /******FIXME***** use gethostbyname_r or mutex ****/
      if ((hp = gethostbyname(host)) == NULL) {
         Dmsg2(0, "gethostbyname() for %s failed: ERR=%s\n", host, strerror(errno));
	 return NULL;
      }
      if (hp->h_length != sizeof(inaddr.s_addr) || hp->h_addrtype != AF_INET) {
          Emsg2(M_WARNING, 0, "gethostbyname() network address length error.\n\
Wanted %d got %d bytes for s_addr.\n", sizeof(inaddr.s_addr), hp->h_length);
	  return NULL;
      }
      i = 0;
      for (p = hp->h_addr_list; *p != 0; p++) {
	 i++;
      }
      i++;
      addr_list = (uint32_t *) malloc(sizeof(uint32_t) * i);
      i = 0;
      for (p = hp->h_addr_list; *p != 0; p++) {
	 addr_list[i++] = (*(struct in_addr **)p)->s_addr;
      }
      addr_list[i] = (uint32_t) -1;
   }
   return addr_list;
}

/*     
 * Open a TCP connection to the UPS network server
 * Returns NULL
 * Returns BSOCK * pointer on success
 *
 *  ***FIXME*** implement service from /etc/services
 */
static BSOCK *
bnet_open(char *name, char *host, char *service, int port)
{
   int sockfd;
   struct sockaddr_in tcp_serv_addr;	 /* socket information */
   uint32_t *addr_list;
   int i, connected = 0;
   int turnon = 1;

   /* 
    * Fill in the structure serv_addr with the address of
    * the server that we want to connect with.
    */
   memset((char *)&tcp_serv_addr, 0,  sizeof(tcp_serv_addr));
   tcp_serv_addr.sin_family = AF_INET;
   tcp_serv_addr.sin_port = htons(port);

   if ((addr_list=bget_host_ip(host)) == NULL) {
     return NULL;
   }

   /* Open a TCP socket */
   if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
      free(addr_list);
      return NULL;
   }

   /*
    * Receive notification when connection dies.
    */
   if (setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, &turnon, sizeof(turnon)) < 0) {
      Emsg1(M_WARNING, 0, "Cannot set SO_KEEPALIVE on socket: %s\n" , strerror(errno));
   }
   
   for (i = 0; addr_list[i] != ((uint32_t) -1); i++) {
      /* connect to server */
      tcp_serv_addr.sin_addr.s_addr = addr_list[i];
      if (connect(sockfd, (struct sockaddr *)&tcp_serv_addr, sizeof(tcp_serv_addr)) < 0) {
	 continue;
      }
      connected = 1;
      break;
   }

   free(addr_list);
   if (!connected) {
      close(sockfd);
      return NULL;
   }
   return init_bsock(sockfd, name, host, port);
}

/*
 * Try to connect to host for max_retry_time at retry_time intervals.
 */
BSOCK *
bnet_connect(void *jcr, int retry_interval, int max_retry_time, char *name,
	     char *host, char *service, int port, int verbose)
{
   int i;
   BSOCK *bsock;

   for (i=0; (bsock = bnet_open(name, host, service, port)) == NULL; i -= retry_interval) {
      if (i <= 0) {
	 i = 60 * 5;		      /* complain again in 5 minutes */
	 if (verbose)
            Jmsg(jcr, M_WARNING, 0, "Could not connect to %s on %s:%d. ERR=%s\n\
Retrying ...\n", name, host, port, strerror(errno));
      }
      sleep(retry_interval);
      max_retry_time -= retry_interval;
      if (max_retry_time <= 0) {
         Jmsg(jcr, M_FATAL, 0, "Unable to connect to %s on %s:%d.\n", 
	      name, host, port);
	 return NULL;
      }
   }
   return bsock;
}


/*
 * Return the string for the error that occurred
 * on the socket. Only the first error is retained.
 */
char *bnet_strerror(BSOCK *bsock)
{
   /*  ***FIXME*** not thread safe */
   return strerror(bsock->b_errno);
}

/*
 * Format and send a message
 *  Returns: 0 on failure
 *	     1 on success
 */
int
bnet_fsend(BSOCK *bs, char *fmt, ...)
{
   va_list arg_ptr;
   int maxlen;

   /* This probably won't work, but we vsnprintf, then if we
    * get a negative length or a length greater than our buffer
    * (depending on which library is used), the printf was truncated, so
    * get a biger buffer and try again.
    */
again:
   maxlen = sizeof_pool_memory(bs->msg) - 1;
   va_start(arg_ptr, fmt);
   bs->msglen = bvsnprintf(bs->msg, maxlen, fmt, arg_ptr);
   va_end(arg_ptr);
   if (bs->msglen < 0 || bs->msglen >= maxlen) {
      bs->msg = (POOLMEM *)realloc_pool_memory(bs->msg, maxlen + 200);
      goto again;
   }
   return bnet_send(bs) < 0 ? 0 : 1;
}

/* 
 * Set the network buffer size, suggested size is in size.
 *  Actual size obtained is returned in bs->msglen
 *
 *  Returns: 0 on failure
 *	     1 on success
 */
int bnet_set_buffer_size(BSOCK *bs, uint32_t size, int rw)
{
   uint32_t dbuf_size;
#if defined(IP_TOS) && defined(IPTOS_THROUGHPUT)
   int opt;

    opt = IPTOS_THROUGHPUT;
    setsockopt(bs->fd, IPPROTO_IP, IP_TOS, (char *)&opt, sizeof(opt));
#endif

   dbuf_size = size;
   if ((bs->msg = (char *) realloc_pool_memory(bs->msg, dbuf_size+100)) == NULL) {
      Emsg0(M_FATAL, 0, "Could not malloc 32K BSOCK data buffer\n");
      return 0;
   }
   if (rw & BNET_SETBUF_READ) {
      while ((dbuf_size > TAPE_BSIZE) &&
	 (setsockopt(bs->fd, SOL_SOCKET, SO_RCVBUF, (char *)&dbuf_size, sizeof(dbuf_size)) < 0)) {
         Emsg1(M_ERROR, 0, "sockopt error: %s\n", strerror(errno));
	 dbuf_size -= TAPE_BSIZE;
      }
      Dmsg1(20, "set network buffer size=%d\n", dbuf_size);
      if (dbuf_size != MAX_NETWORK_BUFFER_SIZE)
         Emsg1(M_WARNING, 0, "Warning network buffer = %d bytes not max size.\n", dbuf_size);
      if (dbuf_size % TAPE_BSIZE != 0) {
         Emsg1(M_ABORT, 0, "Network buffer size %d not multiple of tape block size.\n",
	      dbuf_size);
      }
   }
   dbuf_size = size;
   if (rw & BNET_SETBUF_WRITE) {
      while ((dbuf_size > TAPE_BSIZE) &&
	 (setsockopt(bs->fd, SOL_SOCKET, SO_SNDBUF, (char *)&dbuf_size, sizeof(dbuf_size)) < 0)) {
         Emsg1(M_ERROR, 0, "sockopt error: %s\n", strerror(errno));
	 dbuf_size -= TAPE_BSIZE;
      }
      Dmsg1(20, "set network buffer size=%d\n", dbuf_size);
      if (dbuf_size != MAX_NETWORK_BUFFER_SIZE)
         Emsg1(M_WARNING, 0, "Warning network buffer = %d bytes not max size.\n", dbuf_size);
      if (dbuf_size % TAPE_BSIZE != 0) {
         Emsg1(M_ABORT, 0, "Network buffer size %d not multiple of tape block size.\n",
	      dbuf_size);
      }
   }

   bs->msglen = dbuf_size;
   return 1;
}

/*
 * Send a network "signal" to the other end 
 *  This consists of sending a negative packet length
 *
 *  Returns: 0 on failure
 *	     1 on success
 */
int bnet_sig(BSOCK *bs, int sig)
{
   bs->msglen = sig;
   return bnet_send(bs);
}

/*
 * Convert a network "signal" code into
 * human readable ASCII.
 */
char *bnet_sig_to_ascii(BSOCK *bs)
{
   static char buf[30];
   switch (bs->msglen) {
      case BNET_EOF:
         return "BNET_EOF";
      case BNET_EOD:
         return "BNET_EOD";
      case BNET_EOD_POLL:
         return "BNET_EOD_POLL";
      case BNET_STATUS:
         return "BNET_STATUS";
      case BNET_TERMINATE:
         return "BNET_TERMINATE";
      case BNET_POLL:
         return "BNET_POLL";
      case BNET_HEARTBEAT:
         return "BNET_HEARTBEAT";
      case BNET_HB_RESPONSE:
         return "BNET_HB_RESPONSE";
      case BNET_PROMPT:
         return "BNET_PROMPT";
      default:
         sprintf(buf, "Unknown sig %d", bs->msglen);
	 return buf;
   }
}


/* Initialize internal socket structure.  
 *  This probably should be done in net_open
 */
BSOCK *
init_bsock(int sockfd, char *who, char *host, int port) 
{
   BSOCK *bsock = (BSOCK *)malloc(sizeof(BSOCK));
   if (bsock == NULL) {
      Emsg0(M_ABORT, 0, "Out of memory in init_bsock.\n");
   }
   memset(bsock, 0, sizeof(BSOCK));
   bsock->fd = sockfd;
   bsock->errors = 0;
   bsock->msg = (POOLMEM *)get_pool_memory(PM_MESSAGE);
   bsock->errmsg = (POOLMEM *)get_pool_memory(PM_MESSAGE);
   bsock->who = bstrdup(who);
   bsock->host = bstrdup(host);
   bsock->port = port;
   /*
    * ****FIXME**** reduce this to a few hours once   
    *	heartbeats are implemented
    */
   bsock->timeout = 60 * 60 * 6 * 24; /* 6 days timeout */
   return bsock;
}

BSOCK *
dup_bsock(BSOCK *osock)
{
   BSOCK *bsock = (BSOCK *) malloc(sizeof(BSOCK));
   if (bsock == NULL) {
      Emsg0(M_ABORT, 0, "Out of memory in dup_bsock.\n");
   }
   memcpy(bsock, osock, sizeof(BSOCK));
   bsock->msg = (POOLMEM *)get_pool_memory(PM_MESSAGE);
   bsock->errmsg = (POOLMEM *)get_pool_memory(PM_MESSAGE);
   bsock->duped = TRUE;
   return bsock;
}

/* Close the network connection */
void 
bnet_close(BSOCK *bsock)
{
   BSOCK *next;

   for ( ; bsock != NULL; bsock = next) {
      next = bsock->next;
      if (!bsock->duped) {
	 shutdown(bsock->fd, SHUT_RDWR);
	 close(bsock->fd);
	 term_bsock(bsock);
      } else {
	 free(bsock);
      }
   }
   return;
}

void
term_bsock(BSOCK *bsock)
{
   free_pool_memory(bsock->msg);
   free_pool_memory(bsock->errmsg);
   free(bsock->who);
   free(bsock->host);
   free(bsock);
}
 
