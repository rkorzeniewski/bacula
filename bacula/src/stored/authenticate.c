/*
 * Authenticate caller
 *
 *   Kern Sibbald, October 2000
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
#include "stored.h"

static char Dir_sorry[] = "3999 No go\n";
static char OK_hello[]  = "3000 OK Hello\n";


/********************************************************************* 
 *
 *
 */
static int authenticate(int rcode, BSOCK *bs)
{
   POOLMEM *dirname;
   DIRRES *director = NULL;
   int ssl_need = BNET_SSL_NONE;

   if (rcode != R_DIRECTOR) {
      Emsg1(M_FATAL, 0, _("I only authenticate Directors, not %d\n"), rcode);
      return 0;
   }
   if (bs->msglen < 25 || bs->msglen > 200) {
      Emsg2(M_FATAL, 0, _("Bad Hello command from Director at %s. Len=%d.\n"), 
	    bs->who, bs->msglen);
      return 0;
   }
   dirname = get_pool_memory(PM_MESSAGE);
   dirname = check_pool_memory_size(dirname, bs->msglen);

   if (sscanf(bs->msg, "Hello Director %127s calling\n", dirname) != 1) {
      bs->msg[100] = 0;
      Emsg2(M_FATAL, 0, _("Bad Hello command from Director at %s: %s\n"), 
	    bs->who, bs->msg);
      return 0;
   }
   director = NULL;
   unbash_spaces(dirname);
   LockRes();
   while ((director=(DIRRES *)GetNextRes(rcode, (RES *)director))) {
      if (strcmp(director->hdr.name, dirname) == 0)
	 break;
   }
   UnlockRes();
   if (!director) {
      Emsg2(M_FATAL, 0, _("Connection from unknown Director %s at %s rejected.\n"), 
	    dirname, bs->who);
      goto bail_out;
   }
   if (!cram_md5_auth(bs, director->password, ssl_need) ||
       !cram_md5_get_auth(bs, director->password, ssl_need)) {
      Emsg0(M_FATAL, 0, _("Incorrect password given by Director.\n"));
      goto bail_out;
   }
   free_pool_memory(dirname);
   return 1;

bail_out:
   free_pool_memory(dirname);
   return 0;
}

/*
 * Inititiate the message channel with the Director.
 * He has made a connection to our server.
 * 
 * Basic tasks done here:
 *   Assume the Hello message is already in the input
 *     buffer.	We then authenticate him.
 *   Get device, media, and pool information from Director
 *
 *   This is the channel across which we will send error
 *     messages and job status information.
 */
int authenticate_director(JCR *jcr)
{
   BSOCK *dir = jcr->dir_bsock;

   if (!authenticate(R_DIRECTOR, dir)) {
      bnet_fsend(dir, "%s", Dir_sorry);
      Emsg1(M_ERROR, 0, _("Unable to authenticate Director at %s.\n"), dir->who);
      bmicrosleep(5, 0);
      return 0;
   }
   return bnet_fsend(dir, "%s", OK_hello);
}

int authenticate_filed(JCR *jcr)
{
   BSOCK *fd = jcr->file_bsock;
   int ssl_need = BNET_SSL_NONE;

   if (cram_md5_auth(fd, jcr->sd_auth_key, ssl_need) &&
       cram_md5_get_auth(fd, jcr->sd_auth_key, ssl_need)) {
      jcr->authenticated = TRUE;
   }
   if (!jcr->authenticated) {
      Jmsg(jcr, M_FATAL, 0, _("Incorrect authorization key from File daemon at %s rejected.\n"), 
	   fd->who);
   }
   return jcr->authenticated;
}
