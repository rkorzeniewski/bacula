/*
 * Director specific configuration and defines
 *
 *     Kern Sibbald, Feb MM
 *
 *    Version $Id$
 */
/*
   Copyright (C) 2000-2003 2002 Kern Sibbald and John Walker

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
#define R_FIRST               1001

#define R_DIRECTOR            1001
#define R_CLIENT              1002
#define R_JOB                 1003
#define R_STORAGE             1004
#define R_CATALOG             1005
#define R_SCHEDULE            1006
#define R_FILESET             1007
#define R_GROUP               1008
#define R_POOL                1009
#define R_MSGS                1010
#define R_COUNTER             1011
#define R_CONSOLE             1012

#define R_LAST                R_CONSOLE

/*
 * Some resource attributes
 */
#define R_NAME                1020
#define R_ADDRESS             1021
#define R_PASSWORD            1022
#define R_TYPE                1023
#define R_BACKUP              1024


/* Used for certain KeyWord tables */
struct s_kw {       
   char *name;
   int token;   
};

/* Job Level keyword structure */
struct s_jl {
   char *level_name;                  /* level keyword */
   int  level;                        /* level */
   int  job_type;                     /* JobType permitting this level */
};

/* Job Type keyword structure */
struct s_jt {
   char *type_name;
   int job_type;
};

/* Definition of the contents of each Resource */
/* Needed for forward references */
struct SCHED;
struct CLIENT;
struct FILESET;
struct POOL;
struct RUN;

/* 
 *   Director Resource  
 *
 */
struct DIRRES {
   RES   hdr;
   int   DIRport;                     /* where we listen -- UA port server port */
   char *DIRaddr;                     /* bind address */
   char *password;                    /* Password for UA access */
   int enable_ssl;                    /* Use SSL for UA */
   char *query_file;                  /* SQL query file */
   char *working_directory;           /* WorkingDirectory */
   char *pid_directory;               /* PidDirectory */
   char *subsys_directory;            /* SubsysDirectory */
   int require_ssl;                   /* Require SSL for all connections */
   MSGS *messages;                    /* Daemon message handler */
   uint32_t MaxConcurrentJobs;        /* Max concurrent jobs for whole director */
   utime_t FDConnectTimeout;          /* timeout for connect in seconds */
   utime_t SDConnectTimeout;          /* timeout in seconds */
};

/* 
 *    Console Resource
 */
struct CONRES {
   RES   hdr;
   char *password;                    /* UA server password */
   int enable_ssl;                    /* Use SSL */
};


/*
 *   Catalog Resource
 *
 */
struct CAT {
   RES   hdr;

   int   db_port;                     /* Port -- not yet implemented */
   char *db_address;                  /* host name for remote access */
   char *db_socket;                   /* Socket for local access */
   char *db_password;
   char *db_user;
   char *db_name;
};


/*
 *   Client Resource
 *
 */
struct CLIENT {
   RES   hdr;

   int   FDport;                      /* Where File daemon listens */
   int   AutoPrune;                   /* Do automatic pruning? */
   utime_t FileRetention;             /* file retention period in seconds */
   utime_t JobRetention;              /* job retention period in seconds */
   char *address;
   char *password;
   CAT *catalog;                      /* Catalog resource */
   uint32_t MaxConcurrentJobs;        /* Maximume concurrent jobs */
   semlock_t sem;                     /* client semaphore */
   int enable_ssl;                    /* Use SSL */
};

/*
 *   Store Resource
 * 
 */
struct STORE {
   RES   hdr;

   int   SDport;                      /* port where Directors connect */
   int   SDDport;                     /* data port for File daemon */
   char *address;
   char *password;
   char *media_type;
   char *dev_name;   
   int  autochanger;                  /* set if autochanger */
   uint32_t MaxConcurrentJobs;        /* Maximume concurrent jobs */
   semlock_t sem;                     /* storage semaphore */
   int enable_ssl;                    /* Use SSL */
};


/*
 *   Job Resource
 *
 */
struct JOB {
   RES   hdr;

   int   JobType;                     /* job type (backup, verify, restore */
   int   level;                       /* default backup/verify level */
   int   RestoreJobId;                /* What -- JobId to restore */
   char *RestoreWhere;                /* Where on disk to restore -- directory */
   char *RestoreBootstrap;            /* Bootstrap file */
   char *RunBeforeJob;                /* Run program before Job */
   char *RunAfterJob;                 /* Run program after Job */
   char *WriteBootstrap;              /* Where to write bootstrap Job updates */
   int   replace;                     /* How (overwrite, ..) */
   utime_t MaxRunTime;                /* max run time in seconds */
   utime_t MaxStartDelay;             /* max start delay in seconds */
   int PrefixLinks;                   /* prefix soft links with Where path */
   int PruneJobs;                     /* Force pruning of Jobs */
   int PruneFiles;                    /* Force pruning of Files */
   int PruneVolumes;                  /* Force pruning of Volumes */
   int SpoolAttributes;               /* Set to spool attributes in SD */
   uint32_t MaxConcurrentJobs;        /* Maximume concurrent jobs */
  
