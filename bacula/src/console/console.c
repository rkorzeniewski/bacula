/*
 *
 *   Bacula Console interface to the Director
 *
 *     Kern Sibbald, September MM
 *
 *     Version $Id$
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
#include <termios.h>
 
/* Imported functions */
int authenticate_director(JCR *jcr, DIRRES *director);

       
/* Exported variables */


#ifdef HAVE_CYGWIN
int rl_catch_signals;
#else
extern int rl_catch_signals;
#endif

/* Forward referenced functions */
static void terminate_console(int sig);
int get_cmd(FILE *input, char *prompt, BSOCK *sock, int sec);

/* Static variables */
static char *configfile = NULL;
static BSOCK *UA_sock = NULL;
static DIRRES *dir; 
static FILE *output = stdout;
static int stop = FALSE;


#define CONFIG_FILE "./console.conf"   /* default configuration file */

static void usage()
{
   fprintf(stderr,
"\nVersion: " VERSION " (" DATE ")\n\n"
"Usage: console [-s] [-c config_file] [-d debug_level] [config_file]\n"
"       -c <file>   set configuration file to file\n"
"       -dnn        set debug level to nn\n"
"       -s          no signals\n"
"       -t          test - read configuration and exit\n"
"       -?          print this message.\n"  
"\n");

   exit(1);
}


void got_stop(int sig)
{
   stop = TRUE;
}

void got_continue(int sig)
{
   stop = FALSE;
}

void got_tout(int sig) 
{
// printf("Got tout\n");
}

void got_tin(int sig)
{   
// printf("Got tin\n");
}


static void read_and_process_input(FILE *input, BSOCK *UA_sock) 
{
   char *prompt = "*";
   int at_prompt = FALSE;
   int tty_input = isatty(fileno(input));
   int stat;

   for ( ;; ) { 
      if (at_prompt) {                /* don't prompt multiple times */
         prompt = "";
      } else {
         prompt = "*";
	 at_prompt = TRUE;
      }
      if (tty_input) {
	 stat = get_cmd(input, prompt, UA_sock, 30);
      } else {
	 int len = sizeof_pool_memory(UA_sock->msg) - 1;
	 if (fgets(UA_sock->msg, len, input) == NULL) {
	    stat = -1;
	 } else {
	    strip_trailing_junk(UA_sock->msg);
	    UA_sock->msglen = strlen(UA_sock->msg);
	    stat = 1;
	 }
      }
      if (stat < 0) {
	 break; 		      /* error */
      } else if (stat == 0) {	      /* timeout */
         bnet_fsend(UA_sock, ".messages");
      } else {
	 at_prompt = FALSE;
	 if (!bnet_send(UA_sock)) {   /* send command */
	    break;		      /* error */
	 }
      }
      if (strcmp(UA_sock->msg, ".quit") == 0 || strcmp(UA_sock->msg, ".exit") == 0) {
	 break;
      }
      while ((stat = bnet_recv(UA_sock)) >= 0) {
	 if (at_prompt) {
	    if (!stop) {
               fprintf(output, "\n");
	    }
	    at_prompt = FALSE;
	 }
	 if (!stop) {
            fprintf(output, "%s", UA_sock->msg);
	 }
      }
      if (!stop) {
	 fflush(output);
      }
      if (is_bnet_stop(UA_sock)) {
	 break; 		      /* error or term */
      } else if (stat == BNET_SIGNAL) {
	 if (UA_sock->msglen == BNET_PROMPT) {
	    at_prompt = TRUE;
	 }
         Dmsg1(100, "Got poll %s\n", bnet_sig_to_ascii(UA_sock));
      }
   }
}


/*********************************************************************
 *
 *	   Main Bacula Console -- User Interface Program
 *
 */
