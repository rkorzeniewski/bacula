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
 /* 
  * Originally written by Kern Sibbald for inclusion in apcupsd,
  *  but heavily modified for Bacula
  *
  *   Version $Id$
  */

#include "bacula.h"
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

#ifdef HAVE_LIBWRAP
#include "tcpd.h"
int allow_severity = LOG_NOTICE;
int deny_severity = LOG_WARNING;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

/* Become Threaded Network Server */
void
bnet_thread_server(char *bind_addr, int port, int max_clients, workq_t *client_wq, 
		   void handle_client_request(void *bsock))
{
   int newsockfd, sockfd, stat;
   socklen_t clilen;
   struct sockaddr_in cli_addr;       /* client's address */
   struct sockaddr_in serv_addr;      /* our address */
   struct in_addr bind_ip;	      /* address to bind to */
   int tlog;
   fd_set ready, sockset;
   int turnon = 1;
   char *caller;
#ifdef HAVE_LIBWRAP
   struct request_info request;
#endif

   /*
    * Open a TCP socket  
    */
   for (tlog=0; (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0; tlog -= 10 ) {
      if (tlog <= 0) {
	 tlog = 60; 
         Emsg1(M_ERROR, 0, _("Cannot open stream socket: %s. Retrying ...\n"), strerror(errno));
      }
      sleep(10);
   }

   /*
    * Reuse old sockets 
    */
   if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &turnon, sizeof(turnon)) < 0) {
      Emsg1(M_WARNING, 0, _("Cannot set SO_REUSEADDR on socket: %s\n"), strerror(errno));
   }

   /* 
    * Bind our local address so that the client can send to us.
    */
   bind_ip.s_addr = htonl(INADDR_ANY);
   if (bind_addr && bind_addr[0]) {
#ifdef HAVE_INET_PTON
      if (inet_pton(AF_INET, bind_addr, &bind_ip) <= 0) {
#else
      if (inet_aton(bind_addr, &bind_ip) <= 0) {
#endif
         Emsg1(M_WARNING, 0, _("Invalid bind address: %s, using INADDR_ANY\n"),
	    bind_addr);
	 bind_ip.s_addr = htonl(INADDR_ANY);
      }
   }
   memset((char *) &serv_addr, 0, sizeof(serv_addr));
   serv_addr.sin_family = AF_INET;
   serv_addr.sin_addr.s_addr = bind_ip.s_addr;
   serv_addr.sin_port = htons(port);

   int tmax = 30 * (60 / 5);	      /* wait 30 minutes max */
   for (tlog=0; bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0; tlog -= 5 ) {
      if (tlog <= 0) {
	 tlog = 2*60;		      /* Complain every 2 minutes */
         Emsg2(M_WARNING, 0, _("Cannot bind port %d: %s. Retrying ...\n"), port, strerror(errno));
      }
      sleep(5);
      if (--tmax <= 0) {
         Emsg2(M_ABORT, 0, _("Cannot bind port %d: %s.\n"), port, strerror(errno));
      }
   }
   listen(sockfd, 5);		      /* tell system we are ready */

   FD_ZERO(&sockset);
   FD_SET(sockfd, &sockset);

   /* Start work queue thread */
   if ((stat = workq_init(client_wq, max_clients, handle_client_request)) != 0) {
      Emsg1(M_ABORT, 0, _("Could not init client queue: ERR=%s\n"), strerror(stat));
   }

   for (;;) {
      /* 
       * Wait for a connection from a client process.
       */
      ready = sockset;
      if ((stat = select(sockfd+1, &ready, NULL, NULL, NULL)) < 0) {
	 if (errno == EINTR || errno == EAGAIN) {
	    errno = 0;
	    continue;
	 }
	 close(sockfd);
         Emsg1(M_FATAL, 0, _("Error in select: %s\n"), strerror(errno));
	 break;
      }
      clilen = sizeof(cli_addr);
      newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);

#ifdef HAVE_LIBWRAP
      P(mutex); 		      /* hosts_access is not thread safe */
      request_init(&request, RQ_DAEMON, my_name, RQ_FILE, newsockfd, 0);
      fromhost(&request);
      if (!hosts_access(&request)) {
	 V(mutex);
         Jmsg2(NULL, M_WARNING, 0, _("Connection from %s:%d refused by hosts.access"),
	       inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
	 close(newsockfd);
	 continue;
      }
      V(mutex);
#endif

      /*
       * Receive notification when connection dies.
       */
      if (setsockopt(newsockfd, SOL_SOCKET, SO_KEEPALIVE, &turnon, sizeof(turnon)) < 0) {
         Emsg1(M_WARNING, 0, _("Cannot set SO_KEEPALIVE on socket: %s\n") , strerror(errno));
      }

      /* see who client is. i.e. who connected to us. */
      caller = inet_ntoa(cli_addr.sin_addr);
      if (caller == NULL) {
         caller = "unknown client";
      }

      /* Queue client to be served */
      if ((stat = workq_add(client_wq, 
            (void *)init_bsock(NULL, newsockfd, "client", caller, port), NULL, 0)) != 0) {
         Jmsg1(NULL, M_ABORT, 0, _("Could not add job to client queue: ERR=%s\n"), strerror(stat));
      }
   }
}   


