/*
 *
 *   Bacula Director -- User Agent Database File tree for Restore
 *	command. This file interacts with the user implementing the
 *	UA tree commands.
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


/* Forward referenced commands */

static int markcmd(UAContext *ua, TREE_CTX *tree);
static int countcmd(UAContext *ua, TREE_CTX *tree);
static int findcmd(UAContext *ua, TREE_CTX *tree);
static int lscmd(UAContext *ua, TREE_CTX *tree);
static int lsmark(UAContext *ua, TREE_CTX *tree);
static int dircmd(UAContext *ua, TREE_CTX *tree);
static int estimatecmd(UAContext *ua, TREE_CTX *tree);
static int helpcmd(UAContext *ua, TREE_CTX *tree);
static int cdcmd(UAContext *ua, TREE_CTX *tree);
static int pwdcmd(UAContext *ua, TREE_CTX *tree);
static int unmarkcmd(UAContext *ua, TREE_CTX *tree);
static int quitcmd(UAContext *ua, TREE_CTX *tree);


struct cmdstruct { char *key; int (*func)(UAContext *ua, TREE_CTX *tree); char *help; }; 
static struct cmdstruct commands[] = {
 { N_("cd"),         cdcmd,        _("change current directory")},
 { N_("count"),      countcmd,     _("count marked files")},
 { N_("dir"),        dircmd,       _("list current directory")},    
 { N_("done"),       quitcmd,      _("leave file selection mode")},
 { N_("estimate"),   estimatecmd,  _("estimate restore size")},
 { N_("exit"),       quitcmd,      _("exit = done")},
 { N_("find"),       findcmd,      _("find files")},
 { N_("help"),       helpcmd,      _("print help")},
 { N_("lsmark"),     lsmark,       _("list the marked files")},    
 { N_("ls"),         lscmd,        _("list current directory")},    
 { N_("mark"),       markcmd,      _("mark file to be restored")},
 { N_("pwd"),        pwdcmd,       _("print current working directory")},
 { N_("unmark"),     unmarkcmd,    _("unmark file to be restored")},
 { N_("?"),          helpcmd,      _("print help")},    
	     };
#define comsize (sizeof(commands)/sizeof(struct cmdstruct))


/*
 * Enter a prompt mode where the user can select/deselect
 *  files to be restored. This is sort of like a mini-shell
 *  that allows "cd", "pwd", "add", "rm", ...
 */
