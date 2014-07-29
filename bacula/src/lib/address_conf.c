/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2004-2014 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from many
   others, a complete list can be found in the file AUTHORS.

   You may use this file and others of this release according to the
   license defined in the LICENSE file, which includes the Affero General
   Public License, v3.0 ("AGPLv3") and some additional permissions and
   terms pursuant to its AGPLv3 Section 7.

   Bacula® is a registered trademark of Kern Sibbald.
*/
/*
 *   Configuration file parser for IP-Addresse ipv4 and ipv6
 *
 *     Written by Meno Abels, June MMIV
 *
 */


#include "bacula.h"
#ifdef HAVE_ARPA_NAMESER_H
#include <arpa/nameser.h>
#endif
#ifdef HAVE_RESOLV_H
//#include <resolv.h>
#endif

static int add_address(dlist **out, IPADDR::i_type type, unsigned short defaultport, int family,
                const char *hostname_str, const char *port_str, char *buf, int buflen);


IPADDR::IPADDR(const IPADDR &src) : type(src.type)
{
  memcpy(&saddrbuf, &src.saddrbuf, sizeof(saddrbuf));
  saddr  = &saddrbuf.dontuse;
  saddr4 = &saddrbuf.dontuse4;
#ifdef HAVE_IPV6
  saddr6 = &saddrbuf.dontuse6;
#endif
}

IPADDR::IPADDR(int af) : type(R_EMPTY)
{
#ifdef HAVE_IPV6
  if (!(af == AF_INET6 || af == AF_INET)) {
     Emsg1(M_ERROR_TERM, 0, _("Only ipv4 and ipv6 are supported (%d)\n"), af);
  }
#else
  if (af != AF_INET) {
     Emsg1(M_ERROR_TERM, 0, _("Only ipv4 is supported (%d)\n"), af);
  }
#endif
  memset(&saddrbuf, 0, sizeof(saddrbuf));
  saddr  = &saddrbuf.dontuse;
  saddr4 = &saddrbuf.dontuse4;
#ifdef HAVE_IPV6
  saddr6 = &saddrbuf.dontuse6;
#endif
  saddr->sa_family = af;
  if (af  == AF_INET) {
     saddr4->sin_port = 0xffff;
  }
#ifdef HAVE_IPV6
  else if (af  == AF_INET6) {
     saddr6->sin6_port = 0xffff;
  }
#endif
#ifdef HAVE_SA_LEN
#ifdef HAVE_IPV6
  saddr->sa_len = (af == AF_INET) ? sizeof(sockaddr_in) : sizeof(sockaddr_in6);
#else
  saddr->sa_len = sizeof(sockaddr_in);
#endif
#endif
   set_addr_any();
}

void IPADDR::set_type(i_type o)
{
   type = o;
}

IPADDR::i_type IPADDR::get_type() const
{
   return type;
}

unsigned short IPADDR::get_port_net_order() const
{
   unsigned short port = 0;
   if (saddr->sa_family == AF_INET) {
      port = saddr4->sin_port;
   }
#ifdef HAVE_IPV6
   else if (saddr->sa_family == AF_INET6) {
      port = saddr6->sin6_port;
   }
#endif
   return port;
}

void IPADDR::set_port_net(unsigned short port)
{
   if (saddr->sa_family == AF_INET) {
      saddr4->sin_port = port;
   }
#ifdef HAVE_IPV6
   else if (saddr->sa_family == AF_INET6) {
      saddr6->sin6_port = port;
   }
#endif
}

int IPADDR::get_family() const
{
    return saddr->sa_family;
}

/*
 * Note, this returns the address of the socket structure
 *  not the address of the socket address.
 *  This socket address is a union of the different types
 *  of sockets (IPv4, ...) available, so it is portable.
 */
struct sockaddr *IPADDR::get_sockaddr()
{
   return saddr;
}

