/*
 *
 *   Bacula Director -- User Agent Database prune Command
 *	Applies retention periods
 *
 *     Kern Sibbald, February MMII
 *
 *   Version $Id$
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
int prune_files(UAContext *ua, CLIENT *client);
int prune_jobs(UAContext *ua, CLIENT *client);
int prune_volume(UAContext *ua, POOL_DBR *pr, MEDIA_DBR *mr);
static int mark_media_purged(UAContext *ua, MEDIA_DBR *mr);


#define MAX_DEL_LIST_LEN 1000000

/*
 * Select JobIds for File deletion.
 */
static char *select_job =
   "SELECT JobId from Job "    
   "WHERE JobTDate < %s "
   "AND ClientId=%d "
   "AND PurgedFiles=0";

/*
 * List of SQL commands terminated by NULL for deleting
 *  temporary tables and indicies 
 */
static char *drop_deltabs[] = {
   "DROP TABLE DelCandidates",
   "DROP INDEX DelInx1",
   NULL};

/*
 * List of SQL commands to create temp table and indicies
 */
static char *create_deltabs[] = {
   "CREATE TABLE DelCandidates ("
      "JobId INTEGER UNSIGNED NOT NULL, "
      "PurgedFiles TINYINT, "
      "FileSetId INTEGER UNSIGNED)",
   "CREATE INDEX DelInx1 ON DelCandidates (JobId)",
   NULL};


/*
 * Fill candidates table with all Files subject to being deleted
 */
static char *insert_delcand = 
   "INSERT INTO DelCandidates "
   "SELECT JobId, PurgedFiles, FileSetId FROM Job "
   "WHERE JobTDate < %s " 
   "AND ClientId=%d";

/*
 * Select files from the DelCandidates table that have a
 * more recent backup -- i.e. are not the only backup.
 * This is the list of files to delete.
 */
static char *select_del =
   "SELECT DelCandidates.JobId "
   "FROM Job,DelCandidates "
   "WHERE Job.JobTDate >= %s "
   "AND Job.ClientId=%d "
   "AND Job.Level='F' "
   "AND Job.JobStatus='T' "
   "AND Job.FileSetId=DelCandidates.FileSetId";

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
static int count_handler(void *ctx, int num_fields, char **row)
{
   struct s_count_ctx *cnt = (struct s_count_ctx *)ctx;

   if (row[0]) {
      cnt->count = atoi(row[0]);
   } else {
      cnt->count = 0;
   }
   return 0;
}


/*
 * Called here to count the number of Jobs to be pruned
 */
