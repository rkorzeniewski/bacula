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
   Copyright (C) 2000-2004 Kern Sibbald and John Walker

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

static bool rewrite_volume_label(JCR *jcr, DEVICE *dev, DEV_BLOCK *bloc, bool recycle);


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
 *
 */
bool mount_next_write_volume(JCR *jcr, DEVICE *dev, DEV_BLOCK *block, bool release)
{
   int retry = 0;
   bool ask = false, recycle, autochanger;
   int vol_label_status;

   Dmsg0(100, "Enter mount_next_volume()\n");

   init_dev_wait_timers(dev);

   /* 
    * Attempt to mount the next volume. If something non-fatal goes
    *  wrong, we come back here to re-try (new op messages, re-read
    *  Volume, ...)
    */
mount_next_vol:
   /* Ignore retry if this is poll request */
   if (!dev->poll && retry++ > 4) {
      /* Last ditch effort before giving up, force operator to respond */
      jcr->VolCatInfo.Slot = 0;
      if (!dir_ask_sysop_to_mount_volume(jcr)) {
         Jmsg(jcr, M_FATAL, 0, _("Too many errors trying to mount device %s.\n"), 
	      dev_name(dev));
	 return false;
      }
   }
   if (job_canceled(jcr)) {
      Jmsg(jcr, M_FATAL, 0, _("Job %d canceled.\n"), jcr->JobId);
      return false;
   }
   recycle = false;
   if (release) {
      Dmsg0(100, "mount_next_volume release=1\n");
      release_volume(jcr, dev);
      ask = true;		      /* ask operator to mount tape */
   }

   /* 
    * Get Director's idea of what tape we should have mounted. 
    *	 in jcr->VolCatInfo
    */
   Dmsg0(200, "Before dir_find_next_appendable_volume.\n");
   while (!dir_find_next_appendable_volume(jcr)) {
       Dmsg0(200, "not dir_find_next\n");
       if (!dir_ask_sysop_to_create_appendable_volume(jcr)) {
	 return false;
       }
       Dmsg0(200, "Again dir_find_next_append...\n");
   }
   if (job_canceled(jcr)) {
      return false;
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
   dev->state &= ~(ST_APPEND|ST_READ|ST_EOT|ST_WEOT|ST_EOF);

   if (autoload_device(jcr, dev, 1, NULL) > 0) {
      autochanger = true;
      ask = false;
   } else {
      autochanger = false;
      jcr->VolCatInfo.Slot = 0;
   }
   Dmsg1(100, "autoload_dev returns %d\n", autochanger);
   /*
    * If we autochanged to correct Volume or (we have not just
    *	released the Volume AND we can automount) we go ahead 
    *	and read the label. If there is no tape in the drive,
    *	we will err, recurse and ask the operator the next time.
    */
   if (!release && dev_is_tape(dev) && dev_cap(dev, CAP_AUTOMOUNT)) {
      ask = false;                 /* don't ask SYSOP this time */
   }
   /* Don't ask if not removable */
   if (!dev_cap(dev, CAP_REM)) {
      ask = false;
   }
   Dmsg2(100, "Ask=%d autochanger=%d\n", ask, autochanger);
   release = true;                /* release next time if we "recurse" */

   if (ask && !dir_ask_sysop_to_mount_volume(jcr)) {
      Dmsg0(100, "Error return ask_sysop ...\n");
      return false;	     /* error return */
   }
   if (job_canceled(jcr)) {
      return false;
   }
   Dmsg1(100, "want vol=%s\n", jcr->VolumeName);

   if (dev->poll && dev_cap(dev, CAP_CLOSEONPOLL)) {
      force_close_dev(dev);
   }

   /* Ensure the device is open */
   if (!open_device(jcr, dev)) {
      return false;
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
      create_volume_label(dev, jcr->VolumeName, "Default");
      dev->VolHdr.LabelType = PRE_LABEL;
   } else {
      vol_label_status = read_dev_volume_label(jcr, dev, block);
   }

   Dmsg2(100, "dirVol=%s dirStat=%s\n", jcr->VolumeName,
      jcr->VolCatInfo.VolCatStatus);
   /*
    * At this point, dev->VolCatInfo has what is in the drive, if anything,
    *	       and   jcr->VolCatInfo has what the Director wants.
    */
   switch (vol_label_status) {
   case VOL_OK:
      Dmsg1(100, "Vol OK name=%s\n", jcr->VolumeName);
      memcpy(&dev->VolCatInfo, &jcr->VolCatInfo, sizeof(jcr->VolCatInfo));
      recycle = strcmp(dev->VolCatInfo.VolCatStatus, "Recycle") == 0;
      break;			/* got a Volume */
   case VOL_NAME_ERROR:
      VOLUME_CAT_INFO VolCatInfo;

      /* If not removable, Volume is broken */
      if (!dev_cap(dev, CAP_REM)) {
         Jmsg(jcr, M_WARNING, 0, _("Volume \"%s\" not on device %s.\n"),
	    jcr->VolumeName, dev_name(dev));
	 mark_volume_in_error(jcr, dev);
	 goto mount_next_vol;
      }
	 
      Dmsg1(100, "Vol NAME Error Name=%s\n", jcr->VolumeName);
      /* If polling and got a previous bad name, ignore it */
      if (dev->poll && strcmp(dev->BadVolName, dev->VolHdr.VolName) == 0) {
	 ask = true;
         Dmsg1(200, "Vol Name error supress due to poll. Name=%s\n", 
	    jcr->VolumeName);
	 goto mount_next_vol;
      }
      /* 
       * OK, we got a different volume mounted. First save the
       *  requested Volume info (jcr) structure, then query if
       *  this volume is really OK. If not, put back the desired
       *  volume name and continue.
       */
      memcpy(&VolCatInfo, &jcr->VolCatInfo, sizeof(VolCatInfo));
      /* Check if this is a valid Volume in the pool */
      pm_strcpy(&jcr->VolumeName, dev->VolHdr.VolName); 			
      if (!dir_get_volume_info(jcr, GET_VOL_INFO_FOR_WRITE)) {
	 bstrncpy(dev->BadVolName, dev->VolHdr.VolName, sizeof(dev->BadVolName));
         Jmsg(jcr, M_WARNING, 0, _("Director wanted Volume \"%s\".\n"
              "    Current Volume \"%s\" not acceptable because:\n"
              "    %s"),
	     VolCatInfo.VolCatName, dev->VolHdr.VolName,
	     jcr->dir_bsock->msg);
	 /* Restore desired volume name, note device info out of sync */
	 memcpy(&jcr->VolCatInfo, &VolCatInfo, sizeof(jcr->VolCatInfo));
	 ask = true;
	 goto mount_next_vol;
      }
      Dmsg1(100, "want new name=%s\n", jcr->VolumeName);
      memcpy(&dev->VolCatInfo, &jcr->VolCatInfo, sizeof(dev->VolCatInfo));
      recycle = strcmp(dev->VolCatInfo.VolCatStatus, "Recycle") == 0;
      break;		    /* got a Volume */
   /*
    * At this point, we assume we have a blank tape mounted.
    */
   case VOL_NO_LABEL:
   case VOL_IO_ERROR:
      /* 
       * If permitted, we label the device, make sure we can do
       *   it by checking that the VolCatBytes is zero => not labeled, 
       *   once the Volume is labeled we don't want to label another
       *   blank tape with the same name.  For disk, we go ahead and
       *   label it anyway, because the OS insures that there is only
       *   one Volume with that name.
       * As noted above, at this point jcr->VolCatInfo has what
       *   the Director wants and dev->VolCatInfo has info on the
       *   previous tape (or nothing).
       */
      if (dev_cap(dev, CAP_LABEL) && (jcr->VolCatInfo.VolCatBytes == 0 ||
	    (!dev_is_tape(dev) && strcmp(jcr->VolCatInfo.VolCatStatus, 
                                   "Recycle") == 0))) {
         Dmsg0(100, "Create volume label\n");
	 /* Create a new Volume label and write it to the device */
	 if (!write_new_volume_label_to_dev(jcr, dev, jcr->VolumeName,
		jcr->pool_name)) {
            Dmsg0(100, "!write_vol_label\n");
	    goto mount_next_vol;
	 }
         Dmsg0(100, "dir_update_vol_info. Set Append\n");
         /* Copy Director's info into the device info */
	 memcpy(&dev->VolCatInfo, &jcr->VolCatInfo, sizeof(dev->VolCatInfo));
	 dir_update_volume_info(jcr, true);  /* indicate tape labeled */
         Jmsg(jcr, M_INFO, 0, _("Labeled new Volume \"%s\" on device %s.\n"),
	    jcr->VolumeName, dev_name(dev));
	 goto read_volume;	/* read label we just wrote */
      } 
      /* If not removable, Volume is broken */
      if (!dev_cap(dev, CAP_REM)) {
         Jmsg(jcr, M_WARNING, 0, _("Volume \"%s\" not on device %s.\n"),
	    jcr->VolumeName, dev_name(dev));
	 mark_volume_in_error(jcr, dev);
	 goto mount_next_vol;
      }
      /* NOTE! Fall-through wanted. */
   case VOL_NO_MEDIA:
   default:
      /* Send error message */
      if (!dev->poll) {
         Jmsg(jcr, M_WARNING, 0, "%s", jcr->errmsg);                         
      } else {
         Dmsg1(200, "Msg suppressed by poll: %s\n", jcr->errmsg);
      }
      ask = true;
      goto mount_next_vol;
   }

   /* 
    * See if we have a fresh tape or a tape with data.
    *
    * Note, if the LabelType is PRE_LABEL, it was labeled
    *  but never written. If so, rewrite the label but set as
    *  VOL_LABEL.  We rewind and return the label (reconstructed)
    *  in the block so that in the case of a new tape, data can
    *  be appended just after the block label.	If we are writing
    *  a second volume, the calling routine will write the label
    *  before writing the overflow block.
    *
    *  If the tape is marked as Recycle, we rewrite the label.
    */
   if (dev->VolHdr.LabelType == PRE_LABEL || recycle) {
      if (!rewrite_volume_label(jcr, dev, block, recycle)) {
	 goto mount_next_vol;
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
         Jmsg(jcr, M_ERROR, 0, _("Unable to position to end of data on device \"%s\". ERR=%s\n"),
	    dev_name(dev), strerror_dev(dev));
	 mark_volume_in_error(jcr, dev);
	 goto mount_next_vol;
      }
      /* *****FIXME**** we should do some checking for files too */
      if (dev_is_tape(dev)) {
	 /*
	  * Check if we are positioned on the tape at the same place
	  * that the database says we should be.
	  */
	 if (dev->VolCatInfo.VolCatFiles == dev_file(dev)) {
            Jmsg(jcr, M_INFO, 0, _("Ready to append to end of Volume \"%s\" at file=%d.\n"), 
		 jcr->VolumeName, dev_file(dev));
	 } else {
            Jmsg(jcr, M_ERROR, 0, _("I canot write on Volume \"%s\" because:\n\
The number of files mismatch! Volume=%u Catalog=%u\n"), 
		 jcr->VolumeName, dev_file(dev), dev->VolCatInfo.VolCatFiles);
	    mark_volume_in_error(jcr, dev);
	    goto mount_next_vol;
	 }
      }
      dev->VolCatInfo.VolCatMounts++;	   /* Update mounts */
      Dmsg1(100, "update volinfo mounts=%d\n", dev->VolCatInfo.VolCatMounts);
      dir_update_volume_info(jcr, false);
      /* Return an empty block */
      empty_block(block);	      /* we used it for reading so set for write */
   }
   dev->state |= ST_APPEND;
   Dmsg0(100, "Normal return from read_dev_for_append\n");
   return true;
}

/*
 * Write a volume label. 
 *  Returns: true if OK
 *	     false if unable to write it
 */
static bool rewrite_volume_label(JCR *jcr, DEVICE *dev, DEV_BLOCK *block, bool recycle)
{
   Dmsg1(190, "ready_for_append found freshly labeled volume. dev=%x\n", dev);
   dev->VolHdr.LabelType = VOL_LABEL; /* set Volume label */
   if (!write_volume_label_to_block(jcr, dev, block)) {
      return false;
   }
   /*
    * If we are not dealing with a streaming device,
    *  write the block now to ensure we have write permission.
    *  It is better to find out now rather than later.
    */
   if (!dev_cap(dev, CAP_STREAM)) {
      if (!rewind_dev(dev)) {
         Jmsg2(jcr, M_WARNING, 0, _("Rewind error on device \"%s\". ERR=%s\n"), 
	       dev_name(dev), strerror_dev(dev));
      }
      if (recycle) {
	 if (!truncate_dev(dev)) {
            Jmsg2(jcr, M_WARNING, 0, _("Truncate error on device \"%s\". ERR=%s\n"), 
		  dev_name(dev), strerror_dev(dev));
	 }
      }
      /* Attempt write to check write permission */
      if (!write_block_to_dev(jcr->dcr, block)) {
         Jmsg2(jcr, M_ERROR, 0, _("Unable to write device \"%s\". ERR=%s\n"),
	    dev_name(dev), strerror_dev(dev));
	 return false;
      }
   }
   /* Set or reset Volume statistics */
   dev->VolCatInfo.VolCatJobs = 0;
   dev->VolCatInfo.VolCatFiles = 0;
   dev->VolCatInfo.VolCatBytes = 1;
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
   Dmsg0(100, "dir_update_vol_info. Set Append\n");
   bstrncpy(dev->VolCatInfo.VolCatStatus, "Append", sizeof(dev->VolCatInfo.VolCatStatus));
   dir_update_volume_info(jcr, true);  /* indicate doing relabel */
   if (recycle) {
      Jmsg(jcr, M_INFO, 0, _("Recycled volume \"%s\" on device \"%s\", all previous data lost.\n"),
	 jcr->VolumeName, dev_name(dev));
   } else {
      Jmsg(jcr, M_INFO, 0, _("Wrote label to prelabeled Volume \"%s\" on device \"%s\"\n"),
	 jcr->VolumeName, dev_name(dev));
   }
   /*
    * End writing real Volume label (from pre-labeled tape), or recycling
    *  the volume.
    */
    return true;
}


/*
 * Mark volume in error in catalog 
 */
void mark_volume_in_error(JCR *jcr, DEVICE *dev)
{
   Jmsg(jcr, M_INFO, 0, _("Marking Volume \"%s\" in Error in Catalog.\n"),
	jcr->VolumeName);
   bstrncpy(dev->VolCatInfo.VolCatStatus, "Error", sizeof(dev->VolCatInfo.VolCatStatus));
   Dmsg0(100, "dir_update_vol_info. Set Error.\n");
   dir_update_volume_info(jcr, false);
}

/* 
 * If we are reading, we come here at the end of the tape
 *  and see if there are more volumes to be mounted.
 */
bool mount_next_read_volume(JCR *jcr, DEVICE *dev, DEV_BLOCK *block)
{
   Dmsg2(90, "NumVolumes=%d CurVolume=%d\n", jcr->NumVolumes, jcr->CurVolume);
   /*
    * End Of Tape -- mount next Volume (if another specified)
    */
   if (jcr->NumVolumes > 1 && jcr->CurVolume < jcr->NumVolumes) {
      close_dev(dev);
      dev->state &= ~ST_READ; 
      if (!acquire_device_for_read(jcr)) {
         Jmsg2(jcr, M_FATAL, 0, "Cannot open Dev=%s, Vol=%s\n", dev_name(dev),
	       jcr->VolumeName);
	 return false;
      }
      return true;		      /* next volume mounted */
   }
   Dmsg0(90, "End of Device reached.\n");
   return false;
}

/*
 * Either because we are going to hang a new volume, or because
 *  of explicit user request, we release the current volume.
 */
void release_volume(JCR *jcr, DEVICE *dev)
{
   if (jcr->dcr->WroteVol) {
      Jmsg0(jcr, M_ERROR, 0, "Hey!!!!! WroteVol non-zero !!!!!\n");
   }
   /* 
    * First erase all memory of the current volume   
    */
   dev->block_num = dev->file = 0;
   dev->EndBlock = dev->EndFile = 0;
   memset(&dev->VolCatInfo, 0, sizeof(dev->VolCatInfo));
   memset(&jcr->VolCatInfo, 0, sizeof(jcr->VolCatInfo));
   memset(&dev->VolHdr, 0, sizeof(dev->VolHdr));
   /* Force re-read of label */
   dev->state &= ~(ST_LABEL|ST_READ|ST_APPEND);
   jcr->VolumeName[0] = 0;

   if ((dev->state & ST_OPENED) && 
       (!dev_is_tape(dev) || !dev_cap(dev, CAP_ALWAYSOPEN))) {
      offline_or_rewind_dev(dev);
      close_dev(dev);
   }

   /* If we have not closed the device, then at least rewind the tape */
   if (dev->state & ST_OPENED) {
      offline_or_rewind_dev(dev);
   }
}
