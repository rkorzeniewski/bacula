/*
 *
 *  Higher Level Device routines. 
 *  Knows about Bacula tape labels and such  
 *
 *  NOTE! In general, subroutines that have the word
 *        "device" in the name do locking.  Subroutines
 *        that have the word "dev" in the name do not
 *	  do locking.  Thus if xxx_device() calls
 *	  yyy_dev(), all is OK, but if xxx_device()
 *	  calls yyy_device(), everything will hang.
 *	  Obviously, no zzz_dev() is allowed to call
 *	  a www_device() or everything falls apart. 
 *
 * Concerning the routines lock_device() and block_device()
 *  see the end of this module for details.  In general,
 *  blocking a device leaves it in a state where all threads
 *  other than the current thread block when they attempt to 
 *  lock the device. They remain suspended (blocked) until the device
 *  is unblocked. So, a device is blocked during an operation
 *  that takes a long time (initialization, mounting a new
 *  volume, ...) locking a device is done for an operation
 *  that takes a short time such as writing data to the   
 *  device.
 *
 *
 *   Kern Sibbald, MM, MMI
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
static int ready_dev_for_append(JCR *jcr, DEVICE *dev, DEV_BLOCK *block);
static int mount_next_volume(JCR *jcr, DEVICE *dev, DEV_BLOCK *label_blk);

extern char my_name[];
extern int debug_level;


/********************************************************************* 
 * Acquire device for reading.	We permit (for the moment)
 *  only one reader.  We read the Volume label from the block and
 *  leave the block pointers just after the label.
 *
 *  Returns: 0 if failed for any reason
 *	     1 if successful
 */
int acquire_device_for_read(JCR *jcr, DEVICE *dev, DEV_BLOCK *block)
{
   int stat;

   lock_device(dev);
   if (dev->state & ST_READ || dev->num_writers > 0) {
      Jmsg(jcr, M_FATAL, 0, _("Device %s is busy.\n"), dev_name(dev));
      unlock_device(dev);
      return 0;
   }
   dev->state &= ~ST_LABEL;	      /* force reread of label */
   block_device(dev, BST_DOING_ACQUIRE);
   unlock_device(dev);
   stat = ready_dev_for_read(jcr, dev, block);	
   P(dev->mutex); 
   unblock_device(dev);
   V(dev->mutex);
   return stat;
}

/*
 * Acquire device for writing. We permit multiple writers.
 *  If this is the first one, we read the label.
 *
 *  Returns: 0 if failed for any reason
 *	     1 if successful
 */
int acquire_device_for_append(JCR *jcr, DEVICE *dev, DEV_BLOCK *block)
{

   lock_device(dev);
   Dmsg1(90, "acquire_append device is %s\n", dev_is_tape(dev)?"tape":"disk");
   if (!(dev->state & ST_APPEND)) {
      if (dev->state & ST_READ) {
         Jmsg(jcr, M_FATAL, 0, _("Device %s is busy reading.\n"), dev_name(dev));
	 unlock_device(dev);
	 return 0;
      } 
      ASSERT(dev->num_writers == 0);
      block_device(dev, BST_DOING_ACQUIRE);
      unlock_device(dev);
      if (!ready_dev_for_append(jcr, dev, block)) {
         Jmsg(jcr, M_FATAL, 0, _("Could not ready device %s for append.\n"),
	    dev_name(dev));
	 P(dev->mutex);
	 unblock_device(dev);
	 V(dev->mutex);
	 return 0;
      }
      P(dev->mutex);
      dev->VolCatInfo.VolCatJobs++;	    /* increment number of jobs on this media */
      dev->num_writers = 1;
      if (jcr->NumVolumes == 0) {
	 jcr->NumVolumes = 1;
      }
      unblock_device(dev);
      V(dev->mutex);
      return 1;
   } else { 
      /* 
       * Device already in append mode	 
       *
       * Check if we have the right Volume mounted   
       *  OK if AnonVols and volume info OK
       *  OK if next volume matches current volume
       *  otherwise mount desired volume obtained from
       *    dir_find_next_appendable_volume
       */
      strcpy(jcr->VolumeName, dev->VolHdr.VolName);
      if (((dev->capabilities & CAP_ANONVOLS) &&
	    !dir_get_volume_info(jcr)) ||
	  (!dir_find_next_appendable_volume(jcr) || 
	    strcmp(dev->VolHdr.VolName, jcr->VolumeName) != 0)) { /* wrong tape mounted */
	 if (dev->num_writers != 0) {
            Jmsg(jcr, M_FATAL, 0, _("Device %s is busy writing with another Volume.\n"), dev_name(dev));
	    unlock_device(dev);
	    return 0;
	 }
	 /* Wrong tape currently mounted */  
	 block_device(dev, BST_DOING_ACQUIRE);
	 unlock_device(dev);
	 if (!mount_next_volume(jcr, dev, block)) {
            Jmsg(jcr, M_FATAL, 0, _("Unable to mount desired volume.\n"));
	    P(dev->mutex);
	    unblock_device(dev);
	    V(dev->mutex);
	    return 0;
	 }
	 P(dev->mutex);
	 unblock_device(dev);
      }
   }
   dev->VolCatInfo.VolCatJobs++;	    /* increment number of jobs on this media */
   dev->num_writers++;
   if (dev->num_writers > 1) {
      Dmsg2(0, "Hey!!!! There are %d writers on device %s\n", dev->num_writers,
	 dev_name(dev));
   }
   if (jcr->NumVolumes == 0) {
      jcr->NumVolumes = 1;
   }
   unlock_device(dev);
   return 1;			      /* got it */
}

