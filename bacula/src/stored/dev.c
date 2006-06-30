/*
 *
 *   dev.c  -- low level operations on device (storage device)
 *
 *              Kern Sibbald, MM
 *
 *     NOTE!!!! None of these routines are reentrant. You must
 *        use lock_device() and unlock_device() at a higher level,
 *        or use the xxx_device() equivalents.  By moving the
 *        thread synchronization to a higher level, we permit
 *        the higher level routines to "seize" the device and
 *        to carry out operations without worrying about who
 *        set what lock (i.e. race conditions).
 *
 *     Note, this is the device dependent code, and may have
 *           to be modified for each system, but is meant to
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
   Copyright (C) 2000-2006 Kern Sibbald

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   version 2 as amended with additional clauses defined in the
   file LICENSE in the main source directory.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the 
   the file LICENSE for additional details.

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

#ifndef O_NONBLOCK 
#define O_NONBLOCK 0
#endif

/* Forward referenced functions */
void set_os_device_parameters(DEVICE *dev);
static bool dev_get_os_pos(DEVICE *dev, struct mtget *mt_stat);
static char *mode_to_str(int mode);

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
init_dev(JCR *jcr, DEVRES *device)
{
   struct stat statp;
   int errstat;
   DCR *dcr = NULL;
   DEVICE *dev;


   /* If no device type specified, try to guess */
   if (!device->dev_type) {
      /* Check that device is available */
      if (stat(device->device_name, &statp) < 0) {
         berrno be;
         Jmsg2(jcr, M_ERROR, 0, _("Unable to stat device %s: ERR=%s\n"), 
            device->device_name, be.strerror());
         return NULL;
      }
      if (S_ISDIR(statp.st_mode)) {
         device->dev_type = B_FILE_DEV;
      } else if (S_ISCHR(statp.st_mode)) {
         device->dev_type = B_TAPE_DEV;
      } else if (S_ISFIFO(statp.st_mode)) {
         device->dev_type = B_FILE_DEV;
      } else if (!(device->cap_bits & CAP_REQMOUNT)) {
         Jmsg2(jcr, M_ERROR, 0, _("%s is an unknown device type. Must be tape or directory\n"
               " or have RequiresMount=yes for DVD. st_mode=%x\n"),
            device->device_name, statp.st_mode);
         return NULL;
      } else {
         device->dev_type = B_DVD_DEV;
      }
   }

   dev = (DEVICE *)malloc(sizeof(DEVICE));
   memset(dev, 0, sizeof(DEVICE));

   /* Copy user supplied device parameters from Resource */
   dev->dev_name = get_memory(strlen(device->device_name)+1);
   pm_strcpy(dev->dev_name, device->device_name);
   dev->prt_name = get_memory(strlen(device->device_name) + strlen(device->hdr.name) + 20);
   /* We edit "Resource-name" (physical-name) */
   Mmsg(dev->prt_name, "\"%s\" (%s)", device->hdr.name, device->device_name);
   Dmsg1(400, "Allocate dev=%s\n", dev->print_name());
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
   dev->autoselect = device->autoselect;
   dev->dev_type = device->dev_type;
   if (dev->is_tape()) { /* No parts on tapes */
      dev->max_part_size = 0;
   } else {
      dev->max_part_size = device->max_part_size;
   }
   /* Sanity check */
   if (dev->vol_poll_interval && dev->vol_poll_interval < 60) {
      dev->vol_poll_interval = 60;
   }
   /* Link the dev and device structures together */
   dev->device = device;
   device->dev = dev;

   if (dev->is_fifo()) {
      dev->capabilities |= CAP_STREAM; /* set stream device */
   }

   /* If the device requires mount :
    * - Check that the mount point is available 
    * - Check that (un)mount commands are defined
    */
   if ((dev->is_file() || dev->is_dvd()) && dev->requires_mount()) {
      if (!device->mount_point || stat(device->mount_point, &statp) < 0) {
         berrno be;
         dev->dev_errno = errno;
         Jmsg2(jcr, M_ERROR, 0, _("Unable to stat mount point %s: ERR=%s\n"), 
            device->mount_point, be.strerror());
         return NULL;
      }
   }
   if (dev->is_dvd()) {
      if (!device->mount_command || !device->unmount_command) {
         Jmsg0(jcr, M_ERROR_TERM, 0, _("Mount and unmount commands must defined for a device which requires mount.\n"));
      }
      if (!device->write_part_command) {
         Jmsg0(jcr, M_ERROR_TERM, 0, _("Write part command must be defined for a device which requires mount.\n"));
      }
   }

   if (dev->max_block_size > 1000000) {
      Jmsg3(jcr, M_ERROR, 0, _("Block size %u on device %s is too large, using default %u\n"),
         dev->max_block_size, dev->print_name(), DEFAULT_BLOCK_SIZE);
      dev->max_block_size = 0;
   }
   if (dev->max_block_size % TAPE_BSIZE != 0) {
      Jmsg2(jcr, M_WARNING, 0, _("Max block size %u not multiple of device %s block size.\n"),
         dev->max_block_size, dev->print_name());
   }

   dev->errmsg = get_pool_memory(PM_EMSG);
   *dev->errmsg = 0;

   if ((errstat = pthread_mutex_init(&dev->mutex, NULL)) != 0) {
      berrno be;
      dev->dev_errno = errstat;
      Mmsg1(dev->errmsg, _("Unable to init mutex: ERR=%s\n"), be.strerror(errstat));
      Jmsg0(jcr, M_ERROR_TERM, 0, dev->errmsg);
   }
   if ((errstat = pthread_cond_init(&dev->wait, NULL)) != 0) {
      berrno be;
      dev->dev_errno = errstat;
      Mmsg1(dev->errmsg, _("Unable to init cond variable: ERR=%s\n"), be.strerror(errstat));
      Jmsg0(jcr, M_ERROR_TERM, 0, dev->errmsg);
   }
   if ((errstat = pthread_cond_init(&dev->wait_next_vol, NULL)) != 0) {
      berrno be;
      dev->dev_errno = errstat;
      Mmsg1(dev->errmsg, _("Unable to init cond variable: ERR=%s\n"), be.strerror(errstat));
      Jmsg0(jcr, M_ERROR_TERM, 0, dev->errmsg);
   }
   if ((errstat = pthread_mutex_init(&dev->spool_mutex, NULL)) != 0) {
      berrno be;
      dev->dev_errno = errstat;
      Mmsg1(dev->errmsg, _("Unable to init mutex: ERR=%s\n"), be.strerror(errstat));
      Jmsg0(jcr, M_ERROR_TERM, 0, dev->errmsg);
   }
   if ((errstat = rwl_init(&dev->lock)) != 0) {
      berrno be;
      dev->dev_errno = errstat;
      Mmsg1(dev->errmsg, _("Unable to init mutex: ERR=%s\n"), be.strerror(errstat));
      Jmsg0(jcr, M_ERROR_TERM, 0, dev->errmsg);
   }

   dev->clear_opened();
   dev->attached_dcrs = New(dlist(dcr, &dcr->dev_link));
   Dmsg2(29, "init_dev: tape=%d dev_name=%s\n", dev->is_tape(), dev->dev_name);
   
   return dev;
}

/*
 * Open the device with the operating system and
 * initialize buffer pointers.
 *
 * Returns:  -1  on error
 *           fd  on success
 *
 * Note, for a tape, the VolName is the name we give to the
 *    volume (not really used here), but for a file, the
 *    VolName represents the name of the file to be created/opened.
 *    In the case of a file, the full name is the device name
 *    (archive_name) with the VolName concatenated.
 */
