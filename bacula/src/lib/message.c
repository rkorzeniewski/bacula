/*
 * Bacula message handling routines
 *
 *   Kern Sibbald, April 2000 
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
#include "jcr.h"

#define FULL_LOCATION 1 	      /* set for file:line in Debug messages */

/* 
 *  This is where we define "Globals" because all the
 *    daemons include this file.
 */
char *working_directory = NULL;       /* working directory path stored here */
int verbose = 0;		      /* increase User messages */
int debug_level = 0;		      /* debug level */
time_t daemon_start_time = 0;	      /* Daemon start time */
char *version = VERSION " (" BDATE ")";
char my_name[30];		      /* daemon name is stored here */
char *exepath = (char *)NULL;
char *exename = (char *)NULL;
int console_msg_pending = 0;
char con_fname[500];		      /* Console filename */
FILE *con_fd = NULL;		      /* Console file descriptor */
brwlock_t con_lock;		      /* Console lock structure */
FILE *trace_fd = NULL;

#ifdef HAVE_POSTGRESQL
char catalog_db[] = "PostgreSQL";
#else
#ifdef HAVE_MYSQL
char catalog_db[] = "MySQL";
#else 
#ifdef HAVE_SQLITE
char catalog_db[] = "SQLite";
#else
char catalog_db[] = "Internal";
#endif
#endif
#endif

const char *host_os = HOST_OS;
const char *distname = DISTNAME;
const char *distver = DISTVER;

/* Forward referenced functions */

/* Imported functions */


/* Static storage */

static MSGS *daemon_msgs;	       /* global messages */

/* 
 * Set daemon name. Also, find canonical execution
 *  path.  Note, exepath has spare room for tacking on
 *  the exename so that we can reconstruct the full name.
 *
 * Note, this routine can get called multiple times
 *  The second time is to put the name as found in the
 *  Resource record. On the second call, generally,
 *  argv is NULL to avoid doing the path code twice.
 */
#define BTRACE_EXTRA 20
void my_name_is(int argc, char *argv[], const char *name)
{
   char *l, *p, *q;
   char cpath[400], npath[400];
   int len;

   bstrncpy(my_name, name, sizeof(my_name));
   if (argc>0 && argv && argv[0]) {
      /* strip trailing filename and save exepath */
      for (l=p=argv[0]; *p; p++) {
         if (*p == '/') {
	    l = p;			 /* set pos of last slash */
	 }
      }
      if (*l == '/') {
	 l++;
      } else {
	 l = argv[0];
#if defined(HAVE_CYGWIN) || defined(HAVE_WIN32)
	 /* On Windows allow c: junk */
         if (l[1] == ':') {
	    l += 2;
	 }
#endif
      }
      len = strlen(l) + 1;
      if (exename) {
	 free(exename);
      }
      exename = (char *)malloc(len);
      strcpy(exename, l);

      if (exepath) {
	 free(exepath);
      }
      exepath = (char *)malloc(strlen(argv[0]) + 1 + len);
      for (p=argv[0],q=exepath; p < l; ) {
	 *q++ = *p++;
      }
      *q = 0;
      Dmsg1(200, "exepath=%s\n", exepath);
      if (strchr(exepath, '.') || exepath[0] != '/') {
	 npath[0] = 0;
	 if (getcwd(cpath, sizeof(cpath))) {
	    if (chdir(exepath) == 0) {
	       if (!getcwd(npath, sizeof(npath))) {
		  npath[0] = 0;
	       }
	       chdir(cpath);
	    }
	    if (npath[0]) {
	       free(exepath);
	       exepath = (char *)malloc(strlen(npath) + 1 + len);
	       strcpy(exepath, npath);
	    }
	 }
         Dmsg1(200, "Normalized exepath=%s\n", exepath);
      }
   }
}

/* 
 * Initialize message handler for a daemon or a Job
 *   We make a copy of the MSGS resource passed, so it belows
 *   to the job or daemon and thus can be modified.
 * 
 *   NULL for jcr -> initialize global messages for daemon
 *   non-NULL	  -> initialize jcr using Message resource
 */
