/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2000-2014 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from many
   others, a complete list can be found in the file AUTHORS.

   You may use this file and others of this release according to the
   license defined in the LICENSE file, which includes the Affero General
   Public License, v3.0 ("AGPLv3") and some additional permissions and
   terms pursuant to its AGPLv3 Section 7.

   Bacula® is a registered trademark of Kern Sibbald.
*/
/*
 *
 *   tape_dev.c  -- low level operations on tape devices
 *
 *     written by, Kern Sibbald, MM
 *     separated from dev.c in February 2014
 *
 *   The separation between tape and file is not yet clean.
 *
 */

/*
 * Handling I/O errors and end of tape conditions are a bit tricky.
 * This is how it is currently done when writing.
 * On either an I/O error or end of tape,
 * we will stop writing on the physical device (no I/O recovery is
 * attempted at least in this daemon). The state flag will be sent
 * to include ST_EOT, which is ephemeral, and ST_WEOT, which is
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

/* Imported functions */
extern void set_os_device_parameters(DCR *dcr);
extern bool dev_get_os_pos(DEVICE *dev, struct mtget *mt_stat);
extern uint32_t status_dev(DEVICE *dev);
const char *mode_to_str(int mode);

/*
 */
void DEVICE::open_tape_device(DCR *dcr, int omode)
{
   file_size = 0;
   int timeout = max_open_wait;
#if !defined(HAVE_WIN32)
   struct mtop mt_com;
   utime_t start_time = time(NULL);
#endif

   mount(1);                          /* do mount if required */

   Dmsg0(100, "Open dev: device is tape\n");

   get_autochanger_loaded_slot(dcr);

   openmode = omode;
   set_mode(omode);

   if (timeout < 1) {
      timeout = 1;
   }
   errno = 0;
   if (is_fifo() && timeout) {
      /* Set open timer */
      tid = start_thread_timer(dcr->jcr, pthread_self(), timeout);
   }
   Dmsg2(100, "Try open %s mode=%s\n", print_name(), mode_to_str(omode));
#if defined(HAVE_WIN32)

   /*   Windows Code */
   if ((m_fd = d_open(dev_name, mode)) < 0) {
      dev_errno = errno;
   }

#else

   /*  UNIX  Code */
   /* If busy retry each second for max_open_wait seconds */
   for ( ;; ) {
      /* Try non-blocking open */
      m_fd = d_open(dev_name, mode+O_NONBLOCK);
      if (m_fd < 0) {
         berrno be;
         dev_errno = errno;
         Dmsg5(100, "Open error on %s omode=%d mode=%x errno=%d: ERR=%s\n",
              print_name(), omode, mode, errno, be.bstrerror());
      } else {
         /* Tape open, now rewind it */
         Dmsg0(100, "Rewind after open\n");
         mt_com.mt_op = MTREW;
         mt_com.mt_count = 1;
         /* rewind only if dev is a tape */
         if (is_tape() && (d_ioctl(m_fd, MTIOCTOP, (char *)&mt_com) < 0)) {
            berrno be;
            dev_errno = errno;           /* set error status from rewind */
            d_close(m_fd);
            clear_opened();
            Dmsg2(100, "Rewind error on %s close: ERR=%s\n", print_name(),
                  be.bstrerror(dev_errno));
            /* If we get busy, device is probably rewinding, try again */
            if (dev_errno != EBUSY) {
               break;                    /* error -- no medium */
            }
         } else {
            /* Got fd and rewind worked, so we must have medium in drive */
            d_close(m_fd);
            m_fd = d_open(dev_name, mode);  /* open normally */
            if (m_fd < 0) {
               berrno be;
               dev_errno = errno;
               Dmsg5(100, "Open error on %s omode=%d mode=%x errno=%d: ERR=%s\n",
                     print_name(), omode, mode, errno, be.bstrerror());
               break;
            }
            dev_errno = 0;
            lock_door();
            set_os_device_parameters(dcr);       /* do system dependent stuff */
            break;                               /* Successfully opened and rewound */
         }
      }
      bmicrosleep(5, 0);
      /* Exceed wait time ? */
      if (time(NULL) - start_time >= max_open_wait) {
         break;                       /* yes, get out */
      }
   }
#endif

   if (!is_open()) {
      berrno be;
      Mmsg2(errmsg, _("Unable to open device %s: ERR=%s\n"),
            print_name(), be.bstrerror(dev_errno));
      Dmsg1(100, "%s", errmsg);
   }

   /* Stop any open() timer we started */
   if (tid) {
      stop_thread_timer(tid);
      tid = 0;
   }
   Dmsg1(100, "open dev: tape %d opened\n", m_fd);
}


