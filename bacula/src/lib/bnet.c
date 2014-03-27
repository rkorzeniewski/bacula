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
 * Network Utility Routines
 *
 *  by Kern Sibbald
 *
 * Adapted and enhanced for Bacula, originally written
 * for inclusion in the Apcupsd package
 *
 */


#include "bacula.h"
#include "jcr.h"
#include <netdb.h>

#ifndef   INADDR_NONE
#define   INADDR_NONE    -1
#endif

#ifdef HAVE_WIN32
#define socketRead(fd, buf, len)  recv(fd, buf, len, 0)
#define socketWrite(fd, buf, len) send(fd, buf, len, 0)
#define socketClose(fd)           closesocket(fd)
#else
#define socketRead(fd, buf, len)  read(fd, buf, len)
#define socketWrite(fd, buf, len) write(fd, buf, len)
#define socketClose(fd)           close(fd)
#endif

#ifndef HAVE_GETADDRINFO
static pthread_mutex_t ip_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

/*
 * Read a nbytes from the network.
 * It is possible that the total bytes require in several
 * read requests
 */

int32_t read_nbytes(BSOCK * bsock, char *ptr, int32_t nbytes)
{
   int32_t nleft, nread;

#ifdef HAVE_TLS
   if (bsock->tls) {
      /* TLS enabled */
      return (tls_bsock_readn(bsock, ptr, nbytes));
   }
#endif /* HAVE_TLS */

   nleft = nbytes;
   while (nleft > 0) {
      errno = 0;
      nread = socketRead(bsock->m_fd, ptr, nleft);
      if (bsock->is_timed_out() || bsock->is_terminated()) {
         return -1;
      }

#ifdef HAVE_WIN32
      /*
       * For Windows, we must simulate Unix errno on a socket
       *  error in order to handle errors correctly.
       */
      if (nread == SOCKET_ERROR) {
        DWORD err = WSAGetLastError();
        nread = -1;
        if (err == WSAEINTR) {
           errno = EINTR;
        } else if (err == WSAEWOULDBLOCK) {
           errno = EAGAIN;
        } else {
           errno = EIO;            /* some other error */
        }
     }
#endif

      if (nread == -1) {
         if (errno == EINTR) {
            continue;
         }
         if (errno == EAGAIN) {
            bmicrosleep(0, 20000);  /* try again in 20ms */
            continue;
         }
      }
      if (nread <= 0) {
         return -1;                /* error, or EOF */
      }
      nleft -= nread;
      ptr += nread;
      if (bsock->use_bwlimit()) {
         bsock->control_bwlimit(nread);
      }
   }
   return nbytes - nleft;          /* return >= 0 */
}

/*
 * Write nbytes to the network.
 * It may require several writes.
 */

int32_t write_nbytes(BSOCK * bsock, char *ptr, int32_t nbytes)
{
   int32_t nleft, nwritten;

   if (bsock->is_spooling()) {
      nwritten = fwrite(ptr, 1, nbytes, bsock->m_spool_fd);
      if (nwritten != nbytes) {
         berrno be;
         bsock->b_errno = errno;
         Qmsg3(bsock->jcr(), M_FATAL, 0, _("Attr spool write error. wrote=%d wanted=%d bytes. ERR=%s\n"),
               nbytes, nwritten, be.bstrerror());
         Dmsg2(400, "nwritten=%d nbytes=%d.\n", nwritten, nbytes);
         errno = bsock->b_errno;
         return -1;
      }
      return nbytes;
   }

#ifdef HAVE_TLS
   if (bsock->tls) {
      /* TLS enabled */
      return (tls_bsock_writen(bsock, ptr, nbytes));
   }
#endif /* HAVE_TLS */

   nleft = nbytes;
   while (nleft > 0) {
      do {
         errno = 0;
         nwritten = socketWrite(bsock->m_fd, ptr, nleft);
         if (bsock->is_timed_out() || bsock->is_terminated()) {
            return -1;
         }

#ifdef HAVE_WIN32
         /*
          * For Windows, we must simulate Unix errno on a socket
          *  error in order to handle errors correctly.
          */
         if (nwritten == SOCKET_ERROR) {
            DWORD err = WSAGetLastError();
            nwritten = -1;
            if (err == WSAEINTR) {
               errno = EINTR;
            } else if (err == WSAEWOULDBLOCK) {
               errno = EAGAIN;
            } else {
               errno = EIO;        /* some other error */
            }
         }
#endif

      } while (nwritten == -1 && errno == EINTR);
      /*
       * If connection is non-blocking, we will get EAGAIN, so
       * use select() to keep from consuming all the CPU
       * and try again.
       */
      if (nwritten == -1 && errno == EAGAIN) {
         fd_set fdset;
         struct timeval tv;

         FD_ZERO(&fdset);
         FD_SET((unsigned)bsock->m_fd, &fdset);
         tv.tv_sec = 1;
         tv.tv_usec = 0;
         select(bsock->m_fd + 1, NULL, &fdset, NULL, &tv);
         continue;
      }
      if (nwritten <= 0) {
         return -1;                /* error */
      }
      nleft -= nwritten;
      ptr += nwritten;
      if (bsock->use_bwlimit()) {
         bsock->control_bwlimit(nwritten);
      }
   }
   return nbytes - nleft;
}

