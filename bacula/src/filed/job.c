/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2000-2014 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from many
   others, a complete list can be found in the file AUTHORS.

   You may use this file and others of this release according to the
   license defined in the LICENSE file, which includes the Affero General
   Public License, v3.0 ("AGPLv3") and some additional permissions and
   terms pursuant to its AGPLv3 Section 7.

   Bacula® is a registered trademark of Kern Sibbald.
*/
/*
 *  Bacula File Daemon Job processing
 *
 *    Written by Kern Sibbald, October MM
 *
 */

#include "bacula.h"
#include "filed.h"
#include "ch.h"
#ifdef WIN32_VSS
#include "vss.h"
static pthread_mutex_t vss_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

/* Globals */
bool win32decomp = false;
bool no_win32_write_errors = false;
static int enable_vss = 0;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

/*
 * As Windows saves ACLs as part of the standard backup stream
 * we just pretend here that is has implicit acl support.
 */
#if defined(HAVE_ACL) || defined(HAVE_WIN32)
const bool have_acl = true;
#else
const bool have_acl = false;
#endif

#if defined(HAVE_XATTR)
const bool have_xattr = true;
#else
const bool have_xattr = false;
#endif

extern CLIENT *me;                    /* our client resource */

/* Imported functions */
extern int status_cmd(JCR *jcr);
extern int qstatus_cmd(JCR *jcr);
extern int accurate_cmd(JCR *jcr);

/* Forward referenced functions */
static int backup_cmd(JCR *jcr);
static int cancel_cmd(JCR *jcr);
static int setdebug_cmd(JCR *jcr);
static int setbandwidth_cmd(JCR *jcr);
static int estimate_cmd(JCR *jcr);
static int hello_cmd(JCR *jcr);
static int job_cmd(JCR *jcr);
static int fileset_cmd(JCR *jcr);
static int level_cmd(JCR *jcr);
static int verify_cmd(JCR *jcr);
static int restore_cmd(JCR *jcr);
static int end_restore_cmd(JCR *jcr);
static int storage_cmd(JCR *jcr);
static int session_cmd(JCR *jcr);
static int response(JCR *jcr, BSOCK *sd, char *resp, const char *cmd);
static void filed_free_jcr(JCR *jcr);
static int open_sd_read_session(JCR *jcr);
static int runscript_cmd(JCR *jcr);
static int runbefore_cmd(JCR *jcr);
static int runafter_cmd(JCR *jcr);
static int runbeforenow_cmd(JCR *jcr);
static int restore_object_cmd(JCR *jcr);
static int set_options(findFOPTS *fo, const char *opts);
static void set_storage_auth_key(JCR *jcr, char *key);
static int sm_dump_cmd(JCR *jcr);
#ifdef DEVELOPER
static int exit_cmd(JCR *jcr);
#endif

/* Exported functions */


/*
 * The following are the recognized commands from the Director.
 */
struct s_cmds cmds[] = {
   {"backup",       backup_cmd,    0},
   {"cancel",       cancel_cmd,    0},
   {"setdebug=",    setdebug_cmd,  0},
   {"setbandwidth=",setbandwidth_cmd, 0},
   {"estimate",     estimate_cmd,  0},
   {"Hello",        hello_cmd,     1},
   {"fileset",      fileset_cmd,   0},
   {"JobId=",       job_cmd,       0},
   {"level = ",     level_cmd,     0},
   {"restore ",     restore_cmd,   0},
   {"endrestore",   end_restore_cmd, 0},
   {"session",      session_cmd,   0},
   {"status",       status_cmd,    1},
   {".status",      qstatus_cmd,   1},
   {"storage ",     storage_cmd,   0},
   {"verify",       verify_cmd,    0},
   {"RunBeforeNow", runbeforenow_cmd, 0},
   {"RunBeforeJob", runbefore_cmd, 0},
   {"RunAfterJob",  runafter_cmd,  0},
   {"Run",          runscript_cmd, 0},
   {"accurate",     accurate_cmd,  0},
   {"restoreobject", restore_object_cmd, 0},
   {"sm_dump",      sm_dump_cmd, 0},
#ifdef DEVELOPER
   {"exit",         exit_cmd, 0},
#endif
   {NULL,       NULL}                  /* list terminator */
};

/* Commands received from director that need scanning */
static char jobcmd[]      = "JobId=%d Job=%127s SDid=%d SDtime=%d Authorization=%100s";
static char storaddr[]    = "storage address=%s port=%d ssl=%d Authorization=%100s";
static char storaddr_v1[] = "storage address=%s port=%d ssl=%d";
static char sessioncmd[]  = "session %127s %ld %ld %ld %ld %ld %ld\n";

static char restorecmd1[] = "restore replace=%c prelinks=%d where=\n";
static char restorefcmd1[] = "restore files=%d replace=%c prelinks=%d where=\n";

/* The following restore commands may have a big where=/regexwhere= parameter
 * the bsscanf is limiting the default %s to 1000c. To allow more than 1000 bytes,
 * we can specify %xxxxs where xxxx is the size expected in bytes.
 *
 * So, the code will add %s\n to the end of the following restore commands
 */
static char restorecmd[]  = "restore replace=%c prelinks=%d where=";
static char restorecmdR[] = "restore replace=%c prelinks=%d regexwhere=";
static char restorefcmd[]  = "restore files=%d replace=%c prelinks=%d where=";
static char restorefcmdR[] = "restore files=%d replace=%c prelinks=%d regexwhere=";

static char restoreobjcmd[]  = "restoreobject JobId=%u %d,%d,%d,%d,%d,%d,%s";
static char restoreobjcmd1[] = "restoreobject JobId=%u %d,%d,%d,%d,%d,%d\n";
static char endrestoreobjectcmd[] = "restoreobject end\n";
static char verifycmd[]   = "verify level=%30s";
static char estimatecmd[] = "estimate listing=%d";
static char runbefore[]   = "RunBeforeJob %s";
static char runafter[]    = "RunAfterJob %s";
static char runscript[]   = "Run OnSuccess=%d OnFailure=%d AbortOnError=%d When=%d Command=%s";
static char setbandwidth[]= "setbandwidth=%lld Job=%127s";

/* Responses sent to Director */
static char errmsg[]      = "2999 Invalid command\n";
static char no_auth[]     = "2998 No Authorization\n";
static char invalid_cmd[] = "2997 Invalid command for a Director with Monitor directive enabled.\n";
static char OKBandwidth[] = "2000 OK Bandwidth\n";
static char OKinc[]       = "2000 OK include\n";
static char OKest[]       = "2000 OK estimate files=%s bytes=%s\n";
static char OKlevel[]     = "2000 OK level\n";
static char OKbackup[]    = "2000 OK backup\n";
static char OKverify[]    = "2000 OK verify\n";
static char OKrestore[]   = "2000 OK restore\n";
static char OKsession[]   = "2000 OK session\n";
static char OKstore[]     = "2000 OK storage\n";
static char OKstoreend[]  = "2000 OK storage end\n";
static char OKjob[]       = "2000 OK Job %s (%s) %s,%s,%s";
static char OKsetdebug[]  = "2000 OK setdebug=%ld trace=%ld hangup=%ld options=%s tags=%s\n";
static char BADjob[]      = "2901 Bad Job\n";
static char EndJob[]      = "2800 End Job TermCode=%d JobFiles=%d ReadBytes=%lld"
                            " JobBytes=%lld Errors=%d VSS=%d Encrypt=%d\n";
static char OKRunBefore[] = "2000 OK RunBefore\n";
static char OKRunBeforeNow[] = "2000 OK RunBeforeNow\n";
static char OKRunAfter[]  = "2000 OK RunAfter\n";
static char OKRunScript[] = "2000 OK RunScript\n";
static char BADcmd[]      = "2902 Bad %s\n";
static char OKRestoreObject[] = "2000 OK ObjectRestored\n";


/* Responses received from Storage Daemon */
static char OK_end[]       = "3000 OK end\n";
static char OK_close[]     = "3000 OK close Status = %d\n";
static char OK_open[]      = "3000 OK open ticket = %d\n";
static char OK_data[]      = "3000 OK data\n";
static char OK_append[]    = "3000 OK append data\n";


/* Commands sent to Storage Daemon */
static char append_open[]  = "append open session\n";
static char append_data[]  = "append data %d\n";
static char append_end[]   = "append end session %d\n";
static char append_close[] = "append close session %d\n";
static char read_open[]    = "read open session = %s %ld %ld %ld %ld %ld %ld\n";
static char read_data[]    = "read data %d\n";
static char read_close[]   = "read close session %d\n";

/*
 * Accept requests from a Director
 *
 * NOTE! We are running as a separate thread
 *
 * Send output one line
 * at a time followed by a zero length transmission.
 *
 * Return when the connection is terminated or there
 * is an error.
 *
 * Basic task here is:
 *   Authenticate Director (during Hello command).
 *   Accept commands one at a time from the Director
 *     and execute them.
 *
 * Concerning ClientRunBefore/After, the sequence of events
 * is rather critical. If they are not done in the right
 * order one can easily get FD->SD timeouts if the script
 * runs a long time.
 *
 * The current sequence of events is:
 *  1. Dir starts job with FD
 *  2. Dir connects to SD
 *  3. Dir connects to FD
 *  4. FD connects to SD
 *  5. FD gets/runs ClientRunBeforeJob and sends ClientRunAfterJob
 *  6. Dir sends include/exclude
 *  7. FD sends data to SD
 *  8. SD/FD disconnects while SD despools data and attributes (optional)
 *  9. FD runs ClientRunAfterJob
 */

