/*
 *   util.c  miscellaneous utility subroutines for Bacula
 * 
 *    Kern Sibbald, MM
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
#include "jcr.h"
#include "findlib/find.h"

/*
 * Various Bacula Utility subroutines
 *
 */

/*
 * Convert a string to btime_t (64 bit seconds)
 * Returns 0: if error
	   1: if OK, and value stored in value
 */
int string_to_btime(char *str, btime_t *value)
{
   int i, ch, len;
   double val;
   static int  mod[] = {'*', 's', 'n', 'h', 'd', 'w', 'm', 'q', 'y', 0};
   static int mult[] = {1,    1,  60, 60*60, 60*60*24, 60*60*24*7, 60*60*24*30, 
		  60*60*24*91, 60*60*24*365};

   /* Look for modifier */
   len = strlen(str);
   ch = str[len - 1];
   i = 0;
   if (ISALPHA(ch)) {
      if (ISUPPER(ch)) {
	 ch = tolower(ch);
      }
      while (mod[++i] != 0) {
	 if (ch == mod[i]) {
	    len--;
	    str[len] = 0; /* strip modifier */
	    break;
	 }
      }
   }
   if (mod[i] == 0 || !is_a_number(str)) {
      return 0;
   }
   val = strtod(str, NULL);
   if (errno != 0 || val < 0) {
      return 0;
   }
   *value = (btime_t)(val * mult[i]);
   return 1;

}

char *edit_btime(btime_t val, char *buf)
{
   char mybuf[30];
   static int mult[] = {60*60*24*365, 60*60*24*30, 60*60*24, 60*60, 60};
   static char *mod[]  = {"year",  "month",  "day", "hour", "min"};
   int i;
   uint32_t times;

   *buf = 0;
   for (i=0; i<5; i++) {
      times = val / mult[i];
      if (times > 0) {
	 val = val - (btime_t)times * mult[i];
         sprintf(mybuf, "%d %s%s ", times, mod[i], times>1?"s":"");
	 strcat(buf, mybuf);
      }
   }
   if (val == 0 && strlen(buf) == 0) {	   
      strcat(buf, "0 secs");
   } else if (val != 0) {
      sprintf(mybuf, "%d sec%s", (uint32_t)val, val>1?"s":"");
      strcat(buf, mybuf);
   }
   return buf;
}

/*
 * Check if specified string is a number or not.
 *  Taken from SQLite, cool, thanks.
 */
int is_a_number(const char *n)
{
   int digit_seen = 0;

   if( *n == '-' || *n == '+' ) {
      n++;
   }
   while (ISDIGIT(*n)) {
      digit_seen = 1;
      n++;
   }
   if (digit_seen && *n == '.') {
      n++;
      while (ISDIGIT(*n)) { n++; }
   }
   if (digit_seen && (*n == 'e' || *n == 'E')
       && (ISDIGIT(n[1]) || ((n[1]=='-' || n[1] == '+') && ISDIGIT(n[2])))) {
      n += 2;			      /* skip e- or e+ or e digit */
      while (ISDIGIT(*n)) { n++; }
   }
   return digit_seen && *n==0;
}


/*
 * Edit an integer number with commas, the supplied buffer
 * must be at least 27 bytes long.  The incoming number
 * is always widened to 64 bits.
 */
char *edit_uint64_with_commas(uint64_t val, char *buf)
{
   sprintf(buf, "%" lld, val);
   return add_commas(buf, buf);
}

/*
 * Edit an integer number, the supplied buffer
 * must be at least 27 bytes long.  The incoming number
 * is always widened to 64 bits.
 */
char *edit_uint64(uint64_t val, char *buf)
{
   sprintf(buf, "%" lld, val);
   return buf;
}


/*
 * Add commas to a string, which is presumably
 * a number.  
 */
char *add_commas(char *val, char *buf)
{
   int len, nc;
   char *p, *q;
   int i;

   if (val != buf) {
      strcpy(buf, val);
   }
   len = strlen(buf);
   if (len < 1) {
      len = 1;
   }
   nc = (len - 1) / 3;
   p = buf+len;
   q = p + nc;
   *q-- = *p--;
   for ( ; nc; nc--) {
      for (i=0; i < 3; i++) {
	  *q-- = *p--;
      }
      *q-- = ',';
   }   
   return buf;
}


/* Convert a string in place to lower case */
void lcase(char *str)
{
   while (*str) {
      if (ISUPPER(*str))
	 *str = tolower((int)(*str));
       str++;
   }
}

/* Convert spaces to non-space character. 
 * This makes scanf of fields containing spaces easier.
 */
void
bash_spaces(char *str)
{
   while (*str) {
      if (*str == ' ')
	 *str = 0x1;
      str++;
   }
}

