/*
 *
 *   Bacula Director -- admin.c -- responsible for doing admin jobs
 *
 *     Kern Sibbald, May MMIII
 *
 *  Basic tasks done here:
 *     Display the job report.
 *
 *   Version $Id$
 */

/*
   Copyright (C) 2003-2006 Kern Sibbald

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   version 2 as amended with additional clauses defined in the
   file LICENSE in the main source directory.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the 
   the file LICENSE for additional details.

 */

#include "bacula.h"
#include "dird.h"
#include "ua.h"


bool do_admin_init(JCR *jcr)
{
   free_rstorage(jcr);
   return true;
}

/*
 *  Returns:  false on failure
 *            true  on success
 */
bool do_admin(JCR *jcr)
{

   jcr->jr.JobId = jcr->JobId;

   jcr->fname = (char *)get_pool_memory(PM_FNAME);

   /* Print Job Start message */
   Jmsg(jcr, M_INFO, 0, _("Start Admin JobId %d, Job=%s\n"),
        jcr->JobId, jcr->Job);

   set_jcr_job_status(jcr, JS_Running);
   admin_cleanup(jcr, JS_Terminated);
   return true;
}


/*
 * Release resources allocated during backup.
 */
void admin_cleanup(JCR *jcr, int TermCode)
{
   char sdt[50], edt[50];
   char term_code[100];
   const char *term_msg;
   int msg_type;
   MEDIA_DBR mr;

   Dmsg0(100, "Enter backup_cleanup()\n");
   memset(&mr, 0, sizeof(mr));
   set_jcr_job_status(jcr, TermCode);

   update_job_end_record(jcr);        /* update database */

   if (!db_get_job_record(jcr, jcr->db, &jcr->jr)) {
      Jmsg(jcr, M_WARNING, 0, _("Error getting job record for stats: %s"),
         db_strerror(jcr->db));
      set_jcr_job_status(jcr, JS_ErrorTerminated);
   }

   msg_type = M_INFO;                 /* by default INFO message */
   switch (jcr->JobStatus) {
   case JS_Terminated:
      term_msg = _("Admin OK");
      break;
   case JS_FatalError:
   case JS_ErrorTerminated:
      term_msg = _("*** Admin Error ***");
      msg_type = M_ERROR;          /* Generate error message */
      break;
   case JS_Canceled:
      term_msg = _("Admin Canceled");
      break;
   default:
      term_msg = term_code;
      sprintf(term_code, _("Inappropriate term code: %c\n"), jcr->JobStatus);
      break;
   }
   bstrftime(sdt, sizeof(sdt), jcr->jr.StartTime);
   bstrftime(edt, sizeof(edt), jcr->jr.EndTime);

   Jmsg(jcr, msg_type, 0, _("Bacula " VERSION " (" LSMDATE "): %s\n"
"  JobId:                  %d\n"
"  Job:                    %s\n"
"  Start time:             %s\n"
"  End time:               %s\n"
"  Termination:            %s\n\n"),
        edt,
        jcr->jr.JobId,
        jcr->jr.Job,
        sdt,
        edt,
        term_msg);

   Dmsg0(100, "Leave admin_cleanup()\n");
}
