/*
 * This file handles commands from the File daemon.
 *
 * We get here because the Director has initiated a Job with
 *  the Storage daemon, then done the same with the File daemon,
 *  then when the Storage daemon receives a proper connection from
 *  the File daemon, control is passed here to handle the 
 *  subsequent File daemon commands.
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
#include "stored.h"

/* Imported variables */
extern STORES *me;

/* Static variables */
static char ferrmsg[]      = "3900 Invalid command\n";

/* Imported functions */
extern int do_append_data(JCR *jcr);
extern int do_read_data(JCR *jcr);

/* Forward referenced functions */


/* Forward referenced FD commands */
static int append_open_session(JCR *jcr);
static int append_close_session(JCR *jcr);
static int append_data_cmd(JCR *jcr);
static int append_end_session(JCR *jcr);
static int read_open_session(JCR *jcr);
static int read_data_cmd(JCR *jcr);
static int read_close_session(JCR *jcr);
static int bootstrap_cmd(JCR *jcr);

struct s_cmds {
   char *cmd;
   int (*func)(JCR *jcr);
};

/*  
 * The following are the recognized commands from the File daemon
 */
static struct s_cmds fd_cmds[] = {
   {"append open",  append_open_session},
   {"append data",  append_data_cmd},
   {"append end",   append_end_session},
   {"append close", append_close_session},
   {"read open",    read_open_session},
   {"read data",    read_data_cmd},
   {"read close",   read_close_session},
   {"bootstrap",    bootstrap_cmd},
   {NULL,	    NULL}		   /* list terminator */
};

/* Commands from the File daemon that require additional scanning */
static char read_open[]       = "read open session = %s %ld %ld %ld %ld %ld %ld\n";

/* Responses sent to the File daemon */
static char NO_open[]         = "3901 Error session already open\n";
static char NOT_opened[]      = "3902 Error session not opened\n";
static char OK_end[]          = "3000 OK end\n";
static char OK_close[]        = "3000 OK close Volumes = %d\n";
static char OK_open[]         = "3000 OK open ticket = %d\n";
static char OK_append[]       = "3000 OK append data\n";
static char ERROR_append[]    = "3903 Error append data\n";
static char OK_bootstrap[]    = "3000 OK bootstrap\n";
static char ERROR_bootstrap[] = "3904 Error bootstrap\n";

/* Information sent to the Director */
static char Job_start[] = "3010 Job %s start\n";
static char Job_end[]	= 
   "3099 Job %s end JobStatus=%d JobFiles=%d JobBytes=%s\n";

/*
 * Run a File daemon Job -- File daemon already authorized
 *
 * Basic task here is:
 * - Read a command from the File daemon
 * - Execute it
 *
 */
void run_job(JCR *jcr)
{
   int i, found, quit;
   BSOCK *fd = jcr->file_bsock;
   BSOCK *dir = jcr->dir_bsock;
   char ec1[30];


   fd->jcr = (void *)jcr;
   dir->jcr = (void *)jcr;
   Dmsg1(120, "Start run Job=%s\n", jcr->Job);
   bnet_fsend(dir, Job_start, jcr->Job); 
   time(&jcr->start_time);
   jcr->run_time = jcr->start_time;
   jcr->JobStatus = JS_Running;
   dir_send_job_status(jcr);	      /* update director */
   for (quit=0; !quit;) {

      /* Read command coming from the File daemon */
      if (bnet_recv(fd) <= 0) {
	 break; 		      /* connection terminated */
      }
      Dmsg1(110, "<filed: %s", fd->msg);
      found = 0;
      for (i=0; fd_cmds[i].cmd; i++) {
	 if (strncmp(fd_cmds[i].cmd, fd->msg, strlen(fd_cmds[i].cmd)) == 0) {
	    found = 1;		     /* indicate command found */
	    if (!fd_cmds[i].func(jcr)) {    /* do command */
	       jcr->JobStatus = JS_ErrorTerminated;
	       quit = 1;
	    }
	    break;
	 }
      }
      if (!found) {		      /* command not found */
         Dmsg1(110, "<filed: Command not found: %s\n", fd->msg);
	 bnet_fsend(fd, ferrmsg);
	 break;
      }
   }
   time(&jcr->end_time);
   if (!job_cancelled(jcr)) {
      jcr->JobStatus = JS_Terminated;
   }
   bnet_fsend(dir, Job_end, jcr->Job, jcr->JobStatus, jcr->JobFiles,
      edit_uint64(jcr->JobBytes, ec1));

   bnet_sig(dir, BNET_EOD);	      /* send EOD to Director daemon */
   return;
}

	
/*
 *   Append Data command
 *     Open Data Channel and receive Data for archiving
 *     Write the Data to the archive device
 */
