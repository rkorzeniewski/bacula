/*
 *
 *   Bacula Director -- authorize.c -- handles authorization of
 *     Storage and File daemons.
 *
 *     Kern Sibbald, May MMI
 *
 *    This routine runs as a thread and must be thread reentrant.
 *
 *   Version $Id$
 *
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
#include "dird.h"

extern DIRRES *director;
extern char my_name[];

/* Commands sent to Storage daemon and File daemon and received
 *  from the User Agent */
static char hello[]    = "Hello Director %s calling\n";

/* Response from Storage daemon */
static char OKhello[]   = "3000 OK Hello\n";
static char FDOKhello[] = "2000 OK Hello\n";

/* Sent to User Agent */
static char Dir_sorry[]  = N_("1999 You are not authorized.\n");

/* Forward referenced functions */

/*
 * Authenticate Storage daemon connection
 */
int authenticate_storage_daemon(JCR *jcr)
{
   BSOCK *sd = jcr->store_bsock;
   char dirname[MAX_NAME_LENGTH];
   int ssl_need = BNET_SSL_NONE;

   /* 
    * Send my name to the Storage daemon then do authentication
    */
   bstrncpy(dirname, director->hdr.name, sizeof(dirname));
   bash_spaces(dirname);
   /* Timeout Hello after 5 mins */
   btimer_t *tid = start_bsock_timer(sd, 60 * 5);
   if (!bnet_fsend(sd, hello, dirname)) {
      stop_bsock_timer(tid);
      Jmsg(jcr, M_FATAL, 0, _("Error sending Hello to Storage daemon. ERR=%s\n"), bnet_strerror(sd));
      return 0;
   }
   if (!cram_md5_get_auth(sd, jcr->store->password, ssl_need) || 
       !cram_md5_auth(sd, jcr->store->password, ssl_need)) {
      stop_bsock_timer(tid);
      Jmsg0(jcr, M_FATAL, 0, _("Director and Storage daemon passwords or names not the same.\n"));
      return 0;
   }
   Dmsg1(116, ">stored: %s", sd->msg);
   if (bnet_recv(sd) <= 0) {
      stop_bsock_timer(tid);
      Jmsg1(jcr, M_FATAL, 0, _("bdird<stored: bad response to Hello command: ERR=%s\n"),
	 bnet_strerror(sd));
      return 0;
   }
   Dmsg1(110, "<stored: %s", sd->msg);
   stop_bsock_timer(tid);
   if (strncmp(sd->msg, OKhello, sizeof(OKhello)) != 0) {
      Jmsg0(jcr, M_FATAL, 0, _("Storage daemon rejected Hello command\n"));
      return 0;
   }
   return 1;
}

/*
 * Authenticate File daemon connection
 */
int authenticate_file_daemon(JCR *jcr)
{
   BSOCK *fd = jcr->file_bsock;
   char dirname[MAX_NAME_LENGTH];
   int ssl_need = BNET_SSL_NONE;

   /* 
    * Send my name to the File daemon then do authentication
    */
   bstrncpy(dirname, director->hdr.name, sizeof(dirname));
   bash_spaces(dirname);
   /* Timeout Hello after 5 mins */
   btimer_t *tid = start_bsock_timer(fd, 60 * 5);
   if (!bnet_fsend(fd, hello, dirname)) {
      stop_bsock_timer(tid);
      Jmsg(jcr, M_FATAL, 0, _("Error sending Hello to File daemon. ERR=%s\n"), bnet_strerror(fd));
      return 0;
   }
   if (!cram_md5_get_auth(fd, jcr->client->password, ssl_need) || 
       !cram_md5_auth(fd, jcr->client->password, ssl_need)) {
      stop_bsock_timer(tid);
      Jmsg(jcr, M_FATAL, 0, _("Director and File daemon passwords or names not the same.\n"));
      return 0;
   }
   Dmsg1(116, ">filed: %s", fd->msg);
   if (bnet_recv(fd) <= 0) {
      stop_bsock_timer(tid);
      Jmsg(jcr, M_FATAL, 0, _("Bad response from File daemon to Hello command: ERR=%s\n"),
	 bnet_strerror(fd));
      return 0;
   }
   Dmsg1(110, "<stored: %s", fd->msg);
   stop_bsock_timer(tid);
   if (strncmp(fd->msg, FDOKhello, sizeof(FDOKhello)) != 0) {
      Jmsg(jcr, M_FATAL, 0, _("File daemon rejected Hello command\n"));
      return 0;
   }
   return 1;
}

/********************************************************************* 
 *
 */
int authenticate_user_agent(UAContext *uac) 
{
   char name[MAX_NAME_LENGTH];
   int ssl_need = BNET_SSL_NONE;
   bool ok;    
   BSOCK *ua = uac->UA_sock;

   if (ua->msglen < 16 || ua->msglen >= MAX_NAME_LENGTH + 15) {
      Emsg4(M_ERROR, 0, _("UA Hello from %s:%s:%d is invalid. Len=%d\n"), ua->who,
	    ua->host, ua->port, ua->msglen);
      return 0;
   }

   if (sscanf(ua->msg, "Hello %127s calling\n", name) != 1) {
      ua->msg[100] = 0; 	      /* terminate string */
      Emsg4(M_ERROR, 0, _("UA Hello from %s:%s:%d is invalid. Got: %s\n"), ua->who,
	    ua->host, ua->port, ua->msg);
      return 0;
   }
// Dmsg2(000, "Console=%s addr=%s\n", name, inet_ntoa(ua->client_addr.sin_addr));
   name[sizeof(name)-1] = 0;		 /* terminate name */
   if (strcmp(name, "*UserAgent*") == 0) {  /* default console */
      ok = cram_md5_auth(ua, director->password, ssl_need) &&
	   cram_md5_get_auth(ua, director->password, ssl_need);
   } else {
      unbash_spaces(name); 
      CONRES *cons = (CONRES *)GetResWithName(R_CONSOLE, name);
      if (cons) {
	 ok = cram_md5_auth(ua, cons->password, ssl_need) &&
	      cram_md5_get_auth(ua, cons->password, ssl_need);
	 if (ok) {
	    uac->cons = cons;	      /* save console resource pointer */
	 }
      } else {
	 ok = false;
      }
   }
   if (!ok) {
      bnet_fsend(ua, "%s", _(Dir_sorry));
      Emsg4(M_ERROR, 0, _("Unable to authenticate console \"%s\" at %s:%s:%d.\n"),
	    name, ua->who, ua->host, ua->port);
      sleep(5);
      return 0;
   }
   bnet_fsend(ua, "1000 OK: %s Version: " VERSION " (" BDATE ")\n", my_name);
   return 1;
}