void
init_msg(JCR *jcr, MSGS *msg)
{
   DEST *d, *dnew, *temp_chain = NULL;
   int i;

   if (jcr == NULL && msg == NULL) {
      init_last_jobs_list();
   }

#ifndef HAVE_WIN32
   /*
    * Make sure we have fd's 0, 1, 2 open
    *  If we don't do this one of our sockets may open
    *  there and if we then use stdout, it could
    *  send total garbage to our socket.
    *
    */
   int fd;
   fd = open("/dev/null", O_RDONLY, 0644);
   if (fd > 2) {
      close(fd);
   } else {
      for(i=1; fd + i <= 2; i++) {
	 dup2(fd, fd+i);
      }
   }

#endif
   /*
    * If msg is NULL, initialize global chain for STDOUT and syslog
    */
   if (msg == NULL) {
      daemon_msgs = (MSGS *)malloc(sizeof(MSGS));
      memset(daemon_msgs, 0, sizeof(MSGS));
      for (i=1; i<=M_MAX; i++) {
	 add_msg_dest(daemon_msgs, MD_STDOUT, i, NULL, NULL);
	 add_msg_dest(daemon_msgs, MD_SYSLOG, i, NULL, NULL);
      }
      Dmsg1(050, "Create daemon global message resource 0x%x\n", daemon_msgs);
      return;
   }

   /*
    * Walk down the message resource chain duplicating it
    * for the current Job.
    */
   for (d=msg->dest_chain; d; d=d->next) {
      dnew = (DEST *)malloc(sizeof(DEST));
      memcpy(dnew, d, sizeof(DEST));
      dnew->next = temp_chain;
      dnew->fd = NULL;
      dnew->mail_filename = NULL;
      if (d->mail_cmd) {
	 dnew->mail_cmd = bstrdup(d->mail_cmd);
      }
      if (d->where) {
	 dnew->where = bstrdup(d->where);
      }
      temp_chain = dnew;
   }

   if (jcr) {
      jcr->jcr_msgs = (MSGS *)malloc(sizeof(MSGS));
      memset(jcr->jcr_msgs, 0, sizeof(MSGS));
      jcr->jcr_msgs->dest_chain = temp_chain;
      memcpy(jcr->jcr_msgs->send_msg, msg->send_msg, sizeof(msg->send_msg));
   } else {
      /* If we have default values, release them now */
      if (daemon_msgs) {
	 free_msgs_res(daemon_msgs);
      }
      daemon_msgs = (MSGS *)malloc(sizeof(MSGS));
      memset(daemon_msgs, 0, sizeof(MSGS));
      daemon_msgs->dest_chain = temp_chain;
      memcpy(daemon_msgs->send_msg, msg->send_msg, sizeof(msg->send_msg));
   }
   Dmsg2(050, "Copy message resource 0x%x to 0x%x\n", msg, temp_chain);

}

/* Initialize so that the console (User Agent) can
 * receive messages -- stored in a file.
 */
void init_console_msg(char *wd)
{
   int fd;

   bsnprintf(con_fname, sizeof(con_fname), "%s/%s.conmsg", wd, my_name);
   fd = open(con_fname, O_CREAT|O_RDWR|O_BINARY, 0600);
   if (fd == -1) {
      Emsg2(M_ERROR_TERM, 0, _("Could not open console message file %s: ERR=%s\n"),
	  con_fname, strerror(errno));
   }
   if (lseek(fd, 0, SEEK_END) > 0) {
      console_msg_pending = 1;
   }
   close(fd);
   con_fd = fopen(con_fname, "a+");
   if (!con_fd) {
      Emsg2(M_ERROR, 0, _("Could not open console message file %s: ERR=%s\n"),
	  con_fname, strerror(errno));
   }
   if (rwl_init(&con_lock) != 0) {
      Emsg1(M_ERROR_TERM, 0, _("Could not get con mutex: ERR=%s\n"), 
	 strerror(errno));
   }
}

/* 
 * Called only during parsing of the config file.
 *
 * Add a message destination. I.e. associate a message type with
 *  a destination (code).
 * Note, where in the case of dest_code FILE is a filename,
 *  but in the case of MAIL is a space separated list of
 *  email addresses, ...
 */
void add_msg_dest(MSGS *msg, int dest_code, int msg_type, char *where, char *mail_cmd)
{
   DEST *d; 
   /*
    * First search the existing chain and see if we
    * can simply add this msg_type to an existing entry.
    */
   for (d=msg->dest_chain; d; d=d->next) {
      if (dest_code == d->dest_code && ((where == NULL && d->where == NULL) ||
		     (strcmp(where, d->where) == 0))) {  
         Dmsg4(200, "Add to existing d=%x msgtype=%d destcode=%d where=%s\n", 
	     d, msg_type, dest_code, NPRT(where));
	 set_bit(msg_type, d->msg_types);
	 set_bit(msg_type, msg->send_msg);  /* set msg_type bit in our local */
	 return;
      }
   }
   /* Not found, create a new entry */
   d = (DEST *)malloc(sizeof(DEST));
   memset(d, 0, sizeof(DEST));
   d->next = msg->dest_chain;
   d->dest_code = dest_code;
   set_bit(msg_type, d->msg_types);	 /* set type bit in structure */
   set_bit(msg_type, msg->send_msg);	 /* set type bit in our local */
   if (where) {
      d->where = bstrdup(where);
   }
   if (mail_cmd) {
      d->mail_cmd = bstrdup(mail_cmd);
   }
   Dmsg5(200, "add new d=%x msgtype=%d destcode=%d where=%s mailcmd=%s\n", 
	  d, msg_type, dest_code, NPRT(where), NPRT(d->mail_cmd));
   msg->dest_chain = d;
}

