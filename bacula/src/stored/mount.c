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

#include "bacula.h"                   /* pull in global headers */
#include "stored.h"                   /* pull in Storage Deamon headers */

/* Forward referenced functions */
static char *edit_device_codes(JCR *jcr, char *omsg, char *imsg, char *cmd);


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
      Jmsg(jcr, M_FATAL, 0, _("Too many errors trying to mount device %s.\n"), dev_name(dev));
      return 0;
   }
   if (job_cancelled(jcr)) {
      Jmsg(jcr, M_FATAL, 0, _("Job cancelled.\n"));
      return 0;
   }
   recycle = ask = autochanger = 0;
   if (release) {
      Dmsg0(100, "mount_next_volume release=1\n");
      /* 
       * First erase all memory of the current volume	
       */
      dev->block_num = 0;
      dev->file = 0;
      dev->LastBlockNumWritten = 0;
      memset(&dev->VolCatInfo, 0, sizeof(dev->VolCatInfo));
      memset(&jcr->VolCatInfo, 0, sizeof(jcr->VolCatInfo));
      memset(&dev->VolHdr, 0, sizeof(dev->VolHdr));
      dev->state &= ~ST_LABEL;	      /* label not yet read */
      jcr->VolumeName[0] = 0;

      if (!dev_is_tape(dev) || !(dev->capabilities & CAP_ALWAYSOPEN)) {
	 if (dev->capabilities & CAP_OFFLINEUNMOUNT) {
	    offline_dev(dev);
	 }
	 close_dev(dev);
      }

      /* If we have not closed the device, then at least rewind the tape */
      if (dev->state & ST_OPENED) {
	 if (dev->capabilities & CAP_OFFLINEUNMOUNT) {
	    offline_dev(dev);
	 }
	 if (!rewind_dev(dev)) {
            Jmsg2(jcr, M_WARNING, 0, _("Rewind error on device %s. ERR=%s\n"), 
		  dev_name(dev), strerror_dev(dev));
	 }
      }
      ask = 1;			      /* ask operator to mount tape */
   }

   /* 
    * Get Director's idea of what tape we should have mounted. 
    */
   if (!dir_find_next_appendable_volume(jcr)) {
      ask = 1;			   /* we must ask */
      Dmsg0(100, "did not find next volume. Must ask.\n");
   }
   Dmsg2(100, "After find_next_append. Vol=%s Slot=%d\n",
      jcr->VolCatInfo.VolCatName, jcr->VolCatInfo.Slot);
   release = 1;                       /* release if we "recurse" */

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

   jcr->VolFirstFile = jcr->JobFiles; /* first update of Vol FileIndex */
   for ( ;; ) {
      autochanger = autoload_device(jcr, dev, 1, NULL);
      if (autochanger) {
	 ask = 0;		      /* if autochange no need to ask sysop */
      }

      if (ask && !dir_ask_sysop_to_mount_next_volume(jcr, dev)) {
         Dmsg0(100, "Error return ask_sysop ...\n");
	 return 0;		/* error return */
      }
      Dmsg1(100, "want vol=%s\n", jcr->VolumeName);

      /* Open device */
      for ( ; !(dev->state & ST_OPENED); ) {
	  if (open_dev(dev, jcr->VolCatInfo.VolCatName, READ_WRITE) < 0) {
	     if (dev->dev_errno == EAGAIN || dev->dev_errno == EBUSY) {
		sleep(30);
	     }
             Jmsg2(jcr, M_FATAL, 0, _("Unable to open device %s. ERR=%s\n"), 
		dev_name(dev), strerror_dev(dev));
	     return 0;
	  }
      }

      /*
       * Now make sure we have the right tape mounted
       */
read_volume:
      switch (read_dev_volume_label(jcr, dev, block)) {
	 case VOL_OK:
            Dmsg1(100, "Vol OK name=%s\n", jcr->VolumeName);
	    memcpy(&dev->VolCatInfo, &jcr->VolCatInfo, sizeof(jcr->VolCatInfo));
            if (strcmp(dev->VolCatInfo.VolCatStatus, "Recycle") == 0) {
	       recycle = 1;
	    }
	    break;		      /* got it */
	 case VOL_NAME_ERROR:
            Dmsg1(100, "Vol NAME Error Name=%s\n", jcr->VolumeName);
	    /* 
	     * OK, we got a different volume mounted. First save the
	     *	requested Volume info in the dev structure, then query if
	     *	this volume is really OK. If not, put back the desired
	     *	volume name and continue.
	     */
	    memcpy(&dev->VolCatInfo, &jcr->VolCatInfo, sizeof(jcr->VolCatInfo));
	    /* Check if this is a valid Volume in the pool */
	    strcpy(jcr->VolumeName, dev->VolHdr.VolName);
	    if (!dir_get_volume_info(jcr, 1)) {
               Mmsg(&jcr->errmsg, _("Wanted Volume \"%s\".\n\
    Actual Volume \"%s\" not acceptable.\n"),
		  dev->VolCatInfo.VolCatName, dev->VolHdr.VolName);
	       /* Restore desired volume name, note device info out of sync */
	       memcpy(&jcr->VolCatInfo, &dev->VolCatInfo, sizeof(jcr->VolCatInfo));
	       goto mount_error;
	    }
            Dmsg1(100, "want new name=%s\n", jcr->VolumeName);
	    memcpy(&dev->VolCatInfo, &jcr->VolCatInfo, sizeof(jcr->VolCatInfo));
	    break;

	 case VOL_NO_LABEL:
	 case VOL_IO_ERROR:
            Dmsg1(500, "Vol NO_LABEL or IO_ERROR name=%s\n", jcr->VolumeName);
	    /* If permitted, create a label */
	    if (dev->capabilities & CAP_LABEL) {
               Dmsg0(100, "Create volume label\n");
	       if (!write_volume_label_to_dev(jcr, (DEVRES *)dev->device, jcr->VolumeName,
		      jcr->pool_name)) {
                  Dmsg0(100, "!write_vol_label\n");
		  goto mount_next_vol;
	       }
               Jmsg(jcr, M_INFO, 0, _("Created Volume label %s on device %s.\n"),
		  jcr->VolumeName, dev_name(dev));
	       goto read_volume;      /* read label we just wrote */
	    } 
	    /* NOTE! Fall-through wanted. */
	 default:
mount_error:
	    /* Send error message */
            Jmsg1(jcr, M_WARNING, 0, "%s", jcr->errmsg);                         
	    if (autochanger) {
               Jmsg(jcr, M_ERROR, 0, _("Autochanger Volume \"%s\" not found in slot %d.\n\
    Setting slot to zero in catalog.\n"),
		  jcr->VolCatInfo.VolCatName, jcr->VolCatInfo.Slot);
	       jcr->VolCatInfo.Slot = 0; /* invalidate slot */
	       dir_update_volume_info(jcr, &jcr->VolCatInfo, 1);  /* set slot */
	    }
            Dmsg0(100, "Default\n");
	    goto mount_next_vol;
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
       * Write the block now to ensure we have write permission.
       *  It is better to find out now rather than later.
       */
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
      /* Set or reset Volume statistics */
      dev->VolCatInfo.VolCatJobs = 1;
      dev->VolCatInfo.VolCatFiles = 1;
      dev->VolCatInfo.VolCatErrors = 0;
      dev->VolCatInfo.VolCatBlocks = 1;
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
      Dmsg0(100, "dir_update_vol_info. Set Append\n");
      dir_update_volume_info(jcr, &dev->VolCatInfo, 1);  /* indicate doing relabel */
      if (recycle) {
         Jmsg(jcr, M_INFO, 0, _("Recycled volume %s on device %s, all previous data lost.\n"),
	    jcr->VolumeName, dev_name(dev));
      } else {
         Jmsg(jcr, M_INFO, 0, _("Wrote label to prelabeled Volume %s on device %s\n"),
	    jcr->VolumeName, dev_name(dev));
      }

   } else {
      /*
       * OK, at this point, we have a valid Bacula label, but
       * we need to position to the end of the volume, since we are
       * just now putting it into append mode.
       */
      Dmsg0(200, "Device previously written, moving to end of data\n");
      Jmsg(jcr, M_INFO, 0, _("Volume %s previously written, moving to end of data.\n"),
	 jcr->VolumeName);
      if (!eod_dev(dev)) {
         Jmsg(jcr, M_ERROR, 0, _("Unable to position to end of data %s. ERR=%s\n"),
	    dev_name(dev), strerror_dev(dev));
         Jmsg(jcr, M_INFO, 0, _("Marking Volume %s in Error in Catalog.\n"),
	    jcr->VolumeName);
         strcpy(dev->VolCatInfo.VolCatStatus, "Error");
         Dmsg0(100, "dir_update_vol_info. Set Error.\n");
	 dir_update_volume_info(jcr, &dev->VolCatInfo, 0);
	 goto mount_next_vol;
      }
      /* *****FIXME**** we should do some checking for files too */
      if (dev_is_tape(dev)) {
	 /*
	  * Check if we are positioned on the tape at the same place
	  * that the database says we should be.
	  */
	 if (dev->VolCatInfo.VolCatFiles == dev_file(dev) + 1) {
            Jmsg(jcr, M_INFO, 0, _("Ready to append to end of Volume at file=%d.\n"), 
		 dev_file(dev));
	 } else {
            Jmsg(jcr, M_ERROR, 0, _("I canot write on this volume because:\n\
The number of files mismatch! Volume=%d Catalog=%d\n"), 
		 dev_file(dev)+1, dev->VolCatInfo.VolCatFiles);
            strcpy(dev->VolCatInfo.VolCatStatus, "Error");
            Dmsg0(100, "dir_update_vol_info. Set Error.\n");
	    dir_update_volume_info(jcr, &dev->VolCatInfo, 0);
	    goto mount_next_vol;
	 }
      }
      dev->VolCatInfo.VolCatMounts++;	   /* Update mounts */
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
         Emsg2(M_FATAL, 0, "Cannot open Dev=%s, Vol=%s\n", dev_name(dev),
	       jcr->VolumeName);
	 return 0;
      }
      return 1; 		      /* next volume mounted */
   }
   Dmsg0(90, "End of Device reached.\n");
   return 0;
}

