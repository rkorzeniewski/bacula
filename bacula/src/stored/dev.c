/*
 *
 *   dev.c  -- low level operations on device (storage device)
 *
 *		Kern Sibbald, MM
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
   Copyright (C) 2000-2005 Kern Sibbald

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
 * is set all calls to write_block_to_device() call the fix_up
 * routine. In addition, all threads are blocked
 * from writing on the tape by calling lock_dev(), and thread other
 * than the first thread to hit the EOT will block on a condition
 * variable. The first thread to hit the EOT will continue to
 * be able to read and write the tape (he sort of tunnels through
 * the locking mechanism -- see lock_dev() for details).
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
void set_os_device_parameters(DEVICE *dev);
int mount_dev(DEVICE* dev, int timeout);
int unmount_dev(DEVICE* dev, int timeout);
int write_part(DEVICE *dev);
char *edit_device_codes_dev(DEVICE* dev, char *omsg, const char *imsg);
static void get_filename(DEVICE *dev, char *VolName, POOL_MEM& archive_name);
static void update_free_space_dev(DEVICE* dev);

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
   bool tape, fifo;
   int errstat;
   DCR *dcr = NULL;

   /* Check that device is available */
   if (stat(device->device_name, &statp) < 0) {
      berrno be;
      if (dev) {
	 dev->dev_errno = errno;
      }
      Jmsg2(NULL, M_FATAL, 0, _("Unable to stat device %s: ERR=%s\n"), 
	 device->device_name, be.strerror());
      return NULL;
   }
   
   
   tape = false;
   fifo = false;
   if (S_ISDIR(statp.st_mode)) {
      tape = false;
   } else if (S_ISCHR(statp.st_mode)) {
      tape = true;
   } else if (S_ISFIFO(statp.st_mode)) {
      fifo = true;
   } else {
      if (dev) {
	 dev->dev_errno = ENODEV;
      }
      Emsg2(M_FATAL, 0, _("%s is an unknown device type. Must be tape or directory. st_mode=%x\n"),
	 device->device_name, statp.st_mode);
      return NULL;
   }
   if (!dev) {
      dev = (DEVICE *)get_memory(sizeof(DEVICE));
      memset(dev, 0, sizeof(DEVICE));
      dev->state = ST_MALLOC;
   } else {
      memset(dev, 0, sizeof(DEVICE));
   }

   /* Copy user supplied device parameters from Resource */
   dev->dev_name = get_memory(strlen(device->device_name)+1);
   pm_strcpy(dev->dev_name, device->device_name);
   dev->capabilities = device->cap_bits;
   dev->min_block_size = device->min_block_size;
   dev->max_block_size = device->max_block_size;
   dev->max_volume_size = device->max_volume_size;
   dev->max_file_size = device->max_file_size;
   dev->volume_capacity = device->volume_capacity;
   dev->max_rewind_wait = device->max_rewind_wait;
   dev->max_open_wait = device->max_open_wait;
   dev->max_open_vols = device->max_open_vols;
   dev->vol_poll_interval = device->vol_poll_interval;
   dev->max_spool_size = device->max_spool_size;
   dev->drive_index = device->drive_index;
   if (tape) { /* No parts on tapes */
      dev->max_part_size = 0;
   }
   else {
      dev->max_part_size = device->max_part_size;
   }
   /* Sanity check */
   if (dev->vol_poll_interval && dev->vol_poll_interval < 60) {
      dev->vol_poll_interval = 60;
   }
   dev->device = device;

   if (tape) {
      dev->state |= ST_TAPE;
   } else if (fifo) {
      dev->state |= ST_FIFO;
      dev->capabilities |= CAP_STREAM; /* set stream device */
   } else {
      dev->state |= ST_FILE;
   }

   /* If the device requires mount :
    * - Check that the mount point is available 
    * - Check that (un)mount commands are defined
    */
   if (dev->is_file() && device->cap_bits & CAP_REQMOUNT) {
      if (stat(device->mount_point, &statp) < 0) {
	 berrno be;
	 dev->dev_errno = errno;
         Jmsg2(NULL, M_FATAL, 0, _("Unable to stat mount point %s: ERR=%s\n"), 
	    device->mount_point, be.strerror());
	 return NULL;
      }
      if (!device->mount_command || !device->unmount_command) {
         Jmsg0(NULL, M_ERROR_TERM, 0, _("Mount and unmount commands must defined for a device which requires mount.\n"));
      }
      if (!device->write_part_command) {
         Jmsg0(NULL, M_ERROR_TERM, 0, _("Write part command must be defined for a device which requires mount.\n"));
      }
      dev->state |= ST_DVD;
   }

   if (dev->max_block_size > 1000000) {
      Emsg3(M_ERROR, 0, _("Block size %u on device %s is too large, using default %u\n"),
	 dev->max_block_size, dev->dev_name, DEFAULT_BLOCK_SIZE);
      dev->max_block_size = 0;
   }
   if (dev->max_block_size % TAPE_BSIZE != 0) {
      Emsg2(M_WARNING, 0, _("Max block size %u not multiple of device %s block size.\n"),
	 dev->max_block_size, dev->dev_name);
   }

   dev->errmsg = get_pool_memory(PM_EMSG);
   *dev->errmsg = 0;

   if ((errstat = pthread_mutex_init(&dev->mutex, NULL)) != 0) {
      berrno be;
      dev->dev_errno = errstat;
      Mmsg1(dev->errmsg, _("Unable to init mutex: ERR=%s\n"), be.strerror(errstat));
      Emsg0(M_FATAL, 0, dev->errmsg);
   }
   if ((errstat = pthread_cond_init(&dev->wait, NULL)) != 0) {
      berrno be;
      dev->dev_errno = errstat;
      Mmsg1(dev->errmsg, _("Unable to init cond variable: ERR=%s\n"), be.strerror(errstat));
      Emsg0(M_ERROR_TERM, 0, dev->errmsg);
   }
   if ((errstat = pthread_cond_init(&dev->wait_next_vol, NULL)) != 0) {
      berrno be;
      dev->dev_errno = errstat;
      Mmsg1(dev->errmsg, _("Unable to init cond variable: ERR=%s\n"), be.strerror(errstat));
      Emsg0(M_ERROR_TERM, 0, dev->errmsg);
   }
   if ((errstat = pthread_mutex_init(&dev->spool_mutex, NULL)) != 0) {
      berrno be;
      dev->dev_errno = errstat;
      Mmsg1(dev->errmsg, _("Unable to init mutex: ERR=%s\n"), be.strerror(errstat));
      Emsg0(M_ERROR_TERM, 0, dev->errmsg);
   }
   if ((errstat = rwl_init(&dev->lock)) != 0) {
      berrno be;
      dev->dev_errno = errstat;
      Mmsg1(dev->errmsg, _("Unable to init mutex: ERR=%s\n"), be.strerror(errstat));
      Emsg0(M_ERROR_TERM, 0, dev->errmsg);
   }

   dev->fd = -1;
   dev->attached_dcrs = New(dlist(dcr, &dcr->dev_link));
   Dmsg2(29, "init_dev: tape=%d dev_name=%s\n", dev_is_tape(dev), dev->dev_name);
   
   return dev;
}

/* 
 * Write the current volume/part filename to archive_name.
 */
static void get_filename(DEVICE *dev, char *VolName, POOL_MEM& archive_name) 
{
   char partnumber[20];
   
   if (dev->is_dvd()) {
	 /* If we try to open the last part, just open it from disk, 
	 * otherwise, open it from the spooling directory */
      if (dev->part < dev->num_parts) {
	 pm_strcpy(archive_name, dev->device->mount_point);
      } else {
	 /* Use the working directory if spool directory is not defined */
	 if (dev->device->spool_directory) {
	    pm_strcpy(archive_name, dev->device->spool_directory);
	 } else {
	    pm_strcpy(archive_name, working_directory);
	 }
      }
   } else {
      pm_strcpy(archive_name, dev->dev_name);
   }
      
   if (archive_name.c_str()[strlen(archive_name.c_str())-1] != '/') {
      pm_strcat(archive_name, "/");
   }
   pm_strcat(archive_name, VolName);
   /* if part != 0, append .# to the filename (where # is the part number) */
   if (dev->is_dvd() && dev->part != 0) {
      pm_strcat(archive_name, ".");
      bsnprintf(partnumber, sizeof(partnumber), "%d", dev->part);
      pm_strcat(archive_name, partnumber);
   }
}  

