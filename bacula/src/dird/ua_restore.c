/*
 *
 *   Bacula Director -- User Agent Database restore Command
 *	Creates a bootstrap file for restoring files
 *
 *     Kern Sibbald, July MMII
 *
 *   Version $Id$
 */

/*
   Copyright (C) 2002-2003 Kern Sibbald and John Walker

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
#include <fnmatch.h>
#include "findlib/find.h"



/* Imported functions */
extern int runcmd(UAContext *ua, char *cmd);

/* Imported variables */
extern char *uar_list_jobs,	*uar_file,	  *uar_sel_files;
extern char *uar_del_temp,	*uar_del_temp1,   *uar_create_temp;
extern char *uar_create_temp1,	*uar_last_full,   *uar_full;
extern char *uar_inc,		*uar_list_temp,   *uar_sel_jobid_temp;
extern char *uar_sel_all_temp1, *uar_sel_fileset, *uar_mediatype;


/* Context for insert_tree_handler() */
typedef struct s_tree_ctx {
   TREE_ROOT *root;		      /* root */
   TREE_NODE *node;		      /* current node */
   TREE_NODE *avail_node;	      /* unused node last insert */
   int cnt;			      /* count for user feedback */
   UAContext *ua;
} TREE_CTX;

/* Main structure for obtaining JobIds */
typedef struct s_jobids {
   utime_t JobTDate;
   uint32_t TotalFiles;
   char ClientName[MAX_NAME_LENGTH];
   char JobIds[200];
   STORE  *store;
} JobIds;


/* FileIndex entry in bootstrap record */
typedef struct s_rbsr_findex {
   struct s_rbsr_findex *next;
   int32_t findex;
   int32_t findex2;
} RBSR_FINDEX;

/* Restore bootstrap record -- not the real one, but useful here */
typedef struct s_rbsr {
   struct s_rbsr *next; 	      /* next JobId */
   uint32_t JobId;		      /* JobId this bsr */
   uint32_t VolSessionId;		    
   uint32_t VolSessionTime;
   int	    VolCount;		      /* Volume parameter count */
   VOL_PARAMS *VolParams;	      /* Volume, start/end file/blocks */
   RBSR_FINDEX *fi;		      /* File indexes this JobId */
} RBSR;

typedef struct s_name_ctx {
   char **name; 		      /* list of names */
   int num_ids; 		      /* ids stored */
   int max_ids; 		      /* size of array */
   int num_del; 		      /* number deleted */
   int tot_ids; 		      /* total to process */
} NAME_LIST;

#define MAX_ID_LIST_LEN 1000000


/* Forward referenced functions */
static RBSR *new_bsr();
static void free_bsr(RBSR *bsr);
static void write_bsr(UAContext *ua, RBSR *bsr, FILE *fd);
static int  write_bsr_file(UAContext *ua, RBSR *bsr);
static void print_bsr(UAContext *ua, RBSR *bsr);
static int  complete_bsr(UAContext *ua, RBSR *bsr);
static int insert_tree_handler(void *ctx, int num_fields, char **row);
static void add_findex(RBSR *bsr, uint32_t JobId, int32_t findex);
static int last_full_handler(void *ctx, int num_fields, char **row);
static int jobid_handler(void *ctx, int num_fields, char **row);
static int next_jobid_from_list(char **p, uint32_t *JobId);
static int user_select_jobids(UAContext *ua, JobIds *ji);
static void user_select_files(TREE_CTX *tree);
static int fileset_handler(void *ctx, int num_fields, char **row);
static void print_name_list(UAContext *ua, NAME_LIST *name_list);
static int unique_name_list_handler(void *ctx, int num_fields, char **row);
static void free_name_list(NAME_LIST *name_list);
static void get_storage_from_mediatype(UAContext *ua, NAME_LIST *name_list, JobIds *ji);


/*
 *   Restore files
 *
 */