/*
 * Establish a TLS connection -- server side
 *  Returns: true  on success
 *           false on failure
 */
#ifdef HAVE_TLS
bool bnet_tls_server(TLS_CONTEXT *ctx, BSOCK * bsock, alist *verify_list)
{
   TLS_CONNECTION *tls;
   JCR *jcr = bsock->jcr();

   tls = new_tls_connection(ctx, bsock->m_fd);
   if (!tls) {
      Qmsg0(bsock->jcr(), M_FATAL, 0, _("TLS connection initialization failed.\n"));
      return false;
   }

   bsock->tls = tls;

   /* Initiate TLS Negotiation */
   if (!tls_bsock_accept(bsock)) {
      Qmsg0(bsock->jcr(), M_FATAL, 0, _("TLS Negotiation failed.\n"));
      goto err;
   }

   if (verify_list) {
      if (!tls_postconnect_verify_cn(jcr, tls, verify_list)) {
         Qmsg1(bsock->jcr(), M_FATAL, 0, _("TLS certificate verification failed."
                                         " Peer certificate did not match a required commonName\n"),
                                         bsock->host());
         goto err;
      }
   }
   Dmsg0(50, "TLS server negotiation established.\n");
   return true;

err:
   free_tls_connection(tls);
   bsock->tls = NULL;
   return false;
}

/*
 * Establish a TLS connection -- client side
 * Returns: true  on success
 *          false on failure
 */
bool bnet_tls_client(TLS_CONTEXT *ctx, BSOCK *bsock, alist *verify_list)
{
   TLS_CONNECTION *tls;
   JCR *jcr = bsock->jcr();

   tls  = new_tls_connection(ctx, bsock->m_fd);
   if (!tls) {
      Qmsg0(bsock->jcr(), M_FATAL, 0, _("TLS connection initialization failed.\n"));
      return false;
   }

   bsock->tls = tls;

   /* Initiate TLS Negotiation */
   if (!tls_bsock_connect(bsock)) {
      goto err;
   }

   /* If there's an Allowed CN verify list, use that to validate the remote
    * certificate's CN. Otherwise, we use standard host/CN matching. */
   if (verify_list) {
      if (!tls_postconnect_verify_cn(jcr, tls, verify_list)) {
         Qmsg1(bsock->jcr(), M_FATAL, 0, _("TLS certificate verification failed."
                                         " Peer certificate did not match a required commonName\n"),
                                         bsock->host());
         goto err;
      }
   } else if (!tls_postconnect_verify_host(jcr, tls, bsock->host())) {
      /* If host is 127.0.0.1, try localhost */
      if (strcmp(bsock->host(), "127.0.0.1") != 0 ||
             !tls_postconnect_verify_host(jcr, tls, "localhost")) {
         Qmsg1(bsock->jcr(), M_FATAL, 0, _("TLS host certificate verification failed. Host name \"%s\" did not match presented certificate\n"),
               bsock->host());
         goto err;
      }
   }
   Dmsg0(50, "TLS client negotiation established.\n");
   return true;

err:
   free_tls_connection(tls);
   bsock->tls = NULL;
   return false;
}
#else