/*
 * Open the device with the operating system and
 * initialize buffer pointers.
 *
 * Returns:  -1  on error
 *	     fd  on success
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
   if (dev->is_open()) {
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
      Emsg1(M_WARNING, 0, "%s", dev->errmsg);
      return dev->fd;
   }
   if (VolName) {
      bstrncpy(dev->VolCatInfo.VolCatName, VolName, sizeof(dev->VolCatInfo.VolCatName));
   }

   Dmsg3(29, "open_dev: tape=%d dev_name=%s vol=%s\n", dev_is_tape(dev),
	 dev->dev_name, dev->VolCatInfo.VolCatName);
   dev->state &= ~(ST_LABEL|ST_APPEND|ST_READ|ST_EOT|ST_WEOT|ST_EOF);
   if (dev->is_tape() || dev->is_fifo()) {
      dev->file_size = 0;
      int timeout;
      Dmsg0(29, "open_dev: device is tape\n");
      if (mode == OPEN_READ_WRITE) {
	 dev->mode = O_RDWR | O_BINARY;
      } else if (mode == OPEN_READ_ONLY) {
	 dev->mode = O_RDONLY | O_BINARY;
      } else if (mode == OPEN_WRITE_ONLY) {
	 dev->mode = O_WRONLY | O_BINARY;
      } else {
         Emsg0(M_ABORT, 0, _("Illegal mode given to open_dev.\n"));
      }
      timeout = dev->max_open_wait;
      errno = 0;
      if (dev->is_fifo() && timeout) {
	 /* Set open timer */
	 dev->tid = start_thread_timer(pthread_self(), timeout);
      }
      /* If busy retry each second for max_open_wait seconds */
      while ((dev->fd = open(dev->dev_name, dev->mode, MODE_RW)) < 0) {
	 berrno be;
	 if (errno == EINTR || errno == EAGAIN) {
	    continue;
	 }
	 if (errno == EBUSY && timeout-- > 0) {
            Dmsg2(100, "Device %s busy. ERR=%s\n", dev->dev_name, be.strerror());
	    bmicrosleep(1, 0);
	    continue;
	 }
	 dev->dev_errno = errno;
         Mmsg2(&dev->errmsg, _("stored: unable to open device %s: ERR=%s\n"),
	       dev->dev_name, be.strerror());
	 /* Stop any open timer we set */
	 if (dev->tid) {
	    stop_thread_timer(dev->tid);
	    dev->tid = 0;
	 }
	 Emsg0(M_FATAL, 0, dev->errmsg);
	 break;
      }
      if (dev->fd >= 0) {
	 dev->dev_errno = 0;
	 dev->state |= ST_OPENED;
	 dev->use_count = 1;
	 update_pos_dev(dev);		  /* update position */
	 set_os_device_parameters(dev);      /* do system dependent stuff */
      }
      /* Stop any open() timer we started */
      if (dev->tid) {
	 stop_thread_timer(dev->tid);
	 dev->tid = 0;
      }
      Dmsg1(29, "open_dev: tape %d opened\n", dev->fd);
   } else {
      POOL_MEM archive_name(PM_FNAME);
      struct stat filestat;
      /*
       * Handle opening of File Archive (not a tape)
       */     
      if (dev->part == 0) {
	 dev->file_size = 0;
      }
      dev->part_size = 0;
      
      /* if num_parts has not been set, but VolCatInfo is available, copy
       * it from the VolCatInfo.VolCatParts */
      if (dev->num_parts < dev->VolCatInfo.VolCatParts) {
	 dev->num_parts = dev->VolCatInfo.VolCatParts;
      }
      
      if (VolName == NULL || *VolName == 0) {
         Mmsg(dev->errmsg, _("Could not open file device %s. No Volume name given.\n"),
	    dev->dev_name);
	 return -1;
      }
      get_filename(dev, VolName, archive_name);

      if (dev->is_dvd()) {
	 if (mount_dev(dev, 1) < 0) {
            Mmsg(dev->errmsg, _("Could not mount archive device %s.\n"),
		 dev->dev_name);
	    Emsg0(M_FATAL, 0, dev->errmsg);
	    dev->fd = -1;
	    return dev->fd;
	 }
      }
	    
      Dmsg2(29, "open_dev: device is disk %s (mode:%d)\n", archive_name.c_str(), mode);
      dev->openmode = mode;
      
      /*
       * If we are not trying to access the last part, set mode to 
       *   OPEN_READ_ONLY as writing would be an error.
       */
      if (dev->part < dev->num_parts) {
	 mode = OPEN_READ_ONLY;
      }
      
      if (mode == OPEN_READ_WRITE) {
	 dev->mode = O_CREAT | O_RDWR | O_BINARY;
      } else if (mode == OPEN_READ_ONLY) {
	 dev->mode = O_RDONLY | O_BINARY;
      } else if (mode == OPEN_WRITE_ONLY) {
	 dev->mode = O_WRONLY | O_BINARY;
      } else {
         Emsg0(M_ABORT, 0, _("Illegal mode given to open_dev.\n"));
      }
      /* If creating file, give 0640 permissions */
      if ((dev->fd = open(archive_name.c_str(), dev->mode, 0640)) < 0) {
	 berrno be;
	 dev->dev_errno = errno;
         Mmsg2(&dev->errmsg, _("Could not open: %s, ERR=%s\n"), archive_name.c_str(), be.strerror());
	 Emsg0(M_FATAL, 0, dev->errmsg);
      } else {
	 dev->dev_errno = 0;
	 dev->state |= ST_OPENED;
	 dev->use_count = 1;
	 update_pos_dev(dev);		     /* update position */
	 if (fstat(dev->fd, &filestat) < 0) {
	    berrno be;
	    dev->dev_errno = errno;
            Mmsg2(&dev->errmsg, _("Could not fstat: %s, ERR=%s\n"), archive_name.c_str(), be.strerror());
	    Emsg0(M_FATAL, 0, dev->errmsg);
	 } else {
	    dev->part_size = filestat.st_size;
	 }
      }
      Dmsg4(29, "open_dev: disk fd=%d opened, part=%d/%d, part_size=%u\n", dev->fd, dev->part, dev->num_parts, dev->part_size);
      if (dev->is_dvd() && (dev->mode != OPEN_READ_ONLY) && 
	  ((dev->free_space_errno == 0) || (dev->num_parts == dev->part))) {
	 update_free_space_dev(dev);
      }
   }
   return dev->fd;
}

/* (Un)mount the device */
int do_mount_dev(DEVICE* dev, int mount, int dotimeout) {
   POOL_MEM ocmd(PM_FNAME);
   POOLMEM* results;
   results = get_pool_memory(PM_MESSAGE);
   char* icmd;
   int status, timeout;
   
   if (mount) {
      icmd = dev->device->mount_command;
   }
   else {
      icmd = dev->device->unmount_command;
   }
   
   edit_device_codes_dev(dev, ocmd.c_str(), icmd);
   
   Dmsg2(29, "do_mount_dev: cmd=%s state=%d\n", ocmd.c_str(), dev->state & ST_MOUNTED);

   if (dotimeout) {
      /* Try at most 5 times to (un)mount the device. This should perhaps be configurable. */
      timeout = 5;
   }
   else {
      timeout = 0;
   }
   /* If busy retry each second */
   while ((status = run_program_full_output(ocmd.c_str(), dev->max_open_wait/2, results)) != 0) {
      if (--timeout > 0) {
         Dmsg2(40, "Device %s cannot be (un)mounted. Retrying... ERR=%s\n", dev->dev_name, results);
	 /* Sometimes the device cannot be mounted because it is already mounted.
	  * Try to unmount it, then remount it */
	 if (mount) {
            Dmsg1(40, "Trying to unmount the device %s...\n", dev->dev_name);
	    do_mount_dev(dev, 0, 0);
	 }
	 bmicrosleep(1, 0);
	 continue;
      }
      free_pool_memory(results);
      Dmsg2(40, "Device %s cannot be mounted. ERR=%s\n", dev->dev_name, results);
      return -1;
   }
   
   if (mount) {
     dev->state |= ST_MOUNTED;
   }
   else {
     dev->state &= ~ST_MOUNTED;
   }
   free_pool_memory(results);
   
   Dmsg1(29, "do_mount_dev: end_state=%d\n", dev->state & ST_MOUNTED);
   return 0;
}

/* Only for devices that requires mount.
 * Try to find the volume name of the loaded device, and open the
 * first part of this volume. 
 *
 * Returns 0 if read_dev_volume_label can now read the label,
 * -1 if an error occured, and read_dev_volume_label_guess must abort with an IO_ERROR.
 *
 * To guess the device name, it lists all the files on the DVD, and searches for a 
 * file which has a minimum size (500 bytes). If this file has a numeric extension,
 * like part files, try to open the file which has no extension (e.g. the first
 * part file).
 * So, if the DVD does not contains a Bacula volume, a random file is opened,
 * and no valid label could be read from this file.
 *
 * It is useful, so the operator can be told that a wrong volume is mounted, with
 * the label name of the current volume. We can also check that the currently
 * mounted disk is writable. (See also read_dev_volume_label_guess in label.c).
 *
 * Note that if the right volume is mounted, open_guess_name_dev returns the same
 * result as an usual open_dev.
 */