int
DEVICE::open(DCR *dcr, int omode)
{
   int preserve = 0;
   if (is_open()) {
      if (openmode == omode) {
         return fd;
      } else {
        ::close(fd); /* use system close so correct mode will be used on open */
        clear_opened();
        Dmsg0(100, "Close fd for mode change.\n");
        preserve = state & (ST_LABEL|ST_APPEND|ST_READ);
      }
   }
   if (dcr) {
      bstrncpy(VolCatInfo.VolCatName, dcr->VolumeName, sizeof(VolCatInfo.VolCatName));
   }

   Dmsg4(29, "open dev: type=%d dev_name=%s vol=%s mode=%s\n", dev_type,
         print_name(), VolCatInfo.VolCatName, mode_to_str(omode));
   state &= ~(ST_LABEL|ST_APPEND|ST_READ|ST_EOT|ST_WEOT|ST_EOF);
   label_type = B_BACULA_LABEL;
   if (is_tape() || is_fifo()) {
      open_tape_device(dcr, omode);
   } else if (is_dvd()) {
      Dmsg1(100, "call open_dvd_device mode=%s\n", mode_to_str(omode));
      open_dvd_device(dcr, omode);
   } else {
      Dmsg1(100, "call open_file_device mode=%s\n", mode_to_str(omode));
      open_file_device(dcr, omode);
   }
   state |= preserve;                 /* reset any important state info */
   Dmsg1(100, "preserve=0x%x\n", preserve);
   return fd;
}

void DEVICE::set_mode(int new_mode) 
{
   switch (new_mode) {
   case CREATE_READ_WRITE:
      mode = O_CREAT | O_RDWR | O_BINARY;
      break;
   case OPEN_READ_WRITE:
      mode = O_RDWR | O_BINARY;
      break;
   case OPEN_READ_ONLY:
      mode = O_RDONLY | O_BINARY;
      break;
   case OPEN_WRITE_ONLY:
      mode = O_WRONLY | O_BINARY;
      break;
   default:
      Emsg0(M_ABORT, 0, _("Illegal mode given to open dev.\n"));
   }
}

/*
 */
void DEVICE::open_tape_device(DCR *dcr, int omode) 
{
   file_size = 0;
   int timeout;
   int nonblocking = O_NONBLOCK;
   Dmsg0(29, "open dev: device is tape\n");

   if (is_autochanger()) {
      get_autochanger_loaded_slot(dcr);
   }

   set_mode(omode);
   timeout = max_open_wait;
   errno = 0;
   if (is_fifo() && timeout) {
      /* Set open timer */
      tid = start_thread_timer(pthread_self(), timeout);
   }
   /* If busy retry each second for max_open_wait seconds */
   Dmsg3(100, "Try open %s mode=%s nonblocking=%d\n", print_name(),
      mode_to_str(omode), nonblocking);
   /* Use system open() */
   while ((fd = ::open(dev_name, mode+nonblocking)) < 0) {
      berrno be;
      dev_errno = errno;
      Dmsg5(050, "Open omode=%d mode=%x nonblock=%d error errno=%d ERR=%s\n", 
           omode, mode, nonblocking, errno, be.strerror());
      if (dev_errno == EINTR || dev_errno == EAGAIN) {
         Dmsg0(100, "Continue open\n");
         continue;
      }
      /* Busy wait for specified time (default = 5 mins) */
      if (dev_errno == EBUSY && timeout-- > 0) {
         Dmsg2(100, "Device %s busy. ERR=%s\n", print_name(), be.strerror());
         bmicrosleep(1, 0);
         continue;
      }
      Mmsg2(errmsg, _("Unable to open device %s: ERR=%s\n"),
            print_name(), be.strerror(dev_errno));
      /* Stop any open timer we set */
      if (tid) {
         stop_thread_timer(tid);
         tid = 0;
      }
      Jmsg0(dcr->jcr, M_FATAL, 0, errmsg);
      break;
   }

   if (fd >= 0) {
      openmode = omode;              /* save open mode */
      set_blocking();   
      Dmsg2(100, "openmode=%d %s\n", openmode, mode_to_str(openmode));
      dev_errno = 0;
      update_pos_dev(this);                /* update position */
      set_os_device_parameters(this);      /* do system dependent stuff */
   } else {
      clear_opened();
   }

   /* Stop any open() timer we started */
   if (tid) {
      stop_thread_timer(tid);
      tid = 0;
   }
   Dmsg1(29, "open dev: tape %d opened\n", fd);
}

void DEVICE::set_blocking()
{
   int oflags;
   /* Try to reset blocking */
#ifdef xxx
   if ((oflags = fcntl(fd, F_GETFL, 0)) < 0 ||
       fcntl(fd, F_SETFL, oflags & ~O_NONBLOCK) < 0) {
      berrno be;
      ::close(fd);                   /* use system close() */
      fd = ::open(dev_name, mode);       
      Dmsg2(100, "fcntl error. ERR=%s. Close-reopen fd=%d\n", be.strerror(), fd);
   }
#endif
   oflags = fcntl(fd, F_GETFL, 0);       
   if (oflags > 0 && (oflags & O_NONBLOCK)) {
      fcntl(fd, F_SETFL, oflags & ~O_NONBLOCK);
   }
}

/*
 * Open a file device
 */
void DEVICE::open_file_device(DCR *dcr, int omode) 
{
   POOL_MEM archive_name(PM_FNAME);

   if (is_autochanger()) {
      get_autochanger_loaded_slot(dcr);
   }

   /*
    * Handle opening of File Archive (not a tape)
    */     

   pm_strcpy(archive_name, dev_name);
   /*  
    * If this is a virtual autochanger (i.e. changer_res != NULL)
    *  we simply use the deviced name, assuming it has been
    *  appropriately setup by the "autochanger".
    */
   if (!device->changer_res) {
      if (VolCatInfo.VolCatName[0] == 0) {
         Mmsg(errmsg, _("Could not open file device %s. No Volume name given.\n"),
            print_name());
         clear_opened();
         return;
      }

      if (archive_name.c_str()[strlen(archive_name.c_str())-1] != '/') {
         pm_strcat(archive_name, "/");
      }
      pm_strcat(archive_name, VolCatInfo.VolCatName);
   }

   mount(1);                          /* do mount if required */
         
   openmode = omode;
   set_mode(omode);
   /* If creating file, give 0640 permissions */
   Dmsg3(29, "open disk: mode=%s open(%s, 0x%x, 0640)\n", mode_to_str(omode), 
         archive_name.c_str(), mode);
   /* Use system open() */
   if ((fd = ::open(archive_name.c_str(), mode, 0640)) < 0) {
      berrno be;
      dev_errno = errno;
      Mmsg2(errmsg, _("Could not open: %s, ERR=%s\n"), archive_name.c_str(), 
            be.strerror());
      Dmsg1(29, "open failed: %s", errmsg);
      Emsg0(M_FATAL, 0, errmsg);
   } else {
      dev_errno = 0;
      update_pos_dev(this);                /* update position */
   }
   Dmsg4(29, "open dev: disk fd=%d opened, part=%d/%d, part_size=%u\n", 
      fd, part, num_parts, part_size);
}

/*
 * Open a DVD device. N.B. at this point, dcr->VolCatInfo.VolCatName 
 *  (NB:??? I think it's VolCatInfo.VolCatName that is right)
 *  has the desired Volume name, but there is NO assurance that
 *  any other field of VolCatInfo is correct.
 */