bool bnet_tls_server(TLS_CONTEXT *ctx, BSOCK * bsock, alist *verify_list)
{
   Jmsg(bsock->jcr(), M_ABORT, 0, _("TLS enabled but not configured.\n"));
   return false;
}

bool bnet_tls_client(TLS_CONTEXT *ctx, BSOCK * bsock, alist *verify_list)
{
   Jmsg(bsock->jcr(), M_ABORT, 0, _("TLS enable but not configured.\n"));
   return false;
}

#endif /* HAVE_TLS */

#ifndef NETDB_INTERNAL
#define NETDB_INTERNAL  -1         /* See errno. */
#endif
#ifndef NETDB_SUCCESS
#define NETDB_SUCCESS   0          /* No problem. */
#endif
#ifndef HOST_NOT_FOUND
#define HOST_NOT_FOUND  1          /* Authoritative Answer Host not found. */
#endif
#ifndef TRY_AGAIN
#define TRY_AGAIN       2          /* Non-Authoritative Host not found, or SERVERFAIL. */
#endif
#ifndef NO_RECOVERY
#define NO_RECOVERY     3          /* Non recoverable errors, FORMERR, REFUSED, NOTIMP. */
#endif
#ifndef NO_DATA
#define NO_DATA         4          /* Valid name, no data record of requested type. */
#endif

#if HAVE_GETADDRINFO
const char *resolv_host(int family, const char *host, dlist *addr_list)
{
   int res;
   struct addrinfo hints;
   struct addrinfo *ai, *rp;
   IPADDR *addr;

   memset(&hints, 0, sizeof(struct addrinfo));
   hints.ai_family = family;
   hints.ai_socktype = SOCK_STREAM;
   hints.ai_protocol = IPPROTO_TCP;
   hints.ai_flags = 0;

   res = getaddrinfo(host, NULL, &hints, &ai);
   if (res != 0) {
      return gai_strerror(res);
   }

   for (rp = ai; rp != NULL; rp = rp->ai_next) {
      switch (rp->ai_addr->sa_family) {
      case AF_INET:
         addr = New(IPADDR(rp->ai_addr->sa_family));
         addr->set_type(IPADDR::R_MULTIPLE);
         /*
          * Some serious casting to get the struct in_addr *
          * rp->ai_addr == struct sockaddr
          * as this is AF_INET family we can cast that
          * to struct_sockaddr_in. Of that we need the
          * address of the sin_addr member which contains a
          * struct in_addr
          */
         addr->set_addr4(&(((struct sockaddr_in *)rp->ai_addr)->sin_addr));
         break;
#ifdef HAVE_IPV6
      case AF_INET6:
         addr = New(IPADDR(rp->ai_addr->sa_family));
         addr->set_type(IPADDR::R_MULTIPLE);
         /*
          * Some serious casting to get the struct in6_addr *
          * rp->ai_addr == struct sockaddr
          * as this is AF_INET6 family we can cast that
          * to struct_sockaddr_in6. Of that we need the
          * address of the sin6_addr member which contains a
          * struct in6_addr
          */
         addr->set_addr6(&(((struct sockaddr_in6 *)rp->ai_addr)->sin6_addr));
         break;
#endif
      default:
         continue;
      }
      addr_list->append(addr);
   }
   freeaddrinfo(ai);
   return NULL;
}

#else

/*
 * Get human readable error for gethostbyname()
 */
