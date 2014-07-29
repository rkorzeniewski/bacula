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
 *  Signal handlers for Bacula daemons
 *
 *   Kern Sibbald, April 2000
 *
 * Note, we probably should do a core dump for the serious
 * signals such as SIGBUS, SIGPFE, ...
 * Also, for SIGHUP and SIGUSR1, we should re-read the
 * configuration file.  However, since this is a "general"
 * routine, we leave it to the individual daemons to
 * tweek their signals after calling this routine.
 *
 */

#ifndef HAVE_WIN32
#include "bacula.h"

#ifndef _NSIG
#define BA_NSIG 100
#else
#define BA_NSIG _NSIG
#endif

extern char my_name[];
extern char fail_time[];
extern char *exepath;
extern char *exename;
extern bool prt_kaboom;

static const char *sig_names[BA_NSIG+1];

typedef void (SIG_HANDLER)(int sig);
static SIG_HANDLER *exit_handler;

/* main process id */
static pid_t main_pid = 0;

const char *get_signal_name(int sig)
{
   if (sig < 0 || sig > BA_NSIG || !sig_names[sig]) {
      return _("Invalid signal number");
   } else {
      return sig_names[sig];
   }
}

/* defined in jcr.c */
extern void dbg_print_jcr(FILE *fp);
/* defined in plugins.c */
extern void dbg_print_plugin(FILE *fp);
/* defined in lockmgr.c */
extern void dbg_print_lock(FILE *fp);

/*
 * !!! WARNING !!!
 *
 * This function should be used ONLY after a violent signal. We walk through the
 * JCR chain without locking, Bacula should not be running.
 */
static void dbg_print_bacula()
{
   char buf[512];

   snprintf(buf, sizeof(buf), "%s/%s.%d.lockdump",
            working_directory, my_name, (int)getpid());
   FILE *fp = fopen(buf, "a+") ;
   if (!fp) {
      fp = stderr;
   }

   fprintf(stderr, "Dumping: %s\n", buf);

   /* Print also B_DB and RWLOCK structure
    * Can add more info about JCR with dbg_jcr_add_hook()
    */
   dbg_print_lock(fp);
   dbg_print_jcr(fp);
   dbg_print_plugin(fp);

   if (fp != stderr) {
#define direct_print
#ifdef direct_print
      if (prt_kaboom) {
         rewind(fp);
         printf("\n\n ==== lockdump output ====\n\n");
         while (fgets(buf, (int)sizeof(buf), fp) != NULL) {
            printf("%s", buf);
         }
         printf(" ==== End baktrace output ====\n\n");
      }
#else
      if (prt_kaboom) {
         char buf1[512];
         printf("\n\n ==== lockdump output ====\n\n");
         snprintf(buf1, sizeof(buf1), "/bin/cat %s", buf);
         system(buf1);
         printf(" ==== End baktrace output ====\n\n");
      }
#endif
      fclose(fp);
   }
}

/*
 * Handle signals here
 */
