/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2000-2009 Free Software Foundation Europe e.V.

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

   Bacula® is a registered trademark of Kern Sibbald.
   The licensor of Bacula is the Free Software Foundation Europe
   (FSFE), Fiduciary Program, Sumatrastrasse 25, 8006 Zürich,
   Switzerland, email:ftf@fsfeurope.org.
*/
/*
 *  Version $Id $
 *
 */

#include "bacula.h"
#include "filed.h"

static int dbglvl=100;

typedef struct PrivateCurFile {
   hlink link;
   char *fname;
   char *lstat;
   bool seen;
} CurFile;

bool accurate_mark_file_as_seen(JCR *jcr, char *fname)
{
   if (!jcr->accurate || !jcr->file_list) {
      return false;
   }
   /* TODO: just use elt->seen = 1 */
   CurFile *temp = (CurFile *)jcr->file_list->lookup(fname);
   if (temp) {
      temp->seen = 1;              /* records are in memory */
      Dmsg1(dbglvl, "marked <%s> as seen\n", fname);
   } else {
      Dmsg1(dbglvl, "<%s> not found to be marked as seen\n", fname);
   }
   return true;
}

static bool accurate_mark_file_as_seen(JCR *jcr, CurFile *elt)
{
   /* TODO: just use elt->seen = 1 */
   CurFile *temp = (CurFile *)jcr->file_list->lookup(elt->fname);
   if (temp) {
      temp->seen = 1;              /* records are in memory */
   }
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
      Dmsg1(dbglvl, "lookup <%s> ok\n", fname);
   }

   return found;
}

static bool accurate_init(JCR *jcr, int nbfile)
{
   CurFile *elt = NULL;
   jcr->file_list = (htable *)malloc(sizeof(htable));
   jcr->file_list->init(elt, &elt->link, nbfile);
   return true;
}

static bool accurate_send_base_file_list(JCR *jcr)
{
   CurFile *elt;
   struct stat statc;
   int32_t LinkFIc;
   FF_PKT *ff_pkt;
   int stream = STREAM_UNIX_ATTRIBUTES;

   if (!jcr->accurate || jcr->get_JobLevel() != L_FULL) {
      return true;
   }

   if (jcr->file_list == NULL) {
      return true;
   }

   ff_pkt = init_find_files();
   ff_pkt->type = FT_BASE;

   foreach_htable(elt, jcr->file_list) {
      if (elt->seen) {
         Dmsg2(dbglvl, "base file fname=%s seen=%i\n", elt->fname, elt->seen);
         decode_stat(elt->lstat, &statc, &LinkFIc); /* decode catalog stat */      
         ff_pkt->fname = elt->fname;
         ff_pkt->statp = statc;
         encode_and_send_attributes(jcr, ff_pkt, stream);
//       free(elt->fname);
      }
   }

   term_find_files(ff_pkt);
   return true;
}


/* This function is called at the end of backup
 * We walk over all hash disk element, and we check
 * for elt.seen.
 */
static bool accurate_send_deleted_list(JCR *jcr)
{
   CurFile *elt;
   struct stat statc;
   int32_t LinkFIc;
   FF_PKT *ff_pkt;
   int stream = STREAM_UNIX_ATTRIBUTES;

   if (!jcr->accurate) {
      return true;
   }

   if (jcr->file_list == NULL) {
      return true;
   }

   ff_pkt = init_find_files();
   ff_pkt->type = FT_DELETED;

   foreach_htable(elt, jcr->file_list) {
      if (elt->seen || plugin_check_file(jcr, elt->fname)) {
         continue;
      }
      Dmsg2(dbglvl, "deleted fname=%s seen=%i\n", elt->fname, elt->seen);
      decode_stat(elt->lstat, &statc, &LinkFIc); /* decode catalog stat */
      ff_pkt->fname = elt->fname;
      ff_pkt->statp.st_mtime = statc.st_mtime;
      ff_pkt->statp.st_ctime = statc.st_ctime;
      encode_and_send_attributes(jcr, ff_pkt, stream);
//    free(elt->fname);
   }

   term_find_files(ff_pkt);
   return true;
}

void accurate_free(JCR *jcr)
{
   if (jcr->file_list) {
      jcr->file_list->destroy();
      free(jcr->file_list);
      jcr->file_list = NULL;
   }
}

/* Send the deleted or the base file list and cleanup  */
bool accurate_finish(JCR *jcr)
{
   bool ret=true;
   if (jcr->accurate) {
      if (jcr->get_JobLevel() == L_FULL) {
         ret = accurate_send_base_file_list(jcr);
      } else {
         ret = accurate_send_deleted_list(jcr);
      }
      
      accurate_free(jcr);
   } 
   return ret;
}

