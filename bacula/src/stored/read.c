/*
 * Read code for Storage daemon
 *
 *     Kern Sibbald, November MM
 *
 *   Version $Id$
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

/* Forward referenced subroutines */

/* Variables used by Child process */
/* Global statistics */
/* Note, these probably should be in shared memory so that 
 * they are truly global for all processes
 */
extern struct s_shm *shm;	      /* shared memory structure */
extern int  FiledDataChan;	      /* File daemon data channel (port) */


/* Responses sent to the File daemon */
static char OK_data[]    = "3000 OK data\n";
static char rec_header[] = "rechdr %ld %ld %ld %ld %ld";


/* 
 *  Read Data and send to File Daemon
 *   Returns: 0 on failure
 *	      1 on success
 */
int do_read_data(JCR *jcr) 
{
   BSOCK *ds;
   BSOCK *fd_sock = jcr->file_bsock;
   int ok = TRUE;
   DEVICE *dev;
   DEV_RECORD rec;
   DEV_BLOCK *block;
   POOLMEM *hdr; 
   SESSION_LABEL sessrec;	       /* session record */
   
   Dmsg0(20, "Start read data.\n");

   dev = jcr->device->dev;
   memset(&sessrec, 0, sizeof(sessrec));

   /* Tell File daemon we will send data */
   bnet_fsend(fd_sock, OK_data);
   Dmsg1(10, "bstored>filed: %s\n", fd_sock->msg);

   ds = fd_sock;

   if (!bnet_set_buffer_size(ds, MAX_NETWORK_BUFFER_SIZE, BNET_SETBUF_READ)) {
      return 0;
   }


   Dmsg1(20, "Begin read device=%s\n", dev_name(dev));

   block = new_block(dev);

   create_vol_list(jcr);

   Dmsg1(20, "Found %d volumes names to restore.\n", jcr->NumVolumes);

   /* 
    * Ready device for reading, and read records
    */
   if (!acquire_device_for_read(jcr, dev, block)) {
      free_block(block);
      free_vol_list(jcr);
      return 0;
   }

   memset(&rec, 0, sizeof(rec));
   rec.data = ds->msg;		      /* use socket message buffer */
   hdr = get_pool_memory(PM_MESSAGE);

   /*
    *	Read records, apply BSR filtering, and return any that are 
    *	 matched.
    */
   for ( ;ok; ) {
      DEV_RECORD *record;	      /* for reading label of multi-volumes */

      if (job_cancelled(jcr)) {
	 ok = FALSE;
	 break;
      }
      /* Read Record */
      Dmsg1(500, "Main read_record. rem=%d\n", rec.remainder);
      if (!read_record(dev, block, &rec)) {
         Dmsg1(500, "Main read record failed. rem=%d\n", rec.remainder);
	 if (dev->state & ST_EOT) {
	    if (rec.remainder) {
               Dmsg0(500, "Not end of record.\n");
	    }
            Dmsg2(90, "NumVolumes=%d CurVolume=%d\n", jcr->NumVolumes, jcr->CurVolume);
	    /*
	     * End Of Tape -- mount next Volume (if another specified)
	     */
	    if (jcr->NumVolumes > 1 && jcr->CurVolume < jcr->NumVolumes) {
	       VOL_LIST *vol = jcr->VolList;
	       close_dev(dev);
	       jcr->CurVolume++;
	       for (int i=1; i<jcr->CurVolume; i++) {
		  vol = vol->next;
	       }
	       strcpy(jcr->VolumeName, vol->VolumeName);
               Dmsg1(000, "There is another volume %s.\n", jcr->VolumeName);
	       dev->state &= ~ST_READ; 
	       if (!acquire_device_for_read(jcr, dev, block)) {
                  Jmsg(jcr, M_FATAL, 0, _("Cannot open Dev=%s, Vol=%s\n"), dev_name(dev), jcr->VolumeName);
		  ok = FALSE;
		  break;
	       }
	       record = new_record();
               Dmsg1(500, "read record after new tape. rem=%d\n", record->remainder);
	       read_record(dev, block, record); /* read vol label */
	       dump_label_record(dev, record, 0);
	       free_record(record);
	       continue;
	    }
            Dmsg0(90, "End of Device reached.\n");
	    break;		      /* End of Tape */
	 }
	 if (dev->state & ST_EOF) {
            Dmsg0(90, "Got End of File. Trying again ...\n");
	    continue;		      /* End of File */
	 }

         Jmsg2(jcr, M_FATAL, 0, _("Read error on Record Header %s ERR=%s\n"), dev_name(dev), strerror(errno));
	 ok = FALSE;
	 break;
      }

      /* Some sort of label? */ 
      if (rec.FileIndex < 0) {
	 char *rtype;
	 memset(&sessrec, 0, sizeof(sessrec));
	 switch (rec.FileIndex) {
	    case PRE_LABEL:
               rtype = "Fresh Volume Label";   
	       break;
	    case VOL_LABEL:
               rtype = "Volume Label";
	       unser_volume_label(dev, &rec);
	       break;
	    case SOS_LABEL:
               rtype = "Begin Session";
	       unser_session_label(&sessrec, &rec);
	       break;
	    case EOS_LABEL:
               rtype = "End Session";
	       break;
	    case EOM_LABEL:
               rtype = "End of Media";
	       break;
	    default:
               rtype = "Unknown";
	       break;
	 }
	 if (debug_level > 0) {
            printf("%s Record: VolSessionId=%d VolSessionTime=%d JobId=%d DataLen=%d\n",
	       rtype, rec.VolSessionId, rec.VolSessionTime, rec.Stream, rec.data_len);
	 }

         Dmsg1(40, "Got label = %d\n", rec.FileIndex);
	 if (rec.FileIndex == EOM_LABEL) { /* end of tape? */
            Dmsg0(40, "Get EOM LABEL\n");
	    break;			   /* yes, get out */
	 }
	 continue;			   /* ignore other labels */
      } /* end if label record */

      if (jcr->bsr) {
	 /* Match BSR against current record */
	 if (!match_bsr(jcr->bsr, &rec, &dev->VolHdr, &sessrec)) {
            Dmsg0(50, "BSR rejected record\n");
	    continue;
	 }
      } else { 
	 /* Old way, deprecated */
	 if (rec.VolSessionId != jcr->read_VolSessionId ||
	     rec.VolSessionTime != jcr->read_VolSessionTime) {
            Dmsg0(50, "Ignore record ids not equal\n");
	    continue;			 /* ignore */
	 }
      }
       
      /* Generate Header parameters and send to File daemon
       * Note, we build header in hdr buffer to avoid wiping
       * out the data record
       */
      ds->msg = hdr;
      if (!bnet_fsend(ds, rec_header, rec.VolSessionId, rec.VolSessionTime,
	     rec.FileIndex, rec.Stream, rec.data_len)) {
         Dmsg1(30, ">filed: Error Hdr=%s\n", ds->msg);
	 ds->msg = rec.data;
	 ok = FALSE;
	 break;
      } else {
         Dmsg1(30, ">filed: Hdr=%s\n", ds->msg);
      }

      ds->msg = rec.data;	      /* restore data record address */

      /* Send data record to File daemon */
      ds->msglen = rec.data_len;
      Dmsg1(40, ">filed: send %d bytes data.\n", ds->msglen);
      if (!bnet_send(ds)) {
         Dmsg0(0, "Error sending to FD\n");
         Jmsg1(jcr, M_FATAL, 0, _("Error sending to File daemon. ERR=%s\n"),
	    bnet_strerror(ds));
	 ok = FALSE;
      }
   }
   /* Send end of data to FD */
   bnet_sig(ds, BNET_EOD);

   if (!release_device(jcr, dev, block)) {
      ok = FALSE;
   }
   free_pool_memory(hdr);
   free_block(block);
   free_vol_list(jcr);
   Dmsg0(30, "Done reading.\n");
   return ok ? 1 : 0;
}