extern "C" void signal_handler(int sig)
{
   static int already_dead = 0;
   int chld_status=-1;
   utime_t now;

   /* If we come back more than once, get out fast! */
   if (already_dead) {
      exit(1);
   }
   Dmsg2(900, "sig=%d %s\n", sig, sig_names[sig]);
   /* Ignore certain signals -- SIGUSR2 used to interrupt threads */
   if (sig == SIGCHLD || sig == SIGUSR2) {
      return;
   }
   /* FreeBSD seems to generate a signal of 0, which is of course undefined */
   if (sig == 0) {
      return;
   }
   already_dead++;
   /* Don't use Emsg here as it may lock and thus block us */
   if (sig == SIGTERM) {
       syslog(LOG_DAEMON|LOG_ERR, "Shutting down Bacula service: %s ...\n", my_name);
   } else {
      fprintf(stderr, _("Bacula interrupted by signal %d: %s\n"), sig, get_signal_name(sig));
      syslog(LOG_DAEMON|LOG_ERR,
         _("Bacula interrupted by signal %d: %s\n"), sig, get_signal_name(sig));
      /* Edit current time for showing in the dump */
      now = time(NULL);
      bstrftimes(fail_time, 30, now);
   }

#ifdef TRACEBACK
   if (sig != SIGTERM) {
      struct sigaction sigdefault;
      static char *argv[5];
      static char pid_buf[20];
      static char btpath[400];
      char buf[400];
      pid_t pid;
      int exelen = strlen(exepath);

      fprintf(stderr, _("Kaboom! %s, %s got signal %d - %s at %s. Attempting traceback.\n"),
              exename, my_name, sig, get_signal_name(sig), fail_time);
      fprintf(stderr, _("Kaboom! exepath=%s\n"), exepath);

      if (exelen + 12 > (int)sizeof(btpath)) {
         bstrncpy(btpath, "btraceback", sizeof(btpath));
      } else {
         bstrncpy(btpath, exepath, sizeof(btpath));
         if (IsPathSeparator(btpath[exelen-1])) {
            btpath[exelen-1] = 0;
         }
         bstrncat(btpath, "/btraceback", sizeof(btpath));
      }
      if (!IsPathSeparator(exepath[exelen - 1])) {
         strcat(exepath, "/");
      }
      strcat(exepath, exename);
      if (!working_directory) {
         working_directory = buf;
         *buf = 0;
      }
      if (*working_directory == 0) {
         strcpy((char *)working_directory, "/tmp/");
      }
      if (chdir(working_directory) != 0) {  /* dump in working directory */
         berrno be;
         Pmsg2(000, "chdir to %s failed. ERR=%s\n", working_directory,  be.bstrerror());
         strcpy((char *)working_directory, "/tmp/");
      }
      unlink("./core");               /* get rid of any old core file */

#ifdef DEVELOPER /* When DEVELOPER not set, this is done below */
      /* print information about the current state into working/<file>.lockdump */
      dbg_print_bacula();
#endif


      sprintf(pid_buf, "%d", (int)main_pid);
      Dmsg1(300, "Working=%s\n", working_directory);
      Dmsg1(300, "btpath=%s\n", btpath);
      Dmsg1(300, "exepath=%s\n", exepath);
      switch (pid = fork()) {
      case -1:                        /* error */
         fprintf(stderr, _("Fork error: ERR=%s\n"), strerror(errno));
         break;
      case 0:                         /* child */
         argv[0] = btpath;            /* path to btraceback */
         argv[1] = exepath;           /* path to exe */
         argv[2] = pid_buf;
         argv[3] = (char *)working_directory;
         argv[4] = (char *)NULL;
         fprintf(stderr, _("Calling: %s %s %s %s\n"), btpath, exepath, pid_buf,
            working_directory);
         if (execv(btpath, argv) != 0) {
            berrno be;
            printf(_("execv: %s failed: ERR=%s\n"), btpath, be.bstrerror());
         }
         exit(-1);
      default:                        /* parent */
         break;
      }

      /* Parent continue here, waiting for child */
      sigdefault.sa_flags = 0;
      sigdefault.sa_handler = SIG_DFL;
      sigfillset(&sigdefault.sa_mask);

      sigaction(sig,  &sigdefault, NULL);
      if (pid > 0) {
         Dmsg0(500, "Doing waitpid\n");
         waitpid(pid, &chld_status, 0);   /* wait for child to produce dump */
         Dmsg0(500, "Done waitpid\n");
      } else {
         Dmsg0(500, "Doing sleep\n");
         bmicrosleep(30, 0);
      }
      if (WEXITSTATUS(chld_status) == 0) {
         fprintf(stderr, _("It looks like the traceback worked...\n"));
      } else {
         fprintf(stderr, _("The btraceback call returned %d\n"),
                           WEXITSTATUS(chld_status));
      }
      /* If we want it printed, do so */
#ifdef direct_print
      if (prt_kaboom) {
         FILE *fd;
         snprintf(buf, sizeof(buf), "%s/bacula.%s.traceback", working_directory, pid_buf);
         fd = fopen(buf, "r");
         if (fd != NULL) {
            printf("\n\n ==== Traceback output ====\n\n");
            while (fgets(buf, (int)sizeof(buf), fd) != NULL) {
               printf("%s", buf);
            }
            fclose(fd);
            printf(" ==== End traceback output ====\n\n");
         }
      }
#else
      if (prt_kaboom) {
         snprintf(buf, sizeof(buf), "/bin/cat %s/bacula.%s.traceback", working_directory, pid_buf);
         fprintf(stderr, "\n\n ==== Traceback output ====\n\n");
         system(buf);
         fprintf(stderr, " ==== End traceback output ====\n\n");
      }
#endif

#ifndef DEVELOPER /* When DEVELOPER set, this is done above */
      /* print information about the current state into working/<file>.lockdump */
      dbg_print_bacula();
#endif

   }
#endif
   exit_handler(sig);
   Dmsg0(500, "Done exit_handler\n");
}