static int append_data_cmd(JCR *jcr)
{
   BSOCK *fd = jcr->file_bsock;

   Dmsg1(120, "Append data: %s", fd->msg);
   if (jcr->session_opened) {
      Dmsg1(110, "<bfiled: %s", fd->msg);
      if (do_append_data(jcr)) {
	 bnet_fsend(fd, OK_append);
	 jcr->JobType = JT_BACKUP;
	 return 1;
      } else {
	 bnet_fsend(fd, ERROR_append);
	 return 0;
      }
   } else {
      bnet_fsend(fd, NOT_opened);
      return 0;
   }
}

static int append_end_session(JCR *jcr)
{
   BSOCK *fd = jcr->file_bsock;

   Dmsg1(120, "store<file: %s", fd->msg);
   if (!jcr->session_opened) {
      bnet_fsend(fd, NOT_opened);
      return 0;
   }
   return bnet_fsend(fd, OK_end);
}


/* 
 * Append Open session command
 *
 */
static int append_open_session(JCR *jcr)
{
   BSOCK *fd = jcr->file_bsock;

   Dmsg1(120, "Append open session: %s", fd->msg);
   if (jcr->session_opened) {
      bnet_fsend(fd, NO_open);
      return 0;
   }

   Dmsg1(110, "Append open session: %s\n", dev_name(jcr->device->dev));
   jcr->session_opened = TRUE;

   /* Send "Ticket" to File Daemon */
   bnet_fsend(fd, OK_open, jcr->VolSessionId);
   Dmsg1(110, ">filed: %s", fd->msg);

   return 1;
}

/*
 *   Append Close session command
 *	Close the append session and send back Statistics     
 *	   (need to fix statistics)
 */
static int append_close_session(JCR *jcr)
{
   BSOCK *fd = jcr->file_bsock;

   Dmsg1(120, "<filed: %s\n", fd->msg);
   if (!jcr->session_opened) {
      bnet_fsend(fd, NOT_opened);
      return 0;
   }
   /* Send final statistics to File daemon */
   bnet_fsend(fd, OK_close, jcr->NumVolumes);
   Dmsg1(160, ">filed: %s\n", fd->msg);

   bnet_sig(fd, BNET_EOD);	      /* send EOD to File daemon */
       
   Dmsg1(110, "Append close session: %s\n", dev_name(jcr->device->dev));

   if (jcr->JobStatus != JS_ErrorTerminated) {
      jcr->JobStatus = JS_Terminated;
   }
   jcr->session_opened = FALSE;
   return 1;
}

/*
 *   Read Data command
 *     Open Data Channel, read the data from  
 *     the archive device and send to File
 *     daemon.
 */
static int read_data_cmd(JCR *jcr)
{
   BSOCK *fd = jcr->file_bsock;

   Dmsg1(120, "Read data: %s\n", fd->msg);
   if (jcr->session_opened) {
      Dmsg1(120, "<bfiled: %s", fd->msg);
      return do_read_data(jcr);
   } else {
      bnet_fsend(fd, NOT_opened);
      return 0;
   }
}

/* 
 * Read Open session command
 *
 *  We need to scan for the parameters of the job
 *    to be restored.
 */