static bool accurate_add_file(JCR *jcr, char *fname, char *lstat)
{
   bool ret = true;
   CurFile elt;
   elt.seen = 0;

   CurFile *item;
   /* we store CurFile, fname and ctime/mtime in the same chunk */
   item = (CurFile *)jcr->file_list->hash_malloc(sizeof(CurFile)+strlen(fname)+strlen(lstat)+2);
   memcpy(item, &elt, sizeof(CurFile));
   item->fname  = (char *)item+sizeof(CurFile);
   strcpy(item->fname, fname);
   item->lstat  = item->fname+strlen(item->fname)+1;
   strcpy(item->lstat, lstat);
   jcr->file_list->insert(item->fname, item); 

   Dmsg2(dbglvl, "add fname=<%s> lstat=%s\n", fname, lstat);
   return ret;
}

/*
 * This function is called for each file seen in fileset.
 * We check in file_list hash if fname have been backuped
 * the last time. After we can compare Lstat field. 
 * Full Lstat usage have been removed on 6612 
 *
 * Returns: true   if file has changed (must be backed up)
 *          false  file not changed
 */
bool accurate_check_file(JCR *jcr, FF_PKT *ff_pkt)
{
   struct stat statc;
   int32_t LinkFIc;
   bool stat = false;
   char *fname;
   CurFile elt;

   if (!jcr->accurate) {
      return true;
   }

   strip_path(ff_pkt);
 
   if (S_ISDIR(ff_pkt->statp.st_mode)) {
      fname = ff_pkt->link;
   } else {
      fname = ff_pkt->fname;
   } 

   if (!accurate_lookup(jcr, fname, &elt)) {
      Dmsg1(dbglvl, "accurate %s (not found)\n", fname);
      stat = true;
      goto bail_out;
   }

   if (elt.seen) { /* file has been seen ? */
      Dmsg1(dbglvl, "accurate %s (already seen)\n", fname);
      goto bail_out;
   }

   decode_stat(elt.lstat, &statc, &LinkFIc); /* decode catalog stat */

#if 0
   /*
    * Loop over options supplied by user and verify the
    * fields he requests.
    */
   for (p=Opts_Digest; *p; p++) {
      char ed1[30], ed2[30];
      switch (*p) {
      case 'i':                /* compare INODEs */
         if (statc.st_ino != statf.st_ino) {
            prt_fname(jcr);
            Jmsg(jcr, M_INFO, 0, _("      st_ino   differ. Cat: %s File: %s\n"),
                 edit_uint64((uint64_t)statc.st_ino, ed1),
                 edit_uint64((uint64_t)statf.st_ino, ed2));
            set_jcr_job_status(jcr, JS_Differences);
         }
         break;
      case 'p':                /* permissions bits */
         if (statc.st_mode != statf.st_mode) {
            prt_fname(jcr);
            Jmsg(jcr, M_INFO, 0, _("      st_mode  differ. Cat: %x File: %x\n"),
                 (uint32_t)statc.st_mode, (uint32_t)statf.st_mode);
            set_jcr_job_status(jcr, JS_Differences);
         }
         break;
      case 'n':                /* number of links */
         if (statc.st_nlink != statf.st_nlink) {
            prt_fname(jcr);
            Jmsg(jcr, M_INFO, 0, _("      st_nlink differ. Cat: %d File: %d\n"),
                 (uint32_t)statc.st_nlink, (uint32_t)statf.st_nlink);
            set_jcr_job_status(jcr, JS_Differences);
         }
         break;
      case 'u':                /* user id */
         if (statc.st_uid != statf.st_uid) {
            prt_fname(jcr);
            Jmsg(jcr, M_INFO, 0, _("      st_uid   differ. Cat: %u File: %u\n"),
                 (uint32_t)statc.st_uid, (uint32_t)statf.st_uid);
            set_jcr_job_status(jcr, JS_Differences);
         }
         break;
      case 'g':                /* group id */
         if (statc.st_gid != statf.st_gid) {
            prt_fname(jcr);
            Jmsg(jcr, M_INFO, 0, _("      st_gid   differ. Cat: %u File: %u\n"),
                 (uint32_t)statc.st_gid, (uint32_t)statf.st_gid);
            set_jcr_job_status(jcr, JS_Differences);
         }
         break;
      case 's':                /* size */
         if (statc.st_size != statf.st_size) {
            prt_fname(jcr);
            Jmsg(jcr, M_INFO, 0, _("      st_size  differ. Cat: %s File: %s\n"),
                 edit_uint64((uint64_t)statc.st_size, ed1),
                 edit_uint64((uint64_t)statf.st_size, ed2));
            set_jcr_job_status(jcr, JS_Differences);
         }
         break;
      case 'a':                /* access time */
         if (statc.st_atime != statf.st_atime) {
            prt_fname(jcr);
            Jmsg(jcr, M_INFO, 0, _("      st_atime differs\n"));
            set_jcr_job_status(jcr, JS_Differences);
         }
         break;
      case 'm':
         if (statc.st_mtime != statf.st_mtime) {
            prt_fname(jcr);
            Jmsg(jcr, M_INFO, 0, _("      st_mtime differs\n"));
            set_jcr_job_status(jcr, JS_Differences);
         }
         break;
      case 'c':                /* ctime */
         if (statc.st_ctime != statf.st_ctime) {
            prt_fname(jcr);
            Jmsg(jcr, M_INFO, 0, _("      st_ctime differs\n"));
            set_jcr_job_status(jcr, JS_Differences);
         }
         break;
      case 'd':                /* file size decrease */
         if (statc.st_size > statf.st_size) {
            prt_fname(jcr);
            Jmsg(jcr, M_INFO, 0, _("      st_size  decrease. Cat: %s File: %s\n"),
                 edit_uint64((uint64_t)statc.st_size, ed1),
                 edit_uint64((uint64_t)statf.st_size, ed2));
            set_jcr_job_status(jcr, JS_Differences);
         }
         break;
      case '5':                /* compare MD5 */
         Dmsg1(500, "set Do_MD5 for %s\n", jcr->fname);
         do_Digest = CRYPTO_DIGEST_MD5;
         break;
      case '1':                 /* compare SHA1 */
         do_Digest = CRYPTO_DIGEST_SHA1;
               break;
      case ':':
      case 'V':
      default:
         break;
            }
   }
#endif
   /*
    * We check only mtime/ctime like with the normal
    * incremental/differential mode
    */
   if (statc.st_mtime != ff_pkt->statp.st_mtime) {
//   Jmsg(jcr, M_SAVED, 0, _("%s      st_mtime differs\n"), fname);
      Dmsg3(dbglvl, "%s      st_mtime differs (%lld!=%lld)\n", 
            fname, statc.st_mtime, (utime_t)ff_pkt->statp.st_mtime);
     stat = true;
   } else if (!(ff_pkt->flags & FO_MTIMEONLY) 
              && (statc.st_ctime != ff_pkt->statp.st_ctime)) {
//   Jmsg(jcr, M_SAVED, 0, _("%s      st_ctime differs\n"), fname);
      Dmsg1(dbglvl, "%s      st_ctime differs\n", fname);
      stat = true;

   } else if (statc.st_size != ff_pkt->statp.st_size) {
//   Jmsg(jcr, M_SAVED, 0, _("%s      st_size differs\n"), fname);
      Dmsg1(dbglvl, "%s      st_size differs\n", fname);
      stat = true;
   }

   accurate_mark_file_as_seen(jcr, &elt);
//   Dmsg2(dbglvl, "accurate %s = %d\n", fname, stat);

bail_out:
   unstrip_path(ff_pkt);
   return stat;
}

