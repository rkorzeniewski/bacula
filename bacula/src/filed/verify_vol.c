/*
 *  Bacula File Daemon	verify-vol.c Verify files on a Volume
 *    versus attributes in Catalog
 *
 *    Kern Sibbald, July MMII
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

/* Data received from Storage Daemon */
static char rec_header[] = "rechdr %ld %ld %ld %ld %ld";

/* Forward referenced functions */
#ifdef needed
static void print_ls_output(JCR *jcr, char *fname, char *lname, int type, struct stat *statp);
#endif


/* 
 * Verify attributes of the requested files on the Volume
 * 
 */
void do_verify_volume(JCR *jcr)
{
   BSOCK *sd, *dir;
   POOLMEM *fname;		      /* original file name */
   POOLMEM *lname;		      /* link name */
   int32_t stream;
   uint32_t size;
   uint32_t VolSessionId, VolSessionTime, file_index;
   uint32_t record_file_index;
   int type, stat;
   
   sd = jcr->store_bsock;
   if (!sd) {
      Jmsg(jcr, M_FATAL, 0, _("Storage command not issued before Verify.\n"));
      set_jcr_job_status(jcr, JS_FatalError);
      return;
   }
   dir = jcr->dir_bsock;
   set_jcr_job_status(jcr, JS_Running);

   if (!bnet_set_buffer_size(sd, MAX_NETWORK_BUFFER_SIZE, BNET_SETBUF_READ)) {
      set_jcr_job_status(jcr, JS_FatalError);
      return;
   }
   jcr->buf_size = sd->msglen;

   fname = get_pool_memory(PM_FNAME);
   lname = get_pool_memory(PM_FNAME);

   /* 
    * Get a record from the Storage daemon
    */
   while (bget_msg(sd) >= 0 && !job_canceled(jcr)) {
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
      if (bget_msg(sd) < 0) {
         Jmsg1(jcr, M_FATAL, 0, _("Data record error. ERR=%s\n"), bnet_strerror(sd));
	 goto bail_out;
      }
      if (size != ((uint32_t)sd->msglen)) {
         Jmsg2(jcr, M_FATAL, 0, _("Actual data size %d not same as header %d\n"), sd->msglen, size);
	 goto bail_out;
      }
      Dmsg1(30, "Got stream data, len=%d\n", sd->msglen);

      /* File Attributes stream */
      if (stream == STREAM_UNIX_ATTRIBUTES || stream == STREAM_WIN32_ATTRIBUTES) {
	 char *ap, *lp, *fp;

         Dmsg0(400, "Stream=Unix Attributes.\n");

	 if ((int)sizeof_pool_memory(fname) < sd->msglen) {
	    fname = realloc_pool_memory(fname, sd->msglen + 1);
	 }

	 if ((int)sizeof_pool_memory(lname) < sd->msglen) {
	    lname = realloc_pool_memory(lname, sd->msglen + 1);
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
	  *    Extended Attributes (if Win32)
	  */
         if (sscanf(sd->msg, "%d %d", &record_file_index, &type) != 2) {
            Jmsg(jcr, M_FATAL, 0, _("Error scanning record header: %s\n"), sd->msg);
            Dmsg0(0, "\nError scanning header\n");
	    goto bail_out;
	 }
         Dmsg2(30, "Got Attr: FilInx=%d type=%d\n", record_file_index, type);
	 if (record_file_index != file_index) {
            Jmsg(jcr, M_FATAL, 0, _("Record header file index %ld not equal record index %ld\n"),
	       file_index, record_file_index);
            Dmsg0(0, "File index error\n");
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

         Dmsg1(200, "Attr=%s\n", ap);
	 /* Skip to Link name */
	 if (type == FT_LNK || type == FT_LNKSAVED) {
	    lp = ap;
	    while (*lp++ != 0) {
	       ;
	    }
            strcat(lname, lp);        /* "save" link name */
	 } else {
	    *lname = 0;
	 }
	 P(jcr->mutex);
	 jcr->JobFiles++;
	 jcr->num_files_examined++;
	 pm_strcpy(&jcr->last_fname, fname); /* last file examined */
	 V(jcr->mutex);

	 /* 
	  * Send file attributes to Director
	  *   File_index
	  *   Stream
	  *   Verify Options
	  *   Filename (full path)
	  *   Encoded attributes
	  *   Link name (if type==FT_LNK)
	  * For a directory, link is the same as fname, but with trailing
	  * slash. For a linked file, link is the link.
	  */
	 /* Send file attributes to Director */
         Dmsg2(200, "send ATTR inx=%d fname=%s\n", jcr->JobFiles, fname);
	 if (type == FT_LNK || type == FT_LNKSAVED) {
            stat = bnet_fsend(dir, "%d %d %s %s%c%s%c%s%c", jcr->JobFiles,
                          STREAM_UNIX_ATTRIBUTES, "pinsug5", fname, 
			  0, ap, 0, lname, 0);
	 } else {
            stat = bnet_fsend(dir,"%d %d %s %s%c%s%c%c", jcr->JobFiles,
                          STREAM_UNIX_ATTRIBUTES, "pinsug5", fname, 
			  0, ap, 0, 0);
	 }
         Dmsg2(200, "bfiled>bdird: attribs len=%d: msg=%s\n", dir->msglen, dir->msg);
	 if (!stat) {
            Jmsg(jcr, M_FATAL, 0, _("Network error in send to Director: ERR=%s\n"), bnet_strerror(dir));
	    goto bail_out;
	 }


      /* Data stream */
      } else if (stream == STREAM_FILE_DATA || stream == STREAM_SPARSE_DATA) {

	/* Do nothing */
	
      /* GZIP data stream */
      } else if (stream == STREAM_GZIP_DATA || stream == STREAM_SPARSE_GZIP_DATA) {

	/* Do nothing */

      } else if (stream == STREAM_WIN32_DATA || stream == STREAM_WIN32_GZIP_DATA) {

	/* Do nothing */

      /* If MD5 stream */
      } else if (stream == STREAM_MD5_SIGNATURE) {
	 char MD5buf[30];
	 bin_to_base64(MD5buf, (char *)sd->msg, 16); /* encode 16 bytes */
         Dmsg2(400, "send inx=%d MD5=%s\n", jcr->JobFiles, MD5buf);
         bnet_fsend(dir, "%d %d %s *MD5-%d*", jcr->JobFiles, STREAM_MD5_SIGNATURE, MD5buf,
	    jcr->JobFiles);
         Dmsg2(20, "bfiled>bdird: MD5 len=%d: msg=%s\n", dir->msglen, dir->msg);
  
      /* If SHA1 stream */
      } else if (stream == STREAM_SHA1_SIGNATURE) {
	 char SHA1buf[30];
	 bin_to_base64(SHA1buf, (char *)sd->msg, 20); /* encode 20 bytes */
         Dmsg2(400, "send inx=%d SHA1=%s\n", jcr->JobFiles, SHA1buf);
         bnet_fsend(dir, "%d %d %s *SHA1-%d*", jcr->JobFiles, STREAM_SHA1_SIGNATURE, 
	    SHA1buf, jcr->JobFiles);
         Dmsg2(20, "bfiled>bdird: SHA1 len=%d: msg=%s\n", dir->msglen, dir->msg);
      } else {
         Pmsg2(0, "None of above!!! stream=%d data=%s\n", stream,sd->msg);
      }
   }
   set_jcr_job_status(jcr, JS_Terminated);
   goto ok_out;

bail_out:
   set_jcr_job_status(jcr, JS_ErrorTerminated);
ok_out:
   if (jcr->compress_buf) {
      free(jcr->compress_buf);
      jcr->compress_buf = NULL;
   }
   free_pool_memory(fname);
   free_pool_memory(lname);
   Dmsg2(050, "End Verify-Vol. Files=%d Bytes=%" lld "\n", jcr->JobFiles,
      jcr->JobBytes);
}	   

extern char *getuser(uid_t uid);
extern char *getgroup(gid_t gid);

/*
 * Print an ls style message, also send INFO
 */
#ifdef needed
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
   for (f=fname; *f && (p-buf) < (int)sizeof(buf)-10; )
      *p++ = *f++;
   if (type == FT_LNK) {
      *p++ = ' ';
      *p++ = '-';
      *p++ = '>';
      *p++ = ' ';
      /* Copy link name */
      for (f=lname; *f && (p-buf) < (int)sizeof(buf)-10; )
	 *p++ = *f++;
   }
   *p++ = '\n';
   *p = 0;
   Dmsg0(20, buf);
   Jmsg(jcr, M_INFO, 0, "%s", buf);
}
#endif
