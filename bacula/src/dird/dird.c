/*
 *
 *   Bacula Director daemon -- this is the main program
 *
 *     Kern Sibbald, March MM
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

/* Forward referenced subroutines */
static void terminate_dird(int sig);
static int check_resources();
static void reload_config(int sig);

/* Exported subroutines */


/* Imported subroutines */
extern JCR *wait_for_next_job(char *runjob);
extern void term_scheduler();
extern void term_ua_server();
extern int do_backup(JCR *jcr);
extern void backup_cleanup(void);
extern void start_UA_server(char *addr, int port);
extern void run_job(JCR *jcr);
extern void init_job_server(int max_workers);

static char *configfile = NULL;
static char *runjob = NULL;
static int background = 1;

/* Globals Exported */
DIRRES *director;		      /* Director resource */
int FDConnectTimeout;
int SDConnectTimeout;

#define CONFIG_FILE "./bacula-dir.conf" /* default configuration file */

static void usage()
{
   fprintf(stderr, _(
"\nVersion: " VERSION " (" DATE ")\n\n"
"Usage: dird [-f -s] [-c config_file] [-d debug_level] [config_file]\n"
"       -c <file>   set configuration file to file\n"
"       -dnn        set debug level to nn\n"
"       -f          run in foreground (for debugging)\n"
"       -r <job>    run <job> now\n"
"       -s          no signals\n"
"       -t          test - read configuration and exit\n"
"       -?          print this message.\n"  
"\n"));

   exit(1);
}


/*********************************************************************
 *
 *	   Main Bacula Server program
 *
 */
