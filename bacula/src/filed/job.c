/*
 *  Bacula File Daemon Job processing
 *
 *    Kern Sibbald, October MM
 *
 *   Version $Id$
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

extern char my_name[];
			
/* Imported functions */
extern int status_cmd(JCR *jcr);
				   
/* Forward referenced functions */
static int backup_cmd(JCR *jcr);
static int cancel_cmd(JCR *jcr);
static int setdebug_cmd(JCR *jcr);
static int estimate_cmd(JCR *jcr);
static int exclude_cmd(JCR *jcr);
static int hello_cmd(JCR *jcr);
static int job_cmd(JCR *jcr);
static int include_cmd(JCR *jcr);
static int level_cmd(JCR *jcr);
static int verify_cmd(JCR *jcr);
static int restore_cmd(JCR *jcr);
static int storage_cmd(JCR *jcr);
static int session_cmd(JCR *jcr);
static int response(BSOCK *sd, char *resp, char *cmd);
static void filed_free_jcr(JCR *jcr);
static int open_sd_read_session(JCR *jcr);


/* Exported functions */

struct s_cmds {
   char *cmd;
   int (*func)(JCR *);
};

/*  
 * The following are the recognized commands from the Director. 
 */
static struct s_cmds cmds[] = {
   {"backup",   backup_cmd},
   {"cancel",   cancel_cmd},
   {"setdebug=", setdebug_cmd},
   {"estimate", estimate_cmd},
   {"exclude",  exclude_cmd},
   {"Hello",    hello_cmd},
   {"include",  include_cmd},
   {"JobId=",   job_cmd},
   {"level = ", level_cmd},
   {"restore",  restore_cmd},
   {"session",  session_cmd},
   {"status",   status_cmd},
   {"storage ", storage_cmd},
   {"verify",   verify_cmd},
   {NULL,	NULL}		       /* list terminator */
};

/* Commands received from director that need scanning */
static char jobcmd[]     = "JobId=%d Job=%127s SDid=%d SDtime=%d Authorization=%100s";
static char storaddr[]   = "storage address=%s port=%d\n";
static char sessioncmd[] = "session %s %ld %ld %ld %ld %ld %ld\n";
static char restorecmd[] = "restore where=%s\n";
static char verifycmd[]  = "verify level=%20s\n";

/* Responses sent to Director */
static char errmsg[]       = "2999 Invalid command\n";
static char no_auth[]      = "2998 No Authorization\n";
static char OKinc[]        = "2000 OK include\n";
static char OKest[]        = "2000 OK estimate files=%ld bytes=%ld\n";
static char OKexc[]        = "2000 OK exclude\n";
static char OKlevel[]      = "2000 OK level\n";
static char OKbackup[]     = "2000 OK backup\n";
static char OKverify[]     = "2000 OK verify\n";
static char OKrestore[]    = "2000 OK restore\n";
static char OKsession[]    = "2000 OK session\n";
static char OKstore[]      = "2000 OK storage\n";
static char OKjob[]        = "2000 OK Job\n";
static char OKsetdebug[]   = "2000 OK setdebug=%d\n";
static char BADjob[]       = "2901 Bad Job\n";

/* Responses received from Storage Daemon */
static char OK_end[]       = "3000 OK end\n";
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
 */
void *handle_client_request(void *dirp)
{
   int i, found, quit;
   JCR *jcr;
   BSOCK *dir = (BSOCK *) dirp;

   jcr = new_jcr(sizeof(JCR), filed_free_jcr); /* create JCR */
   jcr->dir_bsock = dir;
   jcr->ff = init_find_files();
   jcr->start_time = time(NULL);
   jcr->last_fname = get_pool_memory(PM_FNAME);
   jcr->client_name = get_memory(strlen(my_name) + 1);
   strcpy(jcr->client_name, my_name);

   /**********FIXME******* add command handler error code */

   for (quit=0; !quit;) {

      /* Read command */
      if (bnet_recv(dir) <= 0) {
	 break; 		      /* connection terminated */
      }
      dir->msg[dir->msglen] = 0;
      Dmsg1(9, "<dird: %s", dir->msg);
      found = FALSE;
      for (i=0; cmds[i].cmd; i++) {
	 if (strncmp(cmds[i].cmd, dir->msg, strlen(cmds[i].cmd)) == 0) {
	    if (!jcr->authenticated && cmds[i].func != hello_cmd) {
	       bnet_fsend(dir, no_auth);
	       break;
	    }
	    if (!cmds[i].func(jcr)) {	 /* do command */
	       quit = TRUE;		 /* error, get out */
               Dmsg0(20, "Command error\n");
	    }
	    found = TRUE;	     /* indicate command found */
	    break;
	 }
      }
      if (!found) {		      /* command not found */
	 bnet_fsend(dir, errmsg);
	 quit = TRUE;
	 break;
      }
   }
   Dmsg0(20, "Calling term_find_files\n");
   term_find_files(jcr->ff);
   Dmsg0(20, "Done with term_find_files\n");
   free_jcr(jcr);		      /* destroy JCR record */
   Dmsg0(20, "Done with free_jcr\n");
   return NULL;
}

