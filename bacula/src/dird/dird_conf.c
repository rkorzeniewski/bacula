/*
 *   Main configuration file parser for Bacula Directors,
 *    some parts may be split into separate files such as
 *    the schedule configuration (sch_config.c).
 *
 *   Note, the configuration file parser consists of three parts
 *
 *   1. The generic lexical scanner in lib/lex.c and lib/lex.h
 *
 *   2. The generic config  scanner in lib/parse_config.c and 
 *	lib/parse_config.h.
 *	These files contain the parser code, some utility
 *	routines, and the common store routines (name, int,
 *	string).
 *
 *   3. The daemon specific file, which contains the Resource
 *	definitions as well as any specific store routines
 *	for the resource records.
 *
 *     Kern Sibbald, January MM
 *
 *     Version $Id$
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

#include "bacula.h"
#include "dird.h"

/* Define the first and last resource ID record
 * types. Note, these should be unique for each
 * daemon though not a requirement.
 */
int r_first = R_FIRST;
int r_last  = R_LAST;
pthread_mutex_t res_mutex =  PTHREAD_MUTEX_INITIALIZER;

/* Imported subroutines */
extern void store_run(LEX *lc, struct res_items *item, int index, int pass);


/* Forward referenced subroutines */

static void store_inc(LEX *lc, struct res_items *item, int index, int pass);
static void store_backup(LEX *lc, struct res_items *item, int index, int pass);
static void store_restore(LEX *lc, struct res_items *item, int index, int pass);
static void store_jobtype(LEX *lc, struct res_items *item, int index, int pass);
static void store_level(LEX *lc, struct res_items *item, int index, int pass);
static void store_replace(LEX *lc, struct res_items *item, int index, int pass);


/* We build the current resource here as we are
 * scanning the resource configuration definition,
 * then move it to allocated memory when the resource
 * scan is complete.
 */
URES res_all;
int  res_all_size = sizeof(res_all);


/* Definition of records permitted within each
 * resource with the routine to process the record 
 * information.  NOTE! quoted names must be in lower case.
 */ 
/* 
 *    Director Resource
 *
 *   name	   handler     value		     code flags    default_value
 */
static struct res_items dir_items[] = {
   {"name",        store_name,     ITEM(res_dir.hdr.name), 0, ITEM_REQUIRED, 0},
   {"description", store_str,      ITEM(res_dir.hdr.desc), 0, 0, 0},
   {"messages",    store_res,      ITEM(res_dir.messages), R_MSGS, 0, 0},
   {"dirport",     store_pint,     ITEM(res_dir.DIRport),  0, ITEM_DEFAULT, 9101},
   {"diraddress",  store_str,      ITEM(res_dir.DIRaddr),  0, 0, 0},
   {"queryfile",   store_dir,      ITEM(res_dir.query_file), 0, ITEM_REQUIRED, 0},
   {"workingdirectory", store_dir, ITEM(res_dir.working_directory), 0, ITEM_REQUIRED, 0},
   {"piddirectory", store_dir,     ITEM(res_dir.pid_directory), 0, ITEM_REQUIRED, 0},
   {"subsysdirectory", store_dir,  ITEM(res_dir.subsys_directory), 0, ITEM_REQUIRED, 0},
   {"maximumconcurrentjobs", store_pint, ITEM(res_dir.MaxConcurrentJobs), 0, ITEM_DEFAULT, 1},
   {"password",    store_password, ITEM(res_dir.password), 0, ITEM_REQUIRED, 0},
   {"fdconnecttimeout", store_time,ITEM(res_dir.FDConnectTimeout), 0, ITEM_DEFAULT, 60 * 30},
   {"sdconnecttimeout", store_time,ITEM(res_dir.SDConnectTimeout), 0, ITEM_DEFAULT, 60 * 30},
   {NULL, NULL, NULL, 0, 0, 0}
};

/* 
 *    Client or File daemon resource
 *
 *   name	   handler     value		     code flags    default_value
 */

static struct res_items cli_items[] = {
   {"name",     store_name,       ITEM(res_client.hdr.name), 0, ITEM_REQUIRED, 0},
   {"description", store_str,     ITEM(res_client.hdr.desc), 0, 0, 0},
   {"address",  store_str,        ITEM(res_client.address),  0, ITEM_REQUIRED, 0},
   {"fdport",   store_pint,       ITEM(res_client.FDport),   0, ITEM_DEFAULT, 9102},
   {"password", store_password,   ITEM(res_client.password), 0, ITEM_REQUIRED, 0},
   {"catalog",  store_res,        ITEM(res_client.catalog),  R_CATALOG, 0, 0},
   {"fileretention", store_time,  ITEM(res_client.FileRetention), 0, ITEM_DEFAULT, 60*60*24*60},
   {"jobretention",  store_time,  ITEM(res_client.JobRetention),  0, ITEM_DEFAULT, 60*60*24*180},
   {"autoprune", store_yesno,     ITEM(res_client.AutoPrune), 1, ITEM_DEFAULT, 1},
   {NULL, NULL, NULL, 0, 0, 0} 
};

/* Storage daemon resource
 *
 *   name	   handler     value		     code flags    default_value
 */
static struct res_items store_items[] = {
   {"name",      store_name,     ITEM(res_store.hdr.name),   0, ITEM_REQUIRED, 0},
   {"description", store_str,    ITEM(res_store.hdr.desc),   0, 0, 0},
   {"sdport",    store_pint,     ITEM(res_store.SDport),     0, ITEM_DEFAULT, 9103},
   {"sddport",   store_pint,     ITEM(res_store.SDDport),    0, 0, 0}, /* deprecated */
   {"address",   store_str,      ITEM(res_store.address),    0, ITEM_REQUIRED, 0},
   {"password",  store_password, ITEM(res_store.password),   0, ITEM_REQUIRED, 0},
   {"device",    store_strname,  ITEM(res_store.dev_name),   0, ITEM_REQUIRED, 0},
   {"mediatype", store_strname,  ITEM(res_store.media_type), 0, ITEM_REQUIRED, 0},
   {"autochanger", store_yesno,  ITEM(res_store.autochanger), 1, ITEM_DEFAULT, 0},
   {NULL, NULL, NULL, 0, 0, 0} 
};

/* 
 *    Catalog Resource Directives
 *
 *   name	   handler     value		     code flags    default_value
 */
static struct res_items cat_items[] = {
   {"name",     store_name,     ITEM(res_cat.hdr.name),    0, ITEM_REQUIRED, 0},
   {"description", store_str,   ITEM(res_cat.hdr.desc),    0, 0, 0},
   {"address",  store_str,      ITEM(res_cat.address),     0, 0, 0},
   {"dbport",   store_pint,     ITEM(res_cat.DBport),      0, 0, 0},
   /* keep this password as store_str for the moment */
   {"password", store_str,      ITEM(res_cat.db_password), 0, 0, 0},
   {"user",     store_str,      ITEM(res_cat.db_user),     0, 0, 0},
   {"dbname",   store_str,      ITEM(res_cat.db_name),     0, ITEM_REQUIRED, 0},
   {NULL, NULL, NULL, 0, 0, 0} 
};