/*
 * This job is done, so release the device. From a Unix standpoint,
 *  the device remains open.
 *
 */
int release_device(JCR *jcr, DEVICE *dev, DEV_BLOCK *block)
{
   P(dev->mutex);
   Dmsg1(90, "release_device device is %s\n", dev_is_tape(dev)?"tape":"disk");
   if (dev->state & ST_READ) {
      dev->state &= ~ST_READ;	      /* clear read bit */
      if (!dev_is_tape(dev)) {
	 close_dev(dev);
      }
      /******FIXME**** send read volume info to director */

   } else if (dev->num_writers > 0) {
      dev->num_writers--;
      Dmsg1(90, "There are %d writers in release_device\n", dev->num_writers);
      if (dev->num_writers == 0) {
	 weof_dev(dev, 1);
	 dev->VolCatInfo.VolCatFiles++; 	    /* increment number of files */
	 /* Note! do volume update before close, which zaps VolCatInfo */
	 dir_update_volume_info(jcr, &dev->VolCatInfo);   /* send Volume info to Director */
	 if (!dev_is_tape(dev)) {
	    close_dev(dev);
	 } else {
            Dmsg0(90, "Device is tape leave open in release_device\n");
	 }
      } else {
	 dir_update_volume_info(jcr, &dev->VolCatInfo);   /* send Volume info to Director */
      }
   } else {
      Emsg1(M_ERROR, 0, _("BAD ERROR: release_device %s not in use.\n"), dev_name(dev));
   }
   V(dev->mutex);
   return 1;
}



/*
 * We rewind the current volume, which we no longer want, and
 *  ask the user (console) to mount the next volume.
 *
 *  Continue trying until we get it, and we call
 *  ready_dev_for_append() so that we can write on it.
 *
 * This routine retuns a 0 only if it is REALLY
 *  impossible to get the requested Volume.
 */
static int mount_next_volume(JCR *jcr, DEVICE *dev, DEV_BLOCK *label_blk)
{
   Dmsg0(90, "Enter mount_next_volume()\n");

   /* 
    * First erase all memory of the current volume   
    */
   dev->block_num = 0;
   dev->file = 0;
   dev->LastBlockNumWritten = 0;
   memset(&dev->VolCatInfo, 0, sizeof(dev->VolCatInfo));
   memset(&dev->VolHdr, 0, sizeof(dev->VolHdr));

   /* Keep trying until we get something good mounted */
   for ( ;; ) {
      if (job_cancelled(jcr)) {
         Mmsg0(&dev->errmsg, "Job cancelled.\n");
	 return 0;
      }

      if (dev->state & ST_OPENED && !rewind_dev(dev)) {
         Jmsg2(jcr, M_WARNING, 0, _("Rewind error on device %s. ERR=%s\n"), 
	       dev_name(dev), strerror_dev(dev));
      }

      /*
       * Ask to mount and wait if necessary   
       */
      if (!dir_ask_sysop_to_mount_next_volume(jcr, dev)) {
         Jmsg(jcr, M_FATAL, 0, _("Unable to mount next Volume on device %s\n"),
	    dev_name(dev));
	 return 0;
      }

      /* 
       * Ready output device for writing
       */
      Dmsg1(120, "just before ready_dev_for_append dev=%x\n", dev);
      if (!ready_dev_for_append(jcr, dev, label_blk)) {
	 continue;
      }
      dev->VolCatInfo.VolCatMounts++;
      jcr->VolFirstFile = 0;
      break;			   /* Got new volume, continue */
   }
   return 1;
}


