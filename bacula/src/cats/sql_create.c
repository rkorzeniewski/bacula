/*
 * Bacula Catalog Database Create record interface routines
 * 
 *    Kern Sibbald, March 2000
 *
 *    Version $Id$
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

/* Forward referenced subroutines */
static int db_create_file_record(JCR *jcr, B_DB *mdb, ATTR_DBR *ar);
static int db_create_filename_record(JCR *jcr, B_DB *mdb, ATTR_DBR *ar);
static int db_create_path_record(JCR *jcr, B_DB *mdb, ATTR_DBR *ar);


/* Imported subroutines */
extern void print_dashes(B_DB *mdb);
extern void print_result(B_DB *mdb);
extern int QueryDB(char *file, int line, JCR *jcr, B_DB *db, char *select_cmd);
extern int InsertDB(char *file, int line, JCR *jcr, B_DB *db, char *select_cmd);
extern void split_path_and_filename(JCR *jcr, B_DB *mdb, char *fname);


/* Create a new record for the Job
 * Returns: 0 on failure
 *	    1 on success
 */
int
db_create_job_record(JCR *jcr, B_DB *mdb, JOB_DBR *jr)
{
   char dt[MAX_TIME_LENGTH];
   time_t stime;
   struct tm tm;
   int stat;
   char JobId[30];
   utime_t JobTDate;
   char ed1[30];

   db_lock(mdb);

   stime = jr->SchedTime;
   ASSERT(stime != 0);

   localtime_r(&stime, &tm); 
   strftime(dt, sizeof(dt), "%Y-%m-%d %T", &tm);
   JobTDate = (utime_t)stime;

   if (!db_next_index(jcr, mdb, "Job", JobId)) {
      jr->JobId = 0;
      db_unlock(mdb);
      return 0;
   }
   /* Must create it */
   Mmsg(&mdb->cmd,
"INSERT INTO Job (JobId,Job,Name,Type,Level,JobStatus,SchedTime,JobTDate) VALUES \
(%s,'%s','%s','%c','%c','%c','%s',%s)", 
	   JobId, jr->Job, jr->Name, (char)(jr->Type), (char)(jr->Level), 
	   (char)(jr->JobStatus), dt, edit_uint64(JobTDate, ed1));

   if (!INSERT_DB(jcr, mdb, mdb->cmd)) {
      Mmsg2(&mdb->errmsg, _("Create DB Job record %s failed. ERR=%s\n"), 
	    mdb->cmd, sql_strerror(mdb));
      jr->JobId = 0;
      stat = 0;
   } else {
      jr->JobId = sql_insert_id(mdb);
      stat = 1;
   }
   db_unlock(mdb);
   return stat;
}

/* Create a JobMedia record for medium used this job   
 * Returns: 0 on failure
 *	    1 on success
 */
int
db_create_jobmedia_record(JCR *jcr, B_DB *mdb, JOBMEDIA_DBR *jm)
{
   int stat;
   int count;

   db_lock(mdb);
#ifdef not_used_in_new_code
   Mmsg(&mdb->cmd, "SELECT JobId, MediaId FROM JobMedia WHERE \
JobId=%d AND MediaId=%d", jm->JobId, jm->MediaId);

   Dmsg0(30, mdb->cmd);
   if (QUERY_DB(jcr, mdb, mdb->cmd)) {
      mdb->num_rows = sql_num_rows(mdb);
      if (mdb->num_rows > 0) {
         Mmsg0(&mdb->errmsg, _("Create JobMedia failed. Record already exists.\n"));
	 sql_free_result(mdb);
	 db_unlock(mdb);
         Dmsg0(0, "Already have JobMedia record\n");
	 return 0;
      }
      sql_free_result(mdb);
   }
#endif

   /* Now get count for VolIndex */
   Mmsg(&mdb->cmd, "SELECT count(*) from JobMedia");
   count = get_sql_record_max(jcr, mdb);
   if (count < 0) {
      count = 0;
   }
   count++;

   /* Must create it */
   Mmsg(&mdb->cmd, 
"INSERT INTO JobMedia (JobId,MediaId,FirstIndex,LastIndex,\
StartFile,EndFile,StartBlock,EndBlock,VolIndex) \
VALUES (%u,%u,%u,%u,%u,%u,%u,%u,%u)", 
       jm->JobId, jm->MediaId, jm->FirstIndex, jm->LastIndex,
       jm->StartFile, jm->EndFile, jm->StartBlock, jm->EndBlock,count);

   Dmsg0(30, mdb->cmd);
   if (!INSERT_DB(jcr, mdb, mdb->cmd)) {
      Mmsg2(&mdb->errmsg, _("Create db JobMedia record %s failed. ERR=%s\n"), mdb->cmd, 
	 sql_strerror(mdb));
      stat = 0;
   } else {
      stat = 1;
   }
   db_unlock(mdb);
   Dmsg0(30, "Return from JobMedia\n");
   return stat;
}



