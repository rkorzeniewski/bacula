/*
 *  Routines to acquire and release a device for read/write
 *
 *   Kern Sibbald, August MMII
 *
 *   Version $Id$
 */
/*
   Copyright (C) 2002-2005 Kern Sibbald

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

static int can_reserve_drive(DCR *dcr);

/*
 * Create a new Device Control Record and attach
 *   it to the device (if this is a real job).
 */
DCR *new_dcr(JCR *jcr, DEVICE *dev)
{
   if (jcr && jcr->dcr) {
      return jcr->dcr;
   }
   DCR *dcr = (DCR *)malloc(sizeof(DCR));
   memset(dcr, 0, sizeof(DCR));
   if (jcr) {
      jcr->dcr = dcr;
   }
   dcr->jcr = jcr;
   dcr->dev = dev;
   if (dev) {
      dcr->device = dev->device;
   }
   dcr->block = new_block(dev);
   dcr->rec = new_record();
   dcr->spool_fd = -1;
   dcr->max_spool_size = dev->device->max_spool_size;
   /* Attach this dcr only if dev is initialized */
   if (dev->fd != 0 && jcr && jcr->JobType != JT_SYSTEM) {
      dev->attached_dcrs->append(dcr);	/* attach dcr to device */
//    jcr->dcrs->append(dcr);	      /* put dcr in list for Job */
   }
   return dcr;
}

/*
 * Search the dcrs list for the given dcr. If it is found,
 *  as it should be, then remove it. Also zap the jcr pointer
 *  to the dcr if it is the same one.
 */
#ifdef needed
static void remove_dcr_from_dcrs(DCR *dcr)
{
   JCR *jcr = dcr->jcr;
   if (jcr->dcrs) {
      int i = 0;
      DCR *ldcr;
      int num = jcr->dcrs->size();
      for (i=0; i < num; i++) {
	 ldcr = (DCR *)jcr->dcrs->get(i);
	 if (ldcr == dcr) {
	    jcr->dcrs->remove(i);
	    if (jcr->dcr == dcr) {
	       jcr->dcr = NULL;
	    }
	 }
      }
   }
}
#endif

/*
 * Free up all aspects of the given dcr -- i.e. dechain it,
 *  release allocated memory, zap pointers, ...
 */
void free_dcr(DCR *dcr)
{
   JCR *jcr = dcr->jcr;
   DEVICE *dev = dcr->dev;

   /*
    * If we reserved the device, we must decrement the
    *  number of writers.
    */
   if (dcr->reserved_device) {
      lock_device(dev);
      dev->num_writers--;
      if (dev->num_writers < 0) {
         Jmsg1(dcr->jcr, M_ERROR, 0, _("Hey! num_writers=%d!!!!\n"), dev->num_writers);
	 dev->num_writers = 0;
	 dcr->reserved_device = false;
      }
      unlock_device(dev);
   }

   /* Detach this dcr only if the dev is initialized */
   if (dev->fd != 0 && jcr && jcr->JobType != JT_SYSTEM) {
      dev->attached_dcrs->remove(dcr);	/* detach dcr from device */
//    remove_dcr_from_dcrs(dcr);      /* remove dcr from jcr list */
   }
   if (dcr->block) {
      free_block(dcr->block);
   }
   if (dcr->rec) {
      free_record(dcr->rec);
   }
   if (dcr->jcr) {
      dcr->jcr->dcr = NULL;
   }
   free(dcr);
}


/*
 * We "reserve" the drive by setting the ST_READ bit. No one else
 *  should touch the drive until that is cleared.
 *  This allows the DIR to "reserve" the device before actually
 *  starting the job. If the device is not available, the DIR
 *  can wait (to be implemented 1/05).
 */
