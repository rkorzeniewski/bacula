/*
 * SQL header file
 *
 *   by Kern E. Sibbald
 *
 *   Anyone who accesses the database will need to include
 *   this file.
 *
 * This file contains definitions common to sql.c and
 * the external world, and definitions destined only
 * for the external world. This is control with
 * the define __SQL_C, which is defined only in sql.c
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

#ifndef __SQL_H_
#define __SQL_H_ 1


typedef void (DB_LIST_HANDLER)(void *, char *);
typedef int (DB_RESULT_HANDLER)(void *, int, char **);

#define db_lock(mdb)   _db_lock(__FILE__, __LINE__, mdb)
#define db_unlock(mdb) _db_unlock(__FILE__, __LINE__, mdb)

#ifdef xxxxx_old_way_of_doing_it
#define db_lock(mdb)   P(mdb->mutex)
#define db_unlock(mdb) V(mdb->mutex)
#define db_lock(mdb) \
   do { int errstat; if ((errstat=rwl_writelock(&(mdb->lock)))) \
      e_msg(__FILE__, __LINE__, M_ABORT, 0, "rwl_writelock failure. ERR=%s\n",\
           strerror(errstat)); \
   } while(0)

#define db_unlock(x) \
   do { int errstat; if ((errstat=rwl_writeunlock(&(mdb->lock)))) \
         e_msg(__FILE__, __LINE__, M_ABORT, 0, "rwl_writeunlock failure. ERR=%s\n",\
           strerror(errstat)); \
   } while(0)
#endif

 
#ifdef __SQL_C

#ifdef HAVE_SQLITE

#define BDB_VERSION 1

#include <sqlite.h>

/* Define opaque structure for sqlite */
struct sqlite {
   char dummy;
};

#define IS_NUM(x)             ((x) == 1)
#define IS_NOT_NULL(x)        ((x) == 1)

typedef struct s_sql_field {
   char *name;                        /* name of column */
   uint32_t length;                   /* length */
   uint32_t max_length;               /* max length */
   uint32_t type;                     /* type */
   uint32_t flags;                    /* flags */
} SQL_FIELD;

/*
 * This is the "real" definition that should only be
 * used inside sql.c and associated database interface
 * subroutines.
 *                    S Q L I T E
 */
typedef struct s_db {
   BQUEUE bq;                         /* queue control */
/* pthread_mutex_t mutex; */
   rwlock_t lock;                     /* transaction lock */
   struct sqlite *db;
   char **result;
   int nrow;                          /* nrow returned from sqlite */
   int ncolumn;                       /* ncolum returned from sqlite */
   int num_rows;                      /* used by code */
   int row;                           /* seek row */
   int have_insert_id;                /* do not have insert id */
   int fields_defined;                /* set when fields defined */
   int field;                         /* seek field */
   SQL_FIELD **fields;                /* defined fields */
   int ref_count;
   char *db_name;
   char *db_user;
   char *db_password;
   int connected;
   char *sqlite_errmsg;               /* error message returned by sqlite */
   POOLMEM *errmsg;                   /* nicely edited error message */
   POOLMEM *cmd;                      /* SQL command string */
} B_DB;


/* 
 * "Generic" names for easier conversion   
 *
 *                    S Q L I T E
 */
#define sql_store_result(x)   x->result
#define sql_free_result(x)    my_sqlite_free_table(x)
#define sql_fetch_row(x)      my_sqlite_fetch_row(x)
#define sql_query(x, y)       my_sqlite_query(x, y)
#define sql_close(x)          sqlite_close(x->db)  
#define sql_strerror(x)       x->sqlite_errmsg?x->sqlite_errmsg:"unknown"
#define sql_num_rows(x)       x->nrow
#define sql_data_seek(x, i)   x->row = i
#define sql_affected_rows(x)  1
#define sql_insert_id(x)      sqlite_last_insert_rowid(x->db)
#define sql_field_seek(x, y)  my_sqlite_field_seek(x, y)
#define sql_fetch_field(x)    my_sqlite_fetch_field(x)
#define sql_num_fields(x)     (unsigned)((x)->ncolumn)
#define SQL_ROW               char**   



/* In cats/sqlite.c */
extern int     my_sqlite_query(B_DB *mdb, char *cmd);
extern SQL_ROW my_sqlite_fetch_row(B_DB *mdb);
extern void my_sqlite_free_table(B_DB *mdb);

#else

#ifdef HAVE_MYSQL

#define BDB_VERSION 1

#include <mysql.h>

/*
 * This is the "real" definition that should only be
 * used inside sql.c and associated database interface
 * subroutines.
 *
 *                     M Y S Q L
 */
typedef struct s_db {
   BQUEUE bq;                         /* queue control */
/* pthread_mutex_t mutex; */
   rwlock_t lock;                     /* transaction lock */
   MYSQL mysql;
   MYSQL *db;
   MYSQL_RES *result;
   my_ulonglong num_rows;
   int ref_count;
   char *db_name;
   char *db_user;
   char *db_password;
   int have_insert_id;                /* do have insert_id() */
   int connected;
   POOLMEM *errmsg;                   /* nicely edited error message */
   POOLMEM *cmd;                      /* SQL command string */
} B_DB;


