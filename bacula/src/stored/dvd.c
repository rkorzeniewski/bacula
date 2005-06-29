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
   modify it under the terms of the GNU General Public License
   version 2 as ammended with additional clauses defined in the
   file LICENSE in the main source directory.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the 
   the file LICENSE for additional details.

 */

#include "bacula.h"
#include "stored.h"

/* Forward referenced functions */
static char *edit_device_codes_dev(DEVICE *dev, char *omsg, const char *imsg);
static bool do_mount_dev(DEVICE* dev, int mount, int dotimeout);
static bool dvd_write_part(DEVICE *dev);
static void add_file_and_part_name(DEVICE *dev, POOL_MEM &archive_name);

/* 
 * Write the current volume/part filename to archive_name.
 */
void make_mounted_dvd_filename(DEVICE *dev, POOL_MEM &archive_name) 
{
   pm_strcpy(archive_name, dev->device->mount_point);
   add_file_and_part_name(dev, archive_name);
}

void make_spooled_dvd_filename(DEVICE *dev, POOL_MEM &archive_name)
{
   /* Use the working directory if spool directory is not defined */
   if (dev->device->spool_directory) {
      pm_strcpy(archive_name, dev->device->spool_directory);
   } else {
      pm_strcpy(archive_name, working_directory);
   }
   add_file_and_part_name(dev, archive_name);
}      

static void add_file_and_part_name(DEVICE *dev, POOL_MEM &archive_name)
{
   char partnumber[20];
   if (archive_name.c_str()[strlen(archive_name.c_str())-1] != '/') {
      pm_strcat(archive_name, "/");
   }

   pm_strcat(archive_name, dev->VolCatInfo.VolCatName);
   /* if part != 0, append .# to the filename (where # is the part number) */
   if (dev->part != 0) {
      pm_strcat(archive_name, ".");
      bsnprintf(partnumber, sizeof(partnumber), "%d", dev->part);
      pm_strcat(archive_name, partnumber);
   }
   Dmsg1(100, "Exit make_dvd_filename: arch=%s\n", archive_name.c_str());
}  

/* Mount the device.
 * If timeout, wait until the mount command returns 0.
 * If !timeout, try to mount the device only once.
 */
bool mount_dev(DEVICE* dev, int timeout) 
{
   Dmsg0(900, "Enter mount_dev\n");
   if (dev->is_mounted()) {
      return true;
   } else if (dev->requires_mount()) {
      return do_mount_dev(dev, 1, timeout);
   }       
   return true;
}

/* Unmount the device
 * If timeout, wait until the unmount command returns 0.
 * If !timeout, try to unmount the device only once.
 */
bool unmount_dev(DEVICE *dev, int timeout) 
{
   Dmsg0(900, "Enter unmount_dev\n");
   if (dev->is_mounted()) {
      return do_mount_dev(dev, 0, timeout);
   }
   return true;
}

/* (Un)mount the device */
static bool do_mount_dev(DEVICE* dev, int mount, int dotimeout) 
{
   POOL_MEM ocmd(PM_FNAME);
   POOLMEM *results;
   char *icmd;
   int status, timeout;
   
   if (mount) {
      if (dev->is_mounted()) {
         return true;
      }
      icmd = dev->device->mount_command;
   } else {
      if (!dev->is_mounted()) {
         return true;
      }
      icmd = dev->device->unmount_command;
   }
   
   edit_device_codes_dev(dev, ocmd.c_str(), icmd);
   
   Dmsg2(200, "do_mount_dev: cmd=%s mounted=%d\n", ocmd.c_str(), !!dev->is_mounted());

   if (dotimeout) {
      /* Try at most 1 time to (un)mount the device. This should perhaps be configurable. */
      timeout = 1;
   } else {
      timeout = 0;
   }
   results = get_pool_memory(PM_MESSAGE);
   results[0] = 0;
   /* If busy retry each second */
   while ((status = run_program_full_output(ocmd.c_str(), 
                       dev->max_open_wait/2, results)) != 0) {
      if (fnmatch("*is already mounted on", results, 0) == 0) {
         break;
      }
      if (timeout-- > 0) {
         /* Sometimes the device cannot be mounted because it is already mounted.
          * Try to unmount it, then remount it */
         if (mount) {
            Dmsg1(400, "Trying to unmount the device %s...\n", dev->print_name());
            do_mount_dev(dev, 0, 0);
         }
         bmicrosleep(1, 0);
         continue;
      }
      Dmsg2(40, "Device %s cannot be mounted. ERR=%s\n", dev->print_name(), results);
      Mmsg(dev->errmsg, "Device %s cannot be mounted. ERR=%s\n", 
           dev->print_name(), results);
      free_pool_memory(results);
      return false;
   }
   
   dev->set_mounted(mount);              /* set/clear mounted flag */
   free_pool_memory(results);
   return true;
}

