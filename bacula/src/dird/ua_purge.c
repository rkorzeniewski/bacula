/*
 *
 *   Bacula Director -- User Agent Database Purge Command
 *
 *	Purges Files from specific JobIds
 * or
 *	Purges Jobs from Volumes
 *
 *     Kern Sibbald, February MMII
 */

/*
   Copyright (C) 2002 Kern Sibbald and John Walker

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

/* Forward referenced functions */
int purge_files_from_client(UAContext *ua, CLIENT *client);
int purge_jobs_from_client(UAContext *ua, CLIENT *client);
void purge_files_from_volume(UAContext *ua, POOL_DBR *pr, MEDIA_DBR *mr );
void purge_jobs_from_volume(UAContext *ua, POOL_DBR *pr, MEDIA_DBR *mr);
void purge_files_from_job(UAContext *ua, JOB_DBR *jr);


#define MAX_DEL_LIST_LEN 1000000


static char *select_jobsfiles_from_client =
   "SELECT JobId FROM Job "
   "WHERE ClientId=%d "
   "AND PurgedFiles=0";

static char *select_jobs_from_client =
   "SELECT JobId, PurgedFiles FROM Job "
   "WHERE ClientId=%d";


/* In memory list of JobIds */
struct s_file_del_ctx {
   JobId_t *JobId;
   int num_ids; 		      /* ids stored */
   int max_ids; 		      /* size of array */
   int num_del; 		      /* number deleted */
   int tot_ids; 		      /* total to process */
};

struct s_job_del_ctx {
   JobId_t *JobId;		      /* array of JobIds */
   char *PurgedFiles;		      /* Array of PurgedFile flags */
   int num_ids; 		      /* ids stored */
   int max_ids; 		      /* size of array */
   int num_del; 		      /* number deleted */
   int tot_ids; 		      /* total to process */
};

struct s_count_ctx {
   int count;
};


/*
 * Called here to count entries to be deleted 
 */
static int file_count_handler(void *ctx, int num_fields, char **row)
{
   struct s_file_del_ctx *del = (struct s_file_del_ctx *)ctx;
   del->tot_ids++;
   return 0;
}


static int job_count_handler(void *ctx, int num_fields, char **row)
{
   struct s_job_del_ctx *del = (struct s_job_del_ctx *)ctx;
   del->tot_ids++;
   return 0;
}


/*
 * Called here to make in memory list of JobIds to be
 *  deleted and the associated PurgedFiles flag.
 *  The in memory list will then be transversed
 *  to issue the SQL DELETE commands.  Note, the list
 *  is allowed to get to MAX_DEL_LIST_LEN to limit the
 *  maximum malloc'ed memory.
 */
static int job_delete_handler(void *ctx, int num_fields, char **row)
{
   struct s_job_del_ctx *del = (struct s_job_del_ctx *)ctx;

   if (del->num_ids == MAX_DEL_LIST_LEN) {  
      return 1;
   }
   if (del->num_ids == del->max_ids) {
      del->max_ids = (del->max_ids * 3) / 2;
      del->JobId = (JobId_t *)brealloc(del->JobId, sizeof(JobId_t) * del->max_ids);
      del->PurgedFiles = (char *)brealloc(del->PurgedFiles, del->max_ids);
   }
   del->JobId[del->num_ids] = (JobId_t)strtod(row[0], NULL);
   del->PurgedFiles[del->num_ids++] = (char)atoi(row[0]);
   return 0;
}

static int file_delete_handler(void *ctx, int num_fields, char **row)
{
   struct s_file_del_ctx *del = (struct s_file_del_ctx *)ctx;

   if (del->num_ids == MAX_DEL_LIST_LEN) {  
      return 1;
   }
   if (del->num_ids == del->max_ids) {
      del->max_ids = (del->max_ids * 3) / 2;
      del->JobId = (JobId_t *)brealloc(del->JobId, sizeof(JobId_t) *
	 del->max_ids);
   }
   del->JobId[del->num_ids++] = (JobId_t)strtod(row[0], NULL);
   return 0;
}

void purge_files_from_volume(UAContext *ua, POOL_DBR *pr, MEDIA_DBR *mr ) {} /* ***FIXME*** implement */
void purge_jobs_from_volume(UAContext *ua, POOL_DBR *pr, MEDIA_DBR *mr) {} /* ***FIXME*** implement */



/*
 *   Purge records from database
 *
 *     Purge Files from [Job|JobId|Client|Volume]
 *     Purge Jobs  from [Client|Volume]
 *
 *  N.B. Not all above is implemented yet.
 */