int main(int argc, char *argv[])
{
   int ch, i, ndir, item;
   int no_signals = FALSE;
   int test_config = FALSE;
   JCR jcr;

   init_stack_dump();
   my_name_is(argc, argv, "console");
   init_msg(NULL, NULL);
   working_directory = "/tmp";

   while ((ch = getopt(argc, argv, "bc:d:r:st?")) != -1) {
      switch (ch) {
         case 'c':                    /* configuration file */
	    if (configfile != NULL)
	       free(configfile);
	    configfile = bstrdup(optarg);
	    break;

         case 'd':
	    debug_level = atoi(optarg);
	    if (debug_level <= 0)
	       debug_level = 1;
	    break;

         case 's':                    /* turn off signals */
	    no_signals = TRUE;
	    break;

         case 't':
	    test_config = TRUE;
	    break;

         case '?':
	 default:
	    usage();

      }  
   }
   argc -= optind;
   argv += optind;

   if (!no_signals) {
      init_signals(terminate_console);
   }
   signal(SIGCHLD, SIG_IGN);
   signal(SIGTSTP, got_stop);
   signal(SIGCONT, got_continue);
   signal(SIGTTIN, got_tin);
   signal(SIGTTOU, got_tout);

   if (argc) {
      usage();
   }

   if (configfile == NULL) {
      configfile = bstrdup(CONFIG_FILE);
   }

   parse_config(configfile);

   LockRes();
   ndir = 0;
   for (dir=NULL; (dir = (DIRRES *)GetNextRes(R_DIRECTOR, (RES *)dir)); ) {
      ndir++;
   }
   UnlockRes();
   if (ndir == 0) {
      Emsg1(M_ABORT, 0, "No Director resource defined in %s\n\
Without that I don't how to speak to the Director :-(\n", configfile);
   }

   if (test_config) {
      terminate_console(0);
      exit(0);
   }

   memset(&jcr, 0, sizeof(jcr));

   if (ndir > 1) {
      UA_sock = init_bsock(NULL, 0, "", "", 0);
try_again:
      fprintf(output, "Available Directors:\n");
      LockRes();
      ndir = 0;
      for (dir = NULL; (dir = (DIRRES *)GetNextRes(R_DIRECTOR, (RES *)dir)); ) {
         fprintf(output, "%d  %s at %s:%d\n", 1+ndir++, dir->hdr.name, dir->address,
	    dir->DIRport);
      }
      UnlockRes();
      if (get_cmd(stdin, "Select Director: ", UA_sock, 600) < 0) {
	 return 1;
      }
      item = atoi(UA_sock->msg);
      if (item < 0 || item > ndir) {
         fprintf(output, "You must enter a number between 1 and %d\n", ndir);
	 goto try_again;
      }
      LockRes();
      dir = NULL;
      for (i=0; i<item; i++) {
	 dir = (DIRRES *)GetNextRes(R_DIRECTOR, (RES *)dir);
      }
      UnlockRes();
      term_bsock(UA_sock);
   } else {
      LockRes();
      dir = (DIRRES *)GetNextRes(R_DIRECTOR, NULL);
      UnlockRes();
   }
      

   Dmsg2(-1, "Connecting to Director %s:%d\n", dir->address,dir->DIRport);
   UA_sock = bnet_connect(&jcr, 5, 15, "Director daemon", dir->address, 
			  NULL, dir->DIRport, 0);
   if (UA_sock == NULL) {
      terminate_console(0);
      return 1;
   }
   jcr.dir_bsock = UA_sock;
   if (!authenticate_director(&jcr, dir)) {
      fprintf(stderr, "ERR: %s", UA_sock->msg);
      terminate_console(0);
      return 1;
   }

   Dmsg0(40, "Opened connection with Director daemon\n");

   read_and_process_input(stdin, UA_sock);

   if (UA_sock) {
      bnet_sig(UA_sock, BNET_TERMINATE); /* send EOF */
      bnet_close(UA_sock);
   }

   terminate_console(0);
   return 0;
}


/* Cleanup and then exit */
static void terminate_console(int sig)
{
   static int already_here = FALSE;

   if (already_here)		      /* avoid recursive temination problems */
      exit(1);
   already_here = TRUE;
   exit(0);
}

#ifdef HAVE_READLINE
#undef free
#include "readline/readline.h"
#include "readline/history.h"


int 
get_cmd(FILE *input, char *prompt, BSOCK *sock, int sec)
{
   char *line;

   rl_catch_signals = 1;
   line = readline(prompt);

   if (!line) {
      exit(1);
   }
   strcpy(sock->msg, line);
   strip_trailing_junk(sock->msg);
   sock->msglen = strlen(sock->msg);
   if (sock->msglen) {
      add_history(sock->msg);
   }
   free(line);
   return 1;
}

#else /* no readline, do it ourselves */

/*
 *   Returns: 1 if data available
 *	      0 if timeout
 *	     -1 if error
 */
static int
wait_for_data(int fd, int sec)
{
   fd_set fdset;
   struct timeval tv;

   FD_ZERO(&fdset);
   FD_SET(fd, &fdset);
   tv.tv_sec = sec;
   tv.tv_usec = 0;
   for ( ;; ) {
      switch(select(fd + 1, &fdset, NULL, NULL, &tv)) {
	 case 0:			 /* timeout */
	    return 0;
	 case -1:
	    if (errno == EINTR || errno == EAGAIN) {
	       continue;
	    }
	    return -1;			/* error return */
	 default:
	    return 1;
      }
   }
}

/*	
 * Get next input command from terminal. 
 *
 *   Returns: 1 if got input
 *	      0 if timeout
 *	     -1 if EOF or error
 */
int 
get_cmd(FILE *input, char *prompt, BSOCK *sock, int sec)
{
   int len;  
   if (!stop) {
      fprintf(output, prompt);
      fflush(output);
   }
again:
   switch (wait_for_data(fileno(input), sec)) {
      case 0:
	 return 0;		      /* timeout */
      case -1: 
	 return -1;		      /* error */
      default:
	 len = sizeof_pool_memory(sock->msg) - 1;
	 if (stop) {
	    goto again;
	 }
	 if (fgets(sock->msg, len, input) == NULL) {
	    return -1;
	 }
	 break;
   }
   strip_trailing_junk(sock->msg);
   sock->msglen = strlen(sock->msg);
   return 1;
}

#endif