/*
 * Hello from Director he must identify himself and provide his 
 *  password.
 */
static int hello_cmd(JCR *jcr)
{
   Dmsg0(20, "Calling Authenticate\n");
   if (!authenticate_director(jcr)) {
      return 0;
   }
   Dmsg0(20, "OK Authenticate\n");
   jcr->authenticated = TRUE;
   return 1;
}

/*
 * Cancel a Job
 */
static int cancel_cmd(JCR *jcr)
{
   BSOCK *dir = jcr->dir_bsock;
   char Job[MAX_NAME_LENGTH];
   JCR *cjcr;

   if (sscanf(dir->msg, "cancel Job=%127s", Job) == 1) {
      if (!(cjcr=get_jcr_by_full_name(Job))) {
         bnet_fsend(dir, "2901 Job %s not found.\n", Job);
      } else {
	 cjcr->JobStatus = JS_Cancelled;
	 free_jcr(cjcr);
         bnet_fsend(dir, "2001 Job %s marked to be cancelled.\n", Job);
      }
   } else {
      bnet_fsend(dir, "2902 Error scanning cancel command.\n");
   }
   bnet_sig(dir, BNET_EOF);
   return 1;
}


/*
 * Set debug level as requested by the Director
 *
 */
static int setdebug_cmd(JCR *jcr)
{
   BSOCK *dir = jcr->dir_bsock;
   int level;

   Dmsg1(10, "setdebug_cmd: %s", dir->msg);
   if (sscanf(dir->msg, "setdebug=%d", &level) != 1 || level < 0) {
      bnet_fsend(dir, "2991 Bad setdebug command: %s\n", dir->msg);
      return 0;   
   }
   debug_level = level;
   return bnet_fsend(dir, OKsetdebug, level);
}


static int estimate_cmd(JCR *jcr)
{
   BSOCK *dir = jcr->dir_bsock;
   make_estimate(jcr);
   return bnet_fsend(dir, OKest, jcr->JobFiles, jcr->JobBytes);
}

/*
 * Get JobId and Storage Daemon Authorization key from Director
 */
static int job_cmd(JCR *jcr)
{
   BSOCK *dir = jcr->dir_bsock;
   char *sd_auth_key;

   sd_auth_key = (char *) get_memory(dir->msglen);
   if (sscanf(dir->msg, jobcmd,  &jcr->JobId, jcr->Job,  
	      &jcr->VolSessionId, &jcr->VolSessionTime,
	      sd_auth_key) != 5) {
      bnet_fsend(dir, BADjob);
      Emsg1(M_FATAL, 0, _("Bad Job Command: %s\n"), dir->msg);
      free_pool_memory(sd_auth_key);
      return 0;
   }
   jcr->sd_auth_key = bstrdup(sd_auth_key);
   free_pool_memory(sd_auth_key);
   Dmsg2(20, "JobId=%d Auth=%s\n", jcr->JobId, jcr->sd_auth_key);
   return bnet_fsend(dir, OKjob);
}

/* 
 * 
 * Get list of files/directories to include from Director
 *
 */
static int include_cmd(JCR *jcr)
{
   BSOCK *dir = jcr->dir_bsock;

   while (bnet_recv(dir) > 0) {
       dir->msg[dir->msglen] = 0;
       strip_trailing_junk(dir->msg);
       Dmsg1(10, "filed<dird: include file %s\n", dir->msg);
       add_fname_to_include_list(jcr->ff, 1, dir->msg);
   }

   return bnet_fsend(dir, OKinc);
}

/*
 * Get list of files to exclude from Director
 *
 */
static int exclude_cmd(JCR *jcr)
{
   BSOCK *dir = jcr->dir_bsock;
   char *p;  

   while (bnet_recv(dir) > 0) {
       dir->msg[dir->msglen] = 0;
       strip_trailing_junk(dir->msg);
       /* Skip leading options */
       for (p=dir->msg; *p && *p != ' '; p++)
	  { }
       /* Skip spaces */
       for ( ; *p && *p == ' '; p++)
	  { }
       add_fname_to_exclude_list(jcr->ff, p);
       Dmsg1(10, "<dird: exclude file %s\n", dir->msg);
   }

   return bnet_fsend(dir, OKexc);
}