/* Convert non-space characters (0x1) back into spaces */
void
unbash_spaces(char *str)
{
   while (*str) {
     if (*str == 0x1)
        *str = ' ';
     str++;
   }
}

/* Strip any trailing junk from the command */
void strip_trailing_junk(char *cmd)
{
   char *p;
   p = cmd + strlen(cmd) - 1;

   /* strip trailing junk from command */
   while ((p >= cmd) && (*p == '\n' || *p == '\r' || *p == ' '))
      *p-- = 0;
}

/* Strip any trailing slashes from a directory path */
void strip_trailing_slashes(char *dir)
{
   char *p;
   p = dir + strlen(dir) - 1;

   /* strip trailing slashes */
   while ((p >= dir) && (*p == '/'))
      *p-- = 0;
}

/*
 * Skip spaces
 *  Returns: 0 on failure (EOF) 	    
 *	     1 on success
 *	     new address in passed parameter 
 */
int skip_spaces(char **msg)
{
   char *p = *msg;
   if (!p) {
      return 0;
   }
   while (*p && *p == ' ') {
      p++;
   }
   *msg = p;
   return *p ? 1 : 0;
}

/*
 * Skip nonspaces
 *  Returns: 0 on failure (EOF) 	    
 *	     1 on success
 *	     new address in passed parameter 
 */
int skip_nonspaces(char **msg)
{
   char *p = *msg;

   if (!p) {
      return 0;
   }
   while (*p && *p != ' ') {
      p++;
   }
   *msg = p;
   return *p ? 1 : 0;
}

/* folded search for string - case insensitive */
int
fstrsch(char *a, char *b)   /* folded case search */
{
   register char *s1,*s2;
   register char c1, c2;

   s1=a;
   s2=b;
   while (*s1) {		      /* do it the fast way */
      if ((*s1++ | 0x20) != (*s2++ | 0x20))
	 return 0;		      /* failed */
   }
   while (*a) { 		      /* do it over the correct slow way */
      if (ISUPPER(c1 = *a)) {
	 c1 = tolower((int)c1);
      }
      if (ISUPPER(c2 = *b)) {
	 c2 = tolower((int)c2);
      }
      if (c1 != c2) {
	 return 0;
      }
      a++;
      b++;
   }
   return 1;
}


char *encode_time(time_t time, char *buf)
{
   struct tm tm;
   int n = 0;

   if (localtime_r(&time, &tm)) {
      n = sprintf(buf, "%04d-%02d-%02d %02d:%02d:%02d",
		   tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
		   tm.tm_hour, tm.tm_min, tm.tm_sec);
   }
   return buf+n;
}

/*
 * Concatenate a string (str) onto a pool memory buffer pm
 */
void pm_strcat(POOLMEM **pm, char *str)
{
   int pmlen = strlen(*pm);
   int len = strlen(str) + 1;

   *pm = check_pool_memory_size(*pm, pmlen + len);
   memcpy(*pm+pmlen, str, len);
}


/*
 * Copy a string (str) into a pool memory buffer pm
 */
void pm_strcpy(POOLMEM **pm, char *str)
{
   int len = strlen(str) + 1;

   *pm = check_pool_memory_size(*pm, len);
   memcpy(*pm, str, len);
}


/*
 * Convert a JobStatus code into a human readable form
 */
void jobstatus_to_ascii(int JobStatus, char *msg, int maxlen)
{
   char *termstat, jstat[2];

   switch (JobStatus) {
      case JS_Terminated:
         termstat = _("OK");
	 break;
     case JS_FatalError:
     case JS_ErrorTerminated:
         termstat = _("Error");
	 break;
     case JS_Error:
         termstat = _("Non-fatal error");
	 break;
     case JS_Cancelled:
         termstat = _("Cancelled");
	 break;
     case JS_Differences:
         termstat = _("Verify differences");
	 break;
     default:
	 jstat[0] = last_job.JobStatus;
	 jstat[1] = 0;
	 termstat = jstat;
	 break;
   }
   strncpy(msg, termstat, maxlen);
   msg[maxlen-1] = 0;
}

/*
 * Convert Job Termination Status into a string
 */
char *job_status_to_str(int stat) 
{
   char *str;

   switch (stat) {
   case JS_Terminated:
      str = _("OK");
      break;
   case JS_ErrorTerminated:
   case JS_Error:
      str = _("Error");
      break;
   case JS_FatalError:
      str = _("Fatal Error");
      break;
   case JS_Cancelled:
      str = _("Cancelled");
      break;
   case JS_Differences:
      str = _("Differences");
      break;
   default:
      str = _("Unknown term code");
      break;
   }
   return str;
}


/*
 * Convert Job Type into a string
 */
