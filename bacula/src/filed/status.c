/*
 *  Bacula File Daemon Status routines
 *
 *    Kern Sibbald, August MMI
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
#include "filed.h"

extern char my_name[];
extern struct s_last_job last_job;
extern time_t daemon_start_time;

/*
 * General status generator
 */
static void do_status(void sendit(char *msg, int len, void *sarg), void *arg) 
{
   int sec, bps;
   char *msg, b1[32], b2[32], b3[32];
   int found, len;
   JCR *njcr;
   char dt[MAX_TIME_LENGTH];

   msg = (char *)get_pool_memory(PM_MESSAGE);
   found = 0;
   len = Mmsg(&msg, "%s Version: " VERSION " (" BDATE ")\n", my_name);
   sendit(msg, len, arg);
   bstrftime(dt, sizeof(dt), daemon_start_time);
   len = Mmsg(&msg, _("Daemon started %s, %d Job%s run.\n"), dt, last_job.NumJobs,
        last_job.NumJobs == 1 ? "" : "s");
   sendit(msg, len, arg);
   if (last_job.NumJobs > 0) {
      char termstat[30];

      bstrftime(dt, sizeof(dt), last_job.end_time);
      len = Mmsg(&msg, _("Last Job %s finished at %s\n"), last_job.Job, dt);
      sendit(msg, len, arg);

      jobstatus_to_ascii(last_job.JobStatus, termstat, sizeof(termstat));
      len = Mmsg(&msg, _("  Files=%s Bytes=%s Termination Status=%s\n"), 
	   edit_uint64_with_commas(last_job.JobFiles, b1),
	   edit_uint64_with_commas(last_job.JobBytes, b2),
	   termstat);
      sendit(msg, len, arg);
   }
   lock_jcr_chain();
   for (njcr=NULL; (njcr=get_next_jcr(njcr)); ) {
      bstrftime(dt, sizeof(dt), njcr->start_time);
      if (njcr->JobId == 0) {
         len = Mmsg(&msg, _("Director connected at: %s\n"), dt);
      } else {
         len = Mmsg(&msg, _("JobId %d Job %s is running.\n"), 
		    njcr->JobId, njcr->Job);
	 sendit(msg, len, arg);
         len = Mmsg(&msg, _("    %s Job started: %s\n"), 
		    job_type_to_str(njcr->JobType), dt);
      }
      sendit(msg, len, arg);
      if (njcr->JobId == 0) {
	 free_locked_jcr(njcr);
	 continue;
      }
      sec = time(NULL) - njcr->start_time;
      if (sec <= 0) {
	 sec = 1;
      }
      bps = njcr->JobBytes / sec;
      len = Mmsg(&msg,  _("    Files=%s Bytes=%s Bytes/sec=%s\n"), 
	   edit_uint64_with_commas(njcr->JobFiles, b1),
	   edit_uint64_with_commas(njcr->JobBytes, b2),
	   edit_uint64_with_commas(bps, b3));
      sendit(msg, len, arg);
      len = Mmsg(&msg, _("    Files Examined=%s\n"), 
	   edit_uint64_with_commas(njcr->num_files_examined, b1));
      sendit(msg, len, arg);
      if (njcr->JobFiles > 0) {
	 P(njcr->mutex);
         len = Mmsg(&msg, _("    Processing file: %s\n"), njcr->last_fname);
	 V(njcr->mutex);
	 sendit(msg, len, arg);
      }

      found = 1;
      if (njcr->store_bsock) {
         len = Mmsg(&msg, "    SDReadSeqNo=%" lld " fd=%d\n",
	     njcr->store_bsock->read_seqno, njcr->store_bsock->fd);
	 sendit(msg, len, arg);
      } else {
         len = Mmsg(&msg, _("    SDSocket closed.\n"));
	 sendit(msg, len, arg);
      }
      free_locked_jcr(njcr);
   }
   unlock_jcr_chain();
   if (!found) {
      len = Mmsg(&msg, _("No jobs running.\n"));
      sendit(msg, len, arg);
   }
   free_pool_memory(msg);
}

/*
 * Send to Director 
 */
static void sendit(char *msg, int len, void *arg)
{
   BSOCK *user = (BSOCK *)arg;

   memcpy(user->msg, msg, len+1);
   user->msglen = len+1;
   bnet_send(user);
}
				   
/*
 * Status command from Director
 */
int status_cmd(JCR *jcr)
{
   BSOCK *user = jcr->dir_bsock;

   bnet_fsend(user, "\n");
   do_status(sendit, (void *)user);
   bnet_fsend(user, "====\n");

   bnet_sig(user, BNET_EOD);
   return 1;
}


#ifdef HAVE_CYGWIN
#include <windows.h>

static char buf[100];
int bacstat = 0;

struct s_win32_arg {
   HWND hwnd;
   int idlist;
};

/*
 * Put message in Window List Box
 */
static void win32_sendit(char *msg, int len, void *marg)
{
   struct s_win32_arg *arg = (struct s_win32_arg *)marg;

   if (len > 0) {
      msg[len-1] = 0;		      /* eliminate newline */
   }
   SendDlgItemMessage(arg->hwnd, arg->idlist, LB_ADDSTRING, 0, (LONG)msg);
   
}

void FillStatusBox(HWND hwnd, int idlist)
{
   struct s_win32_arg arg;

   arg.hwnd = hwnd;
   arg.idlist = idlist;

   /* Empty box */
   for ( ; SendDlgItemMessage(hwnd, idlist, LB_DELETESTRING, 0, (LONG)0) > 0; )
      { }
   do_status(win32_sendit, (void *)&arg);
}

char *bac_status(int stat)
{
   JCR *njcr;
   char *termstat = _("Bacula Idle");

   bacstat = 0;
   if (last_job.NumJobs > 0) {
      switch (last_job.JobStatus) {
	case JS_ErrorTerminated:
	    bacstat = -1;
            termstat = _("Last Job Erred");
	    break;
	default:
	    break;
      }
   }
   lock_jcr_chain();
   for (njcr=NULL; (njcr=get_next_jcr(njcr)); ) {
      if (njcr->JobId != 0) {
	 bacstat = 1;
         termstat = _("Bacula Running");
	 free_locked_jcr(njcr);
	 break;
      }
      free_locked_jcr(njcr);
   }
   unlock_jcr_chain();
   strcpy(buf, termstat);
   return buf;
}

#endif /* HAVE_CYGWIN */