/*
 * This routine ensures that the device is ready for
 * writing. We start from the assumption that there
 * may not be a tape mounted. 
 *
 * If the device is a file, we create the output
 * file. If it is a tape, we check the volume name
 * and move the tape to the end of data.
 *
 * It assumes that the device is not already in use!
 *
 *  Returns 0 on failure
 *  Returns 1 on success
 */
static int ready_dev_for_append(JCR *jcr, DEVICE *dev, DEV_BLOCK *block)
{
   int mounted = 0;
   int recycle = 0;

   Dmsg0(100, "Enter ready_dev_for_append\n");

   dev->state &= ~(ST_LABEL|ST_APPEND|ST_READ|ST_EOT|ST_WEOT|ST_EOF);


   for ( ;; ) {
      if (job_cancelled(jcr)) {
         Mmsg(&dev->errmsg, "Job %s cancelled.\n", jcr->Job);
	 return 0;
      }

      /* 
       * Ask Director for Volume Info (Name, attributes) to use.	 
       */
      if (!dir_find_next_appendable_volume(jcr)) {
	 if (!dir_ask_sysop_to_mount_next_volume(jcr, dev)) {
            Jmsg1(jcr, M_ERROR, 0, _("Unable to mount desired Volume for device %s.\n"),
	       dev_name(dev));
	    return 0;		   /* error return */
	 }
      }
      Dmsg1(200, "want vol=%s\n", jcr->VolumeName);

      /* Open device */
      for ( ; !(dev->state & ST_OPENED); ) {
	  if (open_dev(dev, jcr->VolCatInfo.VolCatName, READ_WRITE) < 0) {
	     if (dev->dev_errno == EAGAIN || dev->dev_errno == EBUSY) {
		sleep(30);
	     }
             Jmsg2(jcr, M_ERROR, 0, _("Unable to open device %s. ERR=%s\n"), 
		dev_name(dev), strerror_dev(dev));
	     return 0;
	  }
      }

      /*
       * Now make sure we have the right tape mounted
       */
      switch (read_dev_volume_label(jcr, dev, block)) {
	 case VOL_OK:
            Dmsg1(200, "Vol OK name=%s\n", jcr->VolumeName);
	    memcpy(&dev->VolCatInfo, &jcr->VolCatInfo, sizeof(jcr->VolCatInfo));
            if (strcmp(dev->VolCatInfo.VolCatStatus, "Recycle") == 0) {
	       recycle = 1;
	    }
	    break;		      /* got it */
	 case VOL_NAME_ERROR:
	    /* Check if we can accept this as an anonymous volume */
	    strcpy(jcr->VolumeName, dev->VolHdr.VolName);
	    if (!dev->capabilities & CAP_ANONVOLS ||
		!dir_get_volume_info(jcr)) {
	       goto mount_next_vol;
	    }
            Dmsg1(200, "want new name=%s\n", jcr->VolumeName);
	    memcpy(&dev->VolCatInfo, &jcr->VolCatInfo, sizeof(jcr->VolCatInfo));
	    break;

	 case VOL_NO_LABEL:
	 case VOL_IO_ERROR:
	    /* If permitted, create a label */
	    if (dev->capabilities & CAP_LABEL) {
               Dmsg0(90, "Create volume label\n");
	       if (!write_volume_label_to_dev(jcr, (DEVRES *)dev->device, jcr->VolumeName,
		      jcr->pool_name)) {
		  return 0;
	       }
               Jmsg(jcr, M_INFO, 0, _("Created Volume label %s on device %s.\n"),
		  jcr->VolumeName, dev_name(dev));
	       mounted = 1;
	       continue;	      /* read label we just wrote */
	    } 
	    /* NOTE! Fall-through wanted. */
	 default:
mount_next_vol:
	    /* Send error message generated by read_dev_volume_label() */
            Jmsg(jcr, M_WARNING, 0, "%s", jcr->errmsg);                         
	    rewind_dev(dev);
	    if (!dir_ask_sysop_to_mount_next_volume(jcr, dev)) {
               Jmsg1(jcr, M_ERROR, 0, _("Unable to mount desired Volume for device %s.\n"),
		  dev_name(dev));
	       return 0;	      /* error return */
	    }
	    mounted = 1;
	    continue;		      /* try reading again */
      }
      break;
   }
   if (mounted) {
      dev->VolCatInfo.VolCatMounts++;
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
      Dmsg1(90, "ready_for_append found freshly labeled volume. dev=%x\n", dev);
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
      if (!write_block_to_dev(dev, block)) {
         Jmsg2(jcr, M_ERROR, 0, _("Unable to write device %s. ERR=%s\n"),
	    dev_name(dev), strerror_dev(dev));
	 return 0;
      }
      if (!rewind_dev(dev)) {
         Jmsg2(jcr, M_ERROR, 0, _("Unable to rewind device %s. ERR=%s\n"),
	    dev_name(dev), strerror_dev(dev));
	 return 0;
      }
      /* Recreate a correct volume label and return it in the block */
      write_volume_label_to_block(jcr, dev, block);
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
      dir_update_volume_info(jcr, &dev->VolCatInfo);
      if (recycle) {
         Jmsg(jcr, M_INFO, 0, _("Recycled volume %s on device %s, all previous data lost.\n"),
	    jcr->VolumeName, dev_name(dev));
      } else {
         Jmsg(jcr, M_INFO, 0, _("Wrote label to prelabeled Volume %s on device %s\n"),
	    jcr->VolumeName, dev_name(dev));
      }

   } else {
      /* OK, at this point, we have a valid Bacula label, but
       * we need to position to the end of the volume.
       */
      Dmsg0(20, "Device previously written, moving to end of data\n");
      Jmsg(jcr, M_INFO, 0, _("Volume %s previously written, moving to end of data.\n"),
	 jcr->VolumeName);
      if (!eod_dev(dev)) {
         Jmsg(jcr, M_ERROR, 0, _("Unable to position to end of data %s. ERR=%s\n"),
	    dev_name(dev), strerror_dev(dev));
         Jmsg(jcr, M_INFO, 0, _("Marking Volume %s in Error in Catalog.\n"),
	    jcr->VolumeName);
         strcpy(dev->VolCatInfo.VolCatStatus, "Error");
	 dir_update_volume_info(jcr, &dev->VolCatInfo);
	 return 0;
      }
      /* *****FIXME**** we might do some checking for files too */
      if (dev_is_tape(dev)) {
         Jmsg(jcr, M_INFO, 0, _("Ready to append at EOM File=%d.\n"), dev_file(dev));
	 if (dev->VolCatInfo.VolCatFiles != dev_file(dev) + 1) {
	    /* ****FIXME**** this should refuse to write on tape */
            Jmsg(jcr, M_INFO, 0, _("Hey! Num files mismatch! Catalog Files=%d\n"), dev->VolCatInfo.VolCatFiles);
	 }
      }
      /* Return an empty block */
      empty_block(block);	      /* we used it for reading so set for write */
   }
   dev->state |= ST_APPEND;
   Dmsg0(100, "Normal return from read_dev_for_append\n");
   return 1; 
}