/*
 * Init stack dump by saving main process id --
 *   needed by debugger to attach to this program.
 */
void init_stack_dump(void)
{
   main_pid = getpid();               /* save main thread's pid */
}

/*
 * Initialize signals
 */
void init_signals(void terminate(int sig))
{
   struct sigaction sighandle;
   struct sigaction sigignore;
   struct sigaction sigdefault;
#ifdef _sys_nsig
   int i;

   exit_handler = terminate;
   if (BA_NSIG < _sys_nsig)
      Emsg2(M_ABORT, 0, _("BA_NSIG too small (%d) should be (%d)\n"), BA_NSIG, _sys_nsig);

   for (i=0; i<_sys_nsig; i++)
      sig_names[i] = _sys_siglist[i];
#else
   exit_handler = terminate;
   sig_names[0]         = _("UNKNOWN SIGNAL");
   sig_names[SIGHUP]    = _("Hangup");
   sig_names[SIGINT]    = _("Interrupt");
   sig_names[SIGQUIT]   = _("Quit");
   sig_names[SIGILL]    = _("Illegal instruction");;
   sig_names[SIGTRAP]   = _("Trace/Breakpoint trap");
   sig_names[SIGABRT]   = _("Abort");
#ifdef SIGEMT
   sig_names[SIGEMT]    = _("EMT instruction (Emulation Trap)");
#endif
#ifdef SIGIOT
   sig_names[SIGIOT]    = _("IOT trap");
#endif
   sig_names[SIGBUS]    = _("BUS error");
   sig_names[SIGFPE]    = _("Floating-point exception");
   sig_names[SIGKILL]   = _("Kill, unblockable");
   sig_names[SIGUSR1]   = _("User-defined signal 1");
   sig_names[SIGSEGV]   = _("Segmentation violation");
   sig_names[SIGUSR2]   = _("User-defined signal 2");
   sig_names[SIGPIPE]   = _("Broken pipe");
   sig_names[SIGALRM]   = _("Alarm clock");
   sig_names[SIGTERM]   = _("Termination");
#ifdef SIGSTKFLT
   sig_names[SIGSTKFLT] = _("Stack fault");
#endif
   sig_names[SIGCHLD]   = _("Child status has changed");
   sig_names[SIGCONT]   = _("Continue");
   sig_names[SIGSTOP]   = _("Stop, unblockable");
   sig_names[SIGTSTP]   = _("Keyboard stop");
   sig_names[SIGTTIN]   = _("Background read from tty");
   sig_names[SIGTTOU]   = _("Background write to tty");
   sig_names[SIGURG]    = _("Urgent condition on socket");
   sig_names[SIGXCPU]   = _("CPU limit exceeded");
   sig_names[SIGXFSZ]   = _("File size limit exceeded");
   sig_names[SIGVTALRM] = _("Virtual alarm clock");
   sig_names[SIGPROF]   = _("Profiling alarm clock");
   sig_names[SIGWINCH]  = _("Window size change");
   sig_names[SIGIO]     = _("I/O now possible");
#ifdef SIGPWR
   sig_names[SIGPWR]    = _("Power failure restart");
#endif
#ifdef SIGWAITING
   sig_names[SIGWAITING] = _("No runnable lwp");
#endif
#ifdef SIGLWP
   sig_names[SIGLWP]     = _("SIGLWP special signal used by thread library");
#endif
#ifdef SIGFREEZE
   sig_names[SIGFREEZE] = _("Checkpoint Freeze");
#endif
#ifdef SIGTHAW
   sig_names[SIGTHAW]   = _("Checkpoint Thaw");
#endif
#ifdef SIGCANCEL
   sig_names[SIGCANCEL] = _("Thread Cancellation");
#endif
#ifdef SIGLOST
   sig_names[SIGLOST]   = _("Resource Lost (e.g. record-lock lost)");
#endif
#endif


/* Now setup signal handlers */
   sighandle.sa_flags = 0;
   sighandle.sa_handler = signal_handler;
   sigfillset(&sighandle.sa_mask);
   sigignore.sa_flags = 0;
   sigignore.sa_handler = SIG_IGN;
   sigfillset(&sigignore.sa_mask);
   sigdefault.sa_flags = 0;
   sigdefault.sa_handler = SIG_DFL;
   sigfillset(&sigdefault.sa_mask);


   sigaction(SIGPIPE,   &sigignore, NULL);
   sigaction(SIGCHLD,   &sighandle, NULL);
   sigaction(SIGCONT,   &sigignore, NULL);
   sigaction(SIGPROF,   &sigignore, NULL);
   sigaction(SIGWINCH,  &sigignore, NULL);
   sigaction(SIGIO,     &sighandle, NULL);

   sigaction(SIGINT,    &sigdefault, NULL);
   sigaction(SIGXCPU,   &sigdefault, NULL);
   sigaction(SIGXFSZ,   &sigdefault, NULL);

   sigaction(SIGHUP,    &sigignore, NULL);
   sigaction(SIGQUIT,   &sighandle, NULL);
   sigaction(SIGILL,    &sighandle, NULL);
   sigaction(SIGTRAP,   &sighandle, NULL);
   sigaction(SIGABRT,   &sighandle, NULL);
#ifdef SIGEMT
   sigaction(SIGEMT,    &sighandle, NULL);
#endif
#ifdef SIGIOT
   sigaction(SIGIOT,    &sighandle, NULL);
#endif
   sigaction(SIGBUS,    &sighandle, NULL);
   sigaction(SIGFPE,    &sighandle, NULL);
/* sigaction(SIGKILL,   &sighandle, NULL);  cannot be trapped */
   sigaction(SIGUSR1,   &sighandle, NULL);
   sigaction(SIGSEGV,   &sighandle, NULL);
   sigaction(SIGUSR2,   &sighandle, NULL);
   sigaction(SIGALRM,   &sighandle, NULL);
   sigaction(SIGTERM,   &sighandle, NULL);
#ifdef SIGSTKFLT
   sigaction(SIGSTKFLT, &sighandle, NULL);
#endif
/* sigaction(SIGSTOP,   &sighandle, NULL); cannot be trapped */
   sigaction(SIGTSTP,   &sighandle, NULL);
   sigaction(SIGTTIN,   &sighandle, NULL);
   sigaction(SIGTTOU,   &sighandle, NULL);
   sigaction(SIGURG,    &sighandle, NULL);
   sigaction(SIGVTALRM, &sighandle, NULL);
#ifdef SIGPWR
   sigaction(SIGPWR,    &sighandle, NULL);
#endif
#ifdef SIGWAITING
   sigaction(SIGWAITING,&sighandle, NULL);
#endif
#ifdef SIGLWP
   sigaction(SIGLWP,    &sighandle, NULL);
#endif
#ifdef SIGFREEZE
   sigaction(SIGFREEZE, &sighandle, NULL);
#endif
#ifdef SIGTHAW
   sigaction(SIGTHAW,   &sighandle, NULL);
#endif
#ifdef SIGCANCEL
   sigaction(SIGCANCEL, &sighandle, NULL);
#endif
#ifdef SIGLOST
   sigaction(SIGLOST,   &sighandle, NULL);
#endif
}
#endif
