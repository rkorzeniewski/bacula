/*
 * Bacula Catalog Database Update record interface routines
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
extern void print_result(B_DB *mdb);
extern int UpdateDB(char *file, int line, B_DB *db, char *update_cmd);

/* -----------------------------------------------------------------------
 *
 *   Generic Routines (or almost generic)
 *
 * -----------------------------------------------------------------------
 */
/* Update the attributes record by adding the MD5 signature */
int
db_add_MD5_to_file_record(B_DB *mdb, FileId_t FileId, char *MD5)
{
   int stat;

   db_lock(mdb);
   Mmsg(&mdb->cmd, "UPDATE File SET MD5='%s' WHERE FileId=%d", MD5, FileId);
   stat = UPDATE_DB(mdb, mdb->cmd);
   db_unlock(mdb);
   return stat;
}

/* Mark the file record as being visited during database
 * verify compare. Stuff JobId into MarkedId field
 */
int db_mark_file_record(B_DB *mdb, FileId_t FileId, JobId_t JobId) 
{
   int stat;

   db_lock(mdb);
   Mmsg(&mdb->cmd, "UPDATE File SET MarkId=%d WHERE FileId=%d", JobId, FileId);
   stat = UPDATE_DB(mdb, mdb->cmd);
   db_unlock(mdb);
   return stat;
}

/*
 * Update the Job record at end of Job
 *
 *  Returns: 0 on failure
 *	     1 on success
 */
int
db_update_job_start_record(B_DB *mdb, JOB_DBR *jr)
{
   char dt[MAX_TIME_LENGTH];
   time_t stime;
   struct tm tm;
   btime_t JobTDate;
   int stat;
   char ed1[30];
       
   stime = jr->StartTime;
   localtime_r(&stime, &tm);
   strftime(dt, sizeof(dt), "%Y-%m-%d %T", &tm);
   JobTDate = (btime_t)stime;

   db_lock(mdb);
   Mmsg(&mdb->cmd, "UPDATE Job SET Level='%c', StartTime='%s', \
ClientId=%d, JobTDate=%s WHERE JobId=%d",
      (char)(jr->Level), dt, jr->ClientId, edit_uint64(JobTDate, ed1), jr->JobId);
   stat = UPDATE_DB(mdb, mdb->cmd);
   db_unlock(mdb);
   mdb->changes = 0;
   return stat;
}



/*
 * Update the Job record at end of Job
 *
 *  Returns: 0 on failure
 *	     1 on success
 */
int
db_update_job_end_record(B_DB *mdb, JOB_DBR *jr)
{
   char dt[MAX_TIME_LENGTH];
   time_t ttime;
   struct tm tm;
   int stat;
   char ed1[30], ed2[30];
   btime_t JobTDate;
       
   ttime = jr->EndTime;
   localtime_r(&ttime, &tm);
   strftime(dt, sizeof(dt), "%Y-%m-%d %T", &tm);
   JobTDate = ttime;

   db_lock(mdb);
   Mmsg(&mdb->cmd,
      "UPDATE Job SET JobStatus='%c', EndTime='%s', \
ClientId=%d, JobBytes=%s, JobFiles=%d, JobErrors=%d, VolSessionId=%d, \
VolSessionTime=%d, PoolId=%d, FileSetId=%d, JobTDate=%s WHERE JobId=%d",
      (char)(jr->JobStatus), dt, jr->ClientId, edit_uint64(jr->JobBytes, ed1), 
      jr->JobFiles, jr->JobErrors, jr->VolSessionId, jr->VolSessionTime, 
      jr->PoolId, jr->FileSetId, edit_uint64(JobTDate, ed2), jr->JobId);

   stat = UPDATE_DB(mdb, mdb->cmd);
   db_unlock(mdb);
   return stat;
}


int
db_update_pool_record(B_DB *mdb, POOL_DBR *pr)
{
   int stat;

   db_lock(mdb);
   Mmsg(&mdb->cmd,
"UPDATE Pool SET NumVols=%d, MaxVols=%d, UseOnce=%d, UseCatalog=%d, \
AcceptAnyVolume=%d, LabelFormat='%s' WHERE PoolId=%d",
      pr->NumVols, pr->MaxVols, pr->UseOnce, pr->UseCatalog,
      pr->AcceptAnyVolume, pr->LabelFormat, pr->PoolId);

   stat = UPDATE_DB(mdb, mdb->cmd);
   db_unlock(mdb);
   return stat;
}

/* 
 * Update the Media Record at end of Session
 *
 * Returns: 0 on failure
 *	    numrows on success
 */
int
db_update_media_record(B_DB *mdb, MEDIA_DBR *mr) 
{
   char dt[MAX_TIME_LENGTH];
   time_t ttime;
   struct tm tm;
   int stat;
   char ed1[30], ed2[30];
       
   ttime = mr->LastWritten;
   localtime_r(&ttime, &tm);
   strftime(dt, sizeof(dt), "%Y-%m-%d %T", &tm);

   Dmsg1(100, "update_media: FirstWritten=%d\n", mr->FirstWritten);
   db_lock(mdb);
   if (mr->VolMounts == 1 && mr->VolBlocks==1 && mr->VolWrites==1) {
      Dmsg1(400, "Set FirstWritten Vol=%s\n", mr->VolumeName);
      Mmsg(&mdb->cmd, "UPDATE Media SET FirstWritten='%s'\
 WHERE VolumeName='%s'", dt, mr->VolumeName);
      stat = UPDATE_DB(mdb, mdb->cmd);
      Dmsg1(400, "Firstwritten stat=%d\n", stat);
   }

   Mmsg(&mdb->cmd, "UPDATE Media SET VolJobs=%d,\
 VolFiles=%d, VolBlocks=%d, VolBytes=%s, VolMounts=%d, VolErrors=%d,\
 VolWrites=%d, VolMaxBytes=%s, LastWritten='%s', VolStatus='%s',\
 Slot=%d WHERE VolumeName='%s'",
   mr->VolJobs, mr->VolFiles, mr->VolBlocks, edit_uint64(mr->VolBytes, ed1),
   mr->VolMounts, mr->VolErrors, mr->VolWrites, 
   edit_uint64(mr->VolMaxBytes, ed2), dt, 
   mr->VolStatus, mr->Slot, mr->VolumeName);

   Dmsg1(400, "%s\n", mdb->cmd);

   stat = UPDATE_DB(mdb, mdb->cmd);
   db_unlock(mdb);
   return stat;
}

#endif /* HAVE_MYSQL || HAVE_SQLITE */
