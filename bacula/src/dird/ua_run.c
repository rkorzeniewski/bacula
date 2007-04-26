/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2001-2007 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from
   many others, a complete list can be found in the file AUTHORS.
   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version two of the GNU General Public
   License as published by the Free Software Foundation plus additions
   that are listed in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

   Bacula® is a registered trademark of John Walker.
   The licensor of Bacula is the Free Software Foundation Europe
   (FSFE), Fiduciary Program, Sumatrastrasse 25, 8006 Zürich,
   Switzerland, email:ftf@fsfeurope.org.
*/
/*
 *
 *   Bacula Director -- Run Command
 *
 *     Kern Sibbald, December MMI
 *
 *   Version $Id$
 */

#include "bacula.h"
#include "dird.h"

/* Forward referenced subroutines */
static void select_job_level(UAContext *ua, JCR *jcr);
static bool display_job_parameters(UAContext *ua, JCR *jcr, JOB *job, 
                char *verify_list, char *jid, const char *replace,
                char *client_name);
static void select_where_regexp(UAContext *ua, JCR *jcr);


/* Imported variables */
extern struct s_kw ReplaceOptions[];

/*
 * For Backup and Verify Jobs
 *     run [job=]<job-name> level=<level-name>
 *
 * For Restore Jobs
 *     run <job-name> 
 *
 *  Returns: 0 on error
 *           JobId if OK
 *
 */