/* 
 *    Job Resource Directives
 *
 *   name	   handler     value		     code flags    default_value
 */
static struct res_items job_items[] = {
   {"name",     store_name,    ITEM(res_job.hdr.name), 0, ITEM_REQUIRED, 0},
   {"description", store_str,  ITEM(res_job.hdr.desc), 0, 0, 0},
   {"backup",   store_backup,  ITEM(res_job),          JT_BACKUP, 0, 0},
   {"verify",   store_backup,  ITEM(res_job),          JT_VERIFY, 0, 0},
   {"restore",  store_restore, ITEM(res_job),          JT_RESTORE, 0, 0},
   {"schedule", store_res,     ITEM(res_job.schedule), R_SCHEDULE, 0, 0},
   {"type",     store_jobtype, ITEM(res_job),          0, 0, 0},
   {"level",    store_level,   ITEM(res_job),          0, 0, 0},
   {"messages", store_res,     ITEM(res_job.messages), R_MSGS, 0, 0},
   {"storage",  store_res,     ITEM(res_job.storage),  R_STORAGE, 0, 0},
   {"pool",     store_res,     ITEM(res_job.pool),     R_POOL, 0, 0},
   {"client",   store_res,     ITEM(res_job.client),   R_CLIENT, 0, 0},
   {"fileset",  store_res,     ITEM(res_job.fileset),  R_FILESET, 0, 0},
   {"where",    store_dir,     ITEM(res_job.RestoreWhere), 0, 0, 0},
   {"replace",  store_replace, ITEM(res_job.replace), REPLACE_ALWAYS, ITEM_DEFAULT, 0},
   {"bootstrap",store_dir,     ITEM(res_job.RestoreBootstrap), 0, 0, 0},
   {"maxruntime", store_time,  ITEM(res_job.MaxRunTime), 0, 0, 0},
   {"maxstartdelay", store_time,ITEM(res_job.MaxStartDelay), 0, 0, 0},
   {"prunejobs",   store_yesno, ITEM(res_job.PruneJobs), 1, ITEM_DEFAULT, 0},
   {"prunefiles",  store_yesno, ITEM(res_job.PruneFiles), 1, ITEM_DEFAULT, 0},
   {"prunevolumes", store_yesno, ITEM(res_job.PruneVolumes), 1, ITEM_DEFAULT, 0},
   {"runbeforejob", store_str,  ITEM(res_job.RunBeforeJob), 0, 0, 0},
   {"runafterjob",  store_str,  ITEM(res_job.RunAfterJob),  0, 0, 0},
   {"spoolattributes", store_yesno, ITEM(res_job.SpoolAttributes), 1, ITEM_DEFAULT, 0},
   {"writebootstrap", store_dir, ITEM(res_job.WriteBootstrap), 0, 0, 0},
   {NULL, NULL, NULL, 0, 0, 0} 
};

/* FileSet resource
 *
 *   name	   handler     value		     code flags    default_value
 */
static struct res_items fs_items[] = {
   {"name",        store_name, ITEM(res_fs.hdr.name), 0, ITEM_REQUIRED, 0},
   {"description", store_str,  ITEM(res_fs.hdr.desc), 0, 0, 0},
   {"include",     store_inc,  NULL,                  0, 0, 0},
   {"exclude",     store_inc,  NULL,                  1, 0, 0},
   {NULL,	   NULL,       NULL,		      0, 0, 0} 
};

/* Schedule -- see run_conf.c */
/* Schedule
 *
 *   name	   handler     value		     code flags    default_value
 */
static struct res_items sch_items[] = {
   {"name",     store_name,  ITEM(res_sch.hdr.name), 0, ITEM_REQUIRED, 0},
   {"description", store_str, ITEM(res_sch.hdr.desc), 0, 0, 0},
   {"run",      store_run,   ITEM(res_sch.run),      0, 0, 0},
   {NULL, NULL, NULL, 0, 0, 0} 
};

/* Group resource -- not implemented
 *
 *   name	   handler     value		     code flags    default_value
 */
static struct res_items group_items[] = {
   {"name",        store_name, ITEM(res_group.hdr.name), 0, ITEM_REQUIRED, 0},
   {"description", store_str,  ITEM(res_group.hdr.desc), 0, 0, 0},
   {NULL, NULL, NULL, 0, 0, 0} 
};

/* Pool resource
 *
 *   name	      handler	  value 		       code flags default_value
 */
static struct res_items pool_items[] = {
   {"name",            store_name,    ITEM(res_pool.hdr.name),      0, ITEM_REQUIRED, 0},
   {"description",     store_str,     ITEM(res_pool.hdr.desc),      0, 0,     0},
   {"pooltype",        store_strname, ITEM(res_pool.pool_type),     0, ITEM_REQUIRED, 0},
   {"labelformat",     store_strname, ITEM(res_pool.label_format),  0, 0,     0},
   {"usecatalog",      store_yesno, ITEM(res_pool.use_catalog),     1, ITEM_DEFAULT,  1},
   {"usevolumeonce",   store_yesno, ITEM(res_pool.use_volume_once), 1, 0,        0},
   {"maximumvolumes",  store_pint,  ITEM(res_pool.max_volumes),     0, 0,        0},
   {"maximumvolumejobs", store_pint,  ITEM(res_pool.MaxVolJobs),    0, 0,       0},
   {"maximumvolumefiles", store_pint, ITEM(res_pool.MaxVolFiles),   0, 0,       0},
   {"maximumvolumebytes", store_size, ITEM(res_pool.MaxVolBytes),   0, 0,       0},
   {"acceptanyvolume", store_yesno, ITEM(res_pool.accept_any_volume), 1, ITEM_DEFAULT,     1},
   {"catalogfiles",    store_yesno, ITEM(res_pool.catalog_files),   1, ITEM_DEFAULT,  1},
   {"volumeretention", store_time,  ITEM(res_pool.VolRetention),    0, ITEM_DEFAULT, 60*60*24*365},
   {"volumeuseduration", store_time,  ITEM(res_pool.VolUseDuration),0, 0, 0},
   {"autoprune",       store_yesno, ITEM(res_pool.AutoPrune), 1, ITEM_DEFAULT, 1},
   {"recycle",         store_yesno, ITEM(res_pool.Recycle),     1, ITEM_DEFAULT, 1},
   {NULL, NULL, NULL, 0, 0, 0} 
};

/* 
 * Counter Resource
 *   name	      handler	  value 		       code flags default_value
 */
