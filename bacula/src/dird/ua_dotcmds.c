/*
 *
 *   Bacula Director -- User Agent Commands
 *     These are "dot" commands, i.e. commands preceded
 *	  by a period. These commands are meant to be used
 *	  by a program, so there is no prompting, and the
 *	  returned results are (supposed to be) predictable.
 *
 *     Kern Sibbald, April MMII
 *
 *   Version $Id$
 */

/*
   Copyright (C) 2002 Kern Sibbald and John Walker

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
extern int r_first;
extern int r_last;
extern struct s_res resources[];
extern int console_msg_pending;
extern char my_name[];

/* Imported functions */
extern int qmessagescmd(UAContext *ua, char *cmd);
extern int quitcmd(UAContext *ua, char *cmd);

/* Forward referenced functions */
static int diecmd(UAContext *ua, char *cmd);
static int jobscmd(UAContext *ua, char *cmd);
static int filesetscmd(UAContext *ua, char *cmd);
static int clientscmd(UAContext *ua, char *cmd);
static int msgscmd(UAContext *ua, char *cmd);

struct cmdstruct { char *key; int (*func)(UAContext *ua, char *cmd); char *help; }; 
static struct cmdstruct commands[] = {
 { N_(".die"),        diecmd,       NULL},
 { N_(".jobs"),       jobscmd,      NULL},
 { N_(".filesets"),   filesetscmd,  NULL},
 { N_(".clients"),    clientscmd,   NULL},
 { N_(".msgs"),       msgscmd,      NULL},
 { N_(".messages"),   qmessagescmd, NULL},
 { N_(".quit"),       quitcmd,      NULL},
 { N_(".exit"),       quitcmd,      NULL} 
	     };
#define comsize (sizeof(commands)/sizeof(struct cmdstruct))

/*
 * Execute a command from the UA
 */
int do_a_dot_command(UAContext *ua, char *cmd)
{
   unsigned int i;
   int len, stat;  
   int found;

   found = 0;
   stat = 1;

   Dmsg1(200, "Dot command: %s\n", ua->UA_sock->msg);
   if (ua->argc == 0) {
      return 1;
   }

   len = strlen(ua->argk[0]);
   if (len == 1) {
      return 1; 		      /* no op */
   }
   for (i=0; i<comsize; i++) {	   /* search for command */
      if (strncasecmp(ua->argk[0],  _(commands[i].key), len) == 0) {
	 stat = (*commands[i].func)(ua, cmd);	/* go execute command */
	 found = 1;
	 break;
      }
   }
   if (!found) {
      strcat(ua->UA_sock->msg, _(": is an illegal command\n"));
      ua->UA_sock->msglen = strlen(ua->UA_sock->msg);
      bnet_send(ua->UA_sock);
   }
   return stat;
}

/*
 * Create segmentation fault 
 */
static int diecmd(UAContext *ua, char *cmd)
{
   JCR *jcr = NULL;
   int a;
   
   a = jcr->JobId; /* ref NULL pointer */
   return 0;
}

static int jobscmd(UAContext *ua, char *cmd)
{
   JOB *job = NULL;
   LockRes();
   while ( (job = (JOB *)GetNextRes(R_JOB, (RES *)job)) ) {
      bsendmsg(ua, "%s\n", job->hdr.name);
   }
   UnlockRes();
   return 1;
}

static int filesetscmd(UAContext *ua, char *cmd)
{
   FILESET *fs = NULL;
   LockRes();
   while ( (fs = (FILESET *)GetNextRes(R_FILESET, (RES *)fs)) ) {
      bsendmsg(ua, "%s\n", fs->hdr.name);
   }
   UnlockRes();
   return 1;
}

static int clientscmd(UAContext *ua, char *cmd)
{
   CLIENT *client = NULL;
   LockRes();
   while ( (client = (CLIENT *)GetNextRes(R_CLIENT, (RES *)client)) ) {
      bsendmsg(ua, "%s\n", client->hdr.name);
   }
   UnlockRes();
   return 1;
}

static int msgscmd(UAContext *ua, char *cmd)
{
   MSGS *msgs = NULL;
   LockRes();
   while ( (msgs = (MSGS *)GetNextRes(R_MSGS, (RES *)msgs)) ) {
      bsendmsg(ua, "%s\n", msgs->hdr.name);
   }
   UnlockRes();
   return 1;
}
