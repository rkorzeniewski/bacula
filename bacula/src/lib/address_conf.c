/*
 *   Configuration file parser for IP-Addresse ipv4 and ipv6
 *
 *     Written by Meno Abels, June MMIIII
 *
 *     Version $Id$
 */
/*
   Copyright (C) 2004 Kern Sibbald and John Walker

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

IPADDR::IPADDR(const IPADDR &src) : type(src.type)
{
  memcpy(&buf, &src.buf, sizeof(buf));
  saddr  = &buf.dontuse;
  saddr4 = &buf.dontuse4; 
#ifdef HAVE_IPV6
  saddr6 = &buf.dontuse6;
#endif
}

IPADDR::IPADDR(int af) : type(R_EMPTY)
{
  if (!(af  == AF_INET6 || af  == AF_INET)) {
	Emsg1(M_ERROR_TERM, 0, _("Only ipv4 and ipv6 are supported(%d)\n"), af);
  }
  saddr  = &buf.dontuse;
  saddr4 = &buf.dontuse4; 
#ifdef HAVE_IPV6
  saddr6 = &buf.dontuse6;
#endif
  saddr->sa_family = af;
  if (af  == AF_INET) {
 	saddr4->sin_port = 0xffff;
  }
#ifdef HAVE_IPV6
  else {
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

unsigned short IPADDR::get_port() const
{
 unsigned short port = 0;
 if (saddr->sa_family == AF_INET) {
   port = saddr4->sin_port;
 }
#ifdef HAVE_IPV6
 else {
   port = saddr6->sin6_port;
 }
#endif
  return port;
}

void IPADDR::set_port(unsigned short port)
{
 if (saddr->sa_family == AF_INET) {
   saddr4->sin_port = port;
 }
#ifdef HAVE_IPV6
 else {
   saddr6->sin6_port = port;
 }
#endif
}

int IPADDR::get_family() const
{
  return saddr->sa_family;
}	

struct sockaddr *IPADDR::get_sockaddr()
{
  return saddr;
}

int IPADDR::get_sockaddr_len()
{
  return saddr->sa_family == AF_INET ? sizeof(*saddr4) : sizeof(*saddr6);
}
void IPADDR::copy_addr(IPADDR *src)
{
  if (saddr->sa_family == AF_INET) {
    saddr4->sin_addr.s_addr = src->saddr4->sin_addr.s_addr;
  }
#ifdef HAVE_IPV6
  else {
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
  else {
    saddr6->sin6_addr= in6addr_any;
  }
#endif
}

void IPADDR::set_addr4(struct in_addr *ip4)
{ 
  if (saddr->sa_family != AF_INET) {
     Emsg1(M_ERROR_TERM, 0, _("It was tried to assign a ipv6 address to a ipv4(%d)\n"), saddr->sa_family);
  }
  saddr4->sin_addr = *ip4;
}

#ifdef HAVE_IPV6
void IPADDR::set_addr6(struct in6_addr *ip6)
{ 
  if (saddr->sa_family != AF_INET6) {
     Emsg1(M_ERROR_TERM, 0, _("It was tried to assign a ipv4 address to a ipv6(%d)\n"), saddr->sa_family);
  }
  saddr6->sin6_addr = *ip6;
}
#endif

const char *IPADDR::get_address(char *outputbuf, int outlen)
{
   outputbuf[0] = '\0';
#if defined(HAVE_INET_NTOP) && defined(HAVE_IPV6)
   inet_ntop(saddr->sa_family, 
			 saddr->sa_family == AF_INET ?
				 (void*)&(saddr4->sin_addr) : (void*)&(saddr6->sin6_addr),
			 outputbuf,
			 outlen);
#else
   strcpy(outputbuf, inet_ntoa(saddr4->sin_addr));
#endif
   return outputbuf;
}


const char *build_addresses_str(dlist *addrs, char *buf, int blen) 
{
      if (!addrs->size()) {
         bstrncpy(buf, "", blen);
         return buf;
      }
      char *work = buf;
      IPADDR *p;
      foreach_dlist(p, addrs) {
         char tmp[1024];
         int len = snprintf(work, blen, "%s", p->build_address_str(tmp, sizeof(tmp)));
         if (len < 0)
            break;
         work += len;
         blen -= len;
      }
      return buf;
}

const char *get_first_address(dlist * addrs, char *outputbuf, int outlen)
{
   return ((IPADDR *)(addrs->first()))->get_address(outputbuf, outlen);
}

int get_first_port(dlist * addrs)
{
   return ((IPADDR *)(addrs->first()))->get_port();
}

static int skip_to_next_not_eol(LEX * lc)
{
   int token;

   do {
      token = lex_get_token(lc, T_ALL);
   } while (token == T_EOL);
   return token;
}


void init_default_addresses(dlist ** out, int port)
{
   char *errstr;
   unsigned short sport = port;
   if (!add_address(out, IPADDR::R_DEFAULT, htons(sport), AF_INET, 0, 0, &errstr)) {
      Emsg1(M_ERROR_TERM, 0, _("Can't add default address (%s)\n"), errstr);
      free(errstr);
   }
}

int add_address(dlist ** out, IPADDR::i_type type, unsigned short defaultport, int family,
		const char *hostname_str, const char *port_str, char **errstr)
{
   IPADDR *iaddr;
   IPADDR *jaddr;
   dlist *hostaddrs;
   unsigned short port;
   IPADDR::i_type intype = type;

   dlist *addrs = (dlist *) (*(out));
   if (!addrs) {
      IPADDR *tmp = 0;
      addrs = *out = new dlist(tmp, &tmp->link);
   }

   type = (type == IPADDR::R_SINGLE_PORT
	   || type == IPADDR::R_SINGLE_ADDR) ? IPADDR::R_SINGLE : type;
   if (type != IPADDR::R_DEFAULT) {
      IPADDR *def = 0;
      foreach_dlist(iaddr, addrs) {
	 if (iaddr->get_type() == IPADDR::R_DEFAULT) {
	    def = iaddr;
	 } else if (iaddr->get_type() != type) {
	    *errstr = (char *)malloc(1024);
	    bsnprintf(*errstr, 1023,
                      "the old style addresses could mixed with new style");
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
	    *errstr = (char *)malloc(1024);
            bsnprintf(*errstr, 1023, "can't resolve service(%s)", port_str);
	    return 0;
	 }
      }
   }

   const char *myerrstr;
   hostaddrs = bnet_host2ipaddrs(hostname_str, family, &myerrstr);
   if (!hostaddrs) {
      *errstr = (char *)malloc(1024);
      bsnprintf(*errstr, 1023, "can't resolve hostname(%s) %s", hostname_str,
		myerrstr);
      return 0;
   }

   if (intype == IPADDR::R_SINGLE_PORT || intype == IPADDR::R_SINGLE_ADDR) {
      IPADDR *addr;
      if (addrs->size()) {
	 addr = (IPADDR *) addrs->first();
      } else {
	 addr = new IPADDR(family);
	 addr->set_type(type);
	 addr->set_port(defaultport);
	 addr->set_addr_any();
	 addrs->append(addr);
      }
      if (intype == IPADDR::R_SINGLE_PORT) {
	 addr->set_port(port);
      }
      if (intype == IPADDR::R_SINGLE_ADDR) {
	 addr->copy_addr((IPADDR *) (hostaddrs->first()));
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
	       goto skip;	   /* no price */
	    }
	 }
	 clone = new IPADDR(*iaddr);
	 clone->set_type(type);
	 clone->set_port(port);
	 addrs->append(clone);
       skip:
	 continue;
      }
   }
   free_addresses(hostaddrs);
   return 1;
}