int open_guess_name_dev(DEVICE *dev) 
{
   Dmsg1(29, "open_guess_name_dev: dev=%s\n", dev->dev_name);
   POOL_MEM guessedname(PM_FNAME);
   DIR* dp;
   struct dirent *entry, *result;
   struct stat statp;
   int index;
   int name_max;
   
   if (!dev->is_dvd()) {
      Dmsg1(100, "open_guess_name_dev: device does not require mount, returning 0. dev=%s\n", dev->dev_name);
      return 0;
   }

#ifndef HAVE_DIRENT_H
   Dmsg0(29, "open_guess_name_dev: readdir not available, cannot guess volume name\n");
   return 0; 
#endif
   
   update_free_space_dev(dev);

   if (mount_dev(dev, 1) < 0) {
      /* If the device cannot be mounted, check if it is writable */
      if (dev->free_space_errno >= 0) {
         Dmsg1(100, "open_guess_name_dev: device cannot be mounted, but it seems to be writable, returning 0. dev=%s\n", dev->dev_name);
	 return 0;
      } else {
         Dmsg1(100, "open_guess_name_dev: device cannot be mounted, and is not writable, returning 0. dev=%s\n", dev->dev_name);
	 /* read_dev_volume_label_guess must now check dev->free_space_errno to understand that the media is not writable. */
	 return 0;
      }
   }
      
   name_max = pathconf(".", _PC_NAME_MAX);
   if (name_max < 1024) {
      name_max = 1024;
   }
      
   if (!(dp = opendir(dev->device->mount_point))) {
      berrno be;
      dev->dev_errno = errno;
      Dmsg3(29, "open_guess_name_dev: failed to open dir %s (dev=%s), ERR=%s\n", dev->device->mount_point, dev->dev_name, be.strerror());
      return -1;
   }
   
   entry = (struct dirent *)malloc(sizeof(struct dirent) + name_max + 100);
   while (1) {
      if ((readdir_r(dp, entry, &result) != 0) || (result == NULL)) {
	 dev->dev_errno = ENOENT;
         Dmsg2(29, "open_guess_name_dev: failed to find suitable file in dir %s (dev=%s)\n", dev->device->mount_point, dev->dev_name);
	 closedir(dp);
	 return -1;
      }
      
      ASSERT(name_max+1 > (int)sizeof(struct dirent) + (int)NAMELEN(entry));
      
      if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
	 continue;
      }
      
      pm_strcpy(guessedname, dev->device->mount_point);
      if (guessedname.c_str()[strlen(guessedname.c_str())-1] != '/') {
         pm_strcat(guessedname, "/");
      }
      pm_strcat(guessedname, entry->d_name);
      
      if (stat(guessedname.c_str(), &statp) < 0) {
	 berrno be;
         Dmsg3(29, "open_guess_name_dev: failed to stat %s (dev=%s), ERR=%s\n",
	       guessedname.c_str(), dev->dev_name, be.strerror());
	 continue;
      }
      
      if (!S_ISREG(statp.st_mode) || (statp.st_size < 500)) {
         Dmsg2(100, "open_guess_name_dev: %s is not a regular file, or less than 500 bytes (dev=%s)\n", 
	       guessedname.c_str(), dev->dev_name);
	 continue;
      }
      
      /* Ok, we found a good file, remove the part extension if possible. */
      for (index = strlen(guessedname.c_str())-1; index >= 0; index--) {
         if ((guessedname.c_str()[index] == '/') || 
             (guessedname.c_str()[index] < '0') || 
             (guessedname.c_str()[index] > '9')) {
	    break;
	 }
         if (guessedname.c_str()[index] == '.') {
            guessedname.c_str()[index] = '\0';
	    break;
	 }
      }
      
      if ((stat(guessedname.c_str(), &statp) < 0) || (statp.st_size < 500)) {
	 /* The file with extension truncated does not exists or is too small, so use it with its extension. */
	 berrno be;
         Dmsg3(100, "open_guess_name_dev: failed to stat %s (dev=%s), using the file with its extension, ERR=%s\n", 
	       guessedname.c_str(), dev->dev_name, be.strerror());
	 pm_strcpy(guessedname, dev->device->mount_point);
         if (guessedname.c_str()[strlen(guessedname.c_str())-1] != '/') {
            pm_strcat(guessedname, "/");
	 }
	 pm_strcat(guessedname, entry->d_name);
	 continue;
      }
      break;
   }
   
   closedir(dp);
   
   if (dev->fd >= 0) {
      close(dev->fd);
   }
     
   if ((dev->fd = open(guessedname.c_str(), O_RDONLY | O_BINARY)) < 0) {
      berrno be;
      dev->dev_errno = errno;
      Dmsg3(29, "open_guess_name_dev: failed to open %s (dev=%s), ERR=%s\n", 
	    guessedname.c_str(), dev->dev_name, be.strerror());
      if (open_first_part(dev) < 0) {
	 berrno be;
	 dev->dev_errno = errno;
         Mmsg1(&dev->errmsg, _("Could not open_first_part, ERR=%s\n"), be.strerror());
	 Emsg0(M_FATAL, 0, dev->errmsg);	 
      }
      return -1;
   }
   dev->part_start = 0;
   dev->part_size = statp.st_size;
   dev->part = 0;
   dev->state |= ST_OPENED;
   dev->use_count = 1;
   
   Dmsg2(29, "open_guess_name_dev: %s opened (dev=%s)\n", guessedname.c_str(), dev->dev_name);
   
   return 0;
}

/* Mount the device.
 * If timeout, wait until the mount command returns 0.
 * If !timeout, try to mount the device only once.
 */
int mount_dev(DEVICE* dev, int timeout) 
{
   if (dev->state & ST_MOUNTED) {
      Dmsg0(100, "mount_dev: Device already mounted\n");
      return 0;
   } else {
      return do_mount_dev(dev, 1, timeout);
   }
}

/* Unmount the device
 * If timeout, wait until the unmount command returns 0.
 * If !timeout, try to unmount the device only once.
 */
int unmount_dev(DEVICE* dev, int timeout) 
{
   if (dev->state & ST_MOUNTED) {
      return do_mount_dev(dev, 0, timeout);
   } else {
      Dmsg0(100, "mount_dev: Device already unmounted\n");
      return 0;
   }
}

/* Update the free space on the device */
static void update_free_space_dev(DEVICE* dev) 
{
   POOL_MEM ocmd(PM_FNAME);
   POOLMEM* results;
   char* icmd;
   int timeout;
   long long int free;
   
   icmd = dev->device->free_space_command;
   
   if (!icmd) {
      dev->free_space = 0;
      dev->free_space_errno = 0;
      Dmsg2(29, "update_free_space_dev: free_space=%d, free_space_errno=%d (!icmd)\n", dev->free_space, dev->free_space_errno);
      return;
   }
   
   edit_device_codes_dev(dev, ocmd.c_str(), icmd);
   
   Dmsg1(29, "update_free_space_dev: cmd=%s\n", ocmd.c_str());

   results = get_pool_memory(PM_MESSAGE);
   
   /* Try at most 3 times to get the free space on the device. This should perhaps be configurable. */
   timeout = 3;
   
   while (1) {
      char ed1[50];
      if (run_program_full_output(ocmd.c_str(), dev->max_open_wait/2, results) == 0) {
         Dmsg1(100, "Free space program run : %s\n", results);
	 free = str_to_int64(results);
	 if (free >= 0) {
	    dev->free_space = free;
	    dev->free_space_errno = 1;
            Mmsg0(dev->errmsg, "");
	    break;
	 }
      }
      dev->free_space = 0;
      dev->free_space_errno = -EPIPE;
      Mmsg1(dev->errmsg, "Cannot run free space command (%s)\n", results);
      
      if (--timeout > 0) {
         Dmsg4(40, "Cannot get free space on device %s. free_space=%s, "
            "free_space_errno=%d ERR=%s\n", dev->dev_name, 
	       edit_uint64(dev->free_space, ed1), dev->free_space_errno, 
	       dev->errmsg);
	 bmicrosleep(1, 0);
	 continue;
      }

      dev->dev_errno = -dev->free_space_errno;
      Dmsg4(40, "Cannot get free space on device %s. free_space=%s, "
         "free_space_errno=%d ERR=%s\n",
	    dev->dev_name, edit_uint64(dev->free_space, ed1),
	    dev->free_space_errno, dev->errmsg);
      break;
   }
   
   free_pool_memory(results);
   Dmsg2(29, "update_free_space_dev: free_space=%lld, free_space_errno=%d\n", dev->free_space, dev->free_space_errno);
   return;
}

int write_part(DEVICE *dev) 
{
   Dmsg1(29, "write_part: device is %s\n", dev->dev_name);
   
   if (unmount_dev(dev, 1) < 0) {
      Dmsg0(29, "write_part: unable to unmount the device\n");
   }
   
   POOL_MEM ocmd(PM_FNAME);
   POOLMEM *results;
   results = get_pool_memory(PM_MESSAGE);
   char* icmd;
   int status;
   int timeout;
   
   icmd = dev->device->write_part_command;
   
   edit_device_codes_dev(dev, ocmd.c_str(), icmd);
      
   /* Wait at most the time a maximum size part is written in DVD 0.5x speed
    * FIXME: Minimum speed should be in device configuration 
    */
   timeout = dev->max_open_wait + (dev->max_part_size/(1350*1024/2));
   
   Dmsg2(29, "write_part: cmd=%s timeout=%d\n", ocmd.c_str(), timeout);
      
   status = run_program_full_output(ocmd.c_str(), timeout, results);
   if (status != 0) {
      Mmsg1(dev->errmsg, "Error while writing current part to the DVD: %s", results);
      dev->dev_errno = EIO;
      free_pool_memory(results);
      return -1;
   }
   else {
      Dmsg1(29, "write_part: command output=%s\n", results);
      POOL_MEM archive_name(PM_FNAME);
      get_filename(dev, dev->VolCatInfo.VolCatName, archive_name);
      unlink(archive_name.c_str());
      free_pool_memory(results);
      return 0;
   }
}

/* Open the next part file.
 *  - Close the fd
 *  - Increment part number 
 *  - Reopen the device
 */
