/*
 * Bacula Catalog Database Create record interface routines
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

/* Forward referenced subroutines */
static int db_create_file_record(B_DB *mdb, ATTR_DBR *ar);
static int db_create_filename_record(B_DB *mdb, ATTR_DBR *ar, char *fname);
static int db_create_path_record(B_DB *mdb, ATTR_DBR *ar, char *path);


/* Imported subroutines */
extern void print_dashes(B_DB *mdb);
extern void print_result(B_DB *mdb);
extern int QueryDB(char *file, int line, B_DB *db, char *select_cmd);
extern int InsertDB(char *file, int line, B_DB *db, char *select_cmd);


/* Create a new record for the Job
 * Returns: 0 on failure
 *	    1 on success
 */
int
db_create_job_record(B_DB *mdb, JOB_DBR *jr)
{
   char dt[MAX_TIME_LENGTH];
   time_t stime;
   struct tm tm;
   int stat;
   char *JobId;
   btime_t JobTDate;
   char ed1[30];

   stime = jr->SchedTime;

   localtime_r(&stime, &tm); 
   strftime(dt, sizeof(dt), "%Y-%m-%d %T", &tm);
   JobTDate = (btime_t)stime;

   db_lock(mdb);
   JobId = db_next_index(mdb, "Job");
   if (!JobId) {
      jr->JobId = 0;
      db_unlock(mdb);
      return 0;
   }
   /* Must create it */
   Mmsg(&mdb->cmd,
"INSERT INTO Job (JobId, Job, Name, Type, Level, SchedTime, JobTDate) VALUES \
(%s, \"%s\", \"%s\", \"%c\", \"%c\", \"%s\", %s)", 
	   JobId, jr->Job, jr->Name, (char)(jr->Type), (char)(jr->Level), dt,
	   edit_uint64(JobTDate, ed1));

   if (!INSERT_DB(mdb, mdb->cmd)) {
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
db_create_jobmedia_record(B_DB *mdb, JOBMEDIA_DBR *jm)
{
   int stat;

   db_lock(mdb);
   Mmsg(&mdb->cmd, "SELECT JobId, MediaId FROM JobMedia WHERE \
JobId=%d AND MediaId=%d", jm->JobId, jm->MediaId);

   Dmsg0(30, mdb->cmd);
   if (QUERY_DB(mdb, mdb->cmd)) {
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

   /* Must create it */
   Mmsg(&mdb->cmd, 
"INSERT INTO JobMedia (JobId, MediaId, FirstIndex, LastIndex) \
VALUES (%d, %d, %u, %u)", 
       jm->JobId, jm->MediaId, jm->FirstIndex, jm->LastIndex);

   Dmsg0(30, mdb->cmd);
   if (!INSERT_DB(mdb, mdb->cmd)) {
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
db_create_pool_record(B_DB *mdb, POOL_DBR *pr)
{
   int stat;
   char ed1[30];

   db_lock(mdb);
   Mmsg(&mdb->cmd, "SELECT PoolId,Name FROM Pool WHERE Name=\"%s\"", pr->Name);
   Dmsg1(20, "selectpool: %s\n", mdb->cmd);

   if (QUERY_DB(mdb, mdb->cmd)) {

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
"INSERT INTO Pool (Name, NumVols, MaxVols, UseOnce, UseCatalog, \
AcceptAnyVolume, AutoPrune, Recycle, VolRetention, PoolType, LabelFormat) \
VALUES (\"%s\", %d, %d, %d, %d, %d, %d, %d, %s, \"%s\", \"%s\")", 
		  pr->Name,
		  pr->NumVols, pr->MaxVols,
		  pr->UseOnce, pr->UseCatalog,
		  pr->AcceptAnyVolume,
		  pr->AutoPrune, pr->Recycle,
		  edit_uint64(pr->VolRetention, ed1),
		  pr->PoolType, pr->LabelFormat);
   Dmsg1(500, "Create Pool: %s\n", mdb->cmd);
   if (!INSERT_DB(mdb, mdb->cmd)) {
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
 * Create Unique Media record	
 * Returns: 0 on failure
 *	    1 on success
 */ 
int
db_create_media_record(B_DB *mdb, MEDIA_DBR *mr)
{
   int stat;
   char ed1[30], ed2[30], ed3[30];

   db_lock(mdb);
   Mmsg(&mdb->cmd, "SELECT MediaId FROM Media WHERE VolumeName=\"%s\"", 
	   mr->VolumeName);
   Dmsg1(110, "selectpool: %s\n", mdb->cmd);

   if (QUERY_DB(mdb, mdb->cmd)) {
      mdb->num_rows = sql_num_rows(mdb);
      if (mdb->num_rows > 0) {
         Mmsg1(&mdb->errmsg, _("Media record %s already exists\n"), mr->VolumeName);
	 sql_free_result(mdb);
	 db_unlock(mdb);
	 return 0;
      }
      sql_free_result(mdb);
   }

   /* Must create it */
   Mmsg(&mdb->cmd, 
"INSERT INTO Media (VolumeName, MediaType, PoolId, VolMaxBytes, VolCapacityBytes, \
Recycle, VolRetention, VolStatus) VALUES (\"%s\", \"%s\", %d, %s, %s, %d, %s, \"%s\")", 
		  mr->VolumeName,
		  mr->MediaType, mr->PoolId, 
		  edit_uint64(mr->VolMaxBytes,ed1),
		  edit_uint64(mr->VolCapacityBytes, ed2),
		  mr->Recycle,
		  edit_uint64(mr->VolRetention, ed3),
		  mr->VolStatus);

   Dmsg1(500, "Create Volume: %s\n", mdb->cmd);
   if (!INSERT_DB(mdb, mdb->cmd)) {
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
int db_create_client_record(B_DB *mdb, CLIENT_DBR *cr)
{
   SQL_ROW row;
   int stat;
   char ed1[30], ed2[30];

   db_lock(mdb);
   Mmsg(&mdb->cmd, "SELECT ClientId FROM Client WHERE Name=\"%s\"", cr->Name);

   cr->ClientId = 0;
   if (QUERY_DB(mdb, mdb->cmd)) {

      mdb->num_rows = sql_num_rows(mdb);
      
      /* If more than one, report error, but return first row */
      if (mdb->num_rows > 1) {
         Mmsg1(&mdb->errmsg, _("More than one Client!: %d\n"), (int)(mdb->num_rows));
	 Emsg0(M_ERROR, 0, mdb->errmsg);
      }
      if (mdb->num_rows >= 1) {
	 if ((row = sql_fetch_row(mdb)) == NULL) {
            Mmsg1(&mdb->errmsg, _("error fetching Client row: %s\n"), sql_strerror(mdb));
	    sql_free_result(mdb);
	    db_unlock(mdb);
	    return 0;
	 }
	 sql_free_result(mdb);
	 cr->ClientId = atoi(row[0]);
	 db_unlock(mdb);
	 return 1;
      }
      sql_free_result(mdb);
   }

   /* Must create it */
   Mmsg(&mdb->cmd, "INSERT INTO Client (Name, Uname, AutoPrune, \
FileRetention, JobRetention) VALUES \
(\"%s\", \"%s\", %d, %s, %s)", cr->Name, cr->Uname, cr->AutoPrune,
      edit_uint64(cr->FileRetention, ed1),
      edit_uint64(cr->JobRetention, ed2));

   if (!INSERT_DB(mdb, mdb->cmd)) {
      Mmsg2(&mdb->errmsg, _("Create DB Client record %s failed. ERR=%s\n"),
	    mdb->cmd, sql_strerror(mdb));
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
 * Create a FileSet record. This record is unique in the
 *  name and the MD5 signature of the include/exclude sets.
 *  Returns: 0 on failure
 *	     1 on success with FileSetId in record
 */
int db_create_fileset_record(B_DB *mdb, FILESET_DBR *fsr)
{
   SQL_ROW row;
   int stat;

   db_lock(mdb);
   Mmsg(&mdb->cmd, "SELECT FileSetId FROM FileSet WHERE \
FileSet=\"%s\" and MD5=\"%s\"", fsr->FileSet, fsr->MD5);

   fsr->FileSetId = 0;
   if (QUERY_DB(mdb, mdb->cmd)) {

      mdb->num_rows = sql_num_rows(mdb);
      
      if (mdb->num_rows > 1) {
         Mmsg1(&mdb->errmsg, _("More than one FileSet!: %d\n"), (int)(mdb->num_rows));
	 Emsg0(M_ERROR, 0, mdb->errmsg);
      }
      if (mdb->num_rows >= 1) {
	 if ((row = sql_fetch_row(mdb)) == NULL) {
            Mmsg1(&mdb->errmsg, _("error fetching FileSet row: ERR=%s\n"), sql_strerror(mdb));
	    sql_free_result(mdb);
	    db_unlock(mdb);
	    return 0;
	 }
	 sql_free_result(mdb);
	 fsr->FileSetId = atoi(row[0]);
	 db_unlock(mdb);
	 return 1;
      }
      sql_free_result(mdb);
   }

   /* Must create it */
   Mmsg(&mdb->cmd, "INSERT INTO FileSet (FileSet, MD5) VALUES \
(\"%s\", \"%s\")", fsr->FileSet, fsr->MD5);

   if (!INSERT_DB(mdb, mdb->cmd)) {
      Mmsg2(&mdb->errmsg, _("Create DB FileSet record %s failed. ERR=%s\n"),
	    mdb->cmd, sql_strerror(mdb));
      fsr->FileSetId = 0;
      stat = 0;
   } else {
      fsr->FileSetId = sql_insert_id(mdb);
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
int db_create_file_attributes_record(B_DB *mdb, ATTR_DBR *ar)
{
   int fnl, pnl;
   char *l, *p;
   /* ****FIXME***** malloc these */
   char file[MAXSTRING];
   char spath[MAXSTRING];
   char buf[MAXSTRING];

   Dmsg1(100, "Fname=%s\n", ar->fname);
   Dmsg0(50, "put_file_into_catalog\n");
   /* For the moment, we only handle Unix attributes.  Note, we are
    * also getting any MD5 signature that was computed.
    */
   if (ar->Stream != STREAM_UNIX_ATTRIBUTES) {
      Mmsg0(&mdb->errmsg, _("Attempt to put non-attributes into catalog\n"));
      return 0;
   }

   /* Find path without the filename.  
    * I.e. everything after the last / is a "filename".
    * OK, maybe it is a directory name, but we treat it like
    * a filename. If we don't find a / then the whole name
    * must be a path name (e.g. c:).
    */
   for (p=l=ar->fname; *p; p++) {
      if (*p == '/') {
	 l = p; 		      /* set pos of last slash */
      }
   }
   if (*l == '/') {                   /* did we find a slash? */
      l++;			      /* yes, point to filename */
   } else {			      /* no, whole thing must be path name */
      l = p;
   }

   /* If filename doesn't exist (i.e. root directory), we
    * simply create a blank name consisting of a single 
    * space. This makes handling zero length filenames
    * easier.
    */
   fnl = p - l;
   if (fnl > 255) {
      Emsg1(M_WARNING, 0, _("Filename truncated to 255 chars: %s\n"), l);
      fnl = 255;
   }
   if (fnl > 0) {
      strncpy(file, l, fnl);	      /* copy filename */
      file[fnl] = 0;
   } else {
      file[0] = ' ';                  /* blank filename */
      file[1] = 0;
   }

   pnl = l - ar->fname;    
   if (pnl > 255) {
      Emsg1(M_WARNING, 0, _("Path name truncated to 255 chars: %s\n"), ar->fname);
      pnl = 255;
   }
   strncpy(spath, ar->fname, pnl);
   spath[pnl] = 0;

   if (pnl == 0) {
      Mmsg1(&mdb->errmsg, _("Path length is zero. File=%s\n"), ar->fname);
      Emsg0(M_ERROR, 0, mdb->errmsg);
      spath[0] = ' ';
      spath[1] = 0;
   }

   Dmsg1(100, "spath=%s\n", spath);
   Dmsg1(100, "file=%s\n", file);

   db_escape_string(buf, file, fnl);

   if (!db_create_filename_record(mdb, ar, buf)) {
      return 0;
   }
   Dmsg1(100, "db_create_filename_record: %s\n", buf);

   db_escape_string(buf, spath, pnl);

   if (!db_create_path_record(mdb, ar, buf)) {
      return 0;
   }
   Dmsg1(100, "db_create_path_record\n", buf);

   if (!db_create_file_record(mdb, ar)) {
      return 0;
   }
   Dmsg0(50, "db_create_file_record\n");

   Dmsg3(100, "Path=%s File=%s FilenameId=%d\n", spath, file, ar->FilenameId);

   return 1;
}

static int db_create_file_record(B_DB *mdb, ATTR_DBR *ar)
{
   int stat;

   ASSERT(ar->JobId);
   ASSERT(ar->PathId);
   ASSERT(ar->FilenameId);

   db_lock(mdb);
   /* Must create it */
   Mmsg(&mdb->cmd,
"INSERT INTO File (FileIndex, JobId, PathId, FilenameId, \
LStat, MD5) VALUES (%d, %d, %d, %d, \"%s\", \"0\")", 
     (int)ar->FileIndex, ar->JobId, ar->PathId, ar->FilenameId, 
      ar->attr);

   if (!INSERT_DB(mdb, mdb->cmd)) {
      Mmsg2(&mdb->errmsg, _("Create db File record %s failed. ERR=%s"),       
	 mdb->cmd, sql_strerror(mdb));
      ar->FileId = 0;
      stat = 0;
   } else {
      ar->FileId = sql_insert_id(mdb);
      stat = 1;
   }
   db_unlock(mdb);
   return stat;
}

/* Create a Unique record for the Path -- no duplicates */
static int db_create_path_record(B_DB *mdb, ATTR_DBR *ar, char *path)
{
   SQL_ROW row;
   int stat;

   if (*path == 0) {
      Mmsg0(&mdb->errmsg, _("Null path given to db_create_path_record\n"));
      ar->PathId = 0;
      return 0;
   }

   db_lock(mdb);

   if (mdb->cached_path_id != 0 && strcmp(mdb->cached_path, path) == 0) {
      ar->PathId = mdb->cached_path_id;
      db_unlock(mdb);
      return 1;
   }	      

   Mmsg(&mdb->cmd, "SELECT PathId FROM Path WHERE Path=\"%s\"", path);

   if (QUERY_DB(mdb, mdb->cmd)) {

      mdb->num_rows = sql_num_rows(mdb);

      if (mdb->num_rows > 1) {
	 char ed1[30];
         Mmsg2(&mdb->errmsg, _("More than one Path!: %s for Path=%s\n"), 
	    edit_uint64(mdb->num_rows, ed1), path);
         Emsg1(M_ERROR, 0, "%s", mdb->errmsg);
         Emsg1(M_ERROR, 0, "%s\n", mdb->cmd);
      }
      if (mdb->num_rows >= 1) {
	 if ((row = sql_fetch_row(mdb)) == NULL) {
	    db_unlock(mdb);
            Mmsg1(&mdb->errmsg, _("error fetching row: %s\n"), sql_strerror(mdb));
	    sql_free_result(mdb);
	    ar->PathId = 0;
	    return 0;
	 }
	 sql_free_result(mdb);
	 ar->PathId = atoi(row[0]);
	 /* Cache path */
	 if (ar->PathId != mdb->cached_path_id) {
	    mdb->cached_path_id = ar->PathId;
	    mdb->cached_path = check_pool_memory_size(mdb->cached_path,
	       strlen(path)+1);
	    strcpy(mdb->cached_path, path);
	 }
	 db_unlock(mdb);
	 return 1;
      }

      sql_free_result(mdb);
   }

   Mmsg(&mdb->cmd, "INSERT INTO Path (Path)  VALUES (\"%s\")", path);

   if (!INSERT_DB(mdb, mdb->cmd)) {
      Mmsg2(&mdb->errmsg, _("Create db Path record %s failed. ERR=%s\n"), 
	 mdb->cmd, sql_strerror(mdb));
      ar->PathId = 0;
      stat = 0;
   } else {
      ar->PathId = sql_insert_id(mdb);
      stat = 1;
   }

   /* Cache path */
   if (ar->PathId != mdb->cached_path_id) {
      mdb->cached_path_id = ar->PathId;
      mdb->cached_path = check_pool_memory_size(mdb->cached_path,
	 strlen(path)+1);
      strcpy(mdb->cached_path, path);
   }
   db_unlock(mdb);
   return stat;
}

/* Create a Unique record for the filename -- no duplicates */
static int db_create_filename_record(B_DB *mdb, ATTR_DBR *ar, char *fname) 
{
   SQL_ROW row;

   db_lock(mdb);
   Mmsg(&mdb->cmd, "SELECT FilenameId FROM Filename WHERE Name=\"%s\"", fname);

   if (QUERY_DB(mdb, mdb->cmd)) {
      mdb->num_rows = sql_num_rows(mdb);
      if (mdb->num_rows > 1) {
         Mmsg2(&mdb->errmsg, _("More than one Filename!: %d File=%s\n"), 
	    (int)(mdb->num_rows), fname);
         Emsg1(M_ERROR, 0, "%s", mdb->errmsg);
         Emsg1(M_ERROR, 0, "%s\n", mdb->cmd);
      }
      if (mdb->num_rows >= 1) {
	 if ((row = sql_fetch_row(mdb)) == NULL) {
            Mmsg2(&mdb->errmsg, _("error fetching row for file=%s: ERR=%s\n"), 
		fname, sql_strerror(mdb));
	    ar->FilenameId = 0;
	 } else {
	    ar->FilenameId = atoi(row[0]);
	 }
	 sql_free_result(mdb);
	 db_unlock(mdb);
	 return ar->FilenameId > 0;
      }
      sql_free_result(mdb);
   }

   Mmsg(&mdb->cmd, "INSERT INTO Filename (Name) \
VALUES (\"%s\")", fname);

   if (!INSERT_DB(mdb, mdb->cmd)) {
      Mmsg2(&mdb->errmsg, _("Create db Filename record %s failed. ERR=%s\n"), 
	    mdb->cmd, sql_strerror(mdb));
      ar->FilenameId = 0;
   } else {
      ar->FilenameId = sql_insert_id(mdb);
   }

   db_unlock(mdb);
   return ar->FilenameId > 0;
}

#endif /* HAVE_MYSQL || HAVE_SQLITE */