static void *handle_director_request(BSOCK *dir)
{
   int i;
   bool found, quit;
   bool first = true;
   JCR *jcr;
   const char jobname[12] = "*Director*";

   jcr = new_jcr(sizeof(JCR), filed_free_jcr); /* create JCR */
   jcr->sd_calls_client = false;
   jcr->dir_bsock = dir;
   jcr->ff = init_find_files();
   jcr->start_time = time(NULL);
   jcr->RunScripts = New(alist(10, not_owned_by_alist));
   jcr->last_fname = get_pool_memory(PM_FNAME);
   jcr->last_fname[0] = 0;
   jcr->client_name = get_memory(strlen(my_name) + 1);
   pm_strcpy(jcr->client_name, my_name);
   bstrncpy(jcr->Job, jobname, sizeof(jobname));  /* dummy */
   jcr->crypto.pki_sign = me->pki_sign;
   jcr->crypto.pki_encrypt = me->pki_encrypt;
   jcr->crypto.pki_keypair = me->pki_keypair;
   jcr->crypto.pki_signers = me->pki_signers;
   jcr->crypto.pki_recipients = me->pki_recipients;
   dir->set_jcr(jcr);
   /* Initialize SD start condition variable */
   int errstat = pthread_cond_init(&jcr->job_start_wait, NULL);
   if (errstat != 0) {
      berrno be;
      Jmsg1(jcr, M_FATAL, 0, _("Unable to init job cond variable: ERR=%s\n"), be.bstrerror(errstat));
      goto bail_out;
   }

   enable_backup_privileges(NULL, 1 /* ignore_errors */);

   for (quit=false; !quit;) {
      if (!first) {      /* first call the read is done */
         /* Read command */
         if (dir->recv() < 0) {
            break;               /* connection terminated */
         }
      }
      if (dir->msglen == 0) {    /* Bad connection */
         break;
      }
      first = false;
      dir->msg[dir->msglen] = 0;
      Dmsg1(100, "<dird: %s", dir->msg);
      found = false;
      for (i=0; cmds[i].cmd; i++) {
         if (strncmp(cmds[i].cmd, dir->msg, strlen(cmds[i].cmd)) == 0) {
            found = true;         /* indicate command found */
            if (!jcr->authenticated && cmds[i].func != hello_cmd) {
               dir->fsend(no_auth);
               dir->signal(BNET_EOD);
               break;
            }
            if ((jcr->authenticated) && (!cmds[i].monitoraccess) && (jcr->director->monitor)) {
               Dmsg1(100, "Command \"%s\" is invalid.\n", cmds[i].cmd);
               dir->fsend(invalid_cmd);
               dir->signal(BNET_EOD);
               break;
            }
            if ((me->disabled_cmds_array && me->disabled_cmds_array[i]) ||
                (jcr->director && jcr->director->disabled_cmds_array &&
                 jcr->director->disabled_cmds_array[i])) {
                Jmsg(jcr, M_FATAL, 0, _("Command: \"%s\" is disabled.\n"), cmds[i].cmd);
                quit = true;
                break;
            }
            Dmsg1(100, "Executing Dir %s command.\n", dir->msg);
            if (!cmds[i].func(jcr)) {         /* do command */
               quit = true;         /* error or fully terminated, get out */
               Dmsg1(100, "Quit command loop. Canceled=%d\n", job_canceled(jcr));
            }
            break;
         }
      }
      if (!found) {              /* command not found */
         dir->fsend(errmsg);
         quit = true;
         break;
      }
   }

   /* Inform Storage daemon that we are done */
   if (jcr->store_bsock) {
      jcr->store_bsock->signal(BNET_TERMINATE);
   }

   /* Run the after job */
   run_scripts(jcr, jcr->RunScripts, "ClientAfterJob");

   if (jcr->JobId) {            /* send EndJob if running a job */
      uint32_t vss, encrypt;
      encrypt = jcr->crypto.pki_encrypt;
      vss = jcr->VSS;
      dir->fsend(EndJob, jcr->JobStatus, jcr->JobFiles,
              jcr->ReadBytes, jcr->JobBytes, jcr->JobErrors, vss,
              encrypt);
      //Dmsg0(0/*110*/, dir->msg);
   }

   generate_daemon_event(jcr, "JobEnd");
   generate_plugin_event(jcr, bEventJobEnd);

bail_out:
   dequeue_messages(jcr);             /* send any queued messages */

   /* Inform Director that we are done */
   dir->signal(BNET_TERMINATE);

   free_plugins(jcr);                 /* release instantiated plugins */
   free_and_null_pool_memory(jcr->job_metadata);

   /* Clean up fileset */
   FF_PKT *ff = jcr->ff;
   findFILESET *fileset = ff->fileset;
   if (fileset) {
      int i, j, k;
      /* Delete FileSet Include lists */
      for (i=0; i<fileset->include_list.size(); i++) {
         findINCEXE *incexe = (findINCEXE *)fileset->include_list.get(i);
         for (j=0; j<incexe->opts_list.size(); j++) {
            findFOPTS *fo = (findFOPTS *)incexe->opts_list.get(j);
            if (fo->plugin) {
               free(fo->plugin);
            }
            for (k=0; k<fo->regex.size(); k++) {
               regfree((regex_t *)fo->regex.get(k));
            }
            for (k=0; k<fo->regexdir.size(); k++) {
               regfree((regex_t *)fo->regexdir.get(k));
            }
            for (k=0; k<fo->regexfile.size(); k++) {
               regfree((regex_t *)fo->regexfile.get(k));
            }
            fo->regex.destroy();
            fo->regexdir.destroy();
            fo->regexfile.destroy();
            fo->wild.destroy();
            fo->wilddir.destroy();
            fo->wildfile.destroy();
            fo->wildbase.destroy();
            fo->base.destroy();
            fo->fstype.destroy();
            fo->drivetype.destroy();
         }
         incexe->opts_list.destroy();
         incexe->name_list.destroy();
         incexe->plugin_list.destroy();
         if (incexe->ignoredir) {
            free(incexe->ignoredir);
         }
      }
      fileset->include_list.destroy();

      /* Delete FileSet Exclude lists */
      for (i=0; i<fileset->exclude_list.size(); i++) {
         findINCEXE *incexe = (findINCEXE *)fileset->exclude_list.get(i);
         for (j=0; j<incexe->opts_list.size(); j++) {
            findFOPTS *fo = (findFOPTS *)incexe->opts_list.get(j);
            fo->regex.destroy();
            fo->regexdir.destroy();
            fo->regexfile.destroy();
            fo->wild.destroy();
            fo->wilddir.destroy();
            fo->wildfile.destroy();
            fo->wildbase.destroy();
            fo->base.destroy();
            fo->fstype.destroy();
            fo->drivetype.destroy();
         }
         incexe->opts_list.destroy();
         incexe->name_list.destroy();
         incexe->plugin_list.destroy();
         if (incexe->ignoredir) {
            free(incexe->ignoredir);
         }
      }
      fileset->exclude_list.destroy();
      free(fileset);
   }
   ff->fileset = NULL;
   Dmsg0(100, "Calling term_find_files\n");
   term_find_files(jcr->ff);
   jcr->ff = NULL;
   Dmsg0(100, "Done with term_find_files\n");
   pthread_cond_destroy(&jcr->job_start_wait);
   free_jcr(jcr);                     /* destroy JCR record */
   Dmsg0(100, "Done with free_jcr\n");
   Dsm_check(100);
   garbage_collect_memory_pool();
   return NULL;
}

/*
 * Note, we handle the initial connection request here.
 *   We only get the jobname and the SD version, then we
 *   return, authentication will be done when the Director
 *   sends the storage command -- as is usually the case.
 *   This should be called only once by the SD.
 */
static void *handle_storage_request(BSOCK *sd)
{
   char job_name[500];
   char tbuf[150];
   int sd_version;
   JCR *jcr;

   if (sscanf(sd->msg, "Hello FD: Bacula Storage calling Start Job %127s %d\n",
       job_name, &sd_version) != 2) {
      Jmsg(NULL, M_FATAL, 0, _("SD connect failed: Bad Hello command\n"));
      return NULL;
   }
   Dmsg1(110, "Got a SD connection at %s\n", bstrftimes(tbuf, sizeof(tbuf),
         (utime_t)time(NULL)));
   Dmsg1(50, "%s", sd->msg);

   if (!(jcr=get_jcr_by_full_name(job_name))) {
      Jmsg1(NULL, M_FATAL, 0, _("SD connect failed: Job name not found: %s\n"), job_name);
      Dmsg1(3, "**** Job \"%s\" not found.\n", job_name);
      sd->destroy();
      return NULL;
   }

   Dmsg1(150, "Found Job %s\n", job_name);

   jcr->store_bsock = sd;
   jcr->store_bsock->set_jcr(jcr);

   if (!jcr->max_bandwidth) {
      if (jcr->director->max_bandwidth_per_job) {
         jcr->max_bandwidth = jcr->director->max_bandwidth_per_job;

      } else if (me->max_bandwidth_per_job) {
         jcr->max_bandwidth = me->max_bandwidth_per_job;
      }
   }
   sd->set_bwlimit(jcr->max_bandwidth);
   pthread_cond_signal(&jcr->job_start_wait); /* wake waiting job */
   free_jcr(jcr);
   return NULL;
}

/*
 * Accept requests from a Director or a Storage daemon
 */
void *handle_connection_request(void *caller)
{
   BSOCK *bs = (BSOCK *)caller;

   if (bs->recv() > 0) {
      if (strncmp(bs->msg, "Ping", 4) == 0) {
         bs->fsend("2000 Ping OK\n");
         bs->destroy();
         return NULL;
      }
      if (bs->msglen < 25 || bs->msglen > 500) {
         goto bail_out;
      }
      Dmsg1(100, "Got: %s", bs->msg);
      if (strncmp(bs->msg, "Hello Director", 14) == 0) {
         return handle_director_request(bs);
      }
      if (strncmp(bs->msg, "Hello FD: Bacula Storage", 20) ==0) {
         return handle_storage_request(bs);
      }
   }
bail_out:
   Dmsg2(100, "Bad command from %s. Len=%d.\n", bs->who(), bs->msglen);
   char addr[64];
   char *who = bs->get_peer(addr, sizeof(addr)) ? bs->who() : addr;
   Jmsg2(NULL, M_FATAL, 0, _("Bad command from %s. Len=%d.\n"), who, bs->msglen);
   bs->destroy();
   return NULL;
}

static int sm_dump_cmd(JCR *jcr)
{
   close_memory_pool();
   sm_dump(false, true);
   jcr->dir_bsock->fsend("2000 sm_dump OK\n");
   return 1;
}

#ifdef DEVELOPER
static int exit_cmd(JCR *jcr)
{
   jcr->dir_bsock->fsend("2000 exit OK\n");
   terminate_filed(0);
   return 0;
}
#endif


/**
 * Hello from Director he must identify himself and provide his
 *  password.
 */
static int hello_cmd(JCR *jcr)
{
   Dmsg0(120, "Calling Authenticate\n");
   if (!authenticate_director(jcr)) {
      return 0;
   }
   Dmsg0(120, "OK Authenticate\n");
   jcr->authenticated = true;

   return 1;
}

/**
 * Cancel a Job
 */
static int cancel_cmd(JCR *jcr)
{
   BSOCK *dir = jcr->dir_bsock;
   char Job[MAX_NAME_LENGTH];
   JCR *cjcr;
   int status;
   const char *reason;

   if (sscanf(dir->msg, "cancel Job=%127s", Job) == 1) {
      status = JS_Canceled;
      reason = "canceled";
   } else {
      dir->fsend(_("2902 Error scanning cancel command.\n"));
      goto bail_out;
   }
   if (!(cjcr=get_jcr_by_full_name(Job))) {
      dir->fsend(_("2901 Job %s not found.\n"), Job);
   } else {
      generate_plugin_event(cjcr, bEventCancelCommand, NULL);
      cjcr->setJobStatus(status);
      if (cjcr->store_bsock) {
         cjcr->store_bsock->set_timed_out();
         cjcr->store_bsock->set_terminated();
         cjcr->my_thread_send_signal(TIMEOUT_SIGNAL);
      }
      free_jcr(cjcr);
      dir->fsend(_("2001 Job \"%s\" marked to be %s.\n"),
         Job, reason);
   }

bail_out:
   dir->signal(BNET_EOD);
   return 1;
}

