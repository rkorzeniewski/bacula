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

extern char my_name[];
extern int debug_level;

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
   DEV_BLOCK *label_blk = NULL;
   char b1[30], b2[30];
   time_t wait_time;

   wait_time = time(NULL);
   status_dev(dev, &stat);
   if (stat & MT_EOD) {
      Dmsg0(100, "======= Got EOD ========\n");

      block_device(dev, BST_DOING_ACQUIRE);
      /* Unlock, but leave BLOCKED */
      unlock_device(dev);

      /* 
       * Walk through all attached jcrs creating a jobmedia_record()
       */
      Dmsg1(100, "Walk attached jcrs. Volume=%s\n", dev->VolCatInfo.VolCatName);
      for (JCR *mjcr=NULL; (mjcr=next_attached_jcr(dev, mjcr)); ) {
         Dmsg1(100, "create JobMedia for Job %s\n", mjcr->Job);
	 mjcr->EndBlock = dev->block_num;
	 mjcr->EndFile = dev->file;
	 if (!dir_create_jobmedia_record(mjcr)) {
            Jmsg(mjcr, M_ERROR, 0, _("Could not create JobMedia record for Volume=%s Job=%s\n"),
	       dev->VolCatInfo.VolCatName, mjcr->Job);
	    P(dev->mutex);
	    unblock_device(dev);
	    return 0;
	 }
      }

      strcpy(dev->VolCatInfo.VolCatStatus, "Full");
      Dmsg2(100, "Call update_vol_info Stat=%s Vol=%s\n", 
	 dev->VolCatInfo.VolCatStatus, dev->VolCatInfo.VolCatName);
      if (!dir_update_volume_info(jcr, &dev->VolCatInfo, 0)) {	  /* send Volume info to Director */
         Jmsg(jcr, M_ERROR, 0, _("Could not update Volume info Volume=%s Job=%s\n"),
	    dev->VolCatInfo.VolCatName, jcr->Job);
	 P(dev->mutex);
	 unblock_device(dev);
	 return 0;		      /* device locked */
      }
      Dmsg0(100, "Back from update_vol_info\n");

      strcpy(PrevVolName, dev->VolCatInfo.VolCatName);
      strcpy(dev->VolHdr.PrevVolName, PrevVolName);

      label_blk = new_block(dev);

      /* Inform User about end of media */
      Jmsg(jcr, M_INFO, 0, _("End of media on Volume %s Bytes=%s Blocks=%s.\n"), 
	   PrevVolName, edit_uint64_with_commas(dev->VolCatInfo.VolCatBytes, b1),
	   edit_uint64_with_commas(dev->VolCatInfo.VolCatBlocks, b2));

      if (!mount_next_write_volume(jcr, dev, label_blk, 1)) {
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
       *  used tape, mount_next_write_volume() will return an
       *  empty label_blk, and nothing will be written.
       */
      Dmsg0(190, "write label block to dev\n");
      if (!write_block_to_dev(dev, label_blk)) {
         Pmsg1(0, "write_block_to_device Volume label failed. ERR=%s",
	   strerror_dev(dev));
	 free_block(label_blk);
	 unblock_device(dev);
	 return 0;		      /* device locked */
      }

      /* Write overflow block to tape */
      Dmsg0(190, "Write overflow block to dev\n");
      if (!write_block_to_dev(dev, block)) {
         Pmsg1(0, "write_block_to_device overflow block failed. ERR=%s",
	   strerror_dev(dev));
	 free_block(label_blk);
	 unblock_device(dev);
	 return 0;		      /* device locked */
      }

      jcr->NumVolumes++;
      Dmsg0(190, "Wake up any waiting threads.\n");
      free_block(label_blk);
      for (JCR *mjcr=NULL; (mjcr=next_attached_jcr(dev, mjcr)); ) {
	 /* Set new start/end positions */
	 mjcr->StartBlock = dev->block_num;
	 mjcr->StartFile = dev->file;
	 mjcr->VolFirstFile = mjcr->JobFiles;
	 mjcr->run_time += time(NULL) - wait_time; /* correct run time */
      }
      unblock_device(dev);
      return 1; 			       /* device locked */
   }
   if (label_blk) {
      free_block(label_blk);
   }
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
   Dmsg0(120, "start open_output_device()\n");
   if (!dev) {
      return 0;
   }

   lock_device(dev);

   /* Defer opening files */
   if (!dev_is_tape(dev)) {
      Dmsg0(129, "Device is file, deferring open.\n");
      unlock_device(dev);
      return 1;
   }

   if (!(dev->state & ST_OPENED)) {
      Dmsg0(129, "Opening device.\n");
      if (open_dev(dev, NULL, READ_WRITE) < 0) {
         Emsg1(M_FATAL, 0, _("dev open failed: %s\n"), dev->errmsg);
	 unlock_device(dev);
	 return 0;
      }
   }
   Dmsg1(129, "open_dev %s OK\n", dev_name(dev));

   unlock_device(dev);
   return 1;
}