/* "Generic" names for easier conversion */
#define sql_store_result(x)   mysql_store_result(x->db)
#define sql_free_result(x)    mysql_free_result(x->result)
#define sql_fetch_row(x)      mysql_fetch_row(x->result)
#define sql_query(x, y)       mysql_query(x->db, y)
#define sql_close(x)          mysql_close(x->db)  
#define sql_strerror(x)       mysql_error(x->db)
#define sql_num_rows(x)       mysql_num_rows(x->result)
#define sql_data_seek(x, i)   mysql_data_seek(x->result, i)
#define sql_affected_rows(x)  mysql_affected_rows(x->db)
#define sql_insert_id(x)      mysql_insert_id(x->db)
#define sql_field_seek(x, y)  mysql_field_seek(x->result, y)
#define sql_fetch_field(x)    mysql_fetch_field(x->result)
#define sql_num_fields(x)     mysql_num_fields(x->result)
#define SQL_ROW               MYSQL_ROW
#define SQL_FIELD             MYSQL_FIELD

#else  /* USE BACULA DB routines */

#define HAVE_BACULA_DB 1

/* Change this each time there is some incompatible
 * file format change!!!!
 */
#define BDB_VERSION 9                 /* file version number */

struct s_control {
   int bdb_version;                   /* Version number */
   uint32_t JobId;                    /* next Job Id */
   uint32_t PoolId;                   /* next Pool Id */
   uint32_t MediaId;                  /* next Media Id */
   uint32_t JobMediaId;               /* next JobMedia Id */
   uint32_t ClientId;                 /* next Client Id */
   uint32_t FileSetId;                /* nest FileSet Id */
   time_t time;                       /* time file written */
};


/* This is the REAL definition for using the
 *  Bacula internal DB
 */
typedef struct s_db {
   BQUEUE bq;                         /* queue control */
/* pthread_mutex_t mutex;  */         /* single thread lock */
   rwlock_t lock;                     /* transaction lock */
   int ref_count;                     /* number of times opened */
   struct s_control control;          /* control file structure */
   int cfd;                           /* control file device */
   FILE *jobfd;                       /* Jobs records file descriptor */
   FILE *poolfd;                      /* Pool records fd */
   FILE *mediafd;                     /* Media records fd */
   FILE *jobmediafd;                  /* JobMedia records fd */
   FILE *clientfd;                    /* Client records fd */
   FILE *filesetfd;                   /* FileSet records fd */
   char *db_name;                     /* name of database */
   POOLMEM *errmsg;                   /* nicely edited error message */
   POOLMEM *cmd;                      /* Command string */
} B_DB;

#endif /* HAVE_MYSQL */
#endif /* HAVE_SQLITE */

/* Use for better error location printing */
#define UPDATE_DB(db, cmd) UpdateDB(__FILE__, __LINE__, db, cmd)
#define INSERT_DB(db, cmd) InsertDB(__FILE__, __LINE__, db, cmd)
#define QUERY_DB(db, cmd) QueryDB(__FILE__, __LINE__, db, cmd)
#define DELETE_DB(db, cmd) DeleteDB(__FILE__, __LINE__, db, cmd)


#else    /* not __SQL_C */

/* This is a "dummy" definition for use outside of sql.c
 */
typedef struct s_db {     
   int dummy;                         /* for SunOS compiler */
} B_DB;  

#endif /*  __SQL_C */

/* ***FIXME*** FileId_t should be uint64_t */
typedef uint32_t FileId_t;
typedef uint32_t DBId_t;              /* general DB id type */
typedef uint32_t JobId_t;


/* Job information passed to create job record and update
 * job record at end of job. Note, although this record
 * contains all the fields found in the Job database record,
 * it also contains fields found in the JobMedia record.
 */
/* Job record */
typedef struct {
   JobId_t JobId;
   char Job[MAX_NAME_LENGTH];         /* Job unique name */
   char Name[MAX_NAME_LENGTH];        /* Job base name */
   int Type;                          /* actually char(1) */
   int Level;                         /* actually char(1) */
   int JobStatus;                     /* actually char(1) */
   uint32_t ClientId;                 /* Id of client */
   uint32_t PoolId;                   /* Id of pool */
   uint32_t FileSetId;                /* Id of FileSet */
   time_t SchedTime;                  /* Time job scheduled */
   time_t StartTime;                  /* Job start time */
   time_t EndTime;                    /* Job termination time */
   btime_t JobTDate;                  /* Backup time/date in seconds */
   uint32_t VolSessionId;
   uint32_t VolSessionTime;
   uint32_t JobFiles;
   uint32_t JobErrors;
   uint32_t JobMissingFiles;
   uint64_t JobBytes;

   /* Note, FirstIndex, LastIndex, Start/End File and Block
    * are only used in the JobMedia record.
    */
   uint32_t FirstIndex;               /* First index this Volume */
   uint32_t LastIndex;                /* Last index this Volume */
   uint32_t StartFile;
   uint32_t EndFile;
   uint32_t StartBlock;
   uint32_t EndBlock;

   char cSchedTime[MAX_NAME_LENGTH];
   char cStartTime[MAX_NAME_LENGTH];
   char cEndTime[MAX_NAME_LENGTH];
   /* Extra stuff not in DB */
   faddr_t rec_addr;
} JOB_DBR;

