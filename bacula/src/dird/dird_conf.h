/*
 * Director specific configuration and defines
 *
 *     Kern Sibbald, Feb MM
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

/* NOTE:  #includes at the end of this file */

/*
 * Resource codes -- they must be sequential for indexing   
 */
#define R_FIRST 		      1001

#define R_DIRECTOR		      1001
#define R_CLIENT		      1002
#define R_JOB			      1003
#define R_STORAGE		      1004
#define R_CATALOG		      1005
#define R_SCHEDULE		      1006
#define R_FILESET		      1007
#define R_GROUP 		      1008
#define R_POOL			      1009
#define R_MSGS			      1010
#define R_COUNTER		      1011

#define R_LAST			      R_COUNTER

/*
 * Some resource attributes
 */
#define R_NAME			      1020
#define R_ADDRESS		      1021
#define R_PASSWORD		      1022
#define R_TYPE			      1023
#define R_BACKUP		      1024


/* Used for certain KeyWord tables */
struct s_kw {	    
   char *name;
   int token;	
};

/* Job Level keyword structure */
struct s_jl {
   char *level_name;		      /* level keyword */
   int	level;			      /* level */
   int	job_type;		      /* JobType permitting this level */
};

/* Job Type keyword structure */
struct s_jt {
   char *type_name;
   int job_type;
};

/* Definition of the contents of each Resource */

/* 
 *   Director Resource	
 *
 */
struct s_res_dir {
   RES	 hdr;
   int	 DIRport;		      /* where we listen -- UA port server port */
   char *DIRaddr;		      /* bind address */
   char *password;		      /* Password for UA access */
   char *query_file;		      /* SQL query file */
   char *working_directory;	      /* WorkingDirectory */
   char *pid_directory; 	      /* PidDirectory */
   char *subsys_directory;	      /* SubsysDirectory */
   struct s_res_msgs *messages;       /* Daemon message handler */
   int	 MaxConcurrentJobs;
   btime_t FDConnectTimeout;	      /* timeout for connect in seconds */
   btime_t SDConnectTimeout;	      /* timeout in seconds */
};
typedef struct s_res_dir DIRRES;

/*
 *   Client Resource
 *
 */
struct s_res_client {
   RES	 hdr;

   int	 FDport;		      /* Where File daemon listens */
   int	 AutoPrune;		      /* Do automatic pruning? */
   btime_t FileRetention;	      /* file retention period in seconds */
   btime_t JobRetention;	      /* job retention period in seconds */
   char *address;
   char *password;
   struct s_res_cat    *catalog;       /* Catalog resource */
};
typedef struct s_res_client CLIENT;

/*
 *   Store Resource
 * 
 */
struct s_res_store {
   RES	 hdr;

   int	 SDport;		      /* port where Directors connect */
   int	 SDDport;		      /* data port for File daemon */
   char *address;
   char *password;
   char *media_type;
   char *dev_name;   
   int	autochanger;		      /* set if autochanger */
};
typedef struct s_res_store STORE;

/*
 *   Catalog Resource
 *
 */
struct s_res_cat {
   RES	 hdr;

   int	 DBport;		      /* Port -- not yet implemented */
   char *address;
   char *db_password;
   char *db_user;
   char *db_name;
};
typedef struct s_res_cat CAT;

/*
 *   Job Resource
 *
 */
struct s_res_job {
   RES	 hdr;

   int	 JobType;		      /* job type (backup, verify, restore */
   int	 level; 		      /* default backup/verify level */
   int	 RestoreJobId;		      /* What -- JobId to restore */
   char *RestoreWhere;		      /* Where on disk to restore -- directory */
   char *RestoreBootstrap;	      /* Bootstrap file */
   char *RunBeforeJob;		      /* Run program before Job */
   char *RunAfterJob;		      /* Run program after Job */
   char *WriteBootstrap;	      /* Where to write bootstrap Job updates */
   int	 replace;		      /* How (overwrite, ..) */
   btime_t MaxRunTime;		      /* max run time in seconds */
   btime_t MaxStartDelay;	      /* max start delay in seconds */
   int PruneJobs;		      /* Force pruning of Jobs */
   int PruneFiles;		      /* Force pruning of Files */
   int PruneVolumes;		      /* Force pruning of Volumes */
   int SpoolAttributes; 	      /* Set to spool attributes in SD */