/**
 * Set bandwidth limit as requested by the Director
 *
 */
static int setbandwidth_cmd(JCR *jcr)
{
   BSOCK *dir = jcr->dir_bsock;
   int64_t bw=0;
   JCR *cjcr;
   char Job[MAX_NAME_LENGTH];
   *Job=0;

   if (sscanf(dir->msg, setbandwidth, &bw, Job) != 2 || bw < 0) {
      pm_strcpy(jcr->errmsg, dir->msg);
      dir->fsend(_("2991 Bad setbandwidth command: %s\n"), jcr->errmsg);
      return 0;
   }

   if (*Job) {
      if(!(cjcr=get_jcr_by_full_name(Job))) {
         dir->fsend(_("2901 Job %s not found.\n"), Job);
      } else {
         cjcr->max_bandwidth = bw;
         if (cjcr->store_bsock) {
            cjcr->store_bsock->set_bwlimit(bw);
         }
         free_jcr(cjcr);
      }

   } else {                           /* No job requested, apply globally */
      me->max_bandwidth_per_job = bw; /* Overwrite directive */
      foreach_jcr(cjcr) {
         cjcr->max_bandwidth = bw;
         if (cjcr->store_bsock) {
            cjcr->store_bsock->set_bwlimit(bw);
         }
      }
      endeach_jcr(cjcr);
   }

   return dir->fsend(OKBandwidth);
}

/**
 * Set debug level as requested by the Director
 *
 */
static int setdebug_cmd(JCR *jcr)
{
   BSOCK *dir = jcr->dir_bsock;
   int32_t trace, hangup, lvl;
   int64_t level;
   int scan;
   char options[60];
   char tags[512];

   Dmsg1(50, "setdebug_cmd: %s", dir->msg);
   tags[0] = options[0] = 0;
   scan = sscanf(dir->msg, "setdebug=%ld trace=%ld hangup=%ld options=%55s tags=%511s",
                 &lvl, &trace, &hangup, options, tags);
   if (scan != 5) {
      scan = sscanf(dir->msg, "setdebug=%ld trace=%ld hangup=%ld",
                    &lvl, &trace, &hangup);
      if (scan != 3) {
         Dmsg2(20, "sscanf failed: msg=%s scan=%d\n", dir->msg, scan);
         if (sscanf(dir->msg, "setdebug=%ld trace=%ld", &lvl, &trace) != 2) {
            pm_strcpy(jcr->errmsg, dir->msg);
            dir->fsend(_("2991 Bad setdebug command: %s\n"), jcr->errmsg);
            return 0;
         } else {
            hangup = -1;
         }
      }
   }
   level = lvl;
   set_trace(trace);
   set_hangup(hangup);
   if (!debug_parse_tags(tags, &level)) {
      *tags = 0;
   }
   if (level >= 0) {
      debug_level = level;
   }

   /* handle other options */
   set_debug_flags(options);

   Dmsg5(150, "level=%ld trace=%ld hangup=%ld options=%s tags=%s\n",
         lvl, get_trace(), get_hangup(), options, tags);
   return dir->fsend(OKsetdebug, lvl, get_trace(), get_hangup(), options, tags);
}


static int estimate_cmd(JCR *jcr)
{
   BSOCK *dir = jcr->dir_bsock;
   char ed1[50], ed2[50];

   if (sscanf(dir->msg, estimatecmd, &jcr->listing) != 1) {
      pm_strcpy(jcr->errmsg, dir->msg);
      Jmsg(jcr, M_FATAL, 0, _("Bad estimate command: %s"), jcr->errmsg);
      dir->fsend(_("2992 Bad estimate command.\n"));
      return 0;
   }
   make_estimate(jcr);
   dir->fsend(OKest, edit_uint64_with_commas(jcr->num_files_examined, ed1),
      edit_uint64_with_commas(jcr->JobBytes, ed2));
   dir->signal(BNET_EOD);
   return 1;
}

/**
 * Get JobId and Storage Daemon Authorization key from Director
 */
static int job_cmd(JCR *jcr)
{
   BSOCK *dir = jcr->dir_bsock;
   POOL_MEM sd_auth_key(PM_MESSAGE);
   sd_auth_key.check_size(dir->msglen);

   if (sscanf(dir->msg, jobcmd,  &jcr->JobId, jcr->Job,
              &jcr->VolSessionId, &jcr->VolSessionTime,
              sd_auth_key.c_str()) != 5) {
      pm_strcpy(jcr->errmsg, dir->msg);
      Jmsg(jcr, M_FATAL, 0, _("Bad Job Command: %s"), jcr->errmsg);
      dir->fsend(BADjob);
      return 0;
   }
   set_storage_auth_key(jcr, sd_auth_key.c_str());
   Dmsg2(120, "JobId=%d Auth=%s\n", jcr->JobId, jcr->sd_auth_key);
   Mmsg(jcr->errmsg, "JobId=%d Job=%s", jcr->JobId, jcr->Job);
   new_plugins(jcr);                  /* instantiate plugins for this jcr */
   generate_plugin_event(jcr, bEventJobStart, (void *)jcr->errmsg);
   return dir->fsend(OKjob, VERSION, LSMDATE, HOST_OS, DISTNAME, DISTVER);
}

extern "C" char *job_code_callback_filed(JCR *jcr, const char* param)
{
   switch (param[0]) {
      case 'D':
         if (jcr->director) {
            return jcr->director->hdr.name;
         }
         break;
   }
   return NULL;

}

static int runbefore_cmd(JCR *jcr)
{
   bool ok;
   BSOCK *dir = jcr->dir_bsock;
   POOLMEM *cmd = get_memory(dir->msglen+1);
   RUNSCRIPT *script;

   Dmsg1(100, "runbefore_cmd: %s", dir->msg);
   if (sscanf(dir->msg, runbefore, cmd) != 1) {
      pm_strcpy(jcr->errmsg, dir->msg);
      Jmsg1(jcr, M_FATAL, 0, _("Bad RunBeforeJob command: %s\n"), jcr->errmsg);
      dir->fsend(_("2905 Bad RunBeforeJob command.\n"));
      free_memory(cmd);
      return 0;
   }
   unbash_spaces(cmd);

   /* Run the command now */
   script = new_runscript();
   script->set_job_code_callback(job_code_callback_filed);
   script->set_command(cmd);
   script->when = SCRIPT_Before;
   ok = script->run(jcr, "ClientRunBeforeJob");
   free_runscript(script);

   free_memory(cmd);
   if (ok) {
      dir->fsend(OKRunBefore);
      return 1;
   } else {
      dir->fsend(_("2905 Bad RunBeforeJob command.\n"));
      return 0;
   }
}

static int runbeforenow_cmd(JCR *jcr)
{
   BSOCK *dir = jcr->dir_bsock;

   run_scripts(jcr, jcr->RunScripts, "ClientBeforeJob");
   if (job_canceled(jcr)) {
      dir->fsend(_("2905 Bad RunBeforeNow command.\n"));
      Dmsg0(100, "Back from run_scripts ClientBeforeJob now: FAILED\n");
      return 0;
   } else {
      dir->fsend(OKRunBeforeNow);
      Dmsg0(100, "Back from run_scripts ClientBeforeJob now: OK\n");
      return 1;
   }
}

static int runafter_cmd(JCR *jcr)
{
   BSOCK *dir = jcr->dir_bsock;
   POOLMEM *msg = get_memory(dir->msglen+1);
   RUNSCRIPT *cmd;

   Dmsg1(100, "runafter_cmd: %s", dir->msg);
   if (sscanf(dir->msg, runafter, msg) != 1) {
      pm_strcpy(jcr->errmsg, dir->msg);
      Jmsg1(jcr, M_FATAL, 0, _("Bad RunAfter command: %s\n"), jcr->errmsg);
      dir->fsend(_("2905 Bad RunAfterJob command.\n"));
      free_memory(msg);
      return 0;
   }
   unbash_spaces(msg);

   cmd = new_runscript();
   cmd->set_job_code_callback(job_code_callback_filed);
   cmd->set_command(msg);
   cmd->on_success = true;
   cmd->on_failure = false;
   cmd->when = SCRIPT_After;

   jcr->RunScripts->append(cmd);

   free_pool_memory(msg);
   return dir->fsend(OKRunAfter);
}

static int runscript_cmd(JCR *jcr)
{
   BSOCK *dir = jcr->dir_bsock;
   POOLMEM *msg = get_memory(dir->msglen+1);
   int on_success, on_failure, fail_on_error;

   RUNSCRIPT *cmd = new_runscript() ;
   cmd->set_job_code_callback(job_code_callback_filed);

   Dmsg1(100, "runscript_cmd: '%s'\n", dir->msg);
   /* Note, we cannot sscanf into bools */
   if (sscanf(dir->msg, runscript, &on_success,
                                  &on_failure,
                                  &fail_on_error,
                                  &cmd->when,
                                  msg) != 5) {
      pm_strcpy(jcr->errmsg, dir->msg);
      Jmsg1(jcr, M_FATAL, 0, _("Bad RunScript command: %s\n"), jcr->errmsg);
      dir->fsend(_("2905 Bad RunScript command.\n"));
      free_runscript(cmd);
      free_memory(msg);
      return 0;
   }
   cmd->on_success = on_success;
   cmd->on_failure = on_failure;
   cmd->fail_on_error = fail_on_error;
   unbash_spaces(msg);

   cmd->set_command(msg);
   cmd->debug();
   jcr->RunScripts->append(cmd);

   free_pool_memory(msg);
   return dir->fsend(OKRunScript);
}

/*
 * This reads data sent from the Director from the
 *   RestoreObject table that allows us to get objects
 *   that were backed up (VSS .xml data) and are needed
 *   before starting the restore.
 */