int restorecmd(UAContext *ua, char *cmd)
{
   POOLMEM *query;
   TREE_CTX tree;
   JobId_t JobId, last_JobId;
   char *p;
   RBSR *bsr;
   char *nofname = "";
   JobIds ji;
   JOB *job = NULL;
   JOB *restore_job = NULL;
   int restore_jobs = 0;
   NAME_LIST name_list;
   uint32_t selected_files = 0;

   if (!open_db(ua)) {
      return 0;
   }

   memset(&tree, 0, sizeof(TREE_CTX));
   memset(&name_list, 0, sizeof(name_list));
   memset(&ji, 0, sizeof(ji));

   /* Ensure there is at least one Restore Job */
   LockRes();
   while ( (job = (JOB *)GetNextRes(R_JOB, (RES *)job)) ) {
      if (job->JobType == JT_RESTORE) {
	 if (!restore_job) {
	    restore_job = job;
	 }
	 restore_jobs++;
      }
   }
   UnlockRes();
   if (!restore_jobs) {
      bsendmsg(ua, _(
         "No Restore Job Resource found. You must create at least\n"
         "one before running this command.\n"));
      return 0;
   }

   /* 
    * Request user to select JobIds by various different methods
    *  last 20 jobs, where File saved, most recent backup, ...
    */
   if (!user_select_jobids(ua, &ji)) {
      return 0;
   }

   /* 
    * Build the directory tree containing JobIds user selected
    */
   tree.root = new_tree(ji.TotalFiles);
   tree.root->fname = nofname;
   tree.ua = ua;
   query = get_pool_memory(PM_MESSAGE);
   last_JobId = 0;
   /*
    * For display purposes, the same JobId, with different volumes may
    * appear more than once, however, we only insert it once.
    */
   for (p=ji.JobIds; next_jobid_from_list(&p, &JobId) > 0; ) {

      if (JobId == last_JobId) {	     
	 continue;		      /* eliminate duplicate JobIds */
      }
      last_JobId = JobId;
      bsendmsg(ua, _("Building directory tree for JobId %u ...\n"), JobId);
      /*
       * Find files for this JobId and insert them in the tree
       */
      Mmsg(&query, uar_sel_files, JobId);
      if (!db_sql_query(ua->db, query, insert_tree_handler, (void *)&tree)) {
         bsendmsg(ua, "%s", db_strerror(ua->db));
      }
      /*
       * Find the FileSets for this JobId and add to the name_list
       */
      Mmsg(&query, uar_mediatype, JobId);
      if (!db_sql_query(ua->db, query, unique_name_list_handler, (void *)&name_list)) {
         bsendmsg(ua, "%s", db_strerror(ua->db));
      }

   }
   bsendmsg(ua, "%d items inserted into the tree and marked for extraction.\n");
   free_pool_memory(query);

   /* Check MediaType and select storage that corresponds */
   get_storage_from_mediatype(ua, &name_list, &ji);
   free_name_list(&name_list);

   /* Let the user select which files to restore */
   user_select_files(&tree);

   /*
    * Walk down through the tree finding all files marked to be 
    *  extracted making a bootstrap file.
    */
   bsr = new_bsr();
   for (TREE_NODE *node=first_tree_node(tree.root); node; node=next_tree_node(node)) {
      Dmsg2(400, "FI=%d node=0x%x\n", node->FileIndex, node);
      if (node->extract) {
         Dmsg2(400, "type=%d FI=%d\n", node->type, node->FileIndex);
	 add_findex(bsr, node->JobId, node->FileIndex);
	 selected_files++;
      }
   }

   free_tree(tree.root);	      /* free the directory tree */

   if (bsr->JobId) {
      if (!complete_bsr(ua, bsr)) {   /* find Vol, SessId, SessTime from JobIds */
         bsendmsg(ua, _("Unable to construct a valid BSR. Cannot continue.\n"));
	 free_bsr(bsr);
	 return 0;
      }
//    print_bsr(ua, bsr);
      write_bsr_file(ua, bsr);
      bsendmsg(ua, _("\n%u files selected to restore.\n\n"), selected_files);
   } else {
      bsendmsg(ua, _("No files selected to restore.\n"));
   }
   free_bsr(bsr);

   if (restore_jobs == 1) {
      job = restore_job;
   } else {
      job = select_restore_job_resource(ua);
   }
   if (!job) {
      bsendmsg(ua, _("No Restore Job resource found!\n"));
      return 0;
   }

   /* If no client name specified yet, get it now */
   if (!ji.ClientName[0]) {
      CLIENT_DBR cr;
      memset(&cr, 0, sizeof(cr));
      if (!get_client_dbr(ua, &cr)) {
	 return 0;
      }
      bstrncpy(ji.ClientName, cr.Name, sizeof(ji.ClientName));
   }

    /* Build run command */
    Mmsg(&ua->cmd, 
       "run job=\"%s\" client=\"%s\" storage=\"%s\" bootstrap=\"%s/restore.bsr\"",
       job->hdr.name, ji.ClientName, ji.store?ji.store->hdr.name:"",
       working_directory);

   Dmsg1(400, "Submitting: %s\n", ua->cmd);
   
   parse_ua_args(ua);
   runcmd(ua, ua->cmd);

   bsendmsg(ua, _("Restore command done.\n"));
   return 1;
}

/*
 * The first step in the restore process is for the user to 
 *  select a list of JobIds from which he will subsequently
 *  select which files are to be restored.
 */