   struct s_res_msgs   *messages;     /* How and where to send messages */
   struct s_res_sch    *schedule;     /* When -- Automatic schedule */
   struct s_res_client *client;       /* Who to backup */
   struct s_res_fs     *fileset;      /* What to backup -- Fileset */
   struct s_res_store  *storage;      /* Where is device -- Storage daemon */
   struct s_res_pool   *pool;	      /* Where is media -- Media Pool */
};
typedef struct s_res_job JOB;

/* 
 *   FileSet Resource
 *
 */
struct s_res_fs {
   RES	 hdr;

   char **include_array;
   int num_includes;
   int include_size;
   char **exclude_array;
   int num_excludes;
   int exclude_size;
   int have_MD5;		      /* set if MD5 initialized */
   struct MD5Context md5c;	      /* MD5 of include/exclude */
   char MD5[50];		      /* base 64 representation of MD5 */
};
typedef struct s_res_fs FILESET;
 

/* 
 *   Schedule Resource
 *
 */
struct s_res_sch {
   RES	 hdr;

   struct s_run *run;
};
typedef struct s_res_sch SCHED;

/*
 *   Group Resource (not used)
 *
 */
struct s_res_group {
   RES	 hdr;
};
typedef struct s_res_group GROUP;

/*
 *   Counter Resource
 */
struct s_res_counter {
   RES	 hdr;

   int32_t MinValue;		      /* Minimum value */
   int32_t MaxValue;		      /* Maximum value */
   int	   Global;		      /* global/local */
   char  *WrapCounter;		      /* Wrap counter name */
};
typedef struct s_res_counter COUNTER;

/*
 *   Pool Resource   
 *
 */
struct s_res_pool {
   RES	 hdr;

   struct s_res_counter counter;      /* Counter resources */
   char *pool_type;		      /* Pool type */
   char *label_format;		      /* Label format string */
   int	 use_catalog;		      /* maintain catalog for media */
   int	 catalog_files; 	      /* maintain file entries in catalog */
   int	 use_volume_once;	      /* write on volume only once */
   int	 accept_any_volume;	      /* accept any volume */
   int	 max_volumes;		      /* max number of volumes */
   btime_t VolRetention;	      /* volume retention period in seconds */
   int	 AutoPrune;		      /* default for pool auto prune */
   int	 Recycle;		      /* default for media recycle yes/no */
};
typedef struct s_res_pool POOL;


/* Define the Union of all the above
 * resource structure definitions.
 */
union u_res {
   struct s_res_dir	res_dir;
   struct s_res_client	res_client;
   struct s_res_store	res_store;
   struct s_res_cat	res_cat;
   struct s_res_job	res_job;
   struct s_res_fs	res_fs;
   struct s_res_sch	res_sch;
   struct s_res_group	res_group;
   struct s_res_pool	res_pool;
   struct s_res_msgs	res_msgs;
   struct s_res_counter res_counter;
   RES hdr;
};

typedef union u_res URES;


/* Run structure contained in Schedule Resource */
struct s_run {
   struct s_run *next;		      /* points to next run record */
   int level;			      /* level override */
   int job_type;  
   POOL *pool;			      /* Pool override */
   STORE *storage;		      /* Storage override */
   MSGS *msgs;			      /* Messages override */
   char *since;
   int level_no;
   int minute;			      /* minute to run job */
   time_t last_run;		      /* last time run */
   time_t next_run;		      /* next time to run */
   char hour[nbytes_for_bits(24)];    /* bit set for each hour */
   char mday[nbytes_for_bits(31)];    /* bit set for each day of month */
   char month[nbytes_for_bits(12)];   /* bit set for each month */
   char wday[nbytes_for_bits(7)];     /* bit set for each day of the week */
};
typedef struct s_run RUN;
