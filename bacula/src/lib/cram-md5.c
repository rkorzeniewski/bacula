/*
 *  Challenge Response Authentication Method using MD5 (CRAM-MD5)
 *
 * cram-md5 is based on RFC2104.
 * 
 * Written for Bacula by Kern E. Sibbald, May MMI.
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

int cram_md5_auth(BSOCK *bs, char *password)
{
   struct timeval t1;
   struct timeval t2;
   struct timezone tz;
   int i, ok;
   char chal[MAXSTRING];
   char host[MAXSTRING];
   uint8_t hmac[20];

   gettimeofday(&t1, &tz);
   for (i=0; i<4; i++)
      gettimeofday(&t2, &tz);
   srandom((t1.tv_sec&0xffff) * (t2.tv_usec&0xff));
   gethostname(host, sizeof(host));
   sprintf((char *)chal, "<%u.%u@%s>", (uint32_t)random(), (uint32_t)time(NULL), host);
   if (!bnet_fsend(bs, "auth cram-md5 %s\n", chal)) {
      return 0;
   }
   Dmsg1(99, "%s", bs->msg);
   if (bnet_wait_data(bs, 120) <= 0 || bnet_recv(bs) <= 0) {
      sleep(5);
      return 0;
   }
   hmac_md5((uint8_t *)chal, strlen(chal), (uint8_t *)password, strlen(password), hmac);
   bin_to_base64(host, (char *)hmac, 16);
   ok = strcmp(bs->msg, host) == 0;
   if (ok) {
      Dmsg3(399, "Authenticate %s: wanted %s, got %s\n", 
            ok ? "OK" : "NOT OK", host, bs->msg);
   } else {
      Dmsg3(99, "Authenticate %s: wanted %s, got %s\n", 
            ok ? "OK" : "NOT OK", host, bs->msg);
   }
   if (ok) {
      bnet_fsend(bs, "1000 OK auth\n");
   } else {
      bnet_fsend(bs, "1999 No auth\n");
      sleep(5);
   }
   return ok;
}

int cram_md5_get_auth(BSOCK *bs, char *password)
{
   char chal[MAXSTRING];
   uint8_t hmac[20];

   if (bnet_recv(bs) <= 0) {
      sleep(5);
      return 0;
   }
   if (sscanf(bs->msg, "auth cram-md5 %s", chal) != 1) {
     sleep(5);
     return 0;
   }
   hmac_md5((uint8_t *)chal, strlen(chal), (uint8_t *)password, strlen(password), hmac);
   bs->msglen = bin_to_base64(bs->msg, (char *)hmac, 16);
   if (!bnet_send(bs)) {
      return 0;
   }
   Dmsg1(99, "sending resp to challenge: %s\n", bs->msg);
   if (bnet_wait_data(bs, 120) <= 0 || bnet_recv(bs) <= 0) {
      sleep(5);
      return 0;
   }
   if (strcmp(bs->msg, "1000 OK auth\n") == 0) {
      return 1;
   }
   sleep(5);
   return 0;
}