int open_next_part(DEVICE *dev) {
   int state;
      
   Dmsg3(29, "open_next_part %s %s %d\n", dev->dev_name, dev->VolCatInfo.VolCatName, dev->openmode);
   /* When appending, do not open a new part if the current is empty */
   if (dev->can_append() && (dev->part == dev->num_parts) && 
       (dev->part_size == 0)) {
      Dmsg0(29, "open_next_part exited immediately (dev->part_size == 0).\n");
      return dev->fd;
   }
   
   if (dev->fd >= 0) {
      close(dev->fd);
   }
   
   dev->fd = -1;
   
   state = dev->state;
   dev->state &= ~ST_OPENED;
   
   if (dev->is_dvd() && (dev->part == dev->num_parts) && dev->can_append()) {
      if (write_part(dev) < 0) {
	 return -1;
      }
   }
     
   dev->part_start += dev->part_size;
   dev->part++;
   
   if ((dev->num_parts < dev->part) && (dev->state & ST_APPEND)) {
      dev->num_parts = dev->part;
      
      /* Check that the next part file does not exists.
       * If it does, move it away... */
      POOL_MEM archive_name(PM_FNAME);
      POOL_MEM archive_bkp_name(PM_FNAME);
      struct stat buf;
      
      get_filename(dev, dev->VolCatInfo.VolCatName, archive_name);
      
      /* Check if the next part exists. */
      if ((stat(archive_name.c_str(), &buf) == 0) || (errno != ENOENT)) {
         Dmsg1(29, "open_next_part %s is in the way, moving it away...\n", archive_name.c_str());
	 pm_strcpy(archive_bkp_name, archive_name.c_str());
         pm_strcat(archive_bkp_name, ".bak");
	 unlink(archive_bkp_name.c_str()); 
	 
	 /* First try to rename it */
	 if (rename(archive_name.c_str(), archive_bkp_name.c_str()) < 0) {
	    berrno be;
            Dmsg3(29, "open_next_part can't rename %s to %s, ERR=%s\n", 
		  archive_name.c_str(), archive_bkp_name.c_str(), be.strerror());
	    /* Then try to unlink it */
	    if (unlink(archive_name.c_str()) < 0) {
	       berrno be;
	       dev->dev_errno = errno;
               Mmsg2(&dev->errmsg, _("open_next_part can't unlink existing part %s, ERR=%s\n"), 
		      archive_name.c_str(), be.strerror());
	       Emsg0(M_FATAL, 0, dev->errmsg);
	       return -1;
	    }
	 }
      }
   }
   
   if (open_dev(dev, dev->VolCatInfo.VolCatName, dev->openmode) < 0) {
      return -1;
   } else {
      dev->state = state;
      return dev->fd;
   }
}

/* Open the first part file.
 *  - Close the fd
 *  - Reopen the device
 */
int open_first_part(DEVICE *dev) {
   int state;
      
   Dmsg3(29, "open_first_part %s %s %d\n", dev->dev_name, dev->VolCatInfo.VolCatName, dev->openmode);
   if (dev->fd >= 0) {
      close(dev->fd);
   }
   
   dev->fd = -1;
   state = dev->state;
   dev->state &= ~ST_OPENED;
   
   dev->part_start = 0;
   dev->part = 0;
   
   if (open_dev(dev, dev->VolCatInfo.VolCatName, dev->openmode)) {
      dev->state = state;
      return dev->fd;
   } else {
      return 0;
   }
}

#ifdef debug_tracing
#undef rewind_dev
bool _rewind_dev(char *file, int line, DEVICE *dev)
{
   Dmsg2(100, "rewind_dev called from %s:%d\n", file, line);
   return rewind_dev(dev);
}
#endif

/* Protected version of lseek, which opens the right part if necessary */
off_t lseek_dev(DEVICE *dev, off_t offset, int whence)
{
   int pos, openmode;
   
   if (dev->num_parts == 0) { /* If there is only one part, simply call lseek. */
      return lseek(dev->fd, offset, whence);
   }
      
   switch(whence) {
   case SEEK_SET:
      Dmsg1(100, "lseek_dev SEEK_SET called %d\n", offset);
      if ((uint64_t)offset >= dev->part_start) {
	 if ((uint64_t)(offset - dev->part_start) < dev->part_size) {
	    /* We are staying in the current part, just seek */
	    if ((pos = lseek(dev->fd, (off_t)(offset-dev->part_start), SEEK_SET)) < 0) {
	       return pos;   
	    } else {
	       return pos + dev->part_start;
	    }
	 } else {
	    /* Load next part, and start again */
	    if (open_next_part(dev) < 0) {
               Dmsg0(100, "lseek_dev failed while trying to open the next part\n");
	       return -1;
	    }
	    return lseek_dev(dev, offset, SEEK_SET);
	 }
      } else {
	 /* pos < dev->part_start :
	  * We need to access a previous part, 
	  * so just load the first one, and seek again
	  * until the right one is loaded */
	 if (open_first_part(dev) < 0) {
            Dmsg0(100, "lseek_dev failed while trying to open the first part\n");
	    return -1;
	 }
	 return lseek_dev(dev, offset, SEEK_SET);
      }
      break;
   case SEEK_CUR:
      Dmsg1(100, "lseek_dev SEEK_CUR called %d\n", offset);
      if ((pos = lseek(dev->fd, (off_t)0, SEEK_CUR)) < 0) {
	 return pos;   
      }
      pos += dev->part_start;
      if (offset == 0) {
	 return pos;
      }
      else { /* Not used in Bacula, but should work */
	 return lseek_dev(dev, pos, SEEK_SET);
      }
      break;
   case SEEK_END:
      Dmsg1(100, "lseek_dev SEEK_END called %d\n", offset);
      if (offset > 0) { /* Not used by bacula */
         Dmsg1(100, "lseek_dev SEEK_END called with an invalid offset %d\n", offset);
	 errno = EINVAL;
	 return -1;
      }
      
      if (dev->part == dev->num_parts) { /* The right part is already loaded */
	 if ((pos = lseek(dev->fd, (off_t)0, SEEK_END)) < 0) {
	    return pos;   
	 } else {
	    return pos + dev->part_start;
	 }
      } else {
	 /* Load the first part, then load the next until we reach the last one.
	  * This is the only way to be sure we compute the right file address. */
	 /* Save previous openmode, and open all but last part read-only (useful for DVDs) */
	 openmode = dev->openmode;
	 dev->openmode = OPEN_READ_ONLY;
	 
	 /* Works because num_parts > 0. */
	 if (open_first_part(dev) < 0) {
            Dmsg0(100, "lseek_dev failed while trying to open the first part\n");
	    return -1;
	 }
	 while (dev->part < (dev->num_parts-1)) {
	    if (open_next_part(dev) < 0) {
               Dmsg0(100, "lseek_dev failed while trying to open the next part\n");
	       return -1;
	    }
	 }
	 dev->openmode = openmode;
	 if (open_next_part(dev) < 0) {
            Dmsg0(100, "lseek_dev failed while trying to open the next part\n");
	    return -1;
	 }
	 return lseek_dev(dev, 0, SEEK_END);
      }
      break;
   default:
      errno = EINVAL;
      return -1;
   }
}

/*
 * Rewind the device.
 *  Returns: true  on success
 *	     false on failure
 */