int IPADDR::get_sockaddr_len()
{
#ifdef HAVE_IPV6
   return saddr->sa_family == AF_INET ? sizeof(*saddr4) : sizeof(*saddr6);
#else
   return sizeof(*saddr4);
#endif
}
void IPADDR::copy_addr(IPADDR *src)
{
   if (saddr->sa_family == AF_INET) {
      saddr4->sin_addr.s_addr = src->saddr4->sin_addr.s_addr;
   }
#ifdef HAVE_IPV6
   else if (saddr->sa_family == AF_INET6) {
      saddr6->sin6_addr = src->saddr6->sin6_addr;
   }
#endif
}

void IPADDR::set_addr_any()
{
   if (saddr->sa_family == AF_INET) {
      saddr4->sin_addr.s_addr = INADDR_ANY;
   }
#ifdef HAVE_IPV6
   else if (saddr->sa_family == AF_INET6) {
     saddr6->sin6_addr = in6addr_any;
   }
#endif
}

void IPADDR::set_addr4(struct in_addr *ip4)
{
   if (saddr->sa_family != AF_INET) {
      Emsg1(M_ERROR_TERM, 0, _("You tried to assign a ipv6 address to an ipv4(%d)\n"), saddr->sa_family);
   }
   saddr4->sin_addr = *ip4;
}

#ifdef HAVE_IPV6
void IPADDR::set_addr6(struct in6_addr *ip6)
{
   if (saddr->sa_family != AF_INET6) {
      Emsg1(M_ERROR_TERM, 0, _("You tried to assign an ipv4 address to an ipv6(%d)\n"), saddr->sa_family);
   }
   saddr6->sin6_addr = *ip6;
}
#endif

const char *IPADDR::get_address(char *outputbuf, int outlen)
{
   outputbuf[0] = '\0';
#ifdef HAVE_INET_NTOP
# ifdef HAVE_IPV6
   inet_ntop(saddr->sa_family, saddr->sa_family == AF_INET ?
              (void*)&(saddr4->sin_addr) : (void*)&(saddr6->sin6_addr),
              outputbuf, outlen);
# else
   inet_ntop(saddr->sa_family, (void*)&(saddr4->sin_addr), outputbuf, outlen);
# endif
#else
   bstrncpy(outputbuf, inet_ntoa(saddr4->sin_addr), outlen);
#endif
   return outputbuf;
}

const char *IPADDR::build_address_str(char *buf, int blen)
{
   char tmp[1024];
   *buf = 0;
   if (get_family() == AF_INET) {
      bsnprintf(buf, blen, "%s:%hu ",
                get_address(tmp, sizeof(tmp) - 1), get_port_host_order());
   } else if (get_family() == AF_INET6) {
      bsnprintf(buf, blen, "[%s]:%hu ",
                get_address(tmp, sizeof(tmp) - 1), get_port_host_order());
   }
   return buf;
}

const char *build_addresses_str(dlist *addrs, char *buf, int blen)
{
   if (!addrs || addrs->size() == 0) {
      bstrncpy(buf, "", blen);
      return buf;
   }
   char *work = buf;
   IPADDR *p;
   foreach_dlist(p, addrs) {
      char tmp[1024];
      int len = bsnprintf(work, blen, "%s", p->build_address_str(tmp, sizeof(tmp)));
      if (len < 0)
         break;
      work += len;
      blen -= len;
   }
   return buf;
}

const char *get_first_address(dlist *addrs, char *outputbuf, int outlen)
{
   return ((IPADDR *)(addrs->first()))->get_address(outputbuf, outlen);
}

int get_first_port_net_order(dlist *addrs)
{
   if (!addrs) {
      return 0;
   } else {
      return ((IPADDR *)(addrs->first()))->get_port_net_order();
   }
}

int get_first_port_host_order(dlist *addrs)
{
   if (!addrs) {
      return 0;
   } else {
      return ((IPADDR *)(addrs->first()))->get_port_host_order();
   }
}