/*
 * Rewind the device.
 *  Returns: true  on success
 *           false on failure
 */
bool tape_dev::rewind(DCR *dcr)
{
   struct mtop mt_com;
   unsigned int i;
   bool first = true;

   Dmsg3(400, "rewind res=%d fd=%d %s\n", num_reserved(), m_fd, print_name());
   state &= ~(ST_EOT|ST_EOF|ST_WEOT);  /* remove EOF/EOT flags */
   block_num = file = 0;
   file_size = 0;
   file_addr = 0;
   if (m_fd < 0) {
      return false;
   }
   if (is_tape()) {
      mt_com.mt_op = MTREW;
      mt_com.mt_count = 1;
      /* If we get an I/O error on rewind, it is probably because
       * the drive is actually busy. We loop for (about 5 minutes)
       * retrying every 5 seconds.
       */
      for (i=max_rewind_wait; ; i -= 5) {
         if (d_ioctl(m_fd, MTIOCTOP, (char *)&mt_com) < 0) {
            berrno be;
            clrerror(MTREW);
            if (i == max_rewind_wait) {
               Dmsg1(200, "Rewind error, %s. retrying ...\n", be.bstrerror());
            }
            /*
             * This is a gross hack, because if the user has the
             *   device mounted (i.e. open), then uses mtx to load
             *   a tape, the current open file descriptor is invalid.
             *   So, we close the drive and re-open it.
             */
            if (first && dcr) {
               int open_mode = openmode;
               d_close(m_fd);
               clear_opened();
               open(dcr, open_mode);
               if (m_fd < 0) {
                  return false;
               }
               first = false;
               continue;
            }
#ifdef HAVE_SUN_OS
            if (dev_errno == EIO) {
               Mmsg1(errmsg, _("No tape loaded or drive offline on %s.\n"), print_name());
               return false;
            }
#else
            if (dev_errno == EIO && i > 0) {
               Dmsg0(200, "Sleeping 5 seconds.\n");
               bmicrosleep(5, 0);
               continue;
            }
#endif
            Mmsg2(errmsg, _("Rewind error on %s. ERR=%s.\n"),
               print_name(), be.bstrerror());
            return false;
         }
         break;
      }
   }
   return true;
}

/*
 * Position device to end of medium (end of data)
 *  Returns: true  on succes
 *           false on error
 */