static struct res_items counter_items[] = {
   {"name",            store_name,    ITEM(res_counter.hdr.name),        0, ITEM_REQUIRED, 0},
   {"description",     store_str,     ITEM(res_counter.hdr.desc),        0, 0,     0},
   {"minimum",         store_int,     ITEM(res_counter.MinValue),        0, ITEM_DEFAULT, 0},
   {"maximum",         store_pint,    ITEM(res_counter.MaxValue),        0, ITEM_DEFAULT, INT32_MAX},
   {"global",          store_yesno,   ITEM(res_counter.Global),          0, ITEM_DEFAULT, 0},
   {"wrapcounter",     store_strname, ITEM(res_counter.WrapCounter),     0, 0, 0},
   {NULL, NULL, NULL, 0, 0, 0} 
};


/* Message resource */
extern struct res_items msgs_items[];

/* 
 * This is the master resource definition.  
 * It must have one item for each of the resources.
 *
 *  name	     items	  rcode        res_head
 */
struct s_res resources[] = {
   {"director",      dir_items,   R_DIRECTOR,  NULL},
   {"client",        cli_items,   R_CLIENT,    NULL},
   {"job",           job_items,   R_JOB,       NULL},
   {"storage",       store_items, R_STORAGE,   NULL},
   {"catalog",       cat_items,   R_CATALOG,   NULL},
   {"schedule",      sch_items,   R_SCHEDULE,  NULL},
   {"fileset",       fs_items,    R_FILESET,   NULL},
   {"group",         group_items, R_GROUP,     NULL},
   {"pool",          pool_items,  R_POOL,      NULL},
   {"messages",      msgs_items,  R_MSGS,      NULL},
   {"counter",       counter_items, R_COUNTER, NULL},
   {NULL,	     NULL,	  0,	       NULL}
};


/* Keywords (RHS) permitted in Job Level records   
 *
 *   level_name      level		job_type
 */
struct s_jl joblevels[] = {
   {"Full",          L_FULL,            JT_BACKUP},
   {"Incremental",   L_INCREMENTAL,     JT_BACKUP},
   {"Differential",  L_DIFFERENTIAL,    JT_BACKUP},
   {"Since",         L_SINCE,           JT_BACKUP},
   {"Catalog",       L_VERIFY_CATALOG,  JT_VERIFY},
   {"Initcatalog",   L_VERIFY_INIT,     JT_VERIFY},
   {"VolumeToCatalog", L_VERIFY_VOLUME_TO_CATALOG,   JT_VERIFY},
   {"Data",          L_VERIFY_DATA,     JT_VERIFY},
   {NULL,	     0}
};

/* Keywords (RHS) permitted in Job type records   
 *
 *   type_name	     job_type
 */
struct s_jt jobtypes[] = {
   {"backup",        JT_BACKUP},
   {"admin",         JT_ADMIN},
   {"verify",        JT_VERIFY},
   {"restore",       JT_RESTORE},
   {NULL,	     0}
};


/* Keywords (RHS) permitted in Backup and Verify records */
static struct s_kw BakVerFields[] = {
   {"client",        'C'},
   {"fileset",       'F'},
   {"level",         'L'}, 
   {NULL,	     0}
};

/* Keywords (RHS) permitted in Restore records */
static struct s_kw RestoreFields[] = {
   {"client",        'C'},
   {"fileset",       'F'},
   {"jobid",         'J'},            /* JobId to restore */
   {"where",         'W'},            /* root of restore */
   {"replace",       'R'},            /* replacement options */
   {"bootstrap",     'B'},            /* bootstrap file */
   {NULL,	       0}
};

/* Options permitted in Restore replace= */
struct s_kw ReplaceOptions[] = {
   {"always",         REPLACE_ALWAYS},
   {"ifnewer",        REPLACE_IFNEWER},
   {"ifolder",        REPLACE_IFOLDER},
   {"never",          REPLACE_NEVER},
   {NULL,		0}
};



/* Define FileSet KeyWord values */

#define INC_KW_NONE	    0
#define INC_KW_COMPRESSION  1
#define INC_KW_SIGNATURE    2
#define INC_KW_ENCRYPTION   3
#define INC_KW_VERIFY	    4
#define INC_KW_ONEFS	    5
#define INC_KW_RECURSE	    6
#define INC_KW_SPARSE	    7
#define INC_KW_REPLACE	    8	      /* restore options */

/* Include keywords */
static struct s_kw FS_option_kw[] = {
   {"compression", INC_KW_COMPRESSION},
   {"signature",   INC_KW_SIGNATURE},
   {"encryption",  INC_KW_ENCRYPTION},
   {"verify",      INC_KW_VERIFY},
   {"onefs",       INC_KW_ONEFS},
   {"recurse",     INC_KW_RECURSE},
   {"sparse",      INC_KW_SPARSE},
   {"replace",     INC_KW_REPLACE},
   {NULL,	   0}
};

/* Options for FileSet keywords */

struct s_fs_opt {
   char *name;
   int keyword;
   char *option;
};

/* Options permitted for each keyword and resulting value */
static struct s_fs_opt FS_options[] = {
   {"md5",      INC_KW_SIGNATURE,    "M"},
   {"gzip",     INC_KW_COMPRESSION,  "Z6"},
   {"gzip1",    INC_KW_COMPRESSION,  "Z1"},
   {"gzip2",    INC_KW_COMPRESSION,  "Z2"},
   {"gzip3",    INC_KW_COMPRESSION,  "Z3"},
   {"gzip4",    INC_KW_COMPRESSION,  "Z4"},
   {"gzip5",    INC_KW_COMPRESSION,  "Z5"},
   {"gzip6",    INC_KW_COMPRESSION,  "Z6"},
   {"gzip7",    INC_KW_COMPRESSION,  "Z7"},
   {"gzip8",    INC_KW_COMPRESSION,  "Z8"},
   {"gzip9",    INC_KW_COMPRESSION,  "Z9"},
   {"blowfish", INC_KW_ENCRYPTION,    "B"},   /* ***FIXME*** not implemented */
   {"3des",     INC_KW_ENCRYPTION,    "3"},   /* ***FIXME*** not implemented */
   {"yes",      INC_KW_ONEFS,         "0"},
   {"no",       INC_KW_ONEFS,         "f"},
   {"yes",      INC_KW_RECURSE,       "0"},
   {"no",       INC_KW_RECURSE,       "h"},
   {"yes",      INC_KW_SPARSE,        "s"},
   {"no",       INC_KW_SPARSE,        "0"},
   {"always",   INC_KW_REPLACE,       "a"},
   {"ifnewer",  INC_KW_REPLACE,       "w"},
   {"never",    INC_KW_REPLACE,       "n"},
   {NULL,	0,		     0}
};

char *level_to_str(int level)
{
   int i;
   static char level_no[30];
   char *str = level_no;

   sprintf(level_no, "%d", level);    /* default if not found */
   for (i=0; joblevels[i].level_name; i++) {
      if (level == joblevels[i].level) {
	 str = joblevels[i].level_name;
	 break;
      }
   }
   return str;
}