static int restore_object_cmd(JCR *jcr)
{
   BSOCK *dir = jcr->dir_bsock;
   int32_t FileIndex;
   restore_object_pkt rop;

   memset(&rop, 0, sizeof(rop));
   rop.pkt_size = sizeof(rop);
   rop.pkt_end = sizeof(rop);

   Dmsg1(100, "Enter restoreobject_cmd: %s", dir->msg);
   if (strcmp(dir->msg, endrestoreobjectcmd) == 0) {
      Dmsg0(20, "Got endrestoreobject\n");
      generate_plugin_event(jcr, bEventRestoreObject, NULL);
      return dir->fsend(OKRestoreObject);
   }

   rop.plugin_name = (char *)malloc(dir->msglen);
   *rop.plugin_name = 0;

   if (sscanf(dir->msg, restoreobjcmd, &rop.JobId, &rop.object_len,
              &rop.object_full_len, &rop.object_index,
              &rop.object_type, &rop.object_compression, &FileIndex,
              rop.plugin_name) != 8) {

      /* Old version, no plugin_name */
      if (sscanf(dir->msg, restoreobjcmd1, &rop.JobId, &rop.object_len,
                 &rop.object_full_len, &rop.object_index,
                 &rop.object_type, &rop.object_compression, &FileIndex) != 7) {
         Dmsg0(5, "Bad restore object command\n");
         pm_strcpy(jcr->errmsg, dir->msg);
         Jmsg1(jcr, M_FATAL, 0, _("Bad RestoreObject command: %s\n"), jcr->errmsg);
         goto bail_out;
      }
   }

   unbash_spaces(rop.plugin_name);

   Dmsg7(100, "Recv object: JobId=%u objlen=%d full_len=%d objinx=%d objtype=%d "
         "FI=%d plugin_name=%s\n",
         rop.JobId, rop.object_len, rop.object_full_len,
         rop.object_index, rop.object_type, FileIndex, rop.plugin_name);
   /* Read Object name */
   if (dir->recv() < 0) {
      goto bail_out;
   }
   Dmsg2(100, "Recv Oname object: len=%d Oname=%s\n", dir->msglen, dir->msg);
   rop.object_name = bstrdup(dir->msg);

   /* Read Object */
   if (dir->recv() < 0) {
      goto bail_out;
   }
   /* Transfer object from message buffer, and get new message buffer */
   rop.object = dir->msg;
   dir->msg = get_pool_memory(PM_MESSAGE);

   /* If object is compressed, uncompress it */
   if (rop.object_compression == 1) {   /* zlib level 9 */
      int stat;
      int out_len = rop.object_full_len + 100;
      POOLMEM *obj = get_memory(out_len);
      Dmsg2(100, "Inflating from %d to %d\n", rop.object_len, rop.object_full_len);
      stat = Zinflate(rop.object, rop.object_len, obj, out_len);
      Dmsg1(100, "Zinflate stat=%d\n", stat);
      if (out_len != rop.object_full_len) {
         Jmsg3(jcr, M_ERROR, 0, ("Decompression failed. Len wanted=%d got=%d. Object=%s\n"),
            rop.object_full_len, out_len, rop.object_name);
      }
      free_pool_memory(rop.object);   /* release compressed object */
      rop.object = obj;               /* new uncompressed object */
      rop.object_len = out_len;
   }
   Dmsg2(100, "Recv Object: len=%d Object=%s\n", rop.object_len, rop.object);
   /* we still need to do this to detect a vss restore */
   if (strcmp(rop.object_name, "job_metadata.xml") == 0) {
      Dmsg0(100, "got job metadata\n");
      jcr->got_metadata = true;
   }

   generate_plugin_event(jcr, bEventRestoreObject, (void *)&rop);

   if (rop.object_name) {
      free(rop.object_name);
   }
   if (rop.object) {
      free_pool_memory(rop.object);
   }
   if (rop.plugin_name) {
      free(rop.plugin_name);
   }

   Dmsg1(100, "Send: %s", OKRestoreObject);
   return 1;

bail_out:
   dir->fsend(_("2909 Bad RestoreObject command.\n"));
   return 0;

}


static bool init_fileset(JCR *jcr)
{
   FF_PKT *ff;
   findFILESET *fileset;

   if (!jcr->ff) {
      return false;
   }
   ff = jcr->ff;
   if (ff->fileset) {
      return false;
   }
   fileset = (findFILESET *)malloc(sizeof(findFILESET));
   memset(fileset, 0, sizeof(findFILESET));
   ff->fileset = fileset;
   fileset->state = state_none;
   fileset->include_list.init(1, true);
   fileset->exclude_list.init(1, true);
   return true;
}

static void append_file(JCR *jcr, findINCEXE *incexe,
                        const char *buf, bool is_file)
{
   if (is_file) {
      incexe->name_list.append(new_dlistString(buf));

   } else if (me->plugin_directory) {
      generate_plugin_event(jcr, bEventPluginCommand, (void *)buf);
      incexe->plugin_list.append(new_dlistString(buf));

   } else {
      Jmsg(jcr, M_FATAL, 0,
           _("Plugin Directory not defined. Cannot use plugin: \"%s\"\n"),
           buf);
   }
}

/*
 * Add fname to include/exclude fileset list. First check for
 * | and < and if necessary perform command.
 */
void add_file_to_fileset(JCR *jcr, const char *fname, bool is_file)
{
   findFILESET *fileset = jcr->ff->fileset;
   char *p;
   BPIPE *bpipe;
   POOLMEM *fn;
   FILE *ffd;
   char buf[1000];
   int ch;
   int stat;

   p = (char *)fname;
   ch = (uint8_t)*p;
   switch (ch) {
   case '|':
      p++;                            /* skip over | */
      fn = get_pool_memory(PM_FNAME);
      fn = edit_job_codes(jcr, fn, p, "", job_code_callback_filed);
      bpipe = open_bpipe(fn, 0, "r");
      if (!bpipe) {
         berrno be;
         Jmsg(jcr, M_FATAL, 0, _("Cannot run program: %s. ERR=%s\n"),
            p, be.bstrerror());
         free_pool_memory(fn);
         return;
      }
      free_pool_memory(fn);
      while (fgets(buf, sizeof(buf), bpipe->rfd)) {
         strip_trailing_junk(buf);
         append_file(jcr, fileset->incexe, buf, is_file);
      }
      if ((stat=close_bpipe(bpipe)) != 0) {
         berrno be;
         Jmsg(jcr, M_FATAL, 0, _("Error running program: %s. stat=%d: ERR=%s\n"),
            p, be.code(stat), be.bstrerror(stat));
         return;
      }
      break;
   case '<':
      Dmsg1(100, "Doing < of '%s' include on client.\n", p + 1);
      p++;                      /* skip over < */
      if ((ffd = fopen(p, "rb")) == NULL) {
         berrno be;
         Jmsg(jcr, M_FATAL, 0,
              _("Cannot open FileSet input file: %s. ERR=%s\n"),
            p, be.bstrerror());
         return;
      }
      while (fgets(buf, sizeof(buf), ffd)) {
         strip_trailing_junk(buf);
         append_file(jcr, fileset->incexe, buf, is_file);
      }
      fclose(ffd);
      break;
   default:
      append_file(jcr, fileset->incexe, fname, is_file);
      break;
   }
}

findINCEXE *get_incexe(JCR *jcr)
{
   if (jcr->ff && jcr->ff->fileset) {
      return jcr->ff->fileset->incexe;
   }
   return NULL;
}

void set_incexe(JCR *jcr, findINCEXE *incexe)
{
   findFILESET *fileset = jcr->ff->fileset;
   fileset->incexe = incexe;
}


/**
 * Define a new Exclude block in the FileSet
 */
findINCEXE *new_exclude(JCR *jcr)
{
   findFILESET *fileset = jcr->ff->fileset;

   /* New exclude */
   fileset->incexe = (findINCEXE *)malloc(sizeof(findINCEXE));
   memset(fileset->incexe, 0, sizeof(findINCEXE));
   fileset->incexe->opts_list.init(1, true);
   fileset->incexe->name_list.init();
   fileset->incexe->plugin_list.init();
   fileset->exclude_list.append(fileset->incexe);
   return fileset->incexe;
}

/**
 * Define a new Include block in the FileSet
 */
findINCEXE *new_include(JCR *jcr)
{
   findFILESET *fileset = jcr->ff->fileset;

   /* New include */
   fileset->incexe = (findINCEXE *)malloc(sizeof(findINCEXE));
   memset(fileset->incexe, 0, sizeof(findINCEXE));
   fileset->incexe->opts_list.init(1, true);
   fileset->incexe->name_list.init(); /* for dlist;  was 1,true for alist */
   fileset->incexe->plugin_list.init();
   fileset->include_list.append(fileset->incexe);
   return fileset->incexe;
}

/**
 * Define a new preInclude block in the FileSet
 *   That is the include is prepended to the other
 *   Includes.  This is used for plugin exclusions.
 */
findINCEXE *new_preinclude(JCR *jcr)
{
   findFILESET *fileset = jcr->ff->fileset;

   /* New pre-include */
   fileset->incexe = (findINCEXE *)malloc(sizeof(findINCEXE));
   memset(fileset->incexe, 0, sizeof(findINCEXE));
   fileset->incexe->opts_list.init(1, true);
   fileset->incexe->name_list.init(); /* for dlist;  was 1,true for alist */
   fileset->incexe->plugin_list.init();
   fileset->include_list.prepend(fileset->incexe);
   return fileset->incexe;
}

static findFOPTS *start_options(FF_PKT *ff)
{
   int state = ff->fileset->state;
   findINCEXE *incexe = ff->fileset->incexe;

   if (state != state_options) {
      ff->fileset->state = state_options;
      findFOPTS *fo = (findFOPTS *)malloc(sizeof(findFOPTS));
      memset(fo, 0, sizeof(findFOPTS));
      fo->regex.init(1, true);
      fo->regexdir.init(1, true);
      fo->regexfile.init(1, true);
      fo->wild.init(1, true);
      fo->wilddir.init(1, true);
      fo->wildfile.init(1, true);
      fo->wildbase.init(1, true);
      fo->base.init(1, true);
      fo->fstype.init(1, true);
      fo->drivetype.init(1, true);
      incexe->current_opts = fo;
      incexe->opts_list.append(fo);
   }
   return incexe->current_opts;
}

/*
 * Used by plugins to define a new options block
 */
void new_options(JCR *jcr, findINCEXE *incexe)
{
   if (!incexe) {
      incexe = jcr->ff->fileset->incexe;
   }
   findFOPTS *fo = (findFOPTS *)malloc(sizeof(findFOPTS));
   memset(fo, 0, sizeof(findFOPTS));
   fo->regex.init(1, true);
   fo->regexdir.init(1, true);
   fo->regexfile.init(1, true);
   fo->wild.init(1, true);
   fo->wilddir.init(1, true);
   fo->wildfile.init(1, true);
   fo->wildbase.init(1, true);
   fo->base.init(1, true);
   fo->fstype.init(1, true);
   fo->drivetype.init(1, true);
   incexe->current_opts = fo;
   incexe->opts_list.prepend(fo);
   jcr->ff->fileset->state = state_options;
}

/**
 * Add a regex to the current fileset
 */
