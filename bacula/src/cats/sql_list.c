/*
 * Bacula Catalog Database List records interface routines
 * 
 *    Kern Sibbald, March 2000
 *
 *    Version $Id$
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

/* The following is necessary so that we do not include
 * the dummy external definition of DB.
 */
#define __SQL_C 		      /* indicate that this is sql.c */

#include "bacula.h"
#include "cats.h"

#if    HAVE_MYSQL || HAVE_SQLITE

/* -----------------------------------------------------------------------
 *
 *   Generic Routines (or almost generic)
 *
 * -----------------------------------------------------------------------
 */

/* Imported subroutines */
extern void list_result(B_DB *mdb, DB_LIST_HANDLER *sendit, void *ctx);
extern int QueryDB(char *file, int line, B_DB *db, char *select_cmd);


/* 
 * Submit general SQL query
 */
int db_list_sql_query(B_DB *mdb, char *query, DB_LIST_HANDLER *sendit, void *ctx,
		      int verbose)
{
   db_lock(mdb);
   if (sql_query(mdb, query) != 0) {
      Mmsg(&mdb->errmsg, _("Query failed: %s\n"), sql_strerror(mdb));
      if (verbose) {
	 sendit(ctx, mdb->errmsg);
      }
      db_unlock(mdb);
      return 0;
   }

   mdb->result = sql_store_result(mdb);

   if (mdb->result) {
      list_result(mdb, sendit, ctx);
      sql_free_result(mdb);
   }
   db_unlock(mdb);
   return 1;
}

void
db_list_pool_records(B_DB *mdb, DB_LIST_HANDLER *sendit, void *ctx) 
{
   Mmsg(&mdb->cmd, "SELECT PoolId,Name,NumVols,MaxVols,PoolType,LabelFormat "
"FROM Pool ORDER BY PoolId");

   db_lock(mdb);
   if (!QUERY_DB(mdb, mdb->cmd)) {
      db_unlock(mdb);
      return;
   }

   list_result(mdb, sendit, ctx);
   
   sql_free_result(mdb);
   db_unlock(mdb);
}

void
db_list_media_records(B_DB *mdb, MEDIA_DBR *mdbr, DB_LIST_HANDLER *sendit, void *ctx)
{
   Mmsg(&mdb->cmd, "SELECT MediaId,VolumeName,MediaType,VolStatus,\
VolBytes,LastWritten,VolRetention,Recycle,Slot \
FROM Media WHERE Media.PoolId=%u ORDER BY MediaId", mdbr->PoolId);

   db_lock(mdb);
   if (!QUERY_DB(mdb, mdb->cmd)) {
      db_unlock(mdb);
      return;
   }

   list_result(mdb, sendit, ctx);
   
   sql_free_result(mdb);
   db_unlock(mdb);
}

void db_list_jobmedia_records(B_DB *mdb, uint32_t JobId, DB_LIST_HANDLER *sendit, void *ctx)
{
   if (JobId > 0) {		      /* do by JobId */
      Mmsg(&mdb->cmd, "SELECT JobId, Media.VolumeName, FirstIndex, LastIndex \
FROM JobMedia, Media WHERE Media.MediaId=JobMedia.MediaId and JobMedia.JobId=%u", 
	   JobId);
   } else {
      Mmsg(&mdb->cmd, "SELECT JobId, Media.VolumeName, FirstIndex, LastIndex \
FROM JobMedia, Media WHERE Media.MediaId=JobMedia.MediaId");
   }

   db_lock(mdb);
   if (!QUERY_DB(mdb, mdb->cmd)) {
      db_unlock(mdb);
      return;
   }

   list_result(mdb, sendit, ctx);
   
   sql_free_result(mdb);
   db_unlock(mdb);
}



/*
 * List Job record(s) that match JOB_DBR
 *
 *  Currently, we return all jobs or if jr->JobId is set,
 *  only the job with the specified id.
 */
void
db_list_job_records(B_DB *mdb, JOB_DBR *jr, DB_LIST_HANDLER *sendit, void *ctx)
{
   if (jr->JobId == 0 && jr->Job[0] == 0) {
      Mmsg(&mdb->cmd, 
"SELECT JobId,Name,StartTime,Type,Level,JobFiles,JobBytes,JobStatus "
"FROM Job ORDER BY JobId");
   } else {			      /* single record */
      Mmsg(&mdb->cmd, "SELECT JobId,Name,StartTime,Type,Level,\
JobFiles,JobBytes,JobStatus FROM Job WHERE Job.JobId=%u", jr->JobId);
   }

   db_lock(mdb);
   if (!QUERY_DB(mdb, mdb->cmd)) {
      db_unlock(mdb);
      return;
   }

   list_result(mdb, sendit, ctx);
   
   sql_free_result(mdb);
   db_unlock(mdb);
}

/*
 * List Job totals
 *
 */
void
db_list_job_totals(B_DB *mdb, JOB_DBR *jr, DB_LIST_HANDLER *sendit, void *ctx)
{
   db_lock(mdb);

   /* List by Job */
   Mmsg(&mdb->cmd, "SELECT  count(*) AS Jobs, sum(JobFiles) \
AS Files, sum(JobBytes) AS Bytes, Name AS Job FROM Job GROUP BY Name");

   if (!QUERY_DB(mdb, mdb->cmd)) {
      db_unlock(mdb);
      return;
   }

   list_result(mdb, sendit, ctx);
   
   sql_free_result(mdb);

   /* Do Grand Total */
   Mmsg(&mdb->cmd, "SELECT count(*) AS Jobs,sum(JobFiles) \
AS Files,sum(JobBytes) As Bytes FROM Job");

   if (!QUERY_DB(mdb, mdb->cmd)) {
      db_unlock(mdb);
      return;
   }

   list_result(mdb, sendit, ctx);
   
   sql_free_result(mdb);
   db_unlock(mdb);
}


void
db_list_files_for_job(B_DB *mdb, uint32_t jobid, DB_LIST_HANDLER *sendit, void *ctx)
{

   Mmsg(&mdb->cmd, "SELECT Path.Path,Filename.Name FROM File,\
Filename,Path WHERE File.JobId=%u AND Filename.FilenameId=File.FilenameId \
AND Path.PathId=File.PathId",
      jobid);

   db_lock(mdb);
   if (!QUERY_DB(mdb, mdb->cmd)) {
      db_unlock(mdb);
      return;
   }

   list_result(mdb, sendit, ctx);
   
   sql_free_result(mdb);
   db_unlock(mdb);
}


#endif /* HAVE_MYSQL || HAVE_SQLITE */