void user_select_files_from_tree(TREE_CTX *tree)
{
   char cwd[2000];
   /* Get a new context so we don't destroy restore command args */
   UAContext *ua = new_ua_context(tree->ua->jcr);
   ua->UA_sock = tree->ua->UA_sock;   /* patch in UA socket */

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
      if (!get_cmd(ua, "$ ")) {
	 break;
      }
      parse_ua_args(ua);
      if (ua->argc == 0) {
	 break;
      }

      len = strlen(ua->argk[0]);
      found = 0;
      stat = 0;
      for (i=0; i<(int)comsize; i++)	   /* search for command */
	 if (strncasecmp(ua->argk[0],  _(commands[i].key), len) == 0) {
	    stat = (*commands[i].func)(ua, tree);   /* go execute command */
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
   ua->UA_sock = NULL;                /* don't release restore socket */
   free_ua_context(ua); 	      /* get rid of temp UA context */
}


/*
 * This callback routine is responsible for inserting the
 *  items it gets into the directory tree. For each JobId selected
 *  this routine is called once for each file. We do not allow
 *  duplicate filenames, but instead keep the info from the most
 *  recent file entered (i.e. the JobIds are assumed to be sorted)
 */
int insert_tree_handler(void *ctx, int num_fields, char **row)
{
   TREE_CTX *tree = (TREE_CTX *)ctx;
   char fname[5000];
   TREE_NODE *node, *new_node;
   int type;

   strip_trailing_junk(row[1]);
   if (*row[1] == 0) {		      /* no filename => directory */
      if (*row[0] != '/') {           /* Must be Win32 directory */
	 type = TN_DIR_NLS;
      } else {
	 type = TN_DIR;
      }
   } else {
      type = TN_FILE;
   }
   bsnprintf(fname, sizeof(fname), "%s%s", row[0], row[1]);
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
   new_node->extract = true;	      /* extract all by default */
   tree->cnt++;
   return 0;
}


/*
 * Set extract to value passed. We recursively walk
 *  down the tree setting all children if the 
 *  node is a directory.
 */
static int set_extract(UAContext *ua, TREE_NODE *node, TREE_CTX *tree, bool extract)
{
   TREE_NODE *n;
   FILE_DBR fdbr;
   struct stat statp;
   int count = 0;

   node->extract = extract;
   if (node->type != TN_NEWDIR) {
      count++;
   }
   /* For a non-file (i.e. directory), we see all the children */
   if (node->type != TN_FILE) {
      for (n=node->child; n; n=n->sibling) {
	 count += set_extract(ua, n, tree, extract);
      }
   } else if (extract) {
      char cwd[2000];
      /* Ordinary file, we get the full path, look up the
       * attributes, decode them, and if we are hard linked to
       * a file that was saved, we must load that file too.
       */
      tree_getpath(node, cwd, sizeof(cwd));
      fdbr.FileId = 0;
      fdbr.JobId = node->JobId;
      if (db_get_file_attributes_record(ua->jcr, ua->db, cwd, NULL, &fdbr)) {
	 int32_t LinkFI;
	 decode_stat(fdbr.LStat, &statp, &LinkFI); /* decode stat pkt */
	 /*
	  * If we point to a hard linked file, traverse the tree to
	  * find that file, and mark it to be restored as well. It
	  * must have the Link we just obtained and the same JobId.
	  */
	 if (LinkFI) {
	    for (n=first_tree_node(tree->root); n; n=next_tree_node(n)) {
	       if (n->FileIndex == LinkFI && n->JobId == node->JobId) {
		  n->extract = true;
		  break;
	       }
	    }
	 }
      }
   }
   return count;
}

static int markcmd(UAContext *ua, TREE_CTX *tree)
{
   TREE_NODE *node;
   int count = 0;

   if (ua->argc < 2 || !tree->node->child) {
      bsendmsg(ua, _("No files marked.\n"));
      return 1;
   }
   for (int i=1; i < ua->argc; i++) {
      for (node = tree->node->child; node; node=node->sibling) {
	 if (fnmatch(ua->argk[i], node->fname, 0) == 0) {
	    count += set_extract(ua, node, tree, true);
	 }
      }
   }
   if (count == 0) {
      bsendmsg(ua, _("No files marked.\n"));
   } else {
      bsendmsg(ua, _("%d file%s marked.\n"), count, count==0?"":"s");
   }
   return 1;
}

static int countcmd(UAContext *ua, TREE_CTX *tree)
{
   int total, num_extract;

   total = num_extract = 0;
   for (TREE_NODE *node=first_tree_node(tree->root); node; node=next_tree_node(node)) {
      if (node->type != TN_NEWDIR) {
	 total++;
	 if (node->extract) {
	    num_extract++;
	 }
      }
   }
   bsendmsg(ua, "%d total files. %d marked to be restored.\n", total, num_extract);
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

/*
 * Ls command that lists only the marked files
 */
static int lsmark(UAContext *ua, TREE_CTX *tree)
{
   TREE_NODE *node;

   if (!tree->node->child) {	 
      return 1;
   }
   for (node = tree->node->child; node; node=node->sibling) {
      if (ua->argc == 1 || fnmatch(ua->argk[1], node->fname, 0) == 0 &&
	  node->extract) {
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
static void ls_output(char *buf, char *fname, bool extract, struct stat *statp)
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
	 if (db_get_file_attributes_record(ua->jcr, ua->db, cwd, NULL, &fdbr)) {
	    int32_t LinkFI;
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


static int estimatecmd(UAContext *ua, TREE_CTX *tree)
{
   int total, num_extract;
   uint64_t total_bytes = 0;
   FILE_DBR fdbr;
   struct stat statp;
   char cwd[1100];
   char ec1[50];

   total = num_extract = 0;
   for (TREE_NODE *node=first_tree_node(tree->root); node; node=next_tree_node(node)) {
      if (node->type != TN_NEWDIR) {
	 total++;
	 /* If regular file, get size */
	 if (node->extract && node->type == TN_FILE) {
	    num_extract++;
	    tree_getpath(node, cwd, sizeof(cwd));
	    fdbr.FileId = 0;
	    fdbr.JobId = node->JobId;
	    if (db_get_file_attributes_record(ua->jcr, ua->db, cwd, NULL, &fdbr)) {
	       int32_t LinkFI;
	       decode_stat(fdbr.LStat, &statp, &LinkFI); /* decode stat pkt */
	       if (S_ISREG(statp.st_mode) && statp.st_size > 0) {
		  total_bytes += statp.st_size;
	       }
	    }
	 /* Directory, count only */
	 } else if (node->extract) {
	    num_extract++;
	 }
      }
   }
   bsendmsg(ua, "%d total files; %d marked to be restored; %s bytes.\n", 
	    total, num_extract, edit_uint64_with_commas(total_bytes, ec1));
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
         bstrncpy(cwd, "/", sizeof(cwd));
	 bstrncat(cwd, ua->argk[1], sizeof(cwd));
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
   int count = 0;

   if (ua->argc < 2 || !tree->node->child) {	 
      bsendmsg(ua, _("No files unmarked.\n"));
      return 1;
   }
   for (int i=1; i < ua->argc; i++) {
      for (node = tree->node->child; node; node=node->sibling) {
	 if (fnmatch(ua->argk[i], node->fname, 0) == 0) {
	    count += set_extract(ua, node, tree, false);
	 }
      }
   }
   if (count == 0) {
      bsendmsg(ua, _("No files unmarked.\n"));
   } else {
      bsendmsg(ua, _("%d file%s unmarked.\n"), count, count==0?"":"s");
   }
   return 1;
}

static int quitcmd(UAContext *ua, TREE_CTX *tree) 
{
   return 0;
}