/*
 * This routine ensures that the device is ready for
 * reading. If it is a file, it opens it.
 * If it is a tape, it checks the volume name 
 *
 *  Returns 0 on failure
 *  Returns 1 on success
 */
int ready_dev_for_read(JCR *jcr, DEVICE *dev, DEV_BLOCK *block)
{
   if (!(dev->state & ST_OPENED)) {
       Dmsg1(20, "bstored: open vol=%s\n", jcr->VolumeName);
       if (open_dev(dev, jcr->VolumeName, READ_ONLY) < 0) {
          Jmsg(jcr, M_FATAL, 0, _("Open device %s volume %s failed, ERR=%s\n"), 
	      dev_name(dev), jcr->VolumeName, strerror_dev(dev));
	  return 0;
       }
       Dmsg1(29, "open_dev %s OK\n", dev_name(dev));
   }

   for (;;) {
      if (job_cancelled(jcr)) {
         Mmsg0(&dev->errmsg, _("Job cancelled.\n"));
	 return 0;
      }
      if (!rewind_dev(dev)) {
         Jmsg2(jcr, M_WARNING, 0, _("Rewind error on device %s. ERR=%s\n"), 
	       dev_name(dev), strerror_dev(dev));
      }
      switch (read_dev_volume_label(jcr, dev, block)) {
	 case VOL_OK:
	    break;		      /* got it */
	 default:
	    /* Send error message generated by read_dev_volume_label() */
            Jmsg(jcr, M_WARNING, 0, "%s", jcr->errmsg);                         
	    if (!rewind_dev(dev)) {
               Jmsg2(jcr, M_WARNING, 0, _("Rewind error on device %s. ERR=%s\n"), 
		     dev_name(dev), strerror_dev(dev));
	    }
	    if (!dir_ask_sysop_to_mount_volume(jcr, dev)) {
	       return 0;	      /* error return */
	    }
	    continue;		      /* try reading again */
      }
      break;
   }

   dev->state |= ST_READ;
   return 1; 
}