bool reserve_device_for_read(DCR *dcr)
{
   DEVICE *dev = dcr->dev;
   JCR *jcr = dcr->jcr;
   bool first;

   ASSERT(dcr);

   init_device_wait_timers(dcr);

   dev->block(BST_DOING_ACQUIRE);

   Mmsg(jcr->errmsg, _("Device %s is BLOCKED due to user unmount.\n"),
	dev->print_name());
   for (first=true; device_is_unmounted(dev); first=false) {
      dev->unblock();
      if (!wait_for_device(dcr, jcr->errmsg, first))  {
	 return false;
      }
     dev->block(BST_DOING_ACQUIRE);
   }

   Mmsg2(jcr->errmsg, _("Device %s is busy. Job %d canceled.\n"),
	 dev->print_name(), jcr->JobId);
   for (first=true; dev->is_busy(); first=false) {
      dev->unblock();
      if (!wait_for_device(dcr, jcr->errmsg, first)) {
	 return false;
      }
      dev->block(BST_DOING_ACQUIRE);
   }

   dev->clear_append();
   dev->set_read();
   dev->unblock();
   return true;
}


/*********************************************************************
 * Acquire device for reading. 
 *  The drive should have previously been reserved by calling 
 *  reserve_device_for_read(). We read the Volume label from the block and
 *  leave the block pointers just after the label.
 *
 *  Returns: NULL if failed for any reason
 *	     dcr  if successful
 */
