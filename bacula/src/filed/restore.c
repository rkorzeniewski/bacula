/*
 *  Bacula File Daemon	restore.c Restorefiles.
 *
 *    Kern Sibbald, November MM
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

/* Data received from Storage Daemon */
static char rec_header[] = "rechdr %ld %ld %ld %ld %ld";

/* Forward referenced functions */
static void print_ls_output(JCR *jcr, char *fname, char *lname, int type, struct stat *statp);

#define RETRY 10		      /* retry wait time */

/* 
 * Restore the requested files.
 * 
 */
void do_restore(JCR *jcr)
{
   int wherelen;
   BSOCK *sd;
   POOLMEM *fname;		      /* original file name */
   POOLMEM *ofile;		      /* output name with possible prefix */
   POOLMEM *lname;		      /* link name with possible prefix */
   POOLMEM *attribsEx;		      /* Extended attributes (Win32) */
   int32_t stream;
   uint32_t size;
   uint32_t VolSessionId, VolSessionTime, file_index;
   uint32_t record_file_index;
   struct stat statp;
   int extract = FALSE;
   int ofd = -1;
   int type, stat;
   uint32_t total = 0;		      /* Job total but only 32 bits for debug */
   char *wbuf;			      /* write buffer */
   uint32_t wsize;		      /* write size */
   uint64_t fileAddr = 0;	      /* file write address */
   
   wherelen = strlen(jcr->where);

   sd = jcr->store_bsock;
   jcr->JobStatus = JS_Running;

   if (!bnet_set_buffer_size(sd, MAX_NETWORK_BUFFER_SIZE, BNET_SETBUF_READ)) {
      return;
   }
   jcr->buf_size = sd->msglen;

   fname = get_pool_memory(PM_FNAME);
   ofile = get_pool_memory(PM_FNAME);
   lname = get_pool_memory(PM_FNAME);
   attribsEx = get_pool_memory(PM_FNAME);

#ifdef HAVE_LIBZ
   uint32_t compress_buf_size = jcr->buf_size + 12 + ((jcr->buf_size+999) / 1000) + 100;
   jcr->compress_buf = (char *)bmalloc(compress_buf_size);
#endif

   /* 
    * Get a record from the Storage daemon. We are guaranteed to 
    *	receive records in the following order:
    *	1. Stream record header
    *	2. Stream data
    *	     a. Attributes (Unix or Win32)
    *	 or  b. File data for the file
    *	 or  c. Possibly MD5 record
    *	3. Repeat step 1
    */
   while (bnet_recv(sd) >= 0 && !job_cancelled(jcr)) {
      /*
       * First we expect a Stream Record Header 
       */
      if (sscanf(sd->msg, rec_header, &VolSessionId, &VolSessionTime, &file_index,
	  &stream, &size) != 5) {
         Jmsg1(jcr, M_FATAL, 0, _("Record header scan error: %s\n"), sd->msg);
	 goto bail_out;
      }
      Dmsg2(30, "Got hdr: FilInx=%d Stream=%d.\n", file_index, stream);

      /* 
       * Now we expect the Stream Data
       */
      if (bnet_recv(sd) < 0 && !job_cancelled(jcr)) {
         Jmsg1(jcr, M_FATAL, 0, _("Data record error. ERR=%s\n"), bnet_strerror(sd));
      }
      if (size != (uint32_t)sd->msglen) {
         Jmsg2(jcr, M_FATAL, 0, _("Actual data size %d not same as header %d\n"), sd->msglen, size);
	 goto bail_out;
      }
      Dmsg1(30, "Got stream data, len=%d\n", sd->msglen);

      /* File Attributes stream */
      if (stream == STREAM_UNIX_ATTRIBUTES || stream == STREAM_WIN32_ATTRIBUTES) {
	 char *ap, *lp, *fp, *apex;

         Dmsg1(30, "Stream=Unix Attributes. extract=%d\n", extract);
	 /* If extracting, it was from previous stream, so
	  * close the output file.
	  */
	 if (extract) {
	    if (ofd < 0) {
               Emsg0(M_ERROR, 0, _("Logic error output file should be open\n"));
	    }
	    set_attributes(jcr, fname, ofile, lname, type, stream, 
			   &statp, attribsEx, &ofd);
	    extract = FALSE;
            Dmsg0(30, "Stop extracting.\n");
	 }

	 if ((int)sizeof_pool_memory(fname) <  sd->msglen) {
	    fname = realloc_pool_memory(fname, sd->msglen + 1);
	 }
	 if ((int)sizeof_pool_memory(ofile) < sd->msglen + wherelen + 1) {
	    ofile = realloc_pool_memory(ofile, sd->msglen + wherelen + 1);
	 }
	 if ((int)sizeof_pool_memory(lname) < sd->msglen + wherelen + 1) {
	    lname = realloc_pool_memory(lname, sd->msglen + wherelen + 1);
	 }
	 *fname = 0;
	 *lname = 0;

	 /*		 
	  * An Attributes record consists of:
	  *    File_index
	  *    Type   (FT_types)
	  *    Filename
	  *    Attributes
	  *    Link name (if file linked i.e. FT_LNK)
	  *    Extended attributes (Win32)
	  *
	  */
         Dmsg1(100, "Attr: %s\n", sd->msg);
         if (sscanf(sd->msg, "%d %d", &record_file_index, &type) != 2) {
            Jmsg(jcr, M_FATAL, 0, _("Error scanning attributes: %s\n"), sd->msg);
            Dmsg1(100, "\nError scanning attributes. %s\n", sd->msg);
	    goto bail_out;
	 }
         Dmsg2(100, "Got Attr: FilInx=%d type=%d\n", record_file_index, type);
	 if (record_file_index != file_index) {
            Jmsg(jcr, M_FATAL, 0, _("Record header file index %ld not equal record index %ld\n"),
	       file_index, record_file_index);
            Dmsg0(100, "File index error\n");
	    goto bail_out;
	 }
	 ap = sd->msg;
         while (*ap++ != ' ')         /* skip record file index */
	    ;
         while (*ap++ != ' ')         /* skip type */
	    ;
	 /* Save filename and position to attributes */
	 fp = fname;
	 while (*ap != 0) {
	    *fp++  = *ap++;	      /* copy filename to fname */
	 }
	 *fp = *ap++;		      /* terminate filename & point to attribs */

	 /* Skip to Link name */
	 if (type == FT_LNK || type == FT_LNKSAVED) {
	    lp = ap;
	    while (*lp++ != 0) {
	       ;
	    }
	 } else {
            lp = "";
	 }

	 if (stream == STREAM_WIN32_ATTRIBUTES) {
	    apex = ap;			 /* start at attributes */
	    while (*apex++ != 0) {	 /* skip attributes */
	       ;
	    }
	    while (*apex++ != 0) {	 /* skip link name */
	       ;
	    }
	    pm_strcpy(&attribsEx, apex); /* make a copy */
	 } else {
	    *attribsEx = 0;		 /* no extended attributes */
	 }

         Dmsg3(200, "File %s\nattrib=%s\nattribsEx=%s\n", fname, ap, attribsEx);

	 decode_stat(ap, &statp);
	 /*
	  * Prepend the where directory so that the
	  * files are put where the user wants.
	  *
	  * We do a little jig here to handle Win32 files with
	  *   a drive letter -- we simply strip the drive: from
	  *   every filename if a prefix is supplied.
	  *	
	  */
	 if (jcr->where[0] == 0) {
	    strcpy(ofile, fname);
	    strcpy(lname, lp);
	 } else {
	    char *fn;
	    strcpy(ofile, jcr->where);	/* copy prefix */
            if (win32_client && fname[1] == ':') {
	       fn = fname+2;	      /* skip over drive: */
	    } else {
	       fn = fname;	      /* take whole name */
	    }
	    /* Ensure where is terminated with a slash */
            if (jcr->where[wherelen-1] != '/' && fn[0] != '/') {
               strcat(ofile, "/");
	    }
	    strcat(ofile, fn);	      /* copy rest of name */
	    /* Fixup link name */
	    if (type == FT_LNK || type == FT_LNKSAVED) {
               if (lp[0] == '/') {      /* if absolute path */
		  strcpy(lname, jcr->where);
	       }       
               if (win32_client && lp[1] == ':') {
		  strcat(lname, lp+2); /* copy rest of name */
	       } else {
		  strcat(lname, lp);   /* On Unix systems we take everything */
	       }
	    }
	 }

         Dmsg1(30, "Outfile=%s\n", ofile);

	 extract = FALSE;
	 stat = create_file(jcr, fname, ofile, lname, type, 
			    stream, &statp, attribsEx, &ofd, jcr->replace);
	 switch (stat) {
	 case CF_ERROR:
	 case CF_SKIP:
	    break;
	 case CF_EXTRACT:
	    extract = TRUE;
	    pm_strcpy(&jcr->last_fname, ofile);
	    /* Fall-through wanted */
	 case CF_CREATED:
	    jcr->JobFiles++;
	    fileAddr = 0;
	    print_ls_output(jcr, ofile, lname, type, &statp);
	    break;
	 }  

	 jcr->num_files_examined++;

      /* Data stream */
      } else if (stream == STREAM_FILE_DATA || stream == STREAM_SPARSE_DATA) {
	 if (extract) {
	    if (stream == STREAM_SPARSE_DATA) {
	       ser_declare;
	       uint64_t faddr;
	       char ec1[50];

	       wbuf = sd->msg + SPARSE_FADDR_SIZE;
	       wsize = sd->msglen - SPARSE_FADDR_SIZE;
	       ser_begin(sd->msg, SPARSE_FADDR_SIZE);
	       unser_uint64(faddr);
	       if (fileAddr != faddr) {
		  fileAddr = faddr;
		  if (lseek(ofd, (off_t)fileAddr, SEEK_SET) < 0) {
                     Jmsg3(jcr, M_ERROR, 0, _("Seek to %s error on %s: ERR=%s\n"),
			 edit_uint64(fileAddr, ec1), ofile, strerror(errno));
		     goto bail_out;
		  }
	       }
	    } else {
	       wbuf = sd->msg;
	       wsize = sd->msglen;
	    }
            Dmsg2(30, "Write %u bytes, total before write=%u\n", wsize, total);
	    if ((uint32_t)write(ofd, wbuf, wsize) != wsize) {
               Dmsg0(0, "===Write error===\n");
               Jmsg2(jcr, M_ERROR, 0, _("Write error on %s: %s\n"), ofile, strerror(errno));
	       goto bail_out;
	    }
	    total += wsize;
	    jcr->JobBytes += wsize;
	    fileAddr += wsize;
	 }
	
      /* GZIP data stream */
      } else if (stream == STREAM_GZIP_DATA || stream == STREAM_SPARSE_GZIP_DATA) {
#ifdef HAVE_LIBZ
	 if (extract) {
	    ser_declare;
	    uLong compress_len;
	    uint64_t faddr;
	    char ec1[50];
	    int stat;

	    if (stream == STREAM_SPARSE_GZIP_DATA) {
	       wbuf = sd->msg + SPARSE_FADDR_SIZE;
	       wsize = sd->msglen - SPARSE_FADDR_SIZE;
	       ser_begin(sd->msg, SPARSE_FADDR_SIZE);
	       unser_uint64(faddr);
	       if (fileAddr != faddr) {
		  fileAddr = faddr;
		  if (lseek(ofd, (off_t)fileAddr, SEEK_SET) < 0) {
                     Jmsg3(jcr, M_ERROR, 0, _("Seek to %s error on %s: ERR=%s\n"),
			 edit_uint64(fileAddr, ec1), ofile, strerror(errno));
		     goto bail_out;
		  }
	       }
	    } else {
	       wbuf = sd->msg;
	       wsize = sd->msglen;
	    }
	    compress_len = compress_buf_size;
            Dmsg2(100, "Comp_len=%d msglen=%d\n", compress_len, wsize);
	    if ((stat=uncompress((Byte *)jcr->compress_buf, &compress_len, 
		  (const Byte *)wbuf, (uLong)wsize)) != Z_OK) {
               Jmsg(jcr, M_ERROR, 0, _("Uncompression error. ERR=%d\n"), stat);
	       goto bail_out;
	    }

            Dmsg2(100, "Write uncompressed %d bytes, total before write=%d\n", compress_len, total);
	    if ((uLong)write(ofd, jcr->compress_buf, compress_len) != compress_len) {
               Dmsg0(0, "===Write error===\n");
               Jmsg2(jcr, M_ERROR, 0, "Write error on %s: %s\n", ofile, strerror(errno));
	       goto bail_out;
	    }
	    total += compress_len;
	    jcr->JobBytes += compress_len;
	    fileAddr += compress_len;
	 }
#else
	 if (extract) {
            Jmsg(jcr, M_ERROR, 0, "GZIP data stream found, but GZIP not configured!\n");
	    goto bail_out;
	 }
#endif
      /* If extracting, wierd stream (not 1 or 2), close output file anyway */
      } else if (extract) {
         Dmsg1(30, "Found wierd stream %d\n", stream);
	 if (ofd < 0) {
            Emsg0(M_ERROR, 0, _("Logic error output file should be open\n"));
	 }
	 set_attributes(jcr, fname, ofile, lname, type, stream, 
			&statp, attribsEx, &ofd);
	 extract = FALSE;
      } else if (stream != STREAM_MD5_SIGNATURE) {
         Dmsg2(0, "None of above!!! stream=%d data=%s\n", stream,sd->msg);
      }
   }

   /* If output file is still open, it was the last one in the
    * archive since we just hit an end of file, so close the file. 
    */
   if (ofd >= 0) {
      set_attributes(jcr, fname, ofile, lname, type, stream, 
		     &statp, attribsEx, &ofd);
   }
   jcr->JobStatus = JS_Terminated;
   goto ok_out;

bail_out:
   jcr->JobStatus = JS_ErrorTerminated;
ok_out:
   if (jcr->compress_buf) {
      free(jcr->compress_buf);
      jcr->compress_buf = NULL;
   }
   free_pool_memory(fname);
   free_pool_memory(ofile);
   free_pool_memory(lname);
   free_pool_memory(attribsEx);
   Dmsg2(10, "End Do Restore. Files=%d Bytes=%" lld "\n", jcr->JobFiles,
      jcr->JobBytes);
}	   