int add_regex_to_fileset(JCR *jcr, const char *item, int type)
{
   findFOPTS *current_opts = start_options(jcr->ff);
   regex_t *preg;
   int rc;
   char prbuf[500];

   preg = (regex_t *)malloc(sizeof(regex_t));
   if (current_opts->flags & FO_IGNORECASE) {
      rc = regcomp(preg, item, REG_EXTENDED|REG_ICASE);
   } else {
      rc = regcomp(preg, item, REG_EXTENDED);
   }
   if (rc != 0) {
      regerror(rc, preg, prbuf, sizeof(prbuf));
      regfree(preg);
      free(preg);
      Jmsg(jcr, M_FATAL, 0, _("REGEX %s compile error. ERR=%s\n"), item, prbuf);
      return state_error;
   }
   if (type == ' ') {
      current_opts->regex.append(preg);
   } else if (type == 'D') {
      current_opts->regexdir.append(preg);
   } else if (type == 'F') {
      current_opts->regexfile.append(preg);
   } else {
      return state_error;
   }
   return state_options;
}

/**
 * Add a wild card to the current fileset
 */
int add_wild_to_fileset(JCR *jcr, const char *item, int type)
{
   findFOPTS *current_opts = start_options(jcr->ff);

   if (type == ' ') {
      current_opts->wild.append(bstrdup(item));
   } else if (type == 'D') {
      current_opts->wilddir.append(bstrdup(item));
   } else if (type == 'F') {
      current_opts->wildfile.append(bstrdup(item));
   } else if (type == 'B') {
      current_opts->wildbase.append(bstrdup(item));
   } else {
      return state_error;
   }
   return state_options;
}


/**
 * Add options to the current fileset
 */
int add_options_to_fileset(JCR *jcr, const char *item)
{
   findFOPTS *current_opts = start_options(jcr->ff);

   set_options(current_opts, item);
   return state_options;
}

static void add_fileset(JCR *jcr, const char *item)
{
   FF_PKT *ff = jcr->ff;
   findFILESET *fileset = ff->fileset;
   int state = fileset->state;
   findFOPTS *current_opts;

   /* Get code, optional subcode, and position item past the dividing space */
   Dmsg1(100, "%s\n", item);
   int code = item[0];
   if (code != '\0') {
      ++item;
   }
   int subcode = ' ';               /* A space is always a valid subcode */
   if (item[0] != '\0' && item[0] != ' ') {
      subcode = item[0];
      ++item;
   }
   if (*item == ' ') {
      ++item;
   }

   /* Skip all lines we receive after an error */
   if (state == state_error) {
      Dmsg0(100, "State=error return\n");
      return;
   }

   /**
    * The switch tests the code for validity.
    * The subcode is always good if it is a space, otherwise we must confirm.
    * We set state to state_error first assuming the subcode is invalid,
    * requiring state to be set in cases below that handle subcodes.
    */
   if (subcode != ' ') {
      state = state_error;
      Dmsg0(100, "Set state=error or double code.\n");
   }
   switch (code) {
   case 'I':
      (void)new_include(jcr);
      break;
   case 'E':
      (void)new_exclude(jcr);
      break;
   case 'N':                             /* null */
      state = state_none;
      break;
   case 'F':                             /* file = */
      /* File item to include or exclude list */
      state = state_include;
      add_file_to_fileset(jcr, item, true);
      break;
   case 'P':                              /* plugin */
      /* Plugin item to include list */
      state = state_include;
      add_file_to_fileset(jcr, item, false);
      break;
   case 'R':                              /* regex */
      state = add_regex_to_fileset(jcr, item, subcode);
      break;
   case 'B':
      current_opts = start_options(ff);
      current_opts->base.append(bstrdup(item));
      state = state_options;
      break;
   case 'X':                             /* Filetype or Drive type */
      current_opts = start_options(ff);
      state = state_options;
      if (subcode == ' ') {
         current_opts->fstype.append(bstrdup(item));
      } else if (subcode == 'D') {
         current_opts->drivetype.append(bstrdup(item));
      } else {
         state = state_error;
      }
      break;
   case 'W':                               /* wild cards */
      state = add_wild_to_fileset(jcr, item, subcode);
      break;
   case 'O':                                /* Options */
      state = add_options_to_fileset(jcr, item);
      break;
   case 'Z':                                /* ignore dir */
      state = state_include;
      fileset->incexe->ignoredir = bstrdup(item);
      break;
   case 'D':
      current_opts = start_options(ff);
//    current_opts->reader = bstrdup(item); /* deprecated */
      state = state_options;
      break;
   case 'T':
      current_opts = start_options(ff);
//    current_opts->writer = bstrdup(item); /* deprecated */
      state = state_options;
      break;
   case 'G':                    /* Plugin command for this Option block */
      current_opts = start_options(ff);
      current_opts->plugin = bstrdup(item);
      state = state_options;
      break;
   default:
      Jmsg(jcr, M_FATAL, 0, _("Invalid FileSet command: %s\n"), item);
      state = state_error;
      break;
   }
   ff->fileset->state = state;
}

static bool term_fileset(JCR *jcr)
{
   FF_PKT *ff = jcr->ff;

#ifdef xxx_DEBUG_CODE
   findFILESET *fileset = ff->fileset;
   int i, j, k;

   for (i=0; i<fileset->include_list.size(); i++) {
      findINCEXE *incexe = (findINCEXE *)fileset->include_list.get(i);
      Dmsg0(400, "I\n");
      for (j=0; j<incexe->opts_list.size(); j++) {
         findFOPTS *fo = (findFOPTS *)incexe->opts_list.get(j);
         for (k=0; k<fo->regex.size(); k++) {
            Dmsg1(400, "R %s\n", (char *)fo->regex.get(k));
         }
         for (k=0; k<fo->regexdir.size(); k++) {
            Dmsg1(400, "RD %s\n", (char *)fo->regexdir.get(k));
         }
         for (k=0; k<fo->regexfile.size(); k++) {
            Dmsg1(400, "RF %s\n", (char *)fo->regexfile.get(k));
         }
         for (k=0; k<fo->wild.size(); k++) {
            Dmsg1(400, "W %s\n", (char *)fo->wild.get(k));
         }
         for (k=0; k<fo->wilddir.size(); k++) {
            Dmsg1(400, "WD %s\n", (char *)fo->wilddir.get(k));
         }
         for (k=0; k<fo->wildfile.size(); k++) {
            Dmsg1(400, "WF %s\n", (char *)fo->wildfile.get(k));
         }
         for (k=0; k<fo->wildbase.size(); k++) {
            Dmsg1(400, "WB %s\n", (char *)fo->wildbase.get(k));
         }
         for (k=0; k<fo->base.size(); k++) {
            Dmsg1(400, "B %s\n", (char *)fo->base.get(k));
         }
         for (k=0; k<fo->fstype.size(); k++) {
            Dmsg1(400, "X %s\n", (char *)fo->fstype.get(k));
         }
         for (k=0; k<fo->drivetype.size(); k++) {
            Dmsg1(400, "XD %s\n", (char *)fo->drivetype.get(k));
         }
      }
      if (incexe->ignoredir) {
         Dmsg1(400, "Z %s\n", incexe->ignoredir);
      }
      dlistString *node;
      foreach_dlist(node, &incexe->name_list) {
         Dmsg1(400, "F %s\n", node->c_str());
      }
      foreach_dlist(node, &incexe->plugin_list) {
         Dmsg1(400, "P %s\n", node->c_str());
      }
   }
   for (i=0; i<fileset->exclude_list.size(); i++) {
      findINCEXE *incexe = (findINCEXE *)fileset->exclude_list.get(i);
      Dmsg0(400, "E\n");
      for (j=0; j<incexe->opts_list.size(); j++) {
         findFOPTS *fo = (findFOPTS *)incexe->opts_list.get(j);
         for (k=0; k<fo->regex.size(); k++) {
            Dmsg1(400, "R %s\n", (char *)fo->regex.get(k));
         }
         for (k=0; k<fo->regexdir.size(); k++) {
            Dmsg1(400, "RD %s\n", (char *)fo->regexdir.get(k));
         }
         for (k=0; k<fo->regexfile.size(); k++) {
            Dmsg1(400, "RF %s\n", (char *)fo->regexfile.get(k));
         }
         for (k=0; k<fo->wild.size(); k++) {
            Dmsg1(400, "W %s\n", (char *)fo->wild.get(k));
         }
         for (k=0; k<fo->wilddir.size(); k++) {
            Dmsg1(400, "WD %s\n", (char *)fo->wilddir.get(k));
         }
         for (k=0; k<fo->wildfile.size(); k++) {
            Dmsg1(400, "WF %s\n", (char *)fo->wildfile.get(k));
         }
         for (k=0; k<fo->wildbase.size(); k++) {
            Dmsg1(400, "WB %s\n", (char *)fo->wildbase.get(k));
         }
         for (k=0; k<fo->base.size(); k++) {
            Dmsg1(400, "B %s\n", (char *)fo->base.get(k));
         }
         for (k=0; k<fo->fstype.size(); k++) {
            Dmsg1(400, "X %s\n", (char *)fo->fstype.get(k));
         }
         for (k=0; k<fo->drivetype.size(); k++) {
            Dmsg1(400, "XD %s\n", (char *)fo->drivetype.get(k));
         }
      }
      dlistString *node;
      foreach_dlist(node, &incexe->name_list) {
         Dmsg1(400, "F %s\n", node->c_str());
      }
      foreach_dlist(node, &incexe->plugin_list) {
         Dmsg1(400, "P %s\n", node->c_str());
      }
   }
#endif
   return ff->fileset->state != state_error;
}


/**
 * As an optimization, we should do this during
 *  "compile" time in filed/job.c, and keep only a bit mask
 *  and the Verify options.
 */