/* Dump contents of resource */
void dump_resource(int type, RES *reshdr, void sendit(void *sock, char *fmt, ...), void *sock)
{
   int i;
   URES *res = (URES *)reshdr;
   int recurse = 1;

   if (res == NULL) {
      sendit(sock, "No %s resource defined\n", res_to_str(type));
      return;
   }
   if (type < 0) {		      /* no recursion */
      type = - type;
      recurse = 0;
   }
   switch (type) {
      case R_DIRECTOR:
	 char ed1[30], ed2[30];
         sendit(sock, "Director: name=%s maxjobs=%d FDtimeout=%s SDtimeout=%s\n", 
	    reshdr->name, res->res_dir.MaxConcurrentJobs, 
	    edit_uint64(res->res_dir.FDConnectTimeout, ed1),
	    edit_uint64(res->res_dir.SDConnectTimeout, ed2));
	 if (res->res_dir.query_file) {
            sendit(sock, "   query_file=%s\n", res->res_dir.query_file);
	 }
	 if (res->res_dir.messages) {
            sendit(sock, "  --> ");
	    dump_resource(-R_MSGS, (RES *)res->res_dir.messages, sendit, sock);
	 }
	 break;
      case R_CLIENT:
         sendit(sock, "Client: name=%s address=%s FDport=%d\n",
	    res->res_client.hdr.name, res->res_client.address, res->res_client.FDport);
         sendit(sock, "JobRetention=%" lld " FileRetention=%" lld " AutoPrune=%d\n",
	    res->res_client.JobRetention, res->res_client.FileRetention,
	    res->res_client.AutoPrune);
	 if (res->res_client.catalog) {
            sendit(sock, "  --> ");
	    dump_resource(-R_CATALOG, (RES *)res->res_client.catalog, sendit, sock);
	 }
	 break;
      case R_STORAGE:
         sendit(sock, "Storage: name=%s address=%s SDport=%d\n\
         DeviceName=%s MediaType=%s\n",
	    res->res_store.hdr.name, res->res_store.address, res->res_store.SDport,
	    res->res_store.dev_name, res->res_store.media_type);
	 break;
      case R_CATALOG:
         sendit(sock, "Catalog: name=%s address=%s DBport=%d db_name=%s\n\
         db_user=%s\n",
	    res->res_cat.hdr.name, NPRT(res->res_cat.address),
	    res->res_cat.DBport, res->res_cat.db_name, NPRT(res->res_cat.db_user));
	 break;
      case R_JOB:
         sendit(sock, "Job: name=%s JobType=%d level=%s\n", res->res_job.hdr.name, 
	    res->res_job.JobType, level_to_str(res->res_job.level));
	 if (res->res_job.client) {
            sendit(sock, "  --> ");
	    dump_resource(-R_CLIENT, (RES *)res->res_job.client, sendit, sock);
	 }
	 if (res->res_job.fileset) {
            sendit(sock, "  --> ");
	    dump_resource(-R_FILESET, (RES *)res->res_job.fileset, sendit, sock);
	 }
	 if (res->res_job.schedule) {
            sendit(sock, "  --> ");
	    dump_resource(-R_SCHEDULE, (RES *)res->res_job.schedule, sendit, sock);
	 }
	 if (res->res_job.RestoreWhere) {
            sendit(sock, "  --> Where=%s\n", NPRT(res->res_job.RestoreWhere));
	 }
	 if (res->res_job.RestoreBootstrap) {
            sendit(sock, "  --> Bootstrap=%s\n", NPRT(res->res_job.RestoreBootstrap));
	 }
	 if (res->res_job.RunBeforeJob) {
            sendit(sock, "  --> RunBefore=%s\n", NPRT(res->res_job.RunBeforeJob));
	 }
	 if (res->res_job.RunAfterJob) {
            sendit(sock, "  --> RunAfter=%s\n", NPRT(res->res_job.RunAfterJob));
	 }
	 if (res->res_job.WriteBootstrap) {
            sendit(sock, "  --> WriteBootstrap=%s\n", NPRT(res->res_job.WriteBootstrap));
	 }
	 if (res->res_job.storage) {
            sendit(sock, "  --> ");
	    dump_resource(-R_STORAGE, (RES *)res->res_job.storage, sendit, sock);
	 }
	 if (res->res_job.pool) {
            sendit(sock, "  --> ");
	    dump_resource(-R_POOL, (RES *)res->res_job.pool, sendit, sock);
	 } else {
            sendit(sock, "!!! No Pool resource\n");
	 }
	 if (res->res_job.messages) {
            sendit(sock, "  --> ");
	    dump_resource(-R_MSGS, (RES *)res->res_job.messages, sendit, sock);
	 }
	 break;
      case R_FILESET:
         sendit(sock, "FileSet: name=%s\n", res->res_fs.hdr.name);
	 for (i=0; i<res->res_fs.num_includes; i++)
            sendit(sock, "      Inc: %s\n", res->res_fs.include_array[i]);
	 for (i=0; i<res->res_fs.num_excludes; i++)
            sendit(sock, "      Exc: %s\n", res->res_fs.exclude_array[i]);
	 break;
      case R_SCHEDULE:
	 if (res->res_sch.run) {
	    int i;
	    RUN *run = res->res_sch.run;
	    char buf[1000], num[10];
            sendit(sock, "Schedule: name=%s\n", res->res_sch.hdr.name);
	    if (!run) {
	       break;
	    }
next_run:
            sendit(sock, "  --> Run Level=%s\n", level_to_str(run->level));
            strcpy(buf, "      hour=");
	    for (i=0; i<24; i++) {
	       if (bit_is_set(i, run->hour)) {
                  sprintf(num, "%d ", i);
		  strcat(buf, num);
	       }
	    }
            strcat(buf, "\n");
	    sendit(sock, buf);
            strcpy(buf, "      mday=");
	    for (i=0; i<31; i++) {
	       if (bit_is_set(i, run->mday)) {
                  sprintf(num, "%d ", i+1);
		  strcat(buf, num);
	       }
	    }
            strcat(buf, "\n");
	    sendit(sock, buf);
            strcpy(buf, "      month=");
	    for (i=0; i<12; i++) {
	       if (bit_is_set(i, run->month)) {
                  sprintf(num, "%d ", i+1);
		  strcat(buf, num);
	       }
	    }
            strcat(buf, "\n");
	    sendit(sock, buf);
            strcpy(buf, "      wday=");
	    for (i=0; i<7; i++) {
	       if (bit_is_set(i, run->wday)) {
                  sprintf(num, "%d ", i+1);
		  strcat(buf, num);
	       }
	    }
            strcat(buf, "\n");
	    sendit(sock, buf);
            strcpy(buf, "      wpos=");
	    for (i=0; i<5; i++) {
	       if (bit_is_set(i, run->wpos)) {
                  sprintf(num, "%d ", i+1);
		  strcat(buf, num);
	       }
	    }
            strcat(buf, "\n");
	    sendit(sock, buf);
            sendit(sock, "      mins=%d\n", run->minute);
	    if (run->pool) {
               sendit(sock, "     --> ");
	       dump_resource(-R_POOL, (RES *)run->pool, sendit, sock);
	    }
	    if (run->storage) {
               sendit(sock, "     --> ");
	       dump_resource(-R_STORAGE, (RES *)run->storage, sendit, sock);
	    }
	    if (run->msgs) {
               sendit(sock, "     --> ");
	       dump_resource(-R_MSGS, (RES *)run->msgs, sendit, sock);
	    }
	    /* If another Run record is chained in, go print it */
	    if (run->next) {
	       run = run->next;
	       goto next_run;
	    }
	 } else {
            sendit(sock, "Schedule: name=%s\n", res->res_sch.hdr.name);
	 }
	 break;
      case R_GROUP:
         sendit(sock, "Group: name=%s\n", res->res_group.hdr.name);
	 break;
      case R_POOL:
         sendit(sock, "Pool: name=%s PoolType=%s\n", res->res_pool.hdr.name,
		 res->res_pool.pool_type);
         sendit(sock, "      use_cat=%d use_once=%d acpt_any=%d cat_files=%d\n",
		 res->res_pool.use_catalog, res->res_pool.use_volume_once,
		 res->res_pool.accept_any_volume, res->res_pool.catalog_files);
         sendit(sock, "      max_vols=%d auto_prune=%d VolRetention=%" lld "\n",
		 res->res_pool.max_volumes, res->res_pool.AutoPrune,
		 res->res_pool.VolRetention);
         sendit(sock, "      recycle=%d LabelFormat=%s\n", res->res_pool.Recycle,
		 NPRT(res->res_pool.label_format));
	 break;
      case R_MSGS:
         sendit(sock, "Messages: name=%s\n", res->res_msgs.hdr.name);
	 if (res->res_msgs.mail_cmd) 
            sendit(sock, "      mailcmd=%s\n", res->res_msgs.mail_cmd);
	 if (res->res_msgs.operator_cmd) 
            sendit(sock, "      opcmd=%s\n", res->res_msgs.operator_cmd);
	 break;
      default:
         sendit(sock, "Unknown resource type %d\n", type);
	 break;
   }
   if (recurse && res->res_dir.hdr.next) {
      dump_resource(type, res->res_dir.hdr.next, sendit, sock);
   }
}

