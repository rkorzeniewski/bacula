/*
 *
 *  This file contains all the SQL commands issued by the Director
 *
 *     Kern Sibbald, July MMII
 *
 *   Version $Id$
 */

/*
   Copyright (C) 2002-2003  Kern Sibbald and John Walker

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

/* ====== ua_prune.c */

char *cnt_File     = "SELECT count(*) FROM File WHERE JobId=%u";
char *del_File     = "DELETE FROM File WHERE JobId=%u";
char *upd_Purged   = "UPDATE Job Set PurgedFiles=1 WHERE JobId=%u";
char *cnt_DelCand  = "SELECT count(*) FROM DelCandidates";
char *del_Job      = "DELETE FROM Job WHERE JobId=%u";
char *del_JobMedia = "DELETE FROM JobMedia WHERE JobId=%u"; 
char *cnt_JobMedia = "SELECT count(*) FROM JobMedia WHERE MediaId=%u";
char *sel_JobMedia = "SELECT JobId FROM JobMedia WHERE MediaId=%u";

/* Select JobIds for File deletion. */
char *select_job =
   "SELECT JobId from Job "    
   "WHERE JobTDate < %s "
   "AND ClientId=%u "
   "AND PurgedFiles=0";

/* Delete temp tables and indexes  */
char *drop_deltabs[] = {
   "DROP TABLE DelCandidates",
   "DROP INDEX DelInx1",
   NULL};

/* List of SQL commands to create temp table and indicies  */
char *create_deltabs[] = {
   "CREATE TABLE DelCandidates ("
      "JobId INTEGER UNSIGNED NOT NULL, "
      "PurgedFiles TINYINT, "
      "FileSetId INTEGER UNSIGNED)",
   "CREATE INDEX DelInx1 ON DelCandidates (JobId)",
   NULL};

/* Fill candidates table with all Files subject to being deleted.
 *  This is used for pruning Jobs (first the files, then the Jobs).
 */
char *insert_delcand = 
   "INSERT INTO DelCandidates "
   "SELECT JobId, PurgedFiles, FileSetId FROM Job "
   "WHERE Type='%c' "
   "AND JobTDate<%s " 
   "AND ClientId=%u";

/* Select files from the DelCandidates table that have a
 * more recent backup -- i.e. are not the only backup.
 * This is the list of files to delete for a Backup Job.
 */
char *select_backup_del =
   "SELECT DelCandidates.JobId "
   "FROM Job,DelCandidates "
   "WHERE Job.JobTDate>%s "
   "AND Job.ClientId=%u "
   "AND Job.Type='B' "
   "AND Job.Level='F' "
   "AND Job.JobStatus='T' "
   "AND Job.FileSetId=DelCandidates.FileSetId";

/* Select files from the DelCandidates table that have a
 * more recent InitCatalog -- i.e. are not the only InitCatalog
 * This is the list of files to delete for a Verify Job.
 */
char *select_verify_del =
   "SELECT DelCandidates.JobId "
   "FROM Job,DelCandidates "
   "WHERE Job.JobTDate>%s "
   "AND Job.ClientId=%u "
   "AND Job.Type='V' "
   "AND Job.Level='V' "
   "AND Job.JobStatus='T' "
   "AND Job.FileSetId=DelCandidates.FileSetId";


/* Select files from the DelCandidates table.
 * This is the list of files to delete for a Restore Job.
 */
char *select_restore_del =
   "SELECT DelCandidates.JobId "
   "FROM Job,DelCandidates "
   "WHERE Job.JobTDate>%s "
   "AND Job.ClientId=%u "   
   "AND Job.Type='R'";



/* ======= ua_restore.c */

/* List last 20 Jobs */
char *uar_list_jobs = 
   "SELECT JobId,Client.Name as Client,StartTime,Type as "
   "JobType,JobFiles,JobBytes "
   "FROM Client,Job WHERE Client.ClientId=Job.ClientId AND JobStatus='T' "
   "AND Type='B' LIMIT 20";

#ifdef HAVE_MYSQL
/*  MYSQL IS NOT STANDARD SQL !!!!! */
/* List Jobs where a particular file is saved */
char *uar_file = 
   "SELECT Job.JobId as JobId, Client.Name as Client, "
   "CONCAT(Path.Path,Filename.Name) as Name, "
   "StartTime,Type as JobType,JobFiles,JobBytes "
   "FROM Client,Job,File,Filename,Path WHERE Client.ClientId=Job.ClientId "
   "AND JobStatus='T' AND Job.JobId=File.JobId "
   "AND Path.PathId=File.PathId AND Filename.FilenameId=File.FilenameId "
   "AND Filename.Name='%s' LIMIT 20";
