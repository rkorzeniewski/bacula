/*
 *
 *   Bacula Director -- User Agent Prompt and Selection code
 *
 *     Kern Sibbald, October MMI
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


/* Imported variables */


/* Exported functions */

int do_prompt(UAContext *ua, char *msg, char *prompt);
void add_prompt(UAContext *ua, char *prompt);
void start_prompt(UAContext *ua, char *msg);
STORE *select_storage_resource(UAContext *ua);
JOB *select_job_resource(UAContext *ua);

/* 
 * Given a list of keywords, find the first one
 *  that is in the argument list.
 * Returns: -1 if not found
 *	    index into list (base 0) on success
 */
int find_arg_keyword(UAContext *ua, char **list)
{
   int i, j;
   for (i=1; i<ua->argc; i++) {
      for(j=0; list[j]; j++) {
	 if (strcasecmp(_(list[j]), ua->argk[i]) == 0) {
	    return j;
	 }
      }
   }
   return -1;
}

/* 
 * Given a list of keywords, prompt the user 
 * to choose one.
 *
 * Returns: -1 on failure
 *	    index into list (base 0) on success
 */
int do_keyword_prompt(UAContext *ua, char *msg, char **list)
{
   int i;
   start_prompt(ua, _("You have the following choices:\n"));
   for (i=0; list[i]; i++) {
      add_prompt(ua, list[i]);
   }
   return do_prompt(ua, msg, NULL);
}


/* 
 * Select a Storage resource from prompt list
 */
STORE *select_storage_resource(UAContext *ua)
{
   char name[MAX_NAME_LENGTH];	  
   STORE *store = NULL;

   start_prompt(ua, _("The defined Storage resources are:\n"));
   LockRes();
   while ((store = (STORE *)GetNextRes(R_STORAGE, (RES *)store))) {
      add_prompt(ua, store->hdr.name);
   }
   UnlockRes();
   do_prompt(ua, _("Select Storage resource"), name);
   store = (STORE *)GetResWithName(R_STORAGE, name);
   return store;
}

/* 
 * Select a FileSet resource from prompt list
 */
FILESET *select_fs_resource(UAContext *ua)
{
   char name[MAX_NAME_LENGTH];	  
   FILESET *fs = NULL;

   start_prompt(ua, _("The defined FileSet resources are:\n"));
   LockRes();
   while ((fs = (FILESET *)GetNextRes(R_FILESET, (RES *)fs))) {
      add_prompt(ua, fs->hdr.name);
   }
   UnlockRes();
   do_prompt(ua, _("Select FileSet resource"), name);
   fs = (FILESET *)GetResWithName(R_FILESET, name);
   return fs;
}


/* 
 * Get a catalog resource from prompt list
 */
CAT *get_catalog_resource(UAContext *ua)
{
   char name[MAX_NAME_LENGTH];	  
   CAT *catalog = NULL;
   int i;

   for (i=1; i<ua->argc; i++) {
      if (strcasecmp(ua->argk[i], _("catalog")) == 0 && ua->argv[i]) {
	 catalog = (CAT *)GetResWithName(R_CATALOG, ua->argv[i]);
	 break;
      }
   }
   if (!catalog) {
      start_prompt(ua, _("The defined Catalog resources are:\n"));
      LockRes();
      while ((catalog = (CAT *)GetNextRes(R_CATALOG, (RES *)catalog))) {
	 add_prompt(ua, catalog->hdr.name);
      }
      UnlockRes();
      do_prompt(ua, _("Select Catalog resource"), name);
      catalog = (CAT *)GetResWithName(R_CATALOG, name);
   }
   return catalog;
}


/* 
 * Select a Job resource from prompt list
 */
JOB *select_job_resource(UAContext *ua)
{
   char name[MAX_NAME_LENGTH];	  
   JOB *job = NULL;

   start_prompt(ua, _("The defined Job resources are:\n"));
   LockRes();
   while ( (job = (JOB *)GetNextRes(R_JOB, (RES *)job)) ) {
      add_prompt(ua, job->hdr.name);
   }
   UnlockRes();
   do_prompt(ua, _("Select Job resource"), name);
   job = (JOB *)GetResWithName(R_JOB, name);
   return job;
}


/* 
 * Select a client resource from prompt list
 */
CLIENT *select_client_resource(UAContext *ua)
{
   char name[MAX_NAME_LENGTH];	  
   CLIENT *client = NULL;

   start_prompt(ua, _("The defined Client resources are:\n"));
   LockRes();
   while ( (client = (CLIENT *)GetNextRes(R_CLIENT, (RES *)client)) ) {
      add_prompt(ua, client->hdr.name);
   }
   UnlockRes();
   do_prompt(ua, _("Select Client (File daemon) resource"), name);
   client = (CLIENT *)GetResWithName(R_CLIENT, name);
   return client;
}