DCR *acquire_device_for_read(DCR *dcr)
{
   DEVICE *dev = dcr->dev;
   JCR *jcr = dcr->jcr;
   bool vol_ok = false;
   bool tape_previously_mounted;
   bool tape_initially_mounted;
   VOL_LIST *vol;
   bool try_autochanger = true;
   int i;
   int vol_label_status;
   
   dev->block(BST_DOING_ACQUIRE);

   if (dev->num_writers > 0) {
      Jmsg2(jcr, M_FATAL, 0, _("Num_writers=%d not zero. Job %d canceled.\n"), 
	 dev->num_writers, jcr->JobId);
      goto get_out;
   }

   /* Find next Volume, if any */
   vol = jcr->VolList;
   if (!vol) {
      Jmsg(jcr, M_FATAL, 0, _("No volumes specified. Job %d canceled.\n"), jcr->JobId);
      goto get_out;
   }
   jcr->CurVolume++;
   for (i=1; i<jcr->CurVolume; i++) {
      vol = vol->next;
   }
   if (!vol) {
      goto get_out;		      /* should not happen */	
   }
   bstrncpy(dcr->VolumeName, vol->VolumeName, sizeof(dcr->VolumeName));

   init_device_wait_timers(dcr);

   tape_previously_mounted = dev->can_read() ||
			     dev->can_append() ||
			     dev->is_labeled();
   tape_initially_mounted = tape_previously_mounted;


   /* Volume info is always needed because of VolParts */
   Dmsg0(200, "dir_get_volume_info\n");
   if (!dir_get_volume_info(dcr, GET_VOL_INFO_FOR_READ)) {
      Jmsg1(jcr, M_WARNING, 0, "%s", jcr->errmsg);
   }
   
   dev->num_parts = dcr->VolCatInfo.VolCatParts;
   
   for (i=0; i<5; i++) {
      dev->clear_labeled();		 /* force reread of label */
      if (job_canceled(jcr)) {
         Mmsg1(dev->errmsg, _("Job %d canceled.\n"), jcr->JobId);
	 goto get_out;		      /* error return */
      }
      /*
       * This code ensures that the device is ready for
       * reading. If it is a file, it opens it.
       * If it is a tape, it checks the volume name
       */
      for ( ; !dev->is_open(); ) {
         Dmsg1(120, "bstored: open vol=%s\n", dcr->VolumeName);
	 if (open_dev(dev, dcr->VolumeName, OPEN_READ_ONLY) < 0) {
	    if (dev->dev_errno == EIO) {   /* no tape loaded */
              Jmsg3(jcr, M_WARNING, 0, _("Open device %s Volume \"%s\" failed: ERR=%s\n"),
		    dev->print_name(), dcr->VolumeName, strerror_dev(dev));
	       goto default_path;
	    }
	    
	    /* If we have a dvd that requires mount, 
	     * we need to try to open the label, so the info can be reported
	     * if a wrong volume has been mounted. */
	    if (dev->is_dvd() && (dcr->VolCatInfo.VolCatParts > 0)) {
	       break;
	    }
	    
            Jmsg3(jcr, M_FATAL, 0, _("Open device %s Volume \"%s\" failed: ERR=%s\n"),
		dev->print_name(), dcr->VolumeName, strerror_dev(dev));
	    goto get_out;
	 }
         Dmsg1(129, "open_dev %s OK\n", dev->print_name());
      }
      
      if (dev->is_dvd()) {
	 vol_label_status = read_dev_volume_label_guess(dcr, 0);
      } else {
	 vol_label_status = read_dev_volume_label(dcr);
      }
      
      Dmsg0(200, "calling read-vol-label\n");
      switch (vol_label_status) {
      case VOL_OK:
	 vol_ok = true;
	 memcpy(&dev->VolCatInfo, &dcr->VolCatInfo, sizeof(dev->VolCatInfo));
	 break; 		   /* got it */
      case VOL_IO_ERROR:
	 /*
	  * Send error message generated by read_dev_volume_label()
	  *  only we really had a tape mounted. This supresses superfluous
	  *  error messages when nothing is mounted.
	  */
	 if (tape_previously_mounted) {
            Jmsg(jcr, M_WARNING, 0, "%s", jcr->errmsg);
	 }
	 goto default_path;
      case VOL_NAME_ERROR:
	 if (tape_initially_mounted) {
	    tape_initially_mounted = false;
	    goto default_path;
	 }
	 /* Fall through */
      default:
         Jmsg1(jcr, M_WARNING, 0, "%s", jcr->errmsg);
default_path:
	 tape_previously_mounted = true;
	 
	 /* If the device requires mount, close it, so the device can be ejected.
	  * FIXME: This should perhaps be done for all devices. */
	 if (dev_cap(dev, CAP_REQMOUNT)) {
	    force_close_dev(dev);
	 }
	 
	 /* Call autochanger only once unless ask_sysop called */
	 if (try_autochanger) {
	    int stat;
            Dmsg2(200, "calling autoload Vol=%s Slot=%d\n",
	       dcr->VolumeName, dcr->VolCatInfo.Slot);
	    stat = autoload_device(dcr, 0, NULL);
	    if (stat > 0) {
	       try_autochanger = false;
	       continue;	      /* try reading volume mounted */
	    }
	 }
	 
	 /* Mount a specific volume and no other */
         Dmsg0(200, "calling dir_ask_sysop\n");
	 if (!dir_ask_sysop_to_mount_volume(dcr)) {
	    goto get_out;	      /* error return */
	 }
	 try_autochanger = true;      /* permit using autochanger again */
	 continue;		      /* try reading again */
      } /* end switch */
      break;
   } /* end for loop */
   if (!vol_ok) {
      Jmsg1(jcr, M_FATAL, 0, _("Too many errors trying to mount device %s.\n"),
	    dev->print_name());
      goto get_out;
   }

   dev->clear_append();
   dev->set_read();
   set_jcr_job_status(jcr, JS_Running);
   dir_send_job_status(jcr);
   Jmsg(jcr, M_INFO, 0, _("Ready to read from volume \"%s\" on device %s.\n"),
      dcr->VolumeName, dev->print_name());

get_out:
   dev->unblock();
   if (!vol_ok) {
      free_dcr(dcr);
      dcr = NULL;
   }
   return dcr;
}

/*
 * We reserve the device for appending by incrementing the 
 *  reserved_device. We do virtually all the same work that
 *  is done in acquire_device_for_append(), but we do
 *  not attempt to mount the device. This routine allows
 *  the DIR to reserve multiple devices before *really* 
 *  starting the job. It also permits the SD to refuse 
 *  certain devices (not up, ...).
 *
 * Note, in reserving a device, if the device is for the
 *  same pool and the same pool type, then it is acceptable.
 *  The Media Type has already been checked. If we are
 *  the first tor reserve the device, we put the pool
 *  name and pool type in the device record.
 */