/* Create Unique Pool record
 * Returns: 0 on failure
 *	    1 on success
 */
int
db_create_pool_record(JCR *jcr, B_DB *mdb, POOL_DBR *pr)
{
   int stat;
   char ed1[30], ed2[30], ed3[50];

   Dmsg0(200, "In create pool\n");
   db_lock(mdb);
   Mmsg(&mdb->cmd, "SELECT PoolId,Name FROM Pool WHERE Name='%s'", pr->Name);
   Dmsg1(200, "selectpool: %s\n", mdb->cmd);

   if (QUERY_DB(jcr, mdb, mdb->cmd)) {

      mdb->num_rows = sql_num_rows(mdb);
   
      if (mdb->num_rows > 0) {
         Mmsg1(&mdb->errmsg, _("pool record %s already exists\n"), pr->Name);
	 sql_free_result(mdb);
	 db_unlock(mdb);
	 return 0;
      }
      sql_free_result(mdb);
   }

   /* Must create it */
   Mmsg(&mdb->cmd, 
"INSERT INTO Pool (Name,NumVols,MaxVols,UseOnce,UseCatalog,\
AcceptAnyVolume,AutoPrune,Recycle,VolRetention,VolUseDuration,\
MaxVolJobs,MaxVolFiles,MaxVolBytes,PoolType,LabelFormat) \
VALUES ('%s',%u,%u,%d,%d,%d,%d,%d,%s,%s,%u,%u,%s,'%s','%s')", 
		  pr->Name,
		  pr->NumVols, pr->MaxVols,
		  pr->UseOnce, pr->UseCatalog,
		  pr->AcceptAnyVolume,
		  pr->AutoPrune, pr->Recycle,
		  edit_uint64(pr->VolRetention, ed1),
		  edit_uint64(pr->VolUseDuration, ed2),
		  pr->MaxVolJobs, pr->MaxVolFiles,
		  edit_uint64(pr->MaxVolBytes, ed3),
		  pr->PoolType, pr->LabelFormat);
   Dmsg1(200, "Create Pool: %s\n", mdb->cmd);
   if (!INSERT_DB(jcr, mdb, mdb->cmd)) {
      Mmsg2(&mdb->errmsg, _("Create db Pool record %s failed: ERR=%s\n"), 
	    mdb->cmd, sql_strerror(mdb));
      pr->PoolId = 0;
      stat = 0;
   } else {
      pr->PoolId = sql_insert_id(mdb);
      stat = 1;
   }
   db_unlock(mdb);
   
   return stat;
}


/* 
 * Create Media record. VolumeName and non-zero Slot must be unique
 *
 * Returns: 0 on failure
 *	    1 on success
 */ 
