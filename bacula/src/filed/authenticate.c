/*
 * Authenticate Director who is attempting to connect.
 *
 *   Kern Sibbald, October 2000
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
#include "filed.h"

static char OK_hello[]  = "2000 OK Hello\n";
static char Dir_sorry[] = "2999 No go\n";


/********************************************************************* 
 *
 */
static int authenticate(int rcode, BSOCK *bs)
{
   char *name;
   DIRRES *director;

   if (rcode != R_DIRECTOR) {
      Emsg1(M_FATAL, 0, _("I only authenticate directors, not %d\n"), rcode);
      return 0;
   }
   name = (char *) get_pool_memory(PM_MESSAGE);
   name = (char *) check_pool_memory_size(name, bs->msglen);

   if (sscanf(bs->msg, "Hello Director %s calling\n", name) != 1) {
      free_pool_memory(name);
      Emsg1(M_FATAL, 0, _("Authentication failure: %s"), bs->msg);
      return 0;
   }
   director = NULL;
   LockRes();
   while ((director=(DIRRES *)GetNextRes(rcode, (RES *)director))) {
      if (strcmp(director->hdr.name, name) == 0)
	 break;
   }
   UnlockRes();
   if (director && (!cram_md5_auth(bs, director->password) ||
       !cram_md5_get_auth(bs, director->password))) {
      director = NULL;
   }
   free_pool_memory(name);
   return (director != NULL);
}

/*
 * Inititiate the communications with the Director.
 * He has made a connection to our server.
 * 
 * Basic tasks done here:
 *   We read Director's initial message and authorize him.
 *
 */
int authenticate_director(JCR *jcr)
{
   BSOCK *dir = jcr->dir_bsock;

   if (!authenticate(R_DIRECTOR, dir)) {
      bnet_fsend(dir, "%s", Dir_sorry);
      Emsg0(M_ERROR, 0, _("Unable to authenticate Director\n"));
      return 0;
   }
   return bnet_fsend(dir, "%s", OK_hello);
}

/*
 * First prove our identity to the Storage daemon, then
 * make him prove his identity.
 */
int authenticate_storagedaemon(JCR *jcr)
{
   BSOCK *sd = jcr->store_bsock;

   return cram_md5_get_auth(sd, jcr->sd_auth_key) &&
	  cram_md5_auth(sd, jcr->sd_auth_key);
}
