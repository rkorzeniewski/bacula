/*
 *
 *   dev.c  -- low level operations on device (storage device)
 *
 *		Kern Sibbald
 *
 *     NOTE!!!! None of these routines are reentrant. You must
 *	  use lock_device() and unlock_device() at a higher level,
 *	  or use the xxx_device() equivalents.	By moving the
 *	  thread synchronization to a higher level, we permit
 *        the higher level routines to "seize" the device and 
 *	  to carry out operations without worrying about who
 *	  set what lock (i.e. race conditions).
 *
 *     Note, this is the device dependent code, and my have
 *	     to be modified for each system, but is meant to
 *           be as "generic" as possible.
 * 
 *     The purpose of this code is to develop a SIMPLE Storage
 *     daemon. More complicated coding (double buffering, writer
 *     thread, ...) is left for a later version.
 *
 *     Unfortunately, I have had to add more and more complication
 *     to this code. This was not foreseen as noted above, and as
 *     a consequence has lead to something more contorted than is
 *     really necessary -- KES.  Note, this contortion has been
 *     corrected to a large extent by a rewrite (Apr MMI).
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

/*
 * Handling I/O errors and end of tape conditions are a bit tricky.
 * This is how it is currently done when writting.
 * On either an I/O error or end of tape,
 * we will stop writing on the physical device (no I/O recovery is
 * attempted at least in this daemon). The state flag will be sent
 * to include ST_EOT, which is ephimeral, and ST_WEOT, which is
 * persistent. Lots of routines clear ST_EOT, but ST_WEOT is
 * cleared only when the problem goes away.  Now when ST_WEOT
 * is set all calls to write_dev() are handled as usual. However,
 * in write_block() instead of attempting to write the block to
 * the physical device, it is chained into a list of blocks written
 * after the EOT condition.  In addition, all threads are blocked
 * from writing on the tape by calling lock(), and thread other
 * than the first thread to hit the EOT will block on a condition
 * variable. The first thread to hit the EOT will continue to
 * be able to read and write the tape (he sort of tunnels through
 * the locking mechanism -- see lock() for details).
 *
 * Now presumably somewhere higher in the chain of command 
 * (device.c), someone will notice the EOT condition and 
 * get a new tape up, get the tape label read, and mark 
 * the label for rewriting. Then this higher level routine 
 * will write the unwritten buffer to the new volume.
 * Finally, he will release
 * any blocked threads by doing a broadcast on the condition
 * variable.  At that point, we should be totally back in 
 * business with no lost data.
 */


#include "bacula.h"
#include "stored.h"

/* Forward referenced functions */
int dev_is_tape(DEVICE *dev);
void clrerror_dev(DEVICE *dev, int func);
int fsr_dev(DEVICE *dev, int num);

extern int debug_level;

/* 
 * Allocate and initialize the DEVICE structure
 * Note, if dev is non-NULL, it is already allocated,
 * thus we neither allocate it nor free it. This allows
 * the caller to put the packet in shared memory.
 *
 *  Note, for a tape, the device->device_name is the device name
 *     (e.g. /dev/nst0), and for a file, the device name
 *     is the directory in which the file will be placed.
 *
 */
DEVICE *
init_dev(DEVICE *dev, DEVRES *device)
{
   struct stat statp;
   int tape;
   int errstat;

   /* Check that device is available */
   if (stat(device->device_name, &statp) < 0) {
      if (dev) {
	 dev->dev_errno = errno;
      } 
      Emsg2(M_FATAL, 0, "Unable to stat device %s : %s\n", device->device_name, 
	    strerror(errno));
      return NULL;
   }
   tape = FALSE;
   if (S_ISDIR(statp.st_mode)) {
      tape = FALSE;
   } else if (S_ISCHR(statp.st_mode)) {
      tape = TRUE;
   } else {
      if (dev) {
	 dev->dev_errno = ENODEV;
      }
      Emsg2(M_FATAL, 0, "%s is an unknown device type. Must be tape or directory. st_mode=%x\n", 
	 dev_name, statp.st_mode);
      return NULL;
   }
   if (!dev) {
      dev = (DEVICE *)get_memory(sizeof(DEVICE));
      memset(dev, 0, sizeof(DEVICE));
      dev->state = ST_MALLOC;
   } else {
      memset(dev, 0, sizeof(DEVICE));
   }
   if (tape) {
      dev->state |= ST_TAPE;
   }

   /* Copy user supplied device parameters from Resource */
   dev->dev_name = get_memory(strlen(device->device_name)+1);
   strcpy(dev->dev_name, device->device_name);
   dev->capabilities = device->cap_bits;
   dev->min_block_size = device->min_block_size;
   dev->max_block_size = device->max_block_size;
   dev->max_volume_jobs = device->max_volume_jobs;
   dev->max_volume_files = device->max_volume_files;
   dev->max_volume_size = device->max_volume_size;
   dev->max_file_size = device->max_file_size;
   dev->volume_capacity = device->volume_capacity;
   dev->max_rewind_wait = device->max_rewind_wait;
   dev->max_open_wait = device->max_open_wait;
   dev->device = device;


   dev->errmsg = get_pool_memory(PM_EMSG);
   *dev->errmsg = 0;

   if ((errstat = pthread_mutex_init(&dev->mutex, NULL)) != 0) {
      dev->dev_errno = errstat;
      Mmsg1(&dev->errmsg, _("Unable to init mutex: ERR=%s\n"), strerror(errstat));
      Emsg0(M_FATAL, 0, dev->errmsg);
   }
   if ((errstat = pthread_cond_init(&dev->wait, NULL)) != 0) {
      dev->dev_errno = errstat;
      Mmsg1(&dev->errmsg, _("Unable to init cond variable: ERR=%s\n"), strerror(errstat));
      Emsg0(M_FATAL, 0, dev->errmsg);
   }
   if ((errstat = pthread_cond_init(&dev->wait_next_vol, NULL)) != 0) {
      dev->dev_errno = errstat;
      Mmsg1(&dev->errmsg, _("Unable to init cond variable: ERR=%s\n"), strerror(errstat));
      Emsg0(M_FATAL, 0, dev->errmsg);
   }
   dev->fd = -1;
   Dmsg2(29, "init_dev: tape=%d dev_name=%s\n", dev_is_tape(dev), dev->dev_name);
   return dev;
}

