/*
 *  Bacula File Daemon	backup.c  send file attributes and data
 *   to the Storage daemon.
 *
 *    Kern Sibbald, March MM
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

static int save_file(FF_PKT *ff_pkt, void *pkt);

/* 
 * Find all the requested files and send them
 * to the Storage daemon.
 * 
 */
int blast_data_to_storage_daemon(JCR *jcr, char *addr) 
{
   BSOCK *sd;
   int stat = 1;

   sd = jcr->store_bsock;

   jcr->JobStatus = JS_Running;

   Dmsg1(10, "bfiled: opened data connection %d to stored\n", sd->fd);

   if (!bnet_set_buffer_size(sd, MAX_NETWORK_BUFFER_SIZE, BNET_SETBUF_WRITE)) {
      return 0;
   }

   jcr->buf_size = sd->msglen;		   
   /* Adjust for compression so that output buffer is
    * 12 bytes + 0.1% larger than input buffer plus 2 bytes.
    * Note, we adjust the read size to be smaller so that the
    * same output buffer can be used without growing it.
    */
   jcr->compress_buf_size = jcr->buf_size + ((jcr->buf_size+999) / 1000) + 14;
   jcr->compress_buf = get_memory(jcr->compress_buf_size);

   Dmsg1(100, "set_find_options ff=%p\n", jcr->ff);
   set_find_options(jcr->ff, jcr->incremental, jcr->mtime);
   Dmsg0(10, "start find files\n");

   /* Subroutine save_file() is called for each file */
   /* ***FIXME**** add FSM code */
   if (!find_files(jcr->ff, save_file, (void *)jcr)) {
      stat = 0; 		      /* error */
   }

   bnet_sig(sd, BNET_EOD);	      /* end data connection */

   if (jcr->big_buf) {
      free(jcr->big_buf);
      jcr->big_buf = NULL;
   }
   if (jcr->compress_buf) {
      free_pool_memory(jcr->compress_buf);
      jcr->compress_buf = NULL;
   }
   return stat;
}	   

/* 
 * Called here by find() for each file included.
 *
 *  *****FIXME*****   add FSMs File System Modules
 *
 *  Send the file and its data to the Storage daemon.
 */