/* Update the free space on the device */
void update_free_space_dev(DEVICE* dev) 
{
   POOL_MEM ocmd(PM_FNAME);
   POOLMEM* results;
   char* icmd;
   int timeout;
   long long int free;
   char ed1[50];
   
   icmd = dev->device->free_space_command;
   
   if (!icmd) {
      dev->free_space = 0;
      dev->free_space_errno = 0;
      dev->clear_media();
      Dmsg2(29, "update_free_space_dev: free_space=%d, free_space_errno=%d (!icmd)\n", dev->free_space, dev->free_space_errno);
      return;
   }
   
   edit_device_codes_dev(dev, ocmd.c_str(), icmd);
   
   Dmsg1(29, "update_free_space_dev: cmd=%s\n", ocmd.c_str());

   results = get_pool_memory(PM_MESSAGE);
   results[0] = 0;
   
   /* Try at most 3 times to get the free space on the device. This should perhaps be configurable. */
   timeout = 3;
   
   while (1) {
      if (run_program_full_output(ocmd.c_str(), dev->max_open_wait/2, results) == 0) {
         Dmsg1(100, "Free space program run : %s\n", results);
         free = str_to_int64(results);
         if (free >= 0) {
            dev->free_space = free;
            dev->free_space_errno = 1;
            dev->set_media();
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
   Dmsg3(29, "update_free_space_dev: free_space=%s, free_space_errno=%d have_media=%d\n", 
      edit_uint64(dev->free_space, ed1), dev->free_space_errno, dev->have_media());
   return;
}

static bool dvd_write_part(DEVICE *dev) 
{
   Dmsg1(29, "dvd_write_part: device is %s\n", dev->dev_name);
   
   if (unmount_dev(dev, 1) < 0) {
      Dmsg0(29, "dvd_write_part: unable to unmount the device\n");
   }
   
   POOL_MEM ocmd(PM_FNAME);
   POOLMEM *results;
   char* icmd;
   int status;
   int timeout;
   
   results = get_pool_memory(PM_MESSAGE);
   results[0] = 0;
   icmd = dev->device->write_part_command;
   
   edit_device_codes_dev(dev, ocmd.c_str(), icmd);
      
   /* Wait at most the time a maximum size part is written in DVD 0.5x speed
    * FIXME: Minimum speed should be in device configuration 
    */
   timeout = dev->max_open_wait + (dev->max_part_size/(1350*1024/2));
   
   Dmsg2(29, "dvd_write_part: cmd=%s timeout=%d\n", ocmd.c_str(), timeout);
      
   status = run_program_full_output(ocmd.c_str(), timeout, results);
   if (status != 0) {
      Mmsg1(dev->errmsg, "Error while writing current part to the DVD: %s", 
            results);
      Dmsg1(000, "%s", dev->errmsg);
      dev->dev_errno = EIO;
      free_pool_memory(results);
      return false;
   }

   POOL_MEM archive_name(PM_FNAME);
   /* Delete spool file */
   make_spooled_dvd_filename(dev, archive_name);
   unlink(archive_name.c_str());
   free_pool_memory(results);
   return true;
}

/* Open the next part file.
 *  - Close the fd
 *  - Increment part number 
 *  - Reopen the device
 */
int open_next_part(DCR *dcr)
{
   DEVICE *dev = dcr->dev;
      
   Dmsg5(29, "Enter: open_next_part part=%d npart=%d dev=%s vol=%s mode=%d\n", 
      dev->part, dev->num_parts, dev->print_name(),
         dev->VolCatInfo.VolCatName, dev->openmode);
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
   dev->clear_opened();
   
   /*
    * If we have a part open for write, then write it to
    *  DVD before opening the next part.
    */
   if (dev->is_dvd() && (dev->part == dev->num_parts) && dev->can_append()) {
      if (!dvd_write_part(dev)) {
         return -1;
      }
   }
     
   dev->part_start += dev->part_size;
   dev->part++;
   
   Dmsg2(29, "part=%d num_parts=%d\n", dev->part, dev->num_parts);
   if ((dev->num_parts < dev->part) && dev->can_append()) {
      POOL_MEM archive_name(PM_FNAME);
      struct stat buf;

      /* 
       * First check what is on DVD.  If out part is there, we
       *   are in trouble, so bail out.
       */
      make_mounted_dvd_filename(dev, archive_name);   /* makes dvd name */
      if (stat(archive_name.c_str(), &buf) == 0) {
         /* bad news bail out */
         Mmsg1(&dev->errmsg, _("Next Volume part already exists on DVD. Cannot continue: %s\n"),
            archive_name.c_str());
         return -1;
      }

      dev->num_parts = dev->part;
      make_spooled_dvd_filename(dev, archive_name);   /* makes spool name */
      
      /* Check if the next part exists in spool directory . */
      if ((stat(archive_name.c_str(), &buf) == 0) || (errno != ENOENT)) {
         Dmsg1(29, "open_next_part %s is in the way, moving it away...\n", archive_name.c_str());
         /* Then try to unlink it */
         if (unlink(archive_name.c_str()) < 0) {
            berrno be;
            dev->dev_errno = errno;
            Mmsg2(dev->errmsg, _("open_next_part can't unlink existing part %s, ERR=%s\n"), 
                   archive_name.c_str(), be.strerror());
            return -1;
         }
      }
   }
   
   Dmsg2(50, "Call dev->open(vol=%s, mode=%d", dev->VolCatInfo.VolCatName, 
         dev->openmode);
   /* Open next part */
   if (dev->open(dcr, dev->openmode) < 0) {
      return -1;
   } 
   dev->set_labeled();          /* all next parts are "labeled" */
   return dev->fd;
}

/* Open the first part file.
 *  - Close the fd
 *  - Reopen the device
 */
int open_first_part(DCR *dcr, int mode)
{
   DEVICE *dev = dcr->dev;
   Dmsg3(29, "Enter: open_first_part dev=%s Vol=%s mode=%d\n", dev->dev_name, 
         dev->VolCatInfo.VolCatName, dev->openmode);
   if (dev->fd >= 0) {
      close(dev->fd);
   }
   dev->fd = -1;
   dev->clear_opened();
   
   dev->part_start = 0;
   dev->part = 0;
   
   Dmsg2(50, "Call dev->open(vol=%s, mode=%d)\n", dcr->VolCatInfo.VolCatName, 
         mode);
   if (dev->open(dcr, mode) < 0) {
      Dmsg0(50, "open dev() failed\n");
      return -1;
   }
   Dmsg1(50, "Leave open_first_part state=%s\n", dev->is_open()?"open":"not open");
   return dev->fd;
}


/* Protected version of lseek, which opens the right part if necessary */
off_t lseek_dev(DEVICE *dev, off_t offset, int whence)
{
   int pos, openmode;
   DCR *dcr;
   
   if (dev->num_parts == 0) { /* If there is only one part, simply call lseek. */
      return lseek(dev->fd, offset, whence);
   }
      
   dcr = (DCR *)dev->attached_dcrs->first();  /* any dcr will do */
   switch(whence) {
   case SEEK_SET:
      Dmsg1(100, "lseek_dev SEEK_SET to %d\n", (int)offset);
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
            if (open_next_part(dcr) < 0) {
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
         if (open_first_part(dcr, dev->openmode) < 0) {
            Dmsg0(100, "lseek_dev failed while trying to open the first part\n");
            return -1;
         }
         return lseek_dev(dev, offset, SEEK_SET);
      }
      break;
   case SEEK_CUR:
      Dmsg1(100, "lseek_dev SEEK_CUR to %d\n", (int)offset);
      if ((pos = lseek(dev->fd, (off_t)0, SEEK_CUR)) < 0) {
         return pos;   
      }
      pos += dev->part_start;
      if (offset == 0) {
         return pos;
      } else { /* Not used in Bacula, but should work */
         return lseek_dev(dev, pos, SEEK_SET);
      }
      break;
   case SEEK_END:
      Dmsg1(100, "lseek_dev SEEK_END to %d\n", (int)offset);
      if (offset > 0) { /* Not used by bacula */
         Dmsg1(100, "lseek_dev SEEK_END called with an invalid offset %d\n", (int)offset);
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
         
         /* Works because num_parts > 0. */
         if (open_first_part(dcr, OPEN_READ_ONLY) < 0) {
            Dmsg0(100, "lseek_dev failed while trying to open the first part\n");
            return -1;
         }
         while (dev->part < (dev->num_parts-1)) {
            if (open_next_part(dcr) < 0) {
               Dmsg0(100, "lseek_dev failed while trying to open the next part\n");
               return -1;
            }
         }
         dev->openmode = openmode;
         if (open_next_part(dcr) < 0) {
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
      
      if (ok && (open_next_part(dcr) < 0)) {
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
            make_spooled_dvd_filename(dev, archive_name);
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
      Dmsg1(1900, "add_str %s\n", str);
      pm_strcat(&omsg, (char *)str);
      Dmsg1(1800, "omsg=%s\n", omsg);
   }
   return omsg;
}