/* 
 * Called only during parsing of the config file.
 *
 * Remove a message destination   
 */
void rem_msg_dest(MSGS *msg, int dest_code, int msg_type, char *where)
{
   DEST *d;

   for (d=msg->dest_chain; d; d=d->next) {
      Dmsg2(200, "Remove_msg_dest d=%x where=%s\n", d, NPRT(d->where));
      if (bit_is_set(msg_type, d->msg_types) && (dest_code == d->dest_code) &&
	  ((where == NULL && d->where == NULL) ||
		     (strcmp(where, d->where) == 0))) {  
         Dmsg3(200, "Found for remove d=%x msgtype=%d destcode=%d\n", 
	       d, msg_type, dest_code);
	 clear_bit(msg_type, d->msg_types);
         Dmsg0(200, "Return rem_msg_dest\n");
	 return;
      }
   }
}

static void make_unique_spool_filename(JCR *jcr, POOLMEM **name, int fd)
{
   Mmsg(name, "%s/%s.spool.%s.%d", working_directory, my_name,
      jcr->Job, fd);
}

int open_spool_file(JCR *jcr, BSOCK *bs)
{
    POOLMEM *name  = get_pool_memory(PM_MESSAGE);

    make_unique_spool_filename(jcr, &name, bs->fd);
    bs->spool_fd = fopen(mp_chr(name), "w+");
    if (!bs->spool_fd) {
       Jmsg(jcr, M_ERROR, 0, "fopen spool file %s failed: ERR=%s\n", name, strerror(errno));
       free_pool_memory(name);
       return 0;
    }
    free_pool_memory(name);
    return 1;
}

int close_spool_file(JCR *jcr, BSOCK *bs)
{
    POOLMEM *name  = get_pool_memory(PM_MESSAGE);

    make_unique_spool_filename(jcr, &name, bs->fd);
    fclose(bs->spool_fd);
    unlink(mp_chr(name));
    free_pool_memory(name);
    bs->spool_fd = NULL;
    bs->spool = 0;
    return 1;
}

/*
 * Create a unique filename for the mail command
 */
static void make_unique_mail_filename(JCR *jcr, POOLMEM **name, DEST *d)
{
   if (jcr) {
      Mmsg(name, "%s/%s.mail.%s.%d", working_directory, my_name,
		 jcr->Job, (int)d);
   } else {
      Mmsg(name, "%s/%s.mail.%s.%d", working_directory, my_name,
		 my_name, (int)d);
   }
   Dmsg1(200, "mailname=%s\n", *name);
}

/*
 * Open a mail pipe
 */
static BPIPE *open_mail_pipe(JCR *jcr, POOLMEM **cmd, DEST *d)
{
   BPIPE *bpipe;

   if (d->mail_cmd && jcr) {
      *cmd = edit_job_codes(jcr, *cmd, d->mail_cmd, d->where);
   } else {
      Mmsg(cmd, "mail -s \"Bacula Message\" %s", d->where);
   }
   fflush(stdout);

   if (!(bpipe = open_bpipe(*cmd, 120, "rw"))) {
      Jmsg(jcr, M_ERROR, 0, "open mail pipe %s failed: ERR=%s\n", 
	 *cmd, strerror(errno));
   } 
   return bpipe;
}

/* 
 * Close the messages for this Messages resource, which means to close
 *  any open files, and dispatch any pending email messages.
 */
