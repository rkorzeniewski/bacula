/*
 *
 *   Bacula Director -- User Agent Output Commands
 *     I.e. messages, listing database, showing resources, ...
 *
 *     Kern Sibbald, September MM
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
#include "ua.h"

/* Imported subroutines */
extern void run_job(JCR *jcr);

/* Imported variables */
extern int r_first;
extern int r_last;
extern struct s_res resources[];
extern int console_msg_pending;
extern FILE *con_fd;


/* Imported functions */

/* Forward referenced functions */


/*
 * Turn auto display of console messages on/off
 */
int autodisplaycmd(UAContext *ua, char *cmd)
{
   static char *kw[] = {
      N_("on"), 
      N_("off"),
      NULL};

   switch (find_arg_keyword(ua, kw)) {
      case 0:
	 ua->auto_display_messages = 1;
	 break;
      case 1:
	 ua->auto_display_messages = 0;
	 break;
      default:
         bsendmsg(ua, _("ON or OFF keyword missing.\n"));
	 break;
   }
   return 1; 
}


struct showstruct {char *res_name; int type;};
static struct showstruct reses[] = {
   {N_("directors"),  R_DIRECTOR},
   {N_("clients"),    R_CLIENT},
   {N_("jobs"),       R_JOB},
   {N_("storages"),   R_STORAGE},
   {N_("catalogs"),   R_CATALOG},
   {N_("schedules"),  R_SCHEDULE},
   {N_("filesets"),   R_FILESET},
   {N_("groups"),     R_GROUP},
   {N_("pools"),      R_POOL},
   {N_("messages"),   R_MSGS},
   {N_("all"),        -1},
   {N_("help"),       -2},
   {NULL,	    0}
};


/*
 *  Displays Resources
 *
 *  show all
 *  show <resource-keyword-name>  e.g. show directors
 *  show <resource-keyword-name>=<name> e.g. show director=HeadMan
 *
 */
int showcmd(UAContext *ua, char *cmd)
{
   int i, j, type, len; 
   int recurse;
   char *res_name;
   RES *res;

   Dmsg1(20, "show: %s\n", ua->UA_sock->msg);


   for (i=1; i<ua->argc; i++) {
      type = 0;
      res_name = ua->argk[i]; 
      if (!ua->argv[i]) {	      /* was a name given? */
	 /* No name, dump all resources of specified type */
	 recurse = 1;
	 len = strlen(res_name);
	 for (j=0; reses[j].res_name; j++) {
	    if (strncasecmp(res_name, _(reses[j].res_name), len) == 0) {
	       type = reses[j].type;
	       if (type > 0) {
		  res = resources[type-r_first].res_head;
	       } else {
		  res = NULL;
	       }
	       break;
	    }
	 }
      } else {
	 /* Dump a single resource with specified name */
	 recurse = 0;
	 len = strlen(res_name);
	 for (j=0; reses[j].res_name; j++) {
	    if (strncasecmp(res_name, _(reses[j].res_name), len) == 0) {
	       type = reses[j].type;
	       res = (RES *)GetResWithName(type, ua->argv[i]);
	       if (!res) {
		  type = -3;
	       }
	       break;
	    }
	 }
      }

      switch (type) {
      case -1:				 /* all */
	 for (j=r_first; j<=r_last; j++) {
	    dump_resource(j, resources[j-r_first].res_head, bsendmsg, ua);     
	 }
	 break;
      case -2:
         bsendmsg(ua, _("Keywords for the show command are:\n"));
	 for (j=0; reses[j].res_name; j++) {
            bsendmsg(ua, "%s\n", _(reses[j].res_name));
	 }
	 return 1;
      case -3:
         bsendmsg(ua, _("%s resource %s not found.\n"), res_name, ua->argv[i]);
	 return 1;
      case 0:
         bsendmsg(ua, _("Resource %s not found\n"), res_name);
	 return 1;
      default:
	 dump_resource(recurse?type:-type, res, bsendmsg, ua);
	 break;
      }
   }
   return 1;
}




