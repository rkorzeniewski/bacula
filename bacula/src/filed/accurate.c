/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2000-2008 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from
   many others, a complete list can be found in the file AUTHORS.
   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version two of the GNU General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

   Bacula® is a registered trademark of John Walker.
   The licensor of Bacula is the Free Software Foundation Europe
   (FSFE), Fiduciary Program, Sumatrastrasse 25, 8006 Zürich,
   Switzerland, email:ftf@fsfeurope.org.
*/
/*
 *   Version $Id$
 *
 */

#include "bacula.h"
#include "filed.h"

typedef struct PrivateCurFile {
#ifndef USE_TCHDB
   hlink link;
#endif
   char *fname;			/* not stored with tchdb mode */
   time_t ctime;
   time_t mtime;
   bool seen;
} CurFile;

#ifdef USE_TCHDB

static bool accurate_mark_file_as_seen(JCR *jcr, CurFile *elt)
{
   bool ret=true;

   elt->seen = 1;
   if (!tchdbputasync(jcr->file_list, 
		      elt->fname, strlen(elt->fname)+1, 
		      elt, sizeof(CurFile)))
   {
      Dmsg1(2, "can't update <%s>\n", elt->fname);
      ret = false;		/* TODO: add error message */
   }

   return ret;
}

static bool accurate_lookup(JCR *jcr, char *fname, CurFile *ret)
{
   bool found=false;
   ret->seen = 0;

   if (tchdbget3(jcr->file_list, 
		 fname, strlen(fname)+1, 
		 ret, sizeof(CurFile)) != -1)
   {
      found = true;
      ret->fname = fname;
   }

   if (found) {
      Dmsg1(2, "lookup <%s> ok\n", fname);
   }

   return found;
}

static bool accurate_init(JCR *jcr, int nbfile)
{
   jcr->file_list = tchdbnew();
   tchdbsetcache(jcr->file_list, 300000);
   tchdbtune(jcr->file_list,
	     nbfile,		/* nb bucket 0.5n to 4n */
	     6,			/* size of element 2^x */
	     16,
	     0);		/* options like compression */
   /* TODO: make accurate file unique */
   POOLMEM *name  = get_pool_memory(PM_MESSAGE);
   make_unique_filename(name, jcr->JobId, "accurate");

   if(!tchdbopen(jcr->file_list, name, HDBOWRITER | HDBOCREAT)){
      /* TODO: handle error creation */
      //ecode = tchdbecode(hdb);
      //fprintf(stderr, "open error: %s\n", tchdberrmsg(ecode));
   }
   free_pool_memory(name);
   return true;
}


bool accurate_send_deleted_list(JCR *jcr)
{
   CurFile elt;
   FF_PKT *ff_pkt;
   int stream = STREAM_UNIX_ATTRIBUTES;

   if (!jcr->accurate || jcr->JobLevel == L_FULL) {
      goto bail_out;
   }

   if (jcr->file_list == NULL) {
      goto bail_out;
   }

   ff_pkt = init_find_files();
   ff_pkt->type = FT_DELETED;

   char *key;
  /* traverse records */
   tchdbiterinit(jcr->file_list);
   while((key = tchdbiternext2(jcr->file_list)) != NULL) {
      if (tchdbget3(jcr->file_list, 
		    key, strlen(key)+1, 
		    &elt, sizeof(CurFile)) != -1)
      {
	 if (!elt.seen) {
	    ff_pkt->fname = key;
	    ff_pkt->statp.st_mtime = elt.mtime;
	    ff_pkt->statp.st_ctime = elt.ctime;
	    encode_and_send_attributes(jcr, ff_pkt, stream);
	    Dmsg1(2, "deleted <%s>\n", key);
	 }
//         free(key);

      } else {			/* TODO: add error message */
	 Dmsg1(2, "No value for <%s> key\n", key);
      }
   }

   term_find_files(ff_pkt);
bail_out:
   /* TODO: clean htable when this function is not reached ? */
   if (jcr->file_list) {
      if(!tchdbclose(jcr->file_list)){
//	 ecode = tchdbecode(hdb);
//	 fprintf(stderr, "close error: %s\n", tchdberrmsg(ecode));
      }

      /* delete the object */
      tchdbdel(jcr->file_list);

      POOLMEM *name  = get_pool_memory(PM_MESSAGE);
      make_unique_filename(name, jcr->JobId, "accurate");

//    unlink(name);

      free_pool_memory(name);

      jcr->file_list = NULL;
   }
   return true;
}

#else  /* HTABLE mode */

static bool accurate_mark_file_as_seen(JCR *jcr, CurFile *elt)
{
   CurFile *temp = (CurFile *)jcr->file_list->lookup(elt->fname);
   temp->seen = 1;		/* records are in memory */
   return true;
}

static bool accurate_lookup(JCR *jcr, char *fname, CurFile *ret)
{
   bool found=false;
   ret->seen = 0;

   CurFile *temp = (CurFile *)jcr->file_list->lookup(fname);
   if (temp) {
      memcpy(ret, temp, sizeof(CurFile));
      found=true;
   }

   if (found) {
      Dmsg1(2, "lookup <%s> ok\n", fname);
   }

   return found;
}

static bool accurate_init(JCR *jcr, int nbfile)
{
   CurFile *elt=NULL;
   jcr->file_list = (htable *)malloc(sizeof(htable));
   jcr->file_list->init(elt, &elt->link, nbfile);
   return true;
}