bool reserve_device_for_append(DCR *dcr)
{
   JCR *jcr = dcr->jcr;
   DEVICE *dev = dcr->dev;
   bool ok = false;
   bool first;

   ASSERT(dcr);

   init_device_wait_timers(dcr);

   dev->block(BST_DOING_ACQUIRE);

   Mmsg1(jcr->errmsg, _("Device %s is busy reading.\n"),
	 dev->print_name());
   for (first=true; dev->can_read(); first=false) {
      dev->unblock();
      if (!wait_for_device(dcr, jcr->errmsg, first)) {
	 return false;
      }
      dev->block(BST_DOING_ACQUIRE);
   }


   Mmsg(jcr->errmsg, _("Device %s is BLOCKED due to user unmount.\n"),
	dev->print_name());
   for (first=true; device_is_unmounted(dev); first=false) {
      dev->unblock();	   
      if (!wait_for_device(dcr, jcr->errmsg, first))  {
	 return false;
      }
     dev->block(BST_DOING_ACQUIRE);
   }

   Dmsg1(190, "reserve_append device is %s\n", dev_is_tape(dev)?"tape":"disk");

   for ( ;; ) {
      switch (can_reserve_drive(dcr)) {
      case 0:
         Mmsg1(jcr->errmsg, _("Device %s is busy writing on another Volume.\n"), dev->print_name());
	 dev->unblock();      
	 if (!wait_for_device(dcr, jcr->errmsg, first))  {
	    return false;
	 }
	 dev->block(BST_DOING_ACQUIRE);
	 continue;
      case -1:
	 goto bail_out; 	      /* error */
      default:
	 break; 		      /* OK, reserve drive */
      }
      break;
   }


   dev->reserved_device++;
   dcr->reserved_device = true;
   ok = true;

bail_out:
   dev->unblock();
   return ok;
}

/*
 * Returns: 1 if drive can be reserved
 *	    0 if we should wait
 *	   -1 on error
 */
static int can_reserve_drive(DCR *dcr) 
{
   DEVICE *dev = dcr->dev;
   JCR *jcr = dcr->jcr;
   /*
    * First handle the case that the drive is not yet in append mode
    */
   if (!dev->can_append() && dev->num_writers == 0) {
      /* Now check if there are any reservations on the drive */
      if (dev->reserved_device) {	    
	 /* Yes, now check if we want the same Pool and pool type */
	 if (strcmp(dev->pool_name, dcr->pool_name) == 0 &&
	     strcmp(dev->pool_type, dcr->pool_type) == 0) {
	    /* OK, compatible device */
	 } else {
	    /* Drive not suitable for us */
	    return 0;		      /* wait */
	 }
      } else {
	 /* Device is available but not yet reserved, reserve it for us */
	 bstrncpy(dev->pool_name, dcr->pool_name, sizeof(dev->pool_name));
	 bstrncpy(dev->pool_type, dcr->pool_type, sizeof(dev->pool_type));
      }
      return 1; 		      /* reserve drive */
   }

   /*
    * Now check if the device is in append mode 
    */
   if (dev->can_append() || dev->num_writers > 0) {
      Dmsg0(190, "device already in append.\n");
      /* Yes, now check if we want the same Pool and pool type */
      if (strcmp(dev->pool_name, dcr->pool_name) == 0 &&
	  strcmp(dev->pool_type, dcr->pool_type) == 0) {
	 /* OK, compatible device */
      } else {
	 /* Drive not suitable for us */
         Jmsg(jcr, M_WARNING, 0, _("Device %s is busy writing on another Volume.\n"), dev->print_name());
	 return 0;		      /* wait */
      }
   } else {
      Pmsg0(000, "Logic error!!!! Should not get here.\n");
      Jmsg0(jcr, M_FATAL, 0, _("Logic error!!!! Should not get here.\n"));
      return -1;		      /* error, should not get here */
   }
   return 1;			      /* reserve drive */
}