static int user_select_jobids(UAContext *ua, JobIds *ji)
{
   char fileset_name[MAX_NAME_LENGTH];
   char *p, ed1[50];
   FILESET_DBR fsr;
   CLIENT_DBR cr;
   JobId_t JobId;
   JOB_DBR jr;
   POOLMEM *query;
   int done = 0;
   char *list[] = { 
      "List last 20 Jobs run",
      "List Jobs where a given File is saved",
      "Enter list of JobIds to select",
      "Enter SQL list command", 
      "Select the most recent backup for a client",
      "Cancel",
      NULL };

   bsendmsg(ua, _("\nFirst you select one or more JobIds that contain files\n"
                  "to be restored. You will be presented several methods\n"
                  "of specifying the JobIds. Then you will be allowed to\n"
                  "select which files from those JobIds are to be restored.\n\n"));

   for ( ; !done; ) {
      start_prompt(ua, _("To select the JobIds, you have the following choices:\n"));
      for (int i=0; list[i]; i++) {
	 add_prompt(ua, list[i]);
      }
      done = 1;
      switch (do_prompt(ua, "Select item: ", NULL, 0)) {
      case -1:			      /* error */
	 return 0;
      case 0:			      /* list last 20 Jobs run */
	 db_list_sql_query(ua->jcr, ua->db, uar_list_jobs, prtit, ua, 1, 0);
	 done = 0;
	 break;
      case 1:			      /* list where a file is saved */
	 char *fname;
	 int len;
         if (!get_cmd(ua, _("Enter Filename: "))) {
	    return 0;
	 }
	 len = strlen(ua->cmd);
	 fname = (char *)malloc(len * 2 + 1);
	 db_escape_string(fname, ua->cmd, len);
	 query = get_pool_memory(PM_MESSAGE);
	 Mmsg(&query, uar_file, fname);
	 free(fname);
	 db_list_sql_query(ua->jcr, ua->db, query, prtit, ua, 1, 0);
	 free_pool_memory(query);
	 done = 0;
	 break;
      case 2:			      /* enter a list of JobIds */
         if (!get_cmd(ua, _("Enter JobId(s), comma separated, to restore: "))) {
	    return 0;
	 }
	 bstrncpy(ji->JobIds, ua->cmd, sizeof(ji->JobIds));
	 break;
      case 3:			      /* Enter an SQL list command */
         if (!get_cmd(ua, _("Enter SQL list command: "))) {
	    return 0;
	 }
	 db_list_sql_query(ua->jcr, ua->db, ua->cmd, prtit, ua, 1, 0);
	 done = 0;
	 break;
      case 4:			      /* Select the most recent backups */
	 query = get_pool_memory(PM_MESSAGE);
	 db_sql_query(ua->db, uar_del_temp, NULL, NULL);
	 db_sql_query(ua->db, uar_del_temp1, NULL, NULL);
	 if (!db_sql_query(ua->db, uar_create_temp, NULL, NULL)) {
            bsendmsg(ua, "%s\n", db_strerror(ua->db));
	 }
	 if (!db_sql_query(ua->db, uar_create_temp1, NULL, NULL)) {
            bsendmsg(ua, "%s\n", db_strerror(ua->db));
	 }
	 /*
	  * Select Client from the Catalog
	  */
	 memset(&cr, 0, sizeof(cr));
	 if (!get_client_dbr(ua, &cr)) {
	    free_pool_memory(query);
	    db_sql_query(ua->db, uar_del_temp, NULL, NULL);
	    db_sql_query(ua->db, uar_del_temp1, NULL, NULL);
	    return 0;
	 }
	 bstrncpy(ji->ClientName, cr.Name, sizeof(ji->ClientName));

	 /*
	  * Select FileSet 
	  */
	 Mmsg(&query, uar_sel_fileset, cr.ClientId, cr.ClientId);
         start_prompt(ua, _("The defined FileSet resources are:\n"));
	 if (!db_sql_query(ua->db, query, fileset_handler, (void *)ua)) {
            bsendmsg(ua, "%s\n", db_strerror(ua->db));
	 }
         if (do_prompt(ua, _("Select FileSet resource"), 
		       fileset_name, sizeof(fileset_name)) < 0) {
	    free_pool_memory(query);
	    db_sql_query(ua->db, uar_del_temp, NULL, NULL);
	    db_sql_query(ua->db, uar_del_temp1, NULL, NULL);
	    return 0;
	 }
	 fsr.FileSetId = atoi(fileset_name);  /* Id is first part of name */
	 if (!db_get_fileset_record(ua->jcr, ua->db, &fsr)) {
            bsendmsg(ua, _("Error getting FileSet record: %s\n"), db_strerror(ua->db));
            bsendmsg(ua, _("This probably means you modified the FileSet.\n"
                           "Continuing anyway.\n"));
	 }

	 /* Find JobId of last Full backup for this client, fileset */
	 Mmsg(&query, uar_last_full, cr.ClientId, cr.ClientId, fsr.FileSetId);
	 if (!db_sql_query(ua->db, query, NULL, NULL)) {
            bsendmsg(ua, "%s\n", db_strerror(ua->db));
	 }
	 /* Find all Volumes used by that JobId */
	 if (!db_sql_query(ua->db, uar_full, NULL,NULL)) {
            bsendmsg(ua, "%s\n", db_strerror(ua->db));
	 }
         /* Note, this is needed as I don't seem to get the callback
	  * from the call just above.
	  */
	 if (!db_sql_query(ua->db, uar_sel_all_temp1, last_full_handler, (void *)ji)) {
            bsendmsg(ua, "%s\n", db_strerror(ua->db));
	 }
	 /* Now find all Incremental Jobs */
	 Mmsg(&query, uar_inc, edit_uint64(ji->JobTDate, ed1), cr.ClientId, fsr.FileSetId);
	 if (!db_sql_query(ua->db, query, NULL, NULL)) {
            bsendmsg(ua, "%s\n", db_strerror(ua->db));
	 }
	 free_pool_memory(query);
	 db_list_sql_query(ua->jcr, ua->db, uar_list_temp, prtit, ua, 1, 0);

	 if (!db_sql_query(ua->db, uar_sel_jobid_temp, jobid_handler, (void *)ji)) {
            bsendmsg(ua, "%s\n", db_strerror(ua->db));
	 }
	 db_sql_query(ua->db, uar_del_temp, NULL, NULL);
	 db_sql_query(ua->db, uar_del_temp1, NULL, NULL);
	 break;
      case 5:
	 return 0;
      }
   }

   if (*ji->JobIds == 0) {
      bsendmsg(ua, _("No Jobs selected.\n"));
      return 0;
   }
   bsendmsg(ua, _("You have selected the following JobId: %s\n"), ji->JobIds);

   memset(&jr, 0, sizeof(JOB_DBR));

   ji->TotalFiles = 0;
   for (p=ji->JobIds; ; ) {
      int stat = next_jobid_from_list(&p, &JobId);
      if (stat < 0) {
         bsendmsg(ua, _("Invalid JobId in list.\n"));
	 return 0;
      }
      if (stat == 0) {
	 break;
      }
      jr.JobId = JobId;
      if (!db_get_job_record(ua->jcr, ua->db, &jr)) {
         bsendmsg(ua, _("Unable to get Job record. ERR=%s\n"), db_strerror(ua->db));
	 return 0;
      }
      ji->TotalFiles += jr.JobFiles;
   }
   return 1;
}

