/*
 *  Bacula File Daemon	verify.c  Verify files.
 *
 *    Kern Sibbald, October MM
 *
 *   Version $Id$
 *
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

#include "bacula.h"
#include "filed.h"

static int verify_file(FF_PKT *ff_pkt, void *my_pkt);

/* 
 * Find all the requested files and send attributes
 * to the Director.
 * 
 */
void do_verify(JCR *jcr)
{
   BSOCK *dir = jcr->dir_bsock;
   
   jcr->buf_size = MAX_NETWORK_BUFFER_SIZE;
   if ((jcr->big_buf = (char *) malloc(jcr->buf_size)) == NULL) {
      Emsg1(M_ABORT, 0, _("Cannot malloc %d network read buffer\n"), MAX_NETWORK_BUFFER_SIZE);
   }
   set_find_options(jcr->ff, jcr->incremental, jcr->mtime);
   Dmsg0(10, "Start find files\n");
   /* Subroutine verify_file() is called for each file */
   find_files(jcr->ff, verify_file, (void *)jcr);  
   Dmsg0(10, "End find files\n");

   bnet_sig(dir, BNET_EOD);	      /* signal end of data */

   if (jcr->big_buf) {
      free(jcr->big_buf);
      jcr->big_buf = NULL;
   }
}	   

/* 
 * Called here by find() for each file.

    *****FIXME*****   add FSMs File System Modules
 
 *
 *  Find the file, compute the MD5 and send it back to the Director
 */