bool rewind_dev(DEVICE *dev)
{
   struct mtop mt_com;
   unsigned int i;

   Dmsg1(29, "rewind_dev %s\n", dev->dev_name);
   if (dev->fd < 0) {
      dev->dev_errno = EBADF;
      Mmsg1(&dev->errmsg, _("Bad call to rewind_dev. Device %s not open\n"),
	    dev->dev_name);
      Emsg0(M_ABORT, 0, dev->errmsg);
      return false;
   }
   dev->state &= ~(ST_EOT|ST_EOF|ST_WEOT);  /* remove EOF/EOT flags */
   dev->block_num = dev->file = 0;
   dev->file_size = 0;
   dev->file_addr = 0;
   if (dev->is_tape()) {
      mt_com.mt_op = MTREW;
      mt_com.mt_count = 1;
      /* If we get an I/O error on rewind, it is probably because
       * the drive is actually busy. We loop for (about 5 minutes)
       * retrying every 5 seconds.
       */
      for (i=dev->max_rewind_wait; ; i -= 5) {
	 if (ioctl(dev->fd, MTIOCTOP, (char *)&mt_com) < 0) {
	    berrno be;
	    clrerror_dev(dev, MTREW);
	    if (i == dev->max_rewind_wait) {
               Dmsg1(200, "Rewind error, %s. retrying ...\n", be.strerror());
	    }
	    if (dev->dev_errno == EIO && i > 0) {
               Dmsg0(200, "Sleeping 5 seconds.\n");
	       bmicrosleep(5, 0);
	       continue;
	    }
            Mmsg2(&dev->errmsg, _("Rewind error on %s. ERR=%s.\n"),
	       dev->dev_name, be.strerror());
	    return false;
	 }
	 break;
      }
   } else if (dev->is_file()) {      
      if (lseek_dev(dev, (off_t)0, SEEK_SET) < 0) {
	 berrno be;
	 dev->dev_errno = errno;
         Mmsg2(&dev->errmsg, _("lseek_dev error on %s. ERR=%s.\n"),
	    dev->dev_name, be.strerror());
	 return false;
      }
   }
   return true;
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
   dev->file_size = 0;
   dev->file_addr = 0;
   if (dev->state & (ST_FIFO | ST_PROG)) {
      return 1;
   }
   if (!(dev->is_tape())) {
      pos = lseek_dev(dev, (off_t)0, SEEK_END);
//    Dmsg1(100, "====== Seek to %lld\n", pos);
      if (pos >= 0) {
	 update_pos_dev(dev);
	 dev->state |= ST_EOT;
	 return 1;
      }
      dev->dev_errno = errno;
      berrno be;
      Mmsg2(&dev->errmsg, _("lseek_dev error on %s. ERR=%s.\n"),
	     dev->dev_name, be.strerror());
      return 0;
   }
#ifdef MTEOM

   if (dev_cap(dev, CAP_MTIOCGET) && dev_cap(dev, CAP_FASTFSF) && 
      !dev_cap(dev, CAP_EOM)) {
      struct mtget mt_stat;
      Dmsg0(100,"Using FAST FSF for EOM\n");
      if (ioctl(dev->fd, MTIOCGET, (char *)&mt_stat) == 0 && mt_stat.mt_fileno <= 0) {
	if (!rewind_dev(dev)) {
	  return 0;
	}
      }
      mt_com.mt_op = MTFSF;
      /*
       * ***FIXME*** fix code to handle case that INT16_MAX is
       *   not large enough.
       */
      mt_com.mt_count = INT16_MAX;    /* use big positive number */
      if (mt_com.mt_count < 0) {
	 mt_com.mt_count = INT16_MAX; /* brain damaged system */
      }
   }

   if (dev_cap(dev, CAP_MTIOCGET) && (dev_cap(dev, CAP_FASTFSF) || dev_cap(dev, CAP_EOM))) {
      if (dev_cap(dev, CAP_EOM)) {
         Dmsg0(100,"Using EOM for EOM\n");
	 mt_com.mt_op = MTEOM;
	 mt_com.mt_count = 1;
      }

      if ((stat=ioctl(dev->fd, MTIOCTOP, (char *)&mt_com)) < 0) {
	 berrno be;
	 clrerror_dev(dev, mt_com.mt_op);
         Dmsg1(50, "ioctl error: %s\n", be.strerror());
	 update_pos_dev(dev);
         Mmsg2(&dev->errmsg, _("ioctl MTEOM error on %s. ERR=%s.\n"),
	    dev->dev_name, be.strerror());
	 return 0;
      }

      if (ioctl(dev->fd, MTIOCGET, (char *)&mt_stat) < 0) {
	 berrno be;
	 clrerror_dev(dev, -1);
         Mmsg2(&dev->errmsg, _("ioctl MTIOCGET error on %s. ERR=%s.\n"),
	    dev->dev_name, be.strerror());
	 return 0;
      }
      Dmsg2(100, "EOD file=%d block=%d\n", mt_stat.mt_fileno, mt_stat.mt_blkno);
      dev->file = mt_stat.mt_fileno;

   /*
    * Rewind then use FSF until EOT reached
    */
   } else {
#else
   {
#endif
      if (!rewind_dev(dev)) {
	 return 0;
      }
      /*
       * Move file by file to the end of the tape
       */
      int file_num;
      for (file_num=dev->file; !(dev->state & ST_EOT); file_num++) {
         Dmsg0(200, "eod_dev: doing fsf 1\n");
	 if (!fsf_dev(dev, 1)) {
            Dmsg0(200, "fsf_dev error.\n");
	    return 0;
	 }
	 /*
	  * Avoid infinite loop. ***FIXME*** possibly add code
	  *   to set EOD or to turn off CAP_FASTFSF if on.
	  */
	 if (file_num == (int)dev->file) {
	    struct mtget mt_stat;
            Dmsg1(100, "fsf_dev did not advance from file %d\n", file_num);
	    if (dev_cap(dev, CAP_MTIOCGET) && ioctl(dev->fd, MTIOCGET, (char *)&mt_stat) == 0 &&
		      mt_stat.mt_fileno >= 0) {
               Dmsg2(100, "Adjust file from %d to %d\n", dev->file , mt_stat.mt_fileno);
	       dev->file = mt_stat.mt_fileno;
	    }
	    stat = 0;
	    break;		      /* we are not progressing, bail out */
	 }
      }
   }
   /*
    * Some drivers leave us after second EOF when doing
    * MTEOM, so we must backup so that appending overwrites
    * the second EOF.
    */
   if (dev_cap(dev, CAP_BSFATEOM)) {
      struct mtget mt_stat;
      /* Backup over EOF */
      stat = bsf_dev(dev, 1);
      /* If BSF worked and fileno is known (not -1), set file */
      if (dev_cap(dev, CAP_MTIOCGET) && ioctl(dev->fd, MTIOCGET, (char *)&mt_stat) == 0 && mt_stat.mt_fileno >= 0) {
         Dmsg2(100, "BSFATEOF adjust file from %d to %d\n", dev->file , mt_stat.mt_fileno);
	 dev->file = mt_stat.mt_fileno;
      } else {
	 dev->file++;		      /* wing it -- not correct on all OSes */
      }
   } else {
      update_pos_dev(dev);		     /* update position */
      stat = 1;
   }
   Dmsg1(200, "EOD dev->file=%d\n", dev->file);
   return stat;
}

/*
 * Set the position of the device -- only for files
 *   For other devices, there is no generic way to do it.
 *  Returns: true  on succes
 *	     false on error
 */
bool update_pos_dev(DEVICE *dev)
{
   off_t pos;
   bool ok = true;

   if (dev->fd < 0) {
      dev->dev_errno = EBADF;
      Mmsg0(&dev->errmsg, _("Bad device call. Archive not open\n"));
      Emsg0(M_FATAL, 0, dev->errmsg);
      return false;
   }

   /* Find out where we are */
   if (dev->is_file()) {
      dev->file = 0;
      dev->file_addr = 0;
      pos = lseek_dev(dev, (off_t)0, SEEK_CUR);
      if (pos < 0) {
	 berrno be;
	 dev->dev_errno = errno;
         Pmsg1(000, "Seek error: ERR=%s\n", be.strerror());
         Mmsg2(&dev->errmsg, _("lseek_dev error on %s. ERR=%s.\n"),
	    dev->dev_name, be.strerror());
	 ok = false;
      } else {
	 dev->file_addr = pos;
      }
   }
   return ok;
}


/*
 * Return the status of the device.  This was meant
 * to be a generic routine. Unfortunately, it doesn't
 * seem possible (at least I do not know how to do it
 * currently), which means that for the moment, this
 * routine has very little value.
 *
 *   Returns: status
 */
uint32_t status_dev(DEVICE *dev)
{
   struct mtget mt_stat;
   uint32_t stat = 0;

   if (dev->state & (ST_EOT | ST_WEOT)) {
      stat |= BMT_EOD;
      Dmsg0(-20, " EOD");
   }
   if (dev->state & ST_EOF) {
      stat |= BMT_EOF;
      Dmsg0(-20, " EOF");
   }
   if (dev->is_tape()) {
      stat |= BMT_TAPE;
      Dmsg0(-20," Bacula status:");
      Dmsg2(-20," file=%d block=%d\n", dev->file, dev->block_num);
      if (ioctl(dev->fd, MTIOCGET, (char *)&mt_stat) < 0) {
	 berrno be;
	 dev->dev_errno = errno;
         Mmsg2(&dev->errmsg, _("ioctl MTIOCGET error on %s. ERR=%s.\n"),
	    dev->dev_name, be.strerror());
	 return 0;
      }
      Dmsg0(-20, " Device status:");

#if defined(HAVE_LINUX_OS)
      if (GMT_EOF(mt_stat.mt_gstat)) {
	 stat |= BMT_EOF;
         Dmsg0(-20, " EOF");
      }
      if (GMT_BOT(mt_stat.mt_gstat)) {
	 stat |= BMT_BOT;
         Dmsg0(-20, " BOT");
      }
      if (GMT_EOT(mt_stat.mt_gstat)) {
	 stat |= BMT_EOT;
         Dmsg0(-20, " EOT");
      }
      if (GMT_SM(mt_stat.mt_gstat)) {
	 stat |= BMT_SM;
         Dmsg0(-20, " SM");
      }
      if (GMT_EOD(mt_stat.mt_gstat)) {
	 stat |= BMT_EOD;
         Dmsg0(-20, " EOD");
      }
      if (GMT_WR_PROT(mt_stat.mt_gstat)) {
	 stat |= BMT_WR_PROT;
         Dmsg0(-20, " WR_PROT");
      }
      if (GMT_ONLINE(mt_stat.mt_gstat)) {
	 stat |= BMT_ONLINE;
         Dmsg0(-20, " ONLINE");
      }
      if (GMT_DR_OPEN(mt_stat.mt_gstat)) {
	 stat |= BMT_DR_OPEN;
         Dmsg0(-20, " DR_OPEN");
      }
      if (GMT_IM_REP_EN(mt_stat.mt_gstat)) {
	 stat |= BMT_IM_REP_EN;
         Dmsg0(-20, " IM_REP_EN");
      }
#endif /* !SunOS && !OSF */
      if (dev_cap(dev, CAP_MTIOCGET)) {
         Dmsg2(-20, " file=%d block=%d\n", mt_stat.mt_fileno, mt_stat.mt_blkno);
      } else {
         Dmsg2(-20, " file=%d block=%d\n", -1, -1);
      }
   } else {
      stat |= BMT_ONLINE | BMT_BOT;
   }
   return stat;
}


/*
 * Load medium in device
 *  Returns: true  on success
 *	     false on failure
 */
bool load_dev(DEVICE *dev)
{
#ifdef MTLOAD
   struct mtop mt_com;
#endif

   if (dev->fd < 0) {
      dev->dev_errno = EBADF;
      Mmsg0(&dev->errmsg, _("Bad call to load_dev. Archive not open\n"));
      Emsg0(M_FATAL, 0, dev->errmsg);
      return false;
   }
   if (!(dev->is_tape())) {
      return true;
   }
#ifndef MTLOAD
   Dmsg0(200, "stored: MTLOAD command not available\n");
   berrno be;
   dev->dev_errno = ENOTTY;	      /* function not available */
   Mmsg2(&dev->errmsg, _("ioctl MTLOAD error on %s. ERR=%s.\n"),
	 dev->dev_name, be.strerror());
   return false;
#else

   dev->block_num = dev->file = 0;
   dev->file_size = 0;
   dev->file_addr = 0;
   mt_com.mt_op = MTLOAD;
   mt_com.mt_count = 1;
   if (ioctl(dev->fd, MTIOCTOP, (char *)&mt_com) < 0) {
      berrno be;
      dev->dev_errno = errno;
      Mmsg2(&dev->errmsg, _("ioctl MTLOAD error on %s. ERR=%s.\n"),
	 dev->dev_name, be.strerror());
      return false;
   }
   return true;
#endif
}

/*
 * Rewind device and put it offline
 *  Returns: true  on success
 *	     false on failure
 */
bool offline_dev(DEVICE *dev)
{
   struct mtop mt_com;

   if (dev->fd < 0) {
      dev->dev_errno = EBADF;
      Mmsg0(&dev->errmsg, _("Bad call to offline_dev. Archive not open\n"));
      Emsg0(M_FATAL, 0, dev->errmsg);
      return false;
   }
   if (!(dev->is_tape())) {
      return true;
   }

   dev->state &= ~(ST_APPEND|ST_READ|ST_EOT|ST_EOF|ST_WEOT);  /* remove EOF/EOT flags */
   dev->block_num = dev->file = 0;
   dev->file_size = 0;
   dev->file_addr = 0;
   dev->part = 0;
#ifdef MTUNLOCK
   mt_com.mt_op = MTUNLOCK;
   mt_com.mt_count = 1;
   ioctl(dev->fd, MTIOCTOP, (char *)&mt_com);
#endif
   mt_com.mt_op = MTOFFL;
   mt_com.mt_count = 1;
   if (ioctl(dev->fd, MTIOCTOP, (char *)&mt_com) < 0) {
      berrno be;
      dev->dev_errno = errno;
      Mmsg2(&dev->errmsg, _("ioctl MTOFFL error on %s. ERR=%s.\n"),
	 dev->dev_name, be.strerror());
      return false;
   }
   Dmsg1(100, "Offlined device %s\n", dev->dev_name);
   return true;
}

bool offline_or_rewind_dev(DEVICE *dev)
{
   if (dev->fd < 0) {
      return false;
   }
   if (dev_cap(dev, CAP_OFFLINEUNMOUNT)) {
      return offline_dev(dev);
   } else {
   /*
    * Note, this rewind probably should not be here (it wasn't
    *  in prior versions of Bacula), but on FreeBSD, this is
    *  needed in the case the tape was "frozen" due to an error
    *  such as backspacing after writing and EOF. If it is not
    *  done, all future references to the drive get and I/O error.
    */
      clrerror_dev(dev, MTREW);
      return rewind_dev(dev);
   }
}

/*
 * Foward space a file
 *   Returns: true  on success
 *	      false on failure
 */
bool
fsf_dev(DEVICE *dev, int num)
{
   struct mtget mt_stat;
   struct mtop mt_com;
   int stat = 0;

   if (dev->fd < 0) {
      dev->dev_errno = EBADF;
      Mmsg0(&dev->errmsg, _("Bad call to fsf_dev. Archive not open\n"));
      Emsg0(M_FATAL, 0, dev->errmsg);
      return false;
   }

   if (!dev->is_tape()) {
      return true;
   }
   if (dev->state & ST_EOT) {
      dev->dev_errno = 0;
      Mmsg1(dev->errmsg, _("Device %s at End of Tape.\n"), dev->dev_name);
      return false;
   }
   if (dev->state & ST_EOF) {
      Dmsg0(200, "ST_EOF set on entry to FSF\n");
   }

   Dmsg0(100, "fsf_dev\n");
   dev->block_num = 0;
   /*
    * If Fast forward space file is set, then we
    *  use MTFSF to forward space and MTIOCGET
    *  to get the file position. We assume that
    *  the SCSI driver will ensure that we do not
    *  forward space past the end of the medium.
    */
   if (dev_cap(dev, CAP_FSF) && dev_cap(dev, CAP_MTIOCGET) && dev_cap(dev, CAP_FASTFSF)) {
      mt_com.mt_op = MTFSF;
      mt_com.mt_count = num;
      stat = ioctl(dev->fd, MTIOCTOP, (char *)&mt_com);
      if (stat < 0 || ioctl(dev->fd, MTIOCGET, (char *)&mt_stat) < 0) {
	 berrno be;
	 dev->state |= ST_EOT;
         Dmsg0(200, "Set ST_EOT\n");
	 clrerror_dev(dev, MTFSF);
         Mmsg2(dev->errmsg, _("ioctl MTFSF error on %s. ERR=%s.\n"),
	    dev->dev_name, be.strerror());
         Dmsg1(200, "%s", dev->errmsg);
	 return false;
      }
      Dmsg2(200, "fsf file=%d block=%d\n", mt_stat.mt_fileno, mt_stat.mt_blkno);
      dev->file = mt_stat.mt_fileno;
      dev->state |= ST_EOF;	/* just read EOF */
      dev->file_addr = 0;
      dev->file_size = 0;
      return true;

   /*
    * Here if CAP_FSF is set, and virtually all drives
    *  these days support it, we read a record, then forward
    *  space one file. Using this procedure, which is slow,
    *  is the only way we can be sure that we don't read
    *  two consecutive EOF marks, which means End of Data.
    */
   } else if (dev_cap(dev, CAP_FSF)) {
      POOLMEM *rbuf;
      int rbuf_len;
      Dmsg0(200, "FSF has cap_fsf\n");
      if (dev->max_block_size == 0) {
	 rbuf_len = DEFAULT_BLOCK_SIZE;
      } else {
	 rbuf_len = dev->max_block_size;
      }
      rbuf = get_memory(rbuf_len);
      mt_com.mt_op = MTFSF;
      mt_com.mt_count = 1;
      while (num-- && !(dev->state & ST_EOT)) {
         Dmsg0(100, "Doing read before fsf\n");
	 if ((stat = read(dev->fd, (char *)rbuf, rbuf_len)) < 0) {
	    if (errno == ENOMEM) {     /* tape record exceeds buf len */
	       stat = rbuf_len;        /* This is OK */
	    } else {
	       berrno be;
	       dev->state |= ST_EOT;
	       clrerror_dev(dev, -1);
               Dmsg2(100, "Set ST_EOT read errno=%d. ERR=%s\n", dev->dev_errno,
		  be.strerror());
               Mmsg2(dev->errmsg, _("read error on %s. ERR=%s.\n"),
		  dev->dev_name, be.strerror());
               Dmsg1(100, "%s", dev->errmsg);
	       break;
	    }
	 }
	 if (stat == 0) {		 /* EOF */
	    update_pos_dev(dev);
            Dmsg1(100, "End of File mark from read. File=%d\n", dev->file+1);
	    /* Two reads of zero means end of tape */
	    if (dev->state & ST_EOF) {
	       dev->state |= ST_EOT;
               Dmsg0(100, "Set ST_EOT\n");
	       break;
	    } else {
	       dev->state |= ST_EOF;
	       dev->file++;
	       dev->file_addr = 0;
	       dev->file_size = 0;
	       continue;
	    }
	 } else {			 /* Got data */
	    dev->state &= ~(ST_EOF|ST_EOT);
	 }

         Dmsg0(100, "Doing MTFSF\n");
	 stat = ioctl(dev->fd, MTIOCTOP, (char *)&mt_com);
	 if (stat < 0) {		 /* error => EOT */
	    berrno be;
	    dev->state |= ST_EOT;
            Dmsg0(100, "Set ST_EOT\n");
	    clrerror_dev(dev, MTFSF);
            Mmsg2(&dev->errmsg, _("ioctl MTFSF error on %s. ERR=%s.\n"),
	       dev->dev_name, be.strerror());
            Dmsg0(100, "Got < 0 for MTFSF\n");
            Dmsg1(100, "%s", dev->errmsg);
	 } else {
	    dev->state |= ST_EOF;     /* just read EOF */
	    dev->file++;
	    dev->file_addr = 0;
	    dev->file_size = 0;
	 }
      }
      free_memory(rbuf);

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
         Mmsg1(dev->errmsg, _("Device %s at End of Tape.\n"), dev->dev_name);
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
   return stat == 0;
}

/*
 * Backward space a file
 *  Returns: false on failure
 *	     true  on success
 */
bool
bsf_dev(DEVICE *dev, int num)
{
   struct mtop mt_com;
   int stat;

   if (dev->fd < 0) {
      dev->dev_errno = EBADF;
      Mmsg0(dev->errmsg, _("Bad call to bsf_dev. Archive device not open\n"));
      Emsg0(M_FATAL, 0, dev->errmsg);
      return false;
   }

   if (!dev->is_tape()) {
      Mmsg1(dev->errmsg, _("Device %s cannot BSF because it is not a tape.\n"),
	 dev->dev_name);
      return false;
   }
   Dmsg0(29, "bsf_dev\n");
   dev->state &= ~(ST_EOT|ST_EOF);
   dev->file -= num;
   dev->file_addr = 0;
   dev->file_size = 0;
   mt_com.mt_op = MTBSF;
   mt_com.mt_count = num;
   stat = ioctl(dev->fd, MTIOCTOP, (char *)&mt_com);
   if (stat < 0) {
      berrno be;
      clrerror_dev(dev, MTBSF);
      Mmsg2(dev->errmsg, _("ioctl MTBSF error on %s. ERR=%s.\n"),
	 dev->dev_name, be.strerror());
   }
   update_pos_dev(dev);
   return stat == 0;
}


/*
 * Foward space a record
 *  Returns: false on failure
 *	     true  on success
 */
bool
fsr_dev(DEVICE *dev, int num)
{
   struct mtop mt_com;
   int stat;

   if (dev->fd < 0) {
      dev->dev_errno = EBADF;
      Mmsg0(dev->errmsg, _("Bad call to fsr_dev. Archive not open\n"));
      Emsg0(M_FATAL, 0, dev->errmsg);
      return false;
   }

   if (!dev->is_tape()) {
      return false;
   }
   if (!dev_cap(dev, CAP_FSR)) {
      Mmsg1(dev->errmsg, _("ioctl MTFSR not permitted on %s.\n"), dev->dev_name);
      return false;
   }

   Dmsg1(29, "fsr_dev %d\n", num);
   mt_com.mt_op = MTFSR;
   mt_com.mt_count = num;
   stat = ioctl(dev->fd, MTIOCTOP, (char *)&mt_com);
   if (stat == 0) {
      dev->state &= ~ST_EOF;
      dev->block_num += num;
   } else {
      berrno be;
      struct mtget mt_stat;
      clrerror_dev(dev, MTFSR);
      Dmsg1(100, "FSF fail: ERR=%s\n", be.strerror());
      if (dev_cap(dev, CAP_MTIOCGET) && ioctl(dev->fd, MTIOCGET, (char *)&mt_stat) == 0 && mt_stat.mt_fileno >= 0) {
         Dmsg4(100, "Adjust from %d:%d to %d:%d\n", dev->file,
	    dev->block_num, mt_stat.mt_fileno, mt_stat.mt_blkno);
	 dev->file = mt_stat.mt_fileno;
	 dev->block_num = mt_stat.mt_blkno;
      } else {
	 if (dev->state & ST_EOF) {
	    dev->state |= ST_EOT;
	 } else {
	    dev->state |= ST_EOF;	    /* assume EOF */
	    dev->file++;
	    dev->block_num = 0;
	    dev->file_addr = 0;
	    dev->file_size = 0;
	 }
      }
      Mmsg2(dev->errmsg, _("ioctl MTFSR error on %s. ERR=%s.\n"),
	 dev->dev_name, be.strerror());
   }
   update_pos_dev(dev);
   return stat == 0;
}

/*
 * Backward space a record
 *   Returns:  false on failure
 *	       true  on success
 */
bool
bsr_dev(DEVICE *dev, int num)
{
   struct mtop mt_com;
   int stat;

   if (dev->fd < 0) {
      dev->dev_errno = EBADF;
      Mmsg0(dev->errmsg, _("Bad call to bsr_dev. Archive not open\n"));
      Emsg0(M_FATAL, 0, dev->errmsg);
      return false;
   }

   if (!dev->is_tape()) {
      return false;
   }

   if (!dev_cap(dev, CAP_BSR)) {
      Mmsg1(dev->errmsg, _("ioctl MTBSR not permitted on %s.\n"), dev->dev_name);
      return false;
   }

   Dmsg0(29, "bsr_dev\n");
   dev->block_num -= num;
   dev->state &= ~(ST_EOF|ST_EOT|ST_EOF);
   mt_com.mt_op = MTBSR;
   mt_com.mt_count = num;
   stat = ioctl(dev->fd, MTIOCTOP, (char *)&mt_com);
   if (stat < 0) {
      berrno be;
      clrerror_dev(dev, MTBSR);
      Mmsg2(dev->errmsg, _("ioctl MTBSR error on %s. ERR=%s.\n"),
	 dev->dev_name, be.strerror());
   }
   update_pos_dev(dev);
   return stat == 0;
}

/*
 * Reposition the device to file, block
 * Returns: false on failure
 *	    true  on success
 */
bool
reposition_dev(DEVICE *dev, uint32_t file, uint32_t block)
{
   if (dev->fd < 0) {
      dev->dev_errno = EBADF;
      Mmsg0(dev->errmsg, _("Bad call to reposition_dev. Archive not open\n"));
      Emsg0(M_FATAL, 0, dev->errmsg);
      return false;
   }

   if (!dev->is_tape()) {
      off_t pos = (((off_t)file)<<32) + block;
      Dmsg1(100, "===== lseek_dev to %d\n", (int)pos);
      if (lseek_dev(dev, pos, SEEK_SET) == (off_t)-1) {
	 berrno be;
	 dev->dev_errno = errno;
         Mmsg2(dev->errmsg, _("lseek_dev error on %s. ERR=%s.\n"),
	    dev->dev_name, be.strerror());
	 return false;
      }
      dev->file = file;
      dev->block_num = block;
      dev->file_addr = pos;
      return true;
   }
   Dmsg4(100, "reposition_dev from %u:%u to %u:%u\n",
      dev->file, dev->block_num, file, block);
   if (file < dev->file) {
      Dmsg0(100, "Rewind_dev\n");
      if (!rewind_dev(dev)) {
	 return false;
      }
   }
   if (file > dev->file) {
      Dmsg1(100, "fsf %d\n", file-dev->file);
      if (!fsf_dev(dev, file-dev->file)) {
         Dmsg1(100, "fsf failed! ERR=%s\n", strerror_dev(dev));
	 return false;
      }
      Dmsg2(100, "wanted_file=%d at_file=%d\n", file, dev->file);
   }
   if (block < dev->block_num) {
      Dmsg2(100, "wanted_blk=%d at_blk=%d\n", block, dev->block_num);
      Dmsg0(100, "bsf_dev 1\n");
      bsf_dev(dev, 1);
      Dmsg0(100, "fsf_dev 1\n");
      fsf_dev(dev, 1);
      Dmsg2(100, "wanted_blk=%d at_blk=%d\n", block, dev->block_num);
   }
   if (dev_cap(dev, CAP_POSITIONBLOCKS) && block > dev->block_num) {
      /* Ignore errors as Bacula can read to the correct block */
      Dmsg1(100, "fsr %d\n", block-dev->block_num);
      return fsr_dev(dev, block-dev->block_num);
   }
   return true;
}



/*
 * Write an end of file on the device
 *   Returns: 0 on success
 *	      non-zero on failure
 */
int
weof_dev(DEVICE *dev, int num)
{
   struct mtop mt_com;
   int stat;
   Dmsg0(29, "weof_dev\n");
   
   if (dev->fd < 0) {
      dev->dev_errno = EBADF;
      Mmsg0(dev->errmsg, _("Bad call to weof_dev. Archive drive not open\n"));
      Emsg0(M_FATAL, 0, dev->errmsg);
      return -1;
   }
   dev->file_size = 0;

   if (!dev->is_tape()) {
      return 0;
   }
   dev->state &= ~(ST_EOT | ST_EOF);  /* remove EOF/EOT flags */
   mt_com.mt_op = MTWEOF;
   mt_com.mt_count = num;
   stat = ioctl(dev->fd, MTIOCTOP, (char *)&mt_com);
   if (stat == 0) {
      dev->block_num = 0;
      dev->file += num;
      dev->file_addr = 0;
   } else {
      berrno be;
      clrerror_dev(dev, MTWEOF);
      if (stat == -1) {
         Mmsg2(dev->errmsg, _("ioctl MTWEOF error on %s. ERR=%s.\n"),
	    dev->dev_name, be.strerror());
       }
   }
   return stat;
}

/*
 * Return string message with last error in English
 *  Be careful not to call this routine from within dev.c
 *  while editing an Mmsg() or you will end up in a recursive
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
   const char *msg = NULL;
   struct mtget mt_stat;
   char buf[100];

   dev->dev_errno = errno;	   /* save errno */
   if (errno == EIO) {
      dev->VolCatInfo.VolCatErrors++;
   }

   if (!dev->is_tape()) {
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
#ifdef MTEOM
      case MTEOM:
         msg = "WTEOM";
	 dev->capabilities &= ~CAP_EOM; /* turn off feature */
	 break;
#endif
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
      case MTREW:
         msg = "MTREW";
	 break;
#ifdef MTSETBLK
      case MTSETBLK:
         msg = "MTSETBLK";
	 break;
#endif
#ifdef MTSETBSIZ 
      case MTSETBSIZ:
         msg = "MTSETBSIZ";
	 break;
#endif
#ifdef MTSRSZ
      case MTSRSZ:
         msg = "MTSRSZ";
	 break;
#endif
      default:
         bsnprintf(buf, sizeof(buf), "unknown func code %d", func);
	 msg = buf;
	 break;
      }
      if (msg != NULL) {
	 dev->dev_errno = ENOSYS;
         Mmsg1(dev->errmsg, _("I/O function \"%s\" not supported on this device.\n"), msg);
	 Emsg0(M_ERROR, 0, dev->errmsg);
      }
   }
   /* On some systems such as NetBSD, this clears all errors */
   ioctl(dev->fd, MTIOCGET, (char *)&mt_stat);

/* Found on Linux */
#ifdef MTIOCLRERR
{
   struct mtop mt_com;
   mt_com.mt_op = MTIOCLRERR;
   mt_com.mt_count = 1;
   /* Clear any error condition on the tape */
   ioctl(dev->fd, MTIOCTOP, (char *)&mt_com);
   Dmsg0(200, "Did MTIOCLRERR\n");
}
#endif

/* Typically on FreeBSD */
#ifdef MTIOCERRSTAT
{
   /* Read and clear SCSI error status */
   union mterrstat mt_errstat;
   Dmsg2(200, "Doing MTIOCERRSTAT errno=%d ERR=%s\n", dev->dev_errno,
      strerror(dev->dev_errno));
   ioctl(dev->fd, MTIOCERRSTAT, (char *)&mt_errstat);
}
#endif

/* Clear Subsystem Exception OSF1 */
#ifdef MTCSE
{
   struct mtop mt_com;
   mt_com.mt_op = MTCSE;
   mt_com.mt_count = 1;
   /* Clear any error condition on the tape */
   ioctl(dev->fd, MTIOCTOP, (char *)&mt_com);
   Dmsg0(200, "Did MTCSE\n");
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

   Dmsg1(29, "really close_dev %s\n", dev->dev_name);
   if (dev->fd >= 0) {
      close(dev->fd);
   }
   if (dev_cap(dev, CAP_REQMOUNT)) {
      if (unmount_dev(dev, 1) < 0) {
         Dmsg1(0, "Cannot unmount device %s.\n", dev->dev_name);
      }
   }
   
   /* Remove the last part file if it is empty */
   if (dev->can_append() && (dev->num_parts > 0)) {
      struct stat statp;
      POOL_MEM archive_name(PM_FNAME);
      dev->part = dev->num_parts;
      get_filename(dev, dev->VolCatInfo.VolCatName, archive_name);
      /* Check that the part file is empty */
      if ((stat(archive_name.c_str(), &statp) == 0) && (statp.st_size == 0)) {
	 unlink(archive_name.c_str());
      }
   }
   
   /* Clean up device packet so it can be reused */
   dev->fd = -1;
   dev->state &= ~(ST_OPENED|ST_LABEL|ST_READ|ST_APPEND|ST_EOT|ST_WEOT|ST_EOF);
   dev->file = dev->block_num = 0;
   dev->file_size = 0;
   dev->file_addr = 0;
   dev->part = 0;
   dev->part_size = 0;
   dev->part_start = 0;
   dev->EndFile = dev->EndBlock = 0;
   memset(&dev->VolCatInfo, 0, sizeof(dev->VolCatInfo));
   memset(&dev->VolHdr, 0, sizeof(dev->VolHdr));
   if (dev->tid) {
      stop_thread_timer(dev->tid);
      dev->tid = 0;
   }
   dev->use_count = 0;
}

/*
 * Close the device
 */
void
close_dev(DEVICE *dev)
{
   if (!dev) {
      Mmsg0(&dev->errmsg, _("Bad call to close_dev. Archive not open\n"));
      Emsg0(M_FATAL, 0, dev->errmsg);
      return;
   }
   /*if (dev->fd >= 0 && dev->use_count == 1) {*/
   /* No need to check if dev->fd >= 0: it is checked again
    * in do_close, and do_close MUST be called for volumes
    * splitted in parts, even if dev->fd == -1. */
   if (dev->use_count == 1) {
      do_close(dev);
   } else if (dev->use_count > 0) {
      dev->use_count--;
   }

#ifdef FULL_DEBUG
   ASSERT(dev->use_count >= 0);
#endif
}

/*
 * Used when unmounting the device, ignore use_count
 */
void force_close_dev(DEVICE *dev)
{
   if (!dev) {
      Mmsg0(&dev->errmsg, _("Bad call to force_close_dev. Archive not open\n"));
      Emsg0(M_FATAL, 0, dev->errmsg);
      return;
   }
   Dmsg1(29, "Force close_dev %s\n", dev->dev_name);
   do_close(dev);

#ifdef FULL_DEBUG
   ASSERT(dev->use_count >= 0);
#endif
}

bool truncate_dev(DEVICE *dev)
{
   Dmsg1(100, "truncate_dev %s\n", dev->dev_name);
   if (dev->is_tape()) {
      return true;                    /* we don't really truncate tapes */
      /* maybe we should rewind and write and eof ???? */
   }
   
   /* If there is more than one part, open the first one, and then truncate it. */
   if (dev->num_parts > 0) {
      dev->num_parts = 0;
      dev->VolCatInfo.VolCatParts = 0;
      if (open_first_part(dev) < 0) {
	 berrno be;
         Mmsg1(&dev->errmsg, "Unable to truncate device, because I'm unable to open the first part. ERR=%s\n", be.strerror());
      }
   }
   
   if (ftruncate(dev->fd, 0) != 0) {
      berrno be;
      Mmsg1(&dev->errmsg, _("Unable to truncate device. ERR=%s\n"), be.strerror());
      return false;
   }
   return true;
}

bool
dev_is_tape(DEVICE *dev)
{
   return dev->is_tape() ? true : false;
}


/*
 * return 1 if the device is read for write, and 0 otherwise
 *   This is meant for checking at the end of a job to see
 *   if we still have a tape (perhaps not if at end of tape
 *   and the job is canceled).
 */
bool
dev_can_write(DEVICE *dev)
{
   if (dev->is_open() &&  dev->can_append() &&
       dev->is_labeled()  && !(dev->state & ST_WEOT)) {
      return true;
   } else {
      return false;
   }
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
      Mmsg0(&dev->errmsg, _("Bad call to term_dev. Archive not open\n"));
      Emsg0(M_FATAL, 0, dev->errmsg);
      return;
   }
   do_close(dev);
   Dmsg0(29, "term_dev\n");
   if (dev->dev_name) {
      free_memory(dev->dev_name);
      dev->dev_name = NULL;
   }
   if (dev->errmsg) {
      free_pool_memory(dev->errmsg);
      dev->errmsg = NULL;
   }
   pthread_mutex_destroy(&dev->mutex);
   pthread_cond_destroy(&dev->wait);
   pthread_cond_destroy(&dev->wait_next_vol);
   pthread_mutex_destroy(&dev->spool_mutex);
   rwl_destroy(&dev->lock);
   if (dev->attached_dcrs) {
      delete dev->attached_dcrs;
      dev->attached_dcrs = NULL;
   }
   if (dev->state & ST_MALLOC) {
      free_pool_memory((POOLMEM *)dev);
   }
}