int run_cmd(UAContext *ua, const char *cmd)
{
   JCR *jcr;
   char *job_name, *level_name, *jid, *store_name, *pool_name;
   char *where, *fileset_name, *client_name, *bootstrap, *regexwhere;
   char *restore_client_name;
   const char *replace;
   char *when, *verify_job_name, *catalog_name;
   char *previous_job_name;
   char *since = NULL;
   char *verify_list;
   bool cloned = false;
   int Priority = 0;
   int i, j, opt, files = 0;
   bool kw_ok;
   JOB *job = NULL;
   JOB *verify_job = NULL;
   JOB *previous_job = NULL;
   USTORE store;
   CLIENT *client = NULL;
   FILESET *fileset = NULL;
   POOL *pool = NULL;
   static const char *kw[] = {        /* command line arguments */
      "job",                          /*  Used in a switch() */
      "jobid",                        /* 1 */
      "client",                       /* 2 */
      "fd",
      "fileset",                      /* 4 */
      "level",                        /* 5 */
      "storage",                      /* 6 */
      "sd",                           /* 7 */
      "regexwhere",                   /* 8 where string as a bregexp */
      "where",                        /* 9 */
      "bootstrap",                    /* 10 */
      "replace",                      /* 11 */
      "when",                         /* 12 */
      "priority",                     /* 13 */
      "yes",          /* 14  -- if you change this change YES_POS too */
      "verifyjob",                    /* 15 */
      "files",                        /* 16 number of files to restore */
      "catalog",                      /* 17 override catalog */
      "since",                        /* 18 since */
      "cloned",                       /* 19 cloned */
      "verifylist",                   /* 20 verify output list */
      "migrationjob",                 /* 21 migration job name */
      "pool",                         /* 22 */
      "backupclient",                 /* 23 */
      "restoreclient",                /* 24 */
      NULL};

#define YES_POS 14

   if (!open_client_db(ua)) {
      return 1;
   }

   job_name = NULL;
   level_name = NULL;
   jid = NULL;
   store_name = NULL;
   pool_name = NULL;
   where = NULL;
   regexwhere = NULL;
   when = NULL;
   client_name = NULL;
   restore_client_name = NULL;
   fileset_name = NULL;
   bootstrap = NULL;
   replace = NULL;
   verify_job_name = NULL;
   previous_job_name = NULL;
   catalog_name = NULL;
   verify_list = NULL;

   for (i=1; i<ua->argc; i++) {
      Dmsg2(800, "Doing arg %d = %s\n", i, ua->argk[i]);
      kw_ok = false;
      /* Keep looking until we find a good keyword */
      for (j=0; !kw_ok && kw[j]; j++) {
         if (strcasecmp(ua->argk[i], kw[j]) == 0) {
            /* Note, yes and run have no value, so do not fail */
            if (!ua->argv[i] && j != YES_POS /*yes*/) {
               ua->send_msg(_("Value missing for keyword %s\n"), ua->argk[i]);
               return 1;
            }
            Dmsg1(800, "Got keyword=%s\n", NPRT(kw[j]));
            switch (j) {
            case 0: /* job */
               if (job_name) {
                  ua->send_msg(_("Job name specified twice.\n"));
                  return 0;
               }
               job_name = ua->argv[i];
               kw_ok = true;
               break;
            case 1: /* JobId */
               if (jid) {
                  ua->send_msg(_("JobId specified twice.\n"));
                  return 0;
               }
               jid = ua->argv[i];
               kw_ok = true;
               break;
            case 2: /* client */
            case 3: /* fd */
               if (client_name) {
                  ua->send_msg(_("Client specified twice.\n"));
                  return 0;
               }
               client_name = ua->argv[i];
               kw_ok = true;
               break;
            case 4: /* fileset */
               if (fileset_name) {
                  ua->send_msg(_("FileSet specified twice.\n"));
                  return 0;
               }
               fileset_name = ua->argv[i];
               kw_ok = true;
               break;
            case 5: /* level */
               if (level_name) {
                  ua->send_msg(_("Level specified twice.\n"));
                  return 0;
               }
               level_name = ua->argv[i];
               kw_ok = true;
               break;
            case 6: /* storage */
            case 7: /* sd */
               if (store_name) {
                  ua->send_msg(_("Storage specified twice.\n"));
                  return 0;
               }
               store_name = ua->argv[i];
               kw_ok = true;
               break;
            case 8: /* regexwhere */
                if (regexwhere || where) {
                  ua->send_msg(_("RegexWhere or Where specified twice.\n"));
                  return 0;
               }
               regexwhere = ua->argv[i];
               if (!acl_access_ok(ua, Where_ACL, regexwhere)) {
                  ua->send_msg(_("Forbidden \"regexwhere\" specified.\n"));
                  return 0;
               }
               kw_ok = true;
               break;
           case 9: /* where */
               if (where || regexwhere) {
                  ua->send_msg(_("Where or RegexWhere specified twice.\n"));
                  return 0;
               }
               where = ua->argv[i];
               if (!acl_access_ok(ua, Where_ACL, where)) {
                  ua->send_msg(_("Forbidden \"where\" specified.\n"));
                  return 0;
               }
               kw_ok = true;
               break;
            case 10: /* bootstrap */
               if (bootstrap) {
                  ua->send_msg(_("Bootstrap specified twice.\n"));
                  return 0;
               }
               bootstrap = ua->argv[i];
               kw_ok = true;
               break;
            case 11: /* replace */
               if (replace) {
                  ua->send_msg(_("Replace specified twice.\n"));
                  return 0;
               }
               replace = ua->argv[i];
               kw_ok = true;
               break;
            case 12: /* When */
               if (when) {
                  ua->send_msg(_("When specified twice.\n"));
                  return 0;
               }
               when = ua->argv[i];
               kw_ok = true;
               break;
            case 13:  /* Priority */
               if (Priority) {
                  ua->send_msg(_("Priority specified twice.\n"));
                  return 0;
               }
               Priority = atoi(ua->argv[i]);
               if (Priority <= 0) {
                  ua->send_msg(_("Priority must be positive nonzero setting it to 10.\n"));
                  Priority = 10;
               }
               kw_ok = true;
               break;
            case 14: /* yes */
               kw_ok = true;
               break;
            case 15: /* Verify Job */
               if (verify_job_name) {
                  ua->send_msg(_("Verify Job specified twice.\n"));
                  return 0;
               }
               verify_job_name = ua->argv[i];
               kw_ok = true;
               break;
            case 16: /* files */
               files = atoi(ua->argv[i]);
               kw_ok = true;
               break;

            case 17: /* catalog */
               catalog_name = ua->argv[i];
               kw_ok = true;
               break;

            case 18: /* since */
               since = ua->argv[i];
               kw_ok = true; 
               break;

            case 19: /* cloned */
               cloned = true;
               kw_ok = true;
               break;

            case 20: /* write verify list output */
               verify_list = ua->argv[i];
               kw_ok = true;
               break;
            case 21: /* Migration Job */
               if (previous_job_name) {
                  ua->send_msg(_("Migration Job specified twice.\n"));
                  return 0;
               }
               previous_job_name = ua->argv[i];
               kw_ok = true;
               break;
            case 22: /* pool */
               if (pool_name) {
                  ua->send_msg(_("Pool specified twice.\n"));
                  return 0;
               }
               pool_name = ua->argv[i];
               kw_ok = true;
               break;
            case 23: /* backupclient */
               if (client_name) {
                  ua->send_msg(_("Client specified twice.\n"));
                  return 0;
               }
               client_name = ua->argv[i];
               kw_ok = true;
               break;
            case 24: /* restoreclient */
               if (restore_client_name) {
                  ua->send_msg(_("Restore Client specified twice.\n"));
                  return 0;
               }
               restore_client_name = ua->argv[i];
               kw_ok = true;
               break;
            default:
               break;
            }
         } /* end strcase compare */
      } /* end keyword loop */
      /*
       * End of keyword for loop -- if not found, we got a bogus keyword
       */
      if (!kw_ok) {
         Dmsg1(800, "%s not found\n", ua->argk[i]);
         /*
          * Special case for Job Name, it can be the first
          * keyword that has no value.
          */
         if (!job_name && !ua->argv[i]) {
            job_name = ua->argk[i];   /* use keyword as job name */
            Dmsg1(800, "Set jobname=%s\n", job_name);
         } else {
            ua->send_msg(_("Invalid keyword: %s\n"), ua->argk[i]);
            return 0;
         }
      }
   } /* end argc loop */
             
   Dmsg0(800, "Done scan.\n");

   CAT *catalog = NULL;
   if (catalog_name != NULL) {
       catalog = (CAT *)GetResWithName(R_CATALOG, catalog_name);
       if (catalog == NULL) {
            ua->error_msg(_("Catalog \"%s\" not found\n"), catalog_name);
           return 0;
       }
       if (!acl_access_ok(ua, Catalog_ACL, catalog->name())) {
          ua->error_msg(_("No authorization. Catalog \"%s\".\n"), catalog->name());
          return 0;
       }
   }
   Dmsg1(800, "Using catalog=%s\n", NPRT(catalog_name));

   if (job_name) {
      /* Find Job */
      job = (JOB *)GetResWithName(R_JOB, job_name);
      if (!job) {
         if (*job_name != 0) {
            ua->send_msg(_("Job \"%s\" not found\n"), job_name);
         }
         job = select_job_resource(ua);
      } else {
         Dmsg1(800, "Found job=%s\n", job_name);
      }
   } else {
      ua->send_msg(_("A job name must be specified.\n"));
      job = select_job_resource(ua);
   }
   if (!job) {
      return 0;
   } else if (!acl_access_ok(ua, Job_ACL, job->name())) {
      ua->error_msg( _("No authorization. Job \"%s\".\n"),
         job->name());
      return 0;
   }

   if (pool_name) {
      pool = (POOL *)GetResWithName(R_POOL, pool_name);
      if (!pool) {
         if (*pool_name != 0) {
            ua->warning_msg(_("Pool \"%s\" not found.\n"), pool_name);
         }
         pool = select_pool_resource(ua);
      }
   } else {
      pool = job->pool;             /* use default */
   }
   if (!pool) {
      return 0;
   } else if (!acl_access_ok(ua, Pool_ACL, pool->name())) {
      ua->error_msg(_("No authorization. Pool \"%s\".\n"),
               pool->name());
      return 0;
   }
   Dmsg1(100, "Using pool %s\n", pool->name());

   if (store_name) {
      store.store = (STORE *)GetResWithName(R_STORAGE, store_name);
      pm_strcpy(store.store_source, _("command line"));
      if (!store.store) {
         if (*store_name != 0) {
            ua->warning_msg(_("Storage \"%s\" not found.\n"), store_name);
         }
         store.store = select_storage_resource(ua);
         pm_strcpy(store.store_source, _("user selection"));
      }
   } else {
      get_job_storage(&store, job, NULL);      /* use default */
   }
   if (!store.store) {
      ua->error_msg(_("No storage specified.\n"));
      return 1;
   } else if (!acl_access_ok(ua, Storage_ACL, store.store->name())) {
      ua->error_msg(_("No authorization. Storage \"%s\".\n"),
               store.store->name());
      return 0;
   }
   Dmsg1(800, "Using storage=%s\n", store.store->name());

   if (client_name) {
      client = (CLIENT *)GetResWithName(R_CLIENT, client_name);
      if (!client) {
         if (*client_name != 0) {
            ua->warning_msg(_("Client \"%s\" not found.\n"), client_name);
         }
         client = select_client_resource(ua);
      }
   } else {
      client = job->client;           /* use default */
   }
   if (!client) {
      return 0;
   } else if (!acl_access_ok(ua, Client_ACL, client->name())) {
      ua->error_msg(_("No authorization. Client \"%s\".\n"),
               client->name());
      return 0;
   }
   Dmsg1(800, "Using client=%s\n", client->name());

   if (restore_client_name) {
      client = (CLIENT *)GetResWithName(R_CLIENT, restore_client_name);
      if (!client) {
         if (*restore_client_name != 0) {
            ua->warning_msg(_("Restore Client \"%s\" not found.\n"), restore_client_name);
         }
         client = select_client_resource(ua);
      }
   } else {
      client = job->client;           /* use default */
   }
   if (!client) {
      return 0;
   } else if (!acl_access_ok(ua, Client_ACL, client->name())) {
      ua->error_msg(_("No authorization. Client \"%s\".\n"),
               client->name());
      return 0;
   }
   Dmsg1(800, "Using restore client=%s\n", client->name());


   if (fileset_name) {
      fileset = (FILESET *)GetResWithName(R_FILESET, fileset_name);
      if (!fileset) {
         ua->send_msg(_("FileSet \"%s\" not found.\n"), fileset_name);
         fileset = select_fileset_resource(ua);
      }
   } else {
      fileset = job->fileset;           /* use default */
   }
   if (!fileset) {
      return 0;
   } else if (!acl_access_ok(ua, FileSet_ACL, fileset->name())) {
      ua->send_msg(_("No authorization. FileSet \"%s\".\n"),
               fileset->name());
      return 0;
   }

   if (verify_job_name) {
      verify_job = (JOB *)GetResWithName(R_JOB, verify_job_name);
      if (!verify_job) {
         ua->send_msg(_("Verify Job \"%s\" not found.\n"), verify_job_name);
         verify_job = select_job_resource(ua);
      }
   } else {
      verify_job = job->verify_job;
   }

   if (previous_job_name) {
      previous_job = (JOB *)GetResWithName(R_JOB, previous_job_name);
      if (!previous_job) {
         ua->send_msg(_("Migration Job \"%s\" not found.\n"), previous_job_name);
         previous_job = select_job_resource(ua);
      }
   } else {
      previous_job = job->verify_job;
   }


   /*
    * Create JCR to run job.  NOTE!!! after this point, free_jcr()
    *  before returning.
    */
   jcr = new_jcr(sizeof(JCR), dird_free_jcr);
   set_jcr_defaults(jcr, job);
   jcr->unlink_bsr = ua->jcr->unlink_bsr;    /* copy unlink flag from caller */
   ua->jcr->unlink_bsr = false;

   jcr->verify_job = verify_job;
   jcr->previous_job = previous_job;
   jcr->pool = pool;
   set_rwstorage(jcr, &store);
   jcr->client = client;
   pm_strcpy(jcr->client_name, client->name());
   jcr->fileset = fileset;
   jcr->ExpectedFiles = files;
   if (catalog != NULL) {
      jcr->catalog = catalog;
   }
   if (where) {
      if (jcr->where) {
         free(jcr->where);
      }
      jcr->where = bstrdup(where);
   }

   if (regexwhere) {
      if (jcr->RegexWhere) {
         free(jcr->RegexWhere);
      }
      jcr->RegexWhere = bstrdup(regexwhere);       
   }

   if (when) {
      jcr->sched_time = str_to_utime(when);
      if (jcr->sched_time == 0) {
         ua->send_msg(_("Invalid time, using current time.\n"));
         jcr->sched_time = time(NULL);
      }
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
         ua->send_msg(_("Invalid replace option: %s\n"), replace);
         goto bail_out;
      }
   } else if (job->replace) {
      jcr->replace = job->replace;
   } else {
      jcr->replace = REPLACE_ALWAYS;
   }

   if (Priority) {
      jcr->JobPriority = Priority;
   }

   if (since) {
      if (!jcr->stime) {
         jcr->stime = get_pool_memory(PM_MESSAGE);
      }
      pm_strcpy(jcr->stime, since);
   }

   jcr->cloned = cloned;

   if (find_arg(ua, NT_("fdcalled")) > 0) {
      jcr->file_bsock = dup_bsock(ua->UA_sock);
      ua->quit = true;
   }