/* Open the device with the operating system and
 * initialize buffer pointers.
 *
 * Note, for a tape, the VolName is the name we give to the
 *    volume (not really used here), but for a file, the
 *    VolName represents the name of the file to be created/opened.
 *    In the case of a file, the full name is the device name
 *    (archive_name) with the VolName concatenated.
 */
int
open_dev(DEVICE *dev, char *VolName, int mode)
{
   POOLMEM *archive_name;

   if (dev->state & ST_OPENED) {
      /*
       *  *****FIXME***** how to handle two threads wanting
       *  different volumes mounted???? E.g. one is waiting
       *  for the next volume to be mounted, and a new job
       *  starts and snatches up the device.
       */
      if (VolName && strcmp(dev->VolCatInfo.VolCatName, VolName) != 0) {
	 return -1;
      }
      dev->use_count++;
      Mmsg2(&dev->errmsg, _("WARNING!!!! device %s opened %d times!!!\n"), 
	    dev->dev_name, dev->use_count);
      Emsg0(M_WARNING, 0, dev->errmsg);
      return dev->fd;
   }
   if (VolName) {
      strcpy(dev->VolCatInfo.VolCatName, VolName);
   }

   Dmsg3(29, "open_dev: tape=%d dev_name=%s vol=%s\n", dev_is_tape(dev), 
	 dev->dev_name, dev->VolCatInfo.VolCatName);
   dev->state &= ~(ST_LABEL|ST_APPEND|ST_READ|ST_EOT|ST_WEOT|ST_EOF);
   if (dev->state & ST_TAPE) {
      int timeout;
      Dmsg0(29, "open_dev: device is tape\n");
      if (mode == READ_WRITE) {
	 dev->mode = O_RDWR | O_BINARY;
      } else {
	 dev->mode = O_RDONLY | O_BINARY;
      }
      timeout = dev->max_open_wait;
      errno = 0;
      while ((dev->fd = open(dev->dev_name, dev->mode, MODE_RW)) < 0) {
	 if (errno == EBUSY && timeout-- > 0) {
            Dmsg2(100, "Device %s busy. ERR=%s\n", dev->dev_name, strerror(errno));
	    sleep(1);
	    continue;
	 }
	 dev->dev_errno = errno;
         Mmsg2(&dev->errmsg, _("stored: unable to open device %s: ERR=%s\n"), 
	       dev->dev_name, strerror(dev->dev_errno));
	 Emsg0(M_FATAL, 0, dev->errmsg);
	 break;
      }
      if (dev->fd >= 0) {
	 dev->dev_errno = 0;
	 dev->state |= ST_OPENED;
	 dev->use_count++;
	 update_pos_dev(dev);		  /* update position */
      }
      Dmsg1(29, "open_dev: tape %d opened\n", dev->fd);
   } else {
      /*
       * Handle opening of file
       */
      archive_name = get_pool_memory(PM_FNAME);
      strcpy(archive_name, dev->dev_name);
      if (archive_name[strlen(archive_name)] != '/') {
         strcat(archive_name, "/");
      }
      strcat(archive_name, VolName);
      Dmsg1(29, "open_dev: device is disk %s\n", archive_name);
      if (mode == READ_WRITE) {
	 dev->mode = O_CREAT | O_RDWR | O_BINARY;
      } else {
	 dev->mode = O_RDONLY | O_BINARY;
      }
      if ((dev->fd = open(archive_name, dev->mode, MODE_RW)) < 0) {
	 dev->dev_errno = errno;
         Mmsg2(&dev->errmsg, _("Could not open: %s, ERR=%s\n"), archive_name, strerror(dev->dev_errno));
	 Emsg0(M_FATAL, 0, dev->errmsg);
      } else {
	 dev->dev_errno = 0;
	 dev->state |= ST_OPENED;
	 dev->use_count++;
	 update_pos_dev(dev);		     /* update position */
      }
      Dmsg1(29, "open_dev: disk fd=%d opened\n", dev->fd);
      free_pool_memory(archive_name);
   }
   return dev->fd;
}