static const char *gethost_strerror()
{
   const char *msg;
   berrno be;
   switch (h_errno) {
   case NETDB_INTERNAL:
      msg = be.bstrerror();
      break;
   case NETDB_SUCCESS:
      msg = _("No problem.");
      break;
   case HOST_NOT_FOUND:
      msg = _("Authoritative answer for host not found.");
      break;
   case TRY_AGAIN:
      msg = _("Non-authoritative for host not found, or ServerFail.");
      break;
   case NO_RECOVERY:
      msg = _("Non-recoverable errors, FORMERR, REFUSED, or NOTIMP.");
      break;
   case NO_DATA:
      msg = _("Valid name, no data record of resquested type.");
      break;
   default:
      msg = _("Unknown error.");
   }
   return msg;
}

/*
 * Note: this is the old way of resolving a host
 *  that does not use the new getaddrinfo() above.
 */
static const char *resolv_host(int family, const char *host, dlist * addr_list)
{
   struct hostent *hp;
   const char *errmsg;

   P(ip_mutex);                       /* gethostbyname() is not thread safe */
#ifdef HAVE_GETHOSTBYNAME2
   if ((hp = gethostbyname2(host, family)) == NULL) {
#else
   if ((hp = gethostbyname(host)) == NULL) {
#endif
      /* may be the strerror give not the right result -:( */
      errmsg = gethost_strerror();
      V(ip_mutex);
      return errmsg;
   } else {
      char **p;
      for (p = hp->h_addr_list; *p != 0; p++) {
         IPADDR *addr =  New(IPADDR(hp->h_addrtype));
         addr->set_type(IPADDR::R_MULTIPLE);
         if (addr->get_family() == AF_INET) {
             addr->set_addr4((struct in_addr*)*p);
         }
#ifdef HAVE_IPV6
         else {
             addr->set_addr6((struct in6_addr*)*p);
         }
#endif
         addr_list->append(addr);
      }
      V(ip_mutex);
   }
   return NULL;
}
#endif

static IPADDR *add_any(int family)
{
   IPADDR *addr = New(IPADDR(family));
   addr->set_type(IPADDR::R_MULTIPLE);
   addr->set_addr_any();
   return addr;
}

/*
 * i host = 0 mean INADDR_ANY only ipv4
 */
dlist *bnet_host2ipaddrs(const char *host, int family, const char **errstr)
{
   struct in_addr inaddr;
   IPADDR *addr = 0;
   const char *errmsg;
#ifdef HAVE_IPV6
   struct in6_addr inaddr6;
#endif

   dlist *addr_list = New(dlist(addr, &addr->link));
   if (!host || host[0] == '\0') {
      if (family != 0) {
         addr_list->append(add_any(family));
      } else {
         addr_list->append(add_any(AF_INET));
#ifdef HAVE_IPV6
         addr_list->append(add_any(AF_INET6));
#endif
      }
   } else if (inet_aton(host, &inaddr)) { /* MA Bug 4 */
      addr = New(IPADDR(AF_INET));
      addr->set_type(IPADDR::R_MULTIPLE);
      addr->set_addr4(&inaddr);
      addr_list->append(addr);
#ifdef HAVE_IPV6
   } else if (inet_pton(AF_INET6, host, &inaddr6) == 1) {
      addr = New(IPADDR(AF_INET6));
      addr->set_type(IPADDR::R_MULTIPLE);
      addr->set_addr6(&inaddr6);
      addr_list->append(addr);
#endif
   } else {
      if (family != 0) {
         errmsg = resolv_host(family, host, addr_list);
         if (errmsg) {
            *errstr = errmsg;
            free_addresses(addr_list);
            return 0;
         }
      } else {
#ifdef HAVE_IPV6
         /* We try to resolv host for ipv6 and ipv4, the connection procedure
          * will try to reach the host for each protocols. We report only "Host
          * not found" ipv4 message (no need to have ipv6 and ipv4 messages).
          */
         resolv_host(AF_INET6, host, addr_list);
#endif
         errmsg = resolv_host(AF_INET, host, addr_list);

         if (addr_list->size() == 0) {
            *errstr = errmsg;
            free_addresses(addr_list);
            return 0;
         }
      }
   }
   return addr_list;
}

/*
 * Convert a network "signal" code into
 * human readable ASCII.
 */
const char *bnet_sig_to_ascii(BSOCK * bs)
{
   static char buf[30];
   switch (bs->msglen) {
   case BNET_EOD:
      return "BNET_EOD";           /* end of data stream */
   case BNET_EOD_POLL:
      return "BNET_EOD_POLL";
   case BNET_STATUS:
      return "BNET_STATUS";
   case BNET_TERMINATE:
      return "BNET_TERMINATE";     /* terminate connection */
   case BNET_POLL:
      return "BNET_POLL";
   case BNET_HEARTBEAT:
      return "BNET_HEARTBEAT";
   case BNET_HB_RESPONSE:
      return "BNET_HB_RESPONSE";
   case BNET_SUB_PROMPT:
      return "BNET_SUB_PROMPT";
   case BNET_TEXT_INPUT:
      return "BNET_TEXT_INPUT";
   default:
      sprintf(buf, _("Unknown sig %d"), (int)bs->msglen);
      return buf;
   }
}

/* Initialize internal socket structure.
 *  This probably should be done in net_open
 */
BSOCK *init_bsock(JCR * jcr, int sockfd, const char *who, const char *host, int port,
                  struct sockaddr *client_addr)
{
   Dmsg3(100, "who=%s host=%s port=%d\n", who, host, port);
   BSOCK *bsock = (BSOCK *)malloc(sizeof(BSOCK));
   memset(bsock, 0, sizeof(BSOCK));
   bsock->m_fd = sockfd;
   bsock->tls = NULL;
   bsock->errors = 0;
   bsock->m_blocking = 1;
   bsock->msg = get_pool_memory(PM_BSOCK);
   bsock->errmsg = get_pool_memory(PM_MESSAGE);
   bsock->set_who(bstrdup(who));
   bsock->set_host(bstrdup(host));
   bsock->set_port(port);
   memset(&bsock->peer_addr, 0, sizeof(bsock->peer_addr));
   memcpy(&bsock->client_addr, client_addr, sizeof(bsock->client_addr));
   bsock->timeout = BSOCK_TIMEOUT;
   bsock->set_jcr(jcr);
   return bsock;
}

BSOCK *dup_bsock(BSOCK *osock)
{
   BSOCK *bsock = (BSOCK *)malloc(sizeof(BSOCK));
   memcpy(bsock, osock, sizeof(BSOCK));
   bsock->msg = get_pool_memory(PM_BSOCK);
   bsock->errmsg = get_pool_memory(PM_MESSAGE);
   if (osock->who()) {
      bsock->set_who(bstrdup(osock->who()));
   }
   if (osock->host()) {
      bsock->set_host(bstrdup(osock->host()));
   }
   if (osock->src_addr) {
      bsock->src_addr = New( IPADDR( *(osock->src_addr)) );
   }
   bsock->set_duped();
   return bsock;
}

int set_socket_errno(int sockstat)
{
#ifdef HAVE_WIN32
   /*
    * For Windows, we must simulate Unix errno on a socket
    *  error in order to handle errors correctly.
    */
   if (sockstat == SOCKET_ERROR) {
      berrno be;
      DWORD err = WSAGetLastError();
      if (err == WSAEINTR) {
         errno = EINTR;
         return sockstat;
      } else if (err == WSAEWOULDBLOCK) {
         errno = EAGAIN;
         return sockstat;
      } else {
         errno = b_errno_win32 | b_errno_WSA;
      }
      Dmsg2(20, "Socket error: err=%d %s\n", err, be.bstrerror(err));
   }
#else
   if (sockstat == SOCKET_ERROR) {
      /* Handle errrors from prior connections as EAGAIN */
      switch (errno) {
         case ENETDOWN:
         case EPROTO:
         case ENOPROTOOPT:
         case EHOSTDOWN:
#ifdef ENONET
         case ENONET:
#endif
         case EHOSTUNREACH:
         case EOPNOTSUPP:
         case ENETUNREACH:
            errno = EAGAIN;
            break;
         default:
            break;
      }
   }
#endif
   return sockstat;
}
