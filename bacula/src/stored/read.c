/*
 * Read code for Storage daemon
 *
 *     Kern Sibbald, November MM
 *
 *   Version $Id$
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
#include "stored.h"

/* Forward referenced subroutines */
static bool record_cb(JCR *jcr, DEVICE *dev, DEV_BLOCK *block, DEV_RECORD *rec);


/* Responses sent to the File daemon */
static char OK_data[]    = "3000 OK data\n";
static char FD_error[]   = "3000 error\n";
static char rec_header[] = "rechdr %ld %ld %ld %ld %ld";

/* 
 *  Read Data and send to File Daemon
 *   Returns: false on failure
 *	      true  on success
 */
bool do_read_data(JCR *jcr) 
{
   BSOCK *fd = jcr->file_bsock;
   bool ok = true;
   DEVICE *dev;
   
   Dmsg0(20, "Start read data.\n");

   if (!bnet_set_buffer_size(fd, jcr->device->max_network_buffer_size, BNET_SETBUF_WRITE)) {
      return false;
   }


   create_vol_list(jcr);
   if (jcr->NumVolumes == 0) {
      Jmsg(jcr, M_FATAL, 0, _("No Volume names found for restore.\n"));
      free_vol_list(jcr);
      bnet_fsend(fd, FD_error);
      return false;
   }

   Dmsg2(200, "Found %d volumes names to restore. First=%s\n", jcr->NumVolumes, 
      jcr->VolList->VolumeName);

   pm_strcpy(&jcr->VolumeName, jcr->VolList->VolumeName);

   /* 
    * Ready device for reading, and read records
    */
   if (!acquire_device_for_read(jcr)) {
      free_vol_list(jcr);
      return false;
   }

   dev = jcr->dcr->dev;
   Dmsg1(20, "Begin read device=%s\n", dev_name(dev));

   /* Tell File daemon we will send data */
   bnet_fsend(fd, OK_data);
   ok = read_records(jcr, dev, record_cb, mount_next_read_volume);

   /* Send end of data to FD */
   bnet_sig(fd, BNET_EOD);

   if (!release_device(jcr)) {
      ok = false;
   }
   
   free_vol_list(jcr);
   Dmsg0(30, "Done reading.\n");
   return ok;
}

/*
 * Called here for each record from read_records()
 *  Returns: true if OK
 *	     false if error
 */
static bool record_cb(JCR *jcr, DEVICE *dev, DEV_BLOCK *block, DEV_RECORD *rec)
{
   BSOCK *fd = jcr->file_bsock;
   bool ok = true;
   POOLMEM *save_msg;

   if (rec->FileIndex < 0) {
      return true;
   }
   Dmsg5(100, "Send to FD: SessId=%u SessTim=%u FI=%d Strm=%d, len=%d\n",
      rec->VolSessionId, rec->VolSessionTime, rec->FileIndex, rec->Stream,
      rec->data_len);

   /* Send record header to File daemon */
   if (!bnet_fsend(fd, rec_header, rec->VolSessionId, rec->VolSessionTime,
	  rec->FileIndex, rec->Stream, rec->data_len)) {
      Dmsg1(30, ">filed: Error Hdr=%s\n", fd->msg);
      Jmsg1(jcr, M_FATAL, 0, _("Error sending to File daemon. ERR=%s\n"),
	 bnet_strerror(fd));
      return false;
   } else {
      Dmsg1(30, ">filed: Hdr=%s\n", fd->msg);
   }


   /* Send data record to File daemon */
   save_msg = fd->msg;		/* save fd message pointer */
   fd->msg = rec->data; 	/* pass data directly to bnet_send */
   fd->msglen = rec->data_len;
   Dmsg1(30, ">filed: send %d bytes data.\n", fd->msglen);
   if (!bnet_send(fd)) {
      Pmsg1(000, "Error sending to FD. ERR=%s\n", bnet_strerror(fd));
      Jmsg1(jcr, M_FATAL, 0, _("Error sending to File daemon. ERR=%s\n"),
	 bnet_strerror(fd));

      ok = false;
   }
   fd->msg = save_msg;		      /* restore fd message pointer */
   return ok;
}