/*
 * Rewind the device.
 *  Returns: 1 on success
 *	     0 on failure
 */
int rewind_dev(DEVICE *dev)
{
   struct mtop mt_com;
   unsigned int i;

   Dmsg0(29, "rewind_dev\n");
   if (dev->fd < 0) {
      dev->dev_errno = EBADF;
      Mmsg1(&dev->errmsg, _("Bad call to rewind_dev. Device %s not open\n"),
	    dev->dev_name);
      Emsg0(M_FATAL, 0, dev->errmsg);
      return 0;
   }
   dev->state &= ~(ST_APPEND|ST_READ|ST_EOT | ST_EOF | ST_WEOT);  /* remove EOF/EOT flags */
   dev->block_num = dev->file = 0;
   dev->file_addr = 0;
   if (dev->state & ST_TAPE) {
      mt_com.mt_op = MTREW;
      mt_com.mt_count = 1;
      /* If we get an I/O error on rewind, it is probably because
       * the drive is actually busy. We loop for (about 5 minutes)
       * retrying every 5 seconds.
       */
      for (i=dev->max_rewind_wait; ; i -= 5) {
	 if (ioctl(dev->fd, MTIOCTOP, (char *)&mt_com) < 0) {
	    if (i == dev->max_rewind_wait) {
               Dmsg1(200, "Rewind error, %s. retrying ...\n", strerror(errno));
	    }
	    clrerror_dev(dev, MTREW);
	    if (dev->dev_errno == EIO && i > 0) {
               Dmsg0(200, "Sleeping 5 seconds.\n");
	       sleep(5);
	       continue;
	    }
            Mmsg2(&dev->errmsg, _("Rewind error on %s. ERR=%s.\n"),
	       dev->dev_name, strerror(dev->dev_errno));
	    return 0;
	 }
	 break;
      }
   } else {
      if (lseek(dev->fd, (off_t)0, SEEK_SET) < 0) {
	 dev->dev_errno = errno;
         Mmsg2(&dev->errmsg, _("lseek error on %s. ERR=%s.\n"),
	    dev->dev_name, strerror(dev->dev_errno));
	 return 0;
      }
   }
   return 1;
}

/* 
 * Position device to end of medium (end of data)
 *  Returns: 1 on succes
 *	     0 on error
 */
int 
eod_dev(DEVICE *dev)
{
   struct mtop mt_com;
   struct mtget mt_stat;
   int stat = 0;
   off_t pos;

   Dmsg0(29, "eod_dev\n");
   if (dev->state & ST_EOT) {
      return 1;
   }
   dev->state &= ~(ST_EOF);  /* remove EOF flags */
   dev->block_num = dev->file = 0;
   dev->file_addr = 0;
   if (!(dev->state & ST_TAPE)) {
      pos = lseek(dev->fd, (off_t)0, SEEK_END);
//    Dmsg1(000, "====== Seek to %lld\n", pos);
      if (pos >= 0) {
	 update_pos_dev(dev);
	 dev->state |= ST_EOT;
	 return 1;
      }
      return 0;
   }
   if (dev->capabilities & CAP_EOM) {
      mt_com.mt_op = MTEOM;
      mt_com.mt_count = 1;
      if ((stat=ioctl(dev->fd, MTIOCTOP, (char *)&mt_com)) < 0) {
         Dmsg1(50, "ioctl error: %s\n", strerror(dev->dev_errno));
	 clrerror_dev(dev, mt_com.mt_op);
	 update_pos_dev(dev);
         Mmsg2(&dev->errmsg, _("ioctl MTEOM error on %s. ERR=%s.\n"),
	    dev->dev_name, strerror(dev->dev_errno));
	 return 0;
      }
      if (ioctl(dev->fd, MTIOCGET, (char *)&mt_stat) < 0) {
	 dev->dev_errno = errno;
         Mmsg2(&dev->errmsg, _("ioctl MTIOCGET error on %s. ERR=%s.\n"),
	    dev->dev_name, strerror(dev->dev_errno));
	 return 0;
      }
      Dmsg2(200, "EOD file=%d block=%d\n", mt_stat.mt_fileno, mt_stat.mt_blkno);
      dev->file = mt_stat.mt_fileno;

   /*
    * Rewind then use FSF until EOT reached
    */
   } else {
      if (!rewind_dev(dev)) {
	 return 0;
      }
      while (!(dev->state & ST_EOT)) {
         Dmsg0(200, "Do fsf 1\n");
	 if (fsf_dev(dev, 1) < 0) {
            Dmsg0(200, "fsf_dev return < 0\n");
	    return 0;
	 }
      }
   }
   update_pos_dev(dev); 		     /* update position */
   Dmsg1(200, "EOD dev->file=%d\n", dev->file);
   return 1;
}