static int next_jobid_from_list(char **p, uint32_t *JobId)
{
   char jobid[30];
   int i;
   char *q = *p;

   jobid[0] = 0;
   for (i=0; i<(int)sizeof(jobid); i++) {
      if (*q == ',' || *q == 0) {
	 q++;
	 break;
      }
      jobid[i] = *q++;
      jobid[i+1] = 0;
   }
   if (jobid[0] == 0 || !is_a_number(jobid)) {
      return 0;
   }
   *p = q;
   *JobId = strtoul(jobid, NULL, 10);
   return 1;
}

/*
 * Callback handler make list of JobIds
 */
static int jobid_handler(void *ctx, int num_fields, char **row)
{
   JobIds *ji = (JobIds *)ctx;

   if (strlen(ji->JobIds)+strlen(row[0])+2 < sizeof(ji->JobIds)) {
      if (ji->JobIds[0] != 0) {
         strcat(ji->JobIds, ",");
      }
      strcat(ji->JobIds, row[0]);
   }

   return 0;
}


/*
 * Callback handler to pickup last Full backup JobId and ClientId
 */
static int last_full_handler(void *ctx, int num_fields, char **row)
{
   JobIds *ji = (JobIds *)ctx;

   ji->JobTDate = strtoll(row[1], NULL, 10);

   return 0;
}

/*
 * Callback handler build fileset prompt list
 */
static int fileset_handler(void *ctx, int num_fields, char **row)
{
   char prompt[MAX_NAME_LENGTH+200];

   snprintf(prompt, sizeof(prompt), "%s  %s  %s", row[0], row[1], row[2]);
   add_prompt((UAContext *)ctx, prompt);
   return 0;
}

/* Forward referenced commands */

static int markcmd(UAContext *ua, TREE_CTX *tree);
static int countcmd(UAContext *ua, TREE_CTX *tree);
static int findcmd(UAContext *ua, TREE_CTX *tree);
static int lscmd(UAContext *ua, TREE_CTX *tree);
static int dircmd(UAContext *ua, TREE_CTX *tree);
static int helpcmd(UAContext *ua, TREE_CTX *tree);
static int cdcmd(UAContext *ua, TREE_CTX *tree);
static int pwdcmd(UAContext *ua, TREE_CTX *tree);
static int unmarkcmd(UAContext *ua, TREE_CTX *tree);
static int quitcmd(UAContext *ua, TREE_CTX *tree);


struct cmdstruct { char *key; int (*func)(UAContext *ua, TREE_CTX *tree); char *help; }; 
static struct cmdstruct commands[] = {
 { N_("mark"),       markcmd,      _("mark file for restoration")},
 { N_("unmark"),     unmarkcmd,    _("unmark file for restoration")},
 { N_("cd"),         cdcmd,        _("change current directory")},
 { N_("pwd"),        pwdcmd,       _("print current working directory")},
 { N_("ls"),         lscmd,        _("list current directory")},    
 { N_("dir"),        dircmd,       _("list current directory")},    
 { N_("count"),      countcmd,     _("count marked files")},
 { N_("find"),       findcmd,      _("find files")},
 { N_("done"),       quitcmd,      _("leave file selection mode")},
 { N_("exit"),       quitcmd,      _("exit = done")},
 { N_("help"),       helpcmd,      _("print help")},
 { N_("?"),          helpcmd,      _("print help")},    
	     };
#define comsize (sizeof(commands)/sizeof(struct cmdstruct))


/*
 * Enter a prompt mode where the user can select/deselect
 *  files to be restored. This is sort of like a mini-shell
 *  that allows "cd", "pwd", "add", "rm", ...
 */
static void user_select_files(TREE_CTX *tree)
{
   char cwd[2000];

   bsendmsg(tree->ua, _( 
      "\nYou are now entering file selection mode where you add and\n"
      "remove files to be restored. All files are initially added.\n"
      "Enter \"done\" to leave this mode.\n\n"));
   /*
    * Enter interactive command handler allowing selection
    *  of individual files.
    */
   tree->node = (TREE_NODE *)tree->root;
   tree_getpath(tree->node, cwd, sizeof(cwd));
   bsendmsg(tree->ua, _("cwd is: %s\n"), cwd);
   for ( ;; ) {       
      int found, len, stat, i;
      if (!get_cmd(tree->ua, "$ ")) {
	 break;
      }
      parse_ua_args(tree->ua);
      if (tree->ua->argc == 0) {
	 return;
      }

      len = strlen(tree->ua->argk[0]);
      found = 0;
      stat = 0;
      for (i=0; i<(int)comsize; i++)	   /* search for command */
	 if (strncasecmp(tree->ua->argk[0],  _(commands[i].key), len) == 0) {
	    stat = (*commands[i].func)(tree->ua, tree);   /* go execute command */
	    found = 1;
	    break;
	 }
      if (!found) {
         bsendmsg(tree->ua, _("Illegal command. Enter \"done\" to exit.\n"));
	 continue;
      }
      if (!stat) {
	 break;
      }
   }
}

/*
 * Create new FileIndex entry for BSR 
 */
