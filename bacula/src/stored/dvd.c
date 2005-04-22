/*
 *
 *   dvd.c  -- Routines specific to DVD devices (and
 *             possibly other removable hard media). 
 *
 *    Nicolas Boichat, MMV
 *
 *   Version $Id$
 */
/*
   Copyright (C) 2005 Kern Sibbald

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

int mount_dev(DEVICE *dev, int timeout);
int unmount_dev(DEVICE *dev, int timeout);
void update_free_space_dev(DEVICE *dev);
void get_filename(DEVICE *dev, char *VolName, POOL_MEM& archive_name);

/* Forward referenced functions */
static char *edit_device_codes_dev(DEVICE *dev, char *omsg, const char *imsg);
static int do_mount_dev(DEVICE* dev, int mount, int dotimeout);
static int dvd_write_part(DEVICE *dev);


/* 
 * Write the current volume/part filename to archive_name.
 */
void get_filename(DEVICE *dev, char *VolName, POOL_MEM& archive_name) 
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

/* Mount the device.
 * If timeout, wait until the mount command returns 0.
 * If !timeout, try to mount the device only once.
 */
int mount_dev(DEVICE* dev, int timeout) 
{
   if (dev->state & ST_MOUNTED) {
      Dmsg0(100, "mount_dev: Device already mounted\n");
      return 0;
   } else if (dev_cap(dev, CAP_REQMOUNT)) {
      return do_mount_dev(dev, 1, timeout);
   }       
   return 0;
}

/* Unmount the device
 * If timeout, wait until the unmount command returns 0.
 * If !timeout, try to unmount the device only once.
 */
int unmount_dev(DEVICE *dev, int timeout) 
{
   if (dev->state & ST_MOUNTED) {
      return do_mount_dev(dev, 0, timeout);
   }
   Dmsg0(100, "mount_dev: Device already unmounted\n");
   return 0;
}

/* (Un)mount the device */
static int do_mount_dev(DEVICE* dev, int mount, int dotimeout) {
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
   } else {
     dev->state &= ~ST_MOUNTED;
   }
   free_pool_memory(results);
   
   Dmsg1(29, "do_mount_dev: end_state=%d\n", dev->state & ST_MOUNTED);
   return 0;
}

/* Only for devices that require a mount.
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
         Dmsg1(100, "open_guess_name_dev: device cannot be mounted, and is not writable, returning -1. dev=%s\n", dev->dev_name);
         return -1;
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


/* Update the free space on the device */
void update_free_space_dev(DEVICE* dev) 
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

static int dvd_write_part(DEVICE *dev) 
{
   Dmsg1(29, "dvd_write_part: device is %s\n", dev->dev_name);
   
   if (unmount_dev(dev, 1) < 0) {
      Dmsg0(29, "dvd_write_part: unable to unmount the device\n");
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
   
   Dmsg2(29, "dvd_write_part: cmd=%s timeout=%d\n", ocmd.c_str(), timeout);
      
   status = run_program_full_output(ocmd.c_str(), timeout, results);
   if (status != 0) {
      Mmsg1(dev->errmsg, "Error while writing current part to the DVD: %s", results);
      dev->dev_errno = EIO;
      free_pool_memory(results);
      return -1;
   }
   else {
      Dmsg1(29, "dvd_write_part: command output=%s\n", results);
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
      if (dvd_write_part(dev) < 0) {
         return -1;
      }
   }
     
   dev->part_start += dev->part_size;
   dev->part++;
   
   if ((dev->num_parts < dev->part) && dev->can_append()) {
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
   } 
   dev->state = state;
   return dev->fd;
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
   
   if (open_dev(dev, dev->VolCatInfo.VolCatName, dev->openmode) < 0) {
      return -1;
   }
   dev->state = state;
   return dev->fd;
}


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

bool dvd_close_job(DCR *dcr)
{
   DEVICE *dev = dcr->dev;
   JCR *jcr = dcr->jcr;
   bool ok = true;

   /* If the device is a dvd and WritePartAfterJob
    * is set to yes, open the next part, so, in case of a device
    * that requires mount, it will be written to the device.
    */
   if (dev->is_dvd() && jcr->write_part_after_job && (dev->part_size > 0)) {
      Dmsg0(100, "Writing last part because write_part_after_job is set.\n");
      if (dev->part < dev->num_parts) {
         Jmsg3(jcr, M_FATAL, 0, _("Error while writing, current part number is less than the total number of parts (%d/%d, device=%s)\n"),
               dev->part, dev->num_parts, dev->print_name());
         dev->dev_errno = EIO;
         ok = false;
      }
      
      if (ok && (open_next_part(dev) < 0)) {
         Jmsg2(jcr, M_FATAL, 0, _("Unable to open device next part %s: ERR=%s\n"),
               dev->print_name(), strerror_dev(dev));
         dev->dev_errno = EIO;
         ok = false;
      }
      
      dev->VolCatInfo.VolCatParts = dev->num_parts;
   }
   return ok;
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
static char *edit_device_codes_dev(DEVICE* dev, char *omsg, const char *imsg)
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
