/*
 * Append code for Storage daemon
 *  Kern Sibbald, May MM
 *
 *  Version $Id$
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
#include "stored.h"

extern int  FiledDataChan;	      /* File daemon data channel (port) */
extern int BaculaTapeVersion;	      /* Version number */
extern char BaculaId[]; 	      /* Id string */

/* Responses sent to the File daemon */
static char OK_data[]    = "3000 OK data\n";

/* 
 *  Append Data sent from File daemon	
 *
 */
int do_append_data(JCR *jcr) 
{
   int32_t n;
   long file_index, stream, last_file_index;
   BSOCK *ds;
   BSOCK *fd_sock = jcr->file_bsock;
   int ok = TRUE;
   DEVICE *dev = jcr->device->dev;
   DEV_RECORD rec;
   DEV_BLOCK  *block;
   
   Dmsg0(10, "Start append data.\n");

   /* Tell File daemon to send data */
   bnet_fsend(fd_sock, OK_data);

   sm_check(__FILE__, __LINE__, False);

   ds = fd_sock;

   if (!bnet_set_buffer_size(ds, MAX_NETWORK_BUFFER_SIZE, BNET_SETBUF_WRITE)) {
      jcr->JobStatus = JS_Cancelled;
      Jmsg(jcr, M_FATAL, 0, _("Unable to set network buffer size.\n"));
      return 0;
   }

   Dmsg1(20, "Begin append device=%s\n", dev_name(dev));

   block = new_block(dev);

   /* 
    * Acquire output device for writing.  Note, after acquiring a
    *	device, we MUST release it, which is done at the end of this
    *	subroutine.
    */
   Dmsg0(100, "just before acquire_device\n");
   if (!acquire_device_for_append(jcr, dev, block)) {
      jcr->JobStatus = JS_Cancelled;
      free_block(block);
      return 0;
   }
   sm_check(__FILE__, __LINE__, False);

   Dmsg0(100, "Just after acquire_device_for_append\n");
   /*
    * Write Begin Session Record
    */
   if (!write_session_label(jcr, block, SOS_LABEL)) {
      jcr->JobStatus = JS_Cancelled;
      Jmsg1(jcr, M_FATAL, 0, _("Write session label failed. ERR=%s\n"),
	 strerror_dev(dev));
      ok = FALSE;
   }

   sm_check(__FILE__, __LINE__, False);

   memset(&rec, 0, sizeof(rec));

   /* 
    * Get Data from File daemon, write to device   
    */
   jcr->VolFirstFile = 0;
   time(&jcr->run_time);	      /* start counting time for rates */
   for (last_file_index = 0; ok && !job_cancelled(jcr); ) {
      char info[100];

      /* Read Stream header from the File daemon.
       *  The stream header consists of the following:
       *    file_index (sequential Bacula file index)
       *    stream     (arbitrary Bacula number to distinguish parts of data)
       *    info       (Info for Storage daemon -- compressed, encryped, ...)
       */
      if ((n=bget_msg(ds)) < 0) { 
         Jmsg1(jcr, M_FATAL, 0, _("Error reading data header from FD. ERR=%s\n"),
	    bnet_strerror(ds));
	 ok = FALSE;
	 break;
      }
      if (n == 0 || job_cancelled(jcr)) {
	 break; 		      /* all done */
      }
      sm_check(__FILE__, __LINE__, False);
      ds->msg[ds->msglen] = 0;
      if (sscanf(ds->msg, "%ld %ld %100s", &file_index, &stream, info) != 3) {
         Jmsg1(jcr, M_FATAL, 0, _("Malformed data header from FD: %s\n"), ds->msg);
	 ok = FALSE;
	 break;
      }
      Dmsg2(190, "<filed: Header FilInx=%d stream=%d\n", file_index, stream);

      if (!(file_index > 0 && (file_index == last_file_index ||
	  file_index == last_file_index + 1))) {
         Jmsg0(jcr, M_FATAL, 0, _("File index from FD not positive or sequential\n"));
	 ok = FALSE;
	 break;
      }
      if (file_index != last_file_index) {
	 jcr->JobFiles = file_index;
	 if (jcr->VolFirstFile == 0) {
	    jcr->VolFirstFile = file_index;
	 }
	 last_file_index = file_index;
      }
      
      /* Read data stream from the File daemon.
       *  The data stream is just raw bytes
       */
      sm_check(__FILE__, __LINE__, False);
      while ((n=bget_msg(ds)) > 0 && !job_cancelled(jcr)) {

	 sm_check(__FILE__, __LINE__, False);
	 rec.VolSessionId = jcr->VolSessionId;
	 rec.VolSessionTime = jcr->VolSessionTime;
	 rec.FileIndex = file_index;
	 rec.Stream = stream;
	 rec.data_len = ds->msglen;
	 rec.data = ds->msg;		/* use message buffer */

         Dmsg4(250, "before writ_rec FI=%d SessId=%d Strm=%s len=%d\n",
	    rec.FileIndex, rec.VolSessionId, stream_to_ascii(rec.Stream), 
	    rec.data_len);
	  
	 while (!write_record_to_block(block, &rec)) {
            Dmsg2(150, "!write_record_to_block data_len=%d rem=%d\n", rec.data_len,
		       rec.remainder);
	    if (!write_block_to_device(jcr, dev, block)) {
               Dmsg2(90, "Got write_block_to_dev error on device %s. %s\n",
		  dev_name(dev), strerror_dev(dev));
               Jmsg(jcr, M_FATAL, 0, _("Cannot fixup device error. %s\n"),
		     strerror_dev(dev));
	       ok = FALSE;
	       break;
	    }
	 }
	 sm_check(__FILE__, __LINE__, False);
	 if (!ok) {
	    break;
	 }
	 jcr->JobBytes += rec.data_len;   /* increment bytes this job */
         Dmsg4(190, "write_record FI=%s SessId=%d Strm=%s len=%d\n",
	    FI_to_ascii(rec.FileIndex), rec.VolSessionId, 
	    stream_to_ascii(rec.Stream), rec.data_len);
	 /* Send attributes and MD5 to Director for Catalog */
	 if (stream == STREAM_UNIX_ATTRIBUTES || stream == STREAM_MD5_SIGNATURE) {
	    if (!dir_update_file_attributes(jcr, &rec)) {
	       ok = FALSE;
	       break;
	    }
	 }
	 sm_check(__FILE__, __LINE__, False);
      }
      if (n < 0) {
         Jmsg1(jcr, M_FATAL, 0, _("Network error on data channel. ERR=%s\n"),
	    bnet_strerror(ds));
	 ok = FALSE;
	 break;
      }
   }
   /* 
    *******FIXME***** we should put the ok status in the End of
    * session label 
    *
    *   We probably need a new flag that says "Do not attempt
    *   to write because there is no tape".
    */
   sm_check(__FILE__, __LINE__, False);
   Dmsg0(90, "Write_end_session_label()\n");
   if (!write_session_label(jcr, block, EOS_LABEL)) {
      Jmsg1(jcr, M_FATAL, 0, _("Error writting end session label. ERR=%s\n"),
	  strerror_dev(dev));
      ok = FALSE;
   }
   /* Write out final block of this session */
   if (!write_block_to_device(jcr, dev, block)) {
      Pmsg0(0, "Set ok=FALSE after write_block_to_device.\n");
      ok = FALSE;
   }

   /* Release the device */
   if (!release_device(jcr, dev, block)) {
      Pmsg0(0, "Error in release_device\n");
      ok = FALSE;
   }

   free_block(block);
   Dmsg0(90, "return from do_append_data()\n");
   return ok ? 1 : 0;
}