int main (int argc, char *argv[])
{
   int ch;
   JCR *jcr;
   int no_signals = FALSE;
   int test_config = FALSE;

   init_stack_dump();
   my_name_is(argc, argv, "bacula-dir");
   init_msg(NULL, NULL);	      /* initialize message handler */
   daemon_start_time = time(NULL);
   memset(&last_job, 0, sizeof(last_job));

   while ((ch = getopt(argc, argv, "c:d:fr:st?")) != -1) {
      switch (ch) {
         case 'c':                    /* specify config file */
	    if (configfile != NULL) {
	       free(configfile);
	    }
	    configfile = bstrdup(optarg);
	    break;

         case 'd':                    /* set debug level */
	    debug_level = atoi(optarg);
	    if (debug_level <= 0) {
	       debug_level = 1; 
	    }
            Dmsg1(0, "Debug level = %d\n", debug_level);
	    break;

         case 'f':                    /* run in foreground */
	    background = FALSE;
	    break;

         case 'r':                    /* run job */
	    if (runjob != NULL) {
	       free(runjob);
	    }
	    if (optarg) {
	       runjob = bstrdup(optarg);
	    }
	    break;

         case 's':                    /* turn off signals */
	    no_signals = TRUE;
	    break;

         case 't':                    /* test config */
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
      init_signals(terminate_dird);
   }
   signal(SIGCHLD, SIG_IGN);

   if (argc) {
      if (configfile != NULL) {
	 free(configfile);
      }
      configfile = bstrdup(*argv);
      argc--; 
      argv++;
   }
   if (argc) {
      usage();
   }

   if (configfile == NULL) {
      configfile = bstrdup(CONFIG_FILE);
   }

   parse_config(configfile);

   if (!check_resources()) {
      Jmsg(NULL, M_ERROR_TERM, 0, "Please correct configuration file: %s\n", configfile);
   }

   if (test_config) {
      terminate_dird(0);
   }

   my_name_is(0, NULL, director->hdr.name);    /* set user defined name */

   FDConnectTimeout = (int)director->FDConnectTimeout;
   SDConnectTimeout = (int)director->SDConnectTimeout;

   if (background) {
      daemon_start();
      init_stack_dump();	      /* grab new pid */
   }

   /* Create pid must come after we are a daemon -- so we have our final pid */
   create_pid_file(director->pid_directory, "bacula-dir", director->DIRport);

/* signal(SIGHUP, reload_config); */

   init_console_msg(working_directory);

   set_thread_concurrency(director->MaxConcurrentJobs * 2 +
      4 /* UA */ + 4 /* sched+watchdog+jobsvr+misc */);

   Dmsg0(200, "Start UA server\n");
   start_UA_server(director->DIRaddr, director->DIRport);

   start_watchdog();		      /* start network watchdog thread */

   init_job_server(director->MaxConcurrentJobs);
  
   Dmsg0(200, "wait for next job\n");
   /* Main loop -- call scheduler to get next job to run */
   while ((jcr = wait_for_next_job(runjob))) {
      run_job(jcr);		      /* run job */
      if (runjob) {		      /* command line, run a single job? */
	 break; 		      /* yes, terminate */
      }
   }

   terminate_dird(0);
}

/* Cleanup and then exit */
static void terminate_dird(int sig)
{
   static int already_here = FALSE;

   if (already_here) {		      /* avoid recursive temination problems */
      exit(1);
   }
   already_here = TRUE;
   delete_pid_file(director->pid_directory, "bacula-dir",  
		   director->DIRport);
   stop_watchdog();
   signal(SIGCHLD, SIG_IGN);          /* don't worry about children now */
   term_scheduler();
   if (runjob) {
      free(runjob);
   }
   if (configfile != NULL) {
      free(configfile);
   }
   if (debug_level > 5) {
      print_memory_pool_stats(); 
   }
   free_config_resources();
   term_ua_server();
   term_msg();			      /* terminate message handler */
   close_memory_pool(); 	      /* release free memory in pool */
   sm_dump(False);
   exit(sig != 0);
}

/*
 * If we get here, we have received a SIGHUP, which means to
 * reread our configuration file. 
 *
 *  ***FIXME***  Check that there are no jobs running before
 *		 doing this. 
 */
static void reload_config(int sig)
{
   static int already_here = FALSE;
   sigset_t set;	

   if (already_here) {
      abort();			      /* Oops, recursion -> die */
   }
   already_here = TRUE;
   sigfillset(&set);
   sigprocmask(SIG_BLOCK, &set, NULL);

   free_config_resources();

   parse_config(configfile);

   Dmsg0(200, "check_resources()\n");
   if (!check_resources()) {
      Jmsg(NULL, M_ERROR_TERM, 0, _("Please correct configuration file: %s\n"), configfile);
   }

   /* Reset globals */
   working_directory = director->working_directory;
   FDConnectTimeout = director->FDConnectTimeout;
   SDConnectTimeout = director->SDConnectTimeout;
 
   sigprocmask(SIG_UNBLOCK, &set, NULL);
   signal(SIGHUP, reload_config);
   already_here = FALSE;
   Dmsg0(0, "Director's configuration file reread.\n");
}

/*
 * Make a quick check to see that we have all the
 * resources needed.
 *
 *  **** FIXME **** this routine could be a lot more
 *   intelligent and comprehensive.
 */
static int check_resources()
{
   int OK = TRUE;
   JOB *job;

   LockRes();

   job	= (JOB *)GetNextRes(R_JOB, NULL);
   director = (DIRRES *)GetNextRes(R_DIRECTOR, NULL);
   if (!director) {
      Jmsg(NULL, M_FATAL, 0, _("No Director resource defined in %s\n\
Without that I don't know who I am :-(\n"), configfile);
      OK = FALSE;
   } else {
      if (!director->working_directory) {
         Jmsg(NULL, M_FATAL, 0, _("No working directory specified. Cannot continue.\n"));
	 OK = FALSE;
      }       
      working_directory = director->working_directory;
      if (!director->messages) {       /* If message resource not specified */
	 director->messages = (MSGS *)GetNextRes(R_MSGS, NULL);
	 if (!director->messages) {
            Jmsg(NULL, M_FATAL, 0, _("No Messages resource defined in %s\n"), configfile);
	    OK = FALSE;
	 }
      }
      if (GetNextRes(R_DIRECTOR, (RES *)director) != NULL) {
         Jmsg(NULL, M_FATAL, 0, _("Only one Director resource permitted in %s\n"),
	    configfile);
	 OK = FALSE;
      } 
   }

   if (!job) {
      Jmsg(NULL, M_FATAL, 0, _("No Job records defined in %s\n"), configfile);
      OK = FALSE;
   }
   for (job=NULL; (job = (JOB *)GetNextRes(R_JOB, (RES *)job)); ) {
      if (!job->client) {
         Jmsg(NULL, M_FATAL, 0, _("No Client record defined for job %s\n"), job->hdr.name);
	 OK = FALSE;
      }
      if (!job->fileset) {
         Jmsg(NULL, M_FATAL, 0, _("No FileSet record defined for job %s\n"), job->hdr.name);
	 OK = FALSE;
      }
      if (!job->storage && job->JobType != JT_VERIFY) {
         Jmsg(NULL, M_FATAL, 0, _("No Storage resource defined for job %s\n"), job->hdr.name);
	 OK = FALSE;
      }
      if (!job->pool) {
         Jmsg(NULL, M_FATAL, 0, _("No Pool resource defined for job %s\n"), job->hdr.name);
	 OK = FALSE;
      }
      if (job->client && job->client->catalog) {
	 CAT *catalog = job->client->catalog;
	 B_DB *db;

	 /*
	  * Make sure we can open catalog, otherwise print a warning
	  * message because the server is probably not running.
	  */
	 db = db_init_database(NULL, catalog->db_name, catalog->db_user,
			    catalog->db_password);
	 if (!db_open_database(db)) {
            Jmsg(NULL, M_FATAL,  0, "%s", db_strerror(db));
	 } else {
	    /* If a pool is defined for this job, create the pool DB	   
	     *	record if it is not already created. 
	     */
	    if (job->pool) {
	       create_pool(db, job->pool);
	    }
	    db_close_database(db);
	 }

      } else {
	 if (job->client) {
            Jmsg(NULL, M_FATAL, 0, _("No Catalog resource defined for client %s\n"), 
	       job->client->hdr.name);
	    OK = FALSE;
	 }
      }
   }

   UnlockRes();
   if (OK) {
      close_msg(NULL);		      /* close temp message handler */
      init_msg(NULL, director->messages); /* open daemon message handler */
   }
   return OK;
}