/*
 * Get backup level from Director
 *
 */
static int level_cmd(JCR *jcr)
{
   BSOCK *dir = jcr->dir_bsock;
   char *level;
   struct tm tm;
   time_t mtime;

   level = (char *) get_memory(dir->msglen);
   Dmsg1(10, "level_cmd: %s", dir->msg);
   if (sscanf(dir->msg, "level = %s ", level) != 1) {
      Jmsg1(jcr, M_FATAL, 0, _("Bad level command: %s\n"), dir->msg);
      free_memory(level);
      return 0;
   }
   /*
    * Full backup requested
    */
   if (strcmp(level, "full") == 0) {
      jcr->save_level = L_FULL;
   /* 
    * Backup requested since <date> <time>
    *  This form is also used for incremental and differential
    */
   } else if (strcmp(level, "since") == 0) {
      jcr->save_level = L_SINCE;
      if (sscanf(dir->msg, "level = since %d-%d-%d %d:%d:%d", 
		 &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
		 &tm.tm_hour, &tm.tm_min, &tm.tm_sec) != 6) {
         Jmsg1(jcr, M_FATAL, 0, "Bad scan of date/time: %s\n", dir->msg);
	 free_memory(level);
	 return 0;
      }
      tm.tm_year -= 1900;
      tm.tm_mon  -= 1;
      tm.tm_wday = tm.tm_yday = 0;		
      tm.tm_isdst = -1;
      mtime = mktime(&tm);
      Dmsg1(90, "Got since time: %s", ctime(&mtime));
      jcr->incremental = 1;
      jcr->mtime = mtime;
   } else {
      Jmsg1(jcr, M_FATAL, 0, "Unknown backup level: %s\n", level);
      free_memory(level);
      return 0;
   }
   free_memory(level);
   return bnet_fsend(dir, OKlevel);
}

/*
 * Get session parameters from Director -- this is for a Restore command
 */
static int session_cmd(JCR *jcr)
{
   BSOCK *dir = jcr->dir_bsock;

   if (sscanf(dir->msg, sessioncmd, jcr->VolumeName,
	      &jcr->VolSessionId, &jcr->VolSessionTime,
	      &jcr->StartFile, &jcr->EndFile, 
	      &jcr->StartBlock, &jcr->EndBlock) != 7) {
      Emsg1(M_FATAL, 0, "Bad session command: %s", dir->msg);
      return 0;
   }

   return bnet_fsend(dir, OKsession);
}

/*
 * Get address of storage daemon from Director
 *
 */
static int storage_cmd(JCR *jcr)
{
   int stored_port;		   /* storage daemon port */
   BSOCK *dir = jcr->dir_bsock;
   BSOCK *sd;			      /* storage daemon bsock */

   if (sscanf(dir->msg, storaddr, &jcr->stored_addr, &stored_port) != 2) {
      Emsg1(M_FATAL, 0, _("Bad storage command: %s\n"), dir->msg);
      return 0;
   }
   Dmsg2(30, "Got storage: %s:%d\n", jcr->stored_addr, stored_port);
   /* Open command communications with Storage daemon */
   /* Try to connect for 1 hour at 10 second intervals */
   sd = bnet_connect(jcr, 10, 3600, _("Storage daemon"), 
		     jcr->stored_addr, NULL, stored_port, 1);
   if (sd == NULL) {
      Jmsg2(jcr, M_FATAL, 0, _("Failed to connect to Storage daemon: %s:%d\n"),
	  jcr->stored_addr, stored_port);
      return 0;
   }

   jcr->store_bsock = sd;

   bnet_fsend(sd, "Hello Start Job %s\n", jcr->Job);
   if (!authenticate_storagedaemon(jcr)) {
      Jmsg(jcr, M_FATAL, 0, _("Failed to authenticate Storage daemon.\n"));
      return 0;
   }

   /* Send OK to Director */
   return bnet_fsend(dir, OKstore);
}


/*  
 * Do a backup. For now, we handle only Full and Incremental.
 */
