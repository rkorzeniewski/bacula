/*
 *  Bacula File Daemon	restore.c Restorefiles.
 *
 *    Kern Sibbald, November MM
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
void do_restore(JCR *jcr, char *addr, int port)
{
   int wherelen;
   BSOCK *sd;
   char *fname; 		      /* original file name */
   char *ofile; 		      /* output name with possible prefix */
   char *lname; 		      /* link name */
   int32_t stream;
   uint32_t size;
   uint32_t VolSessionId, VolSessionTime, file_index;
   uint32_t record_file_index;
   struct stat statp;
   int extract = FALSE;
   int ofd = -1;
   int type;
   uint32_t total = 0;
   
   wherelen = strlen(jcr->where);

   sd = jcr->store_bsock;
   jcr->JobStatus = JS_Running;

   if (!bnet_set_buffer_size(sd, MAX_NETWORK_BUFFER_SIZE, BNET_SETBUF_READ)) {
      return;
   }

   fname = (char *) get_pool_memory(PM_FNAME);
   ofile = (char *) get_pool_memory(PM_FNAME);
   lname = (char *) get_pool_memory(PM_FNAME);

   /* 
    * Get a record from the Storage daemon
    */
   while (bnet_recv(sd) > 0) {
      /*
       * First we expect a Stream Record Header 
       */
      if (sscanf(sd->msg, rec_header, &VolSessionId, &VolSessionTime, &file_index,
	  &stream, &size) != 5) {
         Jmsg1(jcr, M_FATAL, 0, _("Record header scan error: %s\n"), sd->msg);
	 free_pool_memory(fname);
	 free_pool_memory(ofile);
	 free_pool_memory(lname);
	 return;
      }
      Dmsg2(30, "Got hdr: FilInx=%d Stream=%d.\n", file_index, stream);

      /* 
       * Now we expect the Stream Data
       */
      if (bnet_recv(sd) < 0) {
         Jmsg1(jcr, M_FATAL, 0, _("Data record error. ERR=%s\n"), bnet_strerror(sd));
      }
      if (size != ((uint32_t) sd->msglen)) {
         Jmsg2(jcr, M_FATAL, 0, _("Actual data size %d not same as header %d\n"), sd->msglen, size);
	 free_pool_memory(fname);
	 free_pool_memory(ofile);
	 free_pool_memory(lname);
	 return;
      }
      Dmsg1(30, "Got stream data, len=%d\n", sd->msglen);

      /* File Attributes stream */
      if (stream == STREAM_UNIX_ATTRIBUTES) {
	 char *ap, *lp;

         Dmsg1(30, "Stream=Unix Attributes. extract=%d\n", extract);
	 /* If extracting, it was from previous stream, so
	  * close the output file.
	  */
	 if (extract) {
	    if (ofd < 0) {
               Emsg0(M_ABORT, 0, _("Logic error output file should be open\n"));
	    }
	    close(ofd);
	    ofd = -1;
	    extract = FALSE;
	    set_statp(jcr, fname, ofile, lname, type, &statp);
            Dmsg0(30, "Stop extracting.\n");
	 }

	 if ((int)sizeof_pool_memory(fname) <  sd->msglen) {
	    fname = (char *) realloc_pool_memory(fname, sd->msglen + 1);
	 }
	 if (sizeof_pool_memory(ofile) < sizeof_pool_memory(fname) + wherelen + 1) {
	    ofile = (char *) realloc_pool_memory(ofile, sizeof_pool_memory(fname) + wherelen + 1);
	 }
	 if ((int)sizeof_pool_memory(lname) < sd->msglen) {
	    ofile = (char *) realloc_pool_memory(ofile, sd->msglen + 1);
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
	  *
	  */
         if (sscanf(sd->msg, "%d %d %s", &record_file_index, &type, fname) != 3) {
            Emsg1(M_FATAL, 0, _("Error scanning record header: %s\n"), sd->msg);
	    /** ****FIXME**** need to cleanup */
            Dmsg0(0, "\nError scanning header\n");
	    return;  
	 }
         Dmsg3(30, "Got Attr: FilInx=%d type=%d fname=%s\n", record_file_index,
	    type, fname);
	 if (record_file_index != file_index) {
            Emsg2(M_ABORT, 0, _("Record header file index %ld not equal record index %ld\n"),
	       file_index, record_file_index);
            Dmsg0(0, "File index error\n");
	 }
	 ap = sd->msg;
	 /* Skip to attributes */
	 while (*ap++ != 0) {
	    ;
	 }
	 /* Skip to Link name */
	 if (type == FT_LNK) {
	    lp = ap;
	    while (*lp++ != 0) {
	       ;
	    }
            strcat(lname, lp);        /* "save" link name */
	 } else {
	    *lname = 0;
	 }

	 decode_stat(ap, &statp);
	 /*
	  * Prepend the where directory so that the
	  * files are put where the user wants.
	  *
	  * We do a little jig here to handle Win32 files with
	  * a drive letter.  
	  *   If where is null and we are running on a win32 client,
	  *	 change nothing.
	  *   Otherwise, if the second character of the filename is a
	  *   colon (:), change it into a slash (/) -- this creates
	  *   a reasonable pathname on most systems.
	  */
	 if (jcr->where[0] == 0 && win32_client) {
	    strcpy(ofile, fname);
	 } else {
	    strcpy(ofile, jcr->where);
            if (fname[1] == ':') {
               fname[1] = '/';
	       strcat(ofile, fname);
               fname[1] = ':';
	    } else {
	       strcat(ofile, fname);
	    }
	 }

         Dmsg1(30, "Outfile=%s\n", ofile);
	 print_ls_output(jcr, ofile, lname, type, &statp);

	 extract = create_file(jcr, fname, ofile, lname, type, &statp, &ofd);
         Dmsg1(40, "Extract=%d\n", extract);
	 if (extract) {
	    jcr->JobFiles++;
	 }
	 jcr->num_files_examined++;

      /* Data stream */
      } else if (stream == STREAM_FILE_DATA) {
	 if (extract) {
            Dmsg2(30, "Write %d bytes, total before write=%d\n", sd->msglen, total);
	    if (write(ofd, sd->msg, sd->msglen) != sd->msglen) {
               Dmsg0(0, "===Write error===\n");
               Jmsg2(jcr, M_ERROR, 0, "Write error on %s: %s\n", ofile, strerror(errno));
	       free_pool_memory(fname);
	       free_pool_memory(ofile);
	       free_pool_memory(lname);
	       return;
	    }
	    total += sd->msglen;
	    jcr->JobBytes += sd->msglen;
	 }

      /* If extracting, wierd stream (not 1 or 2), close output file anyway */
      } else if (extract) {
         Dmsg1(30, "Found wierd stream %d\n", stream);
	 if (ofd < 0) {
            Emsg0(M_ABORT, 0, _("Logic error output file should be open\n"));
	 }
	 close(ofd);
	 ofd = -1;
	 extract = FALSE;
	 set_statp(jcr, fname, ofile, lname, type, &statp);
      } else if (stream != STREAM_MD5_SIGNATURE) {
         Dmsg2(0, "None of above!!! stream=%d data=%s\n", stream,sd->msg);
      }
   }

   /* If output file is still open, it was the last one in the
    * archive since we just hit an end of file, so close the file. 
    */
   if (ofd >= 0) {
      close(ofd);
      set_statp(jcr, fname, ofile, lname, type, &statp);
   }

   free_pool_memory(fname);
   free_pool_memory(ofile);
   free_pool_memory(lname);
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
   /* ********FIXME******** make memory pool */
   char buf[1000]; 
   char *p, *f;
   int n;

   p = encode_mode(statp->st_mode, buf);
   n = sprintf(p, "  %2d ", (uint32_t)statp->st_nlink);
   p += n;
   n = sprintf(p, "%-8.8s %-8.8s", getuser(statp->st_uid), getgroup(statp->st_gid));
   p += n;
   n = sprintf(p, "%8" lld " ", (uint64_t)statp->st_size);
   p += n;
   p = encode_time(statp->st_ctime, p);
   *p++ = ' ';
   *p++ = ' ';
   for (f=fname; *f; )
      *p++ = *f++;
   if (type == FT_LNK) {
      *p++ = ' ';
      *p++ = '-';
      *p++ = '>';
      *p++ = ' ';
      /* Copy link name */
      for (f=lname; *f; )
	 *p++ = *f++;
   }
   *p++ = '\n';
   *p = 0;
   Dmsg0(20, buf);
   Jmsg(jcr, M_INFO, 0, buf);
}