int purgecmd(UAContext *ua, char *cmd)
{
   CLIENT *client;
   MEDIA_DBR mr;
   POOL_DBR pr;
   JOB_DBR  jr;
   static char *keywords[] = {
      N_("files"),
      N_("jobs"),
      NULL};

   static char *files_keywords[] = {
      N_("Job"),
      N_("JobId"),
      N_("Client"),
      N_("Volume"),
      NULL};

   static char *jobs_keywords[] = {
      N_("Client"),
      N_("Volume"),
      NULL};
      
   bsendmsg(ua, _(
      "This command is DANGEROUUS!\n"
      "It purges (deletes) all Files from a Job,\n"
      "JobId, Client or Volume; or it purges (deletes)\n"
      "all Jobs from a Client or Volume. Normally you\n" 
      "should use the PRUNE command instead.\n"));

   if (!open_db(ua)) {
      return 1;
   }
   switch (find_arg_keyword(ua, keywords)) {
   /* Files */
   case 0:
      switch(find_arg_keyword(ua, files_keywords)) {
      case 0:			      /* Job */
      case 1:
	 if (get_job_dbr(ua, &jr)) {
	    purge_files_from_job(ua, &jr);
	 }
	 return 1;
      case 2:			      /* client */
	 client = select_client_resource(ua);
	 purge_files_from_client(ua, client);
	 return 1;
      case 3:
	 if (select_pool_and_media_dbr(ua, &pr, &mr)) {
	    purge_files_from_volume(ua, &pr, &mr);
	 }
	 return 1;
      }
   /* Jobs */
   case 1:
      switch(find_arg_keyword(ua, jobs_keywords)) {
      case 0:			      /* client */
	 client = select_client_resource(ua);
	 purge_jobs_from_client(ua, client);
	 return 1;
      case 1:
	 if (select_pool_and_media_dbr(ua, &pr, &mr)) {
	    purge_jobs_from_volume(ua, &pr, &mr);
	 }
	 return 1;
      }
   default:
      break;
   }
   switch (do_keyword_prompt(ua, _("Choose item to purge"), keywords)) {
   case 0:
      client = select_client_resource(ua);
      if (!client) {
	 return 1;
      }
      purge_files_from_client(ua, client);
      break;
   case 1:
      client = select_client_resource(ua);
      if (!client) {
	 return 1;
      }
      purge_jobs_from_client(ua, client);
      break;
   }
   return 1;
}

/*
 * Prune File records from the database. For any Job which
 * is older than the retention period, we unconditionally delete
 * all File records for that Job.  This is simple enough that no
 * temporary tables are needed. We simply make an in memory list of
 * the JobIds meeting the prune conditions, then delete all File records
 * pointing to each of those JobIds.
 */
int purge_files_from_client(UAContext *ua, CLIENT *client)
{
   struct s_file_del_ctx del;
   char *query = (char *)get_pool_memory(PM_MESSAGE);
   int i;
   CLIENT_DBR cr;

   memset(&cr, 0, sizeof(cr));
   strcpy(cr.Name, client->hdr.name);
   if (!db_create_client_record(ua->db, &cr)) {
      return 0;
   }

   del.JobId = NULL;
   del.num_ids = 0;
   del.tot_ids = 0;
   del.num_del = 0;
   del.max_ids = 0;

   Mmsg(&query, select_jobsfiles_from_client, cr.ClientId);

   Dmsg1(050, "select sql=%s\n", query);
 
   if (!db_sql_query(ua->db, query, file_count_handler, (void *)&del)) {
      bsendmsg(ua, "%s", db_strerror(ua->db));
      Dmsg0(050, "Count failed\n");
      goto bail_out;
   }
      
   if (del.tot_ids == 0) {
      bsendmsg(ua, _("No Files found for client %s to purge from %s catalog.\n"),
	 client->hdr.name, client->catalog->hdr.name);
      goto bail_out;
   }

   if (del.tot_ids < MAX_DEL_LIST_LEN) {
      del.max_ids = del.tot_ids + 1;
   } else {
      del.max_ids = MAX_DEL_LIST_LEN; 
   }
   del.tot_ids = 0;

   del.JobId = (JobId_t *)malloc(sizeof(JobId_t) * del.max_ids);

   db_sql_query(ua->db, query, file_delete_handler, (void *)&del);

   for (i=0; i < del.num_ids; i++) {
      Dmsg1(050, "Delete JobId=%d\n", del.JobId[i]);
      Mmsg(&query, "DELETE FROM File WHERE JobId=%d", del.JobId[i]);
      db_sql_query(ua->db, query, NULL, (void *)NULL);
      /* 
       * Now mark Job as having files purged. This is necessary to
       * avoid having too many Jobs to process in future prunings. If
       * we don't do this, the number of JobId's in our in memory list
       * will grow very large.
       */
      Mmsg(&query, "UPDATE Job Set PurgedFiles=1 WHERE JobId=%d", del.JobId[i]);
      db_sql_query(ua->db, query, NULL, (void *)NULL);
      Dmsg1(050, "Del sql=%s\n", query);
   }
   bsendmsg(ua, _("%d Files for client %s purged from %s catalog.\n"), del.num_ids,
      client->hdr.name, client->catalog->hdr.name);
   
bail_out:
   if (del.JobId) {
      free(del.JobId);
   }
   free_pool_memory(query);
   return 1;
}