static int backup_cmd(JCR *jcr)
{ 
   int data_port;
   BSOCK *dir = jcr->dir_bsock;
   BSOCK *sd = jcr->store_bsock;
   int len;

   jcr->JobStatus = JS_Blocked;
   jcr->JobType = JT_BACKUP;
   Dmsg1(100, "begin backup ff=%p\n", jcr->ff);

   if (sd == NULL) {
      Emsg0(M_FATAL, 0, _("Cannot contact Storage daemon\n"));
      jcr->JobStatus = JS_ErrorTerminated;
      goto cleanup;
   }

   bnet_fsend(dir, OKbackup);
   Dmsg1(10, "bfiled>dird: %s", dir->msg);

   /* 
    * Send Append Open Session to Storage daemon
    */
   bnet_fsend(sd, append_open);
   Dmsg1(10, ">stored: %s", sd->msg);
   /* 
    * Expect to receive back the Ticket number
    */
   if (bnet_recv(sd) > 0) {
      Dmsg1(10, "<stored: %s", sd->msg);
      if (sscanf(sd->msg, OK_open, &jcr->Ticket) != 1) {
         Emsg1(M_FATAL, 0, _("Bad response to append open: %s\n"), sd->msg);
	 jcr->JobStatus = JS_ErrorTerminated;
	 goto cleanup;
      }
      Dmsg1(10, "Got Ticket=%d\n", jcr->Ticket);
   } else {
      Emsg0(M_FATAL, 0, _("Bad response from stored to open command\n"));
      jcr->JobStatus = JS_ErrorTerminated;
      goto cleanup;
   }

   /* 
    * Send Append data command to Storage daemon
    */
   bnet_fsend(sd, append_data, jcr->Ticket);
   Dmsg1(10, ">stored: %s", sd->msg);

   /* 
    * Expect to get OK data 
    */
   Dmsg1(10, "<stored: %s", sd->msg);
   if (!response(sd, OK_data, "Append Data")) {
      jcr->JobStatus = JS_ErrorTerminated;
      goto cleanup;
   }
      
   /*
    * Send Files to Storage daemon
    */
   Dmsg1(100, "begin blast ff=%p\n", jcr->ff);
   if (!blast_data_to_storage_daemon(jcr, NULL, data_port)) {
      jcr->JobStatus = JS_ErrorTerminated;
   } else {
      jcr->JobStatus = JS_Terminated;
      /* 
       * Expect to get response to append_data from Storage daemon
       */
      if (!response(sd, OK_append, "Append Data")) {
	 jcr->JobStatus = JS_ErrorTerminated;
	 goto cleanup;
      }
     
      /* 
       * Send Append End Data to Storage daemon
       */
      bnet_fsend(sd, append_end, jcr->Ticket);
      /* Get end OK */
      if (!response(sd, OK_end, "Append End")) {
	 jcr->JobStatus = JS_ErrorTerminated;
	 goto cleanup;
      }

      /*
       * Send Append Close to Storage daemon
       */
      bnet_fsend(sd, append_close, jcr->Ticket);
      while ((len = bnet_recv(sd)) > 0) {
	  /* discard anything else returned from SD */
      }
      if (len < 0) {
         Emsg2(M_FATAL, 0, _("<stored: net_recv len=%d: ERR=%s\n"), len, bnet_strerror(sd));
	 jcr->JobStatus = JS_ErrorTerminated;
      }
   }

cleanup:

   /* Inform Storage daemon that we are done */
   if (sd) {
      bnet_sig(sd, BNET_EOF);
   }

   /* Inform Director that we are done */
   bnet_sig(dir, BNET_EOF);

   return jcr->JobStatus == JS_Terminated;
}

/*  
 * Do a Verify for Director
 *
 */
static int verify_cmd(JCR *jcr)
{ 
   BSOCK *dir = jcr->dir_bsock;
   char level[100];

   jcr->JobType = JT_VERIFY;
   if (sscanf(dir->msg, verifycmd, level) != 1) {
      bnet_fsend(dir, "2994 Bad verify command: %s\n", dir->msg);
      return 0;   
   }
   if (strcasecmp(level, "init") == 0) {
      jcr->JobLevel = L_VERIFY_INIT;
   } else if (strcasecmp(level, "catalog") == 0){
      jcr->JobLevel = L_VERIFY_CATALOG;
   } else if (strcasecmp(level, "volume") == 0){
      jcr->JobLevel = L_VERIFY_VOLUME;
   } else if (strcasecmp(level, "data") == 0){
      jcr->JobLevel = L_VERIFY_DATA;
   } else {   
      bnet_fsend(dir, "2994 Bad verify command: %s\n", dir->msg);
      return 0;   
   }

   bnet_fsend(dir, OKverify);
   Dmsg1(10, "bfiled>dird: %s", dir->msg);

   do_verify(jcr);

   /* Inform Director that we are done */
   return bnet_sig(dir, BNET_EOF);
}

/*  
 * Do a Restore for Director
 *
 */