/*
 *  Get client resource, start by looking for
 *   client=<client-name>
 *  if we don't find the keyword, we prompt the user.
 */
CLIENT *get_client_resource(UAContext *ua)
{
   CLIENT *client = NULL;
   int i;
   
   for (i=1; i<ua->argc; i++) {
      if (strcasecmp(ua->argk[i], _("client")) == 0 && ua->argv[i]) {
	 client = (CLIENT *)GetResWithName(R_CLIENT, ua->argv[i]);
	 if (client) {
	    return client;
	 }
         bsendmsg(ua, _("Error: Client resource %s does not exist.\n"), ua->argv[i]);
	 break;
      }
   }
   return select_client_resource(ua);
}

/* Scan what the user has entered looking for:
 * 
 *  pool=<pool-name>   
 *
 *  if error or not found, put up a list of pool DBRs
 *  to choose from.
 *
 *   returns: 0 on error
 *	      poolid on success and fills in POOL_DBR
 */
int get_pool_dbr(UAContext *ua, POOL_DBR *pr)
{
   int i;

   for (i=1; i<ua->argc; i++) {
      if (strcasecmp(ua->argk[i], _("pool")) == 0 && ua->argv[i]) {
	 strcpy(pr->Name, ua->argv[i]);
	 if (!db_get_pool_record(ua->db, pr)) {
            bsendmsg(ua, _("Could not find Pool %s: ERR=%s"), ua->argv[i],
		     db_strerror(ua->db));
	    pr->PoolId = 0;
	    break;
	 }
	 return pr->PoolId;
      }
   }
   if (!select_pool_dbr(ua, pr)) {  /* try once more */
      return 0;
   }
   return pr->PoolId;
}

/*
 * Select a Pool record from the catalog
 */
int select_pool_dbr(UAContext *ua, POOL_DBR *pr)
{
   POOL_DBR opr;
   char name[MAX_NAME_LENGTH];
   int num_pools, i;
   uint32_t *ids; 


   pr->PoolId = 0;
   if (!db_get_pool_ids(ua->db, &num_pools, &ids)) {
      bsendmsg(ua, _("Error obtaining pool ids. ERR=%s\n"), db_strerror(ua->db));
      return 0;
   }
   if (num_pools <= 0) {
      bsendmsg(ua, _("No pools defined. Use the \"create\" command to create one.\n"));
      return 0;
   }
     
   start_prompt(ua, _("Defined Pools:\n"));
   for (i=0; i < num_pools; i++) {
      opr.PoolId = ids[i];
      if (!db_get_pool_record(ua->db, &opr)) {
	 continue;
      }
      add_prompt(ua, opr.Name);
   }
   free(ids);
   if (do_prompt(ua, _("Select the Pool"), name) < 0) {
      return 0;
   }
   memset(&opr, 0, sizeof(pr));
   strcpy(opr.Name, name);

   if (!db_get_pool_record(ua->db, &opr)) {
      bsendmsg(ua, _("Could not find Pool %s: ERR=%s"), name, db_strerror(ua->db));
      return 0;
   }
   memcpy(pr, &opr, sizeof(opr));
   return opr.PoolId;
}

/*
 * Select a Pool and a Media (Volume) record from the database
 */
int select_pool_and_media_dbr(UAContext *ua, POOL_DBR *pr, MEDIA_DBR *mr)
{
   int found = FALSE;
   int i;

   memset(pr, 0, sizeof(POOL_DBR));
   memset(mr, 0, sizeof(MEDIA_DBR));

   /* Get the pool, possibly from pool=<pool-name> */
   if (!get_pool_dbr(ua, pr)) {
      return 0;
   }
   mr->PoolId = pr->PoolId;

   /* See if a volume name is specified as an argument */
   for (i=1; i<ua->argc; i++) {
      if (strcasecmp(ua->argk[i], _("volume")) == 0 && ua->argv[i]) {
	 found = TRUE;
	 break;
      }
   }
   if (found) {
      strcpy(mr->VolumeName, ua->argv[i]);
   } else {
      db_list_media_records(ua->db, mr, prtit, ua);
      if (!get_cmd(ua, _("Enter the Volume name to delete: "))) {
	 return 01;
      }
      strcpy(mr->VolumeName, ua->cmd);
   }
   mr->MediaId = 0;
   if (!db_get_media_record(ua->db, mr)) {
      bsendmsg(ua, "%s", db_strerror(ua->db));
      return 0;
   }
   return 1;
}