try_again:
   /* If pool changed, update migration write storage */
   if (jcr->JobType == JT_MIGRATE) {
      if (!set_migration_wstorage(jcr, pool)) {
         goto bail_out;
      }
   }
   replace = ReplaceOptions[0].name;
   for (i=0; ReplaceOptions[i].name; i++) {
      if (ReplaceOptions[i].token == jcr->replace) {
         replace = ReplaceOptions[i].name;
      }
   }
   if (level_name) {
      if (!get_level_from_name(jcr, level_name)) {
         ua->send_msg(_("Level %s not valid.\n"), level_name);
         goto bail_out;
      }
   }
   if (jid) {
      /* Note, this is also MigrateJobId */
      jcr->RestoreJobId = str_to_int64(jid);
   }

   /* Run without prompting? */
   if (ua->batch || find_arg(ua, NT_("yes")) > 0) {
      goto start_job;
   }

   /*
    * Prompt User to see if all run job parameters are correct, and
    *   allow him to modify them.
    */
   if (!display_job_parameters(ua, jcr, job, verify_list, jid, replace,
        client_name)) {
      goto bail_out;
   }

   if (!get_cmd(ua, _("OK to run? (yes/mod/no): "))) {
      goto bail_out;
   }

   /*
    * At user request modify parameters of job to be run.
    */
   if (ua->cmd[0] != 0 && strncasecmp(ua->cmd, _("mod"), strlen(ua->cmd)) == 0) {
      FILE *fd;

      start_prompt(ua, _("Parameters to modify:\n"));
      add_prompt(ua, _("Level"));            /* 0 */
      add_prompt(ua, _("Storage"));          /* 1 */
      add_prompt(ua, _("Job"));              /* 2 */
      add_prompt(ua, _("FileSet"));          /* 3 */
      if (jcr->JobType == JT_RESTORE) {
         add_prompt(ua, _("Restore Client"));   /* 4 */
      } else {
         add_prompt(ua, _("Client"));        /* 4 */
      }
      add_prompt(ua, _("When"));             /* 5 */
      add_prompt(ua, _("Priority"));         /* 6 */
      if (jcr->JobType == JT_BACKUP ||
          jcr->JobType == JT_MIGRATE ||
          jcr->JobType == JT_VERIFY) {
         add_prompt(ua, _("Pool"));          /* 7 */
         if (jcr->JobType == JT_VERIFY) {
            add_prompt(ua, _("Verify Job"));  /* 8 */
         }
      } else if (jcr->JobType == JT_RESTORE) {
         add_prompt(ua, _("Bootstrap"));     /* 7 */
         add_prompt(ua, _("Where"));         /* 8 */
         add_prompt(ua, _("File Relocation"));/* 9 */    
         add_prompt(ua, _("Replace"));       /* 10 */
         add_prompt(ua, _("JobId"));         /* 11 */
      }
      switch (do_prompt(ua, "", _("Select parameter to modify"), NULL, 0)) {
      case 0:
         /* Level */
         select_job_level(ua, jcr);
         goto try_again;
      case 1:
         /* Storage */
         store.store = select_storage_resource(ua);
         if (store.store) {
            pm_strcpy(store.store_source, _("user selection"));
            set_rwstorage(jcr, &store);
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
         /* Client */
         client = select_client_resource(ua);
         if (client) {
            jcr->client = client;
            goto try_again;
         }
         break;
      case 5:
         /* When */
         if (!get_cmd(ua, _("Please enter desired start time as YYYY-MM-DD HH:MM:SS (return for now): "))) {
            break;
         }
         if (ua->cmd[0] == 0) {
            jcr->sched_time = time(NULL);
         } else {
            jcr->sched_time = str_to_utime(ua->cmd);
            if (jcr->sched_time == 0) {
               ua->send_msg(_("Invalid time, using current time.\n"));
               jcr->sched_time = time(NULL);
            }
         }
         goto try_again;
      case 6:
         /* Priority */
         if (!get_pint(ua, _("Enter new Priority: "))) {
            break;
         }
         if (ua->pint32_val == 0) {
            ua->send_msg(_("Priority must be a positive integer.\n"));
         } else {
            jcr->JobPriority = ua->pint32_val;
         }
         goto try_again;
      case 7:
         /* Pool or Bootstrap depending on JobType */
         if (jcr->JobType == JT_BACKUP ||
             jcr->JobType == JT_MIGRATE ||
             jcr->JobType == JT_VERIFY) {      /* Pool */
            pool = select_pool_resource(ua);
            if (pool) {
               jcr->pool = pool;
               Dmsg1(100, "Set new pool=%s\n", jcr->pool->name());
               goto try_again;
            }
            break;
         }

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
            fd = fopen(jcr->RestoreBootstrap, "rb");
            if (!fd) {
               ua->send_msg(_("Warning cannot open %s: ERR=%s\n"),
                  jcr->RestoreBootstrap, strerror(errno));
               free(jcr->RestoreBootstrap);
               jcr->RestoreBootstrap = NULL;
            } else {
               fclose(fd);
            }
         }
         goto try_again;
      case 8:
         /* Verify Job */
         if (jcr->JobType == JT_VERIFY) {
            verify_job = select_job_resource(ua);
            if (verify_job) {
              jcr->verify_job = verify_job;
            }
            goto try_again;
         }
         /* Where */
         if (!get_cmd(ua, _("Please enter path prefix for restore (/ for none): "))) {
            break;
         }
         if (jcr->RegexWhere) { /* cannot use regexwhere and where */
            free(jcr->RegexWhere);
            jcr->RegexWhere = NULL;
         }
         if (jcr->where) {
            free(jcr->where);
            jcr->where = NULL;
         }
         if (IsPathSeparator(ua->cmd[0]) && ua->cmd[1] == '\0') {
            ua->cmd[0] = 0;
         }
         jcr->where = bstrdup(ua->cmd);
         goto try_again;
      case 9: 
         /* File relocation */
         select_where_regexp(ua, jcr);
         goto try_again;
      case 10:
         /* Replace */
         start_prompt(ua, _("Replace:\n"));
         for (i=0; ReplaceOptions[i].name; i++) {
            add_prompt(ua, ReplaceOptions[i].name);
         }
         opt = do_prompt(ua, "", _("Select replace option"), NULL, 0);
         if (opt >=  0) {
            jcr->replace = ReplaceOptions[opt].token;
         }
         goto try_again;
      case 11:
         /* JobId */
         jid = NULL;                  /* force reprompt */
         jcr->RestoreJobId = 0;
         if (jcr->RestoreBootstrap) {
            ua->send_msg(_("You must set the bootstrap file to NULL to be able to specify a JobId.\n"));
         }
         goto try_again;
      case -1:                        /* error or cancel */
         goto bail_out;
      default:
         goto try_again;
      }
      goto bail_out;
   }

   if (ua->cmd[0] == 0 || strncasecmp(ua->cmd, _("yes"), strlen(ua->cmd)) == 0) {
      JobId_t JobId;
      Dmsg1(800, "Calling run_job job=%x\n", jcr->job);
start_job:
      Dmsg1(100, "Using pool %s\n", jcr->pool->name());
      JobId = run_job(jcr);
#if 0  
      ua->send_msg("<job director=\"console\" time=\"%u\" status=\"%c\" type=\"%c\" "
              "jobid=\"%u\" job=\"%s\" level=\"%c\" finished=\"false\" priority=\"%u\"/>\n",
               time(NULL), jcr->JobStatus, jcr->JobType, jcr->JobId,
              jcr->Job, jcr->JobLevel, jcr->JobPriority);
#endif
      free_jcr(jcr);                  /* release jcr */
      if (JobId == 0) {
         ua->error_msg(_("Job failed.\n"));
      } else {
         char ed1[50];
         ua->send_msg(_("Job queued. JobId=%s\n"), edit_int64(JobId,ed1));
      }
      return JobId;
   }