void close_msg(JCR *jcr)
{
   MSGS *msgs;
   DEST *d;
   BPIPE *bpipe;
   POOLMEM *cmd, *line;
   int len, stat;
   
   Dmsg1(050, "Close_msg jcr=0x%x\n", jcr);

   if (jcr == NULL) {		     /* NULL -> global chain */
      msgs = daemon_msgs;
   } else {
      msgs = jcr->jcr_msgs;
      jcr->jcr_msgs = NULL;
   }
   if (msgs == NULL) {
      return;
   }
   Dmsg1(150, "===Begin close msg resource at 0x%x\n", msgs);
   cmd = get_pool_memory(PM_MESSAGE);
   for (d=msgs->dest_chain; d; ) {
      if (d->fd) {
	 switch (d->dest_code) {
	 case MD_FILE:
	 case MD_APPEND:
	    if (d->fd) {
	       fclose(d->fd);		 /* close open file descriptor */
	    }
	    break;
	 case MD_MAIL:
	 case MD_MAIL_ON_ERROR:
            Dmsg0(150, "Got MD_MAIL or MD_MAIL_ON_ERROR\n");
	    if (!d->fd) {
	       break;
	    }
	    if (d->dest_code == MD_MAIL_ON_ERROR && jcr &&
		jcr->JobStatus == JS_Terminated) {
	       goto rem_temp_file;
	    }
	    
	    if (!(bpipe=open_mail_pipe(jcr, &cmd, d))) {
               Pmsg0(000, "open mail pipe failed.\n");
	       goto rem_temp_file;
	    }
            Dmsg0(150, "Opened mail pipe\n");
	    len = d->max_len+10;
	    line = get_memory(len);
	    rewind(d->fd);
	    while (fgets(mp_chr(line), len, d->fd)) {
	       fputs(line, bpipe->wfd);
	    }
	    if (!close_wpipe(bpipe)) {	     /* close write pipe sending mail */
               Pmsg1(000, "close error: ERR=%s\n", strerror(errno));
	    }

	    /*
             * Since we are closing all messages, before "recursing"
	     * make sure we are not closing the daemon messages, otherwise
	     * kaboom.
	     */
	    if (msgs != daemon_msgs) {
	       /* read what mail prog returned -- should be nothing */
	       while (fgets(mp_chr(line), len, bpipe->rfd)) {
                  Jmsg1(jcr, M_INFO, 0, _("Mail prog: %s"), line);
	       }
	    }

	    stat = close_bpipe(bpipe);
	    if (stat != 0 && msgs != daemon_msgs) {
               Dmsg1(150, "Calling emsg. CMD=%s\n", cmd);
               Jmsg3(jcr, M_ERROR, 0, _("Mail program terminated in error. stat=%d\n"
                                        "CMD=%s\n"
                                        "ERR=%s\n"), stat, cmd, strerror(stat));
	    }
	    free_memory(line);
rem_temp_file:
	    /* Remove temp file */
	    fclose(d->fd);
	    unlink(mp_chr(d->mail_filename));
	    free_pool_memory(d->mail_filename);
	    d->mail_filename = NULL;
            Dmsg0(150, "end mail or mail on error\n");
	    break;
	 default:
	    break;
	 }
	 d->fd = NULL;
      }
      d = d->next;		      /* point to next buffer */
   }
   free_pool_memory(cmd);
   Dmsg0(150, "Done walking message chain.\n");
   if (jcr) {
      free_msgs_res(msgs);
      msgs = NULL;
   }
   Dmsg0(150, "===End close msg resource\n");
}

/*
 * Free memory associated with Messages resource  
 */
void free_msgs_res(MSGS *msgs)
{
   DEST *d, *old;

   /* Walk down the message chain releasing allocated buffers */
   for (d=msgs->dest_chain; d; ) {
      if (d->where) {
	 free(d->where);
      }
      if (d->mail_cmd) {
	 free(d->mail_cmd);
      }
      old = d;			      /* save pointer to release */
      d = d->next;		      /* point to next buffer */
      free(old);		      /* free the destination item */
   }
   msgs->dest_chain = NULL;
   free(msgs);			      /* free the head */
}


/* 
 * Terminate the message handler for good. 
 * Release the global destination chain.
 * 
 * Also, clean up a few other items (cons, exepath). Note,
 *   these really should be done elsewhere.
 */
void term_msg()
{
   Dmsg0(100, "Enter term_msg\n");
   close_msg(NULL);		      /* close global chain */
   free_msgs_res(daemon_msgs);	      /* free the resources */
   daemon_msgs = NULL;
   if (con_fd) {
      fflush(con_fd);
      fclose(con_fd);
      con_fd = NULL;
   }
   if (exepath) {
      free(exepath);
      exepath = NULL;
   }
   if (exename) {
      free(exename);
      exename = NULL;
   }
   if (trace_fd) {
      fclose(trace_fd);
      trace_fd = NULL;
   }
   term_last_jobs_list();
}



/*
 * Handle sending the message to the appropriate place
 */
