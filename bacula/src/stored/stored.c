/*
 * Second generation Storage daemon.
 *
 * It accepts a number of simple commands from the File daemon
 * and acts on them. When a request to append data is made,
 * it opens a data channel and accepts data from the
 * File daemon. 
 *
 *   Version $Id$
 * 
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
#include "stored.h"

/* Imported functions */


/* Forward referenced functions */
void terminate_stored(int sig);
static void check_config();
static void *device_allocation(void *arg);

#define CONFIG_FILE "bacula-sd.conf"  /* Default config file */


/* Global variables exported */
char OK_msg[]   = "3000 OK\n";
char TERM_msg[] = "3999 Terminate\n";
STORES *me;			      /* our Global resource */

static uint32_t VolSessionId = 0;
uint32_t VolSessionTime;
char *configfile;

/* Global static variables */
static int foreground = 0;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static workq_t dird_workq;	      /* queue for processing connections */

static void usage()
{
   fprintf(stderr, _(
"\nVersion: " VERSION " (" BDATE ")\n\n"
"Usage: stored [-s -f ] [-c config_file] [-d debug_level]  [config_file]\n"
"        -c <file>   use <file> as configuration file\n"
"        -dnn        set debug level to nn\n"
"        -f          run in foreground (for debugging)\n"
"        -g          groupid\n"
"        -s          no signals (for debugging)\n"
"        -t          test - read config and exit\n"
"        -u          userid\n"
"        -v          verbose user messages\n"
"        -?          print this message.\n"
"\n"));
   exit(1);
}

/********************************************************************* 
 *
 *  Main Bacula Unix Storage Daemon
 *
 */