void init_default_addresses(dlist **addr_list, int port)
{
   char buf[1024];
   unsigned short sport = port;
   if (!add_address(addr_list, IPADDR::R_DEFAULT, htons(sport), AF_INET, 0, 0, buf, sizeof(buf))) {
      Emsg1(M_ERROR_TERM, 0, _("Can't add default IPv4 address (%s)\n"), buf);
   }
   Dmsg1(20, "Initaddr %s\n", build_addresses_str(*addr_list, buf, sizeof(buf)));

}

static int add_address(dlist **out, IPADDR::i_type type, unsigned short defaultport, int family,
                const char *hostname_str, const char *port_str, char *buf, int buflen)
{
   IPADDR *iaddr;
   IPADDR *jaddr;
   dlist *hostaddrs;
   unsigned short port;
   IPADDR::i_type intype = type;

   buf[0] = 0;
   dlist *addrs = (dlist *)(*(out));
   if (!addrs) {
      IPADDR *tmp = 0;
      addrs = *out = New(dlist(tmp, &tmp->link));
   }

   type = (type == IPADDR::R_SINGLE_PORT ||
           type == IPADDR::R_SINGLE_ADDR) ? IPADDR::R_SINGLE : type;
   if (type != IPADDR::R_DEFAULT) {
      IPADDR *def = 0;
      foreach_dlist(iaddr, addrs) {
         if (iaddr->get_type() == IPADDR::R_DEFAULT) {
            def = iaddr;
         } else if (iaddr->get_type() != type) {
            bsnprintf(buf, buflen,
                      _("Old style addresses cannot be mixed with new style. Try removing Port=nnn."));
            Dmsg1(20, "%s\n", buf);
            return 0;
         }
      }
      if (def) {
         addrs->remove(def);
         delete def;
      }
   }

   if (!port_str || port_str[0] == '\0') {
      port = defaultport;
   } else {
      int pnum = atol(port_str);
      if (0 < pnum && pnum < 0xffff) {
         port = htons(pnum);
      } else {
         struct servent *s = getservbyname(port_str, "tcp");
         if (s) {
            port = s->s_port;
         } else {
            bsnprintf(buf, buflen, _("Cannot resolve service(%s)"), port_str);
            Dmsg1(20, "%s\n", buf);
            return 0;
         }
      }
   }

   const char *myerrstr;
   hostaddrs = bnet_host2ipaddrs(hostname_str, family, &myerrstr);
   if (!hostaddrs) {
      bsnprintf(buf, buflen, _("Cannot resolve hostname(%s) %s"), hostname_str,
                myerrstr);
      Dmsg1(20, "%s\n", buf);
      return 0;
   }

   if (intype == IPADDR::R_SINGLE_PORT || intype == IPADDR::R_SINGLE_ADDR) {
      IPADDR *addr;
      if (addrs->size()) {
         addr = (IPADDR *)addrs->first();
      } else {
         addr = New(IPADDR(family));
         addr->set_type(type);
         addr->set_port_net(defaultport);
         addr->set_addr_any();
         addrs->append(addr);
      }
      if (intype == IPADDR::R_SINGLE_PORT) {
         addr->set_port_net(port);
      }
      if (intype == IPADDR::R_SINGLE_ADDR) {
         addr->copy_addr((IPADDR *)(hostaddrs->first()));
      }
   } else {
      foreach_dlist(iaddr, hostaddrs) {
         IPADDR *clone;
         /* for duplicates */
         foreach_dlist(jaddr, addrs) {
            if (iaddr->get_sockaddr_len() == jaddr->get_sockaddr_len() &&
                !memcmp(iaddr->get_sockaddr(), jaddr->get_sockaddr(),
                    iaddr->get_sockaddr_len()))
                {
               goto skip;          /* no price */
            }
         }
         clone = New(IPADDR(*iaddr));
         clone->set_type(type);
         clone->set_port_net(port);
         addrs->append(clone);
       skip:
         continue;
      }
   }
   free_addresses(hostaddrs);
   return 1;
}