/* 
 * Free memory of resource.  
 * NB, we don't need to worry about freeing any references
 * to other resources as they will be freed when that 
 * resource chain is traversed.  Mainly we worry about freeing
 * allocated strings (names).
 */
void free_resource(int type)
{
   int num;
   URES *res;
   RES *nres;
   int rindex = type - r_first;

   res = (URES *)resources[rindex].res_head;

   if (res == NULL)
      return;

   /* common stuff -- free the resource name and description */
   nres = (RES *)res->res_dir.hdr.next;
   if (res->res_dir.hdr.name) {
      free(res->res_dir.hdr.name);
   }
   if (res->res_dir.hdr.desc) {
      free(res->res_dir.hdr.desc);
   }

   switch (type) {
      case R_DIRECTOR:
	 if (res->res_dir.working_directory) {
	    free(res->res_dir.working_directory);
	 }
	 if (res->res_dir.pid_directory) {
	    free(res->res_dir.pid_directory);
	 }
	 if (res->res_dir.subsys_directory) {
	    free(res->res_dir.subsys_directory);
	 }
	 if (res->res_dir.password) {
	    free(res->res_dir.password);
	 }
	 if (res->res_dir.query_file) {
	    free(res->res_dir.query_file);
	 }
	 if (res->res_dir.DIRaddr) {
	    free(res->res_dir.DIRaddr);
	 }
	 break;
      case R_CLIENT:
	 if (res->res_client.address) {
	    free(res->res_client.address);
	 }
	 if (res->res_client.password) {
	    free(res->res_client.password);
	 }
	 break;
      case R_STORAGE:
	 if (res->res_store.address)
	    free(res->res_store.address);
	 if (res->res_store.password)
	    free(res->res_store.password);
	 if (res->res_store.media_type)
	    free(res->res_store.media_type);
	 if (res->res_store.dev_name)
	    free(res->res_store.dev_name);
	 break;
      case R_CATALOG:
	 if (res->res_cat.address)
	    free(res->res_cat.address);
	 if (res->res_cat.db_user)
	    free(res->res_cat.db_user);
	 if (res->res_cat.db_name)
	    free(res->res_cat.db_name);
	 if (res->res_cat.db_password)
	    free(res->res_cat.db_password);
	 break;
      case R_FILESET:
	 if ((num=res->res_fs.num_includes)) {
	    while (--num >= 0)	  
	       free(res->res_fs.include_array[num]);
	    free(res->res_fs.include_array);
	 }
	 if ((num=res->res_fs.num_excludes)) {
	    while (--num >= 0)	  
	       free(res->res_fs.exclude_array[num]);
	    free(res->res_fs.exclude_array);
	 }
	 break;
      case R_POOL:
	 if (res->res_pool.pool_type) {
	    free(res->res_pool.pool_type);
	 }
	 if (res->res_pool.label_format) {
	    free(res->res_pool.label_format);
	 }
	 break;
      case R_SCHEDULE:
	 if (res->res_sch.run) {
	    RUN *nrun, *next;
	    nrun = res->res_sch.run;
	    while (nrun) {
	       next = nrun->next;
	       free(nrun);
	       nrun = next;
	    }
	 }
	 break;
      case R_JOB:
	 if (res->res_job.RestoreWhere) {
	    free(res->res_job.RestoreWhere);
	 }
	 if (res->res_job.RestoreBootstrap) {
	    free(res->res_job.RestoreBootstrap);
	 }
	 if (res->res_job.WriteBootstrap) {
	    free(res->res_job.WriteBootstrap);
	 }
	 if (res->res_job.RunBeforeJob) {
	    free(res->res_job.RunBeforeJob);
	 }
	 if (res->res_job.RunAfterJob) {
	    free(res->res_job.RunAfterJob);
	 }
	 break;
      case R_MSGS:
	 if (res->res_msgs.mail_cmd)
	    free(res->res_msgs.mail_cmd);
	 if (res->res_msgs.operator_cmd)
	    free(res->res_msgs.operator_cmd);
	 free_msgs_res((MSGS *)res);  /* free message resource */
	 res = NULL;
	 break;
      case R_GROUP:
	 break;
      default:
         printf("Unknown resource type %d\n", type);
   }
   /* Common stuff again -- free the resource, recurse to next one */
   if (res) {
      free(res);
   }
   resources[rindex].res_head = nres;
   if (nres) {
      free_resource(type);
   }
}

/*
 * Save the new resource by chaining it into the head list for
 * the resource. If this is pass 2, we update any resource
 * pointers because they may not have been defined until 
 * later in pass 1.
 */