int
db_create_media_record(JCR *jcr, B_DB *mdb, MEDIA_DBR *mr)
{
   int stat;
   char ed1[30], ed2[30], ed3[30], ed4[30], ed5[30];
   char dt[MAX_TIME_LENGTH];
   struct tm tm;

   db_lock(mdb);
   Mmsg(&mdb->cmd, "SELECT MediaId FROM Media WHERE VolumeName='%s'", 
	   mr->VolumeName);
   Dmsg1(110, "selectpool: %s\n", mdb->cmd);

   if (QUERY_DB(jcr, mdb, mdb->cmd)) {
      mdb->num_rows = sql_num_rows(mdb);
      if (mdb->num_rows > 0) {
         Mmsg1(&mdb->errmsg, _("Volume \"%s\" already exists.\n"), mr->VolumeName);
	 sql_free_result(mdb);
	 db_unlock(mdb);
	 return 0;
      }
      sql_free_result(mdb);
   }

   /* Make sur Slot, if non-zero, is unique */
   db_make_slot_unique(jcr, mdb, mr);

   /* Must create it */
   if (mr->LabelDate) {
      localtime_r(&mr->LabelDate, &tm); 
      strftime(dt, sizeof(dt), "%Y-%m-%d %T", &tm);
   } else {
      bstrncpy(dt, "0000-00-00 00:00:00", sizeof(dt));
   }
   Mmsg(&mdb->cmd, 
"INSERT INTO Media (VolumeName,MediaType,PoolId,MaxVolBytes,VolCapacityBytes," 
"Recycle,VolRetention,VolUseDuration,MaxVolJobs,MaxVolFiles,"
"VolStatus,LabelDate,Slot,VolBytes) "
"VALUES ('%s','%s',%u,%s,%s,%d,%s,%s,%u,%u,'%s','%s',%d,%s)", 
		  mr->VolumeName,
		  mr->MediaType, mr->PoolId, 
		  edit_uint64(mr->MaxVolBytes,ed1),
		  edit_uint64(mr->VolCapacityBytes, ed2),
		  mr->Recycle,
		  edit_uint64(mr->VolRetention, ed3),
		  edit_uint64(mr->VolUseDuration, ed4),
		  mr->MaxVolJobs,
		  mr->MaxVolFiles,
		  mr->VolStatus, dt,
		  mr->Slot,
		  edit_uint64(mr->VolBytes, ed5));

   Dmsg1(500, "Create Volume: %s\n", mdb->cmd);
   if (!INSERT_DB(jcr, mdb, mdb->cmd)) {
      Mmsg2(&mdb->errmsg, _("Create DB Media record %s failed. ERR=%s\n"),
	    mdb->cmd, sql_strerror(mdb));
      stat = 0;
   } else {
      mr->MediaId = sql_insert_id(mdb);
      stat = 1;
   }
   db_unlock(mdb);
   return stat;
}



/* 
 * Create a Unique record for the client -- no duplicates 
 * Returns: 0 on failure
 *	    1 on success with id in cr->ClientId
 */
int db_create_client_record(JCR *jcr, B_DB *mdb, CLIENT_DBR *cr)
{
   SQL_ROW row;
   int stat;
   char ed1[30], ed2[30];

   db_lock(mdb);
   Mmsg(&mdb->cmd, "SELECT ClientId,Uname FROM Client WHERE Name='%s'", cr->Name);

   cr->ClientId = 0;
   if (QUERY_DB(jcr, mdb, mdb->cmd)) {

      mdb->num_rows = sql_num_rows(mdb);
      
      /* If more than one, report error, but return first row */
      if (mdb->num_rows > 1) {
         Mmsg1(&mdb->errmsg, _("More than one Client!: %d\n"), (int)(mdb->num_rows));
         Jmsg(jcr, M_ERROR, 0, "%s", mdb->errmsg);
      }
      if (mdb->num_rows >= 1) {
	 if ((row = sql_fetch_row(mdb)) == NULL) {
            Mmsg1(&mdb->errmsg, _("error fetching Client row: %s\n"), sql_strerror(mdb));
            Jmsg(jcr, M_ERROR, 0, "%s", mdb->errmsg);
	    sql_free_result(mdb);
	    db_unlock(mdb);
	    return 0;
	 }
	 cr->ClientId = atoi(row[0]);
	 if (row[1]) {
	    bstrncpy(cr->Uname, row[1], sizeof(cr->Uname));
	 } else {
	    cr->Uname[0] = 0;	      /* no name */
	 }
	 sql_free_result(mdb);
	 db_unlock(mdb);
	 return 1;
      }
      sql_free_result(mdb);
   }

   /* Must create it */
   Mmsg(&mdb->cmd, "INSERT INTO Client (Name, Uname, AutoPrune, \
FileRetention, JobRetention) VALUES \
('%s', '%s', %d, %s, %s)", cr->Name, cr->Uname, cr->AutoPrune,
      edit_uint64(cr->FileRetention, ed1),
      edit_uint64(cr->JobRetention, ed2));

   if (!INSERT_DB(jcr, mdb, mdb->cmd)) {
      Mmsg2(&mdb->errmsg, _("Create DB Client record %s failed. ERR=%s\n"),
	    mdb->cmd, sql_strerror(mdb));
      Jmsg(jcr, M_ERROR, 0, "%s", mdb->errmsg);
      cr->ClientId = 0;
      stat = 0;
   } else {
      cr->ClientId = sql_insert_id(mdb);
      stat = 1;
   }
   db_unlock(mdb);
   return stat;
}


/* 
 * Create a Unique record for the counter -- no duplicates 
 * Returns: 0 on failure
 *	    1 on success with counter filled in
 */
