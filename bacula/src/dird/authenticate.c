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

   /* 
    * Send my name to the Storage daemon then do authentication
    */
   strcpy(dirname, director->hdr.name);
   bash_spaces(dirname);
   if (!bnet_fsend(sd, hello, dirname)) {
      Jmsg(jcr, M_FATAL, 0, _("Error sending Hello to Storage daemon. ERR=%s\n"), bnet_strerror(sd));
      return 0;
   }
   if (!cram_md5_get_auth(sd, jcr->store->password) || 
       !cram_md5_auth(sd, jcr->store->password)) {
      Jmsg0(jcr, M_FATAL, 0, _("Director and Storage daemon passwords not the same.\n"));
      return 0;
   }
   Dmsg1(116, ">stored: %s", sd->msg);
   if (bnet_recv(sd) <= 0) {
      Emsg1(M_FATAL, 0, _("bdird<stored: bad response to Hello command: ERR=%s\n"),
	 bnet_strerror(sd));
      return 0;
   }
   Dmsg1(110, "<stored: %s", sd->msg);
   if (strncmp(sd->msg, OKhello, sizeof(OKhello)) != 0) {
      Emsg0(M_FATAL, 0, _("Storage daemon rejected Hello command\n"));
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

   /* 
    * Send my name to the File daemon then do authentication
    */
   strcpy(dirname, director->hdr.name);
   bash_spaces(dirname);
   if (!bnet_fsend(fd, hello, director->hdr.name)) {
      Jmsg(jcr, M_FATAL, 0, _("Error sending Hello to File daemon. ERR=%s\n"), bnet_strerror(fd));
      return 0;
   }
   if (!cram_md5_get_auth(fd, jcr->client->password) || 
       !cram_md5_auth(fd, jcr->client->password)) {
      Jmsg(jcr, M_FATAL, 0, _("Director and File daemon passwords not the same.\n"));
      return 0;
   }
   Dmsg1(116, ">filed: %s", fd->msg);
   if (bnet_recv(fd) <= 0) {
      Jmsg(jcr, M_FATAL, 0, _("bdird<filed: bad response to Hello command: ERR=%s\n"),
	 bnet_strerror(fd));
      return 0;
   }
   Dmsg1(110, "<stored: %s", fd->msg);
   if (strncmp(fd->msg, FDOKhello, sizeof(FDOKhello)) != 0) {
      Jmsg(jcr, M_FATAL, 0, _("File daemon rejected Hello command\n"));
      return 0;
   }
   return 1;
}

/********************************************************************* 
 *
 */
int authenticate_user_agent(BSOCK *ua)
{
   char name[128];
   int ok = 0;


   if (sscanf(ua->msg, "Hello %127s calling\n", name) != 1) {
      Emsg1(M_FATAL, 0, _("Authentication failure: %s"), ua->msg);
      return 0;
   }

   ok = cram_md5_auth(ua, director->password) &&
	cram_md5_get_auth(ua, director->password);

   if (!ok) {
      bnet_fsend(ua, "%s", _(Dir_sorry));
      Emsg0(M_WARNING, 0, _("Unable to authenticate User Agent\n"));
      sleep(5);
      return 0;
   }
   bnet_fsend(ua, "1000 OK: %s Version: " VERSION " (" DATE ")\n", my_name);
   return 1;
}
