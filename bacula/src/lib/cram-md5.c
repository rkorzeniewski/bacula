/*
 *  Challenge Response Authentication Method using MD5 (CRAM-MD5)
 *
 * cram-md5 is based on RFC2104.
 * 
 * Written for Bacula by Kern E. Sibbald, May MMI.
 *
 *   Version $Id$
 */
/*
   Copyright (C) 2000, 2001, 2002 Kern Sibbald and John Walker

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston,
   MA 02111-1307, USA.

 */

#include "bacula.h"

/* Authorize other end */
int cram_md5_auth(BSOCK *bs, char *password, int ssl_need)
{
   struct timeval t1;
   struct timeval t2;
   struct timezone tz;
   int i, ok;
   char chal[MAXSTRING];
   char host[MAXSTRING];
   uint8_t hmac[20];

   gettimeofday(&t1, &tz);
   for (i=0; i<4; i++) {
      gettimeofday(&t2, &tz);
   }
   srandom((t1.tv_sec&0xffff) * (t2.tv_usec&0xff));
   if (!gethostname(host, sizeof(host))) {
      bstrncpy(host, my_name, sizeof(host));
   }
   bsnprintf(chal, sizeof(chal), "<%u.%u@%s>", (uint32_t)random(), (uint32_t)time(NULL), host);
   if (!bnet_fsend(bs, "auth cram-md5 %s ssl=%d\n", chal, ssl_need)) {
      return 0;
   }

   if (!bnet_ssl_client(bs, password, ssl_need)) {
      return 0;
   }

   Dmsg1(99, "sent challenge: %s", bs->msg);
   if (bnet_wait_data(bs, 180) <= 0 || bnet_recv(bs) <= 0) {
      bmicrosleep(5, 0);
      return 0;
   }
   hmac_md5((uint8_t *)chal, strlen(chal), (uint8_t *)password, strlen(password), hmac);
   bin_to_base64(host, (char *)hmac, 16);
   ok = strcmp(mp_chr(bs->msg), host) == 0;
   Dmsg3(99, "Authenticate %s: wanted %s, got %s\n", 
              ok ? "OK" : "NOT OK", host, bs->msg);
   if (ok) {
      bnet_fsend(bs, "1000 OK auth\n");
   } else {
      Dmsg1(100, "PW: %s\n", password);
      bnet_fsend(bs, "1999 Authorization failed.\n");
      bmicrosleep(5, 0);
   }
   return ok;
}

/* Get authorization from other end */
int cram_md5_get_auth(BSOCK *bs, char *password, int ssl_need)
{
   char chal[MAXSTRING];
   uint8_t hmac[20];
   int ssl_has; 		      /* This is what the other end has */

   if (bnet_recv(bs) <= 0) {
      bmicrosleep(5, 0);
      return 0;
   }
   if (bs->msglen >= MAXSTRING) {
      Dmsg1(99, "Wanted auth cram... Got: %s", bs->msg);
      bmicrosleep(5, 0);
      return 0;
   }
   if (sscanf(mp_chr(bs->msg), "auth cram-md5 %s ssl=%d\n", chal, &ssl_has) != 2) {
      ssl_has = BNET_SSL_NONE;
      if (sscanf(mp_chr(bs->msg), "auth cram-md5 %s\n", chal) != 1) {
         Dmsg1(100, "Cannot scan challenge: %s\n", bs->msg);
	 bmicrosleep(5, 0);
	 return 0;
      }
   }
   if (!bnet_ssl_server(bs, password, ssl_need, ssl_has)) {
      return 0;
   }

   hmac_md5((uint8_t *)chal, strlen(chal), (uint8_t *)password, strlen(password), hmac);
   bs->msglen = bin_to_base64(mp_chr(bs->msg), (char *)hmac, 16) + 1;
   if (!bnet_send(bs)) {
      Dmsg0(100, "Send response failed.\n");
      return 0;
   }
   Dmsg1(99, "sending resp to challenge: %s\n", bs->msg);
   if (bnet_wait_data(bs, 180) <= 0 || bnet_recv(bs) <= 0) {
      bmicrosleep(5, 0);
      return 0;
   }
   if (strcmp(mp_chr(bs->msg), "1000 OK auth\n") == 0) {
      return 1;
   }
   Dmsg1(100, "PW: %s\n", password);
   bmicrosleep(5, 0);
   return 0;
}