int db_create_counter_record(JCR *jcr, B_DB *mdb, COUNTER_DBR *cr)
{
   COUNTER_DBR mcr;
   int stat;

   db_lock(mdb);
   memset(&mcr, 0, sizeof(mcr));
   bstrncpy(mcr.Counter, cr->Counter, sizeof(mcr.Counter));
   if (db_get_counter_record(jcr, mdb, &mcr)) {
      memcpy(cr, &mcr, sizeof(COUNTER_DBR));
      db_unlock(mdb);
      return 1;
   }

   /* Must create it */
   Mmsg(&mdb->cmd, "INSERT INTO Counters (Counter,MinValue,MaxValue,CurrentValue,"
      "WrapCounter) VALUES ('%s','%d','%d','%d','%s')",
      cr->Counter, cr->MinValue, cr->MaxValue, cr->CurrentValue,
      cr->WrapCounter);

   if (!INSERT_DB(jcr, mdb, mdb->cmd)) {
      Mmsg2(&mdb->errmsg, _("Create DB Counters record %s failed. ERR=%s\n"),
	    mdb->cmd, sql_strerror(mdb));
      Jmsg(jcr, M_ERROR, 0, "%s", mdb->errmsg);
      stat = 0;
   } else {
      stat = 1;
   }
   db_unlock(mdb);
   return stat;
}


/* 
 * Create a FileSet record. This record is unique in the
 *  name and the MD5 signature of the include/exclude sets.
 *  Returns: 0 on failure
 *	     1 on success with FileSetId in record
 */
int db_create_fileset_record(JCR *jcr, B_DB *mdb, FILESET_DBR *fsr)
{
   SQL_ROW row;
   int stat;
   struct tm tm;

   db_lock(mdb);
   fsr->created = false;
   Mmsg(&mdb->cmd, "SELECT FileSetId,CreateTime FROM FileSet WHERE "
"FileSet='%s' AND MD5='%s'", fsr->FileSet, fsr->MD5);

   fsr->FileSetId = 0;
   if (QUERY_DB(jcr, mdb, mdb->cmd)) {

      mdb->num_rows = sql_num_rows(mdb);
      
      if (mdb->num_rows > 1) {
         Mmsg1(&mdb->errmsg, _("More than one FileSet!: %d\n"), (int)(mdb->num_rows));
         Jmsg(jcr, M_ERROR, 0, "%s", mdb->errmsg);
      }
      if (mdb->num_rows >= 1) {
	 if ((row = sql_fetch_row(mdb)) == NULL) {
            Mmsg1(&mdb->errmsg, _("error fetching FileSet row: ERR=%s\n"), sql_strerror(mdb));
            Jmsg(jcr, M_ERROR, 0, "%s", mdb->errmsg);
	    sql_free_result(mdb);
	    db_unlock(mdb);
	    return 0;
	 }
	 fsr->FileSetId = atoi(row[0]);
	 if (row[1] == NULL) {
	    fsr->cCreateTime[0] = 0;
	 } else {
	    bstrncpy(fsr->cCreateTime, row[1], sizeof(fsr->cCreateTime));
	 }
	 sql_free_result(mdb);
	 db_unlock(mdb);
	 return 1;
      }
      sql_free_result(mdb);
   }

   if (fsr->CreateTime == 0 && fsr->cCreateTime[0] == 0) {
      fsr->CreateTime = time(NULL);
   }
   localtime_r(&fsr->CreateTime, &tm);
   strftime(fsr->cCreateTime, sizeof(fsr->cCreateTime), "%Y-%m-%d %T", &tm);

   /* Must create it */
      Mmsg(&mdb->cmd, "INSERT INTO FileSet (FileSet,MD5,CreateTime) "
"VALUES ('%s','%s','%s')", fsr->FileSet, fsr->MD5, fsr->cCreateTime);

   if (!INSERT_DB(jcr, mdb, mdb->cmd)) {
      Mmsg2(&mdb->errmsg, _("Create DB FileSet record %s failed. ERR=%s\n"),
	    mdb->cmd, sql_strerror(mdb));
      Jmsg(jcr, M_ERROR, 0, "%s", mdb->errmsg);
      fsr->FileSetId = 0;
      stat = 0;
   } else {
      fsr->FileSetId = sql_insert_id(mdb);
      fsr->created = true;
      stat = 1;
   }

   db_unlock(mdb);
   return stat;
}