/*
 *  List contents of database
 *
 *  list jobs		- lists all jobs run
 *  list jobid=nnn	- list job data for jobid
 *  list job=name	- list job data for job
 *  list jobmedia jobid=<nn>
 *  list jobmedia job=name
 *  list files jobid=<nn> - list files saved for job nn
 *  list files job=name
 *  list pools		- list pool records
 *  list jobtotals	- list totals for all jobs
 *  list media		- list media for given pool
 *
 */
int listcmd(UAContext *ua, char *cmd) 
{
   char *VolumeName;
   int jobid, n;
   int i, j;
   JOB_DBR jr;
   POOL_DBR pr;
   MEDIA_DBR mr;

   if (!open_db(ua))
      return 1;

   memset(&jr, 0, sizeof(jr));
   memset(&pr, 0, sizeof(pr));
   memset(&mr, 0, sizeof(mr));

   Dmsg1(20, "list: %s\n", cmd);

   if (!ua->db) {
      bsendmsg(ua, _("Hey! DB is NULL\n"));
   }

   /* Scan arguments looking for things to do */
   for (i=1; i<ua->argc; i++) {
      /* List JOBS */
      if (strcasecmp(ua->argk[i], _("jobs")) == 0) {
	 db_list_job_records(ua->db, &jr, prtit, ua);

	 /* List JOBTOTALS */
      } else if (strcasecmp(ua->argk[i], _("jobtotals")) == 0) {
	 db_list_job_totals(ua->db, &jr, prtit, ua);

      /* List JOBID */
      } else if (strcasecmp(ua->argk[i], _("jobid")) == 0) {
	 if (ua->argv[i]) {
	    jobid = atoi(ua->argv[i]);
	    if (jobid > 0) {
	       jr.JobId = jobid;
	       db_list_job_records(ua->db, &jr, prtit, ua);
	    }
	 }

      /* List JOB */
      } else if (strcasecmp(ua->argk[i], _("job")) == 0 && ua->argv[i]) {
	 strncpy(jr.Job, ua->argv[i], MAX_NAME_LENGTH);
	 jr.Job[MAX_NAME_LENGTH-1] = 0;
	 jr.JobId = 0;
	 db_list_job_records(ua->db, &jr, prtit, ua);

      /* List FILES */
      } else if (strcasecmp(ua->argk[i], _("files")) == 0) {

	 for (j=i+1; j<ua->argc; j++) {
            if (strcasecmp(ua->argk[j], _("job")) == 0 && ua->argv[j]) {
	       strncpy(jr.Job, ua->argv[i], MAX_NAME_LENGTH);
	       jr.Job[MAX_NAME_LENGTH-1] = 0;
	       jr.JobId = 0;
	       db_get_job_record(ua->db, &jr);
	       jobid = jr.JobId;
            } else if (strcasecmp(ua->argk[j], _("jobid")) == 0 && ua->argv[j]) {
	       jobid = atoi(ua->argv[j]);
	    } else {
	       continue;
	    }
	    if (jobid > 0) {
	       db_list_files_for_job(ua->db, jobid, prtit, ua);
	    }
	 }
      
      /* List JOBMEDIA */
      } else if (strcasecmp(ua->argk[i], _("jobmedia")) == 0) {
	 int done = FALSE;
	 for (j=i+1; j<ua->argc; j++) {
            if (strcasecmp(ua->argk[j], _("job")) == 0 && ua->argv[j]) {
	       strncpy(jr.Job, ua->argv[i], MAX_NAME_LENGTH);
	       jr.Job[MAX_NAME_LENGTH-1] = 0;
	       jr.JobId = 0;
	       db_get_job_record(ua->db, &jr);
	       jobid = jr.JobId;
            } else if (strcasecmp(ua->argk[j], _("jobid")) == 0 && ua->argv[j]) {
	       jobid = atoi(ua->argv[j]);
	    } else {
	       continue;
	    }
	    db_list_jobmedia_records(ua->db, jobid, prtit, ua);
	    done = TRUE;
	 }
	 if (!done) {
	    /* List for all jobs (jobid=0) */
	    db_list_jobmedia_records(ua->db, 0, prtit, ua);
	 }

      /* List POOLS */
      } else if (strcasecmp(ua->argk[i], _("pools")) == 0) {
	 db_list_pool_records(ua->db, prtit, ua);

      /* List MEDIA */
      } else if (strcasecmp(ua->argk[i], _("media")) == 0) {
	 int done = FALSE;
	 for (j=i+1; j<ua->argc; j++) {
            if (strcasecmp(ua->argk[j], _("job")) == 0 && ua->argv[j]) {
	       strncpy(jr.Job, ua->argv[i], MAX_NAME_LENGTH);
	       jr.Job[MAX_NAME_LENGTH-1] = 0;
	       jr.JobId = 0;
	       db_get_job_record(ua->db, &jr);
	       jobid = jr.JobId;
            } else if (strcasecmp(ua->argk[j], _("jobid")) == 0 && ua->argv[j]) {
	       jobid = atoi(ua->argv[j]);
	    } else {
	       continue;
	    }
	    VolumeName = (char *) get_pool_memory(PM_FNAME);
	    n = db_get_job_volume_names(ua->db, jobid, VolumeName);
            bsendmsg(ua, _("Jobid %d used %d Volume(s): %s\n"), jobid, n, VolumeName);
	    free_memory(VolumeName);
	    done = TRUE;
	 }
	 /* if no job or jobid keyword found, then we list all media */
	 if (!done) {
	    if (!get_pool_dbr(ua, &pr)) {
	       return 1;
	    }
	    mr.PoolId = pr.PoolId;
	    db_list_media_records(ua->db, &mr, prtit, ua);
	 }
      } else {
         bsendmsg(ua, _("Unknown list keyword: %s\n"), ua->argk[i]);
      }
   }
   return 1;
}