/*
 * Set the position of the device.
 *  Returns: 1 on succes
 *	     0 on error
 */
int update_pos_dev(DEVICE *dev)
{
#ifdef xxxx
   struct mtget mt_stat;
#endif
   off_t pos;
   int stat = 0;

   if (dev->fd < 0) {
      dev->dev_errno = EBADF;
      Mmsg0(&dev->errmsg, _("Bad device call. Archive not open\n"));
      Emsg0(M_FATAL, 0, dev->errmsg);
      return 0;
   }

   /* Find out where we are */
   if (!(dev->state & ST_TAPE)) {
      dev->file = 0;
      dev->file_addr = 0;
      pos = lseek(dev->fd, (off_t)0, SEEK_CUR);
      if (pos < 0) {
         Dmsg1(000, "Seek error: ERR=%s\n", strerror(dev->dev_errno));
	 dev->dev_errno = errno;
         Mmsg2(&dev->errmsg, _("lseek error on %s. ERR=%s.\n"),
	    dev->dev_name, strerror(dev->dev_errno));
      } else {
	 stat = 1;
	 dev->file_addr = pos;
      }
      return stat;
   }

#ifdef REALLY_IMPLEMENTED
   if (ioctl(dev->fd, MTIOCGET, (char *)&mt_stat) < 0) {
      Dmsg1(50, "MTIOCGET error: %s\n", strerror(dev->dev_errno));
      dev->dev_errno = errno;
      Mmsg2(&dev->errmsg, _("ioctl MTIOCGET error on %s. ERR=%s.\n"),
	 dev->dev_name, strerror(dev->dev_errno));
   } else {
      stat = 1;
   }
   return stat;
#endif
   return 1;
}


/* 
 * Return the status of the device.  This was meant
 * to be a generic routine. Unfortunately, it doesn't
 * seem possible (at least I do not know how to do it
 * currently), which means that for the moment, this
 * routine has very little value.
 *
 *   Returns: 1 on success
 *	      0 on error
 */
int
status_dev(DEVICE *dev, uint32_t *status)
{
   struct mtget mt_stat;
   uint32_t stat = 0;

   if (dev->state & (ST_EOT | ST_WEOT)) {
      stat |= MT_EOD;
      Dmsg0(-20, " EOD");
   }
   if (dev->state & ST_EOF) {
      stat |= MT_EOF;
      Dmsg0(-20, " EOF");
   }
   if (dev->state & ST_TAPE) {
      stat |= MT_TAPE;
      Dmsg0(-20," Driver status:");
      Dmsg2(-20," file=%d block=%d\n", dev->file, dev->block_num);
      if (ioctl(dev->fd, MTIOCGET, (char *)&mt_stat) < 0) {
	 dev->dev_errno = errno;
         Mmsg2(&dev->errmsg, _("ioctl MTIOCGET error on %s. ERR=%s.\n"),
	    dev->dev_name, strerror(dev->dev_errno));
	 return 0;
      }
      Dmsg0(-20, " Device status:");

#if defined(HAVE_LINUX_OS)
      if (GMT_EOF(mt_stat.mt_gstat)) {
	 stat |= MT_EOF;
         Dmsg0(-20, " EOF");
      }
      if (GMT_BOT(mt_stat.mt_gstat)) {
	 stat |= MT_BOT;
         Dmsg0(-20, " BOT");
      }
      if (GMT_EOT(mt_stat.mt_gstat)) {
	 stat |= MT_EOT;
         Dmsg0(-20, " EOT");
      }
      if (GMT_SM(mt_stat.mt_gstat)) {
	 stat |= MT_SM;
         Dmsg0(-20, " SM");
      }
      if (GMT_EOD(mt_stat.mt_gstat)) {
	 stat |= MT_EOD;
         Dmsg0(-20, " EOD");
      }
      if (GMT_WR_PROT(mt_stat.mt_gstat)) {
	 stat |= MT_WR_PROT;
         Dmsg0(-20, " WR_PROT");
      }
      if (GMT_ONLINE(mt_stat.mt_gstat)) {
	 stat |= MT_ONLINE;
         Dmsg0(-20, " ONLINE");
      }
      if (GMT_DR_OPEN(mt_stat.mt_gstat)) {
	 stat |= MT_DR_OPEN;
         Dmsg0(-20, " DR_OPEN");       
      }
      if (GMT_IM_REP_EN(mt_stat.mt_gstat)) {
	 stat |= MT_IM_REP_EN;
         Dmsg0(-20, " IM_REP_EN");
      }
#endif /* !SunOS && !OSF */
      Dmsg2(-20, " file=%d block=%d\n", mt_stat.mt_fileno, mt_stat.mt_blkno);
   } else {
      stat |= MT_ONLINE | MT_BOT;
   }
   *status = stat; 
   return 1;
}


