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
extern char *uar_list_jobs;
extern char *uar_file;
extern char *uar_sel_files;

/* Context for insert_tree_handler() */
typedef struct s_tree_ctx {
   TREE_ROOT *root;		      /* root */
   TREE_NODE *node;		      /* current node */
   TREE_NODE *avail_node;	      /* unused node last insert */
   int cnt;			      /* count for user feedback */
   UAContext *ua;
} TREE_CTX;


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
static void print_bsr(UAContext *ua, RBSR *bsr);
static int  complete_bsr(UAContext *ua, RBSR *bsr);
static int insert_tree_handler(void *ctx, int num_fields, char **row);
static void add_findex(RBSR *bsr, uint32_t JobId, int32_t findex);
static void user_select_files(TREE_CTX *tree);



/*
 *   Restore files
 *
 */
int restorecmd(UAContext *ua, char *cmd)
{
   POOLMEM *query;
   int JobId, done = 0;
   TREE_CTX tree;
   RBSR *bsr;
   char *nofname = "";
   JOB_DBR jr;
   char *list[] = { 
      "List last Jobs run",
      "Enter list of JobIds",
      "Enter SQL list command", 
      "Select a File",
      "Cancel",
      NULL };

   if (!open_db(ua)) {
      return 0;
   }

   memset(&tree, 0, sizeof(TREE_CTX));

   for ( ; !done; ) {
      start_prompt(ua, _("To narrow down the restore, you have the following choices:\n"));
      for (int i=0; list[i]; i++) {
	 add_prompt(ua, list[i]);
      }
      done = 1;
      switch (do_prompt(ua, "Select item: ", NULL)) {
      case -1:
	 return 0;
      case 0:
	 db_list_sql_query(ua->db, uar_list_jobs, prtit, ua, 1);
         if (!get_cmd(ua, _("Enter JobId to select files for restore: "))) {
	    return 0;
	 }
	 if (!is_a_number(ua->cmd)) {
           bsendmsg(ua, _("Bad JobId entered.\n"));
	   return 0;
	 }
	 JobId = atoi(ua->cmd);
	 break;

      case 1:
         if (!get_cmd(ua, _("Enter JobIds: "))) {
	    return 0;
	 }
	 JobId = atoi(ua->cmd);
	 break;
      case 2:
         if (!get_cmd(ua, _("Enter SQL list command: "))) {
	    return 0;
	 }
	 db_list_sql_query(ua->db, ua->cmd, prtit, ua, 1);
	 done = 0;
	 break;
      case 3:
         if (!get_cmd(ua, _("Enter Filename: "))) {
	    return 0;
	 }
	 query = get_pool_memory(PM_MESSAGE);
	 Mmsg(&query, uar_file, ua->cmd);
	 db_list_sql_query(ua->db, query, prtit, ua, 1);
	 free_pool_memory(query);
         if (!get_cmd(ua, _("Enter JobId to select files for restore: "))) {
	    return 0;
	 }
	 if (!is_a_number(ua->cmd)) {
           bsendmsg(ua, _("Bad JobId entered.\n"));
	   return 0;
	 }
	 JobId = atoi(ua->cmd);
	 break;
      case 4:
	 return 0;
      }
   }

   memset(&jr, 0, sizeof(JOB_DBR));
   jr.JobId = JobId;
   if (!db_get_job_record(ua->db, &jr)) {
      bsendmsg(ua, _("Unable to get Job record. ERR=%s\n"), db_strerror(ua->db));
      return 0;
   }

   /* 
    * Build the directory tree	
    */
   bsendmsg(ua, _("Building directory tree of backed up files ...\n"));
   memset(&tree, 0, sizeof(tree));
   tree.root = new_tree(jr.JobFiles);
   tree.root->fname = nofname;
   tree.ua = ua;
   query = get_pool_memory(PM_MESSAGE);
   Mmsg(&query, uar_sel_files, JobId);
   if (!db_sql_query(ua->db, query, insert_tree_handler, (void *)&tree)) {
      bsendmsg(ua, "%s", db_strerror(ua->db));
   }
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
      complete_bsr(ua, bsr);
      print_bsr(ua, bsr);
   } else {
      bsendmsg(ua, _("No files selected to restore.\n"));
   }
   free_bsr(bsr);

   bsendmsg(ua, _("Restore command done.\n"));
   return 1;
}