void save_resource(int type, struct res_items *items, int pass)
{
   URES *res;
   int rindex = type - r_first;
   int i, size;
   int error = 0;
   
   /* 
    * Ensure that all required items are present
    */
   for (i=0; items[i].name; i++) {
      if (items[i].flags & ITEM_REQUIRED) {
	    if (!bit_is_set(i, res_all.res_dir.hdr.item_present)) {  
               Emsg2(M_ERROR_TERM, 0, "%s item is required in %s resource, but not found.\n",
		 items[i].name, resources[rindex]);
	     }
      }
      /* If this triggers, take a look at lib/parse_conf.h */
      if (i >= MAX_RES_ITEMS) {
         Emsg1(M_ERROR_TERM, 0, "Too many items in %s resource\n", resources[rindex]);
      }
   }

   /* During pass 2 in each "store" routine, we looked up pointers 
    * to all the resources referrenced in the current resource, now we
    * must copy their addresses from the static record to the allocated
    * record.
    */
   if (pass == 2) {
      switch (type) {
	 /* Resources not containing a resource */
	 case R_CATALOG:
	 case R_STORAGE:
	 case R_FILESET:
	 case R_GROUP:
	 case R_POOL:
	 case R_MSGS:
	    break;

	 /* Resources containing another resource */
	 case R_DIRECTOR:
	    if ((res = (URES *)GetResWithName(R_DIRECTOR, res_all.res_dir.hdr.name)) == NULL) {
               Emsg1(M_ERROR_TERM, 0, "Cannot find Director resource %s\n", res_all.res_dir.hdr.name);
	    }
	    res->res_dir.messages = res_all.res_dir.messages;
	    break;
	 case R_JOB:
	    if ((res = (URES *)GetResWithName(R_JOB, res_all.res_dir.hdr.name)) == NULL) {
               Emsg1(M_ERROR_TERM, 0, "Cannot find Job resource %s\n", res_all.res_dir.hdr.name);
	    }
	    res->res_job.messages = res_all.res_job.messages;
	    res->res_job.schedule = res_all.res_job.schedule;
	    res->res_job.client   = res_all.res_job.client;
	    res->res_job.fileset  = res_all.res_job.fileset;
	    res->res_job.storage  = res_all.res_job.storage;
	    res->res_job.pool	  = res_all.res_job.pool;
	    if (res->res_job.JobType == 0) {
               Emsg1(M_ERROR_TERM, 0, "Job Type not defined for Job resource %s\n", res_all.res_dir.hdr.name);
	    }
	    if (res->res_job.level != 0) {
	       int i;
	       for (i=0; joblevels[i].level_name; i++) {
		  if (joblevels[i].level == res->res_job.level &&
		      joblevels[i].job_type == res->res_job.JobType) {
		     i = 0;
		     break;
		  }
	       }
	       if (i != 0) {
                  Emsg1(M_ERROR_TERM, 0, "Inappropriate level specified in Job resource %s\n", 
		     res_all.res_dir.hdr.name);
	       }
	    }
	    break;
	 case R_CLIENT:
	    if ((res = (URES *)GetResWithName(R_CLIENT, res_all.res_client.hdr.name)) == NULL) {
               Emsg1(M_ERROR_TERM, 0, "Cannot find Client resource %s\n", res_all.res_client.hdr.name);
	    }
	    res->res_client.catalog = res_all.res_client.catalog;
	    break;
	 case R_SCHEDULE:
	    /* Schedule is a bit different in that it contains a RUN record
             * chain which isn't a "named" resource. This chain was linked
	     * in by run_conf.c during pass 2, so here we jam the pointer 
	     * into the Schedule resource.			   
	     */
	    if ((res = (URES *)GetResWithName(R_SCHEDULE, res_all.res_client.hdr.name)) == NULL) {
               Emsg1(M_ERROR_TERM, 0, "Cannot find Schedule resource %s\n", res_all.res_client.hdr.name);
	    }
	    res->res_sch.run = res_all.res_sch.run;
	    break;
	 default:
            Emsg1(M_ERROR, 0, "Unknown resource type %d\n", type);
	    error = 1;
	    break;
      }
      /* Note, the resource name was already saved during pass 1,
       * so here, we can just release it.
       */
      if (res_all.res_dir.hdr.name) {
	 free(res_all.res_dir.hdr.name);
	 res_all.res_dir.hdr.name = NULL;
      }
      if (res_all.res_dir.hdr.desc) {
	 free(res_all.res_dir.hdr.desc);
	 res_all.res_dir.hdr.desc = NULL;
      }
      return;
   }

   /* The following code is only executed for pass 1 */
   switch (type) {
      case R_DIRECTOR:
	 size = sizeof(DIRRES);
	 break;
      case R_CLIENT:
	 size =sizeof(CLIENT);
	 break;
      case R_STORAGE:
	 size = sizeof(STORE); 
	 break;
      case R_CATALOG:
	 size = sizeof(CAT);
	 break;
      case R_JOB:
	 size = sizeof(JOB);
	 break;
      case R_FILESET:
	 size = sizeof(FILESET);
	 break;
      case R_SCHEDULE:
	 size = sizeof(SCHED);
	 break;
      case R_GROUP:
	 size = sizeof(GROUP);
	 break;
      case R_POOL:
	 size = sizeof(POOL);
	 break;
      case R_MSGS:
	 size = sizeof(MSGS);
	 break;
      default:
         printf("Unknown resource type %d\n", type);
	 error = 1;
	 size = 1;
	 break;
   }
   /* Common */
   if (!error) {
      res = (URES *)malloc(size);
      memcpy(res, &res_all, size);
      if (!resources[rindex].res_head) {
	 resources[rindex].res_head = (RES *)res; /* store first entry */
      } else {
	 RES *next;
	 /* Add new res to end of chain */
	 for (next=resources[rindex].res_head; next->next; next=next->next)
	    { }
	 next->next = (RES *)res;
         Dmsg2(90, "Inserting %s res: %s\n", res_to_str(type),
	       res->res_dir.hdr.name);
      }
   }
}

/* 
 * Store JobType (backup, verify, restore)
 *
 */
static void store_jobtype(LEX *lc, struct res_items *item, int index, int pass)
{
   int token, i;   

   token = lex_get_token(lc, T_NAME);
   /* Store the type both pass 1 and pass 2 */
   for (i=0; jobtypes[i].type_name; i++) {
      if (strcasecmp(lc->str, jobtypes[i].type_name) == 0) {
	 ((JOB *)(item->value))->JobType = jobtypes[i].job_type;
	 i = 0;
	 break;
      }
   }
   if (i != 0) {
      scan_err1(lc, "Expected a Job Type keyword, got: %s", lc->str);
   }
   scan_to_eol(lc);
   set_bit(index, res_all.hdr.item_present);
}

/* 
 * Store Job Level (Full, Incremental, ...)
 *
 */
static void store_level(LEX *lc, struct res_items *item, int index, int pass)
{
   int token, i;

   token = lex_get_token(lc, T_NAME);
   /* Store the level pass 2 so that type is defined */
   for (i=0; joblevels[i].level_name; i++) {
      if (strcasecmp(lc->str, joblevels[i].level_name) == 0) {
	 ((JOB *)(item->value))->level = joblevels[i].level;
	 i = 0;
	 break;
      }
   }
   if (i != 0) {
      scan_err1(lc, "Expected a Job Level keyword, got: %s", lc->str);
   }
   scan_to_eol(lc);
   set_bit(index, res_all.hdr.item_present);
}