/* 
 * When dev_blocked is set, all threads EXCEPT thread with id no_wait_id
 * must wait. The no_wait_id thread is out obtaining a new volume
 * and preparing the label.
 */
void _lock_device(char *file, int line, DEVICE *dev)
{
   int stat;
   Dmsg3(100, "lock %d from %s:%d\n", dev->dev_blocked, file, line);
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

void _unlock_device(char *file, int line, DEVICE *dev) 
{
   Dmsg2(100, "unlock from %s:%d\n", file, line);
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
void _block_device(char *file, int line, DEVICE *dev, int state)
{
   Dmsg3(100, "block set %d from %s:%d\n", state, file, line);
   ASSERT(dev->dev_blocked == BST_NOT_BLOCKED);
   dev->dev_blocked = state;	      /* make other threads wait */
   dev->no_wait_id = pthread_self();  /* allow us to continue */
}



/*
 * Unblock the device, and wake up anyone who went to sleep.
 */
void _unblock_device(char *file, int line, DEVICE *dev)
{
   Dmsg3(100, "unblock %d from %s:%d\n", dev->dev_blocked, file, line);
   ASSERT(dev->dev_blocked);
   dev->dev_blocked = BST_NOT_BLOCKED;
   if (dev->num_waiting > 0) {
      pthread_cond_broadcast(&dev->wait); /* wake them up */
   }
}

void _steal_device_lock(char *file, int line, DEVICE *dev, bsteal_lock_t *hold, int state)
{
   Dmsg4(100, "steal lock. old=%d new=%d from %s:%d\n", dev->dev_blocked, state,
      file, line);
   hold->dev_blocked = dev->dev_blocked;
   hold->no_wait_id = dev->no_wait_id;
   dev->dev_blocked = state;
   dev->no_wait_id = pthread_self();
   V(dev->mutex);
}

void _return_device_lock(char *file, int line, DEVICE *dev, bsteal_lock_t *hold)	   
{
   Dmsg4(100, "return lock. old=%d new=%d from %s:%d\n", 
      dev->dev_blocked, hold->dev_blocked, file, line);
   P(dev->mutex);
   dev->dev_blocked = hold->dev_blocked;
   dev->no_wait_id = hold->no_wait_id;
}



/* ==================================================================
 *  New device locking code.  It is not currently used.
 * ==================================================================
 */

/*
 * New device locking scheme 
 */
void _new_lock_device(char *file, int line, DEVICE *dev)
{
#ifdef NEW_LOCK
   int errstat;
   if ((errstat=rwl_writelock(&dev->lock)) != 0) {
      e_msg(file, line, M_ABORT, 0, "rwl_writelock failure. ERR=%s\n",
	   strerror(errstat));
   }
#endif
}    

void _new_lock_device(char *file, int line, DEVICE *dev, int state)
{
#ifdef NEW_LOCK
   int errstat;
   if ((errstat=rwl_writelock(&dev->lock)) != 0) {
      e_msg(file, line, M_ABORT, 0, "rwl_writelock failure. ERR=%s\n",
	   strerror(errstat));
   }
   dev->dev_blocked = state;
#endif
}    

void _new_unlock_device(char *file, int line, DEVICE *dev)
{
#ifdef NEW_LOCK
   int errstat;
   if (dev->lock.w_active == 1) {
      dev->dev_blocked = BST_NOT_BLOCKED;
   }
   if ((errstat=rwl_writeunlock(&dev->lock)) != 0) {
      e_msg(file, line, M_ABORT, 0, "rwl_writeunlock failure. ERR=%s\n",
	   strerror(errstat));
   }
#endif
}    

void new_steal_device_lock(DEVICE *dev, brwsteal_t *hold, int state)
{
#ifdef NEW_LOCK
   hold->state = dev->dev_blocked;
   hold->writer_id = dev->lock.writer_id;
   dev->dev_blocked = state;
   dev->lock.writer_id = pthread_self();
   V(dev->lock.mutex);
#endif
}

void new_return_device_lock(DEVICE *dev, brwsteal_t *hold)	     
{
#ifdef NEW_LOCK
   P(dev->lock.mutex);
   dev->dev_blocked = hold->state;
   dev->lock.writer_id = hold->writer_id;
#endif
}