/*
 * Acquire device for writing. We permit multiple writers.
 *  If this is the first one, we read the label.
 *
 *  Returns: NULL if failed for any reason
 *	     dcr if successful.
 *   Note, normally reserve_device_for_append() is called
 *   before this routine.
 */
DCR *acquire_device_for_append(DCR *dcr)
{
   bool release = false;
   bool recycle = false;
   bool do_mount = false;
   DEVICE *dev = dcr->dev;
   JCR *jcr = dcr->jcr;

   init_device_wait_timers(dcr);

   dev->block(BST_DOING_ACQUIRE);
   Dmsg1(190, "acquire_append device is %s\n", dev_is_tape(dev)?"tape":"disk");

   if (dcr->reserved_device) {
      dev->reserved_device--;
      dcr->reserved_device = false;
   }

   /*
    * With the reservation system, this should not happen
    */
   if (dev->can_read()) {
      Jmsg(jcr, M_FATAL, 0, _("Device %s is busy reading.\n"), dev->print_name());
      goto get_out;
   }

   if (dev->can_append()) {
      Dmsg0(190, "device already in append.\n");
      /*
       * Device already in append mode
       *
       * Check if we have the right Volume mounted
       *   OK if current volume info OK
       *   OK if next volume matches current volume
       *   otherwise mount desired volume obtained from
       *    dir_find_next_appendable_volume
       */
      bstrncpy(dcr->VolumeName, dev->VolHdr.VolName, sizeof(dcr->VolumeName));
      if (!dir_get_volume_info(dcr, GET_VOL_INFO_FOR_WRITE) &&
	  !(dir_find_next_appendable_volume(dcr) &&
	    strcmp(dev->VolHdr.VolName, dcr->VolumeName) == 0)) { /* wrong tape mounted */
         Dmsg0(190, "Wrong tape mounted.\n");
	 if (dev->num_writers != 0 || dev->reserved_device) {
            Jmsg(jcr, M_FATAL, 0, _("Device %s is busy writing on another Volume.\n"), dev->print_name());
	    goto get_out;
	 }
	 /* Wrong tape mounted, release it, then fall through to get correct one */
         Dmsg0(190, "Wrong tape mounted, release and try mount.\n");
	 release = true;
	 do_mount = true;
      } else {
	 /*
	  * At this point, the correct tape is already mounted, so
	  *   we do not need to do mount_next_write_volume(), unless
	  *   we need to recycle the tape.
	  */
          recycle = strcmp(dcr->VolCatInfo.VolCatStatus, "Recycle") == 0;
          Dmsg1(190, "Correct tape mounted. recycle=%d\n", recycle);
	  if (recycle && dev->num_writers != 0) {
             Jmsg(jcr, M_FATAL, 0, _("Cannot recycle volume \"%s\""
                  " because it is in use by another job.\n"));
	     goto get_out;
	  }
	  if (dev->num_writers == 0) {
	     memcpy(&dev->VolCatInfo, &dcr->VolCatInfo, sizeof(dev->VolCatInfo));
	  }
       }
   } else {
      /* Not already in append mode, so mount the device */
      Dmsg0(190, "Not in append mode, try mount.\n");
      ASSERT(dev->num_writers == 0);
      do_mount = true;
   }

   if (do_mount || recycle) {
      Dmsg0(190, "Do mount_next_write_vol\n");
      bool mounted = mount_next_write_volume(dcr, release);
      if (!mounted) {
	 if (!job_canceled(jcr)) {
            /* Reduce "noise" -- don't print if job canceled */
            Jmsg(jcr, M_FATAL, 0, _("Could not ready device %s for append.\n"),
	       dev->print_name());
	 }
	 goto get_out;
      }
   }

   dev->num_writers++;		      /* we are now a writer */
   if (jcr->NumVolumes == 0) {
      jcr->NumVolumes = 1;
   }
   goto ok_out;

/*
 * If we jump here, it is an error return because
 *  rtn_dev will still be NULL
 */
get_out:
   free_dcr(dcr);
   dcr = NULL;
ok_out:
   dev->unblock();
   return dcr;
}