static void store_replace(LEX *lc, struct res_items *item, int index, int pass)
{
   int token, i;
   token = lex_get_token(lc, T_NAME);
   /* Scan Replacement options */
   for (i=0; ReplaceOptions[i].name; i++) {
      if (strcasecmp(lc->str, ReplaceOptions[i].name) == 0) {
	  ((JOB *)(item->value))->replace = ReplaceOptions[i].token;
	 i = 0;
	 break;
      }
   }
   if (i != 0) {
      scan_err1(lc, "Expected a Restore replacement option, got: %s", lc->str);
   }
   scan_to_eol(lc);
   set_bit(index, res_all.hdr.item_present);
}

/* 
 * Store backup/verify info for Job record 
 *
 * Note, this code is used for both BACKUP and VERIFY jobs
 *
 *    Backup = Client=<client-name> FileSet=<FileSet-name> Level=<level>
 */
static void store_backup(LEX *lc, struct res_items *item, int index, int pass)
{
   int token, i;
   RES *res;
   int options = lc->options;

   lc->options |= LOPT_NO_IDENT;      /* make spaces significant */

   
   ((JOB *)(item->value))->JobType = item->code;
   while ((token = lex_get_token(lc, T_ALL)) != T_EOL) {
      int found;

      Dmsg1(150, "store_backup got token=%s\n", lex_tok_to_str(token));
      
      if (token != T_IDENTIFIER && token != T_UNQUOTED_STRING && token != T_QUOTED_STRING) {
         scan_err1(lc, "Expected a backup/verify keyword, got: %s", lc->str);
      }
      Dmsg1(190, "Got keyword: %s\n", lc->str);
      found = FALSE;
      for (i=0; BakVerFields[i].name; i++) {
	 if (strcasecmp(lc->str, BakVerFields[i].name) == 0) {
	    found = TRUE;
	    if (lex_get_token(lc, T_ALL) != T_EQUALS) {
               scan_err1(lc, "Expected an equals, got: %s", lc->str);
	    }
	    token = lex_get_token(lc, T_NAME);
            Dmsg1(190, "Got value: %s\n", lc->str);
	    switch (BakVerFields[i].token) {
               case 'C':
		  /* Find Client Resource */
		  if (pass == 2) {
		     res = GetResWithName(R_CLIENT, lc->str);
		     if (res == NULL) {
                        scan_err1(lc, "Could not find specified Client Resource: %s",
				   lc->str);
		     }
		     res_all.res_job.client = (CLIENT *)res;
		  }
		  break;
               case 'F':
		  /* Find FileSet Resource */
		  if (pass == 2) {
		     res = GetResWithName(R_FILESET, lc->str);
		     if (res == NULL) {
                        scan_err1(lc, "Could not find specified FileSet Resource: %s\n",
				    lc->str);
		     }
		     res_all.res_job.fileset = (FILESET *)res;
		  }
		  break;
               case 'L':
		  /* Get level */
		  for (i=0; joblevels[i].level_name; i++) {
		     if (joblevels[i].job_type == item->code && 
			  strcasecmp(lc->str, joblevels[i].level_name) == 0) {
			((JOB *)(item->value))->level = joblevels[i].level;
			i = 0;
			break;
		     }
		  }
		  if (i != 0) {
                     scan_err1(lc, "Expected a Job Level keyword, got: %s", lc->str);
		  }
		  break;
	    } /* end switch */
	    break;
	 } /* end if strcmp() */
      } /* end for */
      if (!found) {
         scan_err1(lc, "%s not a valid Backup/verify keyword", lc->str);
      }
   } /* end while */
   lc->options = options;	      /* reset original options */
   set_bit(index, res_all.hdr.item_present);
}

/* 
 * Store restore info for Job record 
 *
 *    Restore = JobId=<job-id> Where=<root-directory> Replace=<options> Bootstrap=<file>
 *
 */
static void store_restore(LEX *lc, struct res_items *item, int index, int pass)
{
   int token, i;
   RES *res;
   int options = lc->options;

   lc->options |= LOPT_NO_IDENT;      /* make spaces significant */

   Dmsg0(190, "Enter store_restore()\n");
   
   ((JOB *)(item->value))->JobType = item->code;
   while ((token = lex_get_token(lc, T_ALL)) != T_EOL) {
      int found; 

      if (token != T_IDENTIFIER && token != T_UNQUOTED_STRING && token != T_QUOTED_STRING) {
         scan_err1(lc, "expected a name, got: %s", lc->str);
      }
      found = FALSE;
      for (i=0; RestoreFields[i].name; i++) {
         Dmsg1(190, "Restore kw=%s\n", lc->str);
	 if (strcasecmp(lc->str, RestoreFields[i].name) == 0) {
	    found = TRUE;
	    if (lex_get_token(lc, T_ALL) != T_EQUALS) {
               scan_err1(lc, "Expected an equals, got: %s", lc->str);
	    }
	    token = lex_get_token(lc, T_ALL);
            Dmsg1(190, "Restore value=%s\n", lc->str);
	    switch (RestoreFields[i].token) {
               case 'B':
		  /* Bootstrap */
		  if (token != T_IDENTIFIER && token != T_UNQUOTED_STRING && token != T_QUOTED_STRING) {
                     scan_err1(lc, "Expected a Restore bootstrap file, got: %s", lc->str);
		  }
		  if (pass == 1) {
		     res_all.res_job.RestoreBootstrap = bstrdup(lc->str);
		  }
		  break;
               case 'C':
		  /* Find Client Resource */
		  if (pass == 2) {
		     res = GetResWithName(R_CLIENT, lc->str);
		     if (res == NULL) {
                        scan_err1(lc, "Could not find specified Client Resource: %s",
				   lc->str);
		     }
		     res_all.res_job.client = (CLIENT *)res;
		  }
		  break;
               case 'F':
		  /* Find FileSet Resource */
		  if (pass == 2) {
		     res = GetResWithName(R_FILESET, lc->str);
		     if (res == NULL) {
                        scan_err1(lc, "Could not find specified FileSet Resource: %s\n",
				    lc->str);
		     }
		     res_all.res_job.fileset = (FILESET *)res;
		  }
		  break;
               case 'J':
		  /* JobId */
		  if (token != T_NUMBER) {
                     scan_err1(lc, "expected an integer number, got: %s", lc->str);
		  }
		  errno = 0;
		  res_all.res_job.RestoreJobId = strtol(lc->str, NULL, 0);
                  Dmsg1(190, "RestorJobId=%d\n", res_all.res_job.RestoreJobId);
		  if (errno != 0) {
                     scan_err1(lc, "expected an integer number, got: %s", lc->str);
		  }
		  break;
               case 'W':
		  /* Where */
		  if (token != T_IDENTIFIER && token != T_UNQUOTED_STRING && token != T_QUOTED_STRING) {
                     scan_err1(lc, "Expected a Restore root directory, got: %s", lc->str);
		  }
		  if (pass == 1) {
		     res_all.res_job.RestoreWhere = bstrdup(lc->str);
		  }
		  break;
               case 'R':
		  /* Replacement options */
		  if (token != T_IDENTIFIER && token != T_UNQUOTED_STRING && token != T_QUOTED_STRING) {
                     scan_err1(lc, "Expected a keyword name, got: %s", lc->str);
		  }
		  /* Fix to scan Replacement options */
		  for (i=0; ReplaceOptions[i].name; i++) {
		     if (strcasecmp(lc->str, ReplaceOptions[i].name) == 0) {
			 ((JOB *)(item->value))->replace = ReplaceOptions[i].token;
			i = 0;
			break;
		     }
		  }
		  if (i != 0) {
                     scan_err1(lc, "Expected a Restore replacement option, got: %s", lc->str);
		  }
		  break;
	    } /* end switch */
	    break;
	 } /* end if strcmp() */
      } /* end for */
      if (!found) {
         scan_err1(lc, "%s not a valid Restore keyword", lc->str);
      }
   } /* end while */
   lc->options = options;	      /* reset original options */
   set_bit(index, res_all.hdr.item_present);
}