/*
 * Load medium in device
 *  Returns: 1 on success
 *	     0 on failure
 */
int load_dev(DEVICE *dev)
{
#ifdef MTLOAD
   struct mtop mt_com;
#endif

   if (dev->fd < 0) {
      dev->dev_errno = EBADF;
      Mmsg0(&dev->errmsg, _("Bad call to load_dev. Archive not open\n"));
      Emsg0(M_FATAL, 0, dev->errmsg);
      return 0;
   }
   if (!(dev->state & ST_TAPE)) {
      return 1;
   }
#ifndef MTLOAD
   Dmsg0(200, "stored: MTLOAD command not available\n");
   dev->dev_errno = ENOTTY;	      /* function not available */
   Mmsg2(&dev->errmsg, _("ioctl MTLOAD error on %s. ERR=%s.\n"),
	 dev->dev_name, strerror(dev->dev_errno));	return 0;
   return 0;
#else

   dev->block_num = dev->file = 0;
   dev->file_addr = 0;
   mt_com.mt_op = MTLOAD;
   mt_com.mt_count = 1;
   if (ioctl(dev->fd, MTIOCTOP, (char *)&mt_com) < 0) {
      dev->dev_errno = errno;
      Mmsg2(&dev->errmsg, _("ioctl MTLOAD error on %s. ERR=%s.\n"),
	 dev->dev_name, strerror(dev->dev_errno));	return 0;
   }
   return 1;
#endif
}

/*
 * Rewind device and put it offline
 *  Returns: 1 on success
 *	     0 on failure
 */
int offline_dev(DEVICE *dev)
{
   struct mtop mt_com;

   if (dev->fd < 0) {
      dev->dev_errno = EBADF;
      Mmsg0(&dev->errmsg, _("Bad call to load_dev. Archive not open\n"));
      Emsg0(M_FATAL, 0, dev->errmsg);
      return 0;
   }
   if (!(dev->state & ST_TAPE)) {
      return 1;
   }

   dev->block_num = dev->file = 0;
   dev->file_addr = 0;
#ifdef MTUNLOCK
   mt_com.mt_op = MTUNLOCK;
   mt_com.mt_count = 1;
   ioctl(dev->fd, MTIOCTOP, (char *)&mt_com);
#endif
   mt_com.mt_op = MTOFFL;
   mt_com.mt_count = 1;
   if (ioctl(dev->fd, MTIOCTOP, (char *)&mt_com) < 0) {
      dev->dev_errno = errno;
      Mmsg2(&dev->errmsg, _("ioctl MTOFFL error on %s. ERR=%s.\n"),
	 dev->dev_name, strerror(dev->dev_errno));
      return 0;
   }
   Dmsg1(100, "Offlined device %s\n", dev->dev_name);
   return 1;
}


/* 
 * Foward space a file	
 */