static int verify_file(FF_PKT *ff_pkt, void *pkt) 
{
   char attribs[MAXSTRING];
   int32_t n;
   int fid, stat, len;
   struct MD5Context md5c;
   unsigned char signature[16];
   BSOCK *dir;
   JCR *jcr = (JCR *)pkt;

   if (job_cancelled(jcr)) {
      return 0;
   }
   
   dir = jcr->dir_bsock;
   jcr->num_files_examined++;	      /* bump total file count */

   switch (ff_pkt->type) {
   case FT_LNKSAVED:		      /* Hard linked, file already saved */
      Dmsg2(30, "FT_LNKSAVED saving: %s => %s\n", ff_pkt->fname, ff_pkt->link);
      break;
   case FT_REGE:
      Dmsg1(30, "FT_REGE saving: %s\n", ff_pkt->fname);
      break;
   case FT_REG:
      Dmsg1(30, "FT_REG saving: %s\n", ff_pkt->fname);
      break;
   case FT_LNK:
      Dmsg2(30, "FT_LNK saving: %s -> %s\n", ff_pkt->fname, ff_pkt->link);
      break;
   case FT_DIR:
      Dmsg1(30, "FT_DIR saving: %s\n", ff_pkt->fname);
      break;
   case FT_SPEC:
      Dmsg1(30, "FT_SPEC saving: %s\n", ff_pkt->fname);
      break;
   case FT_NOACCESS:
      Jmsg(jcr, M_NOTSAVED, -1, _("     Could not access %s: ERR=%s\n"), ff_pkt->fname, strerror(ff_pkt->ff_errno));
      return 1;
   case FT_NOFOLLOW:
      Jmsg(jcr, M_NOTSAVED, -1, _("     Could not follow link %s: ERR=%s\n"), ff_pkt->fname, strerror(ff_pkt->ff_errno));
      return 1;
   case FT_NOSTAT:
      Jmsg(jcr, M_NOTSAVED, -1, _("      Could not stat %s: ERR=%s\n"), ff_pkt->fname, strerror(ff_pkt->ff_errno));
      return 1;
   case FT_DIRNOCHG:
   case FT_NOCHG:
      Jmsg(jcr, M_INFO, -1, _("     Unchanged file skipped: %s\n"), ff_pkt->fname);
      return 1;
   case FT_ISARCH:
      Jmsg(jcr, M_SKIPPED, -1, _("     Archive file skipped: %s\n"), ff_pkt->fname);
      return 1;
   case FT_NORECURSE:
      Jmsg(jcr, M_SKIPPED, -1, _("     Recursion turned off. Directory skipped: %s\n"), ff_pkt->fname);
      return 1;
   case FT_NOFSCHG:
      Jmsg(jcr, M_SKIPPED, -1, _("     File system change prohibited. Directory skipped: %s\n"), ff_pkt->fname);
      return 1;
   case FT_NOOPEN:
      Jmsg(jcr, M_NOTSAVED, -1, _("     Could not open directory %s: ERR=%s\n"), ff_pkt->fname, strerror(ff_pkt->ff_errno));
      return 1;
   default:
      Jmsg(jcr, M_NOTSAVED, 0, _("Unknown file type %d: %s\n"), ff_pkt->type, ff_pkt->fname);
      return 1;
   }


   if (ff_pkt->type != FT_LNKSAVED && S_ISREG(ff_pkt->statp.st_mode) && 
	 ff_pkt->statp.st_size > 0) {
      if ((fid = open(ff_pkt->fname, O_RDONLY | O_BINARY)) < 0) {
	 ff_pkt->ff_errno = errno;
         Jmsg(jcr, M_NOTSAVED, -1, _("Cannot open %s: ERR=%s.\n"), ff_pkt->fname, strerror(ff_pkt->ff_errno));
	 return 1;
      }
   } else {
      fid = -1;
   }

   encode_stat(attribs, &ff_pkt->statp);
     
   jcr->JobFiles++;		     /* increment number of files sent */
   len = strlen(ff_pkt->fname);
   jcr->last_fname = check_pool_memory_size(jcr->last_fname, len + 1);
   jcr->last_fname[len] = 0;	      /* terminate properly before copy */
   strcpy(jcr->last_fname, ff_pkt->fname);
    
   if (ff_pkt->VerifyOpts[0] == 0) {
      ff_pkt->VerifyOpts[0] = 'V';
      ff_pkt->VerifyOpts[1] = 0;
   }


   /* 
    * Send file attributes to Director
    *	File_index
    *	Stream
    *	Verify Options
    *	Filename (full path)
    *	Encoded attributes
    *	Link name (if type==FT_LNK)
    * For a directory, link is the same as fname, but with trailing
    * slash. For a linked file, link is the link.
    */
   /* Send file attributes to Director (note different format than for Storage) */
   Dmsg2(400, "send ATTR inx=%d fname=%s\n", jcr->JobFiles, ff_pkt->fname);
   if (ff_pkt->type == FT_LNK || ff_pkt->tye == FT_LNKSAVED) {
      stat = bnet_fsend(dir, "%d %d %s %s%c%s%c%s%c", jcr->JobFiles,
		    STREAM_UNIX_ATTRIBUTES, ff_pkt->VerifyOpts, ff_pkt->fname, 
		    0, attribs, 0, ff_pkt->link, 0);
   } else {
      stat = bnet_fsend(dir,"%d %d %s %s%c%s%c%c", jcr->JobFiles,
		    STREAM_UNIX_ATTRIBUTES, ff_pkt->VerifyOpts, ff_pkt->fname, 
		    0, attribs, 0, 0);
   }
   Dmsg2(20, "bfiled>bdird: attribs len=%d: msg=%s\n", dir->msglen, dir->msg);
   if (!stat) {
      Jmsg(jcr, M_ERROR, 0, _("Network error in send to Director: ERR=%s\n"), bnet_strerror(dir));
      if (fid >= 0) {
	 close(fid);
      }
      return 0;
   }

   /* If file opened, compute MD5 */
   if (fid >= 0  && ff_pkt->flags & FO_MD5) {
      char MD5buf[50];		      /* 24 should do */

      MD5Init(&md5c);

      while ((n=read(fid, jcr->big_buf, jcr->buf_size)) > 0) {
	 MD5Update(&md5c, ((unsigned char *) jcr->big_buf), n);
	 jcr->JobBytes += n;
      }
      if (n < 0) {
         Jmsg(jcr, M_WARNING, -1, _("  Error reading file %s: ERR=%s\n"), ff_pkt->fname, strerror(ff_pkt->ff_errno));
      }

      MD5Final(signature, &md5c);

      bin_to_base64(MD5buf, (char *)signature, 16); /* encode 16 bytes */
      Dmsg2(400, "send inx=%d MD5=%s\n", jcr->JobFiles, MD5buf);
      bnet_fsend(dir, "%d %d %s *MD5-%d*", jcr->JobFiles, STREAM_MD5_SIGNATURE, MD5buf,
	 jcr->JobFiles);
      Dmsg2(20, "bfiled>bdird: MD5 len=%d: msg=%s\n", dir->msglen, dir->msg);
   }
   if (fid >= 0) {
      close(fid);
   }
   return 1;
}