/*
 * Called here to do an autoload using the autochanger, if
 *  configured, and if a Slot has been defined for this Volume.
 *  On success this routine loads the indicated tape, but the
 *  label is not read, so it must be verified.
 *
 *  Note if dir is not NULL, it is the console requesting the 
 *   autoload for labeling, so we respond directly to the
 *   dir bsock.
 *
 *  Returns: 1 on success
 *	     0 on failure
 */
int autoload_device(JCR *jcr, DEVICE *dev, int writing, BSOCK *dir)
{
   int slot = jcr->VolCatInfo.Slot;
   int rtn_stat = 0;
     
   /*
    * Handle autoloaders here.	If we cannot autoload it, we
    *  will return FALSE to ask the sysop.
    */
   if (writing && dev->capabilities && CAP_AUTOCHANGER && slot <= 0) {
      if (dir) {
	 return 0;		      /* For user, bail out right now */
      }
      if (dir_find_next_appendable_volume(jcr)) {
	 slot = jcr->VolCatInfo.Slot; 
      }
   }
   Dmsg1(100, "Want changer slot=%d\n", slot);

   if (slot > 0 && jcr->device->changer_name && jcr->device->changer_command) {
      uint32_t timeout = jcr->device->max_changer_wait;
      POOLMEM *changer, *results;
      int status, loaded;

      results = get_pool_memory(PM_MESSAGE);
      changer = get_pool_memory(PM_FNAME);

      /* Find out what is loaded, zero means device is unloaded */
      changer = edit_device_codes(jcr, changer, jcr->device->changer_command, 
                   "loaded");
      status = run_program(changer, timeout, results);
      if (status == 0) {
	 loaded = atoi(results);
      } else {
	 loaded = -1;		   /* force unload */
      }
      Dmsg1(100, "loaded=%s\n", results);

      /* If bad status or tape we want is not loaded, load it. */
      if (status != 0 || loaded != slot) { 
	 if (dev->capabilities & CAP_OFFLINEUNMOUNT) {
	    offline_dev(dev);
	 }
	 /* We are going to load a new tape, so close the device */
	 force_close_dev(dev);
	 if (loaded != 0) {	   /* must unload drive */
            Dmsg0(100, "Doing changer unload.\n");
	    if (dir) {
               bnet_fsend(dir, _("3902 Issuing autochanger \"unload\" command.\n"));
	    } else {
               Jmsg(jcr, M_INFO, 0, _("Issuing autochanger \"unload\" command.\n"));
	    }
	    changer = edit_device_codes(jcr, changer, 
                        jcr->device->changer_command, "unload");
	    status = run_program(changer, timeout, NULL);
            Dmsg1(100, "unload status=%d\n", status);
	 }
	 /*
	  * Load the desired cassette	 
	  */
         Dmsg1(100, "Doing changer load slot %d\n", slot);
	 if (dir) {
            bnet_fsend(dir, _("3903 Issuing autochanger \"load slot %d\" command.\n"),
	       slot);
	 } else {
            Jmsg(jcr, M_INFO, 0, _("Issuing autochanger \"load slot %d\" command.\n"),
	       slot);
	 }
	 changer = edit_device_codes(jcr, changer, 
                      jcr->device->changer_command, "load");
	 status = run_program(changer, timeout, NULL);
         Dmsg2(100, "load slot %d status=%d\n", slot, status);
      }
      free_pool_memory(changer);
      free_pool_memory(results);
      Dmsg1(100, "After changer, status=%d\n", status);
      if (status == 0) {	   /* did we succeed? */
	 rtn_stat = 1;		   /* tape loaded by changer */
      }
   }
   return rtn_stat;
}