void store_addresses(LEX * lc, RES_ITEM * item, int index, int pass)
{
   int token;
   enum { EMPTYLINE = 0, PORTLINE = 0x1, ADDRLINE = 0x2 } next_line = EMPTYLINE;
   int exist;
   char hostname_str[1024];
   char port_str[128];
   int family = 0;

   /*
    *   =  { [[ip|ipv4|ipv6] = { [[addr|port] = [^ ]+[\n;]+] }]+ }
    *	or my tests
    *	positiv
    *	= { ip = { addr = 1.2.3.4; port = 1205; } ipv4 = { addr = 1.2.3.4; port = http; } }
    *	= { ip = { 
    *	      addr = 1.2.3.4; port = 1205; } 
    *	  ipv4 = { 
    *	      addr = 1.2.3.4; port = http; } 
    *	  ipv6 = { 
    *	    addr = 1.2.3.4; 
    *	    port = 1205;
    *	  } 
    *	  ip = {
    *	    addr = 1.2.3.4
    *	    port = 1205
    *	  } 
    *	  ip = {
    *	    addr = 1.2.3.4
    *	  } 
    *	  ip = {
    *	    addr = 2001:220:222::2
    *	  } 
    *	  ip = {
    *	    addr = bluedot.thun.net
    (	  } 
    *	}
    *	negativ
    *	= { ip = { } }
    *	= { ipv4 { addr = doof.nowaytoheavenxyz.uhu; } }
    *	= { ipv4 { port = 4711 } }
    */

   token = skip_to_next_not_eol(lc);
   if (token != T_BOB) {
      scan_err1(lc, _("Expected a block begin { , got: %s"), lc->str);
   }

   token = skip_to_next_not_eol(lc);
   if (token == T_EOB) {
      scan_err0(lc, _("Empty addr block is not allowed"));
   }
   do {
      if (!(token == T_UNQUOTED_STRING || token == T_IDENTIFIER)) {
         scan_err1(lc, _("Expected a string, got: %s"), lc->str);
      }
      if (!strcmp("ip", lc->str) || !strcmp("ipv4", lc->str)) {
	 family = AF_INET;
      }
#ifdef HAVE_IPV6
      else if (!strcmp("ipv6", lc->str)) {
	 family = AF_INET6;
      }
#endif
      else {
         scan_err1(lc, _("Expected a string [ip|ipv4|ipv6], got: %s"), lc->str);
      }
      token = skip_to_next_not_eol(lc);
      if (token != T_EQUALS) {
         scan_err1(lc, _("Expected a equal =, got: %s"), lc->str);
      }
      token = skip_to_next_not_eol(lc);
      if (token != T_BOB) {
         scan_err1(lc, _("Expected a block beginn { , got: %s"), lc->str);
      }
      token = skip_to_next_not_eol(lc);
      exist = EMPTYLINE;
      port_str[0] = hostname_str[0] = '\0';
      do {
	 if (token != T_IDENTIFIER) {
            scan_err1(lc, _("Expected a identifier [addr|port], got: %s"), lc->str);
	 }
         if (!strcmp("port", lc->str)) {
	    next_line = PORTLINE;
	    if (exist & PORTLINE) {
               scan_err0(lc, _("Only one port per address block"));
	    }
	    exist |= PORTLINE;
         } else if (!strcmp("addr", lc->str)) {
	    next_line = ADDRLINE;
	    if (exist & ADDRLINE) {
               scan_err0(lc, _("Only one addr per address block"));
	    }
	    exist |= ADDRLINE;
	 } else {
            scan_err1(lc, _("Expected a identifier [addr|port], got: %s"), lc->str);
	 }
	 token = lex_get_token(lc, T_ALL);
	 if (token != T_EQUALS) {
            scan_err1(lc, _("Expected a equal =, got: %s"), lc->str);
	 }
	 token = lex_get_token(lc, T_ALL);
	 switch (next_line) {
	 case PORTLINE:
	    if (!
		(token == T_UNQUOTED_STRING || token == T_NUMBER
		 || token == T_IDENTIFIER)) {
               scan_err1(lc, _("Expected a number or a string, got: %s"), lc->str);
	    }
	    bstrncpy(port_str, lc->str, sizeof(port_str));
	    break;
	 case ADDRLINE:
	    if (!(token == T_UNQUOTED_STRING || token == T_IDENTIFIER)) {
               scan_err1(lc, _("Expected a ipnumber or a hostname, got: %s"),
			 lc->str);
	    }
	    bstrncpy(hostname_str, lc->str, sizeof(hostname_str));
	    break;
	 case EMPTYLINE:
            scan_err0(lc, _("Statemachine missmatch"));
	    break;
	 }
	 token = skip_to_next_not_eol(lc);
      } while (token == T_IDENTIFIER);
      if (token != T_EOB) {
         scan_err1(lc, _("Expected a end of block }, got: %s"), lc->str);
      }

      char *errstr;
      if (!add_address
	  ((dlist **) (item->value), IPADDR::R_MULTIPLE, htons(item->default_value),
	   family, hostname_str, port_str, &errstr)) {
         scan_err3(lc, _("Can't add hostname(%s) and port(%s) to addrlist (%s)"),
		   hostname_str, port_str, errstr);
	 free(errstr);
      }
      token = skip_to_next_not_eol(lc);
   } while ((token == T_IDENTIFIER || token == T_UNQUOTED_STRING));
   if (token != T_EOB) {
      scan_err1(lc, _("Expected a end of block }, got: %s"), lc->str);
   }
}