int
fsf_dev(DEVICE *dev, int num)
{ 
   struct mtop mt_com;
   int stat = 0;
   char rbuf[1024];

   if (dev->fd < 0) {
      dev->dev_errno = EBADF;
      Mmsg0(&dev->errmsg, _("Bad call to fsf_dev. Archive not open\n"));
      Emsg0(M_FATAL, 0, dev->errmsg);
      return -1;
   }

   if (!(dev->state & ST_TAPE)) {
      return 0;
   }
   if (dev->state & ST_EOT) {
      dev->dev_errno = 0;
      Mmsg1(&dev->errmsg, _("Device %s at End of Tape.\n"), dev->dev_name);
      return -1;
   }
   if (dev->state & ST_EOF)
      Dmsg0(200, "ST_EOF set on entry to FSF\n");
   if (dev->state & ST_EOT)
      Dmsg0(200, "ST_EOT set on entry to FSF\n");
      
   Dmsg0(29, "fsf_dev\n");
   dev->block_num = 0;
   if (dev->capabilities & CAP_FSF) {
      Dmsg0(200, "FSF has cap_fsf\n");
      mt_com.mt_op = MTFSF;
      mt_com.mt_count = 1;
      while (num-- && !(dev->state & ST_EOT)) {
         Dmsg0(200, "Doing read for fsf\n");
	 if ((stat = read(dev->fd, rbuf, sizeof(rbuf))) < 0) {
	    if (errno == ENOMEM) {     /* tape record exceeds buf len */
	       stat = sizeof(rbuf);   /* This is OK */
	    } else {
	       dev->state |= ST_EOT;
	       clrerror_dev(dev, -1);
               Dmsg1(200, "Set ST_EOT read error %d\n", dev->dev_errno);
               Mmsg2(&dev->errmsg, _("read error on %s. ERR=%s.\n"),
		  dev->dev_name, strerror(dev->dev_errno));
               Dmsg1(200, "%s", dev->errmsg);
	       break;
	    }
	 }
	 if (stat == 0) {		 /* EOF */
	    update_pos_dev(dev);
            Dmsg1(200, "End of File mark from read. File=%d\n", dev->file+1);
	    /* Two reads of zero means end of tape */
	    if (dev->state & ST_EOF) {
	       dev->state |= ST_EOT;
               Dmsg0(200, "Set ST_EOT\n");
	       break;
	    } else {
	       dev->state |= ST_EOF;
	       dev->file++;
	       dev->file_addr = 0;
	       continue;
	    }
	 } else {			 /* Got data */
	    dev->state &= ~(ST_EOF|ST_EOT);
	 }

         Dmsg0(200, "Doing MT_FSF\n");
	 stat = ioctl(dev->fd, MTIOCTOP, (char *)&mt_com);
	 if (stat < 0) {		 /* error => EOT */
	    dev->state |= ST_EOT;
            Dmsg0(200, "Set ST_EOT\n");
	    clrerror_dev(dev, MTFSF);
            Mmsg2(&dev->errmsg, _("ioctl MTFSF error on %s. ERR=%s.\n"),
	       dev->dev_name, strerror(dev->dev_errno));
            Dmsg0(200, "Got < 0 for MT_FSF\n");
            Dmsg1(200, "%s", dev->errmsg);
	 } else {
	    dev->state |= ST_EOF;     /* just read EOF */
	    dev->file++;
	    dev->file_addr = 0;
	 }   
      }
   
   /*
    * No FSF, so use FSR to simulate it
    */
   } else {
      Dmsg0(200, "Doing FSR for FSF\n");
      while (num-- && !(dev->state & ST_EOT)) {
	 fsr_dev(dev, INT32_MAX);    /* returns -1 on EOF or EOT */
      }
      if (dev->state & ST_EOT) {
	 dev->dev_errno = 0;
         Mmsg1(&dev->errmsg, _("Device %s at End of Tape.\n"), dev->dev_name);
	 stat = -1;
      } else {
	 stat = 0;
      }
   }
   update_pos_dev(dev);
   Dmsg1(200, "Return %d from FSF\n", stat);
   if (dev->state & ST_EOF)
      Dmsg0(200, "ST_EOF set on exit FSF\n");
   if (dev->state & ST_EOT)
      Dmsg0(200, "ST_EOT set on exit FSF\n");
   Dmsg1(200, "Return from FSF file=%d\n", dev->file);
   return stat;
}

/* 
 * Backward space a file  
 */
int
bsf_dev(DEVICE *dev, int num)
{ 
   struct mtop mt_com;
   int stat;

   if (dev->fd < 0) {
      dev->dev_errno = EBADF;
      Mmsg0(&dev->errmsg, _("Bad call to fsf_dev. Archive not open\n"));
      Emsg0(M_FATAL, 0, dev->errmsg);
      return -1;
   }

   if (!(dev->state & ST_TAPE)) {
      return 0;
   }
   Dmsg0(29, "bsf_dev\n");
   dev->state &= ~(ST_EOT|ST_EOF);
   dev->file -= num;
   dev->file_addr = 0;
   mt_com.mt_op = MTBSF;
   mt_com.mt_count = num;
   stat = ioctl(dev->fd, MTIOCTOP, (char *)&mt_com);
   if (stat < 0) {
      clrerror_dev(dev, MTBSF);
      Mmsg2(&dev->errmsg, _("ioctl MTBSF error on %s. ERR=%s.\n"),
	 dev->dev_name, strerror(dev->dev_errno));
   }
   update_pos_dev(dev);
   return stat;
}


/* 
 * Foward space a record
 */
int
fsr_dev(DEVICE *dev, int num)
{ 
   struct mtop mt_com;
   int stat;

   if (dev->fd < 0) {
      dev->dev_errno = EBADF;
      Mmsg0(&dev->errmsg, _("Bad call to fsf_dev. Archive not open\n"));
      Emsg0(M_FATAL, 0, dev->errmsg);
      return -1;
   }

   if (!(dev->state & ST_TAPE)) {
      return 0;
   }
   Dmsg0(29, "fsr_dev\n");
   dev->block_num += num;
   mt_com.mt_op = MTFSR;
   mt_com.mt_count = num;
   stat = ioctl(dev->fd, MTIOCTOP, (char *)&mt_com);
   if (stat == 0) {
      dev->state &= ~ST_EOF;
   } else {
      if (dev->state & ST_EOF) {
	 dev->state |= ST_EOT;
      } else {
	 dev->state |= ST_EOF;		 /* assume EOF */
	 dev->file++;
	 dev->file_addr = 0;
      }
      clrerror_dev(dev, MTFSR);
      Mmsg2(&dev->errmsg, _("ioctl MTFSR error on %s. ERR=%s.\n"),
	 dev->dev_name, strerror(dev->dev_errno));
   }
   update_pos_dev(dev);
   return stat;
}