bail_out:
   ua->send_msg(_("Job not run.\n"));
   free_jcr(jcr);
   return 0;                       /* do not run */
}

static void select_where_regexp(UAContext *ua, JCR *jcr)
{
   alist *regs;
   char *strip_prefix, *add_prefix, *add_suffix, *rwhere;
   strip_prefix = add_suffix = rwhere = add_prefix = NULL;

try_again_reg:
   ua->send_msg(_("strip_prefix=%s add_prefix=%s add_suffix=%s\n"),
                NPRT(strip_prefix), NPRT(add_prefix), NPRT(add_suffix));

   start_prompt(ua, _("This will replace your current Where value\n"));
   add_prompt(ua, _("Strip prefix"));                /* 0 */
   add_prompt(ua, _("Add prefix"));                  /* 1 */
   add_prompt(ua, _("Add file suffix"));             /* 2 */
   add_prompt(ua, _("Enter a regexp"));              /* 3 */
   add_prompt(ua, _("Test filename manipulation"));  /* 4 */
   add_prompt(ua, _("Use this ?"));                  /* 5 */
   
   switch (do_prompt(ua, "", _("Select parameter to modify"), NULL, 0)) {
   case 0:
      /* Strip prefix */
      if (get_cmd(ua, _("Please enter path prefix to strip: "))) {
         if (strip_prefix) bfree(strip_prefix);
         strip_prefix = bstrdup(ua->cmd);
      }
      
      goto try_again_reg;
   case 1:
      /* Add prefix */
      if (get_cmd(ua, _("Please enter path prefix to add (/ for none): "))) {
         if (IsPathSeparator(ua->cmd[0]) && ua->cmd[1] == '\0') {
            ua->cmd[0] = 0;
         }

         if (add_prefix) bfree(add_prefix);
         add_prefix = bstrdup(ua->cmd);
      }
      goto try_again_reg;
   case 2:
      /* Add suffix */
      if (get_cmd(ua, _("Please enter file suffix to add: "))) {
         if (add_suffix) bfree(add_suffix);
         add_suffix = bstrdup(ua->cmd);
      }      
      goto try_again_reg;
   case 3:
      /* Add rwhere */
      if (get_cmd(ua, _("Please enter a valid regexp (!from!to!): "))) {
         if (rwhere) bfree(rwhere);
         rwhere = bstrdup(ua->cmd);
      }
      
      goto try_again_reg;      
   case 4:
      /* Test regexp */ 
      char *result;
      char *regexp;
      
      if (rwhere && rwhere[0] != '\0') {
         regs = get_bregexps(rwhere);
         ua->send_msg(_("regexwhere=%s\n"), NPRT(rwhere));
      } else {
         int len = bregexp_get_build_where_size(strip_prefix, add_prefix, add_suffix);
         regexp = (char *) bmalloc (len * sizeof(char));
         bregexp_build_where(regexp, len, strip_prefix, add_prefix, add_suffix);
         regs = get_bregexps(regexp);
         ua->send_msg(_("strip_prefix=%s add_prefix=%s add_suffix=%s result=%s\n"),
                      NPRT(strip_prefix), NPRT(add_prefix), NPRT(add_suffix), NPRT(regexp));
         
         bfree(regexp);
      }

      if (!regs) {
         ua->send_msg(_("Cannot use your regexp\n"));
         goto try_again_reg;
      }

      while (get_cmd(ua, _("Please enter filename to test: "))) {
         apply_bregexps(ua->cmd, regs, &result);
         ua->send_msg(_("%s -> %s\n"), ua->cmd, result);
      }
      free_bregexps(regs);
      delete regs;
      goto try_again_reg;

   case 5:
      /* OK */
      break;
   case -1:                        /* error or cancel */
      goto bail_out_reg;
   default:
      goto try_again_reg;
   }

   /* replace the existing where */
   if (jcr->where) {
      bfree(jcr->where);
      jcr->where = NULL;
   }

   /* replace the existing regexwhere */
   if (jcr->RegexWhere) {
      bfree(jcr->RegexWhere);
      jcr->RegexWhere = NULL;
   }

   if (rwhere) {
      jcr->RegexWhere = bstrdup(rwhere);
   } else if (strip_prefix || add_prefix || add_suffix) {
      int len = bregexp_get_build_where_size(strip_prefix, add_prefix, add_suffix);
      jcr->RegexWhere = (char *) bmalloc(len*sizeof(char));
      bregexp_build_where(jcr->RegexWhere, len, strip_prefix, add_prefix, add_suffix);
   }

   regs = get_bregexps(jcr->RegexWhere);
   if (regs) {
      free_bregexps(regs);
      delete regs;
   } else {
      if (jcr->RegexWhere) {
         bfree(jcr->RegexWhere);
         jcr->RegexWhere = NULL;
      }
      ua->send_msg(_("Cannot use your regexp.\n"));
   }

bail_out_reg:
   if (strip_prefix) bfree(strip_prefix);
   if (add_prefix)   bfree(add_prefix);
   if (add_suffix)   bfree(add_suffix);
   if (rwhere)       bfree(rwhere);
}