void dispatch_message(JCR *jcr, int type, int level, char *msg)
{
    DEST *d;   
    char dt[MAX_TIME_LENGTH];
    POOLMEM *mcmd;
    int len;
    MSGS *msgs;
    BPIPE *bpipe;

    Dmsg2(800, "Enter dispatch_msg type=%d msg=%s\n", type, msg);

    if (type == M_ABORT || type == M_ERROR_TERM) {
       fputs(msg, stdout);	   /* print this here to INSURE that it is printed */
    }

    /* Now figure out where to send the message */
    msgs = NULL;
    if (jcr) {
       msgs = jcr->jcr_msgs;
    } 
    if (msgs == NULL) {
       msgs = daemon_msgs;
    }
    for (d=msgs->dest_chain; d; d=d->next) {
       if (bit_is_set(type, d->msg_types)) {
	  switch (d->dest_code) {
	     case MD_CONSOLE:
                Dmsg1(800, "CONSOLE for following msg: %s", msg);
		if (!con_fd) {
                   con_fd = fopen(con_fname, "a+");
                   Dmsg0(800, "Console file not open.\n");
		}
		if (con_fd) {
		   Pw(con_lock);      /* get write lock on console message file */
		   errno = 0;
		   bstrftime(dt, sizeof(dt), time(NULL));
		   len = strlen(dt);
                   dt[len++] = ' ';
		   fwrite(dt, len, 1, con_fd);
		   len = strlen(msg);
		   if (len > 0) {
		      fwrite(msg, len, 1, con_fd);
                      if (msg[len-1] != '\n') {
                         fwrite("\n", 2, 1, con_fd);
		      }
		   } else {
                      fwrite("\n", 2, 1, con_fd);
		   }
		   fflush(con_fd);
		   console_msg_pending = TRUE;
		   Vw(con_lock);
		}
		break;
	     case MD_SYSLOG:
                Dmsg1(800, "SYSLOG for collowing msg: %s\n", msg);
		/*
		 * We really should do an openlog() here.  
		 */
                syslog(LOG_DAEMON|LOG_ERR, "%s", msg);
		break;
	     case MD_OPERATOR:
                Dmsg1(800, "OPERATOR for collowing msg: %s\n", msg);
		mcmd = get_pool_memory(PM_MESSAGE);
		if ((bpipe=open_mail_pipe(jcr, &mcmd, d))) {
		   int stat;
		   fputs(msg, bpipe->wfd);
		   /* Messages to the operator go one at a time */
		   stat = close_bpipe(bpipe);
		   if (stat != 0) {
                      Jmsg2(jcr, M_ERROR, 0, _("Operator mail program terminated in error.\n"
                            "CMD=%s\n"
                            "ERR=%s\n"), mcmd, strerror(stat));
		   }
		}
		free_pool_memory(mcmd);
		break;
	     case MD_MAIL:
	     case MD_MAIL_ON_ERROR:
                Dmsg1(800, "MAIL for following msg: %s", msg);
		if (!d->fd) {
		   POOLMEM *name = get_pool_memory(PM_MESSAGE);
		   make_unique_mail_filename(jcr, &mp_chr(name), d);
                   d->fd = fopen(mp_chr(name), "w+");
		   if (!d->fd) {
		      d->fd = stdout;
                      Jmsg2(jcr, M_ERROR, 0, "fopen %s failed: ERR=%s\n", name, strerror(errno));
		      d->fd = NULL;
		      free_pool_memory(name);
		      break;
		   }
		   d->mail_filename = name;
		}
		len = strlen(msg);
		if (len > d->max_len) {
		   d->max_len = len;	  /* keep max line length */
		}
		fputs(msg, d->fd);
		break;
	     case MD_FILE:
                Dmsg1(800, "FILE for following msg: %s", msg);
		if (!d->fd) {
                   d->fd = fopen(d->where, "w+");
		   if (!d->fd) {
		      d->fd = stdout;
                      Jmsg2(jcr, M_ERROR, 0, "fopen %s failed: ERR=%s\n", d->where, strerror(errno));
		      d->fd = NULL;
		      break;
		   }
		}
		fputs(msg, d->fd);
		break;
	     case MD_APPEND:
                Dmsg1(800, "APPEND for following msg: %s", msg);
		if (!d->fd) {
                   d->fd = fopen(d->where, "a");
		   if (!d->fd) {
		      d->fd = stdout;
                      Jmsg2(jcr, M_ERROR, 0, "fopen %s failed: ERR=%s\n", d->where, strerror(errno));
		      d->fd = NULL;
		      break;
		   }
		}
		fputs(msg, d->fd);
		break;
	     case MD_DIRECTOR:
                Dmsg1(800, "DIRECTOR for following msg: %s", msg);
		if (jcr && jcr->dir_bsock && !jcr->dir_bsock->errors) {
                   bnet_fsend(jcr->dir_bsock, "Jmsg Job=%s type=%d level=%d %s", 
		      jcr->Job, type, level, msg);
		}
		break;
	     case MD_STDOUT:
                Dmsg1(800, "STDOUT for following msg: %s", msg);
		if (type != M_ABORT && type != M_ERROR_TERM) { /* already printed */
		   fputs(msg, stdout);
		}
		break;
	     case MD_STDERR:
                Dmsg1(800, "STDERR for following msg: %s", msg);
		fputs(msg, stderr);
		break;
	     default:
		break;
	  }
       }
    }
}


/*********************************************************************
 *
 *  This subroutine prints a debug message if the level number
 *  is less than or equal the debug_level. File and line numbers
 *  are included for more detail if desired, but not currently
 *  printed.
 *  
 *  If the level is negative, the details of file and line number
 *  are not printed.
 */
