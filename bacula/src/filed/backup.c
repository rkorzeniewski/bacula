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
   Copyright (C) 2000-2003 Kern Sibbald and John Walker

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

   set_jcr_job_status(jcr, JS_Running);

   Dmsg1(110, "bfiled: opened data connection %d to stored\n", sd->fd);

   if (!bnet_set_buffer_size(sd, MAX_NETWORK_BUFFER_SIZE, BNET_SETBUF_WRITE)) {
      return 0;
   }

   jcr->buf_size = sd->msglen;		   
   /* Adjust for compression so that output buffer is
    * 12 bytes + 0.1% larger than input buffer plus 18 bytes.
    * This gives a bit extra plus room for the sparse addr if any.
    * Note, we adjust the read size to be smaller so that the
    * same output buffer can be used without growing it.
    */
   jcr->compress_buf_size = jcr->buf_size + ((jcr->buf_size+999) / 1000) + 30;
   jcr->compress_buf = get_memory(jcr->compress_buf_size);

   Dmsg1(100, "set_find_options ff=%p\n", jcr->ff);
   set_find_options((FF_PKT *)jcr->ff, jcr->incremental, jcr->mtime);
   Dmsg0(110, "start find files\n");

   /* Subroutine save_file() is called for each file */
   if (!find_files(jcr, (FF_PKT *)jcr->ff, save_file, (void *)jcr)) {
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
   char attribsEx[MAXSTRING];
   int stat, stream; 
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
      Dmsg2(130, "FT_LNKSAVED hard link: %s => %s\n", ff_pkt->fname, ff_pkt->link);
      break;
   case FT_REGE:
      Dmsg1(130, "FT_REGE saving: %s\n", ff_pkt->fname);
      break;
   case FT_REG:
      Dmsg1(130, "FT_REG saving: %s\n", ff_pkt->fname);
      break;
   case FT_LNK:
      Dmsg2(130, "FT_LNK saving: %s -> %s\n", ff_pkt->fname, ff_pkt->link);
      break;
   case FT_DIR:
      Dmsg1(130, "FT_DIR saving: %s\n", ff_pkt->link);
      break;
   case FT_SPEC:
      Dmsg1(130, "FT_SPEC saving: %s\n", ff_pkt->fname);
      break;
   case FT_RAW:
      Dmsg1(130, "FT_RAW saving: %s\n", ff_pkt->fname);
      break;
   case FT_FIFO:
      Dmsg1(130, "FT_FIFO saving: %s\n", ff_pkt->fname);
      break;
   case FT_NOACCESS:
      Jmsg(jcr, M_NOTSAVED, -1, _("     Could not access %s: ERR=%s\n"), ff_pkt->fname, 
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
      Jmsg(jcr, M_NOTSAVED, 0,  _("     Unknown file type %d; not saved: %s\n"), ff_pkt->type, ff_pkt->fname);
      return 1;
   }

   if (ff_pkt->type != FT_LNKSAVED && (S_ISREG(ff_pkt->statp.st_mode) && 
	 ff_pkt->statp.st_size > 0) || 
	 ff_pkt->type == FT_RAW || ff_pkt->type == FT_FIFO) {
      if ((ff_pkt->fid = open(ff_pkt->fname, O_RDONLY | O_BINARY)) < 0) {
	 ff_pkt->ff_errno = errno;
         Jmsg(jcr, M_NOTSAVED, -1, _("     Cannot open %s: ERR=%s.\n"), ff_pkt->fname, strerror(ff_pkt->ff_errno));
	 return 1;
      }
   } else {
      ff_pkt->fid = -1;
   }

   Dmsg1(130, "bfiled: sending %s to stored\n", ff_pkt->fname);
   encode_stat(attribs, &ff_pkt->statp);
   stream = encode_attribsEx(jcr, attribsEx, ff_pkt);
   Dmsg3(200, "File %s\nattribs=%s\nattribsEx=%s\n", ff_pkt->fname, attribs, attribsEx);
     
   P(jcr->mutex);
   jcr->JobFiles++;		       /* increment number of files sent */
   pm_strcpy(&jcr->last_fname, ff_pkt->fname);
   V(jcr->mutex);
    
   /*
    * Send Attributes header to Storage daemon
    *	 <file-index> <stream> <info>
    */
#ifndef NO_FD_SEND_TEST
   if (!bnet_fsend(sd, "%ld %d 0", jcr->JobFiles, stream)) {
      if (ff_pkt->fid >= 0) {
	 close(ff_pkt->fid);
      }
      return 0;
   }
   Dmsg1(100, ">stored: attrhdr %s\n", sd->msg);

   /*
    * Send file attributes to Storage daemon
    *	File_index
    *	File type
    *	Filename (full path)
    *	Encoded attributes
    *	Link name (if type==FT_LNK or FT_LNKSAVED)
    *	Encoded extended-attributes (for Win32)
    *
    * For a directory, link is the same as fname, but with trailing
    * slash. For a linked file, link is the link.
    */
   if (ff_pkt->type == FT_LNK || ff_pkt->type == FT_LNKSAVED) {
      Dmsg2(100, "Link %s to %s\n", ff_pkt->fname, ff_pkt->link);
      stat = bnet_fsend(sd, "%ld %d %s%c%s%c%s%c%s%c", jcr->JobFiles, 
	       ff_pkt->type, ff_pkt->fname, 0, attribs, 0, ff_pkt->link, 0,
	       attribsEx, 0);
   } else if (ff_pkt->type == FT_DIR) {
      /* Here link is the canonical filename (i.e. with trailing slash) */
      stat = bnet_fsend(sd, "%ld %d %s%c%s%c%c%s%c", jcr->JobFiles, 
	       ff_pkt->type, ff_pkt->link, 0, attribs, 0, 0, attribsEx, 0);
   } else {
      stat = bnet_fsend(sd, "%ld %d %s%c%s%c%c%s%c", jcr->JobFiles, 
	       ff_pkt->type, ff_pkt->fname, 0, attribs, 0, 0, attribsEx, 0);
   }

   Dmsg2(100, ">stored: attr len=%d: %s\n", sd->msglen, sd->msg);
   if (!stat) {
      if (ff_pkt->fid >= 0) {
	 close(ff_pkt->fid);
      }
      return 0;
   }
   bnet_sig(sd, BNET_EOD);	      /* indicate end of attributes data */
#endif

   /* 
    * If the file has data, read it and send to the Storage daemon
    *
    */
   if (ff_pkt->fid >= 0) {
      uint64_t fileAddr = 0;	      /* file address */
      char *rbuf, *wbuf;
      int rsize = jcr->buf_size;      /* read buffer size */

      msgsave = sd->msg;
      rbuf = sd->msg;		      /* read buffer */ 	    
      wbuf = sd->msg;		      /* write buffer */

      Dmsg1(100, "Saving data, type=%d\n", ff_pkt->type);

      if (ff_pkt->flags & FO_SPARSE) {
	 stream = STREAM_SPARSE_DATA;
      } else {
	 stream = STREAM_FILE_DATA;
      }

#ifdef HAVE_LIBZ
      uLong compress_len, max_compress_len = 0;
      const Bytef *cbuf = NULL;

      if (ff_pkt->flags & FO_GZIP) {
	 if (stream == STREAM_FILE_DATA) {
	    stream = STREAM_GZIP_DATA;
	 } else {
	    stream = STREAM_SPARSE_GZIP_DATA;
	 }

	 if (ff_pkt->flags & FO_SPARSE) {
	    cbuf = (Bytef *)jcr->compress_buf + SPARSE_FADDR_SIZE;
	    max_compress_len = jcr->compress_buf_size - SPARSE_FADDR_SIZE;
	 } else {
	    cbuf = (Bytef *)jcr->compress_buf;
	    max_compress_len = jcr->compress_buf_size; /* set max length */
	 }
	 wbuf = jcr->compress_buf;    /* compressed output here */
      }
#endif

#ifndef NO_FD_SEND_TEST
      /*
       * Send Data header to Storage daemon
       *    <file-index> <stream> <info>
       */
      if (!bnet_fsend(sd, "%ld %d 0", jcr->JobFiles, stream)) {
	 close(ff_pkt->fid);
	 return 0;
      }
      Dmsg1(100, ">stored: datahdr %s\n", sd->msg);
#endif

      if (ff_pkt->flags & FO_MD5) {
	 MD5Init(&md5c);
      }

      /*
       * Make space at beginning of buffer for fileAddr because this
       *   same buffer will be used for writing if compression if off. 
       */
      if (ff_pkt->flags & FO_SPARSE) {
	 rbuf += SPARSE_FADDR_SIZE;
	 rsize -= SPARSE_FADDR_SIZE;
      }

      /* 
       * Read the file data
       */
      while ((sd->msglen=read(ff_pkt->fid, rbuf, rsize)) > 0) {
	 int sparseBlock = 0;

	 /* Check for sparse blocks */
	 if (ff_pkt->flags & FO_SPARSE) {
	    ser_declare;
	    if (sd->msglen == rsize && 
		(fileAddr+sd->msglen < (uint64_t)ff_pkt->statp.st_size)) {
	       sparseBlock = is_buf_zero(rbuf, rsize);
	    }
	       
	    ser_begin(wbuf, SPARSE_FADDR_SIZE);
	    ser_uint64(fileAddr);     /* store fileAddr in begin of buffer */
	 } 

	 jcr->ReadBytes += sd->msglen;	    /* count bytes read */
	 fileAddr += sd->msglen;

	 /* Update MD5 if requested */
	 if (ff_pkt->flags & FO_MD5) {
	    MD5Update(&md5c, (unsigned char *)rbuf, sd->msglen);
	    gotMD5 = 1;
	 }

#ifdef HAVE_LIBZ
	 /* Do compression if turned on */
	 if (!sparseBlock && ff_pkt->flags & FO_GZIP) {
	    int zstat;
	    compress_len = max_compress_len;
            Dmsg4(400, "cbuf=0x%x len=%u rbuf=0x%x len=%u\n", cbuf, compress_len,
	       rbuf, sd->msglen);
	    /* NOTE! This call modifies compress_len !!! */
	    if ((zstat=compress2((Bytef *)cbuf, &compress_len, 
		  (const Bytef *)rbuf, (uLong)sd->msglen,
		  ff_pkt->GZIP_level)) != Z_OK) {
               Jmsg(jcr, M_FATAL, 0, _("Compression error: %d\n"), zstat);
	       sd->msg = msgsave;
	       sd->msglen = 0;
	       close(ff_pkt->fid);
	       return 0;
	    }
            Dmsg2(400, "compressed len=%d uncompressed len=%d\n", 
	       compress_len, sd->msglen);

	    sd->msglen = compress_len;	 /* set compressed length */
	 }
#endif

#ifndef NO_FD_SEND_TEST
	 /* Send the buffer to the Storage daemon */
	 if (!sparseBlock) {
	    if (ff_pkt->flags & FO_SPARSE) {
	       sd->msglen += SPARSE_FADDR_SIZE; /* include fileAddr in size */
	    }
	    sd->msg = wbuf;	      /* set correct write buffer */
	    if (!bnet_send(sd)) {
	       sd->msg = msgsave;     /* restore read buffer */
	       sd->msglen = 0;
	       close(ff_pkt->fid);
	       return 0;
	    }
	 }
         Dmsg1(130, "Send data to FD len=%d\n", sd->msglen);
#endif
	 jcr->JobBytes += sd->msglen;	/* count bytes saved possibly compressed */
	 sd->msg = msgsave;		/* restore read buffer */

      } /* end while read file data */

      if (sd->msglen < 0) {
         Jmsg(jcr, M_ERROR, 0, _("Read error on file %s. ERR=%s\n"),
	    ff_pkt->fname, strerror(errno));
      }

#ifndef NO_FD_SEND_TEST
      bnet_sig(sd, BNET_EOD);	      /* indicate end of file data */
#endif /* NO_FD_SEND_TEST */
      close(ff_pkt->fid);	      /* close file */
   }


   /* Terminate any MD5 signature and send it to Storage daemon and the Director */
   if (gotMD5 && ff_pkt->flags & FO_MD5) {
      MD5Final(signature, &md5c);
#ifndef NO_FD_SEND_TEST
      bnet_fsend(sd, "%ld %d 0", jcr->JobFiles, STREAM_MD5_SIGNATURE);
      Dmsg1(100, "bfiled>stored:header %s\n", sd->msg);
      memcpy(sd->msg, signature, 16);
      sd->msglen = 16;
      bnet_send(sd);
      bnet_sig(sd, BNET_EOD);	      /* end of MD5 */
#endif
      gotMD5 = 0;
   }
   return 1;
}
