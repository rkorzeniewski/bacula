/*
 * Bacula JCR Structure definition for Daemons and the Library
 *  This definition consists of a "Global" definition common
 *  to all daemons and used by the library routines, and a
 *  daemon specific part that is enabled with #defines.
 *
 * Kern Sibbald, Nov MM
 *
 */

/*
   Copyright (C) 2000, 2001 Kern Sibbald and John Walker

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
 
#ifndef __JCR_H_
#define __JCR_H_ 1

/* Backup/Verify level code */
#define L_FULL                   'F'
#define L_INCREMENTAL            'I'  /* since last backup */
#define L_DIFFERENTIAL           'D'  /* since last full backup */
#define L_LEVEL                  'L'
#define L_SINCE                  'S'
#define L_VERIFY_CATALOG         'C'  /* verify from catalog */
#define L_VERIFY_INIT            'V'  /* verify save (init DB) */
#define L_VERIFY_VOLUME          'O'  /* verify we can read volume */
#define L_VERIFY_DATA            'A'  /* verify data on volume */


/* Job Types */
#define JT_BACKUP                'B'
#define JT_VERIFY                'V'
#define JT_RESTORE               'R'
#define JT_CONSOLE               'C'  /* console program */

/* Job Status */
#define JS_Created               'C'
#define JS_Running               'R'
#define JS_Blocked               'B'
#define JS_Terminated            'T'  /* terminated normally */
#define JS_ErrorTerminated       'E'
#define JS_Errored               'E'
#define JS_Differences           'D'  /* Verify differences */
#define JS_Cancelled             'A'  /* cancelled by user */
#define JS_WaitFD                'F'  /* waiting on File daemon */
#define JS_WaitSD                'S'  /* waiting on the Storage daemon */
#define JS_WaitMedia             'm'  /* waiting for new media */
#define JS_WaitMount             'M'  /* waiting for Mount */

#define job_cancelled(jcr) \
  (jcr->JobStatus == JS_Cancelled || jcr->JobStatus == JS_ErrorTerminated)

typedef void (JCR_free_HANDLER)(struct s_jcr *jcr);

/* Job Control Record (JCR) */
struct s_jcr {
   /* Global part of JCR common to all daemons */
   struct s_jcr *next;
   struct s_jcr *prev;
   pthread_t my_thread_id;            /* id of thread controlling jcr */
   pthread_mutex_t mutex;             /* jcr mutex */
   BSOCK *dir_bsock;                  /* Director bsock or NULL if we are him */
   BSOCK *store_bsock;                /* Storage connection socket */
   BSOCK *file_bsock;                 /* File daemon connection socket */
   JCR_free_HANDLER *daemon_free_jcr; /* Local free routine */
   int use_count;                     /* use count */
   char *errmsg;                      /* edited error message */
   char Job[MAX_NAME_LENGTH];         /* Job name */
   uint32_t JobId;                    /* Director's JobId */
   uint32_t VolSessionId;
   uint32_t VolSessionTime;
   uint32_t JobFiles;                 /* Number of files written, this job */
   uint32_t JobErrors;
   uint64_t JobBytes;                 /* Number of bytes processed this job */
   int JobStatus;                     /* ready, running, blocked, terminated */ 
   int JobTermCode;                   /* termination code */
   int JobType;                       /* backup, restore, verify ... */
   int level;
   int authenticated;                 /* set when client authenticated */
   time_t sched_time;                 /* job schedule time, i.e. when it should start */
   time_t start_time;                 /* when job actually started */
   time_t run_time;                   /* used for computing speed */
   time_t end_time;                   /* job end time */
   char *VolumeName;                  /* Volume name desired -- pool_memory */
   char *client_name;                 /* client name */
   char *sd_auth_key;                 /* SD auth key */
   DEST *dest_chain;                  /* Job message destination chain */
   char send_msg[nbytes_for_bits(M_MAX+1)]; /* message bit mask */

