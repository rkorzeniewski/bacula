/*
 *
 *   Bacula UA authentication. Provides authentication with
 *     the Director.
 *
 *     Kern Sibbald, June MMI
 *
 *    This routine runs as a thread and must be thread reentrant.
 *
 *  Basic tasks done here:
 *
 */
/*
   Copyright (C) 2000, 2001 Kern Sibbald and John Walker

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "bacula.h"
#include "console_conf.h"
#include "jcr.h"


/* Commands sent to Director */
static char hello[]    = "Hello %s calling\n";

/* Response from Director */
static char OKhello[]   = "1000 OK:";

/* Forward referenced functions */

/*
 * Authenticate Director
 */
int authenticate_director(JCR *jcr, DIRRES *director)
{
   BSOCK *dir = jcr->dir_bsock;

   /* 
    * Send my name to the Director then do authentication
    */
   bnet_fsend(dir, hello, "UserAgent");

   if (!cram_md5_get_auth(dir, director->password) || 
       !cram_md5_auth(dir, director->password)) {
      Pmsg0(-1, _("Director authorization problem.\n"
            "Most likely the passwords do not agree.\n"));  
      return 0;
   }

   Dmsg1(6, ">dird: %s", dir->msg);
   if (bnet_recv(dir) <= 0) {
      Pmsg1(-1, "Bad response to Hello command: ERR=%s\n",
	 bnet_strerror(dir));
      Pmsg0(-1, "The Director is probably not running.\n");
      return 0;
   }
   Dmsg1(10, "<dird: %s", dir->msg);
   if (strncmp(dir->msg, OKhello, sizeof(OKhello)-1) != 0) {
      Pmsg0(-1, "Director rejected Hello command\n");
      return 0;
   } else {
      Pmsg1(-1, "%s", dir->msg);
   }
   return 1;
}