#else
/* List Jobs where a particular file is saved */
char *uar_file = 
   "SELECT Job.JobId as JobId, Client.Name as Client, "
   "Path.Path||Filename.Name as Name, "
   "StartTime,Type as JobType,JobFiles,JobBytes "
   "FROM Client,Job,File,Filename,Path WHERE Client.ClientId=Job.ClientId "
   "AND JobStatus='T' AND Job.JobId=File.JobId "
   "AND Path.PathId=File.PathId AND Filename.FilenameId=File.FilenameId "
   "AND Filename.Name='%s' LIMIT 20";
#endif


char *uar_sel_files = 
   "SELECT Path.Path,Filename.Name,FileIndex,JobId "
   "FROM File,Filename,Path "
   "WHERE File.JobId=%d AND Filename.FilenameId=File.FilenameId "
   "AND Path.PathId=File.PathId";

char *uar_del_temp  = "DROP TABLE temp";
char *uar_del_temp1 = "DROP TABLE temp1";

char *uar_create_temp = 
   "CREATE TABLE temp (JobId INTEGER UNSIGNED NOT NULL,"
   "JobTDate BIGINT UNSIGNED,"
   "ClientId INTEGER UNSIGNED,"
   "Level CHAR,"
   "JobFiles INTEGER UNSIGNED,"
   "StartTime TEXT,"
   "VolumeName TEXT,"
   "StartFile INTEGER UNSIGNED,"
   "VolSessionId INTEGER UNSIGNED,"
   "VolSessionTime INTEGER UNSIGNED)";

char *uar_create_temp1 = 
   "CREATE TABLE temp1 (JobId INTEGER UNSIGNED NOT NULL,"
   "JobTDate BIGINT UNSIGNED)";

char *uar_last_full =
   "INSERT INTO temp1 SELECT Job.JobId,JobTdate "
   "FROM Client,Job,JobMedia,Media WHERE Client.ClientId=%u "
   "AND Job.ClientId=%u "
   "AND Level='F' AND JobStatus='T' "
   "AND JobMedia.JobId=Job.JobId "
   "AND JobMedia.MediaId=Media.MediaId "
   "AND Job.FileSetId=%u "
   "ORDER BY Job.JobTDate DESC LIMIT 1";

char *uar_full = 
   "INSERT INTO temp SELECT Job.JobId,Job.JobTDate,"          
   " Job.ClientId,Job.Level,Job.JobFiles,"
   " StartTime,VolumeName,JobMedia.StartFile,VolSessionId,VolSessionTime "
   "FROM temp1,Job,JobMedia,Media WHERE temp1.JobId=Job.JobId "
   "AND Level='F' AND JobStatus='T' "
   "AND JobMedia.JobId=Job.JobId "
   "AND JobMedia.MediaId=Media.MediaId";

char *uar_inc =
   "INSERT INTO temp SELECT Job.JobId,Job.JobTDate,Job.ClientId,"
   "Job.Level,Job.JobFiles,Job.StartTime,Media.VolumeName,JobMedia.StartFile,"
   "Job.VolSessionId,Job.VolSessionTime "
   "FROM Job,JobMedia,Media "
   "WHERE Job.JobTDate>%s AND Job.ClientId=%u "
   "AND JobMedia.JobId=Job.JobId "
   "AND JobMedia.MediaId=Media.MediaId "
   "AND Job.Level IN ('I', 'D') AND JobStatus='T' "
   "AND Job.FileSetId=%u "
   "GROUP BY Job.JobId";

char *uar_list_temp = 
   "SELECT JobId,Level,JobFiles,StartTime,VolumeName,StartFile,"
   "VolSessionId,VolSessionTime FROM temp";

char *uar_sel_jobid_temp = "SELECT JobId FROM temp";

char *uar_sel_all_temp1 = "SELECT * FROM temp1";

/* Select filesets for this Client */
char *uar_sel_fileset = 
   "SELECT FileSet.FileSetId,FileSet.FileSet,FileSet.MD5 FROM Job,"
   "Client,FileSet WHERE Job.FileSetId=FileSet.FileSetId "
   "AND Job.ClientId=%u AND Client.ClientId=%u "
   "GROUP BY FileSet.FileSetId";

/* Find MediaType used by this Job */
char *uar_mediatype =
   "SELECT MediaType FROM JobMedia,Media WHERE JobMedia.JobId=%u "
   "AND JobMedia.MediaId=Media.MediaId";