static int save_file(FF_PKT *ff_pkt, void *ijcr)
{
   char attribs[MAXSTRING];
   int fid, stat, stream, len;
   struct MD5Context md5c;
   int gotMD5 = 0;
   unsigned char signature[16];
   BSOCK *sd;
   JCR *jcr = (JCR *)ijcr;
   POOLMEM *msgsave;

   if (job_cancelled(jcr)) {
      return 0;
   }

   sd = jcr->store_bsock;
   jcr->num_files_examined++;	      /* bump total file count */

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
      Dmsg1(30, "FT_DIR saving: %s\n", ff_pkt->link);
      break;
   case FT_SPEC:
      Dmsg1(30, "FT_SPEC saving: %s\n", ff_pkt->fname);
      break;
   case FT_NOACCESS:
      Jmsg(jcr, M_NOTSAVED, -1, _("     Could not access %s: ERR=%s"), ff_pkt->fname, 
	 strerror(ff_pkt->ff_errno));
      return 1;
   case FT_NOFOLLOW:
      Jmsg(jcr, M_NOTSAVED, -1, _("     Could not follow link %s: ERR=%s\n"), ff_pkt->fname, 
	 strerror(ff_pkt->ff_errno));
      return 1;
   case FT_NOSTAT:
      Jmsg(jcr, M_NOTSAVED, -1, _("     Could not stat %s: ERR=%s\n"), ff_pkt->fname, 
	 strerror(ff_pkt->ff_errno));
      return 1;
   case FT_DIRNOCHG:
   case FT_NOCHG:
      Jmsg(jcr, M_SKIPPED, -1,  _("     Unchanged file skipped: %s\n"), ff_pkt->fname);
      return 1;
   case FT_ISARCH:
      Jmsg(jcr, M_NOTSAVED, -1, _("     Archive file not saved: %s\n"), ff_pkt->fname);
      return 1;
   case FT_NORECURSE:
      Jmsg(jcr, M_SKIPPED, -1,  _("     Recursion turned off. Directory skipped: %s\n"), 
	 ff_pkt->fname);
      return 1;
   case FT_NOFSCHG:
      Jmsg(jcr, M_SKIPPED, -1,  _("     File system change prohibited. Directory skipped. %s\n"), 
	 ff_pkt->fname);
      return 1;
   case FT_NOOPEN:
      Jmsg(jcr, M_NOTSAVED, -1, _("     Could not open directory %s: ERR=%s\n"), ff_pkt->fname, 
	 strerror(ff_pkt->ff_errno));
      return 1;
   default:
      Jmsg(jcr, M_ERROR, 0, _("Unknown file type %d; not saved: %s\n"), ff_pkt->type, ff_pkt->fname);
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

   Dmsg1(30, "bfiled: sending %s to stored\n", ff_pkt->fname);
   encode_stat(attribs, &ff_pkt->statp);
     
   jcr->JobFiles++;		       /* increment number of files sent */
   len = strlen(ff_pkt->fname);
   jcr->last_fname = check_pool_memory_size(jcr->last_fname, len + 1);
   jcr->last_fname[len] = 0;	      /* terminate properly before copy */
   strcpy(jcr->last_fname, ff_pkt->fname);
    
   /*
    * Send Attributes header to Storage daemon
    *	 <file-index> <stream> <info>
    */
#ifndef NO_FD_SEND_TEST
   if (!bnet_fsend(sd, "%ld %d 0", jcr->JobFiles, STREAM_UNIX_ATTRIBUTES)) {
      if (fid >= 0) {
	 close(fid);
      }
      return 0;
   }
   Dmsg1(10, ">stored: attrhdr %s\n", sd->msg);

   /* 
    * Send file attributes to Storage daemon   
    *	File_index
    *	File type
    *	Filename (full path)
    *	Encoded attributes
    *	Link name (if type==FT_LNK)
    * For a directory, link is the same as fname, but with trailing
    * slash. For a linked file, link is the link.
    */
   if (ff_pkt->type == FT_LNK) {
      stat = bnet_fsend(sd, "%ld %d %s%c%s%c%s%c", jcr->JobFiles, 
	       ff_pkt->type, ff_pkt->fname, 0, attribs, 0, ff_pkt->link, 0);
   } else if (ff_pkt->type == FT_DIR) {
      stat = bnet_fsend(sd, "%ld %d %s%c%s%c%c", jcr->JobFiles, 
	       ff_pkt->type, ff_pkt->link, 0, attribs, 0, 0);
   } else {
      stat = bnet_fsend(sd, "%ld %d %s%c%s%c%c", jcr->JobFiles, 
	       ff_pkt->type, ff_pkt->fname, 0, attribs, 0, 0);
   }

   Dmsg2(20, ">stored: attr len=%d: %s\n", sd->msglen, sd->msg);
   if (!stat) {
      if (fid >= 0) {
	 close(fid);
      }
      return 0;
   }
   /* send data termination sentinel */
   bnet_sig(sd, BNET_EOD);
#endif

   /* 
    * If the file has data, read it and send to the Storage daemon
    *
    */
   if (fid >= 0) {

      Dmsg1(60, "Saving data, type=%d\n", ff_pkt->type);
      /*
       * Send Data header to Storage daemon
       *    <file-index> <stream> <info>
       */

      stream = STREAM_FILE_DATA;

#ifdef HAVE_LIBZ
      if (ff_pkt->flags & FO_GZIP) {
	 stream = STREAM_GZIP_DATA;
      }
#endif

#ifndef NO_FD_SEND_TEST
      if (!bnet_fsend(sd, "%ld %d 0", jcr->JobFiles, stream)) {
	 close(fid);
	 return 0;
      }
      Dmsg1(10, ">stored: datahdr %s\n", sd->msg);
#endif

      if (ff_pkt->flags & FO_MD5) {
	 MD5Init(&md5c);
      }

      msgsave = sd->msg;
      while ((sd->msglen=read(fid, sd->msg, jcr->buf_size)) > 0) {
	 if (ff_pkt->flags & FO_MD5) {
	    MD5Update(&md5c, (unsigned char *)(sd->msg), sd->msglen);
	    gotMD5 = 1;
	 }
#ifdef HAVE_LIBZ
	 /* ***FIXME*** add compression level options */
	 if (ff_pkt->flags & FO_GZIP) {
	    uLong compress_len;
	    compress_len = jcr->compress_buf_size; /* set max length */
	    if (compress2((Bytef *)jcr->compress_buf, &compress_len, 
		  (const Bytef *)sd->msg, (uLong)sd->msglen,
		  ff_pkt->GZIP_level)  != Z_OK) {
               Jmsg(jcr, M_ERROR, 0, _("Compression error\n"));
	       sd->msg = msgsave;
	       sd->msglen = 0;
	       close(fid);
	       return 0;
	    }
            Dmsg2(100, "compressed len=%d uncompressed len=%d\n", 
	       compress_len, sd->msglen);

	    sd->msg = jcr->compress_buf; /* write compressed buffer */
	    sd->msglen = compress_len;
#ifndef NO_FD_SEND_TEST
	    if (!bnet_send(sd)) {
	       sd->msg = msgsave;     /* restore read buffer */
	       sd->msglen = 0;
	       close(fid);
	       return 0;
	    }
            Dmsg1(30, "Send data to FD len=%d\n", sd->msglen);
#endif
	    jcr->JobBytes += sd->msglen;
	    sd->msg = msgsave;	      /* restore read buffer */
	    continue;
	 }
#endif
#ifndef NO_FD_SEND_TEST
	 if (!bnet_send(sd)) {
	    close(fid);
	    return 0;
	 }
         Dmsg1(30, "Send data to FD len=%d\n", sd->msglen);
#endif
	 jcr->JobBytes += sd->msglen;
      } /* end while */

      if (sd->msglen < 0) {
         Jmsg(jcr, M_ERROR, 0, _("Error during save reading ERR=%s\n"), ff_pkt->fname, 
	    strerror(ff_pkt->ff_errno));
      }

      /* Send data termination poll signal to Storage daemon.
       *  NOTE possibly put this poll on a counter as specified
       *  by the user to improve efficiency (i.e. poll every
       *  other file, every third file, ... 
       */
#ifndef NO_FD_SEND_TEST
#ifndef NO_POLL_TEST
      bnet_sig(sd, BNET_EOD_POLL);
      Dmsg0(30, "Send EndData_Poll\n");
      /* ***FIXME**** change to use bget_msg() */
      if (bnet_recv(sd) <= 0) {
	 close(fid);
	 return 0;
      } else {
         if (strcmp(sd->msg, "3000 OK\n") != 0) {
           Jmsg1(jcr, M_ERROR, 0, _("Job aborted by Storage daemon: %s\n"), sd->msg);
	   close(fid);
	   return 0;
	 }
      }
#else 
      bnet_sig(sd, BNET_EOD);
#endif
#endif /* NO_FD_SEND_TEST */
      close(fid);			 /* close file */
   }


   /* Terminate any MD5 signature and send it to Storage daemon and the Director */
   if (gotMD5 && ff_pkt->flags & FO_MD5) {
      MD5Final(signature, &md5c);
#ifndef NO_FD_SEND_TEST
      bnet_fsend(sd, "%ld %d 0", jcr->JobFiles, STREAM_MD5_SIGNATURE);
      Dmsg1(10, "bfiled>stored:header %s\n", sd->msg);
      memcpy(sd->msg, signature, 16);
      sd->msglen = 16;
      bnet_send(sd);
      bnet_sig(sd, BNET_EOD);	      /* end of MD5 */
#endif
      gotMD5 = 0;
   }
#ifdef really_needed
   if (ff_pkt->type == FT_DIR) {
      Jmsg(jcr, M_SAVED, -1, _("     Directory saved normally: %s\n"), ff_pkt->link);
   } else {
      Jmsg(jcr, M_SAVED, -1, _("     File saved normally: %s\n"), ff_pkt->fname);
   }
#endif
   return 1;
}