/*
 * Purging Jobs is a bit more complicated than purging Files
 * because we delete Job records only if there is a more current
 * backup of the FileSet. Otherwise, we keep the Job record.
 * In other words, we never delete the only Job record that
 * contains a current backup of a FileSet. This prevents the
 * Volume from being recycled and destroying a current backup.
 */
int purge_jobs_from_client(UAContext *ua, CLIENT *client)
{
   struct s_job_del_ctx del;
   char *query = (char *)get_pool_memory(PM_MESSAGE);
   int i;
   CLIENT_DBR cr;

   memset(&cr, 0, sizeof(cr));
   strcpy(cr.Name, client->hdr.name);
   if (!db_create_client_record(ua->db, &cr)) {
      return 0;
   }

   del.JobId = NULL;
   del.num_ids = 0;
   del.tot_ids = 0;
   del.num_del = 0;
   del.max_ids = 0;

   Mmsg(&query, select_jobs_from_client, cr.ClientId);

   Dmsg1(050, "select sql=%s\n", query);
 
   if (!db_sql_query(ua->db, query, job_count_handler, (void *)&del)) {
      bsendmsg(ua, "%s", db_strerror(ua->db));
      Dmsg0(050, "Count failed\n");
      goto bail_out;
   }
   if (del.tot_ids == 0) {
      bsendmsg(ua, _("No Jobs found for client %s to purge from %s catalog.\n"),
	 client->hdr.name, client->catalog->hdr.name);
      goto bail_out;
   }

   if (del.tot_ids < MAX_DEL_LIST_LEN) {
      del.max_ids = del.tot_ids + 1;
   } else {
      del.max_ids = MAX_DEL_LIST_LEN; 
   }

   del.tot_ids = 0;

      

   del.JobId = (JobId_t *)malloc(sizeof(JobId_t) * del.max_ids);
   del.PurgedFiles = (char *)malloc(del.max_ids);

   db_sql_query(ua->db, query, job_delete_handler, (void *)&del);

   /* 
    * OK, now we have the list of JobId's to be purged, first check
    * if the Files have been purged, if not, purge (delete) them.
    * Then delete the Job entry, and finally and JobMedia records.
    */
   for (i=0; i < del.num_ids; i++) {
      Dmsg1(050, "Delete JobId=%d\n", del.JobId[i]);
      if (!del.PurgedFiles[i]) {
         Mmsg(&query, "DELETE FROM File WHERE JobId=%d", del.JobId[i]);
	 db_sql_query(ua->db, query, NULL, (void *)NULL);
         Dmsg1(050, "Del sql=%s\n", query);
      }

      Mmsg(&query, "DELETE FROM Job WHERE JobId=%d", del.JobId[i]);
      db_sql_query(ua->db, query, NULL, (void *)NULL);
      Dmsg1(050, "Del sql=%s\n", query);

      Mmsg(&query, "DELETE FROM JobMedia WHERE JobId=%d", del.JobId[i]);
      db_sql_query(ua->db, query, NULL, (void *)NULL);
      Dmsg1(050, "Del sql=%s\n", query);
   }
   bsendmsg(ua, _("%d Jobs for client %s purged from %s catalog.\n"), del.num_ids,
      client->hdr.name, client->catalog->hdr.name);
   
bail_out:
   if (del.JobId) {
      free(del.JobId);
   }
   if (del.PurgedFiles) {
      free(del.PurgedFiles);
   }
   free_pool_memory(query);
   return 1;
}

void purge_files_from_job(UAContext *ua, JOB_DBR *jr)
{
   char *query = (char *)get_pool_memory(PM_MESSAGE);
   
   Mmsg(&query, "DELETE FROM File WHERE JobId=%d", jr->JobId);
   db_sql_query(ua->db, query, NULL, (void *)NULL);

   Mmsg(&query, "UPDATE Job Set PurgedFiles=1 WHERE JobId=%d", jr->JobId);
   db_sql_query(ua->db, query, NULL, (void *)NULL);

   free_pool_memory(query);
}
