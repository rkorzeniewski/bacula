/*
 *  Bacula File Daemon	verify.c  Verify files.
 *
 *    Kern Sibbald, October MM
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
   if (!find_files(jcr->ff, verify_file, (void *)jcr)) {
      /****FIXME**** error termination */
      Dmsg0(0, "========= Error return from find_files\n");
   }
   Dmsg0(10, "End find files\n");

   dir->msglen = 0;
   bnet_send(dir);		      /* signal end attributes to director */
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
   int fid;
   struct MD5Context md5c;
   unsigned char signature[16];
   BSOCK *sd, *dir;
   JCR *jcr = (JCR *)pkt;

   sd = jcr->store_bsock;
   dir = jcr->dir_bsock;

   switch (ff_pkt->type) {
   case FT_LNKSAVED:		      /* Hard linked, file already saved */
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
      Jmsg(jcr, M_ERROR, -1, _("     Could not access %s: ERR=%s\n"), ff_pkt->fname, strerror(ff_pkt->ff_errno));
      return 1;
   case FT_NOFOLLOW:
      Jmsg(jcr, M_ERROR, -1, _("     Could not follow link %s: ERR=%s\n"), ff_pkt->fname, strerror(ff_pkt->ff_errno));
      return 1;
   case FT_NOSTAT:
      Jmsg(jcr, M_ERROR, -1, _("      Could not stat %s: ERR=%s\n"), ff_pkt->fname, strerror(ff_pkt->ff_errno));
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
      Jmsg(jcr, M_ERROR, -1, _("     Could not open directory %s: ERR=%s\n"), ff_pkt->fname, strerror(ff_pkt->ff_errno));
      return 1;
   default:
      Jmsg(jcr, M_ERROR, 0, _("Unknown file type %d: %s\n"), ff_pkt->type, ff_pkt->fname);
      return 1;
   }

   if ((fid = open(ff_pkt->fname, O_RDONLY | O_BINARY)) < 0) {
      ff_pkt->ff_errno = errno;
      Jmsg(jcr, M_ERROR, -1, _("  Cannot open %s: ERR=%s.\n"), ff_pkt->fname, strerror(ff_pkt->ff_errno));
      return 1;
   }

   Dmsg2(50, "opened %s fid=%d\n", ff_pkt->fname, fid);
   Dmsg1(10, "bfiled: sending %s to Director\n", ff_pkt->fname);
   encode_stat(attribs, &ff_pkt->statp);
     
   jcr->JobFiles++;		     /* increment number of files sent */
    
   if (ff_pkt->VerifyOpts[0] == 0) {
      ff_pkt->VerifyOpts[0] = 'V';
      ff_pkt->VerifyOpts[1] = 0;
   }
   /* Send file attributes to Director (note different format than for Storage) */
   if (ff_pkt->type == FT_LNK) {
      dir->msglen = sprintf(dir->msg,"%d %d %s %s%c%s%c%s%c", jcr->JobFiles,
		    STREAM_UNIX_ATTRIBUTES, ff_pkt->VerifyOpts, ff_pkt->fname, 
		    0, attribs, 0, ff_pkt->link, 0);
   } else {
      dir->msglen = sprintf(dir->msg,"%d %d %s %s%c%s%c%c", jcr->JobFiles,
		    STREAM_UNIX_ATTRIBUTES, ff_pkt->VerifyOpts, ff_pkt->fname, 
		    0, attribs, 0, 0);
   }
   Dmsg2(20, "bfiled>bdird: attribs len=%d: msg=%s\n", dir->msglen, dir->msg);
   bnet_send(dir);	      /* send to Director */


   /* 
    * If MD5 is requested, read the file and compute the MD5   
    *
    */
   if (ff_pkt->flags & FO_MD5) {
      char MD5buf[50];		      /* 24 should do */

      MD5Init(&md5c);

      if (ff_pkt->type != FT_LNKSAVED && S_ISREG(ff_pkt->statp.st_mode) && 
	    ff_pkt->statp.st_size > 0) {
	 while ((n=read(fid, jcr->big_buf, jcr->buf_size)) > 0) {
	    MD5Update(&md5c, ((unsigned char *) jcr->big_buf), n);
	    jcr->JobBytes += n;
	 }
	 if (n < 0) {
            Jmsg(jcr, M_ERROR, -1, _("  Error reading %s: ERR=%s\n"), ff_pkt->fname, strerror(ff_pkt->ff_errno));
	 }
      }

      MD5Final(signature, &md5c);

      bin_to_base64(MD5buf, (char *)signature, 16); /* encode 16 bytes */
      dir->msglen = sprintf(dir->msg, "%d %d %s X", jcr->JobFiles, 
	 STREAM_MD5_SIGNATURE, MD5buf);
      Dmsg2(20, "bfiled>bdird: MD5 len=%d: msg=%s\n", dir->msglen, dir->msg);
      bnet_send(dir);		     /* send MD5 signature to Director */
   }
   close(fid);
   return 1;
}