static RBSR_FINDEX *new_findex() 
{
   RBSR_FINDEX *fi = (RBSR_FINDEX *)bmalloc(sizeof(RBSR_FINDEX));
   memset(fi, 0, sizeof(RBSR_FINDEX));
   return fi;
}

/* Free all BSR FileIndex entries */
static void free_findex(RBSR_FINDEX *fi)
{
   if (fi) {
      free_findex(fi->next);
      free(fi);
   }
}

static void write_findex(UAContext *ua, RBSR_FINDEX *fi, FILE *fd) 
{
   if (fi) {
      if (fi->findex == fi->findex2) {
         fprintf(fd, "FileIndex=%d\n", fi->findex);
      } else {
         fprintf(fd, "FileIndex=%d-%d\n", fi->findex, fi->findex2);
      }
      write_findex(ua, fi->next, fd);
   }
}


static void print_findex(UAContext *ua, RBSR_FINDEX *fi)
{
   if (fi) {
      if (fi->findex == fi->findex2) {
         bsendmsg(ua, "FileIndex=%d\n", fi->findex);
      } else {
         bsendmsg(ua, "FileIndex=%d-%d\n", fi->findex, fi->findex2);
      }
      print_findex(ua, fi->next);
   }
}

/* Create a new bootstrap record */
static RBSR *new_bsr()
{
   RBSR *bsr = (RBSR *)bmalloc(sizeof(RBSR));
   memset(bsr, 0, sizeof(RBSR));
   return bsr;
}

/* Free the entire BSR */
static void free_bsr(RBSR *bsr)
{
   if (bsr) {
      free_findex(bsr->fi);
      free_bsr(bsr->next);
      if (bsr->VolParams) {
	 free(bsr->VolParams);
      }
      free(bsr);
   }
}

/*
 * Complete the BSR by filling in the VolumeName and
 *  VolSessionId and VolSessionTime using the JobId
 */
static int complete_bsr(UAContext *ua, RBSR *bsr)
{
   JOB_DBR jr;

   if (bsr) {
      memset(&jr, 0, sizeof(jr));
      jr.JobId = bsr->JobId;
      if (!db_get_job_record(ua->jcr, ua->db, &jr)) {
         bsendmsg(ua, _("Unable to get Job record. ERR=%s\n"), db_strerror(ua->db));
	 return 0;
      }
      bsr->VolSessionId = jr.VolSessionId;
      bsr->VolSessionTime = jr.VolSessionTime;
      if ((bsr->VolCount=db_get_job_volume_parameters(ua->jcr, ua->db, bsr->JobId, 
	   &(bsr->VolParams))) == 0) {
         bsendmsg(ua, _("Unable to get Job Volume Parameters. ERR=%s\n"), db_strerror(ua->db));
	 if (bsr->VolParams) {
	    free(bsr->VolParams);
	    bsr->VolParams = NULL;
	 }
	 return 0;
      }
      return complete_bsr(ua, bsr->next);
   }
   return 1;
}

/*
 * Write the bootstrap record to file
 */
static int write_bsr_file(UAContext *ua, RBSR *bsr)
{
   FILE *fd;
   POOLMEM *fname = get_pool_memory(PM_MESSAGE);
   int stat;
   RBSR *nbsr;

   Mmsg(&fname, "%s/restore.bsr", working_directory);
   fd = fopen(fname, "w+");
   if (!fd) {
      bsendmsg(ua, _("Unable to create bootstrap file %s. ERR=%s\n"), 
	 fname, strerror(errno));
      free_pool_memory(fname);
      return 0;
   }
   write_bsr(ua, bsr, fd);
   stat = !ferror(fd);
   fclose(fd);
   bsendmsg(ua, _("Bootstrap records written to %s\n"), fname);
   bsendmsg(ua, _("\nThe restore job will require the following Volumes:\n"));
   /* Create Unique list of Volumes using prompt list */
   start_prompt(ua, "");
   for (nbsr=bsr; nbsr; nbsr=nbsr->next) {
      for (int i=0; i < nbsr->VolCount; i++) {
	 add_prompt(ua, nbsr->VolParams[i].VolumeName);
      }
   }
   for (int i=0; i < ua->num_prompts; i++) {
      bsendmsg(ua, "   %s\n", ua->prompt[i]);
      free(ua->prompt[i]);
   }
   ua->num_prompts = 0;
   bsendmsg(ua, "\n");
   free_pool_memory(fname);
   return stat;
}

static void write_bsr(UAContext *ua, RBSR *bsr, FILE *fd)
{
   if (bsr) {
      for (int i=0; i < bsr->VolCount; i++) {
         fprintf(fd, "Volume=\"%s\"\n", bsr->VolParams[i].VolumeName);
         fprintf(fd, "VolSessionId=%u\n", bsr->VolSessionId);
         fprintf(fd, "VolSessionTime=%u\n", bsr->VolSessionTime);
         fprintf(fd, "VolFile=%u-%u\n", bsr->VolParams[i].StartFile, 
		 bsr->VolParams[i].EndFile);
         fprintf(fd, "VolBlock=%u-%u\n", bsr->VolParams[i].StartBlock,
		 bsr->VolParams[i].EndBlock);
	 write_findex(ua, bsr->fi, fd);
      }
      write_bsr(ua, bsr->next, fd);
   }
}

