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

/* Return true of buffer has all zero bytes */
int is_buf_zero(char *buf, int len)
{
   uint64_t *ip = (uint64_t *)buf;
   char *p;
   int i, len64, done, rem;

   /* Optimize by checking uint64_t for zero */
   len64 = len >> sizeof(uint64_t);
   for (i=0; i < len64; i++) {
      if (ip[i] != 0) {
	 return 0;
      }
   }
   done = len64 << sizeof(uint64_t);  /* bytes already checked */
   p = buf + done;
   rem = len - done;
   for (i = 0; i < rem; i++) {
      if (p[i] != 0) {
	 return 0;
      }
   }
   return 1;
}


/* Convert a string in place to lower case */
void lcase(char *str)
{
   while (*str) {
      if (B_ISUPPER(*str))
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
      if (B_ISUPPER(c1 = *a)) {
	 c1 = tolower((int)c1);
      }
      if (B_ISUPPER(c2 = *b)) {
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
   bstrncpy(msg, termstat, maxlen);
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
      break;
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


/*  MAKESESSIONKEY  --	Generate session key with optional start
			key.  If mode is TRUE, the key will be
			translated to a string, otherwise it is
			returned as 16 binary bytes.

    from SpeakFreely by John Walker */

void makeSessionKey(char *key, char *seed, int mode)
{
     int j, k;
     struct MD5Context md5c;
     unsigned char md5key[16], md5key1[16];
     char s[1024];

     s[0] = 0;
     if (seed != NULL) {
	strcat(s, seed);
     }

     /* The following creates a seed for the session key generator
	based on a collection of volatile and environment-specific
	information unlikely to be vulnerable (as a whole) to an
        exhaustive search attack.  If one of these items isn't
	available on your machine, replace it with something
	equivalent or, if you like, just delete it. */

     sprintf(s + strlen(s), "%lu", (unsigned long) getpid());
     sprintf(s + strlen(s), "%lu", (unsigned long) getppid());
     getcwd(s + strlen(s), 256);
     sprintf(s + strlen(s), "%lu", (unsigned long) clock());
     sprintf(s + strlen(s), "%lu", (unsigned long) time(NULL));
#ifdef Solaris
     sysinfo(SI_HW_SERIAL,s + strlen(s), 12);
#endif
#ifdef HAVE_GETHOSTID
     sprintf(s + strlen(s), "%lu", (unsigned long) gethostid());
#endif
#ifdef HAVE_GETDOMAINNAME
     getdomainname(s + strlen(s), 256);
#endif
     gethostname(s + strlen(s), 256);
     sprintf(s + strlen(s), "%u", (unsigned)getuid());
     sprintf(s + strlen(s), "%u", (unsigned)getgid());
     MD5Init(&md5c);
     MD5Update(&md5c, (unsigned char *)s, strlen(s));
     MD5Final(md5key, &md5c);
     sprintf(s + strlen(s), "%lu", (unsigned long) ((time(NULL) + 65121) ^ 0x375F));
     MD5Init(&md5c);
     MD5Update(&md5c, (unsigned char *)s, strlen(s));
     MD5Final(md5key1, &md5c);
#define nextrand    (md5key[j] ^ md5key1[j])
     if (mode) {
	for (j = k = 0; j < 16; j++) {
	    unsigned char rb = nextrand;

#define Rad16(x) ((x) + 'A')
	    key[k++] = Rad16((rb >> 4) & 0xF);
	    key[k++] = Rad16(rb & 0xF);
#undef Rad16
	    if (j & 1) {
                 key[k++] = '-';
	    }
	}
	key[--k] = 0;
     } else {
	for (j = 0; j < 16; j++) {
	    key[j] = nextrand;
	}
     }
}
#undef nextrand