static int set_options(findFOPTS *fo, const char *opts)
{
   int j;
   const char *p;
   char strip[100];

   for (p=opts; *p; p++) {
      switch (*p) {
      case 'a':                 /* alway replace */
      case '0':                 /* no option */
         break;
      case 'e':
         fo->flags |= FO_EXCLUDE;
         break;
      case 'f':
         fo->flags |= FO_MULTIFS;
         break;
      case 'h':                 /* no recursion */
         fo->flags |= FO_NO_RECURSION;
         break;
      case 'H':                 /* no hard link handling */
         fo->flags |= FO_NO_HARDLINK;
         break;
      case 'i':
         fo->flags |= FO_IGNORECASE;
         break;
      case 'M':                 /* MD5 */
         fo->flags |= FO_MD5;
         break;
      case 'n':
         fo->flags |= FO_NOREPLACE;
         break;
      case 'p':                 /* use portable data format */
         fo->flags |= FO_PORTABLE;
         break;
      case 'R':                 /* Resource forks and Finder Info */
         fo->flags |= FO_HFSPLUS;
         break;
      case 'r':                 /* read fifo */
         fo->flags |= FO_READFIFO;
         break;
      case 'S':
         switch(*(p + 1)) {
         case '1':
            fo->flags |= FO_SHA1;
            p++;
            break;
#ifdef HAVE_SHA2
         case '2':
            fo->flags |= FO_SHA256;
            p++;
            break;
         case '3':
            fo->flags |= FO_SHA512;
            p++;
            break;
#endif
         default:
            /*
             * If 2 or 3 is seen here, SHA2 is not configured, so
             *  eat the option, and drop back to SHA-1.
             */
            if (p[1] == '2' || p[1] == '3') {
               p++;
            }
            fo->flags |= FO_SHA1;
            break;
         }
         break;
      case 's':
         fo->flags |= FO_SPARSE;
         break;
      case 'm':
         fo->flags |= FO_MTIMEONLY;
         break;
      case 'k':
         fo->flags |= FO_KEEPATIME;
         break;
      case 'A':
         fo->flags |= FO_ACL;
         break;
      case 'V':                  /* verify options */
         /* Copy Verify Options */
         for (j=0; *p && *p != ':'; p++) {
            fo->VerifyOpts[j] = *p;
            if (j < (int)sizeof(fo->VerifyOpts) - 1) {
               j++;
            }
         }
         fo->VerifyOpts[j] = 0;
         break;
      case 'C':                  /* accurate options */
         /* Copy Accurate Options */
         for (j=0; *p && *p != ':'; p++) {
            fo->AccurateOpts[j] = *p;
            if (j < (int)sizeof(fo->AccurateOpts) - 1) {
               j++;
            }
         }
         fo->AccurateOpts[j] = 0;
         break;
      case 'J':                  /* Basejob options */
         /* Copy BaseJob Options */
         for (j=0; *p && *p != ':'; p++) {
            fo->BaseJobOpts[j] = *p;
            if (j < (int)sizeof(fo->BaseJobOpts) - 1) {
               j++;
            }
         }
         fo->BaseJobOpts[j] = 0;
         break;
      case 'P':                  /* strip path */
         /* Get integer */
         p++;                    /* skip P */
         for (j=0; *p && *p != ':'; p++) {
            strip[j] = *p;
            if (j < (int)sizeof(strip) - 1) {
               j++;
            }
         }
         strip[j] = 0;
         fo->strip_path = atoi(strip);
         fo->flags |= FO_STRIPPATH;
         Dmsg2(100, "strip=%s strip_path=%d\n", strip, fo->strip_path);
         break;
      case 'w':
         fo->flags |= FO_IF_NEWER;
         break;
      case 'W':
         fo->flags |= FO_ENHANCEDWILD;
         break;
      case 'Z':                 /* compression */
         p++;                   /* skip Z */
         if (*p >= '0' && *p <= '9') {
            fo->flags |= FO_COMPRESS;
            fo->Compress_algo = COMPRESS_GZIP;
            fo->Compress_level = *p - '0';
         }
         else if (*p == 'o') {
            fo->flags |= FO_COMPRESS;
            fo->Compress_algo = COMPRESS_LZO1X;
            fo->Compress_level = 1; /* not used with LZO */
         }
         break;
      case 'K':
         fo->flags |= FO_NOATIME;
         break;
      case 'c':
         fo->flags |= FO_CHKCHANGES;
         break;
      case 'N':
         fo->flags |= FO_HONOR_NODUMP;
         break;
      case 'X':
         fo->flags |= FO_XATTR;
         break;
      default:
         Jmsg1(NULL, M_ERROR, 0, _("Unknown include/exclude option: %c\n"), *p);
         break;
      }
   }
   return state_options;
}


/**
 * Director is passing his Fileset
 */
static int fileset_cmd(JCR *jcr)
{
   POOL_MEM buf(PM_MESSAGE);
   BSOCK *dir = jcr->dir_bsock;
   int rtnstat;
   int vss = 0;

   sscanf(dir->msg, "fileset vss=%d", &vss);
   enable_vss = vss;

   if (!init_fileset(jcr)) {
      return 0;
   }
   while (dir->recv() >= 0) {
      strip_trailing_junk(dir->msg);
      Dmsg1(500, "Fileset: %s\n", dir->msg);
      pm_strcpy(buf, dir->msg);
      add_fileset(jcr, buf.c_str());
   }
   if (!term_fileset(jcr)) {
      return 0;
   }
   rtnstat = dir->fsend(OKinc);
   generate_plugin_event(jcr, bEventEndFileSet);
   return rtnstat;
}



/**
 * Get backup level from Director
 *
 * Note: there are odd things such as accurate_differential,
 *  and accurate_incremental that are passed in level, thus
 *  the calls to strstr() below.
 *
 */
static int level_cmd(JCR *jcr)
{
   BSOCK *dir = jcr->dir_bsock;
   POOLMEM *level, *buf = NULL;
   int mtime_only;

   level = get_memory(dir->msglen+1);
   Dmsg1(10, "level_cmd: %s", dir->msg);

   /* keep compatibility with older directors */
   if (strstr(dir->msg, "accurate")) {
      jcr->accurate = true;
   }
   if (strstr(dir->msg, "rerunning")) {
      jcr->rerunning = true;
   }
   if (sscanf(dir->msg, "level = %s ", level) != 1) {
      goto bail_out;
   }
   /* Base backup requested? */
   if (strcasecmp(level, "base") == 0) {
      jcr->setJobLevel(L_BASE);
   /* Full backup requested? */
   } else if (strcasecmp(level, "full") == 0) {
      jcr->setJobLevel(L_FULL);
   } else if (strstr(level, "differential")) {
      jcr->setJobLevel(L_DIFFERENTIAL);
      free_memory(level);
      return 1;
   } else if (strstr(level, "incremental")) {
      jcr->setJobLevel(L_INCREMENTAL);
      free_memory(level);
      return 1;
   /*
    * We get his UTC since time, then sync the clocks and correct it
    *   to agree with our clock.
    */
   } else if (strcasecmp(level, "since_utime") == 0) {
      buf = get_memory(dir->msglen+1);
      utime_t since_time, adj;
      btime_t his_time, bt_start, rt=0, bt_adj=0;
      if (jcr->getJobLevel() == L_NONE) {
         jcr->setJobLevel(L_SINCE);     /* if no other job level set, do it now */
      }
      if (sscanf(dir->msg, "level = since_utime %s mtime_only=%d prev_job=%127s",
                 buf, &mtime_only, jcr->PrevJob) != 3) {
         if (sscanf(dir->msg, "level = since_utime %s mtime_only=%d",
                    buf, &mtime_only) != 2) {
            goto bail_out;
         }
      }
      since_time = str_to_uint64(buf);  /* this is the since time */
      Dmsg2(100, "since_time=%lld prev_job=%s\n", since_time, jcr->PrevJob);
      char ed1[50], ed2[50];
      /*
       * Sync clocks by polling him for the time. We take
       *   10 samples of his time throwing out the first two.
       */
      for (int i=0; i<10; i++) {
         bt_start = get_current_btime();
         dir->signal(BNET_BTIME);     /* poll for time */
         if (dir->recv() <= 0) {      /* get response */
            goto bail_out;
         }
         if (sscanf(dir->msg, "btime %s", buf) != 1) {
            goto bail_out;
         }
         if (i < 2) {                 /* toss first two results */
            continue;
         }
         his_time = str_to_uint64(buf);
         rt = get_current_btime() - bt_start; /* compute round trip time */
         Dmsg2(100, "Dirtime=%s FDtime=%s\n", edit_uint64(his_time, ed1),
               edit_uint64(bt_start, ed2));
         bt_adj +=  bt_start - his_time - rt/2;
         Dmsg2(100, "rt=%s adj=%s\n", edit_uint64(rt, ed1), edit_uint64(bt_adj, ed2));
      }

      bt_adj = bt_adj / 8;            /* compute average time */
      Dmsg2(100, "rt=%s adj=%s\n", edit_uint64(rt, ed1), edit_uint64(bt_adj, ed2));
      adj = btime_to_utime(bt_adj);
      since_time += adj;              /* adjust for clock difference */
      /* Don't notify if time within 3 seconds */
      if (adj > 3 || adj < -3) {
         int type;
         if (adj > 600 || adj < -600) {
            type = M_WARNING;
         } else {
            type = M_INFO;
         }
         Jmsg(jcr, type, 0, _("DIR and FD clocks differ by %lld seconds, FD automatically compensating.\n"), adj);
      }
      dir->signal(BNET_EOD);

      Dmsg2(100, "adj=%lld since_time=%lld\n", adj, since_time);
      jcr->incremental = 1;           /* set incremental or decremental backup */
      jcr->mtime = since_time;        /* set since time */
      generate_plugin_event(jcr, bEventSince, (void *)(time_t)jcr->mtime);
   } else {
      Jmsg1(jcr, M_FATAL, 0, _("Unknown backup level: %s\n"), level);
      free_memory(level);
      return 0;
   }
   free_memory(level);
   if (buf) {
      free_memory(buf);
   }
   generate_plugin_event(jcr, bEventLevel, (void*)(intptr_t)jcr->getJobLevel());
   return dir->fsend(OKlevel);

bail_out:
   pm_strcpy(jcr->errmsg, dir->msg);
   Jmsg1(jcr, M_FATAL, 0, _("Bad level command: %s\n"), jcr->errmsg);
   free_memory(level);
   if (buf) {
      free_memory(buf);
   }
   return 0;
}

/**
 * Get session parameters from Director -- this is for a Restore command
 *   This is deprecated. It is now passed via the bsr.
 */
static int session_cmd(JCR *jcr)
{
   BSOCK *dir = jcr->dir_bsock;

   Dmsg1(100, "SessionCmd: %s", dir->msg);
   if (sscanf(dir->msg, sessioncmd, jcr->VolumeName,
              &jcr->VolSessionId, &jcr->VolSessionTime,
              &jcr->StartFile, &jcr->EndFile,
              &jcr->StartBlock, &jcr->EndBlock) != 7) {
      pm_strcpy(jcr->errmsg, dir->msg);
      Jmsg(jcr, M_FATAL, 0, _("Bad session command: %s"), jcr->errmsg);
      return 0;
   }

   return dir->fsend(OKsession);
}

static void set_storage_auth_key(JCR *jcr, char *key)
{
   /* if no key don't update anything */
   if (!*key) {
      return;
   }

   /**
    * We can be contacting multiple storage daemons.
    * So, make sure that any old jcr->store_bsock is cleaned up.
    */
   if (jcr->store_bsock) {
      jcr->store_bsock->destroy();
      jcr->store_bsock = NULL;
   }

   /**
    * We can be contacting multiple storage daemons.
    *   So, make sure that any old jcr->sd_auth_key is cleaned up.
    */
   if (jcr->sd_auth_key) {
      /*
       * If we already have a Authorization key, director can do multi
       * storage restore
       */
      Dmsg0(5, "set multi_restore=true\n");
      jcr->multi_restore = true;
      bfree(jcr->sd_auth_key);
   }

   jcr->sd_auth_key = bstrdup(key);
   Dmsg1(5, "set sd auth key %s\n", jcr->sd_auth_key);
}