static void print_bsr(UAContext *ua, RBSR *bsr)
{
   if (bsr) {
      for (int i=0; i < bsr->VolCount; i++) {
         bsendmsg(ua, "Volume=\"%s\"\n", bsr->VolParams[i].VolumeName);
         bsendmsg(ua, "VolSessionId=%u\n", bsr->VolSessionId);
         bsendmsg(ua, "VolSessionTime=%u\n", bsr->VolSessionTime);
         bsendmsg(ua, "VolFile=%u-%u\n", bsr->VolParams[i].StartFile, 
		  bsr->VolParams[i].EndFile);
         bsendmsg(ua, "VolBlock=%u-%u\n", bsr->VolParams[i].StartBlock,
		  bsr->VolParams[i].EndBlock);
	 print_findex(ua, bsr->fi);
      }
      print_bsr(ua, bsr->next);
   }
}


/*
 * Add a FileIndex to the list of BootStrap records.
 *  Here we are only dealing with JobId's and the FileIndexes
 *  associated with those JobIds.
 */
static void add_findex(RBSR *bsr, uint32_t JobId, int32_t findex)
{
   RBSR *nbsr;
   RBSR_FINDEX *fi, *lfi;

   if (findex == 0) {
      return;			      /* probably a dummy directory */
   }
   
   if (!bsr->fi) {		      /* if no FI add one */
      /* This is the first FileIndex item in the chain */
      bsr->fi = new_findex();
      bsr->JobId = JobId;
      bsr->fi->findex = findex;
      bsr->fi->findex2 = findex;
      return;
   }
   /* Walk down list of bsrs until we find the JobId */
   if (bsr->JobId != JobId) {
      for (nbsr=bsr->next; nbsr; nbsr=nbsr->next) {
	 if (nbsr->JobId == JobId) {
	    bsr = nbsr;
	    break;
	 }
      }

      if (!nbsr) {		      /* Must add new JobId */
	 /* Add new JobId at end of chain */
	 for (nbsr=bsr; nbsr->next; nbsr=nbsr->next) 
	    {  }
	 nbsr->next = new_bsr();
	 nbsr->next->JobId = JobId;
	 nbsr->next->fi = new_findex();
	 nbsr->next->fi->findex = findex;
	 nbsr->next->fi->findex2 = findex;
	 return;
      }
   }

   /* 
    * At this point, bsr points to bsr containing JobId,
    *  and we are sure that there is at least one fi record.
    */
   lfi = fi = bsr->fi;
   /* Check if this findex is smaller than first item */
   if (findex < fi->findex) {
      if ((findex+1) == fi->findex) {
	 fi->findex = findex;	      /* extend down */
	 return;
      }
      fi = new_findex();	      /* yes, insert before first item */
      fi->findex = findex;
      fi->findex2 = findex;
      fi->next = lfi;
      bsr->fi = fi;
      return;
   }
   /* Walk down fi chain and find where to insert insert new FileIndex */
   for ( ; fi; fi=fi->next) {
      if (findex == (fi->findex2 + 1)) {  /* extend up */
	 RBSR_FINDEX *nfi;     
	 fi->findex2 = findex;
	 if (fi->next && ((findex+1) == fi->next->findex)) { 
	    nfi = fi->next;
	    fi->findex2 = nfi->findex2;
	    fi->next = nfi->next;
	    free(nfi);
	 }
	 return;
      }
      if (findex < fi->findex) {      /* add before */
	 if ((findex+1) == fi->findex) {
	    fi->findex = findex;
	    return;
	 }
	 break;
      }
      lfi = fi;
   }
   /* Add to last place found */
   fi = new_findex();
   fi->findex = findex;
   fi->findex2 = findex;
   fi->next = lfi->next;
   lfi->next = fi;
   return;
}

/*
 * This callback routine is responsible for inserting the
 *  items it gets into the directory tree. For each JobId selected
 *  this routine is called once for each file. We do not allow
 *  duplicate filenames, but instead keep the info from the most
 *  recent file entered (i.e. the JobIds are assumed to be sorted)
 */
static int insert_tree_handler(void *ctx, int num_fields, char **row)
{
   TREE_CTX *tree = (TREE_CTX *)ctx;
   char fname[2000];
   TREE_NODE *node, *new_node;
   int type;

   strip_trailing_junk(row[1]);
   if (*row[1] == 0) {
      if (*row[0] != '/') {           /* Must be Win32 directory */
	 type = TN_DIR_NLS;
      } else {
	 type = TN_DIR;
      }
   } else {
      type = TN_FILE;
   }
   sprintf(fname, "%s%s", row[0], row[1]);
   if (tree->avail_node) {
      node = tree->avail_node;
   } else {
      node = new_tree_node(tree->root, type);
      tree->avail_node = node;
   }
   Dmsg3(200, "FI=%d type=%d fname=%s\n", node->FileIndex, type, fname);
   new_node = insert_tree_node(fname, node, tree->root, NULL);
   /* Note, if node already exists, save new one for next time */
   if (new_node != node) {
      tree->avail_node = node;
   } else {
      tree->avail_node = NULL;
   }
   new_node->FileIndex = atoi(row[2]);
   new_node->JobId = atoi(row[3]);
   new_node->type = type;
   new_node->extract = 1;	      /* extract all by default */
   tree->cnt++;
   return 0;
}


/*
 * Set extract to value passed. We recursively walk
 *  down the tree setting all children if the 
 *  node is a directory.
 */