char *job_type_to_str(int type) 
{
   char *str;

   switch (type) {
   case JT_BACKUP:
      str = _("Backup");
      break;
   case JT_VERIFY:
      str = _("Verify");
      break;
   case JT_RESTORE:
      str = _("Restore");
      break;
   case JT_ADMIN:
      str = _("Admin");
   default:
      str = _("Unknown Type");
      break;
   }
   return str;
}

/*
 * Convert Job Level into a string
 */
char *job_level_to_str(int level) 
{
   char *str;

   switch (level) {
   case L_FULL:
      str = _("Full");
      break;
   case L_INCREMENTAL:
      str = _("Incremental");
      break;
   case L_DIFFERENTIAL:
      str = _("Differential");
      break;
   case L_LEVEL:
      str = _("Level");
      break;
   case L_SINCE:
      str = _("Since");
      break;
   case L_VERIFY_CATALOG:
      str = _("Verify Catalog");
      break;
   case L_VERIFY_INIT:
      str = _("Verify Init Catalog");
      break;
   case L_VERIFY_VOLUME_TO_CATALOG:
      str = _("Verify Volume to Catalog");
      break;
   case L_VERIFY_DATA:
      str = _("Verify Data");
      break;
   default:
      str = _("Unknown Job Level");
      break;
   }
   return str;
}


/***********************************************************************
 * Encode the mode bits into a 10 character string like LS does
 ***********************************************************************/

char *encode_mode(mode_t mode, char *buf)
{
  char *cp = buf;  

  *cp++ = S_ISDIR(mode) ? 'd' : S_ISBLK(mode) ? 'b' : S_ISCHR(mode) ? 'c' :
          S_ISLNK(mode) ? 'l' : '-';
  *cp++ = mode & S_IRUSR ? 'r' : '-';
  *cp++ = mode & S_IWUSR ? 'w' : '-';
  *cp++ = (mode & S_ISUID
               ? (mode & S_IXUSR ? 's' : 'S')
               : (mode & S_IXUSR ? 'x' : '-'));
  *cp++ = mode & S_IRGRP ? 'r' : '-';
  *cp++ = mode & S_IWGRP ? 'w' : '-';
  *cp++ = (mode & S_ISGID
               ? (mode & S_IXGRP ? 's' : 'S')
               : (mode & S_IXGRP ? 'x' : '-'));
  *cp++ = mode & S_IROTH ? 'r' : '-';
  *cp++ = mode & S_IWOTH ? 'w' : '-';
  *cp++ = (mode & S_ISVTX
               ? (mode & S_IXOTH ? 't' : 'T')
               : (mode & S_IXOTH ? 'x' : '-'));
  *cp = '\0';
  return cp;
}


int do_shell_expansion(char *name)
{
/*  ****FIXME***** this should work for Win32 too */
#define UNIX
#ifdef UNIX
#ifndef PATH_MAX
#define PATH_MAX 512
#endif

   int pid, wpid, stat;
   int waitstatus;
   char *shellcmd;
   void (*istat)(int), (*qstat)(int);
   int i;
   char echout[PATH_MAX + 256];
   int pfd[2];
   static char meta[] = "~\\$[]*?`'<>\"";
   int found = FALSE;
   int len;

   /* Check if any meta characters are present */
   len = strlen(meta);
   for (i = 0; i < len; i++) {
      if (strchr(name, meta[i])) {
	 found = TRUE;
	 break;
      }
   }
   stat = 0;
   if (found) {
#ifdef nt
       /* If the filename appears to be a DOS filename,
          convert all backward slashes \ to Unix path
          separators / and insert a \ infront of spaces. */
       len = strlen(name);
       if (len >= 3 && name[1] == ':' && name[2] == '\\') {
	  for (i=2; i<len; i++)
             if (name[i] == '\\')
                name[i] = '/';
       }
#else
       /* Pass string off to the shell for interpretation */
       if (pipe(pfd) == -1)
	  return 0;
       switch(pid = fork()) {
       case -1:
	  break;

       case 0:				  /* child */
	  /* look for shell */
          if ((shellcmd = getenv("SHELL")) == NULL)
             shellcmd = "/bin/sh";
	  close(1); dup(pfd[1]);	  /* attach pipes to stdin and stdout */
	  close(2); dup(pfd[1]);
	  for (i = 3; i < 32; i++)	  /* close everything else */
	     close(i);
          strcpy(echout, "echo ");        /* form echo command */
	  strcat(echout, name);
          execl(shellcmd, shellcmd, "-c", echout, NULL); /* give to shell */
          exit(127);                      /* shouldn't get here */

       default: 			  /* parent */
	  /* read output from child */
	  i = read(pfd[0], echout, sizeof echout);
	  echout[--i] = 0;		  /* set end of string */
	  /* look for first word or first line. */
	  while (--i >= 0) {
             if (echout[i] == ' ' || echout[i] == '\n')
		echout[i] = 0;		  /* keep only first one */
	  }
	  istat = signal(SIGINT, SIG_IGN);
	  qstat = signal(SIGQUIT, SIG_IGN);
	  /* wait for child to exit */
	  while ((wpid = wait(&waitstatus)) != pid && wpid != -1)
	     { ; }
	  signal(SIGINT, istat);
	  signal(SIGQUIT, qstat);
	  strcpy(name, echout);
	  stat = 1;
	  break;
       }
       close(pfd[0]);			  /* close pipe */
       close(pfd[1]);
#endif /* nt */
   }
   return stat;

#endif /* UNIX */

#if  MSC | MSDOS | __WATCOMC__

   char prefix[100], *env, *getenv();

   /* Home directory reference? */
   if (*name == '~' && (env=getenv("HOME"))) {
      strcpy(prefix, env);	      /* copy HOME directory name */
      name++;			      /* skip over ~ in name */
      strcat(prefix, name);
      name--;			      /* get back to beginning */
      strcpy(name, prefix);	      /* move back into name */
   }
   return 1;
#endif

}

