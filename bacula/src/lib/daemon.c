/*
 *  daemon.c by Kern Sibbald
 *
 *   Version $Id$
 *
 *   this code is inspired by the Prentice Hall book
 *   "Unix Network Programming" by W. Richard Stevens
 *   and later updated from his book "Advanced Programming
 *   in the UNIX Environment"
 *
 * Initialize a daemon process completely detaching us from
 * any terminal processes. 
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

void 
daemon_start()
{
#ifndef HAVE_CYGWIN
   int i;
   int cpid;
   mode_t oldmask;
   /*
    *  Become a daemon.
    */

   Dmsg0(200, "Enter daemon_start\n");
   if ( (cpid = fork() ) < 0)
      Emsg1(M_ABORT, 0, "Cannot fork to become daemon: %s\n", strerror(errno));
   else if (cpid > 0)
      exit(0);		    /* parent exits */
   /* Child continues */
      
   setsid();

   /* In the PRODUCTION system, we close ALL
    * file descriptors. It is useful
    * for debugging to leave the standard ones open.
    */
   for (i=sysconf(_SC_OPEN_MAX)-1; i >=0; i--) {
#ifdef DEBUG
      if (i != STDIN_FILENO && i != STDOUT_FILENO && i != STDERR_FILENO) {
	 close(i);
      }
#else 
      close(i);
#endif
   }

   /* Move to root directory. For debug we stay
    * in current directory so dumps go there.
    */
#ifndef DEBUG
   chdir("/");
#endif

   /* 
    * Avoid creating files 666 but don't override any
    * more restrictive mask set by the user.
    */
   oldmask = umask(022);
   oldmask |= 022;
   umask(oldmask);

   Dmsg0(200, "Exit daemon_start\n");
#endif /* HAVE_CYGWIN */
}
