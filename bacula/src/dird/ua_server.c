/*
 *
 *   Bacula Director -- User Agent Server
 *
 *     Kern Sibbald, September MM
 *
 *    Version $Id$
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

/* Imported subroutines */
extern void run_job(JCR *jcr);

/* Imported variables */
extern int r_first;
extern int r_last;
extern struct s_res resources[];
extern int console_msg_pending;
extern char my_name[];

/* Static variables */

/* Exported variables */
int quit_cmd_thread = 0;

/* Imported functions */

/* Forward referenced functions */

static void *connect_thread(void *arg);
static void handle_UA_client_request(void *arg);


/* Global variables */
static int started = FALSE;
static workq_t ua_workq;

struct s_addr_port {
   char *addr;
   int port;
};

/* Called here by Director daemon to start UA (user agent)
 * command thread. This routine creates the thread and then
 * returns.
 */
void start_UA_server(char *UA_addr, int UA_port)
{
   pthread_t thid;
   int status;
   static struct s_addr_port arg;

   arg.port = UA_port;
   arg.addr = UA_addr;
   if ((status=pthread_create(&thid, NULL, connect_thread, (void *)&arg)) != 0) {
      Emsg1(M_ABORT, 0, _("Cannot create UA thread: %s\n"), strerror(status));
   }
   started = TRUE;
   return;
}

static void *connect_thread(void *arg)
{
   struct s_addr_port *UA = (struct s_addr_port *)arg;

   pthread_detach(pthread_self());

   /*  ****FIXME**** put # 5 on config parameter */
   bnet_thread_server(UA->addr, UA->port, 5, &ua_workq, handle_UA_client_request);
   return NULL;
}

/*
 * Handle Director User Agent commands	 
 *
 */
static void handle_UA_client_request(void *arg)
{
   int stat;
   UAContext ua;
   BSOCK *UA_sock = (BSOCK *) arg;

   pthread_detach(pthread_self());

   memset(&ua, 0, sizeof(ua));
   ua.automount = TRUE;
   ua.verbose = 1;
   ua.jcr = new_jcr(sizeof(JCR), dird_free_jcr);
   ua.jcr->sd_auth_key = bstrdup("dummy"); /* dummy Storage daemon key */
   ua.UA_sock = UA_sock;
   ua.cmd = get_pool_memory(PM_FNAME);
   ua.args = get_pool_memory(PM_FNAME);

   create_unique_job_name(ua.jcr, "*Console*");
   ua.jcr->sched_time = ua.jcr->start_time;
   ua.jcr->JobType = JT_CONSOLE;

   bnet_recv(ua.UA_sock);	   /* Get first message */
   if (!authenticate_user_agent(ua.UA_sock)) {
      goto getout;
   }

   while (!ua.quit) {
      stat = bnet_recv(ua.UA_sock);
      if (stat > 0) {
	 ua.cmd = check_pool_memory_size(ua.cmd, ua.UA_sock->msglen+1);
	 bstrncpy(ua.cmd, ua.UA_sock->msg, ua.UA_sock->msglen+1);
	 parse_command_args(&ua);
         if (ua.argc > 0 && ua.argk[0][0] == '.') {
	    do_a_dot_command(&ua, ua.cmd);
	 } else {
	    do_a_command(&ua, ua.cmd);
	 }
	 if (!ua.quit) {
	    if (ua.auto_display_messages) {
               strcpy(ua.cmd, "messages");
	       qmessagescmd(&ua, ua.cmd);
	       ua.user_notified_msg_pending = FALSE;
	    } else if (!ua.user_notified_msg_pending && console_msg_pending) {
               bsendmsg(&ua, _("You have messages.\n"));
	       ua.user_notified_msg_pending = TRUE;
	    }
	    bnet_sig(ua.UA_sock, BNET_EOD); /* send end of command */
	 }
      } else if (stat == 0) {
	 if (ua.UA_sock->msglen == BNET_TERMINATE) {
	    ua.quit = TRUE;
	    break;
	 }
	 bnet_sig(ua.UA_sock, BNET_POLL);
      } else {
	 break; 		   /* error, exit */
      }
   }

getout:
   if (ua.UA_sock) {
      bnet_close(ua.UA_sock);
      ua.UA_sock = NULL;
   }

   close_db(&ua);		      /* do this before freeing JCR */

   if (ua.jcr) {
      free_jcr(ua.jcr);
      ua.jcr = NULL;
   }
   if (ua.prompt) {
      free(ua.prompt);
   }
   if (ua.cmd) {
      free_pool_memory(ua.cmd);
   }
   if (ua.args) {
      free_pool_memory(ua.args);
   }
   return;
}

/*
 * Called from main Bacula thread 
 */
void term_ua_server()
{
   if (!started) {
      return;
   }
   quit_cmd_thread = TRUE;
}
