/*
 * Directory tree build/traverse routines
 * 
 *    Kern Sibbald, June MMII
 *
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
#include "findlib/find.h"
#include "findlib/system.h"
	     
#ifndef MAXPATHLEN
#define MAXPATHLEN 1000
#endif

TREE_NODE *new_tree_node(int type)
{
   TREE_NODE *node;
   int size;

   if (type == TN_ROOT) {
      size = sizeof(TREE_ROOT);
   } else {
      size = sizeof(TREE_NODE);
   }
   node = (TREE_NODE *)malloc(size);
   memset(node, 0, size);
   node->type = type;
   return node;
}

TREE_NODE *insert_tree_node(char *path, TREE_NODE *node, TREE_ROOT *root, TREE_NODE *parent)
{
   char *p, *fname;

   Dmsg1(100, "insert_tree_node: %s\n", path);
   p = strrchr(path, '/');
   if (!p) {
      Dmsg1(000, "No / found: %s\n", path);
      exit(1);
   }
   *p = 0;
   fname = p + 1;
   if (!parent) {
      parent = make_tree_path(path, root);
   }
   *p = '/';
   append_tree_node(fname, node, root, parent);
   Dmsg1(100, "insert_tree_node: parent=%s\n", parent->fname);
   return parent;
}

TREE_NODE *make_tree_path(char *path, TREE_ROOT *root)
{
   TREE_NODE *parent, *sibling;
   char *fname, *p;

   Dmsg1(100, "make_tree_path: %s\n", path);
   if (!*path) {
      Dmsg0(100, "make_tree_path: parent=*root*\n");
      return (TREE_NODE *)root;
   }
   p = strrchr(path, '/');
   if (!p) {
      Dmsg1(000, "No / found: %s\n", path);
      exit(1);
   }
   *p = 0;
   fname = p + 1;
   /* Find parent */
   parent = make_tree_path(path, root);
   *p = '/';
   /* Is it already a sibling? */
   for (sibling=parent->sibling; sibling; sibling=sibling->sibling) {
      if (strcmp(sibling->fname, fname) == 0) {
         Dmsg1(100, "make_tree_path: found parent=%s\n", parent->fname);
	 return parent;
      }
   }
   /* Must add */
   sibling = new_tree_node(TN_NEWDIR);
   append_tree_node(fname, sibling, root, parent);
   parent = sibling;
   Dmsg1(100, "make_tree_path: add parent=%s\n", parent->fname);
   return parent;
}  

/*
 *  Append sibling to parent's child chain
 */
void append_tree_node(char *fname, TREE_NODE *node, TREE_ROOT *root, TREE_NODE *parent)
{
   TREE_NODE *child;

   Dmsg1(100, "append_tree_node: %s\n", fname);
   node->fname = bstrdup(fname);
   node->parent = parent;
   if (!parent->child) {
      parent->child = node;
      goto item_link;
   }
   for (child=parent->child; child->sibling; child=child->sibling)
      { }
   child->sibling = node;

item_link:
   if (!root->first) {
      root->first = node;
      root->last = node;
   } else {
      root->last->next = node;
      root->last = node;
   }
   return;
}

TREE_NODE *first_tree_node(TREE_ROOT *root)
{
   return root->first;
}

TREE_NODE *next_tree_node(TREE_NODE *node)
{
   return node->next;
}


void print_tree(char *path, TREE_NODE *tree)
{
   char buf[MAXPATHLEN];
   char *termchr;

   if (!tree) {
      return;
   }
   switch (tree->type) {
   case TN_DIR:
   case TN_NEWDIR:  
      termchr = "/";
      break;
   case TN_ROOT:
   case TN_FILE:
   default:
      termchr = "";
      break;
   }
   Dmsg3(-1, "%s/%s%s\n", path, tree->fname, termchr);
   switch (tree->type) {
   case TN_FILE:
      break;
   case TN_DIR:
      sprintf(buf, "%s/%s", path, tree->fname);
      print_tree(buf, tree->child);
      break;
   case TN_ROOT:
      print_tree(path, tree->child);
      break;
   case TN_NEWDIR:  
      sprintf(buf, "%s/%s", path, tree->fname);
      print_tree(buf, tree->child);
      break;
   default:
      Dmsg1(000, "Unknown node type %d\n", tree->type);
   }
   print_tree(path, tree->sibling);
   return;
}

void free_tree(TREE_NODE *node)
{
   if (!node) {
      return;
   }
   switch (node->type) {
   case TN_FILE:
      break;
   case TN_DIR:
   case TN_ROOT:
   case TN_NEWDIR:  
      free_tree(node->child);
      break;
   default:
      Dmsg1(000, "Unknown node type %d\n", node->type);
      break;
   }
   free_tree(node->sibling);
   free(node->fname);
   free(node);
   return;
}

int tree_getpath(TREE_NODE *node, char *buf, int buf_size)
{
   if (!node) {
      buf[0] = 0;
      return 1;
   }
   tree_getpath(node->parent, buf, buf_size);
   strcat(buf, node->fname);
   if (node->type != TN_FILE) {
      strcat(buf, "/");
   }
   return 1;
}

/* 
 * Change to specified directory
 */