#define MAX_ARGV 100
static void build_argc_argv(char *cmd, int *bargc, char *bargv[], int max_arg);

/*
 * Run an external program. Optionally wait a specified number
 *   of seconds. Program killed if wait exceeded. Optionally
 *   return the output from the program (normally a single line).
 */
int run_program(char *prog, int wait, POOLMEM *results)
{
   int stat = ETIME;
   int chldstatus = 0;
   pid_t pid1, pid2 = 0;
   int pfd[2];
   char *bargv[MAX_ARGV];
   int bargc;

   
   build_argc_argv(prog, &bargc, bargv, MAX_ARGV);
#ifdef xxxxxxxxxx
   printf("argc=%d\n", bargc);
   int i;
   for (i=0; i<bargc; i++) {
      printf("argc=%d argv=%s\n", i, bargv[i]);
   }
#endif

   if (results && pipe(pfd) == -1) {
      return errno;
   }
   /* Start worker process */
   switch (pid1 = fork()) {
   case -1:
      break;

   case 0:			      /* child */
//    printf("execl of %s\n", prog);
      if (results) {
	 close(1); dup(pfd[1]);       /* attach pipes to stdin and stdout */
	 close(2); dup(pfd[1]);
      }
      execvp(bargv[0], bargv);
      exit(errno);                   /* shouldn't get here */

   default:			      /* parent */
      /* start timer process */
      if (wait > 0) {
	 switch (pid2=fork()) {
	 case -1:
	    break;
	 case 0:			 /* child 2 */
	    /* Time the worker process */  
	    sleep(wait);
	    if (kill(pid1, SIGTERM) == 0) { /* time expired kill it */
	       exit(0);
	    }
	    sleep(3);
	    kill(pid1, SIGKILL);
	    exit(0);
	 default:			 /* parent */
	    break;
	 }
      }

      /* Parent continues here */
      int i;
      if (results) {
	 i = read(pfd[0], results, sizeof_pool_memory(results) - 1);
	 if (--i < 0) {
	    i = 0;
	 }
	 results[i] = 0;		/* set end of string */
      }
      /* wait for worker child to exit */
      for ( ;; ) {
	 pid_t wpid;
	 wpid = waitpid(pid1, &chldstatus, 0);	       
	 if (wpid == pid1 || (errno != EINTR)) {
	    break;
	 }
      }
      if (WIFEXITED(chldstatus))
	 stat = WEXITSTATUS(chldstatus);

      if (wait > 0) {
	 kill(pid2, SIGKILL);		/* kill off timer process */
	 waitpid(pid2, &chldstatus, 0); /* reap timer process */
      }
      if (results) { 
	 close(pfd[0]); 	     /* close pipe */
	 close(pfd[1]);
      }
      break;
   }
   return stat;
}

/*
 * Build argc and argv from a string
 */
static void build_argc_argv(char *cmd, int *bargc, char *bargv[], int max_argv)
{
   int i, quote;
   char *p, *q;
   int argc = 0;

   argc = 0;
   for (i=0; i<max_argv; i++)
      bargv[i] = NULL;

   p = cmd;
   quote = 0;
   while  (*p && (*p == ' ' || *p == '\t'))
      p++;
   if (*p == '\"') {
      quote = 1;
      p++;
   }
   if (*p) {
      while (*p && argc < MAX_ARGV) {
	 q = p;
	 if (quote) {
            while (*q && *q != '\"')
	    q++;
	    quote = 0;
	 } else {
            while (*q && *q != ' ')
	    q++;
	 }
	 if (*q)
            *(q++) = '\0';
	 bargv[argc++] = p;
	 p = q;
         while (*p && (*p == ' ' || *p == '\t'))
	    p++;
         if (*p == '\"') {
	    quote = 1;
	    p++;
	 }
      }
   }
   *bargc = argc;
}