/* 
 * Scan for Include options (keyword=option) is converted into one or
 *  two characters. Verifyopts=xxxx is Vxxxx:
 */
static void scan_include_options(LEX *lc, int keyword, char *opts, int optlen)
{
   int token, i;
   char option[3];

   option[0] = 0;		      /* default option = none */
   option[2] = 0;		      /* terminate options */
   token = lex_get_token(lc, T_NAME);		  /* expect at least one option */	 
   if (keyword == INC_KW_VERIFY) { /* special case */
      /* ***FIXME**** ensure these are in permitted set */
      bstrncat(opts, "V", optlen);         /* indicate Verify */
      bstrncat(opts, lc->str, optlen);
      bstrncat(opts, ":", optlen);         /* terminate it */
   } else {
      for (i=0; FS_options[i].name; i++) {
	 if (strcasecmp(lc->str, FS_options[i].name) == 0 && FS_options[i].keyword == keyword) {
	    /* NOTE! maximum 2 letters here or increase option[3] */
	    option[0] = FS_options[i].option[0];
	    option[1] = FS_options[i].option[1];
	    i = 0;
	    break;
	 }
      }
      if (i != 0) {
         scan_err1(lc, "Expected a FileSet option keyword, got:%s:", lc->str);
      } else { /* add option */
	 bstrncat(opts, option, optlen);
         Dmsg3(200, "Catopts=%s option=%s optlen=%d\n", opts, option,optlen);
      }
   }

   /* If option terminated by comma, eat it */
   if (lc->ch == ',') {
      token = lex_get_token(lc, T_ALL);      /* yes, eat comma */
   }
}


/* Store FileSet Include/Exclude info */
static void store_inc(LEX *lc, struct res_items *item, int index, int pass)
{
   int token, i;
   int options = lc->options;
   int keyword;
   char *fname;
   char inc_opts[100];
   int inc_opts_len;

   lc->options |= LOPT_NO_IDENT;      /* make spaces significant */

   /* Get include options */
   inc_opts[0] = 0;
   while ((token=lex_get_token(lc, T_ALL)) != T_BOB) {
      keyword = INC_KW_NONE;
      for (i=0; FS_option_kw[i].name; i++) {
	 if (strcasecmp(lc->str, FS_option_kw[i].name) == 0) {
	    keyword = FS_option_kw[i].token;
	    break;
	 }
      }
      if (keyword == INC_KW_NONE) {
         scan_err1(lc, "Expected a FileSet keyword, got: %s", lc->str);
      }
      /* Option keyword should be following by = <option> */
      if ((token=lex_get_token(lc, T_ALL)) != T_EQUALS) {
         scan_err1(lc, "expected an = following keyword, got: %s", lc->str);
      }
      scan_include_options(lc, keyword, inc_opts, sizeof(inc_opts));
      if (token == T_BOB) {
	 break;
      }
   }
   if (!inc_opts[0]) {
      strcat(inc_opts, "0 ");         /* set no options */
   } else {
      strcat(inc_opts, " ");          /* add field separator */
   }
   inc_opts_len = strlen(inc_opts);

   if (pass == 1) {
      if (!res_all.res_fs.have_MD5) {
	 MD5Init(&res_all.res_fs.md5c);
	 res_all.res_fs.have_MD5 = TRUE;
      }
      /* Pickup include/exclude names. Note, they are stored as
       * XYZ fname
       * where XYZ are the include/exclude options for the FileSet
       *     a "0 " (zero) indicates no options,
       * and fname is the file/directory name given
       */
      while ((token = lex_get_token(lc, T_ALL)) != T_EOB) {
	 switch (token) {
	    case T_COMMA:
	    case T_EOL:
	       continue;

	    case T_IDENTIFIER:
	    case T_UNQUOTED_STRING:
	    case T_QUOTED_STRING:
	       fname = (char *) malloc(lc->str_len + inc_opts_len + 1);
	       strcpy(fname, inc_opts);
	       strcat(fname, lc->str);
	       if (res_all.res_fs.have_MD5) {
		  MD5Update(&res_all.res_fs.md5c, (unsigned char *) fname, inc_opts_len + lc->str_len);
	       }
	       if (item->code == 0) { /* include */
		  if (res_all.res_fs.num_includes == res_all.res_fs.include_size) {
		     res_all.res_fs.include_size += 10;
		     if (res_all.res_fs.include_array == NULL) {
			res_all.res_fs.include_array = (char **) malloc(sizeof(char *) * res_all.res_fs.include_size);
		     } else {
			res_all.res_fs.include_array = (char **) realloc(res_all.res_fs.include_array,
			   sizeof(char *) * res_all.res_fs.include_size);
		     }
		  }
		  res_all.res_fs.include_array[res_all.res_fs.num_includes++] =    
		     fname;
	       } else { 	       /* exclude */
		  if (res_all.res_fs.num_excludes == res_all.res_fs.exclude_size) {
		     res_all.res_fs.exclude_size += 10;
		     if (res_all.res_fs.exclude_array == NULL) {
			res_all.res_fs.exclude_array = (char **) malloc(sizeof(char *) * res_all.res_fs.exclude_size);
		     } else {
			res_all.res_fs.exclude_array = (char **) realloc(res_all.res_fs.exclude_array,
			   sizeof(char *) * res_all.res_fs.exclude_size);
		     }
		  }
		  res_all.res_fs.exclude_array[res_all.res_fs.num_excludes++] =    
		     fname;
	       }
	       break;
	    default:
               scan_err1(lc, "Expected a filename, got: %s", lc->str);
	 }				   
      }
   } else { /* pass 2 */
      while (lex_get_token(lc, T_ALL) != T_EOB) 
	 {}
   }
   scan_to_eol(lc);
   lc->options = options;
   set_bit(index, res_all.hdr.item_present);
}
