/*
 *
 *   Bacula Director -- Run Command
 *
 *     Kern Sibbald, December MMI
 *
 *   Version $Id$
 */

/*
   Copyright (C) 2001, 2002 Kern Sibbald and John Walker

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
extern struct s_jl joblevels[];
extern struct s_kw ReplaceOptions[];

/*
 * For Backup and Verify Jobs
 *     run [job=]<job-name> level=<level-name>
 *
 * For Restore Jobs
 *     run <job-name> jobid=nn
 *
 */
int runcmd(UAContext *ua, char *cmd)
{
   JCR *jcr;
   char *job_name, *level_name, *jid, *store_name;
   char *where, *fileset_name, *client_name, *bootstrap, *replace;
   int i, j, found, opt;
   JOB *job = NULL;
   STORE *store = NULL;
   CLIENT *client = NULL;
   FILESET *fileset = NULL;
   static char *kw[] = {
      N_("job"),
      N_("jobid"),
      N_("client"),
      N_("fileset"),
      N_("level"),
      N_("storage"),
      N_("where"),
      N_("bootstrap"),
      N_("replace"),
      NULL};

   if (!open_db(ua)) {
      return 1;
   }

   job_name = NULL;
   level_name = NULL;
   jid = NULL;
   store_name = NULL;
   where = NULL;
   client_name = NULL;
   fileset_name = NULL;
   bootstrap = NULL;
   replace = NULL;

   for (i=1; i<ua->argc; i++) {
      found = False;
      Dmsg2(200, "Doing arg %d = %s\n", i, ua->argk[i]);
      for (j=0; !found && kw[j]; j++) {
	 if (strcasecmp(ua->argk[i], _(kw[j])) == 0) {
	    if (!ua->argv[i]) {
               bsendmsg(ua, _("Value missing for keyword %s\n"), ua->argk[i]);
	       return 1;
	    }
            Dmsg1(200, "Got keyword=%s\n", kw[j]);
	    switch (j) {
	       case 0: /* job */
		  if (job_name) {
                     bsendmsg(ua, _("Job name specified twice.\n"));
		     return 1;
		  }
		  job_name = ua->argv[i];
		  found = True;
		  break;
	       case 1: /* JobId */
		  if (jid) {
                     bsendmsg(ua, _("JobId specified twice.\n"));
		     return 1;
		  }
		  jid = ua->argv[i];
		  found = True;
		  break;
	       case 2: /* client */
		  if (client_name) {
                     bsendmsg(ua, _("Client specified twice.\n"));
		     return 1;
		  }
		  client_name = ua->argv[i];
		  found = True;
		  break;
	       case 3: /* fileset */
		  if (fileset_name) {
                     bsendmsg(ua, _("FileSet specified twice.\n"));
		     return 1;
		  }
		  fileset_name = ua->argv[i];
		  found = True;
		  break;
	       case 4: /* level */
		  if (level_name) {
                     bsendmsg(ua, _("Level specified twice.\n"));
		     return 1;
		  }
		  level_name = ua->argv[i];
		  found = True;
		  break;
	       case 5: /* storage */
		  if (store_name) {
                     bsendmsg(ua, _("Storage specified twice.\n"));
		     return 1;
		  }
		  store_name = ua->argv[i];
		  found = True;
		  break;
	       case 6: /* where */
		  if (where) {
                     bsendmsg(ua, _("Where specified twice.\n"));
		     return 1;
		  }
		  where = ua->argv[i];
		  found = True;
		  break;
	       case 7: /* bootstrap */
		  if (bootstrap) {
                     bsendmsg(ua, _("Bootstrap specified twice.\n"));
		     return 1;
		  }
		  bootstrap = ua->argv[i];
		  found = True;
		  break;
	       case 8: /* replace */
		  if (replace) {
                     bsendmsg(ua, _("Replace specified twice.\n"));
		     return 1;
		  }
		  replace = ua->argv[i];
		  found = True;
		  break;
	       default:
		  break;
	    }
	 } /* end strcase compare */
      } /* end keyword loop */
      if (!found) {
         Dmsg1(200, "%s not found\n", ua->argk[i]);
	 /*
	  * Special case for Job Name, it can be the first
	  * keyword that has no value.
	  */
	 if (!job_name && !ua->argv[i]) {
	    job_name = ua->argk[i];   /* use keyword as job name */
            Dmsg1(200, "Set jobname=%s\n", job_name);
	 } else {
            bsendmsg(ua, _("Invalid keyword: %s\n"), ua->argk[i]);
	    return 1;
	 }
      }
   } /* end argc loop */
	     
   Dmsg0(200, "Done scan.\n");



   if (job_name) {
      /* Find Job */
      job = (JOB *)GetResWithName(R_JOB, job_name);
      if (!job) {
         bsendmsg(ua, _("Job \"%s\" not found\n"), job_name);
	 job = select_job_resource(ua);
      } else {
         Dmsg1(200, "Found job=%s\n", job_name);
      }
   } else {
      bsendmsg(ua, _("A job name must be specified.\n"));
      job = select_job_resource(ua);
   }
   if (!job) {
      return 1;
   }

   if (store_name) {
      store = (STORE *)GetResWithName(R_STORAGE, store_name);
      if (!store) {
         bsendmsg(ua, _("Storage \"%s\" not found.\n"), store_name);
	 store = select_storage_resource(ua);
      }
   } else {
      store = job->storage;	      /* use default */
   }
   if (!store) {
      return 1;
   }

   if (client_name) {
      client = (CLIENT *)GetResWithName(R_CLIENT, client_name);
      if (!client) {
         bsendmsg(ua, _("Client \"%s\" not found.\n"), client_name);
	 client = select_client_resource(ua);
      }
   } else {
      client = job->client;	      /* use default */
   }
   if (!client) {
      return 1;
   }

   if (fileset_name) {
      fileset = (FILESET *)GetResWithName(R_FILESET, fileset_name);
      if (!fileset) {
         bsendmsg(ua, _("FileSet \"%s\" not found.\n"), fileset_name);
	 fileset = select_fileset_resource(ua);
      }
   } else {
      fileset = job->fileset;		/* use default */
   }
   if (!fileset) {
      return 1;
   }


   /* Create JCR to run job */
   jcr = new_jcr(sizeof(JCR), dird_free_jcr);
   set_jcr_defaults(jcr, job);

   jcr->store = store;
   jcr->client = client;
   jcr->fileset = fileset;
   if (where) {
      if (jcr->RestoreWhere) {
	 free(jcr->RestoreWhere);
      }
      jcr->RestoreWhere = bstrdup(where);
   }
   if (bootstrap) {
      if (jcr->RestoreBootstrap) {
	 free(jcr->RestoreBootstrap);
      }
      jcr->RestoreBootstrap = bstrdup(bootstrap);
   }

   if (replace) {
      jcr->replace = 0;
      for (i=0; ReplaceOptions[i].name; i++) {
	 if (strcasecmp(replace, ReplaceOptions[i].name) == 0) {
	    jcr->replace = ReplaceOptions[i].token;
	 }
      }
      if (!jcr->replace) {
         bsendmsg(ua, _("Invalid replace option: %s\n"), replace);
	 return 0;
      }
   } else if (job->replace) {
      jcr->replace = job->replace;
   } else {
      jcr->replace = REPLACE_ALWAYS;
   }

try_again:
   replace = ReplaceOptions[0].name;
   for (i=0; ReplaceOptions[i].name; i++) {
      if (ReplaceOptions[i].token == jcr->replace) {
	 replace = ReplaceOptions[i].name;
      }
   }
   Dmsg1(20, "JobType=%c\n", jcr->JobType);
   switch (jcr->JobType) {
      char ec1[30];
      case JT_ADMIN:
         bsendmsg(ua, _("Run %s job\n\
JobName:  %s\n\
FileSet:  %s\n\
Client:   %s\n\
Storage:  %s\n"),
                 _("Admin"),
		 job->hdr.name,
		 jcr->fileset->hdr.name,
		 NPRT(jcr->client->hdr.name),
		 NPRT(jcr->store->hdr.name));
	 break;
	 break;
      case JT_BACKUP:
      case JT_VERIFY:
	 if (level_name) {
	    /* Look up level name and pull code */
	    found = 0;
	    for (i=0; joblevels[i].level_name; i++) {
	       if (strcasecmp(level_name, _(joblevels[i].level_name)) == 0) {
		  jcr->JobLevel = joblevels[i].level;
		  found = 1;
		  break;
	       }
	    }
	    if (!found) { 
               bsendmsg(ua, _("Level %s not valid.\n"), level_name);
	       free_jcr(jcr);
	       return 1;
	    }
	 }
	 level_name = NULL;
         bsendmsg(ua, _("Run %s job\n\
JobName:  %s\n\
FileSet:  %s\n\
Level:    %s\n\
Client:   %s\n\
Storage:  %s\n"),
                 jcr->JobType==JT_BACKUP?_("Backup"):_("Verify"),
		 job->hdr.name,
		 jcr->fileset->hdr.name,
		 level_to_str(jcr->JobLevel),
		 jcr->client->hdr.name,
		 jcr->store->hdr.name);
	 break;
      case JT_RESTORE:
	 if (jcr->RestoreJobId == 0 && !jcr->RestoreBootstrap) {
	    if (jid) {
	       jcr->RestoreJobId = atoi(jid);
	    } else {
               if (!get_cmd(ua, _("Please enter a JobId for restore: "))) {
		  free_jcr(jcr);
		  return 1;
	       }  
	       jcr->RestoreJobId = atoi(ua->cmd);
	    }
	 }
         jcr->JobLevel = 'F';         /* default level */
         Dmsg1(20, "JobId to restore=%d\n", jcr->RestoreJobId);
         bsendmsg(ua, _("Run Restore job\n\
JobName:    %s\n\
Bootstrap:  %s\n\
Where:      %s\n\
Replace:    %s\n\
FileSet:    %s\n\
Client:     %s\n\
Storage:    %s\n\
JobId:      %s\n"),
		 job->hdr.name,
		 NPRT(jcr->RestoreBootstrap),
		 jcr->RestoreWhere?jcr->RestoreWhere:NPRT(job->RestoreWhere),
		 replace,
		 jcr->fileset->hdr.name,
		 jcr->client->hdr.name,
		 jcr->store->hdr.name, 
                 jcr->RestoreJobId==0?"*None*":edit_uint64(jcr->RestoreJobId, ec1));
	 break;
      default:
         bsendmsg(ua, _("Unknown Job Type=%d\n"), jcr->JobType);
	 free_jcr(jcr);
	 return 0;
   }
   if (!get_cmd(ua, _("OK to run? (yes/mod/no): "))) {
      free_jcr(jcr);
      return 0; 		      /* do not run */
   }
   /*
    * At user request modify parameters of job to be run.
    */
   if (strlen(ua->cmd) == 0) {
      bsendmsg(ua, _("Job not run.\n"));
      free_jcr(jcr);
      return 0; 		      /* do not run */
   }
   if (strncasecmp(ua->cmd, _("mod"), strlen(ua->cmd)) == 0) {
      FILE *fd;

      start_prompt(ua, _("Parameters to modify:\n"));
      add_prompt(ua, _("Level"));            /* 0 */
      add_prompt(ua, _("Storage"));          /* 1 */
      add_prompt(ua, _("Job"));              /* 2 */
      add_prompt(ua, _("FileSet"));          /* 3 */
      add_prompt(ua, _("Client"));           /* 4 */
      if (jcr->JobType == JT_RESTORE) {
         add_prompt(ua, _("Bootstrap"));     /* 5 */
         add_prompt(ua, _("Where"));         /* 6 */
         add_prompt(ua, _("Replace"));       /* 7 */
         add_prompt(ua, _("JobId"));         /* 8 */
      }
      switch (do_prompt(ua, _("Select parameter to modify"), NULL, 0)) {
      case 0:
	 /* Level */
	 if (jcr->JobType == JT_BACKUP) {
            start_prompt(ua, _("Levels:\n"));
            add_prompt(ua, _("Full"));
            add_prompt(ua, _("Incremental"));
            add_prompt(ua, _("Differential"));
            add_prompt(ua, _("Since"));
            switch (do_prompt(ua, _("Select level"), NULL, 0)) {
	    case 0:
	       jcr->JobLevel = L_FULL;
	       break;
	    case 1:
	       jcr->JobLevel = L_INCREMENTAL;
	       break;
	    case 2:
	       jcr->JobLevel = L_DIFFERENTIAL;
	       break;
	    case 3:
	       jcr->JobLevel = L_SINCE;
	       break;
	    default:
	       break;
	    }
	    goto try_again;
	 } else if (jcr->JobType == JT_VERIFY) {
            start_prompt(ua, _("Levels:\n"));
            add_prompt(ua, _("Initialize Catalog"));
            add_prompt(ua, _("Verify Catalog"));
            add_prompt(ua, _("Verify Volume"));
            add_prompt(ua, _("Verify Volume Data"));
            switch (do_prompt(ua, _("Select level"), NULL, 0)) {
	    case 0:
	       jcr->JobLevel = L_VERIFY_INIT;
	       break;
	    case 1:
	       jcr->JobLevel = L_VERIFY_CATALOG;
	       break;
	    case 2:
	       jcr->JobLevel = L_VERIFY_VOLUME_TO_CATALOG;
	       break;
	    case 3:
	       jcr->JobLevel = L_VERIFY_DATA;
	       break;
	    default:
	       break;
	    }
	    goto try_again;
	 } else {
            bsendmsg(ua, _("Level not appropriate for this Job. Cannot be changed.\n"));
	 }
	 goto try_again;
      case 1:
	 store = select_storage_resource(ua);
	 if (store) {
	    jcr->store = store;
	    goto try_again;
	 }
	 break;
      case 2:
	 /* Job */
	 job = select_job_resource(ua);
	 if (job) {
	    jcr->job = job;
	    set_jcr_defaults(jcr, job);
	    goto try_again;
	 }
	 break;
      case 3:
	 /* FileSet */
	 fileset = select_fileset_resource(ua);
	 if (fileset) {
	    jcr->fileset = fileset;
	    goto try_again;
	 }	
	 break;
      case 4:
	 client = select_client_resource(ua);
	 if (client) {
	    jcr->client = client;
	    goto try_again;
	 }
	 break;
      case 5:
	 /* Bootstrap */
         if (!get_cmd(ua, _("Please enter the Bootstrap file name: "))) {
	    break;
	 }
	 if (jcr->RestoreBootstrap) {
	    free(jcr->RestoreBootstrap);
	    jcr->RestoreBootstrap = NULL;
	 }
	 if (ua->cmd[0] != 0) {
	    jcr->RestoreBootstrap = bstrdup(ua->cmd);
            fd = fopen(jcr->RestoreBootstrap, "r");
	    if (!fd) {
               bsendmsg(ua, _("Warning cannot open %s: ERR=%s\n"),
		  jcr->RestoreBootstrap, strerror(errno));
	       free(jcr->RestoreBootstrap);
	       jcr->RestoreBootstrap = NULL;
	    } else {
	       fclose(fd);
	    }
	 }
	 goto try_again;
      case 6:
	 /* Where */
         if (!get_cmd(ua, _("Please enter path prefix for restore (/ for none): "))) {
	    break;
	 }
	 if (jcr->RestoreWhere) {
	    free(jcr->RestoreWhere);
	    jcr->RestoreWhere = NULL;
	 }
         if (ua->cmd[0] == '/' && ua->cmd[1] == 0) {
	    ua->cmd[0] = 0;
	 }
	 jcr->RestoreWhere = bstrdup(ua->cmd);
	 goto try_again;
      case 7:
	 /* Replace */
         start_prompt(ua, _("Replace:\n"));
	 for (i=0; ReplaceOptions[i].name; i++) {
	    add_prompt(ua, ReplaceOptions[i].name);
	 }
         opt = do_prompt(ua, _("Select replace option"), NULL, 0);
	 if (opt >=  0) {
	    jcr->replace = ReplaceOptions[opt].token;
	 }
	 goto try_again;
      case 8:
	 /* JobId */
	 jid = NULL;		      /* force reprompt */
	 jcr->RestoreJobId = 0;
	 if (jcr->RestoreBootstrap) {
            bsendmsg(ua, _("You must set the bootstrap file to NULL to be able to specify a JobId.\n"));
	 }
	 goto try_again;
      default: 
	 goto try_again;
      }
      bsendmsg(ua, _("Job not run.\n"));
      free_jcr(jcr);
      return 0; 		      /* error do no run Job */
   }
   if (strncasecmp(ua->cmd, _("yes"), strlen(ua->cmd)) == 0) {
      Dmsg1(200, "Calling run_job job=%x\n", jcr->job);
      run_job(jcr);
      return 1;
   }

   bsendmsg(ua, _("Job not run.\n"));
   free_jcr(jcr);
   return 0;			   /* do not run */

}