/* Job Media information used to create the media records
 * for each Volume used for the job.
 */
/* JobMedia record */
typedef struct {
   uint32_t JobMediaId;               /* record id */
   JobId_t  JobId;                    /* JobId */
   uint32_t MediaId;                  /* MediaId */
   uint32_t FirstIndex;               /* First index this Volume */
   uint32_t LastIndex;                /* Last index this Volume */
   uint32_t StartFile;                /* File for start of data */
   uint32_t EndFile;                  /* End file on Volume */
   uint32_t StartBlock;               /* start block on tape */
   uint32_t EndBlock;                 /* last block */
} JOBMEDIA_DBR;


/* Attributes record -- NOT same as in database because
 *  in general, this "record" creates multiple database
 *  records (e.g. pathname, filename, fileattributes).
 */
typedef struct {
   char *fname;                       /* full path & filename */
   char *link;                        /* link if any */
   char *attr;                        /* attributes statp */
   uint32_t FileIndex;
   uint32_t Stream;
   JobId_t  JobId;
   uint32_t ClientId;
   uint32_t PathId;
   uint32_t FilenameId;
   FileId_t FileId;
} ATTR_DBR;


/* File record -- same format as database */
typedef struct {
   FileId_t FileId;
   uint32_t FileIndex;
   JobId_t  JobId;
   uint32_t FilenameId;
   uint32_t PathId;
   char LStat[256];
/*   int Status; */
   char MD5[50];
} FILE_DBR;

/* Pool record -- same format as database */
typedef struct {
   uint32_t PoolId;
   char Name[MAX_NAME_LENGTH];        /* Pool name */
   uint32_t NumVols;                  /* total number of volumes */
   uint32_t MaxVols;                  /* max allowed volumes */
   int UseOnce;                       /* set to use once only */
   int UseCatalog;                    /* set to use catalog */
   int AcceptAnyVolume;               /* set to accept any volume sequence */
   int AutoPrune;                     /* set to prune automatically */
   int Recycle;                       /* default Vol recycle flag */
   btime_t VolRetention;              /* retention period in seconds */
   char PoolType[MAX_NAME_LENGTH];             
   char LabelFormat[MAX_NAME_LENGTH];
   /* Extra stuff not in DB */
   faddr_t rec_addr;
} POOL_DBR;

/* Media record -- same as the database */
typedef struct {
   uint32_t MediaId;                  /* Unique volume id */
   char VolumeName[MAX_NAME_LENGTH];  /* Volume name */
   char MediaType[MAX_NAME_LENGTH];   /* Media type */
   uint32_t PoolId;                   /* Pool id */
   time_t FirstWritten;               /* Time Volume first written */
   time_t LastWritten;                /* Time Volume last written */
   time_t LabelDate;                  /* Date/Time Volume labelled */
   uint32_t VolJobs;                  /* number of jobs on this medium */
   uint32_t VolFiles;                 /* Number of files */
   uint32_t VolBlocks;                /* Number of blocks */
   uint32_t VolMounts;                /* Number of times mounted */
   uint32_t VolErrors;                /* Number of read/write errors */
   uint32_t VolWrites;                /* Number of writes */
   uint32_t VolReads;                 /* Number of reads */
   uint64_t VolBytes;                 /* Number of bytes written */
   uint64_t VolMaxBytes;              /* max bytes to write */
   uint64_t VolCapacityBytes;         /* capacity estimate */
   btime_t  VolRetention;             /* Volume retention in seconds */
   int Recycle;                       /* recycle yes/no */
   char VolStatus[20];                /* Volume status */
   /* Extra stuff not in DB */
   faddr_t rec_addr;                  /* found record address */
} MEDIA_DBR;

/* Client record -- same as the database */
typedef struct {
   uint32_t ClientId;                 /* Unique Client id */
   int AutoPrune;
   btime_t FileRetention;
   btime_t JobRetention;
   char Name[MAX_NAME_LENGTH];        /* Client name */
   char Uname[256];                   /* Uname for client */
} CLIENT_DBR;

/* FileSet record -- same as the database */
typedef struct {
   uint32_t FileSetId;                /* Unique FileSet id */
   char FileSet[MAX_NAME_LENGTH];     /* FileSet name */
   char MD5[50];                      /* MD5 signature of include/exclude */
} FILESET_DBR;


#include "protos.h"
#include "jcr.h"
 
#endif /* __SQL_H_ */
