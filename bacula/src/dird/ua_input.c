/*
 *
 *   Bacula Director -- User Agent Input and scanning code
 *
 *     Kern Sibbald, October MMI
 *
 *   Version $Id$
 */

/*
   Copyright (C) 2000-2003 Kern Sibbald and John Walker

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


/* Imported variables */


/* Exported functions */

int get_cmd(UAContext *ua, char *prompt)
{
   BSOCK *sock = ua->UA_sock;
   int stat;

   ua->cmd[0] = 0;
   if (!sock) { 		      /* No UA */
      return 0;
   }
   bnet_fsend(sock, "%s", prompt);
   bnet_sig(sock, BNET_PROMPT);       /* request more input */
   for ( ;; ) {
      stat = bnet_recv(sock);
      if (stat == BNET_SIGNAL) {
	 continue;		      /* ignore signals */
      }
      if (is_bnet_stop(sock)) {
	 return 0;		      /* error or terminate */
      }
      ua->cmd = check_pool_memory_size(ua->cmd, sock->msglen+1);
      bstrncpy(ua->cmd, sock->msg, sock->msglen+1);
      strip_trailing_junk(ua->cmd);
      if (strcmp(ua->cmd, ".messages") == 0) {
	 qmessagescmd(ua, ua->cmd);
      }
      /* ****FIXME**** if .command, go off and do it. For now ignore it. */
      if (ua->cmd[0] == '.' && ua->cmd[1] != 0) {
	 continue;		      /* dot command */
      }
      /* Lone dot => break or actual response */
      break;
   }
   return 1;
}

void parse_ua_args(UAContext *ua)
{
   return parse_command_args(ua->cmd, ua->args, &ua->argc, ua->argk, ua->argv);
}
