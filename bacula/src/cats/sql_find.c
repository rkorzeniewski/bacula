/*
 * Bacula Catalog Database Find record interface routines
 *
 *  Note, generally, these routines are more complicated
 *	  that a simple search by name or id. Such simple
 *	  request are in get.c
 * 
 *    Kern Sibbald, December 2000
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

/* *****FIXME**** fix fixed length of select_cmd[] and insert_cmd[] */

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
extern void print_result(B_DB *mdb);
extern int QueryDB(char *file, int line, B_DB *db, char *select_cmd);


/* Find job start time. Used to find last full save
 * for Incremental and Differential saves.
 *	
 * Returns: 0 on failure
 *	    1 on success, jr is unchanged, but stime is set
 */
int
db_find_job_start_time(B_DB *mdb, JOB_DBR *jr, char *stime)
{
   SQL_ROW row;
   int JobId;

   strcpy(stime, "0000-00-00 00:00:00");   /* default */
   db_lock(mdb);
   /* If no Id given, we must find corresponding job */
   if (jr->JobId == 0) {
      /* Differential is since last Full backup */
      if (jr->Level == L_DIFFERENTIAL) {
	 Mmsg(&mdb->cmd, 
"SELECT JobId from Job WHERE JobStatus='T' and Type='%c' and \
Level='%c' and Name='%s' and ClientId=%d and FileSetId=%d \
ORDER by StartTime DESC LIMIT 1",
	   jr->Type, L_FULL, jr->Name, jr->ClientId, jr->FileSetId);
      /* Incremental is since last Full, Incremental, or Differential */
      } else if (jr->Level == L_INCREMENTAL) {
	 Mmsg(&mdb->cmd, 
"SELECT JobId from Job WHERE JobStatus='T' and Type='%c' and \
(Level='%c' or Level='%c' or Level='%c') and Name='%s' and ClientId=%d \
ORDER by StartTime DESC LIMIT 1",
	   jr->Type, L_INCREMENTAL, L_DIFFERENTIAL, L_FULL, jr->Name,
	   jr->ClientId);
      } else {
         Mmsg1(&mdb->errmsg, _("Unknown level=%d\n"), jr->Level);
	 db_unlock(mdb);
	 return 0;
      }
      Dmsg1(100, "Submitting: %s\n", mdb->cmd);
      if (!QUERY_DB(mdb, mdb->cmd)) {
         Mmsg1(&mdb->errmsg, _("Query error for start time request: %s\n"), mdb->cmd);
	 db_unlock(mdb);
	 return 0;
      }
      if ((row = sql_fetch_row(mdb)) == NULL) {
	 sql_free_result(mdb);
	 db_unlock(mdb);
	 return 0;
      }
      JobId = atoi(row[0]);
      sql_free_result(mdb);
   } else {
      JobId = jr->JobId;	      /* search for particular id */
   }

   Dmsg1(100, "Submitting: %s\n", mdb->cmd);
   Mmsg(&mdb->cmd, "SELECT StartTime from Job WHERE Job.JobId=%d", JobId);

   if (!QUERY_DB(mdb, mdb->cmd)) {
      Mmsg1(&mdb->errmsg, _("Query error for start time request: %s\n"), mdb->cmd);
      db_unlock(mdb);
      return 0;
   }

   if ((row = sql_fetch_row(mdb)) == NULL) {
      *stime = 0;		      /* set EOS */
      Mmsg2(&mdb->errmsg, _("No Job found for JobId=%d: %s\n"), JobId, sql_strerror(mdb));
      sql_free_result(mdb);
      db_unlock(mdb);
      return 0;
   }
   Dmsg1(100, "Got start time: %s\n", row[0]);
   strcpy(stime, row[0]);

   sql_free_result(mdb);

   db_unlock(mdb);
   return 1;
}

/* 
 * Find JobId of last job that ran.  E.g. for
 *   VERIFY_CATALOG we want the JobId of the last INIT.
 *   For VERIFY_VOLUME_TO_CATALOG, we want the JobId of the last Job.
 *
 * Returns: 1 on success
 *	    0 on failure
 */