static int restore_cmd(JCR *jcr)
{ 
   BSOCK *dir = jcr->dir_bsock;
   BSOCK *sd = jcr->store_bsock;
   POOLMEM *where;

   /*
    * Scan WHERE (base directory for restore) from command
    */
   Dmsg0(50, "restore command\n");
   /* Pickup where string */
   where = get_memory(dir->msglen+1);
   *where = 0;
   sscanf(dir->msg, restorecmd, where);
   Dmsg1(50, "Got where=%s\n", where);
   jcr->where = where;

   bnet_fsend(dir, OKrestore);
   Dmsg1(10, "bfiled>dird: %s", dir->msg);

   jcr->JobType = JT_RESTORE;
   jcr->JobStatus = JS_Blocked;

   if (!open_sd_read_session(jcr)) {
      return 0;
   }

   /* 
    * Do restore of files and data
    */
   do_restore(jcr);

   /* 
    * Send Close session command to Storage daemon
    */
   bnet_fsend(sd, read_close, jcr->Ticket);
   Dmsg1(30, "bfiled>stored: %s", sd->msg);

   /* ****FIXME**** check response */
   bnet_recv(sd);		      /* get OK */

   /* Inform Storage daemon that we are done */
   bnet_sig(sd, BNET_EOF);

   /* Inform Director that we are done */
   bnet_sig(dir, BNET_EOF);

   Dmsg0(30, "Done in job.c\n");
   return 1;
}

static int open_sd_read_session(JCR *jcr)
{
   int len;
   BSOCK *sd = jcr->store_bsock;

   if (!sd) {
      Jmsg(jcr, M_FATAL, 0, _("Improper calling sequence.\n"));
      return 0;
   }
   Dmsg4(20, "VolSessId=%ld VolsessT=%ld SF=%ld EF=%ld\n",
      jcr->VolSessionId, jcr->VolSessionTime, jcr->StartFile, jcr->EndFile);
   Dmsg2(20, "JobId=%d vol=%s\n", jcr->JobId, "DummyVolume");
   /* 
    * Open Read Session with Storage daemon
    */
   bnet_fsend(sd, read_open, jcr->VolumeName,
      jcr->VolSessionId, jcr->VolSessionTime, jcr->StartFile, jcr->EndFile, 
      jcr->StartBlock, jcr->EndBlock);
   Dmsg1(10, ">stored: %s", sd->msg);

   /* 
    * Get ticket number
    */
   if ((len = bnet_recv(sd)) > 0) {
      Dmsg1(10, "bfiled<stored: %s", sd->msg);
      if (sscanf(sd->msg, OK_open, &jcr->Ticket) != 1) {
         Jmsg(jcr, M_FATAL, 0, _("Bad response to SD read open: %s\n"), sd->msg);
	 return 0;
      }
      Dmsg1(10, "bfiled: got Ticket=%d\n", jcr->Ticket);
   } else {
      Jmsg(jcr, M_FATAL, 0, _("Bad response from stored to read open command\n"));
      return 0;
   }

   /* 
    * Start read of data with Storage daemon
    */
   bnet_fsend(sd, read_data, jcr->Ticket);
   Dmsg1(10, ">stored: %s", sd->msg);

   /* 
    * Get OK data
    */
   if (!response(sd, OK_data, "Read Data")) {
      return 0;
   }
   return 1;
}

/* 
 * Destroy the Job Control Record and associated
 * resources (sockets).
 */
static void filed_free_jcr(JCR *jcr) 
{
   if (jcr->store_bsock) {
      bnet_close(jcr->store_bsock);
   }
   if (jcr->where) {
      free_pool_memory(jcr->where);
   }
   if (jcr->last_fname) {
      free_pool_memory(jcr->last_fname);
   }
   return;
}

/*
 * Get response from Storage daemon to a command we
 * sent. Check that the response is OK.
 *
 *  Returns: 0 on failure
 *	     1 on success
 */
int response(BSOCK *sd, char *resp, char *cmd)
{
   int n;

   if (sd->errors) {
      return 0;
   }
   if ((n = bnet_recv(sd)) > 0) {
      Dmsg0(10, sd->msg);
      if (strcmp(sd->msg, resp) == 0) {
	 return 1;
      }
   } 
   /* ********FIXME******** segfault if the following is executed */
   if (n > 0) {
      Emsg3(M_FATAL, 0, _("<stored: bad response to %s: wanted: %s, got: %s\n"),
	 cmd, resp, sd->msg);
   } else {
      Emsg2(M_FATAL, 0, _("<stored: bad response to %s command: ERR=%s\n"),
	 cmd, bnet_strerror(sd));
   }
   return 0;
}