   /* Daemon specific part of JCR */
   /* This should be empty in the library */

#ifdef DIRECTOR_DAEMON
   /* Director Daemon specific part of JCR */
   pthread_t SD_msg_chan;             /* Message channel thread id */
   pthread_cond_t term_wait;          /* Wait for job termination */
   int msg_thread_done;               /* Set when Storage message thread terms */
   BSOCK *ua;                         /* User agent */
   JOB *job;                          /* Job resource */
   STORE *store;                      /* Storage resource */
   CLIENT *client;                    /* Client resource */
   POOL *pool;                        /* Pool resource */
   FILESET *fileset;                  /* FileSet resource */
   CAT *catalog;                      /* Catalog resource */
   int SDJobStatus;                   /* Storage Job Status */
   int mode;                          /* manual/auto run */
   B_DB *db;                          /* database pointer */
   int MediaId;                       /* DB record IDs associated with this job */
   int ClientId;                      /* Client associated with file */
   uint32_t PoolId;                   /* Pool record id */
   FileId_t FileId;                   /* Last file id inserted */
   uint32_t FileIndex;                /* Last FileIndex processed */
   char *fname;                       /* name to put into catalog */
   int fn_printed;                    /* printed filename */
   char *stime;                       /* start time for incremental/differential */
   JOB_DBR jr;                        /* Job record in Database */
   int RestoreJobId;                  /* Id specified by UA */
   char *RestoreWhere;                /* Where to restore the files */
#endif /* DIRECTOR_DAEMON */

#ifdef FILE_DAEMON
   /* File Daemon specific part of JCR */
   uint32_t num_files_examined;       /* files examined this job */
   char *last_fname;                  /* last file saved */
   /*********FIXME********* add missing files and files to be retried */
   int incremental;                   /* set if incremental for SINCE */
   time_t mtime;                      /* begin time for SINCE */
   int mode;                          /* manual/auto run */
   int status;                        /* job status */
   long Ticket;                       /* Ticket */
   int save_level;                    /* save level */
   char *big_buf;                     /* I/O buffer */
   char *compress_buf;                /* Compression buffer */
   char *where;                       /* Root where to restore */
   int buf_size;                      /* length of buffer */
   FF_PKT *ff;                        /* Find Files packet */
   char stored_addr[MAX_NAME_LENGTH]; /* storage daemon address */
   uint32_t StartFile;
   uint32_t EndFile;
   uint32_t StartBlock;
   uint32_t EndBlock;
#endif /* FILE_DAEMON */

#ifdef STORAGE_DAEMON
   /* Storage Daemon specific part of JCR */
   pthread_cond_t job_start_wait;     /* Wait for FD to start Job */
   int type;
   DEVRES *device;                    /* device to use */
   VOLUME_CAT_INFO VolCatInfo;        /* Catalog info for desired volume */
   char *pool_name;                   /* pool to use */
   char *pool_type;                   /* pool type to use */
   char *job_name;                    /* job name */
   char *media_type;                  /* media type */
   char *dev_name;                    /* device name */
   long NumVolumes;                   /* number of volumes used */
   long CurVolume;                    /* current volume number */
   int mode;                          /* manual/auto run */
   int label_status;                  /* device volume label status */
   int session_opened;
   DEV_RECORD rec;                    /* Read/Write record */
   long Ticket;                       /* ticket for this job */
   uint32_t VolFirstFile;             /* First file index this Volume */
   uint32_t start_block;              /* Start write block */
   uint32_t start_file;               /* Start write file */
   uint32_t end_block;                /* Ending block written */
   uint32_t end_file;                 /* End file written */

   /* Parmaters for Open Read Session */
   uint32_t read_VolSessionId;
   uint32_t read_VolSessionTime;
   uint32_t read_StartFile;
   uint32_t read_EndFile;
   uint32_t read_StartBlock;
   uint32_t read_EndBlock;

#endif /* STORAGE_DAEMON */

}; 

/* 
 * Structure for all daemons that keeps some summary
 *  info on the last job run.
 */
struct s_last_job {
   int NumJobs;
   int JobType;
   int JobStatus;
   uint32_t JobId;
   uint32_t VolSessionId;
   uint32_t VolSessionTime;
   uint32_t JobFiles;
   uint64_t JobBytes;
   time_t start_time;
   time_t end_time;
   char Job[MAX_NAME_LENGTH];
};

extern struct s_last_job last_job;

#undef JCR
typedef struct s_jcr JCR;


/* The following routines are found in lib/jcr.c */
extern JCR *new_jcr(int size, JCR_free_HANDLER *daemon_free_jcr);
extern void free_jcr(JCR *jcr);
extern void free_locked_jcr(JCR *jcr);
extern JCR *get_jcr_by_id(uint32_t JobId);
extern JCR *get_jcr_by_partial_name(char *Job);
extern JCR *get_jcr_by_full_name(char *Job);
extern JCR *get_next_jcr(JCR *jcr);
extern void lock_jcr_chain();
extern void unlock_jcr_chain();

#endif /* __JCR_H_ */