static void select_job_level(UAContext *ua, JCR *jcr)
{
   if (jcr->JobType == JT_BACKUP) {
      start_prompt(ua, _("Levels:\n"));
      add_prompt(ua, _("Base"));
      add_prompt(ua, _("Full"));
      add_prompt(ua, _("Incremental"));
      add_prompt(ua, _("Differential"));
      add_prompt(ua, _("Since"));
      switch (do_prompt(ua, "", _("Select level"), NULL, 0)) {
      case 0:
         jcr->JobLevel = L_BASE;
         break;
      case 1:
         jcr->JobLevel = L_FULL;
         break;
      case 2:
         jcr->JobLevel = L_INCREMENTAL;
         break;
      case 3:
         jcr->JobLevel = L_DIFFERENTIAL;
         break;
      case 4:
         jcr->JobLevel = L_SINCE;
         break;
      default:
         break;
      }
   } else if (jcr->JobType == JT_VERIFY) {
      start_prompt(ua, _("Levels:\n"));
      add_prompt(ua, _("Initialize Catalog"));
      add_prompt(ua, _("Verify Catalog"));
      add_prompt(ua, _("Verify Volume to Catalog"));
      add_prompt(ua, _("Verify Disk to Catalog"));
      add_prompt(ua, _("Verify Volume Data (not yet implemented)"));
      switch (do_prompt(ua, "",  _("Select level"), NULL, 0)) {
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
         jcr->JobLevel = L_VERIFY_DISK_TO_CATALOG;
         break;
      case 4:
         jcr->JobLevel = L_VERIFY_DATA;
         break;
      default:
         break;
      }
   } else {
      ua->warning_msg(_("Level not appropriate for this Job. Cannot be changed.\n"));
   }
   return;
}