void DEVICE::open_dvd_device(DCR *dcr, int omode) 
{
   POOL_MEM archive_name(PM_FNAME);
   struct stat filestat;

   /*
    * Handle opening of DVD Volume
    */     
   Dmsg3(29, "Enter: open_dvd_dev: %s dev=%s mode=%s\n", is_dvd()?"DVD":"disk",
         archive_name.c_str(), mode_to_str(omode));

   if (VolCatInfo.VolCatName[0] == 0) {
      Dmsg1(10,  "Could not open file device %s. No Volume name given.\n",
         print_name());
      Mmsg(errmsg, _("Could not open file device %s. No Volume name given.\n"),
         print_name());
      clear_opened();
      return;
   }

   if (part == 0) {
      file_size = 0;
   }
   part_size = 0;

   Dmsg2(99, "open_dvd_device: num_parts=%d, VolCatInfo.VolCatParts=%d\n",
      dcr->dev->num_parts, dcr->VolCatInfo.VolCatParts);
   if (dcr->dev->num_parts < dcr->VolCatInfo.VolCatParts) {
      Dmsg2(99, "open_dvd_device: num_parts updated to %d (was %d)\n",
         dcr->VolCatInfo.VolCatParts, dcr->dev->num_parts);
      dcr->dev->num_parts = dcr->VolCatInfo.VolCatParts;
   }

   if (mount_dvd(this, 1)) {
      if ((num_parts == 0) && (!truncating)) {
         /* If we can mount the device, and we are not truncating the DVD, we usually want to abort. */
         /* There is one exception, if there is only one 0-sized file on the DVD, with the right volume name,
          * we continue (it's the method used by truncate_dvd to truncate a volume). */
         if (!check_can_write_on_non_blank_dvd(dcr)) {
            Mmsg(errmsg, _("The media in the device %s is not empty, please blank it before writing anything to it.\n"), print_name());
            Emsg0(M_FATAL, 0, errmsg);
            unmount_dvd(this, 1); /* Unmount the device, so the operator can change it. */
            clear_opened();
            return;
         }
      }
   } else {
      /* We cannot mount the device */
      if (num_parts == 0) {
         /* Run free space, check there is a media. */
         update_free_space_dev(this);
         if (have_media()) {
            Dmsg1(29, "Could not mount device %s, this is not a problem (num_parts == 0), and have media.\n", print_name());
         }
         else {
            Mmsg(errmsg, _("There is no valid media in the device %s.\n"), print_name());
            Emsg0(M_FATAL, 0, errmsg);
            clear_opened();
            return;
         }
      }
      else {
         Mmsg(errmsg, _("Could not mount device %s.\n"), print_name());
         Emsg0(M_FATAL, 0, errmsg);
         clear_opened();
         return;
      }
   }
   
   Dmsg6(29, "open dev: %s dev=%s mode=%s part=%d npart=%d volcatnparts=%d\n", 
      is_dvd()?"DVD":"disk", archive_name.c_str(), mode_to_str(omode),
      part, num_parts, dcr->VolCatInfo.VolCatParts);
   openmode = omode;
   Dmsg2(100, "openmode=%d %s\n", openmode, mode_to_str(openmode));
   
   /*
    * If we are not trying to access the last part, set mode to 
    *   OPEN_READ_ONLY as writing would be an error.
    */
   if (part < num_parts) {
      omode = OPEN_READ_ONLY;
      make_mounted_dvd_filename(this, archive_name);
   }
   else {
      make_spooled_dvd_filename(this, archive_name);
   }
   set_mode(omode);

   /* If creating file, give 0640 permissions */
   Dmsg3(29, "mode=%s open(%s, 0x%x, 0640)\n", mode_to_str(omode), 
         archive_name.c_str(), mode);
   /* Use system open() */
   if ((fd = ::open(archive_name.c_str(), mode, 0640)) < 0) {
      berrno be;
      Mmsg2(errmsg, _("Could not open: %s, ERR=%s\n"), archive_name.c_str(), 
            be.strerror());
      dev_errno = EIO; /* Interpreted as no device present by acquire.c:acquire_device_for_read(). */
      Dmsg1(29, "open failed: %s", errmsg);
      
      if ((omode == OPEN_READ_ONLY) && (part == num_parts)) {
         /* If the last part (on spool), doesn't exists when reading, create it and read from it
          * (it will report immediately an EOF):
          * Sometimes it is better to finish with an EOF than with an error. */
         set_mode(OPEN_READ_WRITE);
         fd = ::open(archive_name.c_str(), mode, 0640);
         set_mode(OPEN_READ_ONLY);
      }
      
      /* We don't need it. Only the last part is on spool */
      /*if (omode == OPEN_READ_ONLY) {
         make_spooled_dvd_filename(this, archive_name);
         fd = ::open(archive_name.c_str(), mode, 0640);  // try on spool
      }*/
   }
   Dmsg1(100, "after open fd=%d\n", fd);
   if (fd >= 0) {
      /* Get size of file */
      if (fstat(fd, &filestat) < 0) {
         berrno be;
         dev_errno = errno;
         Mmsg2(errmsg, _("Could not fstat: %s, ERR=%s\n"), archive_name.c_str(), 
               be.strerror());
         Dmsg1(29, "open failed: %s", errmsg);
         /* Use system close() */
         ::close(fd);
         clear_opened();
      } else {
         part_size = filestat.st_size;
         dev_errno = 0;
         update_pos_dev(this);                /* update position */
         
         /* NB: It seems this code is wrong... part number is incremented in open_next_part, not here */
         
         /* Check if just created Volume  part */
/*         if (omode == OPEN_READ_WRITE && (part == 0 || part_size == 0)) {
            part++;
            num_parts = part;
            VolCatInfo.VolCatParts = num_parts;
         } else {
            if (part == 0) {             // we must have opened the first part
               part++;
            }
         }*/
      }
   }
}


/*
 * Rewind the device.
 *  Returns: true  on success
 *           false on failure
 */
bool DEVICE::rewind(DCR *dcr)
{
   struct mtop mt_com;
   unsigned int i;
   bool first = true;

   Dmsg3(400, "rewind res=%d fd=%d %s\n", reserved_device, fd, print_name());
   if (fd < 0) {
      if (!is_dvd()) { /* In case of major error, the fd is not open on DVD, so we don't want to abort. */
         dev_errno = EBADF;
         Mmsg1(errmsg, _("Bad call to rewind. Device %s not open\n"),
            print_name());
         Emsg0(M_ABORT, 0, errmsg);
      }
      return false;
   }
   state &= ~(ST_EOT|ST_EOF|ST_WEOT);  /* remove EOF/EOT flags */
   block_num = file = 0;
   file_size = 0;
   file_addr = 0;
   if (is_tape()) {
      mt_com.mt_op = MTREW;
      mt_com.mt_count = 1;
      /* If we get an I/O error on rewind, it is probably because
       * the drive is actually busy. We loop for (about 5 minutes)
       * retrying every 5 seconds.
       */
      for (i=max_rewind_wait; ; i -= 5) {
         if (ioctl(fd, MTIOCTOP, (char *)&mt_com) < 0) {
            berrno be;
            clrerror(MTREW);
            if (i == max_rewind_wait) {
               Dmsg1(200, "Rewind error, %s. retrying ...\n", be.strerror());
            }
            /*
             * This is a gross hack, because if the user has the
             *   device mounted (i.e. open), then uses mtx to load
             *   a tape, the current open file descriptor is invalid.
             *   So, we close the drive and re-open it.
             */
            if (first && dcr) {
               int open_mode = openmode;
               ::close(fd);
               clear_opened();
               open(dcr, open_mode);
               if (fd < 0) {
                  return false;
               }
               first = false;
               continue;
            }
            if (dev_errno == EIO && i > 0) {
               Dmsg0(200, "Sleeping 5 seconds.\n");
               bmicrosleep(5, 0);
               continue;
            }
            Mmsg2(errmsg, _("Rewind error on %s. ERR=%s.\n"),
               print_name(), be.strerror());
            return false;
         }
         break;
      }
   } else if (is_file() || is_dvd()) {
      if (lseek_dev(this, (off_t)0, SEEK_SET) < 0) {
         berrno be;
         dev_errno = errno;
         Mmsg2(errmsg, _("lseek_dev error on %s. ERR=%s.\n"),
            print_name(), be.strerror());
         return false;
      }
   }
   return true;
}

void DEVICE::block(int why)
{
   lock_device(this);
   block_device(this, why);
   V(mutex);
}

void DEVICE::unblock()
{  
   P(mutex);
   unblock_device(this);
   V(mutex);
}

const char *DEVICE::print_blocked() const 
{
   switch (dev_blocked) {
   case BST_NOT_BLOCKED:
      return "BST_NOT_BLOCKED";
   case BST_UNMOUNTED:
      return "BST_UNMOUNTED";
   case BST_WAITING_FOR_SYSOP:
      return "BST_WAITING_FOR_SYSOP";
   case BST_DOING_ACQUIRE:
      return "BST_DOING_ACQUIRE";
   case BST_WRITING_LABEL:
      return "BST_WRITING_LABEL";
   case BST_UNMOUNTED_WAITING_FOR_SYSOP:
      return "BST_UNMOUNTED_WAITING_FOR_SYSOP";
   case BST_MOUNT:
      return "BST_MOUNT";
   default:
      return _("unknown blocked code");
   }
}