/**
 * Get address of storage daemon from Director
 *
 */
static int storage_cmd(JCR *jcr)
{
   int stored_port = 0;            /* storage daemon port */
   int enable_ssl;                 /* enable ssl to sd */
   POOL_MEM sd_auth_key(PM_MESSAGE);
   BSOCK *dir = jcr->dir_bsock;
   BSOCK *sd;

   Dmsg1(100, "StorageCmd: %s", dir->msg);
   sd_auth_key.check_size(dir->msglen);
   if (sscanf(dir->msg, storaddr, &jcr->stored_addr, &stored_port,
              &enable_ssl, sd_auth_key.c_str()) == 4) {
      Dmsg1(100, "Set auth key %s\n", sd_auth_key.c_str());
      set_storage_auth_key(jcr, sd_auth_key.c_str());
  } else if (sscanf(dir->msg, storaddr_v1, &jcr->stored_addr,
                 &stored_port, &enable_ssl) != 3) {
      pm_strcpy(jcr->errmsg, dir->msg);
      Jmsg(jcr, M_FATAL, 0, _("Bad storage command: %s"), jcr->errmsg);
      Pmsg1(010, "Bad storage command: %s", jcr->errmsg);
      goto bail_out;
   }


   /* TODO: see if we put limit on restore and backup... */
   if (!jcr->max_bandwidth) {
      if (jcr->director->max_bandwidth_per_job) {
         jcr->max_bandwidth = jcr->director->max_bandwidth_per_job;

      } else if (me->max_bandwidth_per_job) {
         jcr->max_bandwidth = me->max_bandwidth_per_job;
      }
   }

   if (stored_port != 0) {
      jcr->sd_calls_client = false;   /* We are doing the connecting */
      Dmsg3(110, "Connect to storage: %s:%d ssl=%d\n", jcr->stored_addr, stored_port,
            enable_ssl);
      sd = new_bsock();
      /* Open command communications with Storage daemon */
      /* Try to connect for 1 hour at 10 second intervals */
      sd->set_source_address(me->FDsrc_addr);
      if (!sd->connect(jcr, 10, (int)me->SDConnectTimeout, me->heartbeat_interval,
                _("Storage daemon"), jcr->stored_addr, NULL, stored_port, 1)) {
         /* destroy() OK because sd is local */
         sd->destroy();
         Jmsg2(jcr, M_FATAL, 0, _("Failed to connect to Storage daemon: %s:%d\n"),
             jcr->stored_addr, stored_port);
         Dmsg2(100, "Failed to connect to Storage daemon: %s:%d\n",
             jcr->stored_addr, stored_port);
         goto bail_out;
      }

      Dmsg0(110, "Connection OK to SD.\n");
      jcr->store_bsock = sd;
   } else {                      /* The storage daemon called us */
      struct timeval tv;
      struct timezone tz;
      struct timespec timeout;
      int errstat;

      jcr->sd_calls_client = true;
      /*
       * Wait for the Storage daemon to contact us to start the Job,
       *  when he does, we will be released, unless the 30 minutes
       *  expires.
       */
      gettimeofday(&tv, &tz);
      timeout.tv_nsec = tv.tv_usec * 1000;
      timeout.tv_sec = tv.tv_sec + 30 * 60;  /* wait 30 minutes */
      P(mutex);
      while (jcr->store_bsock == NULL && !jcr->is_job_canceled()) {
         errstat = pthread_cond_timedwait(&jcr->job_start_wait, &mutex, &timeout);
         if (errstat == ETIMEDOUT || errstat == EINVAL || errstat == EPERM) {
            break;
         }
         Dmsg1(800, "=== Auth cond errstat=%d\n", errstat);
      }
      V(mutex);
      Dmsg2(800, "Auth fail or cancel for jid=%d %p\n", jcr->JobId, jcr);

      /* We should already have a storage connection! */
      if (jcr->store_bsock == NULL) {
         Pmsg0(000, "Failed connect from Storage daemon. SD bsock=NULL.\n");
         Pmsg1(000, "Storagecmd: %s", dir->msg);
         Jmsg0(jcr, M_FATAL, 0, _("Failed connect from Storage daemon. SD bsock=NULL.\n"));
         goto bail_out;
      }
      if (jcr->is_job_canceled()) {
         goto bail_out;
      }
   }
   jcr->store_bsock->set_bwlimit(jcr->max_bandwidth);

   if (!authenticate_storagedaemon(jcr)) {
      goto bail_out;
   }
   memset(jcr->sd_auth_key, 0, strlen(jcr->sd_auth_key));
   Dmsg0(110, "Authenticated with SD.\n");

   /* Send OK to Director */
   return dir->fsend(OKstore);

bail_out:
   dir->fsend(BADcmd, "storage");
   return 0;
}


/**
 * Do a backup.
 */
static int backup_cmd(JCR *jcr)
{
   BSOCK *dir = jcr->dir_bsock;
   BSOCK *sd = jcr->store_bsock;
   int ok = 0;
   int SDJobStatus;
   int32_t FileIndex;

   if (sscanf(dir->msg, "backup FileIndex=%ld\n", &FileIndex) == 1) {
      jcr->JobFiles = FileIndex;
      Dmsg1(100, "JobFiles=%ld\n", jcr->JobFiles);
   }

   /*
    * Validate some options given to the backup make sense for the compiled in
    * options of this filed.
    */
   if (jcr->ff->flags & FO_ACL && !have_acl) {
      Jmsg(jcr, M_FATAL, 0, _("ACL support not configured for your machine.\n"));
      goto cleanup;
   }
   if (jcr->ff->flags & FO_XATTR && !have_xattr) {
      Jmsg(jcr, M_FATAL, 0, _("XATTR support not configured for your machine.\n"));
      goto cleanup;
   }

   jcr->setJobStatus(JS_Blocked);
   jcr->setJobType(JT_BACKUP);
   Dmsg1(100, "begin backup ff=%p\n", jcr->ff);

   if (sd == NULL) {
      Jmsg(jcr, M_FATAL, 0, _("Cannot contact Storage daemon\n"));
      dir->fsend(BADcmd, "backup");
      goto cleanup;
   }

   dir->fsend(OKbackup);
   Dmsg1(110, "filed>dird: %s", dir->msg);

   /**
    * Send Append Open Session to Storage daemon
    */
   sd->fsend(append_open);
   Dmsg1(110, ">stored: %s", sd->msg);
   /**
    * Expect to receive back the Ticket number
    */
   if (bget_msg(sd) >= 0) {
      Dmsg1(110, "<stored: %s", sd->msg);
      if (sscanf(sd->msg, OK_open, &jcr->Ticket) != 1) {
         Jmsg(jcr, M_FATAL, 0, _("Bad response to append open: %s\n"), sd->msg);
         goto cleanup;
      }
      Dmsg1(110, "Got Ticket=%d\n", jcr->Ticket);
   } else {
      Jmsg(jcr, M_FATAL, 0, _("Bad response from stored to open command\n"));
      goto cleanup;
   }

   /**
    * Send Append data command to Storage daemon
    */
   sd->fsend(append_data, jcr->Ticket);
   Dmsg1(110, ">stored: %s", sd->msg);

   /**
    * Expect to get OK data
    */
   Dmsg1(110, "<stored: %s", sd->msg);
   if (!response(jcr, sd, OK_data, "Append Data")) {
      goto cleanup;
   }

   generate_daemon_event(jcr, "JobStart");
   generate_plugin_event(jcr, bEventStartBackupJob);

   /*
    * Send Files to Storage daemon
    */
   Dmsg1(110, "begin blast ff=%p\n", (FF_PKT *)jcr->ff);
   if (!blast_data_to_storage_daemon(jcr, NULL)) {
      jcr->setJobStatus(JS_ErrorTerminated);
      sd->suppress_error_messages(true);
      Dmsg0(110, "Error in blast_data.\n");
   } else {
      jcr->setJobStatus(JS_Terminated);
      /* Note, the above set status will not override an error */
      if (!(jcr->JobStatus == JS_Terminated || jcr->JobStatus == JS_Warnings)) {
         sd->suppress_error_messages(true);
         goto cleanup;                /* bail out now */
      }
      /**
       * Expect to get response to append_data from Storage daemon
       */
      if (!response(jcr, sd, OK_append, "Append Data")) {
         jcr->setJobStatus(JS_ErrorTerminated);
         goto cleanup;
      }

      /**
       * Send Append End Data to Storage daemon
       */
      sd->fsend(append_end, jcr->Ticket);
      /* Get end OK */
      if (!response(jcr, sd, OK_end, "Append End")) {
         jcr->setJobStatus(JS_ErrorTerminated);
         goto cleanup;
      }

      /**
       * Send Append Close to Storage daemon
       */
      sd->fsend(append_close, jcr->Ticket);
      while (bget_msg(sd) >= 0) {    /* stop on signal or error */
         if (sscanf(sd->msg, OK_close, &SDJobStatus) == 1) {
            ok = 1;
            Dmsg2(200, "SDJobStatus = %d %c\n", SDJobStatus, (char)SDJobStatus);
         }
      }
      if (!ok) {
         Jmsg(jcr, M_FATAL, 0, _("Append Close with SD failed.\n"));
         goto cleanup;
      }
      if (!(SDJobStatus == JS_Terminated || SDJobStatus == JS_Warnings)) {
         Jmsg(jcr, M_FATAL, 0, _("Bad status %d %c returned from Storage Daemon.\n"),
            SDJobStatus, (char)SDJobStatus);
      }
   }

cleanup:
   generate_plugin_event(jcr, bEventEndBackupJob);
   return 0;                          /* return and stop command loop */
}

/**
 * Do a Verify for Director
 *
 */