extern char *getuser(uid_t uid);
extern char *getgroup(gid_t gid);

/*
 * Print an ls style message, also send INFO
 */
static void print_ls_output(JCR *jcr, char *fname, char *lname, int type, struct stat *statp)
{
   char buf[2000]; 
   char ec1[30];
   char *p, *f;
   int n;

   p = encode_mode(statp->st_mode, buf);
   n = sprintf(p, "  %2d ", (uint32_t)statp->st_nlink);
   p += n;
   n = sprintf(p, "%-8.8s %-8.8s", getuser(statp->st_uid), getgroup(statp->st_gid));
   p += n;
   n = sprintf(p, "%8.8s ", edit_uint64(statp->st_size, ec1));
   p += n;
   p = encode_time(statp->st_ctime, p);
   *p++ = ' ';
   *p++ = ' ';
   for (f=fname; *f && (p-buf) < (int)sizeof(buf); )
      *p++ = *f++;
   if (type == FT_LNK) {
      *p++ = ' ';
      *p++ = '-';
      *p++ = '>';
      *p++ = ' ';
      /* Copy link name */
      for (f=lname; *f && (p-buf) < (int)sizeof(buf); )
	 *p++ = *f++;
   }
   *p++ = '\n';
   *p = 0;
   Dmsg0(20, buf);
   Jmsg(jcr, M_INFO, 0, buf);
}