/*
 * Called to indicate that we have just read an
 *  EOF from the device.
 */
void DEVICE::set_ateof() 
{ 
   set_eof();
   if (is_tape()) {
      file++;
   }
   file_addr = 0;
   file_size = 0;
   block_num = 0;
}

/*
 * Called to indicate we are now at the end of the tape, and
 *   writing is not possible.
 */
void DEVICE::set_ateot() 
{
   /* Make tape effectively read-only */
   state |= (ST_EOF|ST_EOT|ST_WEOT);
   clear_append();
}

/*
 * Position device to end of medium (end of data)
 *  Returns: true  on succes
 *           false on error
 */
bool DEVICE::eod()
{
   struct mtop mt_com;
   struct mtget mt_stat;
   bool ok = true;
   off_t pos;

   if (fd < 0) {
      dev_errno = EBADF;
      Mmsg1(errmsg, _("Bad call to eod_dev. Device %s not open\n"), print_name());
      return false;
   }

#if defined (__digital__) && defined (__unix__)
   return fsf(VolCatInfo.VolCatFiles);
#endif

   Dmsg0(29, "eod_dev\n");
   if (at_eot()) {
      return true;
   }
   clear_eof();         /* remove EOF flag */
   block_num = file = 0;
   file_size = 0;
   file_addr = 0;
   if (is_fifo() || is_prog()) {
      return true;
   }
   if (!is_tape()) {
      pos = lseek_dev(this, (off_t)0, SEEK_END);
//    Dmsg1(100, "====== Seek to %lld\n", pos);
      if (pos >= 0) {
         update_pos_dev(this);
         set_eot();
         return true;
      }
      dev_errno = errno;
      berrno be;
      Mmsg2(errmsg, _("lseek_dev error on %s. ERR=%s.\n"),
             print_name(), be.strerror());
      return false;
   }
#ifdef MTEOM
   if (has_cap(CAP_FASTFSF) && !has_cap(CAP_EOM)) {
      Dmsg0(100,"Using FAST FSF for EOM\n");
      /* If unknown position, rewind */
      if (!dev_get_os_pos(this, &mt_stat)) {
        if (!rewind(NULL)) {
          return false;
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

   if (has_cap(CAP_MTIOCGET) && (has_cap(CAP_FASTFSF) || has_cap(CAP_EOM))) {
      if (has_cap(CAP_EOM)) {
         Dmsg0(100,"Using EOM for EOM\n");
         mt_com.mt_op = MTEOM;
         mt_com.mt_count = 1;
      }

      if (ioctl(fd, MTIOCTOP, (char *)&mt_com) < 0) {
         berrno be;
         clrerror(mt_com.mt_op);
         Dmsg1(50, "ioctl error: %s\n", be.strerror());
         update_pos_dev(this);
         Mmsg2(errmsg, _("ioctl MTEOM error on %s. ERR=%s.\n"),
            print_name(), be.strerror());
         return false;
      }

      if (!dev_get_os_pos(this, &mt_stat)) {
         berrno be;
         clrerror(-1);
         Mmsg2(errmsg, _("ioctl MTIOCGET error on %s. ERR=%s.\n"),
            print_name(), be.strerror());
         return false;
      }
      Dmsg2(100, "EOD file=%d block=%d\n", mt_stat.mt_fileno, mt_stat.mt_blkno);
      set_ateof();
      file = mt_stat.mt_fileno;
   } else {
#else
   {
#endif
      /*
       * Rewind then use FSF until EOT reached
       */
      if (!rewind(NULL)) {
         return false;
      }
      /*
       * Move file by file to the end of the tape
       */
      int file_num;
      for (file_num=file; !at_eot(); file_num++) {
         Dmsg0(200, "eod_dev: doing fsf 1\n");
         if (!fsf(1)) {
            Dmsg0(200, "fsf error.\n");
            return false;
         }
         /*
          * Avoid infinite loop by ensuring we advance.
          */
         if (file_num == (int)file) {
            struct mtget mt_stat;
            Dmsg1(100, "fsf did not advance from file %d\n", file_num);
            set_ateof();
            if (dev_get_os_pos(this, &mt_stat)) {
               Dmsg2(100, "Adjust file from %d to %d\n", file , mt_stat.mt_fileno);
               file = mt_stat.mt_fileno;
            }       
            break;
         }
      }
   }
   /*
    * Some drivers leave us after second EOF when doing
    * MTEOM, so we must backup so that appending overwrites
    * the second EOF.
    */
   if (has_cap(CAP_BSFATEOM)) {
      struct mtget mt_stat;
      /* Backup over EOF */
      ok = bsf(1);
      /* If BSF worked and fileno is known (not -1), set file */
      if (dev_get_os_pos(this, &mt_stat)) {
         Dmsg2(100, "BSFATEOF adjust file from %d to %d\n", file , mt_stat.mt_fileno);
         file = mt_stat.mt_fileno;
      } else {
         file++;                 /* wing it -- not correct on all OSes */
      }
   } else {
      update_pos_dev(this);                   /* update position */
   }
   Dmsg1(200, "EOD dev->file=%d\n", file);
   return ok;
}

/*
 * Set the position of the device -- only for files and DVD
 *   For other devices, there is no generic way to do it.
 *  Returns: true  on succes
 *           false on error
 */
bool update_pos_dev(DEVICE *dev)
{
   off_t pos;
   bool ok = true;

   if (dev->fd < 0) {
      dev->dev_errno = EBADF;
      Mmsg0(dev->errmsg, _("Bad device call. Device not open\n"));
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
         Pmsg1(000, _("Seek error: ERR=%s\n"), be.strerror());
         Mmsg2(dev->errmsg, _("lseek_dev error on %s. ERR=%s.\n"),
            dev->print_name(), be.strerror());
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
      Pmsg0(-20, " EOD");
   }
   if (dev->state & ST_EOF) {
      stat |= BMT_EOF;
      Pmsg0(-20, " EOF");
   }
   if (dev->is_tape()) {
      stat |= BMT_TAPE;
      Pmsg0(-20,_(" Bacula status:"));
      Pmsg2(-20,_(" file=%d block=%d\n"), dev->file, dev->block_num);
      if (ioctl(dev->fd, MTIOCGET, (char *)&mt_stat) < 0) {
         berrno be;
         dev->dev_errno = errno;
         Mmsg2(dev->errmsg, _("ioctl MTIOCGET error on %s. ERR=%s.\n"),
            dev->print_name(), be.strerror());
         return 0;
      }
      Pmsg0(-20, _(" Device status:"));

#if defined(HAVE_LINUX_OS)
      if (GMT_EOF(mt_stat.mt_gstat)) {
         stat |= BMT_EOF;
         Pmsg0(-20, " EOF");
      }
      if (GMT_BOT(mt_stat.mt_gstat)) {
         stat |= BMT_BOT;
         Pmsg0(-20, " BOT");
      }
      if (GMT_EOT(mt_stat.mt_gstat)) {
         stat |= BMT_EOT;
         Pmsg0(-20, " EOT");
      }
      if (GMT_SM(mt_stat.mt_gstat)) {
         stat |= BMT_SM;
         Pmsg0(-20, " SM");
      }
      if (GMT_EOD(mt_stat.mt_gstat)) {
         stat |= BMT_EOD;
         Pmsg0(-20, " EOD");
      }
      if (GMT_WR_PROT(mt_stat.mt_gstat)) {
         stat |= BMT_WR_PROT;
         Pmsg0(-20, " WR_PROT");
      }
      if (GMT_ONLINE(mt_stat.mt_gstat)) {
         stat |= BMT_ONLINE;
         Pmsg0(-20, " ONLINE");
      }
      if (GMT_DR_OPEN(mt_stat.mt_gstat)) {
         stat |= BMT_DR_OPEN;
         Pmsg0(-20, " DR_OPEN");
      }
      if (GMT_IM_REP_EN(mt_stat.mt_gstat)) {
         stat |= BMT_IM_REP_EN;
         Pmsg0(-20, " IM_REP_EN");
      }
#endif /* !SunOS && !OSF */
      if (dev->has_cap(CAP_MTIOCGET)) {
         Pmsg2(-20, _(" file=%d block=%d\n"), mt_stat.mt_fileno, mt_stat.mt_blkno);
      } else {
         Pmsg2(-20, _(" file=%d block=%d\n"), -1, -1);
      }
   } else {
      stat |= BMT_ONLINE | BMT_BOT;
   }
   return stat;
}


/*
 * Load medium in device
 *  Returns: true  on success
 *           false on failure
 */
bool load_dev(DEVICE *dev)
{
#ifdef MTLOAD
   struct mtop mt_com;
#endif

   if (dev->fd < 0) {
      dev->dev_errno = EBADF;
      Mmsg0(dev->errmsg, _("Bad call to load_dev. Device not open\n"));
      Emsg0(M_FATAL, 0, dev->errmsg);
      return false;
   }
   if (!(dev->is_tape())) {
      return true;
   }
#ifndef MTLOAD
   Dmsg0(200, "stored: MTLOAD command not available\n");
   berrno be;
   dev->dev_errno = ENOTTY;           /* function not available */
   Mmsg2(dev->errmsg, _("ioctl MTLOAD error on %s. ERR=%s.\n"),
         dev->print_name(), be.strerror());
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
      Mmsg2(dev->errmsg, _("ioctl MTLOAD error on %s. ERR=%s.\n"),
         dev->print_name(), be.strerror());
      return false;
   }
   return true;
#endif
}

/*
 * Rewind device and put it offline
 *  Returns: true  on success
 *           false on failure
 */
bool DEVICE::offline()
{
   struct mtop mt_com;

   if (!is_tape()) {
      return true;                    /* device not open */
   }

   state &= ~(ST_APPEND|ST_READ|ST_EOT|ST_EOF|ST_WEOT);  /* remove EOF/EOT flags */
   block_num = file = 0;
   file_size = 0;
   file_addr = 0;
   part = 0;
#ifdef MTUNLOCK
   mt_com.mt_op = MTUNLOCK;
   mt_com.mt_count = 1;
   ioctl(fd, MTIOCTOP, (char *)&mt_com);
#endif
   mt_com.mt_op = MTOFFL;
   mt_com.mt_count = 1;
   if (ioctl(fd, MTIOCTOP, (char *)&mt_com) < 0) {
      berrno be;
      dev_errno = errno;
      Mmsg2(errmsg, _("ioctl MTOFFL error on %s. ERR=%s.\n"),
         print_name(), be.strerror());
      return false;
   }
   Dmsg1(100, "Offlined device %s\n", print_name());
   return true;
}

bool DEVICE::offline_or_rewind()
{
   if (fd < 0) {
      return false;
   }
   if (has_cap(CAP_OFFLINEUNMOUNT)) {
      return offline();
   } else {
   /*
    * Note, this rewind probably should not be here (it wasn't
    *  in prior versions of Bacula), but on FreeBSD, this is
    *  needed in the case the tape was "frozen" due to an error
    *  such as backspacing after writing and EOF. If it is not
    *  done, all future references to the drive get and I/O error.
    */
      clrerror(MTREW);
      return rewind(NULL);
   }
}

/*
 * Foward space a file
 *   Returns: true  on success
 *            false on failure
 */
bool DEVICE::fsf(int num)
{
   struct mtget mt_stat;
   struct mtop mt_com;
   int stat = 0;

   if (fd < 0) {
      dev_errno = EBADF;
      Mmsg0(errmsg, _("Bad call to fsf_dev. Device not open\n"));
      Emsg0(M_FATAL, 0, errmsg);
      return false;
   }

   if (!is_tape()) {
      return true;
   }
   if (at_eot()) {
      dev_errno = 0;
      Mmsg1(errmsg, _("Device %s at End of Tape.\n"), print_name());
      return false;
   }
   if (at_eof()) {
      Dmsg0(200, "ST_EOF set on entry to FSF\n");
   }

   Dmsg0(100, "fsf\n");
   block_num = 0;
   /*
    * If Fast forward space file is set, then we
    *  use MTFSF to forward space and MTIOCGET
    *  to get the file position. We assume that
    *  the SCSI driver will ensure that we do not
    *  forward space past the end of the medium.
    */
   if (has_cap(CAP_FSF) && has_cap(CAP_MTIOCGET) && has_cap(CAP_FASTFSF)) {
      mt_com.mt_op = MTFSF;
      mt_com.mt_count = num;
      stat = ioctl(fd, MTIOCTOP, (char *)&mt_com);
      if (stat < 0 || !dev_get_os_pos(this, &mt_stat)) {
         berrno be;
         set_eot();
         Dmsg0(200, "Set ST_EOT\n");
         clrerror(MTFSF);
         Mmsg2(errmsg, _("ioctl MTFSF error on %s. ERR=%s.\n"),
            print_name(), be.strerror());
         Dmsg1(200, "%s", errmsg);
         return false;
      }
      Dmsg2(200, "fsf file=%d block=%d\n", mt_stat.mt_fileno, mt_stat.mt_blkno);
      set_ateof();
      file = mt_stat.mt_fileno;
      return true;

   /*
    * Here if CAP_FSF is set, and virtually all drives
    *  these days support it, we read a record, then forward
    *  space one file. Using this procedure, which is slow,
    *  is the only way we can be sure that we don't read
    *  two consecutive EOF marks, which means End of Data.
    */
   } else if (has_cap(CAP_FSF)) {
      POOLMEM *rbuf;
      int rbuf_len;
      Dmsg0(200, "FSF has cap_fsf\n");
      if (max_block_size == 0) {
         rbuf_len = DEFAULT_BLOCK_SIZE;
      } else {
         rbuf_len = max_block_size;
      }
      rbuf = get_memory(rbuf_len);
      mt_com.mt_op = MTFSF;
      mt_com.mt_count = 1;
      while (num-- && !at_eot()) {
         Dmsg0(100, "Doing read before fsf\n");
         if ((stat = read(fd, (char *)rbuf, rbuf_len)) < 0) {
            if (errno == ENOMEM) {     /* tape record exceeds buf len */
               stat = rbuf_len;        /* This is OK */
            /*
             * On IBM drives, they return ENOSPC at EOM
             *  instead of EOF status
             */
            } else if (at_eof() && errno == ENOSPC) {
               stat = 0;
            } else {
               berrno be;
               set_eot();
               clrerror(-1);
               Dmsg2(100, "Set ST_EOT read errno=%d. ERR=%s\n", dev_errno,
                  be.strerror());
               Mmsg2(errmsg, _("read error on %s. ERR=%s.\n"),
                  print_name(), be.strerror());
               Dmsg1(100, "%s", errmsg);
               break;
            }
         }
         if (stat == 0) {                /* EOF */
            update_pos_dev(this);
            Dmsg1(100, "End of File mark from read. File=%d\n", file+1);
            /* Two reads of zero means end of tape */
            if (at_eof()) {
               set_eot();
               Dmsg0(100, "Set ST_EOT\n");
               break;
            } else {
               set_ateof();
               continue;
            }
         } else {                        /* Got data */
            clear_eot();
            clear_eof();
         }

         Dmsg0(100, "Doing MTFSF\n");
         stat = ioctl(fd, MTIOCTOP, (char *)&mt_com);
         if (stat < 0) {                 /* error => EOT */
            berrno be;
            set_eot();
            Dmsg0(100, "Set ST_EOT\n");
            clrerror(MTFSF);
            Mmsg2(errmsg, _("ioctl MTFSF error on %s. ERR=%s.\n"),
               print_name(), be.strerror());
            Dmsg0(100, "Got < 0 for MTFSF\n");
            Dmsg1(100, "%s", errmsg);
         } else {
            set_ateof();
         }
      }
      free_memory(rbuf);

   /*
    * No FSF, so use FSR to simulate it
    */
   } else {
      Dmsg0(200, "Doing FSR for FSF\n");
      while (num-- && !at_eot()) {
         fsr(INT32_MAX);    /* returns -1 on EOF or EOT */
      }
      if (at_eot()) {
         dev_errno = 0;
         Mmsg1(errmsg, _("Device %s at End of Tape.\n"), print_name());
         stat = -1;
      } else {
         stat = 0;
      }
   }
   update_pos_dev(this);
   Dmsg1(200, "Return %d from FSF\n", stat);
   if (at_eof()) {
      Dmsg0(200, "ST_EOF set on exit FSF\n");
   }
   if (at_eot()) {
      Dmsg0(200, "ST_EOT set on exit FSF\n");
   }
   Dmsg1(200, "Return from FSF file=%d\n", file);
   return stat == 0;
}

/*
 * Backward space a file
 *  Returns: false on failure
 *           true  on success
 */
bool DEVICE::bsf(int num)
{
   struct mtop mt_com;
   int stat;

   if (fd < 0) {
      dev_errno = EBADF;
      Mmsg0(errmsg, _("Bad call to bsf. Device not open\n"));
      Emsg0(M_FATAL, 0, errmsg);
      return false;
   }

   if (!is_tape()) {
      Mmsg1(errmsg, _("Device %s cannot BSF because it is not a tape.\n"),
         print_name());
      return false;
   }
   Dmsg0(29, "bsf\n");
   clear_eot();
   clear_eof();
   file -= num;
   file_addr = 0;
   file_size = 0;
   mt_com.mt_op = MTBSF;
   mt_com.mt_count = num;
   stat = ioctl(fd, MTIOCTOP, (char *)&mt_com);
   if (stat < 0) {
      berrno be;
      clrerror(MTBSF);
      Mmsg2(errmsg, _("ioctl MTBSF error on %s. ERR=%s.\n"),
         print_name(), be.strerror());
   }
   update_pos_dev(this);
   return stat == 0;
}


/*
 * Foward space num records
 *  Returns: false on failure
 *           true  on success
 */
bool DEVICE::fsr(int num)
{
   struct mtop mt_com;
   int stat;

   if (fd < 0) {
      dev_errno = EBADF;
      Mmsg0(errmsg, _("Bad call to fsr. Device not open\n"));
      Emsg0(M_FATAL, 0, errmsg);
      return false;
   }

   if (!is_tape()) {
      return false;
   }
   if (!has_cap(CAP_FSR)) {
      Mmsg1(errmsg, _("ioctl MTFSR not permitted on %s.\n"), print_name());
      return false;
   }

   Dmsg1(29, "fsr %d\n", num);
   mt_com.mt_op = MTFSR;
   mt_com.mt_count = num;
   stat = ioctl(fd, MTIOCTOP, (char *)&mt_com);
   if (stat == 0) {
      clear_eof();
      block_num += num;
   } else {
      berrno be;
      struct mtget mt_stat;
      clrerror(MTFSR);
      Dmsg1(100, "FSF fail: ERR=%s\n", be.strerror());
      if (dev_get_os_pos(this, &mt_stat)) {
         Dmsg4(100, "Adjust from %d:%d to %d:%d\n", file,
            block_num, mt_stat.mt_fileno, mt_stat.mt_blkno);
         file = mt_stat.mt_fileno;
         block_num = mt_stat.mt_blkno;
      } else {
         if (at_eof()) {
            set_eot();
         } else {
            set_ateof();
         }
      }
      Mmsg3(errmsg, _("ioctl MTFSR %d error on %s. ERR=%s.\n"),
         num, print_name(), be.strerror());
   }
   update_pos_dev(this);
   return stat == 0;
}

/*
 * Backward space a record
 *   Returns:  false on failure
 *             true  on success
 */
bool DEVICE::bsr(int num)
{
   struct mtop mt_com;
   int stat;

   if (fd < 0) {
      dev_errno = EBADF;
      Mmsg0(errmsg, _("Bad call to bsr_dev. Device not open\n"));
      Emsg0(M_FATAL, 0, errmsg);
      return false;
   }

   if (!is_tape()) {
      return false;
   }

   if (!has_cap(CAP_BSR)) {
      Mmsg1(errmsg, _("ioctl MTBSR not permitted on %s.\n"), print_name());
      return false;
   }

   Dmsg0(29, "bsr_dev\n");
   block_num -= num;
   clear_eof();
   clear_eot();
   mt_com.mt_op = MTBSR;
   mt_com.mt_count = num;
   stat = ioctl(fd, MTIOCTOP, (char *)&mt_com);
   if (stat < 0) {
      berrno be;
      clrerror(MTBSR);
      Mmsg2(errmsg, _("ioctl MTBSR error on %s. ERR=%s.\n"),
         print_name(), be.strerror());
   }
   update_pos_dev(this);
   return stat == 0;
}

/*
 * Reposition the device to file, block
 * Returns: false on failure
 *          true  on success
 */
bool DEVICE::reposition(uint32_t rfile, uint32_t rblock)
{
   if (fd < 0) {
      dev_errno = EBADF;
      Mmsg0(errmsg, _("Bad call to reposition. Device not open\n"));
      Emsg0(M_FATAL, 0, errmsg);
      return false;
   }

   if (!is_tape()) {
      off_t pos = (((off_t)rfile)<<32) + (off_t)rblock;
      Dmsg1(100, "===== lseek_dev to %d\n", (int)pos);
      if (lseek_dev(this, pos, SEEK_SET) == (off_t)-1) {
         berrno be;
         dev_errno = errno;
         Mmsg2(errmsg, _("lseek_dev error on %s. ERR=%s.\n"),
            print_name(), be.strerror());
         return false;
      }
      file = rfile;
      block_num = rblock;
      file_addr = pos;
      return true;
   }
   Dmsg4(100, "reposition from %u:%u to %u:%u\n",
      file, block_num, rfile, rblock);
   if (rfile < file) {
      Dmsg0(100, "Rewind\n");
      if (!rewind(NULL)) {
         return false;
      }
   }
   if (rfile > file) {
      Dmsg1(100, "fsf %d\n", rfile-file);
      if (!fsf(rfile-file)) {
         Dmsg1(100, "fsf failed! ERR=%s\n", bstrerror());
         return false;
      }
      Dmsg2(100, "wanted_file=%d at_file=%d\n", rfile, file);
   }
   if (rblock < block_num) {
      Dmsg2(100, "wanted_blk=%d at_blk=%d\n", rblock, block_num);
      Dmsg0(100, "bsf 1\n");
      bsf(1);
      Dmsg0(100, "fsf_dev 1\n");
      fsf(1);
      Dmsg2(100, "wanted_blk=%d at_blk=%d\n", rblock, block_num);
   }
   if (has_cap(CAP_POSITIONBLOCKS) && rblock > block_num) {
      /* Ignore errors as Bacula can read to the correct block */
      Dmsg1(100, "fsr %d\n", rblock-block_num);
      return fsr(rblock-block_num);
   }
   return true;
}



/*
 * Write an end of file on the device
 *   Returns: true on success
 *            false on failure
 */
bool DEVICE::weof(int num)
{
   struct mtop mt_com;
   int stat;
   Dmsg0(129, "weof_dev\n");
   
   if (fd < 0) {
      dev_errno = EBADF;
      Mmsg0(errmsg, _("Bad call to weof_dev. Device not open\n"));
      Emsg0(M_FATAL, 0, errmsg);
      return false;
   }
   file_size = 0;

   if (!is_tape()) {
      return true;
   }
   if (!can_append()) {
      Mmsg0(errmsg, _("Attempt to WEOF on non-appendable Volume\n"));
      Emsg0(M_FATAL, 0, errmsg);
      return false;
   }
      
   clear_eof();
   clear_eot();
   mt_com.mt_op = MTWEOF;
   mt_com.mt_count = num;
   stat = ioctl(fd, MTIOCTOP, (char *)&mt_com);
   if (stat == 0) {
      block_num = 0;
      file += num;
      file_addr = 0;
   } else {
      berrno be;
      clrerror(MTWEOF);
      if (stat == -1) {
         Mmsg2(errmsg, _("ioctl MTWEOF error on %s. ERR=%s.\n"),
            print_name(), be.strerror());
       }
   }
   return stat == 0;
}


/*
 * If implemented in system, clear the tape
 * error status.
 */
void DEVICE::clrerror(int func)
{
   const char *msg = NULL;
   struct mtget mt_stat;
   char buf[100];

   dev_errno = errno;         /* save errno */
   if (errno == EIO) {
      VolCatInfo.VolCatErrors++;
   }

   if (!is_tape()) {
      return;
   }
   if (errno == ENOTTY || errno == ENOSYS) { /* Function not implemented */
      switch (func) {
      case -1:
         Emsg0(M_ABORT, 0, _("Got ENOTTY on read/write!\n"));
         break;
      case MTWEOF:
         msg = "WTWEOF";
         capabilities &= ~CAP_EOF; /* turn off feature */
         break;
#ifdef MTEOM
      case MTEOM:
         msg = "WTEOM";
         capabilities &= ~CAP_EOM; /* turn off feature */
         break;
#endif
      case MTFSF:
         msg = "MTFSF";
         capabilities &= ~CAP_FSF; /* turn off feature */
         break;
      case MTBSF:
         msg = "MTBSF";
         capabilities &= ~CAP_BSF; /* turn off feature */
         break;
      case MTFSR:
         msg = "MTFSR";
         capabilities &= ~CAP_FSR; /* turn off feature */
         break;
      case MTBSR:
         msg = "MTBSR";
         capabilities &= ~CAP_BSR; /* turn off feature */
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
#ifdef MTLOAD
      case MTLOAD:
         msg = "MTLOAD";
         break;
#endif
#ifdef MTUNLOCK
      case MTUNLOCK:
         msg = "MTUNLOCK";
         break;
#endif
      case MTOFFL:
         msg = "MTOFFL";
         break;
      default:
         bsnprintf(buf, sizeof(buf), _("unknown func code %d"), func);
         msg = buf;
         break;
      }
      if (msg != NULL) {
         dev_errno = ENOSYS;
         Mmsg1(errmsg, _("I/O function \"%s\" not supported on this device.\n"), msg);
         Emsg0(M_ERROR, 0, errmsg);
      }
   }

   /*
    * Now we try different methods of clearing the error
    *  status on the drive so that it is not locked for
    *  further operations.
    */

   /* On some systems such as NetBSD, this clears all errors */
   ioctl(fd, MTIOCGET, (char *)&mt_stat);

/* Found on Linux */
#ifdef MTIOCLRERR
{
   struct mtop mt_com;
   mt_com.mt_op = MTIOCLRERR;
   mt_com.mt_count = 1;
   /* Clear any error condition on the tape */
   ioctl(fd, MTIOCTOP, (char *)&mt_com);
   Dmsg0(200, "Did MTIOCLRERR\n");
}
#endif

/* Typically on FreeBSD */
#ifdef MTIOCERRSTAT
{
   /* Read and clear SCSI error status */
   union mterrstat mt_errstat;
   Dmsg2(200, "Doing MTIOCERRSTAT errno=%d ERR=%s\n", dev_errno,
      strerror(dev_errno));
   ioctl(fd, MTIOCERRSTAT, (char *)&mt_errstat);
}
#endif

/* Clear Subsystem Exception OSF1 */
#ifdef MTCSE
{
   struct mtop mt_com;
   mt_com.mt_op = MTCSE;
   mt_com.mt_count = 1;
   /* Clear any error condition on the tape */
   ioctl(fd, MTIOCTOP, (char *)&mt_com);
   Dmsg0(200, "Did MTCSE\n");
}
#endif
}

/*
 * Close the device
 */
void DEVICE::close()
{
   Dmsg1(100, "close_dev %s\n", print_name());
   if (has_cap(CAP_OFFLINEUNMOUNT)) {
      offline();
   }
   if (fd >= 0) {
      ::close(fd);
   } else {
      Dmsg2(100, "device %s already closed vol=%s\n", print_name(),
         VolHdr.VolumeName);
      return;                         /* already closed */
   }

   if (is_dvd() && !unmount_dvd(this, 1)) {
      Dmsg1(0, "Cannot unmount device %s.\n", print_name());
   }
   
   /* Remove the last part file if it is empty */
   if (num_parts > 0) {
      struct stat statp;
      POOL_MEM archive_name(PM_FNAME);
      part = num_parts;
      Dmsg1(100, "Call make_dvd_filename. Vol=%s\n", VolCatInfo.VolCatName);
      make_spooled_dvd_filename(this, archive_name);
      /* Check that the part file is empty */
      if ((stat(archive_name.c_str(), &statp) == 0) && (statp.st_size == 0)) {
         Dmsg1(100, "unlink(%s)\n", archive_name.c_str());
         unlink(archive_name.c_str());
      }
   }
   
   /* Clean up device packet so it can be reused */
   clear_opened();
   state &= ~(ST_LABEL|ST_READ|ST_APPEND|ST_EOT|ST_WEOT|ST_EOF);
   label_type = B_BACULA_LABEL;
   file = block_num = 0;
   file_size = 0;
   file_addr = 0;
   part = 0;
   num_parts = 0;
   part_size = 0;
   part_start = 0;
   EndFile = EndBlock = 0;
   free_volume(this);
   memset(&VolCatInfo, 0, sizeof(VolCatInfo));
   memset(&VolHdr, 0, sizeof(VolHdr));
   if (tid) {
      stop_thread_timer(tid);
      tid = 0;
   }
   openmode = 0;
}



bool DEVICE::truncate(DCR *dcr) /* We need the DCR for DVD-writing */
{
   Dmsg1(100, "truncate_dev %s\n", print_name());
   if (is_tape()) {
      return true;                    /* we don't really truncate tapes */
      /* maybe we should rewind and write and eof ???? */
   }
   
   if (is_dvd()) {
      return truncate_dvd(dcr);
   }
   
   if (ftruncate(fd, 0) != 0) {
      berrno be;
      Mmsg2(errmsg, _("Unable to truncate device %s. ERR=%s\n"), 
            print_name(), be.strerror());
      return false;
   }
   return true;
}

/* Mount the device.
 * If timeout, wait until the mount command returns 0.
 * If !timeout, try to mount the device only once.
 */
bool DEVICE::mount(int timeout) 
{
   Dmsg0(190, "Enter mount\n");
   if (is_mounted()) {
      return true;
   } else if (requires_mount()) {
      return do_mount(1, timeout);
   }       
   return true;
}

/* Unmount the device
 * If timeout, wait until the unmount command returns 0.
 * If !timeout, try to unmount the device only once.
 */
bool DEVICE::unmount(int timeout) 
{
   Dmsg0(90, "Enter unmount_dvd\n");
   if (is_mounted()) {
      return do_mount(0, timeout);
   }
   return true;
}

/* (Un)mount the device */
bool DEVICE::do_mount(int mount, int dotimeout) 
{
   POOL_MEM ocmd(PM_FNAME);
   POOLMEM *results;
   char *icmd;
   int status, timeout;
   
   sm_check(__FILE__, __LINE__, false);
   if (mount) {
      if (is_mounted()) {
         Dmsg0(200, "======= mount=1\n");
         return true;
      }
      icmd = device->mount_command;
   } else {
      if (!is_mounted()) {
         Dmsg0(200, "======= mount=0\n");
         return true;
      }
      icmd = device->unmount_command;
   }
   
   edit_mount_codes(ocmd, icmd);
   
   Dmsg2(000, "do_mount_dvd: cmd=%s mounted=%d\n", ocmd.c_str(), !!is_mounted());

   if (dotimeout) {
      /* Try at most 1 time to (un)mount the device. This should perhaps be configurable. */
      timeout = 1;
   } else {
      timeout = 0;
   }
   results = get_memory(2000);
   results[0] = 0;
   /* If busy retry each second */
   while ((status = run_program_full_output(ocmd.c_str(), 
                       max_open_wait/2, results)) != 0) {
      /* Doesn't work with internationalisation (This is not a problem) */
      if (fnmatch("*is already mounted on", results, 0) == 0) {
         break;
      }
      if (timeout-- > 0) {
         /* Sometimes the device cannot be mounted because it is already mounted.
          * Try to unmount it, then remount it */
         if (mount) {
            Dmsg1(400, "Trying to unmount the device %s...\n", print_name());
            do_mount(0, 0);
         }
         bmicrosleep(1, 0);
         continue;
      }
      Dmsg2(40, "Device %s cannot be mounted. ERR=%s\n", print_name(), results);
      Mmsg(errmsg, _("Device %s cannot be mounted. ERR=%s\n"), 
           print_name(), results);
      /*
       * Now, just to be sure it is not mounted, try to read the
       *  filesystem.
       */
      DIR* dp;
      struct dirent *entry, *result;
      int name_max;
      int count;
      
      name_max = pathconf(".", _PC_NAME_MAX);
      if (name_max < 1024) {
         name_max = 1024;
      }
         
      if (!(dp = opendir(device->mount_point))) {
         berrno be;
         dev_errno = errno;
         Dmsg3(29, "do_mount: failed to open dir %s (dev=%s), ERR=%s\n", 
               device->mount_point, print_name(), be.strerror());
         goto get_out;
      }
      
      entry = (struct dirent *)malloc(sizeof(struct dirent) + name_max + 1000);
      count = 0;
      while (1) {
         if ((readdir_r(dp, entry, &result) != 0) || (result == NULL)) {
            dev_errno = EIO;
            Dmsg2(129, "do_mount: failed to find suitable file in dir %s (dev=%s)\n", 
                  device->mount_point, print_name());
            break;
         }
         if ((strcmp(result->d_name, ".")) && (strcmp(result->d_name, "..")) && (strcmp(result->d_name, ".keep"))) {
            count++; /* result->d_name != ., .. or .keep (Gentoo-specific) */
            break;
         } else {
            Dmsg2(129, "do_mount: ignoring %s in %s\n", result->d_name, device->mount_point);
         }
      }
      free(entry);
      closedir(dp);
      
      Dmsg1(29, "do_mount: got %d files in the mount point (not counting ., .. and .keep)\n", count);
      
      if (count > 0) {
         mount = 1;                      /* If we got more than ., .. and .keep */
         break;                          /*   there must be something mounted */
      }
get_out:
      set_mounted(false);
      sm_check(__FILE__, __LINE__, false);
      free_pool_memory(results);
      Dmsg0(200, "============ mount=0\n");
      return false;
   }
   
   set_mounted(mount);              /* set/clear mounted flag */
   free_pool_memory(results);
   Dmsg1(200, "============ mount=%d\n", mount);
   return true;
}

/*
 * Edit codes into (Un)MountCommand, Write(First)PartCommand
 *  %% = %
 *  %a = archive device name
 *  %e = erase (set if cannot mount and first part)
 *  %n = part number
 *  %m = mount point
 *  %v = last part name
 *
 *  omsg = edited output message
 *  imsg = input string containing edit codes (%x)
 *
 */
void DEVICE::edit_mount_codes(POOL_MEM &omsg, const char *imsg)
{
   const char *p;
   const char *str;
   char add[20];
   
   POOL_MEM archive_name(PM_FNAME);

   omsg.c_str()[0] = 0;
   Dmsg1(800, "edit_mount_codes: %s\n", imsg);
   for (p=imsg; *p; p++) {
      if (*p == '%') {
         switch (*++p) {
         case '%':
            str = "%";
            break;
         case 'a':
            str = dev_name;
            break;
         case 'e':
            if (num_parts == 0) {
               str = "1";
            } else {
               str = "0";
            }
            break;
         case 'n':
            bsnprintf(add, sizeof(add), "%d", part);
            str = add;
            break;
         case 'm':
            str = device->mount_point;
            break;
         case 'v':
            make_spooled_dvd_filename(this, archive_name);
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
      pm_strcat(omsg, (char *)str);
      Dmsg1(1800, "omsg=%s\n", omsg.c_str());
   }
}


/* Return the resource name for the device */
const char *DEVICE::name() const
{
   return device->hdr.name;
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
void DEVICE::term(void)
{
   Dmsg1(900, "term dev: %s\n", print_name());
   close();
   if (dev_name) {
      free_memory(dev_name);
      dev_name = NULL;
   }
   if (prt_name) {
      free_memory(prt_name);
      prt_name = NULL;
   }
   if (errmsg) {
      free_pool_memory(errmsg);
      errmsg = NULL;
   }
   pthread_mutex_destroy(&mutex);
   pthread_cond_destroy(&wait);
   pthread_cond_destroy(&wait_next_vol);
   pthread_mutex_destroy(&spool_mutex);
   rwl_destroy(&lock);
   if (attached_dcrs) {
      delete attached_dcrs;
      attached_dcrs = NULL;
   }
   if (device) {
      device->dev = NULL;
   }
   free((char *)this);
}

/*
 * This routine initializes the device wait timers
 */
void init_device_wait_timers(DCR *dcr)
{
   DEVICE *dev = dcr->dev;
   JCR *jcr = dcr->jcr;

   /* ******FIXME******* put these on config variables */
   dev->min_wait = 60 * 60;
   dev->max_wait = 24 * 60 * 60;
   dev->max_num_wait = 9;              /* 5 waits =~ 1 day, then 1 day at a time */
   dev->wait_sec = dev->min_wait;
   dev->rem_wait_sec = dev->wait_sec;
   dev->num_wait = 0;
   dev->poll = false;
   dev->BadVolName[0] = 0;

   jcr->min_wait = 60 * 60;
   jcr->max_wait = 24 * 60 * 60;
   jcr->max_num_wait = 9;              /* 5 waits =~ 1 day, then 1 day at a time */
   jcr->wait_sec = jcr->min_wait;
   jcr->rem_wait_sec = jcr->wait_sec;
   jcr->num_wait = 0;

}

void init_jcr_device_wait_timers(JCR *jcr)
{
   /* ******FIXME******* put these on config variables */
   jcr->min_wait = 60 * 60;
   jcr->max_wait = 24 * 60 * 60;
   jcr->max_num_wait = 9;              /* 5 waits =~ 1 day, then 1 day at a time */
   jcr->wait_sec = jcr->min_wait;
   jcr->rem_wait_sec = jcr->wait_sec;
   jcr->num_wait = 0;
}


/*
 * The dev timers are used for waiting on a particular device 
 *
 * Returns: true if time doubled
 *          false if max time expired
 */
bool double_dev_wait_time(DEVICE *dev)
{
   dev->wait_sec *= 2;               /* double wait time */
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
         dev->clrerror(MTSETBLK);
      }
      mt_com.mt_op = MTSETDRVBUFFER;
      mt_com.mt_count = MT_ST_CLEARBOOLEANS;
      if (!dev->has_cap(CAP_TWOEOF)) {
         mt_com.mt_count |= MT_ST_TWO_FM;
      }
      if (dev->has_cap(CAP_EOM)) {
         mt_com.mt_count |= MT_ST_FAST_MTEOM;
      }
      if (ioctl(dev->fd, MTIOCTOP, (char *)&mt_com) < 0) {
         dev->clrerror(MTSETBLK);
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
         dev->clrerror(MTSETBSIZ);
      }
      /* Get notified at logical end of tape */
      mt_com.mt_op = MTEWARN;
      mt_com.mt_count = 1;
      if (ioctl(dev->fd, MTIOCTOP, (char *)&mt_com) < 0) {
         dev->clrerror(MTEWARN);
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
         dev->clrerror(MTSETBSIZ);
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
         dev->clrerror(MTSRSZ);
      }
   }
   return;
#endif
}

static bool dev_get_os_pos(DEVICE *dev, struct mtget *mt_stat)
{
   return dev->has_cap(CAP_MTIOCGET) && 
          ioctl(dev->fd, MTIOCGET, (char *)mt_stat) == 0 &&
          mt_stat->mt_fileno >= 0;
}

static char *modes[] = {
   "CREATE_READ_WRITE",
   "OPEN_READ_WRITE",
   "OPEN_READ_ONLY",
   "OPEN_WRITE_ONLY"
};


static char *mode_to_str(int mode)  
{
   return modes[mode-1];
}
