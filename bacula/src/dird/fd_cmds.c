/*
 *
 *   Bacula Director -- fd_cmds.c -- send commands to File daemon
 *
 *     Kern Sibbald, October MM
 *
 *    This routine is run as a separate thread.  There may be more
 *    work to be done to make it totally reentrant!!!!
 * 
 *  Utility functions for sending info to File Daemon.
 *   These functions are used by both backup and verify.
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
#include "dird.h"

/* Commands sent to File daemon */
static char inc[]         = "include\n";
static char exc[]         = "exclude\n";
static char jobcmd[]      = "JobId=%d Job=%s SDid=%u SDtime=%u Authorization=%s\n";


/* Responses received from File daemon */
static char OKinc[]      = "2000 OK include\n";
static char OKexc[]      = "2000 OK exclude\n";
static char OKjob[]      = "2000 OK Job\n";

/* Forward referenced functions */

/* External functions */
extern int debug_level;
extern DIRRES *director; 
extern int FDConnectTimeout;

/*
 * Open connection with File daemon. 
 * Try connecting every 10 seconds, give up after 1 hour.
 */

int connect_to_file_daemon(JCR *jcr, int retry_interval, int max_retry_time,
			   int verbose)
{
   BSOCK   *fd;

   fd = bnet_connect(jcr, retry_interval, max_retry_time,
        _("File daemon"), jcr->client->address, 
	NULL, jcr->client->FDport, verbose);
   if (fd == NULL) {
      jcr->JobStatus = JS_ErrorTerminated;
      return 0;
   }
   Dmsg0(10, "Opened connection with File daemon\n");
   fd->res = (RES *)jcr->client;      /* save resource in BSOCK */
   jcr->file_bsock = fd;
   jcr->JobStatus = JS_Running;

   if (!authenticate_file_daemon(jcr)) {
      jcr->JobStatus = JS_ErrorTerminated;
      return 0;
   }
	
   /*
    * Now send JobId and authorization key
    */
   bnet_fsend(fd, jobcmd, jcr->JobId, jcr->Job, jcr->VolSessionId, 
      jcr->VolSessionTime, jcr->sd_auth_key);
   Dmsg1(10, ">filed: %s", fd->msg);
   if (bnet_recv(fd) > 0) {
       Dmsg1(10, "<filed: %s", fd->msg);
       if (strcmp(fd->msg, OKjob) != 0) {
          Jmsg(jcr, M_FATAL, 0, _("File daemon rejected Job command: %s\n"), fd->msg);
	  jcr->JobStatus = JS_ErrorTerminated;
	  return 0;
       } 
   } else {
      Jmsg(jcr, M_FATAL, 0, _("<filed: bad response to JobId command: %s\n"),
	 bnet_strerror(fd));
      jcr->JobStatus = JS_ErrorTerminated;
      return 0;
   }
   return 1;
}


/*
 * Send include list to File daemon
 */
int send_include_list(JCR *jcr)
{
   FILESET *fileset;
   BSOCK   *fd;
   int i;
   char *msgsave;

   fd = jcr->file_bsock;
   fileset = jcr->fileset;

   msgsave = fd->msg;

   fd->msglen = sprintf(fd->msg, inc);
   bnet_send(fd);
   for (i=0; i < fileset->num_includes; i++) {
      fd->msglen = strlen(fileset->include_array[i]);
      Dmsg1(20, "dird>filed: include file: %s\n", fileset->include_array[i]);
      fd->msg = fileset->include_array[i];
      if (!bnet_send(fd)) {
         Emsg0(M_FATAL, 0, _(">filed: write error on socket\n"));
	 jcr->JobStatus = JS_ErrorTerminated;
	 return 0;
      }
   }
   bnet_sig(fd, BNET_EOF);
   fd->msg = msgsave;
   if (!response(fd, OKinc, "Include")) {
      jcr->JobStatus = JS_ErrorTerminated;
      return 0;
   }
   return 1;
}

/*
 * Send exclude list to File daemon 
 */