static int verify_cmd(JCR *jcr)
{
   BSOCK *dir = jcr->dir_bsock;
   BSOCK *sd  = jcr->store_bsock;
   char level[100];

   jcr->setJobType(JT_VERIFY);
   if (sscanf(dir->msg, verifycmd, level) != 1) {
      dir->fsend(_("2994 Bad verify command: %s\n"), dir->msg);
      return 0;
   }

   if (strcasecmp(level, "init") == 0) {
      jcr->setJobLevel(L_VERIFY_INIT);
   } else if (strcasecmp(level, "catalog") == 0){
      jcr->setJobLevel(L_VERIFY_CATALOG);
   } else if (strcasecmp(level, "volume") == 0){
      jcr->setJobLevel(L_VERIFY_VOLUME_TO_CATALOG);
   } else if (strcasecmp(level, "data") == 0){
      jcr->setJobLevel(L_VERIFY_DATA);
   } else if (strcasecmp(level, "disk_to_catalog") == 0) {
      jcr->setJobLevel(L_VERIFY_DISK_TO_CATALOG);
   } else {
      dir->fsend(_("2994 Bad verify level: %s\n"), dir->msg);
      return 0;
   }

   dir->fsend(OKverify);

   generate_daemon_event(jcr, "JobStart");
   generate_plugin_event(jcr, bEventLevel,(void *)(intptr_t)jcr->getJobLevel());
   generate_plugin_event(jcr, bEventStartVerifyJob);

   Dmsg1(110, "filed>dird: %s", dir->msg);

   switch (jcr->getJobLevel()) {
   case L_VERIFY_INIT:
   case L_VERIFY_CATALOG:
      do_verify(jcr);
      break;
   case L_VERIFY_VOLUME_TO_CATALOG:
      if (!open_sd_read_session(jcr)) {
         return 0;
      }
      start_dir_heartbeat(jcr);
      do_verify_volume(jcr);
      stop_dir_heartbeat(jcr);
      /*
       * Send Close session command to Storage daemon
       */
      sd->fsend(read_close, jcr->Ticket);
      Dmsg1(130, "filed>stored: %s", sd->msg);

      /* ****FIXME**** check response */
      bget_msg(sd);                      /* get OK */

      /* Inform Storage daemon that we are done */
      sd->signal(BNET_TERMINATE);

      break;
   case L_VERIFY_DISK_TO_CATALOG:
      do_verify(jcr);
      break;
   default:
      dir->fsend(_("2994 Bad verify level: %s\n"), dir->msg);
      return 0;
   }

   dir->signal(BNET_EOD);
   generate_plugin_event(jcr, bEventEndVerifyJob);
   return 0;                          /* return and terminate command loop */
}

/*
 * Do a Restore for Director
 *
 */
static int restore_cmd(JCR *jcr)
{
   BSOCK *dir = jcr->dir_bsock;
   BSOCK *sd = jcr->store_bsock;
   POOLMEM *args=NULL, *restore_where=NULL, *restore_rwhere=NULL;
   bool use_regexwhere=false;
   int prefix_links;
   char replace;
   bool scan_ok = true;
   int files;
   int ret = 0;

   /**
    * Scan WHERE (base directory for restore) from command
    */
   Dmsg0(100, "restore command\n");

   /* Pickup where string */
   args = get_memory(dir->msglen+1);
   *args = 0;

   restore_where = get_pool_memory(PM_FNAME);
   restore_rwhere = get_pool_memory(PM_FNAME);

   /* We don't know the size of where/rwhere in advance,
    * where= -> where=%202s\n
    */
   Mmsg(restore_where, "%s%%%ds\n", restorefcmd, dir->msglen);
   Mmsg(restore_rwhere, "%s%%%ds\n", restorefcmdR, dir->msglen);

   Dmsg2(200, "where=%srwhere=%s", restore_where, restore_rwhere);

   /* Scan for new form with number of files to restore */
   if (sscanf(dir->msg, restore_where, &files, &replace, &prefix_links, args) != 4) {
      if (sscanf(dir->msg, restore_rwhere, &files, &replace, &prefix_links, args) != 4) {
         if (sscanf(dir->msg, restorefcmd1, &files, &replace, &prefix_links) != 3) {
            scan_ok = false;
         }
         *args = 0;             /* No where argument */
      } else {
         use_regexwhere = true;
      }
   }

   if (scan_ok) {
      jcr->ExpectedFiles = files;
   } else {
      /* Scan for old form without number of files */
      jcr->ExpectedFiles = 0;

      /* where= -> where=%202s\n */
      Mmsg(restore_where, "%s%%%ds\n", restorecmd, dir->msglen);
      Mmsg(restore_rwhere, "%s%%%ds\n", restorecmdR, dir->msglen);

      if (sscanf(dir->msg, restore_where, &replace, &prefix_links, args) != 3) {
         if (sscanf(dir->msg, restore_rwhere, &replace, &prefix_links, args) != 3){
            if (sscanf(dir->msg, restorecmd1, &replace, &prefix_links) != 2) {
               pm_strcpy(jcr->errmsg, dir->msg);
               Jmsg(jcr, M_FATAL, 0, _("Bad replace command. CMD=%s\n"), jcr->errmsg);
               goto free_mempool;
            }
            *args = 0;          /* No where argument */
         } else {
            use_regexwhere = true;
         }
      }
   }

   /* Turn / into nothing */
   if (IsPathSeparator(args[0]) && args[1] == '\0') {
      args[0] = '\0';
   }

   Dmsg2(150, "Got replace %c, where=%s\n", replace, args);
   unbash_spaces(args);

   /* Keep track of newly created directories to apply them correct attributes */
   if (replace == REPLACE_NEVER || replace == REPLACE_IFNEWER) {
      jcr->keep_path_list = true;
   }

   if (use_regexwhere) {
      jcr->where_bregexp = get_bregexps(args);
      if (!jcr->where_bregexp) {
         Jmsg(jcr, M_FATAL, 0, _("Bad where regexp. where=%s\n"), args);
         goto free_mempool;
      }
   } else {
      jcr->where = bstrdup(args);
   }

   jcr->replace = replace;
   jcr->prefix_links = prefix_links;

   dir->fsend(OKrestore);
   Dmsg1(110, "filed>dird: %s", dir->msg);

   jcr->setJobType(JT_RESTORE);

   jcr->setJobStatus(JS_Blocked);

   if (!open_sd_read_session(jcr)) {
      jcr->setJobStatus(JS_ErrorTerminated);
      goto bail_out;
   }

   jcr->setJobStatus(JS_Running);

   /**
    * Do restore of files and data
    */
   start_dir_heartbeat(jcr);
   generate_daemon_event(jcr, "JobStart");
   generate_plugin_event(jcr, bEventStartRestoreJob);

   do_restore(jcr);
   stop_dir_heartbeat(jcr);

   jcr->setJobStatus(JS_Terminated);
   if (jcr->JobStatus != JS_Terminated) {
      sd->suppress_error_messages(true);
   }

   /**
    * Send Close session command to Storage daemon
    */
   sd->fsend(read_close, jcr->Ticket);
   Dmsg1(100, "filed>stored: %s", sd->msg);

   bget_msg(sd);                      /* get OK */

   /* Inform Storage daemon that we are done */
   sd->signal(BNET_TERMINATE);


bail_out:
   bfree_and_null(jcr->where);

   if (jcr->JobErrors) {
      jcr->setJobStatus(JS_ErrorTerminated);
   }

   Dmsg0(100, "Done in job.c\n");

   if (jcr->multi_restore) {
      Dmsg0(100, OKstoreend);
      dir->fsend(OKstoreend);
      ret = 1;     /* we continue the loop, waiting for next part */
   } else {
      ret = 0;     /* we stop here */
   }

   if (job_canceled(jcr)) {
      ret = 0;     /* we stop here */
   }

   if (ret == 0) {
      end_restore_cmd(jcr);  /* stopping so send bEventEndRestoreJob */
   }

free_mempool:
   free_and_null_pool_memory(args);
   free_and_null_pool_memory(restore_where);
   free_and_null_pool_memory(restore_rwhere);

   return ret;
}

static int end_restore_cmd(JCR *jcr)
{
   Dmsg0(5, "end_restore_cmd\n");
   generate_plugin_event(jcr, bEventEndRestoreJob);
   return 0;                          /* return and terminate command loop */
}

static int open_sd_read_session(JCR *jcr)
{
   BSOCK *sd = jcr->store_bsock;

   if (!sd) {
      Jmsg(jcr, M_FATAL, 0, _("Improper calling sequence.\n"));
      return 0;
   }
   Dmsg4(120, "VolSessId=%ld VolsessT=%ld SF=%ld EF=%ld\n",
      jcr->VolSessionId, jcr->VolSessionTime, jcr->StartFile, jcr->EndFile);
   Dmsg2(120, "JobId=%d vol=%s\n", jcr->JobId, "DummyVolume");
   /*
    * Open Read Session with Storage daemon
    */
   sd->fsend(read_open, "DummyVolume",
      jcr->VolSessionId, jcr->VolSessionTime, jcr->StartFile, jcr->EndFile,
      jcr->StartBlock, jcr->EndBlock);
   Dmsg1(110, ">stored: %s", sd->msg);

   /*
    * Get ticket number
    */
   if (bget_msg(sd) >= 0) {
      Dmsg1(110, "filed<stored: %s", sd->msg);
      if (sscanf(sd->msg, OK_open, &jcr->Ticket) != 1) {
         Jmsg(jcr, M_FATAL, 0, _("Bad response to SD read open: %s\n"), sd->msg);
         return 0;
      }
      Dmsg1(110, "filed: got Ticket=%d\n", jcr->Ticket);
   } else {
      Jmsg(jcr, M_FATAL, 0, _("Bad response from stored to read open command\n"));
      return 0;
   }

   /*
    * Start read of data with Storage daemon
    */
   sd->fsend(read_data, jcr->Ticket);
   Dmsg1(110, ">stored: %s", sd->msg);

   /*
    * Get OK data
    */
   if (!response(jcr, sd, OK_data, "Read Data")) {
      return 0;
   }
   return 1;
}

/**
 * Destroy the Job Control Record and associated
 * resources (sockets).
 */
static void filed_free_jcr(JCR *jcr)
{
   free_bsock(jcr->dir_bsock);
   free_bsock(jcr->store_bsock);
   if (jcr->last_fname) {
      free_pool_memory(jcr->last_fname);
   }
   free_runscripts(jcr->RunScripts);
   delete jcr->RunScripts;
   free_path_list(jcr);

   if (jcr->JobId != 0)
      write_state_file(me->working_directory, "bacula-fd", get_first_port_host_order(me->FDaddrs));

   return;
}

/**
 * Get response from Storage daemon to a command we
 * sent. Check that the response is OK.
 *
 *  Returns: 0 on failure
 *           1 on success
 */
int response(JCR *jcr, BSOCK *sd, char *resp, const char *cmd)
{
   if (sd->errors) {
      return 0;
   }
   if (bget_msg(sd) > 0) {
      Dmsg0(110, sd->msg);
      if (strcmp(sd->msg, resp) == 0) {
         return 1;
      }
   }
   if (job_canceled(jcr)) {
      return 0;                       /* if canceled avoid useless error messages */
   }
   if (sd->is_error()) {
      Jmsg2(jcr, M_FATAL, 0, _("Comm error with SD. bad response to %s. ERR=%s\n"),
         cmd, sd->bstrerror());
   } else {
      Jmsg3(jcr, M_FATAL, 0, _("Bad response from SD to %s command. Wanted %s, got %s\n"),
         cmd, resp, sd->msg);
   }
   return 0;
}