/*
 * This is the dreaded moment. We either have an end of
 * medium condition or worse, and error condition.
 * Attempt to "recover" by obtaining a new Volume.
 *
 * We enter with device locked, and 
 *     exit with device locked.
 *
 * Note, we are called only from one place in block.c
 *
 *  Returns: 1 on success
 *	     0 on failure
 */
int fixup_device_block_write_error(JCR *jcr, DEVICE *dev, DEV_BLOCK *block)
{
   uint32_t stat = 0;			
   char PrevVolName[MAX_NAME_LENGTH];
   DEV_BLOCK *label_blk;
   char b1[30], b2[30];
   time_t wait_time;

   wait_time = time(NULL);
   status_dev(dev, &stat);
   if (stat & MT_EOD) {
      Dmsg0(90, "======= Got EOD ========\n");

      block_device(dev, BST_DOING_ACQUIRE);

      strcpy(dev->VolCatInfo.VolCatStatus, "Full");
      Dmsg0(90, "Call update_vol_info\n");
      if (!dir_update_volume_info(jcr, &dev->VolCatInfo)) {    /* send Volume info to Director */
         Jmsg(jcr, M_ERROR, 0, _("Could not update Volume info Volume=%s Job=%s\n"),
	    dev->VolCatInfo.VolCatName, jcr->Job);
	 return 0;		      /* device locked */
      }
      Dmsg0(90, "Back from update_vol_info\n");

      strcpy(PrevVolName, dev->VolCatInfo.VolCatName);
      strcpy(dev->VolHdr.PrevVolName, PrevVolName);

      label_blk = new_block(dev);

      /* Inform User about end of media */
      Jmsg(jcr, M_INFO, 0, _("End of media on Volume %s Bytes=%s Blocks=%s.\n"), 
	   PrevVolName, edit_uint64_with_commas(dev->VolCatInfo.VolCatBytes, b1),
	   edit_uint64_with_commas(dev->VolCatInfo.VolCatBlocks, b2));

      if (!dev_is_tape(dev)) {		 /* If file, */
	 close_dev(dev);		 /* yes, close it */
      }

      /* Unlock, but leave BLOCKED */
      unlock_device(dev);
      if (!mount_next_volume(jcr, dev, label_blk)) {
	 P(dev->mutex);
	 unblock_device(dev);
	 return 0;		      /* device locked */
      }

      P(dev->mutex);		      /* lock again */

      Jmsg(jcr, M_INFO, 0, _("New volume %s mounted on device %s\n"),
	 jcr->VolumeName, dev_name(dev));

      /* 
       * If this is a new tape, the label_blk will contain the
       *  label, so write it now. If this is a previously
       *  used tape, mount_next_volume() will return an
       *  empty label_blk, and nothing will be written.
       */
      Dmsg0(90, "write label block to dev\n");
      if (!write_block_to_dev(dev, label_blk)) {
         Dmsg1(0, "write_block_to_device Volume label failed. ERR=%s",
	   strerror_dev(dev));
	 free_block(label_blk);
	 unblock_device(dev);
	 return 0;		      /* device locked */
      }

      /* Write overflow block to tape */
      Dmsg0(90, "Write overflow block to dev\n");
      if (!write_block_to_dev(dev, block)) {
         Dmsg1(0, "write_block_to_device overflow block failed. ERR=%s",
	   strerror_dev(dev));
	 free_block(label_blk);
	 unblock_device(dev);
	 return 0;		      /* device locked */
      }

      jcr->NumVolumes++;
      Dmsg0(90, "Wake up any waiting threads.\n");
      free_block(label_blk);
      unblock_device(dev);
      jcr->run_time += time(NULL) - wait_time; /* correct run time */
      return 1; 			       /* device locked */
   }
   free_block(label_blk);
   return 0;			      /* device locked */
}