/*
 *  Some IPv6 rules from Wikipedia:
 *
 *  For convenience, an IPv6 address may be abbreviated to shorter
 *  notations by application of the following rules, where possible.
 *
 * 1.  One or more leading zeroes from any groups of hexadecimal
 *     digits are removed; this is usually done to either all or none of
 *     the leading zeroes.  For example, the group 0042 is converted to
 *     42.
 *
 * 2.  Consecutive sections of zeroes are replaced with a double
 *     colon (::).  The double colon may only be used once in an
 *     address, as multiple use would render the address indeterminate.
 *     RFC 5952 recommends that a double colon must not be used to
 *     denote an omitted single section of zeroes.[39]
 *
 *   my tests
 *   positiv
 *   = { ip = { addr = 1.2.3.4; port = 1205; } ipv4 = { addr = 1.2.3.4; port = http; } }
 *   = { ip = {
 *         addr = 1.2.3.4; port = 1205; }
 *     ipv4 = {
 *         addr = 1.2.3.4; port = http; }
 *     ipv6 = {
 *       addr = 1.2.3.4;
 *       port = 1205;
 *     }
 *     ip = {
 *       addr = 1.2.3.4
 *       port = 1205
 *     }
 *     ip = {
 *       addr = 1.2.3.4
 *     }
 *     ip = {
 *       addr = 2001:220:222::2
 *     }
 *     ip = {
 *       addr = bluedot.thun.net
 *     }
 *   }
 *
 *   negativ
 *   = { ip = { } }
 *   = { ipv4 { addr = doof.nowaytoheavenxyz.uhu; } }
 *   = { ipv4 { port = 4711 } }
 */