static void set_extract(UAContext *ua, TREE_NODE *node, TREE_CTX *tree, int value)
{
   TREE_NODE *n;
   FILE_DBR fdbr;
   struct stat statp;

   node->extract = value;
   /* For a non-file (i.e. directory), we see all the children */
   if (node->type != TN_FILE) {
      for (n=node->child; n; n=n->sibling) {
	 set_extract(ua, n, tree, value);
      }
   } else if (value) {
      char cwd[2000];
      /* Ordinary file, we get the full path, look up the
       * attributes, decode them, and if we are hard linked to
       * a file that was saved, we must load that file too.
       */
      tree_getpath(node, cwd, sizeof(cwd));
      fdbr.FileId = 0;
      fdbr.JobId = node->JobId;
      if (db_get_file_attributes_record(ua->jcr, ua->db, cwd, &fdbr)) {
	 uint32_t LinkFI;
	 decode_stat(fdbr.LStat, &statp, &LinkFI); /* decode stat pkt */
	 /*
	  * If we point to a hard linked file, traverse the tree to
	  * find that file, and mark it for restoration as well. It
	  * must have the Link we just obtained and the same JobId.
	  */
	 if (LinkFI) {
	    for (n=first_tree_node(tree->root); n; n=next_tree_node(n)) {
	       if (n->FileIndex == LinkFI && n->JobId == node->JobId) {
		  n->extract = 1;
		  break;
	       }
	    }
	 }
      }
   }
}

static int markcmd(UAContext *ua, TREE_CTX *tree)
{
   TREE_NODE *node;

   if (ua->argc < 2)
      return 1;
   if (!tree->node->child) {	 
      return 1;
   }
   for (node = tree->node->child; node; node=node->sibling) {
      if (fnmatch(ua->argk[1], node->fname, 0) == 0) {
	 set_extract(ua, node, tree, 1);
      }
   }
   return 1;
}

static int countcmd(UAContext *ua, TREE_CTX *tree)
{
   int total, extract;

   total = extract = 0;
   for (TREE_NODE *node=first_tree_node(tree->root); node; node=next_tree_node(node)) {
      if (node->type != TN_NEWDIR) {
	 total++;
	 if (node->extract) {
	    extract++;
	 }
      }
   }
   bsendmsg(ua, "%d total files. %d marked for restoration.\n", total, extract);
   return 1;
}

static int findcmd(UAContext *ua, TREE_CTX *tree)
{
   char cwd[2000];

   if (ua->argc == 1) {
      bsendmsg(ua, _("No file specification given.\n"));
      return 0;
   }
   
   for (int i=1; i < ua->argc; i++) {
      for (TREE_NODE *node=first_tree_node(tree->root); node; node=next_tree_node(node)) {
	 if (fnmatch(ua->argk[i], node->fname, 0) == 0) {
	    tree_getpath(node, cwd, sizeof(cwd));
            bsendmsg(ua, "%s%s\n", node->extract?"*":"", cwd);
	 }
      }
   }
   return 1;
}



static int lscmd(UAContext *ua, TREE_CTX *tree)
{
   TREE_NODE *node;

   if (!tree->node->child) {	 
      return 1;
   }
   for (node = tree->node->child; node; node=node->sibling) {
      if (ua->argc == 1 || fnmatch(ua->argk[1], node->fname, 0) == 0) {
         bsendmsg(ua, "%s%s%s\n", node->extract?"*":"", node->fname,
            (node->type==TN_DIR||node->type==TN_NEWDIR)?"/":"");
      }
   }
   return 1;
}

extern char *getuser(uid_t uid);
extern char *getgroup(gid_t gid);

/*
 * This is actually the long form used for "dir"
 */
static void ls_output(char *buf, char *fname, int extract, struct stat *statp)
{
   char *p, *f;
   char ec1[30];
   int n;

   p = encode_mode(statp->st_mode, buf);
   n = sprintf(p, "  %2d ", (uint32_t)statp->st_nlink);
   p += n;
   n = sprintf(p, "%-8.8s %-8.8s", getuser(statp->st_uid), getgroup(statp->st_gid));
   p += n;
   n = sprintf(p, "%8.8s  ", edit_uint64(statp->st_size, ec1));
   p += n;
   p = encode_time(statp->st_ctime, p);
   *p++ = ' ';
   if (extract) {
      *p++ = '*';
   } else {
      *p++ = ' ';
   }
   for (f=fname; *f; )
      *p++ = *f++;
   *p = 0;
}


/*
 * Like ls command, but give more detail on each file
 */
static int dircmd(UAContext *ua, TREE_CTX *tree)
{
   TREE_NODE *node;
   FILE_DBR fdbr;
   struct stat statp;
   char buf[1000];
   char cwd[1100];

   if (!tree->node->child) {	 
      return 1;
   }
   for (node = tree->node->child; node; node=node->sibling) {
      if (ua->argc == 1 || fnmatch(ua->argk[1], node->fname, 0) == 0) {
	 tree_getpath(node, cwd, sizeof(cwd));
	 fdbr.FileId = 0;
	 fdbr.JobId = node->JobId;
	 if (db_get_file_attributes_record(ua->jcr, ua->db, cwd, &fdbr)) {
	    uint32_t LinkFI;
	    decode_stat(fdbr.LStat, &statp, &LinkFI); /* decode stat pkt */
	    ls_output(buf, cwd, node->extract, &statp);
            bsendmsg(ua, "%s\n", buf);
	 } else {
	    /* Something went wrong getting attributes -- print name */
            bsendmsg(ua, "%s%s%s\n", node->extract?"*":"", node->fname,
               (node->type==TN_DIR||node->type==TN_NEWDIR)?"/":"");
	 }
      }
   }
   return 1;
}


