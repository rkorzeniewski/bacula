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
  * Originally written by Kern Sibbald for inclusion in apcupsd.
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
bnet_thread_server(int port, int max_clients, workq_t *client_wq, 
		   void handle_client_request(void *bsock))
{
   int newsockfd, sockfd, stat;
   socklen_t clilen;
   struct sockaddr_in cli_addr;       /* client's address */
   struct sockaddr_in serv_addr;      /* our address */
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
         Emsg1(M_ERROR, 0, "Cannot open stream socket: %s. Retrying ...\n", strerror(errno));
      }
      sleep(10);
   }

   /*
    * Reuse old sockets 
    */
   if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &turnon, sizeof(turnon)) < 0) {
      Emsg1(M_WARNING, 0, "Cannot set SO_REUSEADDR on socket: %s\n" , strerror(errno));
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
	 tlog = 2*60;		      /* Complain every 2 minutes */
         Emsg2(M_WARNING, 0, "Cannot bind port %d: %s. Retrying ...\n", port, strerror(errno));
      }
      sleep(5);
   }
   listen(sockfd, 5);		      /* tell system we are ready */

   FD_ZERO(&sockset);
   FD_SET(sockfd, &sockset);

   /* Start work queue thread */
   if ((stat = workq_init(client_wq, max_clients, handle_client_request)) != 0) {
      Emsg1(M_ABORT, 0, "Could not init client queue: ERR=%s\n", strerror(stat));
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
         Emsg1(M_FATAL, 0, "Error in select: %s\n", strerror(errno));
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
         Emsg2(M_WARNING, 0, "Connection from %s:%d refused by hosts.access",
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
         Emsg1(M_WARNING, 0, "Cannot set SO_KEEPALIVE on socket: %s\n" , strerror(errno));
      }

      /* see who client is. i.e. who connected to us. */
      caller = inet_ntoa(cli_addr.sin_addr);
      if (caller == NULL) {
         caller = "unknown client";
      }

      /* Queue client to be served */
      if ((stat = workq_add(client_wq, 
            (void *)init_bsock(newsockfd, "client", caller, port))) != 0) {
         Emsg1(M_ABORT, 0, "Could not add job to client queue: ERR=%s\n", strerror(stat));
      }
   }
}   


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
         Emsg1(M_ERROR, 0, "Cannot open stream socket: %s\n", strerror(errno));
      }
      sleep(60);
   }

   /*
    * Reuse old sockets 
    */
   if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &turnon, sizeof(turnon)) < 0) {
      Emsg1(M_WARNING, 0, "Cannot set SO_REUSEADDR on socket: %s\n" , strerror(errno));
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
         Emsg2(M_WARNING, 0, "Cannot bind port %d: %s: retrying ...\n", port, strerror(errno));
      }
      sleep(5);
   }
   listen(sockfd, 1);		      /* tell system we are ready */
   return init_bsock(sockfd, "Server socket", "client", port);
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
         Emsg1(M_FATAL, 0, "Error in select: %s\n", strerror(errno));
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
      Emsg2(M_WARNING, 0, "Connection from %s:%d refused by hosts.access",
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
      Emsg1(M_WARNING, 0, "Cannot set SO_KEEPALIVE on socket: %s\n" , strerror(errno));
   }

   /* see who client is. I.e. who connected to us.
    * return it in the input message buffer.
    */
   if ((caller = inet_ntoa(cli_addr.sin_addr)) != NULL) {
      strcpy(bsock->msg, caller);
   } else {
      bsock->msg[0] = 0;
   }
   bsock->msglen = strlen(bsock->msg);

   if (newsockfd < 0) {
      Emsg2(M_FATAL, 0, "Socket accept error for %s. ERR=%s\n", who,
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
      bs = init_bsock(newsockfd, "client", buf, bsock->port);
      free(buf);
      return bs;		      /* return new BSOCK */
   }
}   




/*
 * The following code will soon be deleted, don't attempt
 * to use it.
 */
#ifdef dont_have_threads_junk

/*
 * This routine is called by the main process to
 * track its children. On CYGWIN, the child processes
 * don't always exit when the other end of the socket
 * hangs up. Thus they remain hung on a read(). After
 * 30 seconds, we send them a SIGTERM signal, which 
 * causes them to wake up to the reality of the situation.
 *
 * Also, on certain Unix systems such as SunOS, we must
 * explicitly do the wait, otherwise, the child that dies
 * remains a zombie potentially remaining bound to the
 * port.
 */

#ifdef HAVE_SUN_OS