/*
 * This routine initializes the device wait timers
 */
void init_dev_wait_timers(DEVICE *dev)
{
   /* ******FIXME******* put these on config variables */
   dev->min_wait = 60 * 60;
   dev->max_wait = 24 * 60 * 60;
   dev->max_num_wait = 9;	       /* 5 waits =~ 1 day, then 1 day at a time */
   dev->wait_sec = dev->min_wait;
   dev->rem_wait_sec = dev->wait_sec;
   dev->num_wait = 0;
   dev->poll = false;
   dev->BadVolName[0] = 0;
}

/*
 * Returns: true if time doubled
 *	    false if max time expired
 */
bool double_dev_wait_time(DEVICE *dev)
{
   dev->wait_sec *= 2;		     /* double wait time */
   if (dev->wait_sec > dev->max_wait) {   /* but not longer than maxtime */
      dev->wait_sec = dev->max_wait;
   }
   dev->num_wait++;
   dev->rem_wait_sec = dev->wait_sec;
   if (dev->num_wait >= dev->max_num_wait) {
      return false;
   }
   return true;
}

void set_os_device_parameters(DEVICE *dev)
{
#ifdef HAVE_LINUX_OS
   struct mtop mt_com;
   if (dev->min_block_size == dev->max_block_size &&
       dev->min_block_size == 0) {    /* variable block mode */
      mt_com.mt_op = MTSETBLK;
      mt_com.mt_count = 0;
      if (ioctl(dev->fd, MTIOCTOP, (char *)&mt_com) < 0) {
	 clrerror_dev(dev, MTSETBLK);
      }
      mt_com.mt_op = MTSETDRVBUFFER;
      mt_com.mt_count = MT_ST_CLEARBOOLEANS;
      if (!dev_cap(dev, CAP_TWOEOF)) {
	 mt_com.mt_count |= MT_ST_TWO_FM;
      }
      if (dev_cap(dev, CAP_EOM)) {
	 mt_com.mt_count |= MT_ST_FAST_MTEOM;
      }
      if (ioctl(dev->fd, MTIOCTOP, (char *)&mt_com) < 0) {
	 clrerror_dev(dev, MTSETBLK);
      }
   }
   return;
#endif

#ifdef HAVE_NETBSD_OS
   struct mtop mt_com;
   if (dev->min_block_size == dev->max_block_size &&
       dev->min_block_size == 0) {    /* variable block mode */
      mt_com.mt_op = MTSETBSIZ;
      mt_com.mt_count = 0;
      if (ioctl(dev->fd, MTIOCTOP, (char *)&mt_com) < 0) {
	 clrerror_dev(dev, MTSETBSIZ);
      }
      /* Get notified at logical end of tape */
      mt_com.mt_op = MTEWARN;
      mt_com.mt_count = 1;
      if (ioctl(dev->fd, MTIOCTOP, (char *)&mt_com) < 0) {
	 clrerror_dev(dev, MTEWARN);
      }
   }
   return;
#endif

#if HAVE_FREEBSD_OS || HAVE_OPENBSD_OS
   struct mtop mt_com;
   if (dev->min_block_size == dev->max_block_size &&
       dev->min_block_size == 0) {    /* variable block mode */
      mt_com.mt_op = MTSETBSIZ;
      mt_com.mt_count = 0;
      if (ioctl(dev->fd, MTIOCTOP, (char *)&mt_com) < 0) {
	 clrerror_dev(dev, MTSETBSIZ);
      }
   }
   return;
#endif

#ifdef HAVE_SUN_OS
   struct mtop mt_com;
   if (dev->min_block_size == dev->max_block_size &&
       dev->min_block_size == 0) {    /* variable block mode */
      mt_com.mt_op = MTSRSZ;
      mt_com.mt_count = 0;
      if (ioctl(dev->fd, MTIOCTOP, (char *)&mt_com) < 0) {
	 clrerror_dev(dev, MTSRSZ);
      }
   }
   return;
#endif
}