static int helpcmd(UAContext *ua, TREE_CTX *tree) 
{
   unsigned int i;

/* usage(); */
   bsendmsg(ua, _("  Command    Description\n  =======    ===========\n"));
   for (i=0; i<comsize; i++) {
      bsendmsg(ua, _("  %-10s %s\n"), _(commands[i].key), _(commands[i].help));
   }
   bsendmsg(ua, "\n");
   return 1;
}

/*
 * Change directories.	Note, if the user specifies x: and it fails,
 *   we assume it is a Win32 absolute cd rather than relative and
 *   try a second time with /x: ...  Win32 kludge.
 */
static int cdcmd(UAContext *ua, TREE_CTX *tree) 
{
   TREE_NODE *node;
   char cwd[2000];

   if (ua->argc != 2) {
      return 1;
   }
   node = tree_cwd(ua->argk[1], tree->root, tree->node);
   if (!node) {
      /* Try once more if Win32 drive -- make absolute */
      if (ua->argk[1][1] == ':') {  /* win32 drive */
         strcpy(cwd, "/");
	 strcat(cwd, ua->argk[1]);
	 node = tree_cwd(cwd, tree->root, tree->node);
      }
      if (!node) {
         bsendmsg(ua, _("Invalid path given.\n"));
      } else {
	 tree->node = node;
      }
   } else {
      tree->node = node;
   }
   tree_getpath(tree->node, cwd, sizeof(cwd));
   bsendmsg(ua, _("cwd is: %s\n"), cwd);
   return 1;
}

static int pwdcmd(UAContext *ua, TREE_CTX *tree) 
{
   char cwd[2000];
   tree_getpath(tree->node, cwd, sizeof(cwd));
   bsendmsg(ua, _("cwd is: %s\n"), cwd);
   return 1;
}


static int unmarkcmd(UAContext *ua, TREE_CTX *tree)
{
   TREE_NODE *node;

   if (ua->argc < 2)
      return 1;
   if (!tree->node->child) {	 
      return 1;
   }
   for (node = tree->node->child; node; node=node->sibling) {
      if (fnmatch(ua->argk[1], node->fname, 0) == 0) {
	 set_extract(ua, node, tree, 0);
      }
   }
   return 1;
}

static int quitcmd(UAContext *ua, TREE_CTX *tree) 
{
   return 0;
}


/*
 * Called here with each name to be added to the list. The name is
 *   added to the list if it is not already in the list.
 */
static int unique_name_list_handler(void *ctx, int num_fields, char **row)
{
   NAME_LIST *name = (NAME_LIST *)ctx;

   if (name->num_ids == MAX_ID_LIST_LEN) {  
      return 1;
   }
   if (name->num_ids == name->max_ids) {
      if (name->max_ids == 0) {
	 name->max_ids = 1000;
	 name->name = (char **)bmalloc(sizeof(char *) * name->max_ids);
      } else {
	 name->max_ids = (name->max_ids * 3) / 2;
	 name->name = (char **)brealloc(name->name, sizeof(char *) * name->max_ids);
      }
   }
   for (int i=0; i<name->num_ids; i++) {
      if (strcmp(name->name[i], row[0]) == 0) {
	 return 0;		      /* already in list, return */
      }
   }
   /* Add new name to list */
   name->name[name->num_ids++] = bstrdup(row[0]);
   return 0;
}


/*
 * Print names in the list
 */
static void print_name_list(UAContext *ua, NAME_LIST *name_list)
{ 
   int i;

   for (i=0; i < name_list->num_ids; i++) {
      bsendmsg(ua, "%s\n", name_list->name[i]);
   }
}


/*
 * Free names in the list
 */
static void free_name_list(NAME_LIST *name_list)
{ 
   int i;

   for (i=0; i < name_list->num_ids; i++) {
      free(name_list->name[i]);
   }
   if (name_list->name) {
      free(name_list->name);
   }
   name_list->max_ids = 0;
   name_list->num_ids = 0;
}

static void get_storage_from_mediatype(UAContext *ua, NAME_LIST *name_list, JobIds *ji)
{
   char name[MAX_NAME_LENGTH];
   STORE *store = NULL;

   if (name_list->num_ids > 1) {
      bsendmsg(ua, _("Warning, the JobIds that you selected refer to more than one MediaType.\n"
         "Restore is not possible. The MediaTypes used are:\n"));
      print_name_list(ua, name_list);
      ji->store = select_storage_resource(ua);
      return;
   }

   if (name_list->num_ids == 0) {
      bsendmsg(ua, _("No MediaType found for your JobIds.\n"));
      ji->store = select_storage_resource(ua);
      return;
   }

   start_prompt(ua, _("The defined Storage resources are:\n"));
   LockRes();
   while ((store = (STORE *)GetNextRes(R_STORAGE, (RES *)store))) {
      if (strcmp(store->media_type, name_list->name[0]) == 0) {
	 add_prompt(ua, store->hdr.name);
      }
   }
   UnlockRes();
   do_prompt(ua, _("Select Storage resource"), name, sizeof(name));
   ji->store = (STORE *)GetResWithName(R_STORAGE, name);
   if (!ji->store) {
      bsendmsg(ua, _("\nWarning. Unable to find Storage resource for\n"
         "MediaType %s, needed by the Jobs you selected.\n"
         "You will be allowed to select a Storage device later.\n"),
	 name_list->name[0]); 
   }
}
