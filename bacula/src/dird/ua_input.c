/*
 *
 *   Bacula Director -- User Agent Input and scanning code
 *
 *     Kern Sibbald, October MMI
 *
 *   Version $Id$
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
#include "ua.h"


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

/* 
 * Return next argument from command line.  Note, this
 * routine is destructive.
 */
char *next_arg(char **s)
{
   char *p, *q, *n;
   int in_quote = 0;

   /* skip past spaces to next arg */
   for (p=*s; *p && *p == ' '; ) {
      p++;
   }	
   Dmsg1(400, "Next arg=%s\n", p);
   for (n = q = p; *p ; ) {
      if (*p == '\\') {
	 p++;
	 if (*p) {
	    *q++ = *p++;
	 } else {
	    *q++ = *p;
	 }
	 continue;
      }
      if (*p == '"') {                  /* start or end of quote */
	 if (in_quote) {
	    p++;			/* skip quote */
	    in_quote = 0;
	    continue;
	 }
	 in_quote = 1;
	 p++;
	 continue;
      }
      if (!in_quote && *p == ' ') {     /* end of field */
	 p++;
	 break;
      }
      *q++ = *p++;
   }
   *q = 0;
   *s = p;
   Dmsg2(400, "End arg=%s next=%s\n", n, p);
   return n;
}   

/*
 * This routine parses the input command line.
 * It makes a copy in args, then builds an
 *  argc, argv like list where
 *    
 *  argc = count of arguments
 *  argk[i] = argument keyword (part preceding =)
 *  argv[i] = argument value (part after =)
 *
 *  example:  arg1 arg2=abc arg3=
 *
 *  argc = c
 *  argk[0] = arg1
 *  argv[0] = NULL
 *  argk[1] = arg2
 *  argv[1] = abc
 *  argk[2] = arg3
 *  argv[2] = 
 */

void parse_command_args(UAContext *ua)
{
   char *p, *q, *n;
   int i, len;

   len = strlen(ua->cmd) + 1;
   ua->args = check_pool_memory_size(ua->args, len);
   bstrncpy(ua->args, ua->cmd, len);
   strip_trailing_junk(ua->args);
   ua->argc = 0;
   p = ua->args;
   /* Pick up all arguments */
   while (ua->argc < MAX_ARGS) {
      n = next_arg(&p);   
      if (*n) {
	 ua->argk[ua->argc++] = n;
      } else {
	 break;
      }
   }
   /* Separate keyword and value */
   for (i=0; i<ua->argc; i++) {
      p = strchr(ua->argk[i], '=');
      if (p) {
	 *p++ = 0;		      /* terminate keyword and point to value */
	 /* Unquote quoted values */
         if (*p == '"') {
            for (n = q = ++p; *p && *p != '"'; ) {
               if (*p == '\\') {
		  p++;
	       }
	       *q++ = *p++;
	    }
	    *q = 0;		      /* terminate string */
	    p = n;		      /* point to string */
	 }
	 if (strlen(p) > MAX_NAME_LENGTH-1) {
	    p[MAX_NAME_LENGTH-1] = 0; /* truncate to max len */
	 }
      }
      ua->argv[i] = p;		      /* save ptr to value or NULL */
   }
#ifdef xxxx
   for (i=0; i<ua->argc; i++) {
      Dmsg3(000, "Arg %d: kw=%s val=%s\n", i, 
         ua->argk[i], ua->argv[i]?ua->argv[i]:"NULL");
   }
#endif
}
