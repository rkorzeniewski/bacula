/*
 *  Bacula File Daemon
 *
 *    Kern Sibbald, March MM
 *
 *   Version $Id$
 *
 */
/*
   Copyright (C) 2000-2004 Kern Sibbald and John Walker

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

/* Imported Functions */
extern void *handle_client_request(void *dir_sock);

/* Forward referenced functions */
void terminate_filed(int sig);

/* Exported variables */
CLIENT *me;			      /* my resource */
char OK_msg[]   = "2000 OK\n";
char TERM_msg[] = "2999 Terminate\n";


#if defined(HAVE_CYGWIN) || defined(HAVE_WIN32)
const int win32_client = 1;
#else
const int win32_client = 0;
#endif


#define CONFIG_FILE "./bacula-fd.conf" /* default config file */

static char *configfile = NULL;
static int foreground = 0;
static int inetd_request = 0;
static workq_t dir_workq;	      /* queue of work from Director */
static pthread_t server_tid;


static void usage()
{
   fprintf(stderr, _(
"\nVersion: " VERSION " (" BDATE ")\n\n"
"Usage: bacula-fd [-f -s] [-c config_file] [-d debug_level]\n"  
"        -c <file>   use <file> as configuration file\n"
"        -dnn        set debug level to nn\n"
"        -f          run in foreground (for debugging)\n"
"        -g          groupid\n"
"        -i          inetd request\n"
"        -s          no signals (for debugging)\n"
"        -t          test configuration file and exit\n"
"        -u          userid\n"
"        -v          verbose user messages\n"
"        -?          print this message.\n"
"\n"));         
   exit(1);
}


/********************************************************************* 
 *
 *  Main Bacula Unix Client Program			   
 *
 */
#if defined(HAVE_CYGWIN) || defined(HAVE_WIN32)
#define main BaculaMain
#endif

int main (int argc, char *argv[])
{
   int ch;
   int no_signals = FALSE;
   int test_config = FALSE;
   DIRRES *director;
   char *uid = NULL;
   char *gid = NULL;

   init_stack_dump();
   my_name_is(argc, argv, "bacula-fd");
   textdomain("bacula-fd");
   init_msg(NULL, NULL);
   daemon_start_time = time(NULL);

   while ((ch = getopt(argc, argv, "c:d:fg:istu:v?")) != -1) {
      switch (ch) {
      case 'c':                    /* configuration file */
	 if (configfile != NULL) {
	    free(configfile);
	 }
	 configfile = bstrdup(optarg);
	 break;

      case 'd':                    /* debug level */
	 debug_level = atoi(optarg);
	 if (debug_level <= 0) {
	    debug_level = 1; 
	 }
	 break;

      case 'f':                    /* run in foreground */
	 foreground = TRUE;
	 break;

      case 'g':                    /* set group */
	 gid = optarg;
	 break;

      case 'i':
	 inetd_request = TRUE;
	 break;
      case 's':
	 no_signals = TRUE;
	 break;

      case 't':
	 test_config = TRUE;
	 break;

      case 'u':                    /* set userid */
	 uid = optarg;
	 break;

      case 'v':                    /* verbose */
	 verbose++;
	 break;

      case '?':
      default:
	 usage();

      }  
   }
   argc -= optind;
   argv += optind;

   if (argc) {
      if (configfile != NULL)
	 free(configfile);
      configfile = bstrdup(*argv);
      argc--; 
      argv++;
   }
   if (argc) {
      usage();
   }

   if (!no_signals) {
      init_signals(terminate_filed);
   }

   if (configfile == NULL) {
      configfile = bstrdup(CONFIG_FILE);
   }

   parse_config(configfile);

   LockRes();
   director = (DIRRES *)GetNextRes(R_DIRECTOR, NULL);
   UnlockRes();
   if (!director) {
      Emsg1(M_ABORT, 0, _("No Director resource defined in %s\n"),
	 configfile);
   }

   LockRes();
   me = (CLIENT *)GetNextRes(R_CLIENT, NULL);
   UnlockRes();
   if (!me) {
      Emsg1(M_ABORT, 0, _("No File daemon resource defined in %s\n\
Without that I don't know who I am :-(\n"), configfile);
   } else {
      my_name_is(0, NULL, me->hdr.name);
      if (!me->messages) {
	 LockRes();
	 me->messages = (MSGS *)GetNextRes(R_MSGS, NULL);
	 UnlockRes();
	 if (!me->messages) {
             Emsg1(M_ABORT, 0, _("No Messages resource defined in %s\n"), configfile);
	 }
      }
      close_msg(NULL);		      /* close temp message handler */
      init_msg(NULL, me->messages);   /* open user specified message handler */
   }

   set_working_directory(me->working_directory);

   if (test_config) {
      terminate_filed(0);
   }

   if (!foreground &&!inetd_request) {
      daemon_start();
      init_stack_dump();	      /* set new pid */
   }

   /* Maximum 1 daemon at a time */
   create_pid_file(me->pid_directory, "bacula-fd", me->FDport);
   read_state_file(me->working_directory, "bacula-fd", me->FDport);

   drop(uid, gid);

#ifdef BOMB
   me += 1000000;
#endif

   set_thread_concurrency(10);

   start_watchdog();		      /* start watchdog thread */

   init_jcr_subsystem();	      /* start JCR watchdogs etc. */
   server_tid = pthread_self();

   if (inetd_request) {
      /* Socket is on fd 0 */	       
      struct sockaddr_in client_addr;
      memset(&client_addr, 0, sizeof(client_addr));
      BSOCK *bs = init_bsock(NULL, 0, "client", "unknown client", me->FDport, 
			     &client_addr);
      handle_client_request((void *)bs);
   } else {
      /* Become server, and handle requests */
      Dmsg1(10, "filed: listening on port %d\n", me->FDport);
      bnet_thread_server(me->FDaddr, me->FDport, me->MaxConcurrentJobs, 
		      &dir_workq, handle_client_request);
   }

   terminate_filed(0);
   exit(0);			      /* should never get here */
}

void terminate_filed(int sig)
{
   bnet_stop_thread_server(server_tid);
   write_state_file(me->working_directory, "bacula-fd", me->FDport);
   delete_pid_file(me->pid_directory, "bacula-fd", me->FDport);
   if (configfile != NULL) {
      free(configfile);
   }
   if (debug_level > 5) {
      print_memory_pool_stats(); 
   }
   free_config_resources();
   term_msg();
   stop_watchdog();
   close_memory_pool(); 	      /* release free memory in pool */
   sm_dump(False);		      /* dump orphaned buffers */
   exit(sig);
}