int
db_find_last_jobid(B_DB *mdb, JOB_DBR *jr)
{
   SQL_ROW row;

   /* Find last full */
   db_lock(mdb);
   if (jr->Level == L_VERIFY_CATALOG) {
      Mmsg(&mdb->cmd, 
"SELECT JobId FROM Job WHERE Type='%c' AND Level='%c' AND Name='%s' AND \
ClientId=%d ORDER BY StartTime DESC LIMIT 1",
	   JT_VERIFY, L_VERIFY_INIT, jr->Name, jr->ClientId);
   } else if (jr->Level == L_VERIFY_VOLUME_TO_CATALOG) {
      Mmsg(&mdb->cmd, 
"SELECT JobId FROM Job WHERE Type='%c' AND \
ClientId=%d ORDER BY StartTime DESC LIMIT 1",
	   JT_BACKUP, jr->ClientId);
   } else {
      Mmsg1(&mdb->errmsg, _("Unknown Job level=%c\n"), jr->Level);
      db_unlock(mdb);
      return 0;
   }

   if (!QUERY_DB(mdb, mdb->cmd)) {
      db_unlock(mdb);
      return 0;
   }
   if ((row = sql_fetch_row(mdb)) == NULL) {
      Mmsg1(&mdb->errmsg, _("No Job found for: %s.\n"), mdb->cmd);
      sql_free_result(mdb);
      db_unlock(mdb);
      return 0;
   }

   jr->JobId = atoi(row[0]);
   sql_free_result(mdb);

   Dmsg1(100, "db_get_last_jobid: got JobId=%d\n", jr->JobId);
   if (jr->JobId <= 0) {
      Mmsg1(&mdb->errmsg, _("No Job found for: %s\n"), mdb->cmd);
      db_unlock(mdb);
      return 0;
   }

   db_unlock(mdb);
   return 1;
}

/* 
 * Find Available Media (Volume) for Pool
 *
 * Find a Volume for a given PoolId, MediaType, and Status.
 *
 * Returns: 0 on failure
 *	    numrows on success
 */
int
db_find_next_volume(B_DB *mdb, int item, MEDIA_DBR *mr) 
{
   SQL_ROW row;
   int numrows;

   db_lock(mdb);
   Mmsg(&mdb->cmd, "SELECT MediaId,VolumeName,VolJobs,VolFiles,VolBlocks,\
VolBytes,VolMounts,VolErrors,VolWrites,VolMaxBytes,VolCapacityBytes,Slot \
FROM Media WHERE PoolId=%d AND MediaType='%s' AND VolStatus='%s' \
ORDER BY MediaId", mr->PoolId, mr->MediaType, mr->VolStatus); 

   if (!QUERY_DB(mdb, mdb->cmd)) {
      db_unlock(mdb);
      return 0;
   }

   numrows = sql_num_rows(mdb);
   if (item > numrows) {
      Mmsg2(&mdb->errmsg, _("Request for Volume item %d greater than max %d\n"),
	 item, numrows);
      db_unlock(mdb);
      return 0;
   }
   
   /* Seek to desired item 
    * Note, we use base 1; SQL uses base 0
    */
   sql_data_seek(mdb, item-1);

   if ((row = sql_fetch_row(mdb)) == NULL) {
      Mmsg1(&mdb->errmsg, _("No Volume record found for item %d.\n"), item);
      sql_free_result(mdb);
      db_unlock(mdb);
      return 0;
   }

   /* Return fields in Media Record */
   mr->MediaId = atoi(row[0]);
   strcpy(mr->VolumeName, row[1]);
   mr->VolJobs = atoi(row[2]);
   mr->VolFiles = atoi(row[3]);
   mr->VolBlocks = atoi(row[4]);
   mr->VolBytes = (uint64_t)strtod(row[5], NULL);
   mr->VolMounts = atoi(row[6]);
   mr->VolErrors = atoi(row[7]);
   mr->VolWrites = atoi(row[8]);
   mr->VolMaxBytes = (uint64_t)strtod(row[9], NULL);
   mr->VolCapacityBytes = (uint64_t)strtod(row[10], NULL);
   mr->Slot = atoi(row[11]);

   sql_free_result(mdb);

   db_unlock(mdb);
   return numrows;
}


#endif /* HAVE_MYSQL || HAVE_SQLITE */