/* 
 * TODO: use big buffer from htable
 */
int accurate_cmd(JCR *jcr)
{
   BSOCK *dir = jcr->dir_bsock;
   int len;
   int32_t nb;

   if (job_canceled(jcr)) {
      return true;
   }
   if (sscanf(dir->msg, "accurate files=%ld", &nb) != 1) {
      dir->fsend(_("2991 Bad accurate command\n"));
      return false;
   }

   jcr->accurate = true;

   accurate_init(jcr, nb);

   /*
    * buffer = sizeof(CurFile) + dirmsg
    * dirmsg = fname + \0 + lstat
    */
   /* get current files */
   while (dir->recv() >= 0) {
      len = strlen(dir->msg) + 1;
      if (len < dir->msglen) {
         accurate_add_file(jcr, dir->msg, dir->msg + len);
      }
   }

#ifdef DEBUG
   extern void *start_heap;

   char b1[50], b2[50], b3[50], b4[50], b5[50];
   Dmsg5(dbglvl," Heap: heap=%s smbytes=%s max_bytes=%s bufs=%s max_bufs=%s\n",
         edit_uint64_with_commas((char *)sbrk(0)-(char *)start_heap, b1),
         edit_uint64_with_commas(sm_bytes, b2),
         edit_uint64_with_commas(sm_max_bytes, b3),
         edit_uint64_with_commas(sm_buffers, b4),
         edit_uint64_with_commas(sm_max_buffers, b5));
#endif

   return true;
}