void store_addresses_address(LEX * lc, RES_ITEM * item, int index, int pass)
{
   int token = lex_get_token(lc, T_ALL);
   if (!(token == T_UNQUOTED_STRING || token == T_NUMBER || token == T_IDENTIFIER)) {
      scan_err1(lc, _("Expected a hostname or ipnummer, got: %s"), lc->str);
   }
   char *errstr;
   if (!add_address((dlist **) (item->value), IPADDR::R_SINGLE_ADDR,
		    htons(item->default_value), AF_INET, lc->str, 0, &errstr)) {
      scan_err2(lc, _("can't add port (%s) to (%s)"), lc->str, errstr);
      free(errstr);
   }
}

void store_addresses_port(LEX * lc, RES_ITEM * item, int index, int pass)
{
   int token = lex_get_token(lc, T_ALL);
   if (!(token == T_UNQUOTED_STRING || token == T_NUMBER || token == T_IDENTIFIER)) {
      scan_err1(lc, _("Expected a port nummer or string, got: %s"), lc->str);
   }
   char *errstr;
   if (!add_address((dlist **) (item->value), IPADDR::R_SINGLE_PORT,
		    htons(item->default_value), AF_INET, 0, lc->str, &errstr)) {
      scan_err2(lc, _("can't add port (%s) to (%s)"), lc->str, errstr);
      free(errstr);
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