int main (int argc, char *argv[])
{
   int ch;   
   int no_signals = FALSE;
   int test_config = FALSE;
   pthread_t thid;
   char *uid = NULL;
   char *gid = NULL;

   init_stack_dump();
   my_name_is(argc, argv, "bacula-sd");
   textdomain("bacula-sd");
   init_msg(NULL, NULL);
   daemon_start_time = time(NULL);
   memset(&last_job, 0, sizeof(last_job));

   /* Sanity checks */
   if (TAPE_BSIZE % DEV_BSIZE != 0 || TAPE_BSIZE / DEV_BSIZE == 0) {
      Emsg2(M_ABORT, 0, "Tape block size (%d) not multiple of system size (%d)\n",
	 TAPE_BSIZE, DEV_BSIZE);
   }
   if (TAPE_BSIZE != (1 << (ffs(TAPE_BSIZE)-1))) {
      Emsg1(M_ABORT, 0, "Tape block size (%d) is not a power of 2\n", TAPE_BSIZE);
   }

   while ((ch = getopt(argc, argv, "c:d:fg:stu:v?")) != -1) {
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

         case 'g':                    /* set group id */
	    gid = optarg;
	    break;

         case 's':                    /* no signals */
	    no_signals = TRUE;
	    break;

         case 't':
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

   if (argc) {
      if (configfile != NULL) {
	 free(configfile);
      }
      configfile = bstrdup(*argv);
      argc--; 
      argv++;
   }
   if (argc)
      usage();

   if (!no_signals) {
      init_signals(terminate_stored);
   }


   if (configfile == NULL) {
      configfile = bstrdup(CONFIG_FILE);
   }

   parse_config(configfile);
   check_config();

   if (test_config) {
      terminate_stored(0);
   }

   if (!foreground) {
      daemon_start();		      /* become daemon */
      init_stack_dump();	      /* pick up new pid */
   }

   drop(uid, gid);

   create_pid_file(me->pid_directory, "bacula-sd", me->SDport);

   /* Ensure that Volume Session Time and Id are both
    * set and are both non-zero.
    */
   VolSessionTime = (long)daemon_start_time;
   if (VolSessionTime == 0) { /* paranoid */
      Emsg0(M_ABORT, 0, _("Volume Session Time is ZERO!\n"));
   }

   /* Make sure on Solaris we can run concurrent, watch dog + servers + misc */
   set_thread_concurrency(me->max_concurrent_jobs * 2 + 4);

    /*
     * Start the device allocation thread
     */
   if (pthread_create(&thid, NULL, device_allocation, NULL) != 0) {
      Emsg1(M_ABORT, 0, _("Unable to create thread. ERR=%s\n"), strerror(errno));
   }

   start_watchdog();		      /* start watchdog thread */

   /* 
    * Sleep a bit to give device thread a chance to lock the resource
    * chain before we start the server.
    */
   bmicrosleep(1, 0);
				 
   /* Single server used for Director and File daemon */
   bnet_thread_server(me->SDaddr, me->SDport, me->max_concurrent_jobs * 2 + 1,
		      &dird_workq, connection_request);
   exit(1);			      /* to keep compiler quiet */
}

/* Return a new Session Id */
uint32_t newVolSessionId()
{
   uint32_t Id;

   P(mutex);
   VolSessionId++;
   Id = VolSessionId;
   V(mutex);
   return Id;
}

/* Check Configuration file for necessary info */
static void check_config()
{
   LockRes();
   me = (STORES *)GetNextRes(R_STORAGE, NULL);
   if (!me) {
      UnlockRes();
      Emsg1(M_ERROR_TERM, 0, _("No Storage resource defined in %s. Cannot continue.\n"),
	 configfile);
   }

   my_name_is(0, (char **)NULL, me->hdr.name);	   /* Set our real name */

   if (GetNextRes(R_STORAGE, (RES *)me) != NULL) {
      UnlockRes();
      Emsg1(M_ERROR_TERM, 0, _("Only one Storage resource permitted in %s\n"), 
	 configfile);
   }
   if (GetNextRes(R_DIRECTOR, NULL) == NULL) {
      UnlockRes();
      Emsg1(M_ERROR_TERM, 0, _("No Director resource defined in %s. Cannot continue.\n"),
	 configfile);
   }
   if (GetNextRes(R_DEVICE, NULL) == NULL){
      UnlockRes();
      Emsg1(M_ERROR_TERM, 0, _("No Device resource defined in %s. Cannot continue.\n"),
	   configfile);
   }
   if (!me->messages) {
      me->messages = (MSGS *)GetNextRes(R_MSGS, NULL);
      if (!me->messages) {
         Emsg1(M_ERROR_TERM, 0, _("No Messages resource defined in %s. Cannot continue.\n"),
	    configfile);
      }
   }
   close_msg(NULL);		      /* close temp message handler */
   init_msg(NULL, me->messages);      /* open daemon message handler */

   UnlockRes();

   if (!me->working_directory) {
      Emsg1(M_ERROR_TERM, 0, _("No Working Directory defined in %s. Cannot continue.\n"),
	 configfile);
   }
   
   set_working_directory(me->working_directory);
}

/*
 * We are started as a separate thread.  The
 *  resources are alread locked.
 */
static void *device_allocation(void *arg)
{
   int i;
   DEVRES *device;

   LockRes();
   pthread_detach(pthread_self());

   for (device=NULL,i=0;  (device=(DEVRES *)GetNextRes(R_DEVICE, (RES *)device)); i++) {
      Dmsg1(90, "calling init_dev %s\n", device->device_name);
      device->dev = init_dev(NULL, device);
      Dmsg1(10, "SD init done %s\n", device->device_name);
      if (!device->dev) {
         Emsg1(M_ERROR, 0, _("Could not initialize %s\n"), device->device_name);
	 continue;
      }
      if (device->cap_bits & CAP_ALWAYSOPEN) {
         Dmsg1(20, "calling open_device %s\n", device->device_name);
	 if (!open_device(device->dev)) {
            Emsg1(M_ERROR, 0, _("Could not open device %s\n"), device->device_name);
	 }
      }
      if (device->cap_bits & CAP_AUTOMOUNT && device->dev && 
	  device->dev->state & ST_OPENED) {
	 DEV_BLOCK *block;
	 JCR *jcr;
	 block = new_block(device->dev);
	 jcr = new_jcr(sizeof(JCR), stored_free_jcr);
	 switch (read_dev_volume_label(jcr, device->dev, block)) {
	    case VOL_OK:
	       break;
	    default:
               Emsg1(M_WARNING, 0, _("Could not mount device %s\n"), device->device_name);
	       break;
	 }
	 free_jcr(jcr);
	 free_block(block);
      }
   } 
   UnlockRes();
   return NULL;
}


/* Clean up and then exit */
void terminate_stored(int sig)
{
   static int in_here = FALSE;
   DEVRES *device;

   if (in_here) {		      /* prevent loops */
      exit(1);
   }
   in_here = TRUE;

   delete_pid_file(me->pid_directory, "bacula-sd", me->SDport);
   stop_watchdog();

   Dmsg0(200, "In terminate_stored()\n");

   LockRes();
   for (device=NULL; (device=(DEVRES *)GetNextRes(R_DEVICE, (RES *)device)); ) {
      if (device->dev) {
	 term_dev(device->dev);
      }
   } 
   UnlockRes();

   if (configfile)
   free(configfile);
   free_config_resources();

   if (debug_level > 10) {
      print_memory_pool_stats();
   }
   term_msg();
   close_memory_pool();

   sm_dump(False);		      /* dump orphaned buffers */
   exit(1);
}