void store_addresses(LEX * lc, RES_ITEM * item, int index, int pass)
{
   int token;
   enum { EMPTYLINE = 0, PORTLINE = 0x1, ADDRLINE = 0x2 } next_line = EMPTYLINE;
   int exist;
   char hostname_str[1024];
   char port_str[128];
   int family = 0;
   char errmsg[1024];


   token = lex_get_token(lc, T_SKIP_EOL);
   if (token != T_BOB) {
      scan_err1(lc, _("Expected a block to begin with { but got: %s"), lc->str);
   }

   token = lex_get_token(lc, T_SKIP_EOL);
   if (token == T_EOB) {
      scan_err0(lc, _("Empty addr block is not allowed"));
   }
   do {
      if (!(token == T_UNQUOTED_STRING || token == T_IDENTIFIER)) {
         scan_err1(lc, _("Expected a string but got: %s"), lc->str);
      }
      if (strcasecmp("ip", lc->str) == 0) {
         family = AF_INET6;
      } else if (strcasecmp("ipv4", lc->str) == 0) {
         family = AF_INET;
      }
#ifdef HAVE_IPV6
      else if (strcasecmp("ipv6", lc->str) == 0) {
         family = AF_INET6;
      } else {
         scan_err1(lc, _("Expected a string [ip|ipv4|ipv6] but got: %s"), lc->str);
      }
#else
      else {
         scan_err1(lc, _("Expected a string [ip|ipv4] but got: %s"), lc->str);
      }
#endif
      token = lex_get_token(lc, T_SKIP_EOL);
      if (token != T_EQUALS) {
         scan_err1(lc, _("Expected an equal = but got: %s"), lc->str);
      }
      token = lex_get_token(lc, T_SKIP_EOL);
      if (token != T_BOB) {
         scan_err1(lc, _("Expected a block to begin with { but got: %s"), lc->str);
      }
      token = lex_get_token(lc, T_SKIP_EOL);
      exist = EMPTYLINE;
      port_str[0] = hostname_str[0] = '\0';
      do {
         if (token != T_IDENTIFIER) {
            scan_err1(lc, _("Expected an identifier [addr|port] but got: %s"), lc->str);
         }
         if (strcasecmp("port", lc->str) == 0) {
            next_line = PORTLINE;
            if (exist & PORTLINE) {
               scan_err0(lc, _("Only one port per address block"));
            }
            exist |= PORTLINE;
         } else if (strcasecmp("addr", lc->str) == 0) {
            next_line = ADDRLINE;
            if (exist & ADDRLINE) {
               scan_err0(lc, _("Only one addr per address block"));
            }
            exist |= ADDRLINE;
         } else {
            scan_err1(lc, _("Expected a identifier [addr|port] but got: %s"), lc->str);
         }
         token = lex_get_token(lc, T_SKIP_EOL);
         if (token != T_EQUALS) {
            scan_err1(lc, _("Expected a equal =, got: %s"), lc->str);
         }
         token = lex_get_token(lc, T_SKIP_EOL);
         switch (next_line) {
         case PORTLINE:
            if (!
                (token == T_UNQUOTED_STRING || token == T_NUMBER ||
                 token == T_IDENTIFIER)) {
               scan_err1(lc, _("Expected a number or a string but got: %s"), lc->str);
            }
            bstrncpy(port_str, lc->str, sizeof(port_str));
            break;
         case ADDRLINE:
            if (!(token == T_UNQUOTED_STRING || token == T_IDENTIFIER)) {
               scan_err1(lc, _("Expected an IP number or a hostname but got: %s"),
                         lc->str);
            }
            bstrncpy(hostname_str, lc->str, sizeof(hostname_str));
            break;
         case EMPTYLINE:
            scan_err0(lc, _("State machine missmatch"));
            break;
         }
         token = lex_get_token(lc, T_SKIP_EOL);
      } while (token == T_IDENTIFIER);
      if (token != T_EOB) {
         scan_err1(lc, _("Expected a end of block with } but got: %s"), lc->str);
      }

      if (pass == 1 && !add_address((dlist **)(item->value), IPADDR::R_MULTIPLE,
               htons(item->default_value), family, hostname_str, port_str,
               errmsg, sizeof(errmsg))) {
         scan_err3(lc, _("Cannot add hostname(%s) and port(%s) to addrlist (%s)"),
                   hostname_str, port_str, errmsg);
      }
      token = scan_to_next_not_eol(lc);
   } while ((token == T_IDENTIFIER || token == T_UNQUOTED_STRING));
   if (token != T_EOB) {
      scan_err1(lc, _("Expected an end of block with } but got: %s"), lc->str);
   }
}

void store_addresses_address(LEX * lc, RES_ITEM * item, int index, int pass)
{
   char errmsg[1024];
   int token = lex_get_token(lc, T_SKIP_EOL);
   if (!(token == T_UNQUOTED_STRING || token == T_NUMBER || token == T_IDENTIFIER)) {
      scan_err1(lc, _("Expected an IP number or a hostname, got: %s"), lc->str);
   }
   if (pass == 1 && !add_address((dlist **)(item->value), IPADDR::R_SINGLE_ADDR,
                    htons(item->default_value), AF_INET, lc->str, 0,
                    errmsg, sizeof(errmsg))) {
      scan_err2(lc, _("Cannot add port (%s) to (%s)"), lc->str, errmsg);
   }
}

void store_addresses_port(LEX * lc, RES_ITEM * item, int index, int pass)
{
   char errmsg[1024];
   int token = lex_get_token(lc, T_SKIP_EOL);
   if (!(token == T_UNQUOTED_STRING || token == T_NUMBER || token == T_IDENTIFIER)) {
      scan_err1(lc, _("Expected a port number or string, got: %s"), lc->str);
   }
   if (pass == 1 && !add_address((dlist **)(item->value), IPADDR::R_SINGLE_PORT,
                    htons(item->default_value), AF_INET, 0, lc->str,
                    errmsg, sizeof(errmsg))) {
      scan_err2(lc, _("Cannot add port (%s) to (%s)"), lc->str, errmsg);
   }
}