/*
 * Edit codes into (Un)MountCommand, Write(First)PartCommand
 *  %% = %
 *  %a = archive device name
 *  %m = mount point
 *  %v = last part name
 *
 *  omsg = edited output message
 *  imsg = input string containing edit codes (%x)
 *
 */
char *edit_device_codes_dev(DEVICE* dev, char *omsg, const char *imsg)
{
   const char *p;
   const char *str;
   char add[20];
   
   POOL_MEM archive_name(PM_FNAME);
   get_filename(dev, dev->VolCatInfo.VolCatName, archive_name);

   *omsg = 0;
   Dmsg1(800, "edit_device_codes: %s\n", imsg);
   for (p=imsg; *p; p++) {
      if (*p == '%') {
	 switch (*++p) {
            case '%':
               str = "%";
	       break;
            case 'n':
               bsnprintf(add, sizeof(add), "%d", dev->part);
	       str = add;
	       break;
            case 'a':
	       str = dev->dev_name;
	       break;
            case 'm':
	       str = dev->device->mount_point;
	       break;
            case 'v':
	       str = archive_name.c_str();
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
      Dmsg1(900, "add_str %s\n", str);
      pm_strcat(&omsg, (char *)str);
      Dmsg1(800, "omsg=%s\n", omsg);
   }
   return omsg;
}