/*
 *  struct stat
 *  {
 *	dev_t	      st_dev;	    * device *
 *	ino_t	      st_ino;	    * inode *
 *	mode_t	      st_mode;	    * protection *
 *	nlink_t       st_nlink;     * number of hard links *
 *	uid_t	      st_uid;	    * user ID of owner *
 *	gid_t	      st_gid;	    * group ID of owner *
 *	dev_t	      st_rdev;	    * device type (if inode device) *
 *	off_t	      st_size;	    * total size, in bytes *
 *	unsigned long st_blksize;   * blocksize for filesystem I/O *
 *	unsigned long st_blocks;    * number of blocks allocated *
 *	time_t	      st_atime;     * time of last access *
 *	time_t	      st_mtime;     * time of last modification *
 *	time_t	      st_ctime;     * time of last inode change *	     
 *  };
 */



/* 
 * Create File record in B_DB	
 *
 *  In order to reduce database size, we store the File attributes,
 *  the FileName, and the Path separately.  In principle, there   
 *  is a single FileName record and a single Path record, no matter
 *  how many times it occurs.  This is this subroutine, we separate
 *  the file and the path and create three database records.
 */
int db_create_file_attributes_record(JCR *jcr, B_DB *mdb, ATTR_DBR *ar)
{

   Dmsg1(100, "Fname=%s\n", ar->fname);
   Dmsg0(50, "put_file_into_catalog\n");
   /*
    * Make sure we have an acceptable attributes record.
    */
   if (!(ar->Stream == STREAM_UNIX_ATTRIBUTES || 
	 ar->Stream == STREAM_UNIX_ATTRIBUTES_EX)) {
      Mmsg0(&mdb->errmsg, _("Attempt to put non-attributes into catalog\n"));
      Jmsg(jcr, M_ERROR, 0, "%s", mdb->errmsg);
      return 0;
   }

   db_lock(mdb);

   split_path_and_filename(jcr, mdb, ar->fname);

   if (!db_create_filename_record(jcr, mdb, ar)) {
      db_unlock(mdb);
      return 0;
   }
   Dmsg1(500, "db_create_filename_record: %s\n", mdb->esc_name);


   if (!db_create_path_record(jcr, mdb, ar)) {
      db_unlock(mdb);
      return 0;
   }
   Dmsg1(500, "db_create_path_record: %s\n", mdb->esc_name);

   /* Now create master File record */
   if (!db_create_file_record(jcr, mdb, ar)) {
      db_unlock(mdb);
      return 0;
   }
   Dmsg0(500, "db_create_file_record OK\n");

   Dmsg3(100, "CreateAttributes Path=%s File=%s FilenameId=%d\n", mdb->path, mdb->fname, ar->FilenameId);
   db_unlock(mdb);
   return 1;
}

/*
 * This is the master File entry containing the attributes.
 *  The filename and path records have already been created.
 */
static int db_create_file_record(JCR *jcr, B_DB *mdb, ATTR_DBR *ar)
{
   int stat;

   ASSERT(ar->JobId);
   ASSERT(ar->PathId);
   ASSERT(ar->FilenameId);

   /* Must create it */
   Mmsg(&mdb->cmd,
"INSERT INTO File (FileIndex,JobId,PathId,FilenameId,"
"LStat,MD5) VALUES (%u,%u,%u,%u,'%s','0')", 
      ar->FileIndex, ar->JobId, ar->PathId, ar->FilenameId, 
      ar->attr);

   if (!INSERT_DB(jcr, mdb, mdb->cmd)) {
      Mmsg2(&mdb->errmsg, _("Create db File record %s failed. ERR=%s"),       
	 mdb->cmd, sql_strerror(mdb));
      Jmsg(jcr, M_ERROR, 0, "%s", mdb->errmsg);
      ar->FileId = 0;
      stat = 0;
   } else {
      ar->FileId = sql_insert_id(mdb);
      stat = 1;
   }
   return stat;
}