bool DEVICE::eod(DCR *dcr)
{
   struct mtop mt_com;
   bool ok = true;
   boffset_t pos;
   int32_t os_file;

   Enter(100);
   if (m_fd < 0) {
      dev_errno = EBADF;
      Mmsg1(errmsg, _("Bad call to eod. Device %s not open\n"), print_name());
      Dmsg1(100, "%s", errmsg);
      return false;
   }

#if defined (__digital__) && defined (__unix__)
   return fsf(VolCatInfo.VolCatFiles);
#endif

   if (at_eot()) {
      Leave(100);
      return true;
   }
   clear_eof();         /* remove EOF flag */
   block_num = file = 0;
   file_size = 0;
   file_addr = 0;
   if (is_fifo()) {
      Leave(100);
      return true;
   }
   if (!is_tape()) {
      pos = lseek(dcr, (boffset_t)0, SEEK_END);
      Dmsg1(200, "====== Seek to %lld\n", pos);
      if (pos >= 0) {
         update_pos(dcr);
         set_eot();
         Leave(100);
         return true;
      }
      dev_errno = errno;
      berrno be;
      Mmsg2(errmsg, _("lseek error on %s. ERR=%s.\n"),
             print_name(), be.bstrerror());
      Dmsg1(100, "%s", errmsg);
      Leave(100);
      return false;
   }
#ifdef MTEOM
   if (has_cap(CAP_FASTFSF) && !has_cap(CAP_EOM)) {
      Dmsg0(100,"Using FAST FSF for EOM\n");
      /* If unknown position, rewind */
      if (get_os_tape_file() < 0) {
        if (!rewind(NULL)) {
          Dmsg0(100, "Rewind error\n");
          Leave(100);
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

      if (d_ioctl(m_fd, MTIOCTOP, (char *)&mt_com) < 0) {
         berrno be;
         clrerror(mt_com.mt_op);
         Dmsg1(50, "ioctl error: %s\n", be.bstrerror());
         update_pos(dcr);
         Mmsg2(errmsg, _("ioctl MTEOM error on %s. ERR=%s.\n"),
            print_name(), be.bstrerror());
         Dmsg1(100, "%s", errmsg);
         Leave(100);
         return false;
      }

      os_file = get_os_tape_file();
      if (os_file < 0) {
         berrno be;
         clrerror(-1);
         Mmsg2(errmsg, _("ioctl MTIOCGET error on %s. ERR=%s.\n"),
            print_name(), be.bstrerror());
         Dmsg1(100, "%s", errmsg);
         Leave(100);
         return false;
      }
      Dmsg1(100, "EOD file=%d\n", os_file);
      set_ateof();
      file = os_file;
   } else {
#else
   {
#endif
      /*
       * Rewind then use FSF until EOT reached
       */
      if (!rewind(NULL)) {
         Dmsg0(100, "Rewind error.\n");
         Leave(100);
         return false;
      }
      /*
       * Move file by file to the end of the tape
       */
      int file_num;
      for (file_num=file; !at_eot(); file_num++) {
         Dmsg0(200, "eod: doing fsf 1\n");
         if (!fsf(1)) {
            Dmsg0(100, "fsf error.\n");
            Leave(100);
            return false;
         }
         /*
          * Avoid infinite loop by ensuring we advance.
          */
         if (!at_eot() && file_num == (int)file) {
            Dmsg1(100, "fsf did not advance from file %d\n", file_num);
            set_ateof();
            os_file = get_os_tape_file();
            if (os_file >= 0) {
               Dmsg2(100, "Adjust file from %d to %d\n", file_num, os_file);
               file = os_file;
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
      /* Backup over EOF */
      ok = bsf(1);
      /* If BSF worked and fileno is known (not -1), set file */
      os_file = get_os_tape_file();
      if (os_file >= 0) {
         Dmsg2(100, "BSFATEOF adjust file from %d to %d\n", file , os_file);
         file = os_file;
      } else {
         file++;                       /* wing it -- not correct on all OSes */
      }
   } else {
      update_pos(dcr);                 /* update position */
   }
   Dmsg1(200, "EOD dev->file=%d\n", file);
   Leave(100);
   return ok;
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

   if (dev->fd() < 0) {
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
         dev->print_name(), be.bstrerror());
   return false;
#else

   dev->block_num = dev->file = 0;
   dev->file_size = 0;
   dev->file_addr = 0;
   mt_com.mt_op = MTLOAD;
   mt_com.mt_count = 1;
   if (dev->d_ioctl(dev->fd(), MTIOCTOP, (char *)&mt_com) < 0) {
      berrno be;
      dev->dev_errno = errno;
      Mmsg2(dev->errmsg, _("ioctl MTLOAD error on %s. ERR=%s.\n"),
         dev->print_name(), be.bstrerror());
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
bool tape_dev::offline()
{
   struct mtop mt_com;

   if (!is_tape()) {
      return true;                    /* device not open */
   }

   state &= ~(ST_APPEND|ST_READ|ST_EOT|ST_EOF|ST_WEOT);  /* remove EOF/EOT flags */
   block_num = file = 0;
   file_size = 0;
   file_addr = 0;
   unlock_door();
   mt_com.mt_op = MTOFFL;
   mt_com.mt_count = 1;
   if (d_ioctl(m_fd, MTIOCTOP, (char *)&mt_com) < 0) {
      berrno be;
      dev_errno = errno;
      Mmsg2(errmsg, _("ioctl MTOFFL error on %s. ERR=%s.\n"),
         print_name(), be.bstrerror());
      return false;
   }
   Dmsg1(100, "Offlined device %s\n", print_name());
   return true;
}

bool DEVICE::offline_or_rewind()
{
   if (m_fd < 0) {
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
bool tape_dev::fsf(int num)
{
   int32_t os_file = 0;
   struct mtop mt_com;
   int stat = 0;

   if (!is_open()) {
      dev_errno = EBADF;
      Mmsg0(errmsg, _("Bad call to fsf. Device not open\n"));
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
      int my_errno = 0;
      mt_com.mt_op = MTFSF;
      mt_com.mt_count = num;
      stat = d_ioctl(m_fd, MTIOCTOP, (char *)&mt_com);
      if (stat < 0) {
         my_errno = errno;            /* save errno */
      } else if ((os_file=get_os_tape_file()) < 0) {
         my_errno = errno;            /* save errno */
      }
      if (my_errno != 0) {
         berrno be;
         set_eot();
         Dmsg0(200, "Set ST_EOT\n");
         clrerror(MTFSF);
         Mmsg2(errmsg, _("ioctl MTFSF error on %s. ERR=%s.\n"),
            print_name(), be.bstrerror(my_errno));
         Dmsg1(200, "%s", errmsg);
         return false;
      }

      Dmsg1(200, "fsf file=%d\n", os_file);
      set_ateof();
      file = os_file;
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
         if ((stat = this->read((char *)rbuf, rbuf_len)) < 0) {
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
                  be.bstrerror());
               Mmsg2(errmsg, _("read error on %s. ERR=%s.\n"),
                  print_name(), be.bstrerror());
               Dmsg1(100, "%s", errmsg);
               break;
            }
         }
         if (stat == 0) {                /* EOF */
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
         stat = d_ioctl(m_fd, MTIOCTOP, (char *)&mt_com);
         if (stat < 0) {                 /* error => EOT */
            berrno be;
            set_eot();
            Dmsg0(100, "Set ST_EOT\n");
            clrerror(MTFSF);
            Mmsg2(errmsg, _("ioctl MTFSF error on %s. ERR=%s.\n"),
               print_name(), be.bstrerror());
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
bool tape_dev::bsf(int num)
{
   struct mtop mt_com;
   int stat;

   if (!is_open()) {
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

   Dmsg0(100, "bsf\n");
   clear_eot();
   clear_eof();
   file -= num;
   file_addr = 0;
   file_size = 0;
   mt_com.mt_op = MTBSF;
   mt_com.mt_count = num;
   stat = d_ioctl(m_fd, MTIOCTOP, (char *)&mt_com);
   if (stat < 0) {
      berrno be;
      clrerror(MTBSF);
      Mmsg2(errmsg, _("ioctl MTBSF error on %s. ERR=%s.\n"),
         print_name(), be.bstrerror());
   }
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

   if (!is_open()) {
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

   Dmsg1(100, "fsr %d\n", num);
   mt_com.mt_op = MTFSR;
   mt_com.mt_count = num;
   stat = d_ioctl(m_fd, MTIOCTOP, (char *)&mt_com);
   if (stat == 0) {
      clear_eof();
      block_num += num;
   } else {
      berrno be;
      struct mtget mt_stat;
      clrerror(MTFSR);
      Dmsg1(100, "FSF fail: ERR=%s\n", be.bstrerror());
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
         num, print_name(), be.bstrerror());
   }
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

   if (!is_open()) {
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

   Dmsg0(100, "bsr_dev\n");
   block_num -= num;
   clear_eof();
   clear_eot();
   mt_com.mt_op = MTBSR;
   mt_com.mt_count = num;
   stat = d_ioctl(m_fd, MTIOCTOP, (char *)&mt_com);
   if (stat < 0) {
      berrno be;
      clrerror(MTBSR);
      Mmsg2(errmsg, _("ioctl MTBSR error on %s. ERR=%s.\n"),
         print_name(), be.bstrerror());
   }
   return stat == 0;
}

void tape_dev::lock_door()
{
#ifdef MTLOCK
   struct mtop mt_com;
   if (!is_tape()) return;
   mt_com.mt_op = MTLOCK;
   mt_com.mt_count = 1;
   d_ioctl(m_fd, MTIOCTOP, (char *)&mt_com);
#endif
}

void tape_dev::unlock_door()
{
#ifdef MTUNLOCK
   struct mtop mt_com;
   if (!is_tape()) return;
   mt_com.mt_op = MTUNLOCK;
   mt_com.mt_count = 1;
   d_ioctl(m_fd, MTIOCTOP, (char *)&mt_com);
#endif
}

/*
 * Reposition the device to file, block
 * Returns: false on failure
 *          true  on success
 */
bool tape_dev::reposition(DCR *dcr, uint32_t rfile, uint32_t rblock)
{
   if (!is_open()) {
      dev_errno = EBADF;
      Mmsg0(errmsg, _("Bad call to reposition. Device not open\n"));
      Emsg0(M_FATAL, 0, errmsg);
      return false;
   }

   /* After this point, we are tape only */
   Dmsg4(100, "reposition from %u:%u to %u:%u\n", file, block_num, rfile, rblock);
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
      Dmsg0(100, "fsf 1\n");
      fsf(1);
      Dmsg2(100, "wanted_blk=%d at_blk=%d\n", rblock, block_num);
   }
   if (has_cap(CAP_POSITIONBLOCKS) && rblock > block_num) {
      /* Ignore errors as Bacula can read to the correct block */
      Dmsg1(100, "fsr %d\n", rblock-block_num);
      return fsr(rblock-block_num);
   } else {
      while (rblock > block_num) {
         if (!dcr->read_block_from_dev(NO_BLOCK_NUMBER_CHECK)) {
            berrno be;
            dev_errno = errno;
            Dmsg2(30, "Failed to find requested block on %s: ERR=%s",
               print_name(), be.bstrerror());
            return false;
         }
         Dmsg2(300, "moving forward wanted_blk=%d at_blk=%d\n", rblock, block_num);
      }
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
   Dmsg1(129, "=== weof_dev=%s\n", print_name());

   if (!is_open()) {
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
   stat = d_ioctl(m_fd, MTIOCTOP, (char *)&mt_com);
   if (stat == 0) {
      block_num = 0;
      file += num;
      file_addr = 0;
   } else {
      berrno be;
      clrerror(MTWEOF);
      if (stat == -1) {
         Mmsg2(errmsg, _("ioctl MTWEOF error on %s. ERR=%s.\n"),
            print_name(), be.bstrerror());
       }
   }
   return stat == 0;
}

/*
 * (Un)mount the device (for tape devices)
 */
bool DEVICE::do_tape_mount(int mount, int dotimeout)
{
   POOL_MEM ocmd(PM_FNAME);
   POOLMEM *results;
   char *icmd;
   int status, tries;
   berrno be;

   Dsm_check(200);
   if (mount) {
      icmd = device->mount_command;
   } else {
      icmd = device->unmount_command;
   }

   edit_mount_codes(ocmd, icmd);

   Dmsg2(100, "do_tape_mount: cmd=%s mounted=%d\n", ocmd.c_str(), !!is_mounted());

   if (dotimeout) {
      /* Try at most 10 times to (un)mount the device. This should perhaps be configurable. */
      tries = 10;
   } else {
      tries = 1;
   }
   results = get_memory(4000);

   /* If busy retry each second */
   Dmsg1(100, "do_tape_mount run_prog=%s\n", ocmd.c_str());
   while ((status = run_program_full_output(ocmd.c_str(), max_open_wait/2, results)) != 0) {
      if (tries-- > 0) {
         continue;
      }

      Dmsg5(100, "Device %s cannot be %smounted. stat=%d result=%s ERR=%s\n", print_name(),
           (mount ? "" : "un"), status, results, be.bstrerror(status));
      Mmsg(errmsg, _("Device %s cannot be %smounted. ERR=%s\n"),
           print_name(), (mount ? "" : "un"), be.bstrerror(status));

      set_mounted(false);
      free_pool_memory(results);
      Dmsg0(200, "============ mount=0\n");
      Dsm_check(200);
      return false;
   }

   set_mounted(mount);              /* set/clear mounted flag */
   free_pool_memory(results);
   Dmsg1(200, "============ mount=%d\n", mount);
   return true;
}
