/*
 *  Signal handlers for Bacula daemons
 *
 *   Kern Sibbald, April 2000
 *
 *   Version $Id$
 * 
 * Note, we probably should do a core dump for the serious
 * signals such as SIGBUS, SIGPFE, ... 
 * Also, for SIGHUP and SIGUSR1, we should re-read the 
 * configuration file.  However, since this is a "general"  
 * routine, we leave it to the individual daemons to
 * tweek their signals after calling this routine.
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

#ifndef _NSIG
#define BA_NSIG 100
#else
#define BA_NSIG _NSIG
#endif

extern char my_name[];
extern char *exepath;
extern char *exename;

static const char *sig_names[BA_NSIG+1];

typedef void (SIG_HANDLER)(int sig);
static SIG_HANDLER *exit_handler;

/* main process id */
static pid_t main_pid = 0;

/* 
 * Handle signals here
 */
static void signal_handler(int sig)
{
   static int already_dead = FALSE;
   struct sigaction sigdefault;

   if (already_dead) {
      _exit(1);
   }
   already_dead = sig;
   if (sig == SIGTERM) {
      Emsg1(M_TERM, -1, "Shutting down Bacula service: %s ...\n", my_name);
   } else {
      Emsg2(M_FATAL, -1, "Bacula interrupted by signal %d: %s\n", sig, sig_names[sig]);
   }

#ifdef TRACEBACK
   if (sig != SIGTERM) {
      static char *argv[4];
      static char pid_buf[20];
      static char btpath[400];
      pid_t pid;

      fprintf(stderr, "Kaboom! %s, %s got signal %d. Attempting traceback.\n", 
	      exename, my_name, sig);

      if (strlen(exepath) + 12 > (int)sizeof(btpath)) {
         strcpy(btpath, "btraceback");
      } else {
	 strcpy(btpath, exepath);
         strcat(btpath, "/btraceback");
      }
      strcat(exepath, "/");
      strcat(exepath, exename);
      if (chdir(working_directory) !=0) {  /* dump in working directory */
         Pmsg2(000, "chdir to %s failed. ERR=%s\n", working_directory,  strerror(errno));
      }
      unlink("./core");               /* get rid of any old core file */
      sprintf(pid_buf, "%d", (int)main_pid);
      Dmsg1(300, "Working=%s\n", working_directory);
      Dmsg1(300, "btpath=%s\n", btpath);
      Dmsg1(300, "exepath=%s\n", exepath);
      switch (pid = fork()) {
      case -1:			      /* error */
         fprintf(stderr, "Fork error: ERR=%s\n", strerror(errno));
	 break;
      case 0:			      /* child */
	 argv[0] = btpath;	      /* path to btraceback */
	 argv[1] = exepath;	      /* path to exe */
	 argv[2] = pid_buf;
	 argv[3] = (char *)NULL;
	 if (execv(btpath, argv) != 0) {
            printf("execv: %s failed: ERR=%s\n", btpath, strerror(errno));
	 }
	 exit(-1);
      default:			      /* parent */
	 break;
      }
      sigdefault.sa_flags = 0;
      sigdefault.sa_handler = SIG_DFL;
      sigfillset(&sigdefault.sa_mask);

      sigaction(sig,  &sigdefault, NULL);
      if (pid > 0) {
         Dmsg0(500, "Doing waitpid\n");
	 waitpid(pid, NULL, 0);       /* wait for child to produce dump */
         fprintf(stderr, "Traceback complete, attempting cleanup ...\n");
         Dmsg0(500, "Done waitpid\n");
	 exit_handler(1);	      /* clean up if possible */
         Dmsg0(500, "Done exit_handler\n");
      } else {
         Dmsg0(500, "Doing sleep\n");
	 sleep(30);
      }
      fprintf(stderr, "It looks like the traceback worked ...\n");
   }
#endif

   exit_handler(1);
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
      Emsg2(M_ABORT, 0, "BA_NSIG too small (%d) should be (%d)\n", BA_NSIG, _sys_nsig);

   for (i=0; i<_sys_nsig; i++)
      sig_names[i] = _sys_siglist[i];
#else
   exit_handler = terminate;
   sig_names[0]         = "UNKNOWN SIGNAL";
   sig_names[SIGHUP]    = "Hangup";
   sig_names[SIGINT]    = "Interrupt";
   sig_names[SIGQUIT]   = "Quit";
   sig_names[SIGILL]    = "Illegal instruction";;
   sig_names[SIGTRAP]   = "Trace/Breakpoint trap";
   sig_names[SIGABRT]   = "Abort";
#ifdef SIGEMT
   sig_names[SIGEMT]    = "EMT instruction (Emulation Trap)";
#endif
#ifdef SIGIOT
   sig_names[SIGIOT]    = "IOT trap";
#endif
   sig_names[SIGBUS]    = "BUS error";
   sig_names[SIGFPE]    = "Floating-point exception";
   sig_names[SIGKILL]   = "Kill, unblockable";
   sig_names[SIGUSR1]   = "User-defined signal 1";
   sig_names[SIGSEGV]   = "Segmentation violation";
   sig_names[SIGUSR2]   = "User-defined signal 2";
   sig_names[SIGPIPE]   = "Broken pipe";
   sig_names[SIGALRM]   = "Alarm clock";
   sig_names[SIGTERM]   = "Termination";
#ifdef SIGSTKFLT
   sig_names[SIGSTKFLT] = "Stack fault";
#endif
   sig_names[SIGCHLD]   = "Child status has changed";
   sig_names[SIGCONT]   = "Continue";
   sig_names[SIGSTOP]   = "Stop, unblockable";
   sig_names[SIGTSTP]   = "Keyboard stop";
   sig_names[SIGTTIN]   = "Background read from tty";
   sig_names[SIGTTOU]   = "Background write to tty";
   sig_names[SIGURG]    = "Urgent condition on socket";
   sig_names[SIGXCPU]   = "CPU limit exceeded";
   sig_names[SIGXFSZ]   = "File size limit exceeded";
   sig_names[SIGVTALRM] = "Virtual alarm clock";
   sig_names[SIGPROF]   = "Profiling alarm clock";
   sig_names[SIGWINCH]  = "Window size change";
   sig_names[SIGIO]     = "I/O now possible";
#ifdef SIGPWR
   sig_names[SIGPWR]    = "Power failure restart";
#endif
#ifdef SIGWAITING
   sig_names[SIGWAITING] = "No runnable lwp";
#endif
#ifdef SIGLWP
   sig_name[SIGLWP]     = "SIGLWP special signal used by thread library";
#endif
#ifdef SIGFREEZE
   sig_names[SIGFREEZE] = "Checkpoint Freeze";
#endif
#ifdef SIGTHAW
   sig_names[SIGTHAW]   = "Checkpoint Thaw";
#endif
#ifdef SIGCANCEL
   sig_names[SIGCANCEL] = "Thread Cancellation";
#endif
#ifdef SIGLOST
   sig_names[SIGLOST]   = "Resource Lost (e.g. record-lock lost)";
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


   sigaction(SIGPIPE,	&sigignore, NULL);
   sigaction(SIGCHLD,	&sigignore, NULL);
   sigaction(SIGCONT,	&sigignore, NULL);
   sigaction(SIGPROF,	&sigignore, NULL);
   sigaction(SIGWINCH,	&sigignore, NULL);
   sigaction(SIGIO,	&sighandle, NULL);     

   sigaction(SIGINT,	&sigdefault, NULL);    
   sigaction(SIGXCPU,	&sigdefault, NULL);
   sigaction(SIGXFSZ,	&sigdefault, NULL);

   sigaction(SIGHUP,	&sighandle, NULL);
   sigaction(SIGQUIT,	&sighandle, NULL);   
   sigaction(SIGILL,	&sighandle, NULL);    
   sigaction(SIGTRAP,	&sighandle, NULL);   
/* sigaction(SIGABRT,	&sighandle, NULL);   */
#ifdef SIGEMT
   sigaction(SIGEMT,	&sighandle, NULL);
#endif
#ifdef SIGIOT
/* sigaction(SIGIOT,	&sighandle, NULL);  used by debugger */
#endif
   sigaction(SIGBUS,	&sighandle, NULL);    
   sigaction(SIGFPE,	&sighandle, NULL);    
   sigaction(SIGKILL,	&sighandle, NULL);   
   sigaction(SIGUSR1,	&sighandle, NULL);   
   sigaction(SIGSEGV,	&sighandle, NULL);   
   sigaction(SIGUSR2,	&sighandle, NULL);
   sigaction(SIGALRM,	&sighandle, NULL);   
   sigaction(SIGTERM,	&sighandle, NULL);   
#ifdef SIGSTKFLT
   sigaction(SIGSTKFLT, &sighandle, NULL); 
#endif
   sigaction(SIGSTOP,	&sighandle, NULL);   
   sigaction(SIGTSTP,	&sighandle, NULL);   
   sigaction(SIGTTIN,	&sighandle, NULL);   
   sigaction(SIGTTOU,	&sighandle, NULL);   
   sigaction(SIGURG,	&sighandle, NULL);    
   sigaction(SIGVTALRM, &sighandle, NULL); 
#ifdef SIGPWR
   sigaction(SIGPWR,	&sighandle, NULL);    
#endif
#ifdef SIGWAITING
   sigaction(SIGWAITING,&sighandle, NULL);
#endif
#ifdef SIGLWP
   sigaction(SIGLWP,	&sighandle, NULL);
#endif
#ifdef SIGFREEZE
   sigaction(SIGFREEZE, &sighandle, NULL);
#endif
#ifdef SIGTHAW
   sigaction(SIGTHAW,	&sighandle, NULL);
#endif
#ifdef SIGCANCEL
   sigaction(SIGCANCEL, &sighandle, NULL);
#endif
#ifdef SIGLOST
   sigaction(SIGLOST,	&sighandle, NULL);
#endif
}