static int read_open_session(JCR *jcr)
{
   BSOCK *fd = jcr->file_bsock;

   Dmsg1(120, "%s\n", fd->msg);
   if (jcr->session_opened) {
      bnet_fsend(fd, NO_open);
      return 0;
   }

   if (sscanf(fd->msg, read_open, jcr->VolumeName, &jcr->read_VolSessionId,
	 &jcr->read_VolSessionTime, &jcr->read_StartFile, &jcr->read_EndFile,
	 &jcr->read_StartBlock, &jcr->read_EndBlock) == 7) {
      if (jcr->session_opened) {
	 bnet_fsend(fd, NOT_opened);
	 return 0;
      }
      Dmsg4(100, "read_open_session got: JobId=%d Vol=%s VolSessId=%ld VolSessT=%ld\n",
	 jcr->JobId, jcr->VolumeName, jcr->read_VolSessionId, 
	 jcr->read_VolSessionTime);
      Dmsg4(100, "  StartF=%ld EndF=%ld StartB=%ld EndB=%ld\n",
	 jcr->read_StartFile, jcr->read_EndFile, jcr->read_StartBlock,
	 jcr->read_EndBlock);
   }

   Dmsg1(110, "Read open session: %s\n", dev_name(jcr->device->dev));

   jcr->session_opened = TRUE;
   jcr->JobType = JT_RESTORE;

   /* Send "Ticket" to File Daemon */
   bnet_fsend(fd, OK_open, jcr->VolSessionId);
   Dmsg1(110, ">filed: %s", fd->msg);

   return 1;
}

static int bootstrap_cmd(JCR *jcr)
{
   BSOCK *fd = jcr->file_bsock;
   POOLMEM *fname = get_pool_memory(PM_FNAME);
   FILE *bs;

   if (jcr->RestoreBootstrap) {
      unlink(jcr->RestoreBootstrap);
      free_pool_memory(jcr->RestoreBootstrap);
   }
   Mmsg(&fname, "%s/%s.%s.bootstrap", me->working_directory, me->hdr.name,
      jcr->Job);
   Dmsg1(400, "bootstrap=%s\n", fname);
   jcr->RestoreBootstrap = fname;
   bs = fopen(fname, "a+");           /* create file */
   if (!bs) {
      Jmsg(jcr, M_FATAL, 0, _("Could not create bootstrap file %s: ERR=%s\n"),
	 jcr->RestoreBootstrap, strerror(errno));
      goto bail_out;
   }
   while (bnet_recv(fd) > 0) {
       Dmsg1(400, "stored<filed: bootstrap file %s\n", fd->msg);
       fputs(fd->msg, bs);
   }
   fclose(bs);
   jcr->bsr = parse_bsr(jcr, jcr->RestoreBootstrap);
   if (!jcr->bsr) {
      Jmsg(jcr, M_FATAL, 0, _("Error parsing bootstrap file.\n"));
      goto bail_out;
   }
   if (debug_level > 20) {
      dump_bsr(jcr->bsr);
   }
   return bnet_fsend(fd, OK_bootstrap);

bail_out:
   unlink(jcr->RestoreBootstrap);
   free_pool_memory(jcr->RestoreBootstrap);
   jcr->RestoreBootstrap = NULL;
   bnet_fsend(fd, ERROR_bootstrap);
   return 0;

}


/*
 *   Read Close session command
 *	Close the read session
 */
static int read_close_session(JCR *jcr)
{
   BSOCK *fd = jcr->file_bsock;

   Dmsg1(120, "Read close session: %s\n", fd->msg);
   if (!jcr->session_opened) {
      bnet_fsend(fd, NOT_opened);
      return 0;
   }
   /* Send final statistics to File daemon */
   bnet_fsend(fd, OK_close);
   Dmsg1(160, ">filed: %s\n", fd->msg);

   bnet_sig(fd, BNET_EOD);	    /* send EOD to File daemon */
       
   jcr->session_opened = FALSE;
   return 1;
}