void 
d_msg(const char *file, int line, int level, const char *fmt,...)
{
    char      buf[5000];
    int       len;
    va_list   arg_ptr;
    int       details = TRUE;

    if (level < 0) {
       details = FALSE;
       level = -level;
    }

    if (level <= debug_level) {
#ifdef HAVE_WIN32
#define SEND_DMSG_TO_FILE
#endif
#ifdef SEND_DMSG_TO_FILE
       if (!trace_fd) {
          bsnprintf(buf, sizeof(buf), "%s/bacula.trace", working_directory ? working_directory : ".");
          trace_fd = fopen(buf, "a+");
       }
#endif
#ifdef FULL_LOCATION
       if (details) {
	  /* visual studio passes the whole path to the file as well
	   * which makes for very long lines
	   */
          char *f = strrchr(file, '\\');
	  if (f) file = f + 1;
          len = bsnprintf(buf, sizeof(buf), "%s: %s:%d ", my_name, file, line);
       } else {
	  len = 0;
       }
#else
       len = 0;
#endif
       va_start(arg_ptr, fmt);
       bvsnprintf(buf+len, sizeof(buf)-len, (char *)fmt, arg_ptr);
       va_end(arg_ptr);

#ifdef SEND_DMSG_TO_FILE
       if (trace_fd) {
	   fputs(buf, trace_fd);
	   fflush(trace_fd);
       }
#else
       fputs(buf, stdout);
#endif
    }
}


/*********************************************************************
 *
 *  This subroutine prints a message regardless of the debug level
 *  
 *  If the level is negative, the details of file and line number
 *  are not printed.
 */
void 
p_msg(const char *file, int line, int level, const char *fmt,...)
{
    char      buf[5000];
    int       len;
    va_list   arg_ptr;

#ifdef FULL_LOCATION
    if (level >= 0) {
       len = bsnprintf(buf, sizeof(buf), "%s: %s:%d ", my_name, file, line);
    } else {
       len = 0;
    }
#else
    len = 0;
#endif
    va_start(arg_ptr, fmt);
    bvsnprintf(buf+len, sizeof(buf)-len, (char *)fmt, arg_ptr);
    va_end(arg_ptr);
    fputs(buf, stdout);
}


/*********************************************************************
 *
 *  subroutine writes a debug message to the trace file if the level number
 *  is less than or equal the debug_level. File and line numbers
 *  are included for more detail if desired, but not currently
 *  printed.
 *  
 *  If the level is negative, the details of file and line number
 *  are not printed.
 */
void 
t_msg(const char *file, int line, int level, const char *fmt,...)
{
    char      buf[5000];
    int       len;
    va_list   arg_ptr;
    int       details = TRUE;

    if (level < 0) {
       details = FALSE;
       level = -level;
    }

    if (level <= debug_level) {
       if (!trace_fd) {
          bsnprintf(buf, sizeof(buf), "%s/bacula.trace", working_directory);
          trace_fd = fopen(buf, "a+");
       }
    
#ifdef FULL_LOCATION
       if (details) {
          len = bsnprintf(buf, sizeof(buf), "%s: %s:%d ", my_name, file, line);
       } else {
	  len = 0;
       }
#else
       len = 0;
#endif
       va_start(arg_ptr, fmt);
       bvsnprintf(buf+len, sizeof(buf)-len, (char *)fmt, arg_ptr);
       va_end(arg_ptr);
       if (trace_fd != NULL) {
	   fputs(buf, trace_fd);
	   fflush(trace_fd);
       }
   }
}



/* *********************************************************
 *
 * print an error message
 *
 */
void 
e_msg(const char *file, int line, int type, int level, const char *fmt,...)
{
    char     buf[5000];
    va_list   arg_ptr;
    int len;

    /* 
     * Check if we have a message destination defined.	
     * We always report M_ABORT and M_ERROR_TERM 
     */
    if (!daemon_msgs || ((type != M_ABORT && type != M_ERROR_TERM) && 
			 !bit_is_set(type, daemon_msgs->send_msg))) {
       return;			      /* no destination */
    }
    switch (type) {
    case M_ABORT:
       len = bsnprintf(buf, sizeof(buf), "%s: ABORTING due to ERROR in %s:%d\n", 
	       my_name, file, line);
       break;
    case M_ERROR_TERM:
       len = bsnprintf(buf, sizeof(buf), "%s: ERROR TERMINATION at %s:%d\n", 
	       my_name, file, line);
       break;
    case M_FATAL:
       if (level == -1) 	   /* skip details */
          len = bsnprintf(buf, sizeof(buf), "%s: Fatal Error because: ", my_name);
       else
          len = bsnprintf(buf, sizeof(buf), "%s: Fatal Error at %s:%d because:\n", my_name, file, line);
       break;
    case M_ERROR:
       if (level == -1) 	   /* skip details */
          len = bsnprintf(buf, sizeof(buf), "%s: Error: ", my_name);
       else
          len = bsnprintf(buf, sizeof(buf), "%s: Error in %s:%d ", my_name, file, line);
       break;
    case M_WARNING:
       len = bsnprintf(buf, sizeof(buf), "%s: Warning: ", my_name);
       break;
    case M_SECURITY:
       len = bsnprintf(buf, sizeof(buf), "%s: Security violation: ", my_name);
       break;
    default:
       len = bsnprintf(buf, sizeof(buf), "%s: ", my_name);
       break;
    }

    va_start(arg_ptr, fmt);
    bvsnprintf(buf+len, sizeof(buf)-len, (char *)fmt, arg_ptr);
    va_end(arg_ptr);

    dispatch_message(NULL, type, level, buf);

    if (type == M_ABORT) {
       char *p = 0;
       p[0] = 0;		      /* generate segmentation violation */
    }
    if (type == M_ERROR_TERM) {
       exit(1);
    }
}

