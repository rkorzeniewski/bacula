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
   Copyright (C) 2002 Kern Sibbald and John Walker

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
#include "ua.h"
#include <fnmatch.h>



/* Imported functions */
extern int runcmd(UAContext *ua, char *cmd);

/* Imported variables */
extern char *uar_list_jobs;
extern char *uar_file;
extern char *uar_sel_files;
extern char *uar_del_temp;
extern char *uar_del_temp1;
extern char *uar_create_temp;
extern char *uar_create_temp1;
extern char *uar_last_full;
extern char *uar_full;
extern char *uar_inc;
extern char *uar_list_temp;
extern char *uar_sel_jobid_temp;
extern char *uar_sel_all_temp1;

/* Context for insert_tree_handler() */
typedef struct s_tree_ctx {
   TREE_ROOT *root;		      /* root */
   TREE_NODE *node;		      /* current node */
   TREE_NODE *avail_node;	      /* unused node last insert */
   int cnt;			      /* count for user feedback */
   UAContext *ua;
} TREE_CTX;

typedef struct s_jobids {
   btime_t JobTDate;
   uint32_t ClientId;
   uint32_t TotalFiles;
   char JobIds[200];
   CLIENT *client;
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
   char *VolumeName;		      /* Volume name */
   RBSR_FINDEX *fi;		      /* File indexes this JobId */
} RBSR;

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

   if (!open_db(ua)) {
      return 0;
   }

   memset(&tree, 0, sizeof(TREE_CTX));
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


   if (!user_select_jobids(ua, &ji)) {
      return 0;
   }

   /* 
    * Build the directory tree	
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
      Mmsg(&query, uar_sel_files, JobId);
      if (!db_sql_query(ua->db, query, insert_tree_handler, (void *)&tree)) {
         bsendmsg(ua, "%s", db_strerror(ua->db));
      }
   }
   bsendmsg(ua, "\n");
   free_pool_memory(query);

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
      }
   }

   free_tree(tree.root);	      /* free the directory tree */

   if (bsr->JobId) {
      complete_bsr(ua, bsr);	      /* find Vol, SessId, SessTime from JobIds */
//    print_bsr(ua, bsr);
      write_bsr_file(ua, bsr);
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

   if (ji.client) {
      Mmsg(&ua->cmd, "run job=%s client=%s bootstrap=%s/restore.bsr",
	 job->hdr.name, ji.client->hdr.name, working_directory);
   } else {
      Mmsg(&ua->cmd, "run job=%s bootstrap=%s/restore.bsr",
	 job->hdr.name, working_directory);
   }

   Dmsg1(000, "Submitting: %s\n", ua->cmd);
   
   parse_command_args(ua);
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
   char *p;
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
      switch (do_prompt(ua, "Select item: ", NULL)) {
      case -1:			      /* error */
	 return 0;
      case 0:			      /* list last 20 Jobs run */
	 db_list_sql_query(ua->db, uar_list_jobs, prtit, ua, 1);
	 done = 0;
	 break;
      case 1:			      /* list where a file is saved */
         if (!get_cmd(ua, _("Enter Filename: "))) {
	    return 0;
	 }
	 query = get_pool_memory(PM_MESSAGE);
	 Mmsg(&query, uar_file, ua->cmd);
	 db_list_sql_query(ua->db, query, prtit, ua, 1);
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
	 db_list_sql_query(ua->db, ua->cmd, prtit, ua, 1);
	 done = 0;
	 break;
      case 4:			      /* Select the most recent backups */
	 db_sql_query(ua->db, uar_del_temp, NULL, NULL);
	 db_sql_query(ua->db, uar_del_temp1, NULL, NULL);
	 if (!db_sql_query(ua->db, uar_create_temp, NULL, NULL)) {
            bsendmsg(ua, "%s\n", db_strerror(ua->db));
	 }
	 if (!db_sql_query(ua->db, uar_create_temp1, NULL, NULL)) {
            bsendmsg(ua, "%s\n", db_strerror(ua->db));
	 }
	 if (!(ji->client = get_client_resource(ua))) {
	    return 0;
	 }
	 query = get_pool_memory(PM_MESSAGE);
	 Mmsg(&query, uar_last_full, ji->client->hdr.name);
	 /* Find JobId of full Backup of system */
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
	 Mmsg(&query, uar_inc, (uint32_t)ji->JobTDate, ji->ClientId);
	 if (!db_sql_query(ua->db, query, NULL, NULL)) {
            bsendmsg(ua, "%s\n", db_strerror(ua->db));
	 }
	 free_pool_memory(query);
	 db_list_sql_query(ua->db, uar_list_temp, prtit, ua, 1);

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
      if (!db_get_job_record(ua->db, &jr)) {
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

   ji->JobTDate = atoi(row[1]);
   ji->ClientId = atoi(row[2]);

   return 0;
}




/* Forward referenced commands */

static int markcmd(UAContext *ua, TREE_CTX *tree);
static int countcmd(UAContext *ua, TREE_CTX *tree);
static int findcmd(UAContext *ua, TREE_CTX *tree);
static int lscmd(UAContext *ua, TREE_CTX *tree);
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
 { N_("dir"),        lscmd,        _("list current directory")},    
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
      "You are now entering file selection mode where you add and\n"
      "remove files to be restored. All files are initially added.\n"
      "Enter done to leave this mode.\n\n"));
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
      parse_command_args(tree->ua);
      if (tree->ua->argc == 0) {
	 return;
      }

      len = strlen(tree->ua->argk[0]);
      found = 0;
      for (i=0; i<(int)comsize; i++)	   /* search for command */
	 if (strncasecmp(tree->ua->argk[0],  _(commands[i].key), len) == 0) {
	    stat = (*commands[i].func)(tree->ua, tree);   /* go execute command */
	    found = 1;
	    break;
	 }
      if (!found) {
         bsendmsg(tree->ua, _("Illegal command. Enter \"done\" to exit.\n"));
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
   RBSR_FINDEX *fi = (RBSR_FINDEX *)malloc(sizeof(RBSR_FINDEX));
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
   RBSR *bsr = (RBSR *)malloc(sizeof(RBSR));
   memset(bsr, 0, sizeof(RBSR));
   return bsr;
}

