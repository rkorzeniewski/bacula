/*
 *
 *  Routines for handling mounting tapes for reading and for
 *    writing.
 *
 *   Kern Sibbald, August MMII
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

#include "bacula.h"                   /* pull in global headers */
#include "stored.h"                   /* pull in Storage Deamon headers */

/* Forward referenced functions */


/*
 * If release is set, we rewind the current volume, 
 * which we no longer want, and ask the user (console) 
 * to mount the next volume.
 *
 *  Continue trying until we get it, and then ensure
 *  that we can write on it.
 *
 * This routine returns a 0 only if it is REALLY
 *  impossible to get the requested Volume.
 */
int mount_next_write_volume(JCR *jcr, DEVICE *dev, DEV_BLOCK *block, int release)
{
   int recycle, ask, retry = 0, autochanger;

   Dmsg0(100, "Enter mount_next_volume()\n");

mount_next_vol:
   if (retry++ > 5) {
      Jmsg(jcr, M_FATAL, 0, _("Too many errors trying to mount device %s.\n"), 
	   dev_name(dev));
      return 0;
   }
   if (job_canceled(jcr)) {
      Jmsg(jcr, M_FATAL, 0, _("Job canceled.\n"));
      return 0;
   }
   recycle = ask = autochanger = 0;
   if (release) {
      Dmsg0(100, "mount_next_volume release=1\n");
      /* 
       * First erase all memory of the current volume	
       */
      dev->block_num = dev->file = 0;
      dev->EndBlock = dev->EndFile = 0;
      memset(&dev->VolCatInfo, 0, sizeof(dev->VolCatInfo));
      memset(&jcr->VolCatInfo, 0, sizeof(jcr->VolCatInfo));
      memset(&dev->VolHdr, 0, sizeof(dev->VolHdr));
      dev->state &= ~ST_LABEL;	      /* label not yet read */
      jcr->VolumeName[0] = 0;

      if (!dev_is_tape(dev) || !dev_cap(dev, CAP_ALWAYSOPEN)) {
	 if (dev_cap(dev, CAP_OFFLINEUNMOUNT)) {
	    offline_dev(dev);
	 /*	       
          * Note, this rewind probably should not be here (it wasn't
	  *  in prior versions of Bacula), but on FreeBSD, this is
          *  needed in the case the tape was "frozen" due to an error
	  *  such as backspacing after writing and EOF. If it is not
	  *  done, all future references to the drive get and I/O error.
	  */
	 } else if (!rewind_dev(dev)) {
            Jmsg2(jcr, M_WARNING, 0, _("Rewind error on device %s. ERR=%s\n"), 
		  dev_name(dev), strerror_dev(dev));
	 }
	 close_dev(dev);
      }

      /* If we have not closed the device, then at least rewind the tape */
      if (dev->state & ST_OPENED) {
	 if (dev_cap(dev, CAP_OFFLINEUNMOUNT)) {
	    offline_dev(dev);
	 } else if (!rewind_dev(dev)) {
            Jmsg2(jcr, M_WARNING, 0, _("Rewind error on device %s. ERR=%s\n"), 
		  dev_name(dev), strerror_dev(dev));
	 }
      }
      ask = 1;			      /* ask operator to mount tape */
   }

   /* 
    * Get Director's idea of what tape we should have mounted. 
    */
   if (!dir_find_next_appendable_volume(jcr) &&
       !dir_ask_sysop_to_mount_next_volume(jcr, dev)) {
      return 0;
   }
   Dmsg2(100, "After find_next_append. Vol=%s Slot=%d\n",
	 jcr->VolCatInfo.VolCatName, jcr->VolCatInfo.Slot);

   /* 
    * Get next volume and ready it for append
    * This code ensures that the device is ready for
    * writing. We start from the assumption that there
    * may not be a tape mounted. 
    *
    * If the device is a file, we create the output
    * file. If it is a tape, we check the volume name
    * and move the tape to the end of data.
    *
    * It assumes that the device is not already in use!
    *
    */

   Dmsg0(100, "Enter ready_dev_for_append\n");

   dev->state &= ~(ST_APPEND|ST_READ|ST_EOT|ST_WEOT|ST_EOF);

   jcr->VolFirstIndex = jcr->JobFiles; /* first update of Vol FileIndex */
   for ( ;; ) {
      int vol_label_status;
      autochanger = autoload_device(jcr, dev, 1, NULL);

      /*
       * If we autochanged to correct Volume or (we have not just
       *   released the Volume AND we can automount) we go ahead 
       *   and read the label. If there is no tape in the drive,
       *   we will err, recurse and ask the operator the next time.
       */
      if (autochanger || (!release && dev_is_tape(dev) && dev_cap(dev, CAP_AUTOMOUNT))) {
         ask = 0;                     /* don't ask SYSOP this time */
      }

      release = 1;                    /* release next time if we "recurse" */

ask_again:
      if (ask && !dir_ask_sysop_to_mount_next_volume(jcr, dev)) {
         Dmsg0(100, "Error return ask_sysop ...\n");
	 return 0;		/* error return */
      }
      Dmsg1(100, "want vol=%s\n", jcr->VolumeName);

      /* Open device */
      if  (!(dev->state & ST_OPENED)) {
	  int mode;
	  if (dev_cap(dev, CAP_STREAM)) {
	     mode = OPEN_WRITE_ONLY;
	  } else {
	     mode = OPEN_READ_WRITE;
	  }
	  if (open_dev(dev, jcr->VolCatInfo.VolCatName, mode) < 0) {
             Jmsg2(jcr, M_FATAL, 0, _("Unable to open device %s. ERR=%s\n"), 
		dev_name(dev), strerror_dev(dev));
	     return 0;
	  }
      }

      /*
       * Now make sure we have the right tape mounted
       */
read_volume:
      /* 
       * If we are writing to a stream device, ASSUME the volume label
       *  is correct.
       */
      if (dev_cap(dev, CAP_STREAM)) {
	 vol_label_status = VOL_OK;
	 create_volume_label(dev, jcr->VolumeName);
	 dev->VolHdr.LabelType = PRE_LABEL;
      } else {
	 vol_label_status = read_dev_volume_label(jcr, dev, block);
      }
      /*
       * At this point, dev->VolCatInfo has what is in the drive, if anything,
       *	  and	jcr->VolCatInfo has what the Director wants.
       */
      switch (vol_label_status) {
      case VOL_OK:
         Dmsg1(100, "Vol OK name=%s\n", jcr->VolumeName);
	 memcpy(&dev->VolCatInfo, &jcr->VolCatInfo, sizeof(jcr->VolCatInfo));
         if (strcmp(dev->VolCatInfo.VolCatStatus, "Recycle") == 0) {
	    recycle = 1;
	 }
	 break; 		   /* got a Volume */
      case VOL_NAME_ERROR:
	 VOLUME_CAT_INFO VolCatInfo;

         Dmsg1(100, "Vol NAME Error Name=%s\n", jcr->VolumeName);
	 /* 
	  * OK, we got a different volume mounted. First save the
	  *  requested Volume info (jcr) structure, then query if
	  *  this volume is really OK. If not, put back the desired
	  *  volume name and continue.
	  */
	 memcpy(&VolCatInfo, &jcr->VolCatInfo, sizeof(jcr->VolCatInfo));
	 /* Check if this is a valid Volume in the pool */
	 pm_strcpy(&jcr->VolumeName, dev->VolHdr.VolName);			   
	 if (!dir_get_volume_info(jcr, 1)) {
            Mmsg(&jcr->errmsg, _("Director wanted Volume \"%s\".\n"
                 "    Current Volume \"%s\" not acceptable because:\n"
                 "    %s"),
		VolCatInfo.VolCatName, dev->VolHdr.VolName,
		jcr->dir_bsock->msg);
	    /* Restore desired volume name, note device info out of sync */
	    memcpy(&jcr->VolCatInfo, &VolCatInfo, sizeof(jcr->VolCatInfo));
	    goto mount_error;
	 }
         Dmsg1(100, "want new name=%s\n", jcr->VolumeName);
	 memcpy(&dev->VolCatInfo, &jcr->VolCatInfo, sizeof(jcr->VolCatInfo));
	 break; 	       /* got a Volume */

      case VOL_NO_LABEL:
      case VOL_IO_ERROR:
         Dmsg1(500, "Vol NO_LABEL or IO_ERROR name=%s\n", jcr->VolumeName);
	 /* If permitted, create a label */
	 if (dev_cap(dev, CAP_LABEL)) {
            Dmsg0(100, "Create volume label\n");
	    if (!write_volume_label_to_dev(jcr, (DEVRES *)dev->device, jcr->VolumeName,
		   jcr->pool_name)) {
               Dmsg0(100, "!write_vol_label\n");
	       goto mount_next_vol;
	    }
            Jmsg(jcr, M_INFO, 0, _("Labeled new Volume \"%s\" on device %s.\n"),
	       jcr->VolumeName, dev_name(dev));
	    goto read_volume;	   /* read label we just wrote */
	 } 
	 /* NOTE! Fall-through wanted. */
      default:
mount_error:
	 /* Send error message */
         Jmsg1(jcr, M_WARNING, 0, "%s", jcr->errmsg);                         
         Dmsg0(100, "Default\n");
	 ask = 1;
	 goto ask_again;
      }
      break;
   }

   /* 
    * See if we have a fresh tape or tape with data.
    *
    * Note, if the LabelType is PRE_LABEL, it was labeled
    *  but never written. If so, rewrite the label but set as
    *  VOL_LABEL.  We rewind and return the label (reconstructed)
    *  in the block so that in the case of a new tape, data can
    *  be appended just after the block label.	If we are writing
    *  an second volume, the calling routine will write the label
    *  before writing the overflow block.
    *
    *  If the tape is marked as Recycle, we rewrite the label.
    */
   if (dev->VolHdr.LabelType == PRE_LABEL || recycle) {
      Dmsg1(190, "ready_for_append found freshly labeled volume. dev=%x\n", dev);
      dev->VolHdr.LabelType = VOL_LABEL; /* set Volume label */
      write_volume_label_to_block(jcr, dev, block);
      /*
       * If we are not dealing with a streaming device,
       *  write the block now to ensure we have write permission.
       *  It is better to find out now rather than later.
       */
      if (!dev_cap(dev, CAP_STREAM)) {
	 dev->VolCatInfo.VolCatBytes = 0;
	 if (!rewind_dev(dev)) {
            Jmsg2(jcr, M_WARNING, 0, _("Rewind error on device %s. ERR=%s\n"), 
		  dev_name(dev), strerror_dev(dev));
	 }
	 if (recycle) {
	    if (!truncate_dev(dev)) {
               Jmsg2(jcr, M_WARNING, 0, _("Truncate error on device %s. ERR=%s\n"), 
		     dev_name(dev), strerror_dev(dev));
	    }
	 }
	 /* Attempt write to check write permission */
	 if (!write_block_to_dev(jcr, dev, block)) {
            Jmsg2(jcr, M_ERROR, 0, _("Unable to write device %s. ERR=%s\n"),
	       dev_name(dev), strerror_dev(dev));
	    goto mount_next_vol;
	 }
	 if (!rewind_dev(dev)) {
            Jmsg2(jcr, M_ERROR, 0, _("Unable to rewind device %s. ERR=%s\n"),
	       dev_name(dev), strerror_dev(dev));
	    goto mount_next_vol;
	 }

	 /* Recreate a correct volume label and return it in the block */
	 write_volume_label_to_block(jcr, dev, block);
      }
      /* Set or reset Volume statistics */
      dev->VolCatInfo.VolCatJobs = 0;
      dev->VolCatInfo.VolCatFiles = 0;
      dev->VolCatInfo.VolCatErrors = 0;
      dev->VolCatInfo.VolCatBlocks = 0;
      dev->VolCatInfo.VolCatRBytes = 0;
      if (recycle) {
	 dev->VolCatInfo.VolCatMounts++;  
	 dev->VolCatInfo.VolCatRecycles++;
      } else {
	 dev->VolCatInfo.VolCatMounts = 1;
	 dev->VolCatInfo.VolCatRecycles = 0;
	 dev->VolCatInfo.VolCatWrites = 1;
	 dev->VolCatInfo.VolCatReads = 1;
      }
      strcpy(dev->VolCatInfo.VolCatStatus, "Append");
      Dmsg0(200, "dir_update_vol_info. Set Append\n");
      dir_update_volume_info(jcr, &dev->VolCatInfo, 1);  /* indicate doing relabel */
      if (recycle) {
         Jmsg(jcr, M_INFO, 0, _("Recycled volume \"%s\" on device %s, all previous data lost.\n"),
	    jcr->VolumeName, dev_name(dev));
      } else {
         Jmsg(jcr, M_INFO, 0, _("Wrote label to prelabeled Volume \"%s\" on device %s\n"),
	    jcr->VolumeName, dev_name(dev));
      }

   } else {
      /*
       * OK, at this point, we have a valid Bacula label, but
       * we need to position to the end of the volume, since we are
       * just now putting it into append mode.
       */
      Dmsg0(200, "Device previously written, moving to end of data\n");
      Jmsg(jcr, M_INFO, 0, _("Volume \"%s\" previously written, moving to end of data.\n"),
	 jcr->VolumeName);
      if (!eod_dev(dev)) {
         Jmsg(jcr, M_ERROR, 0, _("Unable to position to end of data %s. ERR=%s\n"),
	    dev_name(dev), strerror_dev(dev));
         Jmsg(jcr, M_INFO, 0, _("Marking Volume \"%s\" in Error in Catalog.\n"),
	    jcr->VolumeName);
         strcpy(dev->VolCatInfo.VolCatStatus, "Error");
         Dmsg0(200, "dir_update_vol_info. Set Error.\n");
	 dir_update_volume_info(jcr, &dev->VolCatInfo, 0);
	 goto mount_next_vol;
      }
      /* *****FIXME**** we should do some checking for files too */
      if (dev_is_tape(dev)) {
	 /*
	  * Check if we are positioned on the tape at the same place
	  * that the database says we should be.
	  */
	 if (dev->VolCatInfo.VolCatFiles == dev_file(dev)) {
            Jmsg(jcr, M_INFO, 0, _("Ready to append to end of Volume at file=%d.\n"), 
		 dev_file(dev));
	 } else {
            Jmsg(jcr, M_ERROR, 0, _("I canot write on this volume because:\n\
The number of files mismatch! Volume=%u Catalog=%u\n"), 
		 dev_file(dev), dev->VolCatInfo.VolCatFiles);
            strcpy(dev->VolCatInfo.VolCatStatus, "Error");
            Dmsg0(200, "dir_update_vol_info. Set Error.\n");
	    dir_update_volume_info(jcr, &dev->VolCatInfo, 0);
	    goto mount_next_vol;
	 }
      }
      dev->VolCatInfo.VolCatMounts++;	   /* Update mounts */
      Dmsg1(200, "update volinfo mounts=%d\n", dev->VolCatInfo.VolCatMounts);
      dir_update_volume_info(jcr, &dev->VolCatInfo, 0);
      /* Return an empty block */
      empty_block(block);	      /* we used it for reading so set for write */
   }
   dev->state |= ST_APPEND;
   Dmsg0(100, "Normal return from read_dev_for_append\n");
   return 1; 
}


int mount_next_read_volume(JCR *jcr, DEVICE *dev, DEV_BLOCK *block)
{
   Dmsg2(90, "NumVolumes=%d CurVolume=%d\n", jcr->NumVolumes, jcr->CurVolume);
   /*
    * End Of Tape -- mount next Volume (if another specified)
    */
   if (jcr->NumVolumes > 1 && jcr->CurVolume < jcr->NumVolumes) {
      close_dev(dev);
      dev->state &= ~ST_READ; 
      if (!acquire_device_for_read(jcr, dev, block)) {
         Jmsg2(jcr, M_FATAL, 0, "Cannot open Dev=%s, Vol=%s\n", dev_name(dev),
	       jcr->VolumeName);
	 return 0;
      }
      return 1; 		      /* next volume mounted */
   }
   Dmsg0(90, "End of Device reached.\n");
   return 0;
}
