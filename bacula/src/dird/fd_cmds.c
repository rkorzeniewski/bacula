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
#include "dird.h"

/* Commands sent to File daemon */
static char inc[]         = "include\n";
static char exc[]         = "exclude\n";
static char jobcmd[]      = "JobId=%d Job=%s SDid=%u SDtime=%u Authorization=%s\n";


/* Responses received from File daemon */
static char OKinc[]      = "2000 OK include\n";
static char OKexc[]      = "2000 OK exclude\n";
static char OKjob[]      = "2000 OK Job";

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
   Dmsg1(110, ">filed: %s", fd->msg);
   if (bnet_recv(fd) > 0) {
       Dmsg1(110, "<filed: %s", fd->msg);
       if (strncmp(fd->msg, OKjob, strlen(OKjob)) != 0) {
          Jmsg(jcr, M_FATAL, 0, _("File daemon rejected Job command: %s\n"), fd->msg);
	  jcr->JobStatus = JS_ErrorTerminated;
	  return 0;
       } else {
	  /***** ***FIXME***** update Client Uname */
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
   int i, j;
   char *msgsave;

   fd = jcr->file_bsock;
   fileset = jcr->fileset;

   fd->msglen = sprintf(fd->msg, inc);
   bnet_send(fd);
   for (i=0; i < fileset->num_includes; i++) {
      BPIPE *bpipe;
      FILE *ffd;
      char buf[1000];
      char *o, *p, *q;
      int optlen, stat;

      Dmsg1(120, "dird>filed: include file: %s\n", fileset->include_array[i]);
      o = p = fileset->include_array[i];
      skip_nonspaces(&p);	      /* skip options */
      skip_spaces(&p);
      q = p;			      /* save end of options */
      switch (*p++) {
      case '|':
         fd->msg = edit_job_codes(jcr, fd->msg, p, "");
         bpipe = open_bpipe(fd->msg, 0, "r");
	 if (!bpipe) {
            Jmsg(jcr, M_FATAL, 0, _("Cannot run program: %s. ERR=%s\n"),
	       p, strerror(errno));
	    goto bail_out;
	 }
	 /* Copy File options */
	 optlen = q - o;
	 for (j=0; j < optlen; j++) {
	    buf[j] = *o++;
	 }
	 while (fgets(buf+optlen, sizeof(buf)-optlen, bpipe->rfd)) {
            fd->msglen = Mmsg(&fd->msg, "%s", buf);
            Dmsg2(200, "Including len=%d: %s", fd->msglen, fd->msg);
	    if (!bnet_send(fd)) {
               Jmsg(jcr, M_FATAL, 0, _(">filed: write error on socket\n"));
	       goto bail_out;
	    }
	 }
	 if ((stat=close_bpipe(bpipe)) != 0) {
            Jmsg(jcr, M_FATAL, 0, _("Error running program: %s. RtnStat=%d ERR=%s\n"),
	       p, stat, strerror(errno));
	    goto bail_out;
	 }
	 break;
      case '<':
         if ((ffd = fopen(p, "r")) == NULL) {
            Jmsg(jcr, M_FATAL, 0, _("Cannot open included file: %s. ERR=%s\n"),
	       p, strerror(errno));
	    goto bail_out;
	 }
	 /* Copy File options */
	 optlen = q - o;
	 for (j=0; j < optlen; j++) {
	    buf[j] = *o++;
	 }
	 while (fgets(buf+optlen, sizeof(buf)-optlen, ffd)) {
            fd->msglen = Mmsg(&fd->msg, "%s", buf);
	    if (!bnet_send(fd)) {
               Jmsg(jcr, M_FATAL, 0, _(">filed: write error on socket\n"));
	       goto bail_out;
	    }
	 }
	 fclose(ffd);
	 break;
      default:
	 msgsave = fd->msg;
	 fd->msg = fileset->include_array[i];
	 fd->msglen = strlen(fileset->include_array[i]);
	 if (!bnet_send(fd)) {
            Jmsg(jcr, M_FATAL, 0, _(">filed: write error on socket\n"));
	    goto bail_out;
	 }
	fd->msg = msgsave;
	 break;
      }
   }
   bnet_sig(fd, BNET_EOD);	      /* end of data */
   if (!response(fd, OKinc, "Include")) {
      goto bail_out;
   }
   return 1;

bail_out:
   jcr->JobStatus = JS_ErrorTerminated;
   return 0;

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
      Dmsg1(120, "dird>filed: exclude file: %s\n", fileset->exclude_array[i]);
      fd->msg = fileset->exclude_array[i];
      if (!bnet_send(fd)) {
         Jmsg(jcr, M_FATAL, 0, _(">filed: write error on socket\n"));
	 jcr->JobStatus = JS_ErrorTerminated;
	 return 0;
      }
   }
   bnet_sig(fd, BNET_EOD);
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
   int n = 0;
   ATTR_DBR ar;

   fd = jcr->file_bsock;
   jcr->jr.FirstIndex = 1;
   memset(&ar, 0, sizeof(ar));
   jcr->FileIndex = 0;

   Dmsg0(120, "bdird: waiting to receive file attributes\n");
   /* Pickup file attributes and signature */
   while (!fd->errors && (n = bget_msg(fd, 0)) > 0) {

   /*****FIXME****** improve error handling to stop only on 
    * really fatal problems, or the number of errors is too
    * large.
    */
      long file_index;
      int stream, len;
      char *attr, *p, *fn;
      char Opts_MD5[MAXSTRING];      /* either Verify opts or MD5 signature */
      char MD5[MAXSTRING];

      jcr->fname = check_pool_memory_size(jcr->fname, fd->msglen);
      if ((len = sscanf(fd->msg, "%ld %d %s", &file_index, &stream, Opts_MD5)) != 3) {
         Jmsg(jcr, M_FATAL, 0, _("<filed: bad attributes, expected 3 fields got %d\n\
msglen=%d msg=%s\n"), len, fd->msglen, fd->msg);
	 jcr->JobStatus = JS_ErrorTerminated;
	 return 0;
      }
      p = fd->msg;
      skip_nonspaces(&p);	      /* skip FileIndex */
      skip_spaces(&p);
      skip_nonspaces(&p);	      /* skip Stream */
      skip_spaces(&p);
      skip_nonspaces(&p);	      /* skip Opts_MD5 */   
      p++;			      /* skip space */
      fn = jcr->fname;
      while (*p != 0) {
	 *fn++ = *p++;		      /* copy filename */
      }
      *fn = *p++;		      /* term filename and point to attribs */
      attr = p;

      if (stream == STREAM_UNIX_ATTRIBUTES || stream == STREAM_WIN32_ATTRIBUTES) {
	 jcr->JobFiles++;
	 jcr->FileIndex = file_index;
	 ar.attr = attr;
	 ar.fname = jcr->fname;
	 ar.FileIndex = file_index;
	 ar.Stream = stream;
	 ar.link = NULL;
	 ar.JobId = jcr->JobId;
	 ar.ClientId = jcr->ClientId;
	 ar.PathId = 0;
	 ar.FilenameId = 0;

         Dmsg2(111, "dird<filed: stream=%d %s\n", stream, jcr->fname);
         Dmsg1(120, "dird<filed: attr=%s\n", attr);

	 if (!db_create_file_attributes_record(jcr->db, &ar)) {
            Jmsg1(jcr, M_ERROR, 0, "%s", db_strerror(jcr->db));
	    jcr->JobStatus = JS_Error;
	    continue;
	 }
	 jcr->FileId = ar.FileId;
      } else if (stream == STREAM_MD5_SIGNATURE) {
	 if (jcr->FileIndex != (uint32_t)file_index) {
            Jmsg2(jcr, M_ERROR, 0, _("MD5 index %d not same as attributes %d\n"),
	       file_index, jcr->FileIndex);
	    jcr->JobStatus = JS_Error;
	    continue;
	 }
	 db_escape_string(MD5, Opts_MD5, strlen(Opts_MD5));
         Dmsg2(120, "MD5len=%d MD5=%s\n", strlen(MD5), MD5);
	 if (!db_add_MD5_to_file_record(jcr->db, jcr->FileId, MD5)) {
            Jmsg1(jcr, M_ERROR, 0, "%s", db_strerror(jcr->db));
	    jcr->JobStatus = JS_Error;
	 }
      }
      jcr->jr.JobFiles = jcr->JobFiles = file_index;
      jcr->jr.LastIndex = file_index;
   } 
   if (is_bnet_error(fd)) {
      Jmsg1(jcr, M_FATAL, 0, _("<filed: Network error getting attributes. ERR=%s\n"),
			bnet_strerror(fd));
      jcr->JobStatus = JS_ErrorTerminated;
      return 0;
   }

   jcr->JobStatus = JS_Terminated;
   return 1;
}