/*
 * This job is done, so release the device. From a Unix standpoint,
 *  the device remains open.
 *
 */
bool release_device(DCR *dcr)
{
   bool ok = true;
   JCR *jcr = dcr->jcr;
   DEVICE *dev = dcr->dev;

   lock_device(dev);
   Dmsg1(100, "release_device device is %s\n", dev_is_tape(dev)?"tape":"disk");

   /* if device is reserved, job never started, so release the reserve here */
   if (dcr->reserved_device) {
      dev->reserved_device--;
      dcr->reserved_device = false;
   }

   if (dev->can_read()) {
      dev->clear_read();	      /* clear read bit */

      /******FIXME**** send read volume usage statistics to director */

   } else if (dev->num_writers > 0) {
      dev->num_writers--;
      Dmsg1(100, "There are %d writers in release_device\n", dev->num_writers);
      if (dev->is_labeled()) {
         Dmsg0(100, "dir_create_jobmedia_record. Release\n");
	 if (!dir_create_jobmedia_record(dcr)) {
            Jmsg(jcr, M_FATAL, 0, _("Could not create JobMedia record for Volume=\"%s\" Job=%s\n"),
	       dcr->VolCatInfo.VolCatName, jcr->Job);
	    ok = false;
	 }
	 /* If no more writers, write an EOF */
	 if (!dev->num_writers && dev_can_write(dev)) {
	    weof_dev(dev, 1);
	    write_ansi_ibm_labels(dcr, ANSI_EOF_LABEL, dev->VolHdr.VolName);
	 }
	 dev->VolCatInfo.VolCatFiles = dev->file;   /* set number of files */
	 dev->VolCatInfo.VolCatJobs++;		    /* increment number of jobs */
	 /* Note! do volume update before close, which zaps VolCatInfo */
         Dmsg0(100, "dir_update_vol_info. Release0\n");
	 dir_update_volume_info(dcr, false); /* send Volume info to Director */
         Dmsg0(100, "==== write ansi eof label \n");
      }

   } else {
      /*		
       * If we reach here, it is most likely because the
       *   has failed, since the device is not in read mode and
       *   there are no writers.
       */
   }

   /* If no writers, close if file or !CAP_ALWAYS_OPEN */
   if (dev->num_writers == 0 && (!dev->is_tape() || !dev_cap(dev, CAP_ALWAYSOPEN))) {
      offline_or_rewind_dev(dev);
      close_dev(dev);
   }

   /* Fire off Alert command and include any output */
   if (!job_canceled(jcr) && dcr->device->alert_command) {
      POOLMEM *alert;
      int status = 1;
      BPIPE *bpipe;
      char line[MAXSTRING];
      alert = get_pool_memory(PM_FNAME);
      alert = edit_device_codes(dcr, alert, "");
      bpipe = open_bpipe(alert, 0, "r");
      if (bpipe) {
	 while (fgets(line, sizeof(line), bpipe->rfd)) {
            Jmsg(jcr, M_ALERT, 0, _("Alert: %s"), line);
	 }
	 status = close_bpipe(bpipe);
      } else {
	 status = errno;
      }
      if (status != 0) {
	 berrno be;
         Jmsg(jcr, M_ALERT, 0, _("3997 Bad alert command: %s: ERR=%s.\n"),
	      alert, be.strerror(status));
      }

      Dmsg1(400, "alert status=%d\n", status);
      free_pool_memory(alert);
   }
   unlock_device(dev);
   free_dcr(dcr);
   jcr->dcr = NULL;
   pthread_cond_broadcast(&wait_device_release);
   return ok;
}