TREE_NODE *tree_cwd(char *path, TREE_ROOT *root, TREE_NODE *node)
{
   if (strcmp(path, ".") == 0) {
      return node;
   }
   if (strcmp(path, "..") == 0) {
      if (node->parent) {
	 return node->parent;
      } else {
	 return node;
      }
   }
   if (path[0] == '/') {
      Dmsg0(100, "Doing absolute lookup.\n");
      return tree_relcwd(path+1, root, (TREE_NODE *)root);
   }
   Dmsg0(100, "Doing relative lookup.\n");
   return tree_relcwd(path, root, node);
}


TREE_NODE *tree_relcwd(char *path, TREE_ROOT *root, TREE_NODE *node)
{
   char *p;
   int len;
   TREE_NODE *cd;

   if (*path == 0) {
      return node;
   }
   p = strchr(path, '/');
   if (p) {
      len = p - path;
   }
   for (cd=node->child; cd; cd=cd->sibling) {
      if (strncmp(cd->fname, path, len) == 0) {
         Dmsg1(100, "tree_relcwd: found cd=%s\n", cd->fname);
	 break;
      }
   }
   if (!cd || cd->type == TN_FILE) {
      Dmsg1(100, "tree_relcwd: failed %s is a file.\n", cd->fname);
      return NULL;
   }
   if (!p) {
      Dmsg0(100, "tree_relcwd: no more to lookup. found.\n");
      return cd;
   }
   Dmsg2(100, "recurse tree_relcwd with path=%s, cd=%s\n", p+1, cd->fname);
   return tree_relcwd(p+1, root, cd);
}



#ifdef BUILD_TEST_PROGRAM

void FillDirectoryTree(char *path, TREE_ROOT *root, TREE_NODE *parent);

static uint32_t FileIndex = 0;
/*
 * Simple test program for tree routines
 */
int main(int argc, char *argv[])
{
    TREE_ROOT *root;
    TREE_NODE *node;
    char buf[MAXPATHLEN];

    root = (TREE_ROOT *)new_tree_node(TN_ROOT);
    root->fname = bstrdup("");

    FillDirectoryTree("/home/kern/bacula/k", root, NULL);

    for (node = first_tree_node(root); node; node=next_tree_node(node)) {
       tree_getpath(node, buf, sizeof(buf));
       Dmsg2(100, "%d: %s\n", node->FileIndex, buf);
    }

    node = (TREE_NODE *)root;
    Dmsg0(000, "doing cd /home/kern/bacula/k/techlogs\n");
    node = tree_cwd("/home/kern/bacula/k/techlogs", root, node);
    if (node) {
       tree_getpath(node, buf, sizeof(buf));
       Dmsg2(100, "findex=%d: cwd=%s\n", node->FileIndex, buf);
    }

    Dmsg0(000, "doing cd /home/kern/bacula/k/src/testprogs\n");
    node = tree_cwd("/home/kern/bacula/k/src/testprogs", root, node);
    if (node) {
       tree_getpath(node, buf, sizeof(buf));
       Dmsg2(100, "findex=%d: cwd=%s\n", node->FileIndex, buf);
    } else {
       Dmsg0(100, "testprogs not found.\n");
    }

    free_tree((TREE_NODE *)root);

    return 0;
}

void FillDirectoryTree(char *path, TREE_ROOT *root, TREE_NODE *parent)
{
   TREE_NODE *newparent = NULL;
   TREE_NODE *node;
   struct stat statbuf;
   DIR *dp;
   struct dirent *dir;
   char pathbuf[MAXPATHLEN];
   char file[MAXPATHLEN];
   int type;
   int i;
   
   Dmsg1(100, "FillDirectoryTree: %s\n", path);
   dp = opendir(path);
   if (!dp) {
      return;
   }
   while ((dir = readdir(dp))) {
      if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0) {
	 continue;
      }
      strcpy(file, dir->d_name);
      snprintf(pathbuf, MAXPATHLEN-1, "%s/%s", path, file);
      if (lstat(pathbuf, &statbuf) < 0) {
         printf("lstat() failed. ERR=%s\n", strerror(errno));
	 continue;
      }
//      printf("got file=%s, pathbuf=%s\n", file, pathbuf);
      type = TN_FILE;
      if (S_ISLNK(statbuf.st_mode))
	 type =  TN_FILE;  /* link */
      else if (S_ISREG(statbuf.st_mode))
	 type = TN_FILE;
      else if (S_ISDIR(statbuf.st_mode)) {
	 type = TN_DIR;
      } else if (S_ISCHR(statbuf.st_mode))
	 type = TN_FILE; /* char dev */
      else if (S_ISBLK(statbuf.st_mode))
	 type = TN_FILE; /* block dev */
      else if (S_ISFIFO(statbuf.st_mode))
	 type = TN_FILE; /* fifo */
      else if (S_ISSOCK(statbuf.st_mode))
	 type = TN_FILE; /* sock */
      else {
	 type = TN_FILE;
         printf("Unknown file type: 0x%x\n", statbuf.st_mode);
      }

      Dmsg2(100, "Doing: %d %s\n", type, pathbuf);
      node = new_tree_node(type);
      node->FileIndex = ++FileIndex;
      parent = insert_tree_node(pathbuf, node, root, parent);
      if (S_ISDIR(statbuf.st_mode) && !S_ISLNK(statbuf.st_mode)) {
         Dmsg2(100, "calling fill. pathbuf=%s, file=%s\n", pathbuf, file);
	 FillDirectoryTree(pathbuf, root, node);
      }
   }
   closedir(dp);
}
#endif