static int file_count_handler(void *ctx, int num_fields, char **row)
{
   struct s_file_del_ctx *del = (struct s_file_del_ctx *)ctx;
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

/*
 *   Prune records from database
 *
 *    prune files (from) client=xxx
 *    prune jobs (from) client=xxx
 *    prune volume=xxx	
 */
int prunecmd(UAContext *ua, char *cmd)
{
   CLIENT *client;
   POOL_DBR pr;
   MEDIA_DBR mr;

   static char *keywords[] = {
      N_("Files"),
      N_("Jobs"),
      N_("Volume"),
      NULL};
   if (!open_db(ua)) {
      return 01;
   }
   switch (find_arg_keyword(ua, keywords)) {
   case 0:
      client = select_client_resource(ua);
      if (!client || !confirm_retention(ua, &client->FileRetention, "File")) {
	 return 0;
      }
      prune_files(ua, client);
      return 1;
   case 1:
      client = select_client_resource(ua);
      if (!client || !confirm_retention(ua, &client->JobRetention, "Job")) {
	 return 0;
      }
      prune_jobs(ua, client);
      return 1;
   case 2:
      if (!select_pool_and_media_dbr(ua, &pr, &mr)) {
	 return 0;
      }
      if (!confirm_retention(ua, &mr.VolRetention, "Volume")) {
	 return 0;
      }
      prune_volume(ua, &pr, &mr);
      return 1;
   default:
      break;
   }
   switch (do_keyword_prompt(ua, _("Choose item to prune"), keywords)) {
   case 0:
      client = select_client_resource(ua);
      if (!client || !confirm_retention(ua, &client->FileRetention, "File")) {
	 return 0;
      }
      prune_files(ua, client);
      break;
   case 1:
      client = select_client_resource(ua);
      if (!client || !confirm_retention(ua, &client->JobRetention, "Job")) {
	 return 0;
      }
      prune_jobs(ua, client);
      break;
   case 2:
      if (!select_pool_and_media_dbr(ua, &pr, &mr)) {
	 return 0;
      }
      if (!confirm_retention(ua, &mr.VolRetention, "Volume")) {
	 return 0;
      }
      prune_volume(ua, &pr, &mr);
      return 1;
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
int prune_files(UAContext *ua, CLIENT *client)
{
   struct s_file_del_ctx del;
   char *query = (char *)get_pool_memory(PM_MESSAGE);
   int i;
   btime_t now, period;
   CLIENT_DBR cr;
   char ed1[50], ed2[50];

   memset(&cr, 0, sizeof(cr));
   memset(&del, 0, sizeof(del));
   strcpy(cr.Name, client->hdr.name);
   if (!db_create_client_record(ua->db, &cr)) {
      return 0;
   }

   period = client->FileRetention;
   now = (btime_t)time(NULL);
       
   Mmsg(&query, select_job, edit_uint64(now - period, ed1), cr.ClientId);

   Dmsg1(050, "select sql=%s\n", query);
 
   if (!db_sql_query(ua->db, query, file_count_handler, (void *)&del)) {
      if (ua->verbose) {
         bsendmsg(ua, "%s", db_strerror(ua->db));
      }
      Dmsg0(050, "Count failed\n");
      goto bail_out;
   }
      
   if (del.tot_ids == 0) {
      if (ua->verbose) {
         bsendmsg(ua, _("No Files found for client %s to prune from %s catalog.\n"),
	    client->hdr.name, client->catalog->hdr.name);
      }
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
      struct s_count_ctx cnt;
      Dmsg1(050, "Delete JobId=%d\n", del.JobId[i]);
      Mmsg(&query, "SELECT count(*) FROM File WHERE JobId=%d", del.JobId[i]);
      cnt.count = 0;
      db_sql_query(ua->db, query, count_handler, (void *)&cnt);
      del.tot_ids += cnt.count;
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
   edit_uint64_with_commas(del.tot_ids, ed1);
   edit_uint64_with_commas(del.num_ids, ed2);
   bsendmsg(ua, _("Pruned %s Files from %s Jobs for client %s from %s catalog.\n"), 
      ed1, ed2, client->hdr.name, client->catalog->hdr.name);
   
bail_out:
   if (del.JobId) {
      free(del.JobId);
   }
   free_pool_memory(query);
   return 1;
}


static void drop_temp_tables(UAContext *ua) 
{
   int i;
   for (i=0; drop_deltabs[i]; i++) {
      db_sql_query(ua->db, drop_deltabs[i], NULL, (void *)NULL);
   }
}

static int create_temp_tables(UAContext *ua) 
{
   int i;
   /* Create temp tables and indicies */
   for (i=0; create_deltabs[i]; i++) {
      if (!db_sql_query(ua->db, create_deltabs[i], NULL, (void *)NULL)) {
         bsendmsg(ua, "%s", db_strerror(ua->db));
         Dmsg0(050, "create DelTables table failed\n");
	 return 0;
      }
   }
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
int prune_jobs(UAContext *ua, CLIENT *client)
{
   struct s_job_del_ctx del;
   struct s_count_ctx cnt;
   char *query = (char *)get_pool_memory(PM_MESSAGE);
   int i;
   btime_t now, period;
   CLIENT_DBR cr;
   char ed1[50];

   memset(&cr, 0, sizeof(cr));
   memset(&del, 0, sizeof(del));
   strcpy(cr.Name, client->hdr.name);
   if (!db_create_client_record(ua->db, &cr)) {
      return 0;
   }

   period = client->JobRetention;
   now = (btime_t)time(NULL);

   /* Drop any previous temporary tables still there */
   drop_temp_tables(ua);

   /* Create temp tables and indicies */
   if (!create_temp_tables(ua)) {
      goto bail_out;
   }

   /* 
    * Select all files that are older than the JobRetention period
    *  and stuff them into the "DeletionCandidates" table.
    */
   edit_uint64(now - period, ed1);
   Mmsg(&query, insert_delcand, ed1, cr.ClientId);

   if (!db_sql_query(ua->db, query, NULL, (void *)NULL)) {
      if (ua->verbose) {
         bsendmsg(ua, "%s", db_strerror(ua->db));
      }
      Dmsg0(050, "insert delcand failed\n");
      goto bail_out;
   }

   strcpy(query, "SELECT count(*) FROM DelCandidates");
   
   Dmsg1(100, "select sql=%s\n", query);
 
   if (!db_sql_query(ua->db, query, count_handler, (void *)&cnt)) {
      if (ua->verbose) {
         bsendmsg(ua, "%s", db_strerror(ua->db));
      }
      Dmsg0(050, "Count failed\n");
      goto bail_out;
   }
      
   if (cnt.count == 0) {
      if (ua->verbose) {
         bsendmsg(ua, _("No Jobs for client %s found to prune from %s catalog.\n"),
	    client->hdr.name, client->catalog->hdr.name);
      }
      goto bail_out;
   }

   if (cnt.count < MAX_DEL_LIST_LEN) {
      del.max_ids = cnt.count + 1;
   } else {
      del.max_ids = MAX_DEL_LIST_LEN; 
   }
   del.JobId = (JobId_t *)malloc(sizeof(JobId_t) * del.max_ids);
   del.PurgedFiles = (char *)malloc(del.max_ids);

   Mmsg(&query, select_del, ed1, cr.ClientId);
   db_sql_query(ua->db, query, job_delete_handler, (void *)&del);

   /* 
    * OK, now we have the list of JobId's to be pruned, first check
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
   bsendmsg(ua, _("Pruned %d Jobs for client %s from %s catalog.\n"), del.num_ids,
      client->hdr.name, client->catalog->hdr.name);
   
bail_out:
   drop_temp_tables(ua);
   if (del.JobId) {
      free(del.JobId);
   }
   if (del.PurgedFiles) {
      free(del.PurgedFiles);
   }
   free_pool_memory(query);
   return 1;
}

/*
 * Prune volumes
 */
int prune_volume(UAContext *ua, POOL_DBR *pr, MEDIA_DBR *mr)
{
   char *query = (char *)get_pool_memory(PM_MESSAGE);
   struct s_count_ctx cnt;
   struct s_file_del_ctx del;
   int i;
   JOB_DBR jr;
   btime_t now, period;

   memset(&jr, 0, sizeof(jr));
   memset(&del, 0, sizeof(del));
   cnt.count = 0;
   Mmsg(&query, "SELECT count(*) FROM JobMedia WHERE MediaId=%d", mr->MediaId);
   if (!db_sql_query(ua->db, query, count_handler, (void *)&cnt)) {
      bsendmsg(ua, "%s", db_strerror(ua->db));
      Dmsg0(050, "Count failed\n");
      goto bail_out;
   }
      
   if (cnt.count == 0) {
      if (ua->verbose) {
         bsendmsg(ua, "There are no Jobs associated with Volume %s. It is purged.\n",
	    mr->VolumeName);
      }
      if (!mark_media_purged(ua, mr)) {
	 goto bail_out;
      }
      goto bail_out;
   }

   if (cnt.count < MAX_DEL_LIST_LEN) {
      del.max_ids = cnt.count + 1;
   } else {
      del.max_ids = MAX_DEL_LIST_LEN; 
   }

   del.JobId = (JobId_t *)malloc(sizeof(JobId_t) * del.max_ids);

   Mmsg(&query, "SELECT JobId FROM JobMedia WHERE MediaId=%d", mr->MediaId);
   if (!db_sql_query(ua->db, query, file_delete_handler, (void *)&del)) {
      if (ua->verbose) {
         bsendmsg(ua, "%s", db_strerror(ua->db));
      }
      Dmsg0(050, "Count failed\n");
      goto bail_out;
   }

   /* Use Volume Retention to purge Jobs and Files */
   period = mr->VolRetention;
   now = (btime_t)time(NULL);

   for (i=0; i < del.num_ids; i++) {
      jr.JobId = del.JobId[i];
      if (!db_get_job_record(ua->db, &jr)) {
	 continue;
      }
      if (jr.JobTDate >= (now - period)) {
	 continue;
      }
      Dmsg1(050, "Delete JobId=%d\n", del.JobId[i]);
      Mmsg(&query, "DELETE FROM File WHERE JobId=%d", del.JobId[i]);
      db_sql_query(ua->db, query, NULL, (void *)NULL);
      Mmsg(&query, "DELETE FROM Job WHERE JobId=%d", del.JobId[i]);
      db_sql_query(ua->db, query, NULL, (void *)NULL);
      Mmsg(&query, "DELETE FROM JobMedia WHERE JobId=%d", del.JobId[i]);
      db_sql_query(ua->db, query, NULL, (void *)NULL);
      Dmsg1(050, "Del sql=%s\n", query);
      del.num_del++;
   }
   if (del.JobId) {
      free(del.JobId);
   }
   bsendmsg(ua, _("Pruned %d Jobs on Volume %s from catalog.\n"), del.num_del,
      mr->VolumeName);

   /* If purged, mark it so */
   if (del.num_ids == del.num_del) {
      mark_media_purged(ua, mr);
   }

bail_out:   
   free_pool_memory(query);
   return 1;
}

static int mark_media_purged(UAContext *ua, MEDIA_DBR *mr)
{
   if (strcmp(mr->VolStatus, "Append") == 0 || 
       strcmp(mr->VolStatus, "Full")   == 0) {
      strcpy(mr->VolStatus, "Purged");
      if (!db_update_media_record(ua->db, mr)) {
	 if (ua->verbose) {
            bsendmsg(ua, "%s", db_strerror(ua->db));
	 }
	 return 0;
      }
   }
   return 1;
}