void free_addresses(dlist * addrs)
{
   while (!addrs->empty()) {
      IPADDR *ptr = (IPADDR*)addrs->first();
      addrs->remove(ptr);
      delete ptr;
   }
   delete addrs;
}

int sockaddr_get_port_net_order(const struct sockaddr *client_addr)
{
   if (client_addr->sa_family == AF_INET) {
      return ((struct sockaddr_in *)client_addr)->sin_port;
   }
#ifdef HAVE_IPV6
   else if (client_addr->sa_family == AF_INET6) {
      return ((struct sockaddr_in6 *)client_addr)->sin6_port;
   }
#endif
   return -1;
}

int sockaddr_get_port(const struct sockaddr *client_addr)
{
   if (client_addr->sa_family == AF_INET) {
      return ntohs(((struct sockaddr_in *)client_addr)->sin_port);
   }
#ifdef HAVE_IPV6
   else if (client_addr->sa_family == AF_INET6) {
      return ntohs(((struct sockaddr_in6 *)client_addr)->sin6_port);
   }
#endif
   return -1;
}


char *sockaddr_to_ascii(const struct sockaddr *sa, int socklen, char *buf, int buflen)
{
#ifdef HAVE_GETNAMEINFO
  /* This is the more modern way of doing it */
  char clienthost[NI_MAXHOST];
  char clientservice[NI_MAXSERV];
  int status;
  status = getnameinfo(sa, socklen, clienthost, sizeof(clienthost),
                       clientservice, sizeof(clientservice),
                       NI_NUMERICHOST | NI_NUMERICSERV);
  if (status == 0) {
     /* Enclose IPv6 in [] */
     if (strchr(clienthost, ':') != NULL) {
        bsnprintf(buf, buflen, "[%s]", clienthost);
     } else {
        bstrncpy(buf, clienthost, buflen);
     }
  } else {
     bstrncpy(buf, "Hostname not found", buflen);
  }

#else
#ifdef HAVE_INET_NTOP
   inet_ntop(sa->sa_family,
# ifdef HAVE_IPV6
             sa->sa_family == AF_INET ?
                 (void*)&(((struct sockaddr_in*)sa)->sin_addr) :
                 (void*)&(((struct sockaddr_in6*)sa)->sin6_addr),
# else
                 (void*)&(((struct sockaddr_in*)sa)->sin_addr),
# endif /* HAVE_IPV6 */
             buf, buflen);
#else
   bstrncpy(buf, inet_ntoa(((struct sockaddr_in *)sa)->sin_addr), buflen);
#endif
#endif
   return buf;
}

/*
 * Remove duplicate IP addresses.
 */
void remove_duplicate_addresses(dlist *addr_list)
{
   IPADDR *ipaddr, *next, *duplicate;
   /*
    * Remove any duplicate addresses.
    */
   for (ipaddr = (IPADDR *)addr_list->first(); ipaddr;
        ipaddr = (IPADDR *)addr_list->next(ipaddr)) {
      for (next = (IPADDR *)addr_list->next(ipaddr); next; ) {
         duplicate = NULL;
         if (ipaddr->get_sockaddr_len() == next->get_sockaddr_len() &&
             memcmp(ipaddr->get_sockaddr(), next->get_sockaddr(),
                    ipaddr->get_sockaddr_len()) == 0) {
            duplicate = next;
         }
         next = (IPADDR *)addr_list->next(next);
         if (duplicate) {
            addr_list->remove(duplicate);  /* remove from list */
            delete duplicate;              /* free it */
         }
      }
   }
}

#ifdef HAVE_OLD_SOCKOPT
int inet_aton(const char *cp, struct in_addr *inp)
{
   struct in_addr inaddr;

   if((inaddr.s_addr = inet_addr(cp)) != INADDR_NONE) {
      inp->s_addr = inaddr.s_addr;
      return 1;
   }
   return 0;
}
#endif
