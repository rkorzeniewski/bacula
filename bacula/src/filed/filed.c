/*
 *  Bacula File Daemon
 *
 *    Kern Sibbald, March MM
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

/* Imported Functions */
extern void handle_client_request(void *dir_sock);

/* Forward referenced functions */
void terminate_filed(int sig);

/* Exported variables */


#ifdef HAVE_CYGWIN
int win32_client = 1;
#else
int win32_client = 0;
#endif


#define CONFIG_FILE "./bacula-fd.conf" /* default config file */

static char *configfile = NULL;
static int foreground = 0;
static workq_t dir_workq;	      /* queue of work from Director */

static void usage()
{
   fprintf(stderr, _(
"\nVersion: " VERSION " (" DATE ")\n\n"
"Usage: filed [-f -s] [-c config_file] [-d debug_level] [config_file]\n"  
"        -c <file>   use <file> as configuration file\n"
"        -dnn        set debug level to nn\n"
"        -f          run in foreground (for debugging)\n"
"        -s          no signals (for debugging)\n"
"        -t          test configuration file and exit\n"
"        -?          print this message.\n"
"\n"));         
   exit(1);
}


/********************************************************************* 
 *
 *  Main Bacula Unix Client Program			   
 *
 */
#ifdef HAVE_CYGWIN
#define main BaculaMain
#endif
int main (int argc, char *argv[])
{
   int ch;
   int no_signals = FALSE;
   int test_config = FALSE;
   CLIENT *me;			      /* my resource */
   DIRRES *director;

   init_stack_dump();
   my_name_is(argc, argv, "filed");
   daemon_start_time = time(NULL);

   memset(&last_job, 0, sizeof(last_job));

   while ((ch = getopt(argc, argv, "c:d:fst?")) != -1) {
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

         case 's':
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


   init_msg(NULL);
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
      my_name_is(0, (char **)NULL, me->hdr.name);
   }
   working_directory = me->working_directory;

   if (test_config) {
      terminate_filed(0);
   }

   if (!foreground) {
      daemon_start();
      init_stack_dump();	      /* set new pid */
   }

#ifdef BOMB
   me += 1000000;
#endif

   init_watchdog();		      /* start watchdog thread */

   /* Become server, and handle requests */
   Dmsg1(10, "filed: listening on port %d\n", me->FDport);
   bnet_thread_server(me->FDport, 10, &dir_workq, handle_client_request);

   exit(0);			      /* should never get here */
}

void terminate_filed(int sig)
{
   term_watchdog();

   if (configfile != NULL) {
      free(configfile);
   }
   if (debug_level > 5) {
      print_memory_pool_stats(); 
   }
   free_config_resources();
   close_memory_pool(); 	      /* free memory in pool */
   term_msg();
   sm_dump(False);		      /* dump orphaned buffers */
   exit(1);
}