static void reap_children(int childpid)
{
   static int pids[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
   static int times[10];
   int i;
   time_t now;

   time(&now);
   for (i=0; i<10; i++) {
      if (pids[i] && (waitpid(pids[i], NULL, WNOHANG) == pids[i])) {
	 pids[i] = 0;
      } else {
	 if (pids[i] && ((now - times[i]) > 30))
	    kill(pids[i], SIGTERM);
      }
   }
   for (i=0; i<10; i++) {
      if (pids[i] && (waitpid(pids[i], NULL, WNOHANG) == pids[i])) {
	 pids[i] = 0;
      }
      if (childpid && (pids[i] == 0)) {
	 pids[i] = childpid;
	 times[i] = now;
	 childpid = 0;
      }
   }
}

#endif /* HAVE_SUN_OS */

/* Become network server */
void
bnet_server(int port, void handle_client_request(BSOCK *bsock))
{
   int newsockfd, sockfd, clilen, childpid, stat;
   struct sockaddr_in cli_addr;       /* client's address */
   struct sockaddr_in serv_addr;      /* our address */
   int tlog;
   fd_set ready, sockset;
   int turnon = 1;
   char *caller;
#ifdef HAVE_LIBWRAP
   struct request_info request;
#endif

   /* **** FIXME **** handle BSD too */
   signal(SIGCHLD, SIG_IGN);
   
   /*
    * Open a TCP socket  
    */
   for (tlog=0; (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0; tlog -= 10 ) {
      if (tlog <= 0) {
	 tlog = 60; 
         Emsg1(M_ERROR, 0, "Cannot open stream socket: %s. Retrying ...\n", strerror(errno));
      }
      sleep(10);
   }

   /*
    * Reuse old sockets 
    */
   if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &turnon, sizeof(turnon)) < 0) {
      Emsg1(M_WARNING, 0, "Cannot set SO_REUSEADDR on socket: %s\n" , strerror(errno));
   }

   /*
    * Receive notification when connection dies.
    */
   if (setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, &turnon, sizeof(turnon)) < 0) {
      Emsg1(M_WARNING, 0, "Cannot set SO_KEEPALIVE on socket: %s\n" , strerror(errno));
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
	 tlog = 2*60;		      /* Complain every 2 minutes */
         Emsg2(M_WARNING, 0, "Cannot bind port %d: %s. Retrying ...\n", port, strerror(errno));
      }
      sleep(5);
   }
   listen(sockfd, 5);		      /* tell system we are ready */

   FD_ZERO(&sockset);
   FD_SET(sockfd, &sockset);

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
         Emsg1(M_FATAL, 0, "Error in select: %s\n", strerror(errno));
	 break;
      }
      clilen = sizeof(cli_addr);
      newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);

#ifdef HAVE_LIBWRAP
      request_init(&request, RQ_DAEMON, my_name, RQ_FILE, newsockfd, 0);
      fromhost(&request);
      if (!hosts_access(&request)) {
         Emsg2(M_WARNING, 0, "Connection from %s:%d refused by hosts.access",
	       inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
	 close(newsockfd);
	 continue;
      }
#endif

      /*
       * Receive notification when connection dies.
       */
      if (setsockopt(newsockfd, SOL_SOCKET, SO_KEEPALIVE, &turnon, sizeof(turnon)) < 0) {
         Emsg1(M_WARNING, 0, "Cannot set SO_KEEPALIVE on socket: %s\n" , strerror(errno));
      }


      /* see who client is. i.e. who connected to us. */
      caller = inet_ntoa(cli_addr.sin_addr);
      if (caller == NULL) {
         caller = "client";
      }

#ifdef HAVE_CYGWIN
      childpid = 0;
      handle_client_request(init_bsock(newsockfd, "client", caller, port));
#else
      /* fork to provide the response */
      for (tlog=0; (childpid = fork()) < 0; tlog -= 5*60 ) {
	 if (tlog <= 0) {
	    tlog = 60*60; 
            Emsg1(M_FATAL, 0, "Fork error: %s\n", strerror(errno));
	 }
	 sleep(5*60);
      } 
      if (childpid == 0) {	/* child process */
	 close(sockfd); 	     /* close original socket */
         handle_client_request(init_bsock(newsockfd, "client", caller, port));    /* process the request */
         Dmsg0(99, "back from handle request\n");
	 close(newsockfd);
	 exit(0);
      }
#endif

      close(newsockfd); 	     /* parent process */
#ifdef HAVE_SUN_OS
      reap_children(childpid);
#endif /* HAVE_SUN_OS */
    }
}   

#endif /* no threads -- not supported any more sorry! */