void do_messages(UAContext *ua, char *cmd)
{
   char msg[2000];
   int mlen; 

   fcntl(fileno(con_fd), F_SETLKW);
   rewind(con_fd);
   while (fgets(msg, sizeof(msg), con_fd)) {
      mlen = strlen(msg);
      ua->UA_sock->msg = (char *) check_pool_memory_size(
	 ua->UA_sock->msg, mlen+1);
      strcpy(ua->UA_sock->msg, msg);
      ua->UA_sock->msglen = mlen;
      bnet_send(ua->UA_sock);
   }
   ftruncate(fileno(con_fd), 0L);
   console_msg_pending = FALSE;
   fcntl(fileno(con_fd), F_UNLCK);
   ua->user_notified_msg_pending = FALSE;
}


int qmessagescmd(UAContext *ua, char *cmd)
{
   if (console_msg_pending && ua->auto_display_messages) {
      do_messages(ua, cmd);
   }
   return 1;
}

int messagescmd(UAContext *ua, char *cmd)
{
   if (console_msg_pending) {
      do_messages(ua, cmd);
   } else {
      bnet_fsend(ua->UA_sock, _("You have no messages.\n"));
   }
   return 1;
}

/*
 * Callback routine for "printing" database file listing
 */
void prtit(void *ctx, char *msg)
{
   UAContext *ua = (UAContext *)ctx;
 
   bnet_fsend(ua->UA_sock, "%s", msg);
}

/* 
 * Format message and send to other end.  

 * If the UA_sock is NULL, it means that there is no user
 * agent, so we are being called from Bacula core. In
 * that case direct the messages to the Job.
 */
void bsendmsg(void *ctx, char *fmt, ...)
{
   va_list arg_ptr;
   UAContext *ua = (UAContext *)ctx;
   BSOCK *bs = ua->UA_sock;
   int maxlen, len;
   char *msg;

   if (bs) {
      msg = bs->msg;
   } else {
      msg = (char *)get_pool_memory(PM_EMSG);
   }

again:
   maxlen = sizeof_pool_memory(msg) - 1;
   va_start(arg_ptr, fmt);
   len = bvsnprintf(msg, maxlen, fmt, arg_ptr);
   va_end(arg_ptr);
   if (len < 0 || len >= maxlen) {
      msg = (char *) realloc_pool_memory(msg, maxlen + 200);
      goto again;
   }

   if (bs) {
      bs->msglen = len;
      bnet_send(bs);
   } else {			      /* No UA, send to Job */
      Jmsg(ua->jcr, M_INFO, 0, msg);
      free_memory(msg);
   }

}