/* *********************************************************
 *
 * Generate a Job message
 *
 */
void 
Jmsg(JCR *jcr, int type, int level, const char *fmt,...)
{
    char     rbuf[5000];
    va_list   arg_ptr;
    int len;
    MSGS *msgs;
    char *job;

    
    Dmsg1(800, "Enter Jmsg type=%d\n", type);

    /* Special case for the console, which has a dir_bsock and JobId==0,
     *	in that case, we send the message directly back to the
     *	dir_bsock.  
     */
    if (jcr && jcr->JobId == 0 && jcr->dir_bsock) {
       BSOCK *dir = jcr->dir_bsock;
       va_start(arg_ptr, fmt);
       dir->msglen = bvsnprintf(mp_chr(dir->msg), sizeof_pool_memory(dir->msg), 
				fmt, arg_ptr);
       va_end(arg_ptr);
       bnet_send(jcr->dir_bsock);
       return;
    }

    msgs = NULL;
    job = NULL;
    if (jcr) {
       msgs = jcr->jcr_msgs;
       job = jcr->Job;
    } 
    if (!msgs) {
       msgs = daemon_msgs;	      /* if no jcr, we use daemon handler */
    }
    if (!job) {
       job = "";                      /* Set null job name if none */
    }

    /* 
     * Check if we have a message destination defined.	
     * We always report M_ABORT and M_ERROR_TERM 
     */
    if (msgs && (type != M_ABORT && type != M_ERROR_TERM) &&
	 !bit_is_set(type, msgs->send_msg)) {
       return;			      /* no destination */
    }
    switch (type) {
    case M_ABORT:
       len = bsnprintf(rbuf, sizeof(rbuf), "%s ABORTING due to ERROR\n", my_name);
       break;
    case M_ERROR_TERM:
       len = bsnprintf(rbuf, sizeof(rbuf), "%s ERROR TERMINATION\n", my_name);
       break;
    case M_FATAL:
       len = bsnprintf(rbuf, sizeof(rbuf), "%s: %s Fatal error: ", my_name, job);
       if (jcr) {
	  set_jcr_job_status(jcr, JS_FatalError);
       }
       break;
    case M_ERROR:
       len = bsnprintf(rbuf, sizeof(rbuf), "%s: %s Error: ", my_name, job);
       if (jcr) {
	  jcr->Errors++;
       }
       break;
    case M_WARNING:
       len = bsnprintf(rbuf, sizeof(rbuf), "%s: %s Warning: ", my_name, job);
       break;
    case M_SECURITY:
       len = bsnprintf(rbuf, sizeof(rbuf), "%s: %s Security violation: ", my_name, job);
       break;
    default:
       len = bsnprintf(rbuf, sizeof(rbuf), "%s: ", my_name);
       break;
    }

    va_start(arg_ptr, fmt);
    bvsnprintf(rbuf+len,  sizeof(rbuf)-len, fmt, arg_ptr);
    va_end(arg_ptr);

    dispatch_message(jcr, type, level, rbuf);

    if (type == M_ABORT){
       char *p = 0;
       p[0] = 0;		      /* generate segmentation violation */
    }
    if (type == M_ERROR_TERM) {
       exit(1);
    }
}

/*
 * If we come here, prefix the message with the file:line-number,
 *  then pass it on to the normal Jmsg routine.
 */