int send_exclude_list(JCR *jcr)
{
   FILESET *fileset;
   BSOCK   *fd;
   int i;
   char *msgsave;

   fd = jcr->file_bsock;
   fileset = jcr->fileset;

   msgsave = fd->msg;
   fd->msglen = sprintf(fd->msg, exc);
   bnet_send(fd);
   for (i=0; i < fileset->num_excludes; i++) {
      fd->msglen = strlen(fileset->exclude_array[i]);
      Dmsg1(20, "dird>filed: exclude file: %s\n", fileset->exclude_array[i]);
      fd->msg = fileset->exclude_array[i];
      if (!bnet_send(fd)) {
         Emsg0(M_FATAL, 0, _(">filed: write error on socket\n"));
	 jcr->JobStatus = JS_ErrorTerminated;
	 return 0;
      }
   }
   bnet_sig(fd, BNET_EOF);
   fd->msg = msgsave;
   if (!response(fd, OKexc, "Exclude")) {
      jcr->JobStatus = JS_ErrorTerminated;
      return 0;
   }
   return 1;
}


/* 
 * Read the attributes from the File daemon for
 * a Verify job and store them in the catalog.
 */
int get_attributes_and_put_in_catalog(JCR *jcr)
{
   BSOCK   *fd;
   int n;
   ATTR_DBR ar;

   fd = jcr->file_bsock;
   jcr->jr.FirstIndex = 1;
   memset(&ar, 0, sizeof(ar));

   Dmsg0(20, "bdird: waiting to receive file attributes\n");
   /* Pickup file attributes and signature */
   while (!fd->errors && (n = bget_msg(fd, 0)) > 0) {

   /*****FIXME****** improve error handling to stop only on 
    * really fatal problems, or the number of errors is too
    * large.
    */
       long file_index;
       int stream, len;
       char *attr, buf[MAXSTRING];
       char Opts[MAXSTRING];	      /* either Verify opts or MD5 signature */

       /* ***FIXME*** check fname length */
       if ((len = sscanf(fd->msg, "%ld %d %s %s", &file_index, &stream, 
	     Opts, jcr->fname)) != 4) {
          Jmsg(jcr, M_FATAL, 0, _("<filed: bad attributes, expected 4 fields got %d\n\
msglen=%d msg=%s\n"), len, fd->msglen, fd->msg);
	  jcr->JobStatus = JS_ErrorTerminated;
	  return 0;
       }

       if (stream == STREAM_UNIX_ATTRIBUTES) {
	  len = strlen(fd->msg);      /* length before attributes */
	  ar.attr = &fd->msg[len+1];
	  ar.fname = jcr->fname;
	  ar.FileIndex = 0;	      /* used as mark field during compare */
	  ar.Stream = stream;
	  ar.link = NULL;
	  ar.JobId = jcr->JobId;
	  ar.ClientId = jcr->ClientId;
	  ar.PathId = 0;
	  ar.FilenameId = 0;

          Dmsg2(11, "dird<filed: stream=%d %s\n", stream, jcr->fname);
          Dmsg1(20, "dird<filed: attr=%s\n", attr);

	  /* ***FIXME*** fix link field */
	  if (!db_create_file_attributes_record(jcr->db, &ar)) {
             Jmsg1(jcr, M_FATAL, 0, "%s", db_strerror(jcr->db));
	     jcr->JobStatus = JS_ErrorTerminated;
	     /* This probably should not really be fatal *****FIXME****** */
	     return 0;
	  }
	  jcr->FileIndex = file_index;
	  jcr->FileId = ar.FileId;
       } else if (stream == STREAM_MD5_SIGNATURE) {
	  if (jcr->FileIndex != (uint32_t)file_index) {
             Jmsg0(jcr, M_FATAL, 0, _("Got MD5 but not same block as attributes\n"));
	     jcr->JobStatus = JS_ErrorTerminated;
	     return 0;
	  }
	  db_escape_string(buf, Opts, strlen(Opts));
          Dmsg2(20, "MD5len=%d MD5=%s\n", strlen(buf), buf);
	  if (!db_add_MD5_to_file_record(jcr->db, jcr->FileId, buf)) {
             Jmsg1(jcr, M_WARNING, 0, "%s", db_strerror(jcr->db));
	  }
       }
       jcr->jr.JobFiles = file_index;
       jcr->jr.LastIndex = file_index;
   } 
   if (n < 0) {
      Jmsg1(jcr, M_FATAL, 0, _("<filed: Network error getting attributes. ERR=%s\n"),
			bnet_strerror(fd));
      jcr->JobStatus = JS_ErrorTerminated;
      return 0;
   }

   jcr->JobStatus = JS_Terminated;
   return 1;
}