/* Free the entire BSR */
static void free_bsr(RBSR *bsr)
{
   if (bsr) {
      free_findex(bsr->fi);
      free_bsr(bsr->next);
      if (bsr->VolumeName) {
	 free(bsr->VolumeName);
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
   POOLMEM *VolumeNames;

   if (bsr) {
      VolumeNames = get_pool_memory(PM_MESSAGE);
      memset(&jr, 0, sizeof(jr));
      jr.JobId = bsr->JobId;
      if (!db_get_job_record(ua->db, &jr)) {
         bsendmsg(ua, _("Unable to get Job record. ERR=%s\n"), db_strerror(ua->db));
	 return 0;
      }
      bsr->VolSessionId = jr.VolSessionId;
      bsr->VolSessionTime = jr.VolSessionTime;
      if (!db_get_job_volume_names(ua->db, bsr->JobId, &VolumeNames)) {
         bsendmsg(ua, _("Unable to get Job Volumes. ERR=%s\n"), db_strerror(ua->db));
	 free_pool_memory(VolumeNames);
	 return 0;
      }
      bsr->VolumeName = bstrdup(VolumeNames);
      free_pool_memory(VolumeNames);
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
   free_pool_memory(fname);
   return stat;
}

static void write_bsr(UAContext *ua, RBSR *bsr, FILE *fd)
{
   if (bsr) {
      if (bsr->VolumeName) {
         fprintf(fd, "Volume=\"%s\"\n", bsr->VolumeName);
      }
      fprintf(fd, "VolSessionId=%u\n", bsr->VolSessionId);
      fprintf(fd, "VolSessionTime=%u\n", bsr->VolSessionTime);
      write_findex(ua, bsr->fi, fd);
      write_bsr(ua, bsr->next, fd);
   }
}

static void print_bsr(UAContext *ua, RBSR *bsr)
{
   if (bsr) {
      if (bsr->VolumeName) {
         bsendmsg(ua, "Volume=\"%s\"\n", bsr->VolumeName);
      }
      bsendmsg(ua, "VolSessionId=%u\n", bsr->VolSessionId);
      bsendmsg(ua, "VolSessionTime=%u\n", bsr->VolSessionTime);
      print_findex(ua, bsr->fi);
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
      type = TN_DIR;
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
   Dmsg2(400, "FI=%d fname=%s\n", node->FileIndex, fname);
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
static void set_extract(TREE_NODE *node, int value)
{
   TREE_NODE *n;

   node->extract = value;
   if (node->type != TN_FILE) {
      for (n=node->child; n; n=n->sibling) {
	 set_extract(n, value);
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
	 set_extract(node, 1);
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

static int cdcmd(UAContext *ua, TREE_CTX *tree) 
{
   TREE_NODE *node;
   char cwd[2000];

   if (ua->argc != 2) {
      return 1;
   }
   node = tree_cwd(ua->argk[1], tree->root, tree->node);
   if (!node) {
      bsendmsg(ua, _("Invalid path given.\n"));
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
	 set_extract(node, 0);
      }
   }
   return 1;
}

static int quitcmd(UAContext *ua, TREE_CTX *tree) 
{
   return 0;
}