/*
 *  This routine is ONLY used in the create command.
 *  If you are thinking about using it, you
 *  probably want to use select_pool_dbr() 
 *  or get_pool_dbr() above.
 */
POOL *get_pool_resource(UAContext *ua)
{
   POOL *pool = NULL;
   char name[MAX_NAME_LENGTH];
   int i;
   
   for (i=1; i<ua->argc; i++) {
      if (strcasecmp(ua->argk[i], _("pool")) == 0 && ua->argv[i]) {
	 pool = (POOL *)GetResWithName(R_POOL, ua->argv[i]);
	 if (pool) {
	    return pool;
	 }
         bsendmsg(ua, _("Error: Pool resource %s does not exist.\n"), ua->argv[i]);
	 break;
      }
   }
   start_prompt(ua, _("The defined Pool resources are:\n"));
   LockRes();
   while ((pool = (POOL *)GetNextRes(R_POOL, (RES *)pool))) {
      add_prompt(ua, pool->hdr.name);
   }
   UnlockRes();
   do_prompt(ua, _("Select Pool resource"), name);
   pool = (POOL *)GetResWithName(R_POOL, name);
   return pool;
}

/*
 * List all jobs and ask user to select one
 */
int select_job_dbr(UAContext *ua, JOB_DBR *jr)
{
   db_list_job_records(ua->db, jr, prtit, ua);
   if (!get_cmd(ua, _("Enter the JobId to select: "))) {
      return 0;
   }
   jr->JobId = atoi(ua->cmd);
   if (!db_get_job_record(ua->db, jr)) {
      bsendmsg(ua, "%s", db_strerror(ua->db));
      return 0;
   }
   return jr->JobId;

}


/* Scan what the user has entered looking for:
 * 
 *  jobid=nn
 *
 *  if error or not found, put up a list of Jobs
 *  to choose from.
 *
 *   returns: 0 on error
 *	      JobId on success and fills in JOB_DBR
 */
int get_job_dbr(UAContext *ua, JOB_DBR *jr)
{
   int i;

   for (i=1; i<ua->argc; i++) {
      if (strcasecmp(ua->argk[i], _("job")) == 0 && ua->argv[i]) {
	 jr->JobId = 0;
	 strcpy(jr->Job, ua->argv[i]);
      } else if (strcasecmp(ua->argk[i], _("jobid")) == 0 && ua->argv[i]) {
	 jr->JobId = atoi(ua->argv[i]);
      } else {
	 continue;
      }
      if (!db_get_job_record(ua->db, jr)) {
         bsendmsg(ua, _("Could not find Job %s: ERR=%s"), ua->argv[i],
		  db_strerror(ua->db));
	 jr->JobId = 0;
	 break;
      }
      return jr->JobId;
   }

   if (!select_job_dbr(ua, jr)) {  /* try once more */
      return 0;
   }
   return jr->JobId;
}




/*
 * Implement unique set of prompts 
 */
void start_prompt(UAContext *ua, char *msg)
{
  if (ua->max_prompts == 0) {
     ua->max_prompts = 10;
     ua->prompt = (char **) bmalloc(sizeof(char *) * ua->max_prompts);
  }
  ua->num_prompts = 1;
  ua->prompt[0] = bstrdup(msg);
}

/*
 * Add to prompts -- keeping them unique 
 */
void add_prompt(UAContext *ua, char *prompt)
{
   int i;
   if (ua->num_prompts == ua->max_prompts) {
      ua->max_prompts *= 2;
      ua->prompt = (char **) brealloc(ua->prompt, sizeof(char *) *
	 ua->max_prompts);
    }
    for (i=1; i < ua->num_prompts; i++) {
       if (strcmp(ua->prompt[i], prompt) == 0) {
	  return;
       }
    }
    ua->prompt[ua->num_prompts++] = bstrdup(prompt);
}

/*
 * Display prompts and get user's choice
 *
 *  Returns: -1 on error
 *	      index base 0 on success, and choice
 *		 is copied to prompt if not NULL
 */