static bool display_job_parameters(UAContext *ua, JCR *jcr, JOB *job, char *verify_list,
   char *jid, const char *replace, char *client_name) 
{
   Dmsg1(800, "JobType=%c\n", jcr->JobType);
   switch (jcr->JobType) {
      char ec1[30];
      char dt[MAX_TIME_LENGTH];
   case JT_ADMIN:
      if (ua->api) ua->signal(BNET_RUN_CMD);   
      ua->send_msg(_("Run %s job\n"
                     "JobName:  %s\n"
                     "FileSet:  %s\n"
                     "Client:   %s\n"
                     "Storage:  %s\n"
                     "When:     %s\n"
                     "Priority: %d\n"),
                 _("Admin"),
                 job->name(),
                 jcr->fileset->name(),
                 NPRT(jcr->client->name()),
                 jcr->wstore?jcr->wstore->name():"*None*",
                 bstrutime(dt, sizeof(dt), jcr->sched_time),
                 jcr->JobPriority);
      jcr->JobLevel = L_FULL;
      break;
   case JT_BACKUP:
   case JT_VERIFY:
      if (jcr->JobType == JT_BACKUP) {
         if (ua->api) ua->signal(BNET_RUN_CMD);   
         ua->send_msg(_("Run %s job\n"
                        "JobName:  %s\n"
                        "Level:    %s\n"
                        "Client:   %s\n"
                        "FileSet:  %s\n"
                        "Pool:     %s (From %s)\n"
                        "Storage:  %s (From %s)\n"
                        "When:     %s\n"
                        "Priority: %d\n"),
                 _("Backup"),
                 job->name(),
                 level_to_str(jcr->JobLevel),
                 jcr->client->name(),
                 jcr->fileset->name(),
                 NPRT(jcr->pool->name()), jcr->pool_source,
                 jcr->wstore?jcr->wstore->name():"*None*", jcr->wstore_source,
                 bstrutime(dt, sizeof(dt), jcr->sched_time),
                 jcr->JobPriority);
      } else {  /* JT_VERIFY */
         const char *Name;
         if (jcr->verify_job) {
            Name = jcr->verify_job->name();
         } else {
            Name = "";
         }
         if (!verify_list) {
            verify_list = job->WriteVerifyList;
         }
         if (!verify_list) {
            verify_list = "";
         }
         if (ua->api) ua->signal(BNET_RUN_CMD);   
         ua->send_msg(_("Run %s job\n"
                        "JobName:     %s\n"
                        "Level:       %s\n"
                        "Client:      %s\n"
                        "FileSet:     %s\n"
                        "Pool:        %s (From %s)\n"
                        "Storage:     %s (From %s)\n"
                        "Verify Job:  %s\n"
                        "Verify List: %s\n"
                        "When:        %s\n"
                        "Priority:    %d\n"),
              _("Verify"),
              job->name(),
              level_to_str(jcr->JobLevel),
              jcr->client->name(),
              jcr->fileset->name(),
              NPRT(jcr->pool->name()), jcr->pool_source,
              jcr->rstore->name(), jcr->rstore_source,
              Name,
              verify_list,
              bstrutime(dt, sizeof(dt), jcr->sched_time),
              jcr->JobPriority);
      }
      break;
   case JT_RESTORE:
      if (jcr->RestoreJobId == 0 && !jcr->RestoreBootstrap) {
         if (jid) {
            jcr->RestoreJobId = str_to_int64(jid);
         } else {
            if (!get_pint(ua, _("Please enter a JobId for restore: "))) {
               return false;
            }
            jcr->RestoreJobId = ua->int64_val;
         }
      }
      jcr->JobLevel = L_FULL;      /* default level */
      Dmsg1(800, "JobId to restore=%d\n", jcr->RestoreJobId);
      if (jcr->RestoreJobId == 0) {
         if (ua->api) ua->signal(BNET_RUN_CMD);   
         /* RegexWhere is take before RestoreWhere */
         if (jcr->RegexWhere || (job->RegexWhere && !jcr->where)) {
            ua->send_msg(_("Run Restore job\n"
                        "JobName:        f%s\n"
                        "Bootstrap:       %s\n"
                        "RegexWhere:      %s\n"
                        "Replace:         %s\n"
                        "FileSet:         %s\n"
                        "Backup Client:   %s\n"
                        "Restore Client:  %s\n"
                        "Storage:         %s\n"
                        "When:            %s\n"
                        "Catalog:         %s\n"
                        "Priority:        %d\n"),
                 job->name(),
                 NPRT(jcr->RestoreBootstrap), 
                 jcr->RegexWhere?jcr->RegexWhere:job->RegexWhere,
                 replace,
                 jcr->fileset->name(),
                 client_name,
                 jcr->client->name(),
                 jcr->rstore->name(),
                 bstrutime(dt, sizeof(dt), jcr->sched_time),
                 jcr->catalog->name(),
                 jcr->JobPriority);

         } else {
            ua->send_msg(_("Run Restore job\n"
                        "JobName:         %s\n"
                        "Bootstrap:       %s\n"
                        "Where:           %s\n"
                        "Replace:         %s\n"
                        "FileSet:         %s\n"
                        "Backup Client:   %s\n"
                        "Restore Client:  %s\n"
                        "Storage:         %s\n"
                        "When:            %s\n"
                        "Catalog:         %s\n"
                        "Priority:        %d\n"),
                 job->name(),
                 NPRT(jcr->RestoreBootstrap), 
                 jcr->where?jcr->where:NPRT(job->RestoreWhere), 
                 replace,
                 jcr->fileset->name(),
                 client_name,
                 jcr->client->name(),
                 jcr->rstore->name(),
                 bstrutime(dt, sizeof(dt), jcr->sched_time),
                 jcr->catalog->name(),
                 jcr->JobPriority);
         }

      } else {
         if (ua->api) ua->signal(BNET_RUN_CMD);   
         ua->send_msg(_("Run Restore job\n"
                        "JobName:    %s\n"
                        "Bootstrap:  %s\n"),
                      job->name(),
                      NPRT(jcr->RestoreBootstrap));
                      
         /* RegexWhere is take before RestoreWhere */
         if (jcr->RegexWhere || (job->RegexWhere && !jcr->where)) {
            ua->send_msg(_("RegexWhere: %s\n"),
                         jcr->RegexWhere?jcr->RegexWhere:job->RegexWhere);
         } else {
            ua->send_msg(_("Where:      %s\n"),
                         jcr->where?jcr->where:NPRT(job->RestoreWhere));
         }

         ua->send_msg(_("Replace:    %s\n"
                        "Client:     %s\n"
                        "Storage:    %s\n"
                        "JobId:      %s\n"
                        "When:       %s\n"
                        "Catalog:    %s\n"
                        "Priority:   %d\n"),
              replace,
              jcr->client->name(),
              jcr->rstore->name(),
              jcr->RestoreJobId==0?"*None*":edit_uint64(jcr->RestoreJobId, ec1),
              bstrutime(dt, sizeof(dt), jcr->sched_time),
              jcr->catalog->name(),
              jcr->JobPriority);
      }
      break;
   case JT_MIGRATE:
      jcr->JobLevel = L_FULL;      /* default level */
      if (ua->api) ua->signal(BNET_RUN_CMD);   
      ua->send_msg(_("Run Migration job\n"
                     "JobName:       %s\n"
                     "Bootstrap:     %s\n"
                     "Client:        %s\n"
                     "FileSet:       %s\n"
                     "Pool:          %s (From %s)\n"
                     "Read Storage:  %s (From %s)\n"
                     "Write Storage: %s (From %s)\n"
                     "JobId:         %s\n"
                     "When:          %s\n"
                     "Catalog:       %s\n"
                     "Priority:      %d\n"),
           job->name(),
           NPRT(jcr->RestoreBootstrap),
           jcr->client->name(),
           jcr->fileset->name(),
           NPRT(jcr->pool->name()), jcr->pool_source,
           jcr->rstore->name(), jcr->rstore_source,
           jcr->wstore?jcr->wstore->name():"*None*", jcr->wstore_source,
           jcr->MigrateJobId==0?"*None*":edit_uint64(jcr->MigrateJobId, ec1),
           bstrutime(dt, sizeof(dt), jcr->sched_time),
           jcr->catalog->name(),
           jcr->JobPriority);
      break;
   default:
      ua->error_msg(_("Unknown Job Type=%d\n"), jcr->JobType);
      return false;
   }
   return true;
}