   MSGS                *messages;     /* How and where to send messages */
   SCHED               *schedule;     /* When -- Automatic schedule */
   CLIENT              *client;       /* Who to backup */
   FILESET             *fileset;      /* What to backup -- Fileset */
   STORE               *storage;      /* Where is device -- Storage daemon */
   POOL                *pool;         /* Where is media -- Media Pool */

   semlock_t sem;                     /* Job semaphore */
};

#define MAX_FOPTS 30

struct s_fopts_item {
   char opts[MAX_FOPTS];              /* options string */
   char *match;                       /* match string */
   char **base_list;                  /* list of base job names */
   int  num_base;                     /* number of bases in list */
};
typedef struct s_fopts_item FOPTS;


/* This is either an include item or an exclude item */
struct s_incexc_item {
   FOPTS *current_opts;               /* points to current options structure */
   FOPTS **opts_list;                 /* options list */
   int num_opts;                      /* number of options items */
   char **name_list;                  /* filename list */
   int max_names;                     /* malloc'ed size of name list */
   int num_names;                     /* number of names in the list */
};
typedef struct s_incexc_item INCEXE;

/* 
 *   FileSet Resource
 *
 */
struct FILESET {
   RES   hdr;

   int finclude;                      /* Set if finclude/fexclude used */
   INCEXE **include_items;            /* array of incexe structures */
   int num_includes;                  /* number in array */
   INCEXE **exclude_items;
   int num_excludes;
   int have_MD5;                      /* set if MD5 initialized */
   struct MD5Context md5c;            /* MD5 of include/exclude */
   char MD5[30];                      /* base 64 representation of MD5 */
};

 
/* 
 *   Schedule Resource
 *
 */
struct SCHED {
   RES   hdr;

   RUN *run;
};

/*
 *   Group Resource (not used)
 *
 */
struct GROUP {
   RES   hdr;
};

/*
 *   Counter Resource
 */
struct COUNTER {
   RES   hdr;

   int32_t MinValue;                  /* Minimum value */
   int32_t MaxValue;                  /* Maximum value */
   int     Global;                    /* global/local */
   char  *WrapCounter;                /* Wrap counter name */
};

/*
 *   Pool Resource   
 *
 */
struct POOL {
   RES   hdr;

   COUNTER counter;                   /* Counter resources */
   char *pool_type;                   /* Pool type */
   char *label_format;                /* Label format string */
   char *cleaning_prefix;             /* Cleaning label prefix */
   int   use_catalog;                 /* maintain catalog for media */
   int   catalog_files;               /* maintain file entries in catalog */
   int   use_volume_once;             /* write on volume only once */
   int   accept_any_volume;           /* accept any volume */
   int   recycle_oldest_volume;       /* recycle oldest volume */
   uint32_t max_volumes;              /* max number of volumes */
   utime_t VolRetention;              /* volume retention period in seconds */
   utime_t VolUseDuration;            /* duration volume can be used */
   uint32_t MaxVolJobs;               /* Maximum jobs on the Volume */
   uint32_t MaxVolFiles;              /* Maximum files on the Volume */
   uint64_t MaxVolBytes;              /* Maximum bytes on the Volume */
   int   AutoPrune;                   /* default for pool auto prune */
   int   Recycle;                     /* default for media recycle yes/no */
};


/* Define the Union of all the above
 * resource structure definitions.
 */
union URES {
   DIRRES     res_dir;
   CONRES     res_con;
   CLIENT     res_client;
   STORE      res_store;
   CAT        res_cat;
   JOB        res_job;
   FILESET    res_fs;
   SCHED      res_sch;
   GROUP      res_group;
   POOL       res_pool;
   MSGS       res_msgs;
   COUNTER    res_counter;
   RES        hdr;
};



/* Run structure contained in Schedule Resource */
struct RUN {
   RUN *next;                         /* points to next run record */
   int level;                         /* level override */
   int job_type;  
   POOL *pool;                        /* Pool override */
   STORE *storage;                    /* Storage override */
   MSGS *msgs;                        /* Messages override */
   char *since;
   int level_no;
   int minute;                        /* minute to run job */
   time_t last_run;                   /* last time run */
   time_t next_run;                   /* next time to run */
   char hour[nbytes_for_bits(24)];    /* bit set for each hour */
   char mday[nbytes_for_bits(31)];    /* bit set for each day of month */
   char month[nbytes_for_bits(12)];   /* bit set for each month */
   char wday[nbytes_for_bits(7)];     /* bit set for each day of the week */
   char wpos[nbytes_for_bits(5)];     /* week position */
};
