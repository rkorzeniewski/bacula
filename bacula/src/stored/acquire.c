/*
 *  Routines to acquire and release a device for read/write
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

   new_lock_device_state(dev, BST_DOING_ACQUIRE);
   lock_device(dev);
   if (dev->state & ST_READ || dev->num_writers > 0) {
      Jmsg(jcr, M_FATAL, 0, _("Device %s is busy.\n"), dev_name(dev));
      new_unlock_device(dev);
      unlock_device(dev);
      return 0;
   }
   dev->state &= ~ST_LABEL;	      /* force reread of label */
   block_device(dev, BST_DOING_ACQUIRE);
   unlock_device(dev);
   stat = ready_dev_for_read(jcr, dev, block);	
#ifndef NEW_LOCK
   P(dev->mutex); 
   unblock_device(dev);
   V(dev->mutex);
#endif
   new_unlock_device(dev);
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
   int release = 0;
   int do_mount = 0;

   new_lock_device_state(dev, BST_DOING_ACQUIRE);
   lock_device(dev);
   Dmsg1(190, "acquire_append device is %s\n", dev_is_tape(dev)?"tape":"disk");


   if (dev->state & ST_APPEND) {
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
	    new_unlock_device(dev);
	    return 0;
	 }
	 /* Wrong tape mounted, release it, then fall through to get correct one */
	 release = 1;
	 do_mount = 1;
      }
   } else { 
      /* Not already in append mode, so mount the device */
      if (dev->state & ST_READ) {
         Jmsg(jcr, M_FATAL, 0, _("Device %s is busy reading.\n"), dev_name(dev));
	 unlock_device(dev);
	 new_unlock_device(dev);
	 return 0;
      } 
      ASSERT(dev->num_writers == 0);
      do_mount = 1;
   }

   if (do_mount) {
      block_device(dev, BST_DOING_ACQUIRE);
      unlock_device(dev);
      if (!mount_next_write_volume(jcr, dev, block, release)) {
         Jmsg(jcr, M_FATAL, 0, _("Could not ready device %s for append.\n"),
	    dev_name(dev));
#ifndef NEW_LOCK
	 P(dev->mutex);
	 unblock_device(dev);
	 unlock_device(dev);
#endif
	 new_unlock_device(dev);
	 return 0;
      }
#ifndef NEW_LOCK
      P(dev->mutex);
      unblock_device(dev);
#endif
   }

   dev->num_writers++;
   if (dev->num_writers > 1) {
      Dmsg2(100, "Hey!!!! There are %d writers on device %s\n", dev->num_writers,
	 dev_name(dev));
   }
   if (jcr->NumVolumes == 0) {
      jcr->NumVolumes = 1;
   }
   attach_jcr_to_device(dev, jcr);    /* attach jcr to device */
   unlock_device(dev);
   new_unlock_device(dev);
   return 1;			      /* got it */
}

/*
 * This job is done, so release the device. From a Unix standpoint,
 *  the device remains open.
 *
 */
int release_device(JCR *jcr, DEVICE *dev, DEV_BLOCK *block)
{
#ifndef NEW_LOCK
   P(dev->mutex);
#endif
   new_lock_device(dev);
   Dmsg1(100, "release_device device is %s\n", dev_is_tape(dev)?"tape":"disk");
   if (dev->state & ST_READ) {
      dev->state &= ~ST_READ;	      /* clear read bit */
      if (!dev_is_tape(dev) || !(dev->capabilities & CAP_ALWAYSOPEN)) {
	 if (dev->capabilities & CAP_OFFLINEUNMOUNT) {
	    offline_dev(dev);
	 }
	 close_dev(dev);
      }
      /******FIXME**** send read volume usage statistics to director */

   } else if (dev->num_writers > 0) {
      dev->num_writers--;
      Dmsg1(100, "There are %d writers in release_device\n", dev->num_writers);
      if (dev->num_writers == 0) {
	 weof_dev(dev, 1);
         Dmsg0(100, "dir_create_jobmedia_record. Release\n");
	 dir_create_jobmedia_record(jcr);
	 dev->VolCatInfo.VolCatFiles++; 	    /* increment number of files */
	 /* Note! do volume update before close, which zaps VolCatInfo */
         Dmsg0(100, "dir_update_vol_info. Release\n");
	 dir_update_volume_info(jcr, &dev->VolCatInfo, 0); /* send Volume info to Director */

	 if (!dev_is_tape(dev) || !(dev->capabilities & CAP_ALWAYSOPEN)) {
	    if (dev->capabilities & CAP_OFFLINEUNMOUNT) {
	       offline_dev(dev);
	    }
	    close_dev(dev);
	 }
      } else {
         Dmsg0(100, "dir_create_jobmedia_record. Release\n");
	 dir_create_jobmedia_record(jcr);
         Dmsg0(100, "dir_update_vol_info. Release\n");
	 dir_update_volume_info(jcr, &dev->VolCatInfo, 0); /* send Volume info to Director */
      }
   } else {
      Jmsg1(jcr, M_ERROR, 0, _("BAD ERROR: release_device %s not in use.\n"), dev_name(dev));
   }
   detach_jcr_from_device(dev, jcr);
#ifndef NEW_LOCK
   V(dev->mutex);
#endif
   new_unlock_device(dev);
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
       Dmsg1(120, "bstored: open vol=%s\n", jcr->VolumeName);
       if (open_dev(dev, jcr->VolumeName, READ_ONLY) < 0) {
          Jmsg(jcr, M_FATAL, 0, _("Open device %s volume %s failed, ERR=%s\n"), 
	      dev_name(dev), jcr->VolumeName, strerror_dev(dev));
	  return 0;
       }
       Dmsg1(129, "open_dev %s OK\n", dev_name(dev));
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
	    /* Mount a specific volume and no other */
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