int do_prompt(UAContext *ua, char *msg, char *prompt)
{
   int i, item;
   char pmsg[MAXSTRING];

   bsendmsg(ua, ua->prompt[0]);
   for (i=1; i < ua->num_prompts; i++) {
      bsendmsg(ua, "%6d: %s\n", i, ua->prompt[i]);
   }

   if (prompt) {
      *prompt = 0;
   }

   for ( ;; ) {
      if (ua->num_prompts == 2) {
	 item = 1;
         bsendmsg(ua, _("Item 1 selected automatically.\n"));
	 if (prompt) {
	    strcpy(prompt, ua->prompt[1]);
	 }
	 break;
      } else {
         sprintf(pmsg, "%s (1-%d): ", msg, ua->num_prompts-1);
      }
      /* Either a . or an @ will get you out of the loop */
      if (!get_cmd(ua, pmsg) || *ua->cmd == '.' || *ua->cmd == '@') {
	 item = -1;		      /* error */
	 break;
      }
      item = atoi(ua->cmd);
      if (item < 1 || item >= ua->num_prompts) {
         bsendmsg(ua, _("Please enter a number between 1 and %d\n"), ua->num_prompts-1);
	 continue;
      }
      if (prompt) {
	 strcpy(prompt, ua->prompt[item]);
      }
      break;
   }
			      
   for (i=0; i < ua->num_prompts; i++) {
      free(ua->prompt[i]);
   }
   ua->num_prompts = 0;
   return item - 1;
}


/*
 * We scan what the user has entered looking for
 *    <storage-resource>
 *    device=<device-name>	???? does this work ????
 *    storage=<storage-resource>
 *    job=<job_name>
 *    jobid=<jobid>
 *    ? 	     (prompt him with storage list)
 *    <some-error>   (prompt him with storage list)
 */
STORE *get_storage_resource(UAContext *ua, char *cmd)
{
   char *store_name, *device_name;
   STORE *store;
   int jobid;
   JCR *jcr;
   int i;
      
   if (ua->argc == 1) {
      return select_storage_resource(ua);
   }
   
   device_name = NULL;
   store_name = NULL;

   for (i=1; i<ua->argc; i++) {
      if (!ua->argv[i]) {
	 /* Default argument is storage */
	 if (store_name) {
            bsendmsg(ua, _("Storage name given twice.\n"));
	    return NULL;
	 }
	 store_name = ua->argk[i];
         if (*store_name == '?') {
	    return select_storage_resource(ua);
	 }
      } else {
         if (strcasecmp(ua->argk[i], _("device")) == 0) {
	    device_name = ua->argv[i];

         } else if (strcasecmp(ua->argk[i], _("storage")) == 0) {
	    store_name = ua->argv[i];

         } else if (strcasecmp(ua->argk[i], _("jobid")) == 0) {
	    jobid = atoi(ua->argv[i]);
	    if (jobid <= 0) {
               bsendmsg(ua, _("Expecting jobid=nn command, got: %s\n"), ua->argk[i]);
	       return NULL;
	    }
	    if (!(jcr=get_jcr_by_id(jobid))) {
               bsendmsg(ua, _("JobId %d is not running.\n"), jobid);
	       return NULL;
	    }
	    store = jcr->store;
	    free_jcr(jcr);
	    return store;

         } else if (strcasecmp(ua->argk[i], _("job")) == 0) {
	    if (!(jcr=get_jcr_by_partial_name(ua->argv[i]))) {
               bsendmsg(ua, _("Job %s is not running.\n"), ua->argv[i]);
	       return NULL;
	    }
	    store = jcr->store;
	    free_jcr(jcr);
	    return store;

	 } else {
            bsendmsg(ua, _("Unknown keyword: %s\n"), ua->argk[i]);
	    return NULL;
	 }
      }
   }

   if (!store_name) {
     bsendmsg(ua, _("A storage device name must be given.\n"));
     store = NULL;
   } else {
      store = (STORE *)GetResWithName(R_STORAGE, store_name);
      if (!store) {
         bsendmsg(ua, "Storage resource %s: not found\n", store_name);
      }
   }
   if (!store) {
      store = select_storage_resource(ua);
   }
   return store;
}


/*
 * Scan looking for mediatype= 
 *
 *  if not found or error, put up selection list
 *
 *  Returns: 0 on error
 *	     1 on success, MediaType is set
 */
int get_media_type(UAContext *ua, char *MediaType)
{
   STORE *store;
   int i;
   static char *keyword[] = {
      "mediatype",
      NULL};

   i = find_arg_keyword(ua, keyword);
   if (i >= 0 && ua->argv[i]) {
      strcpy(MediaType, ua->argv[i]);
      return 1;
   }

   start_prompt(ua, _("Media Types defined in conf file:\n"));
   LockRes();
   for (store = NULL; (store = (STORE *)GetNextRes(R_STORAGE, (RES *)store)); ) {
      add_prompt(ua, store->media_type);
   }
   UnlockRes();
   return (do_prompt(ua, _("Select the Media Type"), MediaType) < 0) ? 0 : 1;
}