/* Create a Unique record for the Path -- no duplicates */
static int db_create_path_record(JCR *jcr, B_DB *mdb, ATTR_DBR *ar)
{
   SQL_ROW row;
   int stat;

   mdb->esc_name = check_pool_memory_size(mdb->esc_name, 2*mdb->pnl+2);
   db_escape_string(mdb->esc_name, mdb->path, mdb->pnl);

   if (mdb->cached_path_id != 0 && mdb->cached_path_len == mdb->pnl &&
       strcmp(mdb->cached_path, mdb->path) == 0) {
      ar->PathId = mdb->cached_path_id;
      return 1;
   }	      

   Mmsg(&mdb->cmd, "SELECT PathId FROM Path WHERE Path='%s'", mdb->esc_name);

   if (QUERY_DB(jcr, mdb, mdb->cmd)) {
      mdb->num_rows = sql_num_rows(mdb);
      if (mdb->num_rows > 1) {
	 char ed1[30];
         Mmsg2(&mdb->errmsg, _("More than one Path!: %s for path: %s\n"), 
	    edit_uint64(mdb->num_rows, ed1), mdb->path);
         Jmsg(jcr, M_WARNING, 0, "%s", mdb->errmsg);
      }
      /* Even if there are multiple paths, take the first one */
      if (mdb->num_rows >= 1) {
	 if ((row = sql_fetch_row(mdb)) == NULL) {
            Mmsg1(&mdb->errmsg, _("error fetching row: %s\n"), sql_strerror(mdb));
            Jmsg(jcr, M_ERROR, 0, "%s", mdb->errmsg);
	    sql_free_result(mdb);
	    ar->PathId = 0;
	    ASSERT(ar->PathId);
	    return 0;
	 }
	 ar->PathId = atoi(row[0]);
	 sql_free_result(mdb);
	 /* Cache path */
	 if (ar->PathId != mdb->cached_path_id) {
	    mdb->cached_path_id = ar->PathId;
	    mdb->cached_path_len = mdb->pnl;
	    pm_strcpy(&mdb->cached_path, mdb->path);
	 }
	 ASSERT(ar->PathId);
	 return 1;
      }

      sql_free_result(mdb);
   }

   Mmsg(&mdb->cmd, "INSERT INTO Path (Path) VALUES ('%s')", mdb->esc_name);

   if (!INSERT_DB(jcr, mdb, mdb->cmd)) {
      Mmsg2(&mdb->errmsg, _("Create db Path record %s failed. ERR=%s\n"), 
	 mdb->cmd, sql_strerror(mdb));
      Jmsg(jcr, M_ERROR, 0, "%s", mdb->errmsg);
      ar->PathId = 0;
      stat = 0;
   } else {
      ar->PathId = sql_insert_id(mdb);
      stat = 1;
   }

   /* Cache path */
   if (stat && ar->PathId != mdb->cached_path_id) {
      mdb->cached_path_id = ar->PathId;
      mdb->cached_path_len = mdb->pnl;
      pm_strcpy(&mdb->cached_path, mdb->path);
   }
   return stat;
}

/* Create a Unique record for the filename -- no duplicates */
static int db_create_filename_record(JCR *jcr, B_DB *mdb, ATTR_DBR *ar) 
{
   SQL_ROW row;

   mdb->esc_name = check_pool_memory_size(mdb->esc_name, 2*mdb->fnl+2);
   db_escape_string(mdb->esc_name, mdb->fname, mdb->fnl);

   Mmsg(&mdb->cmd, "SELECT FilenameId FROM Filename WHERE Name='%s'", mdb->esc_name);

   if (QUERY_DB(jcr, mdb, mdb->cmd)) {
      mdb->num_rows = sql_num_rows(mdb);
      if (mdb->num_rows > 1) {
	 char ed1[30];
         Mmsg2(&mdb->errmsg, _("More than one Filename! %s for file: %s\n"), 
	    edit_uint64(mdb->num_rows, ed1), mdb->fname);
         Jmsg(jcr, M_WARNING, 0, "%s", mdb->errmsg);
      }
      if (mdb->num_rows >= 1) {
	 if ((row = sql_fetch_row(mdb)) == NULL) {
            Mmsg2(&mdb->errmsg, _("Error fetching row for file=%s: ERR=%s\n"), 
		mdb->fname, sql_strerror(mdb));
            Jmsg(jcr, M_ERROR, 0, "%s", mdb->errmsg);
	    ar->FilenameId = 0;
	 } else {
	    ar->FilenameId = atoi(row[0]);
	 }
	 sql_free_result(mdb);
	 return ar->FilenameId > 0;
      }
      sql_free_result(mdb);
   }

   Mmsg(&mdb->cmd, "INSERT INTO Filename (Name) VALUES ('%s')", mdb->esc_name);

   if (!INSERT_DB(jcr, mdb, mdb->cmd)) {
      Mmsg2(&mdb->errmsg, _("Create db Filename record %s failed. ERR=%s\n"), 
	    mdb->cmd, sql_strerror(mdb));
      Jmsg(jcr, M_ERROR, 0, "%s", mdb->errmsg);
      ar->FilenameId = 0;
   } else {
      ar->FilenameId = sql_insert_id(mdb);
   }
   return ar->FilenameId > 0;
}

#endif /* HAVE_MYSQL || HAVE_SQLITE */