bool accurate_send_deleted_list(JCR *jcr)
{
   CurFile *elt;
   FF_PKT *ff_pkt;
   int stream = STREAM_UNIX_ATTRIBUTES;

   if (!jcr->accurate || jcr->JobLevel == L_FULL) {
      goto bail_out;
   }

   if (jcr->file_list == NULL) {
      goto bail_out;
   }

   ff_pkt = init_find_files();
   ff_pkt->type = FT_DELETED;

   foreach_htable (elt, jcr->file_list) {
      if (!elt->seen) { /* already seen */
         Dmsg2(1, "deleted fname=%s seen=%i\n", elt->fname, elt->seen);
         ff_pkt->fname = elt->fname;
         ff_pkt->statp.st_mtime = elt->mtime;
         ff_pkt->statp.st_ctime = elt->ctime;
         encode_and_send_attributes(jcr, ff_pkt, stream);
      }
//      free(elt->fname);
   }

   term_find_files(ff_pkt);
bail_out:
   /* TODO: clean htable when this function is not reached ? */
   if (jcr->file_list) {
      jcr->file_list->destroy();
      free(jcr->file_list);
      jcr->file_list = NULL;
   }
   return true;
}

#endif

static bool accurate_add_file(JCR *jcr, char *fname, char *lstat)
{
   CurFile elt;
   struct stat statp;
   int LinkFIc;
   decode_stat(lstat, &statp, &LinkFIc); /* decode catalog stat */
   elt.ctime = statp.st_ctime;
   elt.mtime = statp.st_mtime;
   elt.seen = 0;

#ifdef USE_TCHDB
   if (!tchdbputasync(jcr->file_list,
		      fname, strlen(fname)+1,
		      &elt, sizeof(CurFile)))
   {
      Dmsg1(2, "Can't add <%s> to file_list\n", fname);
      /* TODO: check error */
   }
#else  /* HTABLE */
   CurFile *item;
   /* we store CurFile, fname and ctime/mtime in the same chunk */
   item = (CurFile *)jcr->file_list->hash_malloc(sizeof(CurFile)+strlen(fname)+1);
   memcpy(item, &elt, sizeof(CurFile));
   item->fname  = (char *)item+sizeof(CurFile);
   strcpy(item->fname, fname);
   jcr->file_list->insert(item->fname, item); 
#endif

   Dmsg2(2, "add fname=<%s> lstat=%s\n", fname, lstat);
   return true;
}

/*
 * This function is called for each file seen in fileset.
 * We check in file_list hash if fname have been backuped
 * the last time. After we can compare Lstat field. 
 * Full Lstat usage have been removed on 6612 
 */
bool accurate_check_file(JCR *jcr, FF_PKT *ff_pkt)
{
   bool stat = false;
   char *fname;
   CurFile elt;

   if (!jcr->accurate || jcr->JobLevel == L_FULL) {
      return true;
   }

   strip_path(ff_pkt);
 
   if (S_ISDIR(ff_pkt->statp.st_mode)) {
      fname = ff_pkt->link;
   } else {
      fname = ff_pkt->fname;
   } 

   if (!accurate_lookup(jcr, fname, &elt)) {
      Dmsg1(2, "accurate %s (not found)\n", fname);
      stat = true;
      goto bail_out;
   }

   if (elt.seen) { /* file has been seen ? */
      Dmsg1(2, "accurate %s (already seen)\n", fname);
      goto bail_out;
   }

   if (elt.mtime != ff_pkt->statp.st_mtime) {
     Jmsg(jcr, M_SAVED, 0, _("%s      st_mtime differs\n"), fname);
     stat = true;
   } else if (elt.ctime != ff_pkt->statp.st_ctime) {
     Jmsg(jcr, M_SAVED, 0, _("%s      st_ctime differs\n"), fname);
     stat = true;
   }

   if (stat) {
      Dmsg4(1, "%i = %i\t%i = %i\n", elt.mtime, ff_pkt->statp.st_mtime,
	    elt.ctime, ff_pkt->statp.st_ctime);
   }

   accurate_mark_file_as_seen(jcr, &elt);
   Dmsg2(2, "accurate %s = %i\n", fname, stat);

bail_out:
   unstrip_path(ff_pkt);
   return stat;
}

/* 
 * TODO: use bigbuffer from htable
 */
int accurate_cmd(JCR *jcr)
{
   BSOCK *dir = jcr->dir_bsock;
   int len;
   int32_t nb;

   if (!jcr->accurate || job_canceled(jcr) || jcr->JobLevel==L_FULL) {
      return true;
   }

   if (sscanf(dir->msg, "accurate files=%ld", &nb) != 1) {
      dir->fsend(_("2991 Bad accurate command\n"));
      return false;
   }
   Dmsg2(2, "nb=%d msg=%s\n", nb, dir->msg);

   accurate_init(jcr, nb);

   /*
    * buffer = sizeof(CurFile) + dirmsg
    * dirmsg = fname + \0 + lstat
    */
   /* get current files */
   while (dir->recv() >= 0) {
      Dmsg1(2, "accurate_cmd fname=%s\n", dir->msg);
      len = strlen(dir->msg) + 1;
      if (len < dir->msglen) {
	 accurate_add_file(jcr, dir->msg, dir->msg + len);
      }
   }

#ifdef DEBUG
   extern void *start_heap;

   char b1[50], b2[50], b3[50], b4[50], b5[50];
   Dmsg5(1," Heap: heap=%s smbytes=%s max_bytes=%s bufs=%s max_bufs=%s\n",
         edit_uint64_with_commas((char *)sbrk(0)-(char *)start_heap, b1),
         edit_uint64_with_commas(sm_bytes, b2),
         edit_uint64_with_commas(sm_max_bytes, b3),
         edit_uint64_with_commas(sm_buffers, b4),
         edit_uint64_with_commas(sm_max_buffers, b5));

#endif

   return true;
}