/*
 * Edit codes into ChangerCommand
 *  %% = %
 *  %a = archive device name
 *  %c = changer device name
 *  %f = Client's name
 *  %j = Job name
 *  %o = command
 *  %s = Slot base 0
 *  %S = Slot base 1
 *  %v = Volume name
 *
 *
 *  omsg = edited output message
 *  imsg = input string containing edit codes (%x)
 *  cmd = command string (load, unload, ...) 
 *
 */
static char *edit_device_codes(JCR *jcr, char *omsg, char *imsg, char *cmd) 
{
   char *p;
   const char *str;
   char add[20];

   *omsg = 0;
   Dmsg1(200, "edit_device_codes: %s\n", imsg);
   for (p=imsg; *p; p++) {
      if (*p == '%') {
	 switch (*++p) {
         case '%':
            str = "%";
	    break;
         case 'a':
	    str = jcr->device->dev->dev_name;
	    break;
         case 'c':
	    str = NPRT(jcr->device->changer_name);
	    break;
         case 'o':
	    str = NPRT(cmd);
	    break;
         case 's':
            sprintf(add, "%d", jcr->VolCatInfo.Slot - 1);
	    str = add;
	    break;
         case 'S':
            sprintf(add, "%d", jcr->VolCatInfo.Slot);
	    str = add;
	    break;
         case 'j':                    /* Job name */
	    str = jcr->Job;
	    break;
         case 'v':
	    str = NPRT(jcr->VolumeName);
	    break;
         case 'f':
	    str = NPRT(jcr->client_name);
	    break;

	 default:
            add[0] = '%';
	    add[1] = *p;
	    add[2] = 0;
	    str = add;
	    break;
	 }
      } else {
	 add[0] = *p;
	 add[1] = 0;
	 str = add;
      }
      Dmsg1(200, "add_str %s\n", str);
      pm_strcat(&omsg, (char *)str);
      Dmsg1(200, "omsg=%s\n", omsg);
   }
   return omsg;
}