/* Forward referenced commands */

static int addcmd(UAContext *ua, TREE_CTX *tree);
static int lscmd(UAContext *ua, TREE_CTX *tree);
static int helpcmd(UAContext *ua, TREE_CTX *tree);
static int cdcmd(UAContext *ua, TREE_CTX *tree);
static int pwdcmd(UAContext *ua, TREE_CTX *tree);
static int rmcmd(UAContext *ua, TREE_CTX *tree);
static int quitcmd(UAContext *ua, TREE_CTX *tree);


struct cmdstruct { char *key; int (*func)(UAContext *ua, TREE_CTX *tree); char *help; }; 
static struct cmdstruct commands[] = {
 { N_("add"),        addcmd,       _("add file")},
 { N_("ls"),         lscmd,        _("list current directory")},    
 { N_("dir"),        lscmd,        _("list current directory")},    
 { N_("help"),       helpcmd,      _("print help")},
 { N_("cd"),         cdcmd,        _("change directory")},
 { N_("pwd"),        pwdcmd,       _("print directory")},
 { N_("rm"),         rmcmd,        _("remove a file")},
 { N_("remove"),     rmcmd,        _("remove a file")},
 { N_("done"),       quitcmd,      _("quit")},
 { N_("exit"),       quitcmd,      _("exit = quit")},
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
         bsendmsg(tree->ua, _("Illegal command\n"));
      }
      if (!stat) {
	 break;
      }
   }
}


static RBSR_FINDEX *new_findex() 
{
   RBSR_FINDEX *fi = (RBSR_FINDEX *)malloc(sizeof(RBSR_FINDEX));
   memset(fi, 0, sizeof(RBSR_FINDEX));
   return fi;
}

static void free_findex(RBSR_FINDEX *fi)
{
   if (fi) {
      free_findex(fi->next);
      free(fi);
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

static RBSR *new_bsr()
{
   RBSR *bsr = (RBSR *)malloc(sizeof(RBSR));
   memset(bsr, 0, sizeof(RBSR));
   return bsr;
}

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
 *  VolSessionId and VolSessionTime
 */
static int complete_bsr(UAContext *ua, RBSR *bsr)
{
   JOB_DBR jr;
   char VolumeNames[1000];	      /* ****FIXME**** */

   if (bsr) {
      memset(&jr, 0, sizeof(jr));
      jr.JobId = bsr->JobId;
      if (!db_get_job_record(ua->db, &jr)) {
         bsendmsg(ua, _("Unable to get Job record. ERR=%s\n"), db_strerror(ua->db));
	 return 0;
      }
      bsr->VolSessionId = jr.VolSessionId;
      bsr->VolSessionTime = jr.VolSessionTime;
      if (!db_get_job_volume_names(ua->db, bsr->JobId, VolumeNames)) {
         bsendmsg(ua, _("Unable to get Job Volumes. ERR=%s\n"), db_strerror(ua->db));
	 return 0;
      }
      bsr->VolumeName = bstrdup(VolumeNames);
      return complete_bsr(ua, bsr->next);
   }
   return 1;
}


static void print_bsr(UAContext *ua, RBSR *bsr)
{
   if (bsr) {
      if (bsr->VolumeName) {
         bsendmsg(ua, "VolumeName=%s\n", bsr->VolumeName);
      }
//    bsendmsg(ua, "JobId=%u\n", bsr->JobId);
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
            Dmsg1(400, "Coallase %d\n", findex);
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
   if (((tree->cnt) % 10000) == 0) {
      bsendmsg(tree->ua, "%d ", tree->cnt);
   }
   tree->cnt++;
   return 0;
}


/*
 * Set extract to value passed. We recursively walk
 *  down the tree setting all children.
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

static int addcmd(UAContext *ua, TREE_CTX *tree)
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
   char cwd[2000];
   if (ua->argc != 2) {
      return 1;
   }
   tree->node = tree_cwd(ua->argk[1], tree->root, tree->node);
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


static int rmcmd(UAContext *ua, TREE_CTX *tree)
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