/* 
 * Backward space a record
 */
int
bsr_dev(DEVICE *dev, int num)
{ 
   struct mtop mt_com;
   int stat;

   if (dev->fd < 0) {
      dev->dev_errno = EBADF;
      Mmsg0(&dev->errmsg, _("Bad call to fsf_dev. Archive not open\n"));
      Emsg0(M_FATAL, 0, dev->errmsg);
      return -1;
   }

   if (!(dev->state & ST_TAPE)) {
      return 0;
   }
   Dmsg0(29, "bsr_dev\n");
   dev->block_num -= num;
   dev->state &= ~(ST_EOF|ST_EOT|ST_EOF);
   mt_com.mt_op = MTBSR;
   mt_com.mt_count = num;
   stat = ioctl(dev->fd, MTIOCTOP, (char *)&mt_com);
   if (stat < 0) {
      clrerror_dev(dev, MTBSR);
      Mmsg2(&dev->errmsg, _("ioctl MTBSR error on %s. ERR=%s.\n"),
	 dev->dev_name, strerror(dev->dev_errno));
   }
   update_pos_dev(dev);
   return stat;
}



/*
 * Write an end of file on the device
 */
int 
weof_dev(DEVICE *dev, int num)
{ 
   struct mtop mt_com;
   int stat;

   if (dev->fd < 0) {
      dev->dev_errno = EBADF;
      Mmsg0(&dev->errmsg, _("Bad call to fsf_dev. Archive not open\n"));
      Emsg0(M_FATAL, 0, dev->errmsg);
      return -1;
   }

   if (!(dev->state & ST_TAPE)) {
      return 0;
   }
   dev->state &= ~(ST_EOT | ST_EOF);  /* remove EOF/EOT flags */
   dev->block_num = 0;
   Dmsg0(29, "weof_dev\n");
   mt_com.mt_op = MTWEOF;
   mt_com.mt_count = num;
   stat = ioctl(dev->fd, MTIOCTOP, (char *)&mt_com);
   if (stat == 0) {
      dev->file++;
      dev->file_addr = 0;
   } else {
      clrerror_dev(dev, MTWEOF);
      Mmsg2(&dev->errmsg, _("ioctl MTWEOF error on %s. ERR=%s.\n"),
	 dev->dev_name, strerror(dev->dev_errno));
   }
   return stat;
}

/*
 * Return string message with last error in English
 *  Be careful not to call this routine from within dev.c
 *  while editing an Mmsg(&) or you will end up in a recursive
 *  loop creating a Segmentation Violation.
 */
char *
strerror_dev(DEVICE *dev)
{
   return dev->errmsg;
}


/*
 * If implemented in system, clear the tape
 * error status.
 */
void
clrerror_dev(DEVICE *dev, int func)
{
   char *msg = NULL;

   dev->dev_errno = errno;	   /* save errno */
   if (errno == EIO) {
      dev->VolCatInfo.VolCatErrors++;
   }

   if (!(dev->state & ST_TAPE)) {
      return;
   }
   if (errno == ENOTTY || errno == ENOSYS) { /* Function not implemented */
      switch (func) {
	 case -1:
            Emsg0(M_ABORT, 0, "Got ENOTTY on read/write!\n");
	    break;
	 case MTWEOF:
            msg = "WTWEOF";
	    dev->capabilities &= ~CAP_EOF; /* turn off feature */
	    break;
	 case MTEOM:
            msg = "WTEOM";
	    dev->capabilities &= ~CAP_EOM; /* turn off feature */
	    break;
	 case MTFSF:
            msg = "MTFSF";
	    dev->capabilities &= ~CAP_FSF; /* turn off feature */
	    break;
	 case MTBSF:
            msg = "MTBSF";
	    dev->capabilities &= ~CAP_BSF; /* turn off feature */
	    break;
	 case MTFSR:
            msg = "MTFSR";
	    dev->capabilities &= ~CAP_FSR; /* turn off feature */
	    break;
	 case MTBSR:
            msg = "MTBSR";
	    dev->capabilities &= ~CAP_BSR; /* turn off feature */
	    break;
	 default:
            msg = "Unknown";
	    break;
      }
      if (msg != NULL) {
	 dev->dev_errno = ENOSYS;
         Mmsg1(&dev->errmsg, _("This device does not support %s.\n"), msg);
	 Emsg0(M_ERROR, 0, dev->errmsg);
      }
   }
#ifdef MTIOCLRERR
{
   struct mtop mt_com;
   int stat;
   mt_com.mt_op = MTIOCLRERR;
   mt_com.mt_count = 1;
   stat = ioctl(dev->fd, MTIOCTOP, (char *)&mt_com);
   Dmsg0(200, "Did MTIOCLRERR\n");
}
#endif
}