void j_msg(const char *file, int line, JCR *jcr, int type, int level, const char *fmt,...)
{
   va_list   arg_ptr;
   int i, len, maxlen;
   POOLMEM *pool_buf;

   pool_buf = get_pool_memory(PM_EMSG);
   i = Mmsg(&pool_buf, "%s:%d ", file, line);

again:
   maxlen = sizeof_pool_memory(pool_buf) - i - 1; 
   va_start(arg_ptr, fmt);
   len = bvsnprintf(pool_buf+i, maxlen, fmt, arg_ptr);
   va_end(arg_ptr);
   if (len < 0 || len >= maxlen) {
      pool_buf = realloc_pool_memory(pool_buf, maxlen + i + maxlen/2);
      goto again;
   }

   Jmsg(jcr, type, level, "%s", pool_buf);
   free_memory(pool_buf);
}


/*
 * Edit a message into a Pool memory buffer, with file:lineno
 */
int m_msg(const char *file, int line, POOLMEM **pool_buf, const char *fmt, ...)
{
   va_list   arg_ptr;
   int i, len, maxlen;

   i = sprintf(mp_chr(*pool_buf), "%s:%d ", file, line);

again:
   maxlen = sizeof_pool_memory(*pool_buf) - i - 1; 
   va_start(arg_ptr, fmt);
   len = bvsnprintf(*pool_buf+i, maxlen, fmt, arg_ptr);
   va_end(arg_ptr);
   if (len < 0 || len >= maxlen) {
      *pool_buf = realloc_pool_memory(*pool_buf, maxlen + i + maxlen/2);
      goto again;
   }
   return len;
}

/*
 * Edit a message into a Pool Memory buffer NO file:lineno
 *  Returns: string length of what was edited.
 */
int Mmsg(POOLMEM **pool_buf, const char *fmt, ...)
{
   va_list   arg_ptr;
   int len, maxlen;

again:
   maxlen = sizeof_pool_memory(*pool_buf) - 1; 
   va_start(arg_ptr, fmt);
   len = bvsnprintf(*pool_buf, maxlen, fmt, arg_ptr);
   va_end(arg_ptr);
   if (len < 0 || len >= maxlen) {
      *pool_buf = realloc_pool_memory(*pool_buf, maxlen + maxlen/2);
      goto again;
   }
   return len;
}

static pthread_mutex_t msg_queue_mutex = PTHREAD_MUTEX_INITIALIZER;

/*
 * We queue messages rather than print them directly. This
 *  is generally used in low level routines (msg handler, bnet)
 *  to prevent recursion.
 */
void Qmsg(JCR *jcr, int type, int level, const char *fmt,...)
{
   va_list   arg_ptr;
   int len, maxlen;
   POOLMEM *pool_buf;
   MQUEUE_ITEM *item;

   if (jcr->dequeuing) {	      /* do not allow recursion */
      return;
   }
   pool_buf = get_pool_memory(PM_EMSG);

again:
   maxlen = sizeof_pool_memory(pool_buf) - 1; 
   va_start(arg_ptr, fmt);
   len = bvsnprintf(pool_buf, maxlen, fmt, arg_ptr);
   va_end(arg_ptr);
   if (len < 0 || len >= maxlen) {
      pool_buf = realloc_pool_memory(pool_buf, maxlen + maxlen/2);
      goto again;
   }
   P(msg_queue_mutex);
   item = (MQUEUE_ITEM *)malloc(sizeof(MQUEUE_ITEM) + strlen(pool_buf) + 1);
   item->type = type;
   item->level = level;
   strcpy(item->msg, pool_buf);  
   jcr->msg_queue->append(item);
   V(msg_queue_mutex);
   free_memory(pool_buf);
}

/*
 * Dequeue messages 
 */
void dequeue_messages(JCR *jcr)
{
   MQUEUE_ITEM *item;
   P(msg_queue_mutex);
   jcr->dequeuing = true;
   foreach_dlist(item, jcr->msg_queue) {
      Jmsg(jcr, item->type, item->level, "%s", item->msg);
   }
   jcr->msg_queue->destroy();
   jcr->dequeuing = false;
   V(msg_queue_mutex);
}


/*
 * If we come here, prefix the message with the file:line-number,
 *  then pass it on to the normal Qmsg routine.
 */
void q_msg(const char *file, int line, JCR *jcr, int type, int level, const char *fmt,...)
{
   va_list   arg_ptr;
   int i, len, maxlen;
   POOLMEM *pool_buf;

   pool_buf = get_pool_memory(PM_EMSG);
   i = Mmsg(&pool_buf, "%s:%d ", file, line);

again:
   maxlen = sizeof_pool_memory(pool_buf) - i - 1; 
   va_start(arg_ptr, fmt);
   len = bvsnprintf(pool_buf+i, maxlen, fmt, arg_ptr);
   va_end(arg_ptr);
   if (len < 0 || len >= maxlen) {
      pool_buf = realloc_pool_memory(pool_buf, maxlen + i + maxlen/2);
      goto again;
   }

   Qmsg(jcr, type, level, "%s", pool_buf);
   free_memory(pool_buf);
}
