/*
 *
 *   Bacula Director daemon -- this is the main program
 *
 *     Kern Sibbald, March MM
 *
 *   Version $Id$
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
#include "dird.h"

/* Forward referenced subroutines */
static void terminate_dird(int sig);
static int check_resources();
static void reload_config(int sig);

/* Exported subroutines */


/* Imported subroutines */
JCR *wait_for_next_job(char *runjob);
void term_scheduler();
void term_ua_server();
int do_backup(JCR *jcr);
void backup_cleanup(void);
void start_UA_server(char *addr, int port);
void init_job_server(int max_workers);
void store_jobtype(LEX *lc, RES_ITEM *item, int index, int pass);
void store_level(LEX *lc, RES_ITEM *item, int index, int pass);
void store_replace(LEX *lc, RES_ITEM *item, int index, int pass);

static char *configfile = NULL;
static char *runjob = NULL;
static int background = 1;
static void init_reload(void);

/* Globals Exported */
DIRRES *director;		      /* Director resource */
int FDConnectTimeout;
int SDConnectTimeout;

/* Globals Imported */
extern int r_first, r_last;	      /* first and last resources */
extern RES_ITEM job_items[];
extern URES res_all;


#define CONFIG_FILE "./bacula-dir.conf" /* default configuration file */

static void usage()
{
   fprintf(stderr, _(
"\nVersion: " VERSION " (" BDATE ")\n\n"
"Usage: dird [-f -s] [-c config_file] [-d debug_level] [config_file]\n"
"       -c <file>   set configuration file to file\n"
"       -dnn        set debug level to nn\n"
"       -f          run in foreground (for debugging)\n"
"       -g          groupid\n"
"       -r <job>    run <job> now\n"
"       -s          no signals\n"
"       -t          test - read configuration and exit\n"
"       -u          userid\n"
"       -v          verbose user messages\n"
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
   char *uid = NULL;
   char *gid = NULL;

   init_stack_dump();
   my_name_is(argc, argv, "bacula-dir");
   textdomain("bacula-dir");
   init_msg(NULL, NULL);	      /* initialize message handler */
   init_reload();
   daemon_start_time = time(NULL);

   while ((ch = getopt(argc, argv, "c:d:fg:r:stu:v?")) != -1) {
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

      case 'g':                    /* set group id */
	 gid = optarg;
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

      case 'u':                    /* set uid */
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

   if (!no_signals) {
      init_signals(terminate_dird);
   }

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
      Jmsg((JCR *)NULL, M_ERROR_TERM, 0, _("Please correct configuration file: %s\n"), configfile);
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
   read_state_file(director->working_directory, "bacula-dir", director->DIRport);

   drop(uid, gid);		      /* reduce priveleges if requested */

   signal(SIGHUP, reload_config);

   init_console_msg(working_directory);

   set_thread_concurrency(director->MaxConcurrentJobs * 2 +
      4 /* UA */ + 4 /* sched+watchdog+jobsvr+misc */);

   Dmsg0(200, "Start UA server\n");
   start_UA_server(director->DIRaddr, director->DIRport);

   start_watchdog();		      /* start network watchdog thread */

   init_jcr_subsystem();	      /* start JCR watchdogs etc. */

   init_job_server(director->MaxConcurrentJobs);
  
   Dmsg0(200, "wait for next job\n");
   /* Main loop -- call scheduler to get next job to run */
   while ((jcr = wait_for_next_job(runjob))) {
      run_job(jcr);		      /* run job */
      free_jcr(jcr);		      /* release jcr */
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
   write_state_file(director->working_directory, "bacula-dir", director->DIRport);
   delete_pid_file(director->pid_directory, "bacula-dir", director->DIRport);
// signal(SIGCHLD, SIG_IGN);          /* don't worry about children now */
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
   stop_watchdog();
   close_memory_pool(); 	      /* release free memory in pool */
   sm_dump(False);
   exit(sig);
}

struct RELOAD_TABLE {
   int job_count;
   RES **res_table;
};

static const int max_reloads = 10;
static RELOAD_TABLE reload_table[max_reloads];

static void init_reload(void) 
{
   for (int i=0; i < max_reloads; i++) {
      reload_table[i].job_count = 0;
      reload_table[i].res_table = NULL;
   }
}

/*
 * Called here at the end of every job that was
 * hooked decrementing the active job_count. When
 * it goes to zero, no one is using the associated
 * resource table, so free it.
 */
static void reload_job_end_cb(JCR *jcr)
{
   int i = jcr->reload_id - 1;
   RES **res_tab;
   Dmsg1(000, "reload job_end JobId=%d\n", jcr->JobId);
   if (--reload_table[i].job_count <= 0) {
      int num = r_last - r_first + 1;
      res_tab = reload_table[i].res_table;
      Dmsg0(000, "Freeing resources\n");
      for (int j=0; j<num; j++) {
	 free_resource(res_tab[j], r_first + j);
      }
      free(res_tab);
      reload_table[i].job_count = 0;
      reload_table[i].res_table = NULL;
   }
}

/*
 * If we get here, we have received a SIGHUP, which means to
 *    reread our configuration file. 
 */
static void reload_config(int sig)
{
   static bool already_here = false;
   sigset_t set;	
   JCR *jcr;
   int njobs = 0;
   int table = -1;

   if (already_here) {
      abort();			      /* Oops, recursion -> die */
   }
   already_here = true;
   sigfillset(&set);
   sigprocmask(SIG_BLOCK, &set, NULL);

   lock_jcr_chain();
   LockRes();

   for (int i=0; i < max_reloads; i++) {
      if (reload_table[i].res_table == NULL) {
	 table = i;
	 break;
      }
   }
   if (table < 0) {
      Jmsg(NULL, M_ERROR, 0, _("Too many reload requests.\n"));
      goto bail_out;
   }

   /*
    * Hook all active jobs that are not already hooked (i.e.
    *  reload_id == 0
    */
   foreach_jcr(jcr) {
      /* JobId==0 => console */
      if (jcr->JobId != 0 && jcr->reload_id == 0) {
	 reload_table[table].job_count++;
	 jcr->reload_id = table + 1;
	 job_end_push(jcr, reload_job_end_cb);
	 njobs++;
      }
      free_locked_jcr(jcr);
   }
   Dmsg1(000, "Reload_config njobs=%d\n", njobs);
   if (njobs > 0) {
      reload_table[table].res_table = save_config_resources();
      Dmsg1(000, "Saved old config in table %d\n", table);
   } else {
      free_config_resources();
   }

   Dmsg0(000, "Calling parse config\n");
   parse_config(configfile);

   Dmsg0(000, "Reloaded config file\n");
   if (!check_resources()) {
      Jmsg(NULL, M_ERROR_TERM, 0, _("Please correct configuration file: %s\n"), configfile);
   }

   /* Reset globals */
   set_working_directory(director->working_directory);
   FDConnectTimeout = director->FDConnectTimeout;
   SDConnectTimeout = director->SDConnectTimeout;
   Dmsg0(0, "Director's configuration file reread.\n");

bail_out:
   UnlockRes();
   unlock_jcr_chain();
   sigprocmask(SIG_UNBLOCK, &set, NULL);
   signal(SIGHUP, reload_config);
   already_here = false;
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
   bool OK = true;
   JOB *job;

   LockRes();

   job = (JOB *)GetNextRes(R_JOB, NULL);
   director = (DIRRES *)GetNextRes(R_DIRECTOR, NULL);
   if (!director) {
      Jmsg(NULL, M_FATAL, 0, _("No Director resource defined in %s\n\
Without that I don't know who I am :-(\n"), configfile);
      OK = false;
   } else {
      set_working_directory(director->working_directory);
      if (!director->messages) {       /* If message resource not specified */
	 director->messages = (MSGS *)GetNextRes(R_MSGS, NULL);
	 if (!director->messages) {
            Jmsg(NULL, M_FATAL, 0, _("No Messages resource defined in %s\n"), configfile);
	    OK = false;
	 }
      }
      if (GetNextRes(R_DIRECTOR, (RES *)director) != NULL) {
         Jmsg(NULL, M_FATAL, 0, _("Only one Director resource permitted in %s\n"),
	    configfile);
	 OK = false;
      } 
   }

   if (!job) {
      Jmsg(NULL, M_FATAL, 0, _("No Job records defined in %s\n"), configfile);
      OK = false;
   }
   foreach_res(job, R_JOB) {
      int i;

      if (job->jobdefs) {
	 /* Transfer default items from JobDefs Resource */
	 for (i=0; job_items[i].name; i++) {
	    char **def_svalue, **svalue;  /* string value */
	    int *def_ivalue, *ivalue;	  /* integer value */
	    int64_t *def_lvalue, *lvalue; /* 64 bit values */
	    uint32_t offset;

            Dmsg4(400, "Job \"%s\", field \"%s\" bit=%d def=%d\n",
		job->hdr.name, job_items[i].name, 
		bit_is_set(i, job->hdr.item_present),  
		bit_is_set(i, job->jobdefs->hdr.item_present));

	    if (!bit_is_set(i, job->hdr.item_present) &&
		 bit_is_set(i, job->jobdefs->hdr.item_present)) { 
               Dmsg2(400, "Job \"%s\", field \"%s\": getting default.\n",
		 job->hdr.name, job_items[i].name);
	       offset = (char *)(job_items[i].value) - (char *)&res_all;   
	       /*
		* Handle strings and directory strings
		*/
	       if (job_items[i].handler == store_str ||
		   job_items[i].handler == store_dir) {
		  def_svalue = (char **)((char *)(job->jobdefs) + offset);
                  Dmsg5(400, "Job \"%s\", field \"%s\" def_svalue=%s item %d offset=%u\n", 
		       job->hdr.name, job_items[i].name, *def_svalue, i, offset);
		  svalue = (char **)((char *)job + offset);
		  if (*svalue) {
                     Dmsg1(000, "Hey something is wrong. p=0x%u\n", (unsigned)*svalue);
		  }
		  *svalue = bstrdup(*def_svalue);
		  set_bit(i, job->hdr.item_present);
	       } else if (job_items[i].handler == store_res) {
		  def_svalue = (char **)((char *)(job->jobdefs) + offset);
                  Dmsg4(400, "Job \"%s\", field \"%s\" item %d offset=%u\n", 
		       job->hdr.name, job_items[i].name, i, offset);
		  svalue = (char **)((char *)job + offset);
		  if (*svalue) {
                     Dmsg1(000, "Hey something is wrong. p=0x%u\n", (unsigned)*svalue);
		  }
		  *svalue = *def_svalue;
		  set_bit(i, job->hdr.item_present);
	       /*
		* Handle integer fields 
		*    Note, our store_yesno does not handle bitmaped fields
		*/
	       } else if (job_items[i].handler == store_yesno	||
			  job_items[i].handler == store_pint	||
			  job_items[i].handler == store_jobtype ||
			  job_items[i].handler == store_level	||
			  job_items[i].handler == store_pint	||
			  job_items[i].handler == store_replace) {
		  def_ivalue = (int *)((char *)(job->jobdefs) + offset);
                  Dmsg5(400, "Job \"%s\", field \"%s\" def_ivalue=%d item %d offset=%u\n", 
		       job->hdr.name, job_items[i].name, *def_ivalue, i, offset);
		  ivalue = (int *)((char *)job + offset);
		  *ivalue = *def_ivalue;
		  set_bit(i, job->hdr.item_present);
	       /*
		* Handle 64 bit integer fields 
		*/
	       } else if (job_items[i].handler == store_time   ||
			  job_items[i].handler == store_size   ||
			  job_items[i].handler == store_int64) {
		  def_lvalue = (int64_t *)((char *)(job->jobdefs) + offset);
                  Dmsg5(400, "Job \"%s\", field \"%s\" def_lvalue=%" lld " item %d offset=%u\n", 
		       job->hdr.name, job_items[i].name, *def_lvalue, i, offset);
		  lvalue = (int64_t *)((char *)job + offset);
		  *lvalue = *def_lvalue;
		  set_bit(i, job->hdr.item_present);
	       }
	    }
	 }
      }
      /* 
       * Ensure that all required items are present
       */
      for (i=0; job_items[i].name; i++) {
	 if (job_items[i].flags & ITEM_REQUIRED) {
	       if (!bit_is_set(i, job->hdr.item_present)) {  
                  Jmsg(NULL, M_FATAL, 0, "Field \"%s\" in Job \"%s\" resource is required, but not found.\n",
		    job_items[i].name, job->hdr.name);
		  OK = false;
		}
	 }
	 /* If this triggers, take a look at lib/parse_conf.h */
	 if (i >= MAX_RES_ITEMS) {
            Emsg0(M_ERROR_TERM, 0, "Too many items in Job resource\n");
	 }
      }
      if (job->client && job->client->catalog) {
	 CAT *catalog = job->client->catalog;
	 B_DB *db;

	 /*
	  * Make sure we can open catalog, otherwise print a warning
	  * message because the server is probably not running.
	  */
	 db = db_init_database(NULL, catalog->db_name, catalog->db_user,
			    catalog->db_password, catalog->db_address,
			    catalog->db_port, catalog->db_socket);
	 if (!db || !db_open_database(NULL, db)) {
            Jmsg(NULL, M_FATAL, 0, _("Could not open database \"%s\".\n"),
		 catalog->db_name);
	    if (db) {
               Jmsg(NULL, M_FATAL, 0, _("%s"), db_strerror(db));
	    }
	    OK = false;
	 } else {
	    /* If a pool is defined for this job, create the pool DB	   
	     *	record if it is not already created. 
	     */
	    if (job->pool) {
	       create_pool(NULL, db, job->pool, POOL_OP_UPDATE);  /* update request */
	    }
	    /* Set default value in all counters */
	    COUNTER *counter;
	    foreach_res(counter, R_COUNTER) {
	       /* Write to catalog? */
	       if (!counter->created && counter->Catalog == catalog) {
		  COUNTER_DBR cr;
		  bstrncpy(cr.Counter, counter->hdr.name, sizeof(cr.Counter));
		  cr.MinValue = counter->MinValue;
		  cr.MaxValue = counter->MaxValue;
		  cr.CurrentValue = counter->MinValue;
		  if (counter->WrapCounter) {
		     bstrncpy(cr.WrapCounter, counter->WrapCounter->hdr.name, sizeof(cr.WrapCounter));
		  } else {
		     cr.WrapCounter[0] = 0;  /* empty string */
		  }
		  if (db_create_counter_record(NULL, db, &cr)) {
		     counter->CurrentValue = cr.CurrentValue;
		     counter->created = true;
                     Dmsg2(100, "Create counter %s val=%d\n", counter->hdr.name, counter->CurrentValue);
		  }
	       } 
	       if (!counter->created) {
		  counter->CurrentValue = counter->MinValue;  /* default value */
	       }
	    }
	 }
	 db_close_database(NULL, db);
      }
   }

   UnlockRes();
   if (OK) {
      close_msg(NULL);		      /* close temp message handler */
      init_msg(NULL, director->messages); /* open daemon message handler */
   }
   return OK;
}