/*
 * Flush buffer contents
 *  No longer used.
 */
int flush_dev(DEVICE *dev)
{
   return 1;
}

static void do_close(DEVICE *dev)
{

   Dmsg0(29, "really close_dev\n");
   close(dev->fd);
   /* Clean up device packet so it can be reused */
   dev->fd = -1;
   dev->state &= ~(ST_OPENED|ST_LABEL|ST_READ|ST_APPEND|ST_EOT|ST_WEOT|ST_EOF);
   dev->block_num = 0;
   dev->file = 0;
   dev->file_addr = 0;
   dev->LastBlockNumWritten = 0;
   memset(&dev->VolCatInfo, 0, sizeof(dev->VolCatInfo));
   memset(&dev->VolHdr, 0, sizeof(dev->VolHdr));
   dev->use_count--;
}

/* 
 * Close the device
 */
void
close_dev(DEVICE *dev)
{
   if (!dev) {
      Mmsg0(&dev->errmsg, _("Bad call to fsf_dev. Archive not open\n"));
      Emsg0(M_FATAL, 0, dev->errmsg);
      return;
   }
   if (dev->fd >= 0 && dev->use_count == 1) {
      do_close(dev);
   } else {    
      Dmsg0(29, "close_dev but in use so leave open.\n");
      dev->use_count--;
   }
}

/*
 * Used when unmounting the device
 */
void force_close_dev(DEVICE *dev)
{
   if (!dev) {
      Mmsg0(&dev->errmsg, _("Bad call to fsf_dev. Archive not open\n"));
      Emsg0(M_FATAL, 0, dev->errmsg);
      return;
   }
   Dmsg0(29, "really close_dev\n");
   do_close(dev);
}

int truncate_dev(DEVICE *dev)
{
   if (dev->state & ST_TAPE) {
      return 1;
   }
   if (ftruncate(dev->fd, 0) != 0) {
      Mmsg1(&dev->errmsg, _("Unable to truncate device. ERR=%s\n"), strerror(errno));
      return 0;
   }
   return 1;
}

int 
dev_is_tape(DEVICE *dev)
{  
   return (dev->state & ST_TAPE) ? 1 : 0;
}

char *
dev_name(DEVICE *dev)
{
   return dev->dev_name;
}

char *
dev_vol_name(DEVICE *dev)
{
   return dev->VolCatInfo.VolCatName;
}

uint32_t dev_block(DEVICE *dev)
{
   update_pos_dev(dev);
   return dev->block_num;
}

uint32_t dev_file(DEVICE *dev)
{
   update_pos_dev(dev);
   return dev->file;
}

/* 
 * Free memory allocated for the device
 */
void
term_dev(DEVICE *dev)
{
   if (!dev) {
      dev->dev_errno = EBADF;
      Mmsg0(&dev->errmsg, _("Bad call to fsf_dev. Archive not open\n"));
      Emsg0(M_FATAL, 0, dev->errmsg);
      return;
   }
   close_dev(dev);
   Dmsg0(29, "term_dev\n");
   if (dev->dev_name) {
      free_memory(dev->dev_name);
      dev->dev_name = NULL;
   }
   if (dev->errmsg) {
      free_pool_memory(dev->errmsg);
      dev->errmsg = NULL;
   }
#ifdef NEW_LOCK
   rwl_destroy(&dev->lock);
#endif
   pthread_mutex_destroy(&dev->mutex);
   pthread_cond_destroy(&dev->wait);
   pthread_cond_destroy(&dev->wait_next_vol);
   if (dev->state & ST_MALLOC) {
      free_pool_memory((POOLMEM *)dev);
   }
}


/* To make following two functions more readable */

#define attached_jcrs ((JCR *)(dev->attached_jcrs))

void attach_jcr_to_device(DEVICE *dev, JCR *jcr)
{
   jcr->prev_dev = NULL;
   jcr->next_dev = attached_jcrs;
   if (attached_jcrs) {
      attached_jcrs->prev_dev = jcr;
   }
   attached_jcrs = jcr;
   Dmsg1(100, "Attached Job %s\n", jcr->Job);
}

void detach_jcr_from_device(DEVICE *dev, JCR *jcr)
{
   if (!jcr->prev_dev) {
      attached_jcrs = jcr->next_dev;
   } else {
      jcr->prev_dev->next_dev = jcr->next_dev;
   }
   if (jcr->next_dev) {
      jcr->next_dev->prev_dev = jcr->prev_dev; 
   }
   jcr->next_dev = jcr->prev_dev = NULL;
   Dmsg1(100, "Detached Job %s\n", jcr->Job);
}

JCR *next_attached_jcr(DEVICE *dev, JCR *jcr)
{
   if (jcr == NULL) {
      return attached_jcrs;
   }
   return jcr->next_dev;
}