/*
 *   Open the device. Expect dev to already be initialized.  
 *
 *   This routine is used only when the Storage daemon starts 
 *   and always_open is set, and in the stand-alone utility
 *   routines such as bextract.
 *
 *   Note, opening of a normal file is deferred to later so
 *    that we can get the filename; the device_name for
 *    a file is the directory only. 
 *
 *   Retuns: 0 on failure
 *	     1 on success
 */
int open_device(DEVICE *dev)
{
   Dmsg0(20, "start open_output_device()\n");
   if (!dev) {
      return 0;
   }

   lock_device(dev);

   /* Defer opening files */
   if (!dev_is_tape(dev)) {
      Dmsg0(29, "Device is file, deferring open.\n");
      unlock_device(dev);
      return 1;
   }

   if (!(dev->state & ST_OPENED)) {
      Dmsg0(29, "Opening device.\n");
      if (open_dev(dev, NULL, READ_WRITE) < 0) {
         Emsg1(M_FATAL, 0, _("dev open failed: %s\n"), dev->errmsg);
	 unlock_device(dev);
	 return 0;
      }
   }
   Dmsg1(29, "open_dev %s OK\n", dev_name(dev));

   unlock_device(dev);
   return 1;
}


/* 
 * When dev_blocked is set, all threads EXCEPT thread with id no_wait_id
 * must wait. The no_wait_id thread is out obtaining a new volume
 * and preparing the label.
 */
void lock_device(DEVICE *dev)
{
   int stat;

   Dmsg1(90, "lock %d\n", dev->dev_blocked);
   P(dev->mutex);
   if (dev->dev_blocked && !pthread_equal(dev->no_wait_id, pthread_self())) {
      dev->num_waiting++;	      /* indicate that I am waiting */
      while (dev->dev_blocked) {
	 if ((stat = pthread_cond_wait(&dev->wait, &dev->mutex)) != 0) {
	    V(dev->mutex);
            Emsg1(M_ABORT, 0, _("pthread_cond_wait failure. ERR=%s\n"),
	       strerror(stat));
	 }
      }
      dev->num_waiting--;	      /* no longer waiting */
   }
}

void unlock_device(DEVICE *dev) 
{
   Dmsg0(90, "unlock\n");
   V(dev->mutex);
}

/* 
 * Block all other threads from using the device
 *  Device must already be locked.  After this call,
 *  the device is blocked to any thread calling lock_device(),
 *  but the device is not locked (i.e. no P on device).  Also,
 *  the current thread can do slip through the lock_device()
 *  calls without blocking.
 */
void block_device(DEVICE *dev, int state)
{
   Dmsg1(90, "block set %d\n", state);
   ASSERT(dev->dev_blocked == BST_NOT_BLOCKED);
   dev->dev_blocked = state;	      /* make other threads wait */
   dev->no_wait_id = pthread_self();  /* allow us to continue */
}

/*
 * Unblock the device, and wake up anyone who went to sleep.
 */
void unblock_device(DEVICE *dev)
{
   Dmsg1(90, "unblock %d\n", dev->dev_blocked);
   ASSERT(dev->dev_blocked);
   dev->dev_blocked = BST_NOT_BLOCKED;
   if (dev->num_waiting > 0) {
      pthread_cond_broadcast(&dev->wait); /* wake them up */
   }
}