#ifdef REALLY_USED   
/*
 * Bind an address so that we may accept connections
 * one at a time.
 */
BSOCK *
bnet_bind(int port)
{
   int sockfd;
   struct sockaddr_in serv_addr;      /* our address */
   int tlog;
   int turnon = 1;

   /*
    * Open a TCP socket  
    */
   for (tlog=0; (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0; tlog -= 10 ) {
      if (tlog <= 0) {
	 tlog = 2*60; 
         Emsg1(M_ERROR, 0, _("Cannot open stream socket: %s\n"), strerror(errno));
      }
      sleep(60);
   }

   /*
    * Reuse old sockets 
    */
   if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &turnon, sizeof(turnon)) < 0) {
      Emsg1(M_WARNING, 0, _("Cannot set SO_REUSEADDR on socket: %s\n") , strerror(errno));
   }

   /* 
    * Bind our local address so that the client can send to us.
    */
   bzero((char *) &serv_addr, sizeof(serv_addr));
   serv_addr.sin_family = AF_INET;
   serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
   serv_addr.sin_port = htons(port);

   for (tlog=0; bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0; tlog -= 5 ) {
      if (tlog <= 0) {
	 tlog = 2*60;
         Emsg2(M_WARNING, 0, _("Cannot bind port %d: %s: retrying ...\n"), port, strerror(errno));
      }
      sleep(5);
   }
   listen(sockfd, 1);		      /* tell system we are ready */
   return init_bsock(NULL, sockfd, _("Server socket"), _("client"), port);
}

/*
 * Accept a single connection 
 */
BSOCK *
bnet_accept(BSOCK *bsock, char *who)
{
   fd_set ready, sockset;
   int newsockfd, stat, len;
   socklen_t clilen;
   struct sockaddr_in cli_addr;       /* client's address */
   char *caller, *buf;
   BSOCK *bs;
   int turnon = 1;
#ifdef HAVE_LIBWRAP
   struct request_info request;
#endif

   /* 
    * Wait for a connection from the client process.
    */
   FD_ZERO(&sockset);
   FD_SET(bsock->fd, &sockset);

   for (;;) {
      /* 
       * Wait for a connection from a client process.
       */
      ready = sockset;
      if ((stat = select(bsock->fd+1, &ready, NULL, NULL, NULL)) < 0) {
	 if (errno == EINTR || errno == EAGAIN) {
	    errno = 0;
	    continue;
	 }
         Emsg1(M_FATAL, 0, _("Error in select: %s\n"), strerror(errno));
	 newsockfd = -1;
	 break;
      }
      clilen = sizeof(cli_addr);
      newsockfd = accept(bsock->fd, (struct sockaddr *)&cli_addr, &clilen);
      break;
   }

#ifdef HAVE_LIBWRAP
   P(mutex);
   request_init(&request, RQ_DAEMON, my_name, RQ_FILE, newsockfd, 0);
   fromhost(&request);
   if (!hosts_access(&request)) {
      V(mutex);
      Emsg2(M_WARNING, 0, _("Connection from %s:%d refused by hosts.access"),
	    inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
      close(newsockfd);
      return NULL;
   }
   V(mutex);
#endif

   /*
    * Receive notification when connection dies.
    */
   if (setsockopt(newsockfd, SOL_SOCKET, SO_KEEPALIVE, &turnon, sizeof(turnon)) < 0) {
      Emsg1(M_WARNING, 0, _("Cannot set SO_KEEPALIVE on socket: %s\n"), strerror(errno));
   }

   /* see who client is. I.e. who connected to us.
    * return it in the input message buffer.
    */
   if ((caller = inet_ntoa(cli_addr.sin_addr)) != NULL) {
      pm_strcpy(&bsock->msg, caller);
   } else {
      bsock->msg[0] = 0;
   }
   bsock->msglen = strlen(bsock->msg);

   if (newsockfd < 0) {
      Emsg2(M_FATAL, 0, _("Socket accept error for %s. ERR=%s\n"), who,
	    strerror(errno));
      return NULL;
   } else {
      if (caller == NULL) {
         caller = "unknown";
      }
      len = strlen(caller) + strlen(who) + 3;
      buf = (char *) malloc(len);
      strcpy(buf, who);
      strcat(buf, ": ");
      strcat(buf, caller);
      bs = init_bsock(NULL, newsockfd, "client", buf, bsock->port);
      free(buf);
      return bs;		      /* return new BSOCK */
   }
}   

#endif
