/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2001-2014 Free Software Foundation Europe e.V.

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
 *   block.c -- tape block handling functions
 *
 *              Kern Sibbald, March MMI
 *                 added BB02 format October MMII
 *
 */


#include "bacula.h"
#include "stored.h"

#ifdef DEBUG_BLOCK_CHECKSUM
static const bool debug_block_checksum = true;
#else
static const bool debug_block_checksum = false;
#endif

#ifdef NO_TAPE_WRITE_TEST
static const bool no_tape_write_test = true;
#else
static const bool no_tape_write_test = false;
#endif


bool do_new_file_bookkeeping(DCR *dcr);
//bool do_dvd_size_checks(DCR *dcr);
void reread_last_block(DCR *dcr);
uint32_t get_len_and_clear_block(DEV_BLOCK *block, DEVICE *dev, uint32_t &pad);
uint32_t ser_block_header(DEV_BLOCK *block, bool do_checksum);
bool unser_block_header(JCR *jcr, DEVICE *dev, DEV_BLOCK *block);

/*
 * Write a block to the device, with locking and unlocking
 *
 * Returns: true  on success
 *        : false on failure
 *
 */
bool DCR::write_block_to_device()
{
   bool stat = true;
   DCR *dcr = this;

   if (dcr->spooling) {
      Dmsg0(200, "Write to spool\n");
      stat = write_block_to_spool_file(dcr);
      return stat;
   }

   if (!is_dev_locked()) {        /* device already locked? */
      /* note, do not change this to dcr->rLock */
      dev->rLock(false);          /* no, lock it */
   }

   if (!check_for_newvol_or_newfile(dcr)) {
      stat = false;
      goto bail_out;   /* fatal error */
   }

   Dmsg1(500, "Write block to dev=%p\n", dcr->dev);
   if (!write_block_to_dev()) {
      if (job_canceled(jcr) || jcr->getJobType() == JT_SYSTEM) {
         stat = false;
         Dmsg2(40, "cancel=%d or SYSTEM=%d\n", job_canceled(jcr),
            jcr->getJobType() == JT_SYSTEM);
      } else {
         Dmsg0(40, "Calling fixup_device ...\n");
         stat = fixup_device_block_write_error(dcr);
      }
   }

bail_out:
   if (!dcr->is_dev_locked()) {        /* did we lock dev above? */
      /* note, do not change this to dcr->dunlock */
      dev->Unlock();                  /* unlock it now */
   }
   return stat;
}

/*
 * Write a block to the device
 *
 *  Returns: true  on success or EOT
 *           false on hard error
 */
bool DCR::write_block_to_dev()
{
   ssize_t stat = 0;
   uint32_t wlen;                     /* length to write */
   bool ok = true;
   DCR *dcr = this;
   uint32_t checksum;
   uint32_t pad;                      /* padding or zeros written */

   if (no_tape_write_test) {
      empty_block(block);
      return true;
   }
   if (job_canceled(jcr)) {
      return false;
   }

   Dmsg3(200, "fd=%d bufp-buf=%d binbuf=%d\n", dev->fd(),
      block->bufp-block->buf, block->binbuf);
   ASSERT2(block->binbuf == ((uint32_t)(block->bufp - block->buf)), "binbuf badly set");

   if (is_block_empty(block)) {  /* Does block have data in it? */
      Dmsg0(150, "return write_block_to_dev no data to write\n");
      return true;
   }

   dump_block(block, "before write");
   if (dev->at_weot()) {
      Dmsg0(50, "==== FATAL: At EOM with ST_WEOT.\n");
      dev->dev_errno = ENOSPC;
      Jmsg1(jcr, M_FATAL, 0,  _("Cannot write block. Device at EOM. dev=%s\n"), dev->print_name());
      return false;
   }
   if (!dev->can_append()) {
      dev->dev_errno = EIO;
      Jmsg1(jcr, M_FATAL, 0, _("Attempt to write on read-only Volume. dev=%s\n"), dev->print_name());
      Dmsg1(50, "Attempt to write on read-only Volume. dev=%s\n", dev->print_name());
      return false;
   }

   if (!dev->is_open()) {
      Jmsg1(jcr, M_FATAL, 0, _("Attempt to write on closed device=%s\n"), dev->print_name());
      Dmsg1(50, "Attempt to write on closed device=%s\n", dev->print_name());
      return false;
   }

   wlen = get_len_and_clear_block(block, dev, pad);
   block->block_len = wlen;

   checksum = ser_block_header(block, dev->do_checksum());

   /* Handle max vol size here */
   if (user_volume_size_reached(dcr, true)) {
      Dmsg0(40, "Calling terminate_writing_volume\n");
      terminate_writing_volume(dcr);
      reread_last_block(dcr);   /* Only used on tapes */
      dev->dev_errno = ENOSPC;
      return false;
   }

   /*
    * Limit maximum File size on volume to user specified value.
    *  In practical terms, this means to put an EOF mark on
    *  a tape after every X bytes. This effectively determines
    *  how many index records we have (JobMedia). If you set
    *  max_file_size too small, it will cause a lot of shoe-shine
    *  on very fast modern tape (LTO-3 and above).
    */
   if ((dev->max_file_size > 0) &&
       (dev->file_size+block->binbuf) >= dev->max_file_size) {
      dev->file_size = 0;             /* reset file size */

      if (!dev->weof(1)) {            /* write eof */
         Dmsg0(50, "WEOF error in max file size.\n");
         Jmsg(jcr, M_FATAL, 0, _("Unable to write EOF. ERR=%s\n"),
            dev->bstrerror());
         Dmsg0(40, "Calling terminate_writing_volume\n");
         terminate_writing_volume(dcr);
         dev->dev_errno = ENOSPC;
         return false;
      }
      if (!write_ansi_ibm_labels(dcr, ANSI_EOF_LABEL, dev->VolHdr.VolumeName)) {
         return false;
      }

      if (!do_new_file_bookkeeping(dcr)) {
         /* Error message already sent */
         return false;
      }
   }

   dev->updateVolCatWrites(1);

#ifdef DEBUG_BLOCK_ZEROING
   uint32_t *bp = (uint32_t *)block->buf;
   if (bp[0] == 0 && bp[1] == 0 && bp[2] == 0 && block->buf[12] == 0) {
      Jmsg0(jcr, M_ABORT, 0, _("Write block header zeroed.\n"));
   }
#endif

   /*
    * Do write here, make a somewhat feeble attempt to recover from
    *  I/O errors, or from the OS telling us it is busy.
    */
   int retry = 0;
   errno = 0;
   stat = 0;
   do {
      if (retry > 0 && stat == -1 && errno == EBUSY) {
         berrno be;
         Dmsg4(100, "===== write retry=%d stat=%d errno=%d: ERR=%s\n",
               retry, stat, errno, be.bstrerror());
         bmicrosleep(5, 0);    /* pause a bit if busy or lots of errors */
         dev->clrerror(-1);
      }
      stat = dev->write(block->buf, (size_t)wlen);
      Dmsg4(110, "Write() BlockAddr=%lld NextAddr=%lld Vol=%s wlen=%d\n",
         block->BlockAddr, dev->lseek(dcr, 0, SEEK_CUR),
         dev->VolHdr.VolumeName, wlen);
   } while (stat == -1 && (errno == EBUSY || errno == EIO) && retry++ < 3);

   if (debug_block_checksum) {
      uint32_t achecksum = ser_block_header(block, dev->do_checksum());
      if (checksum != achecksum) {
         Jmsg2(jcr, M_ERROR, 0, _("Block checksum changed during write: before=%ud after=%ud\n"),
            checksum, achecksum);
         dump_block(block, "with checksum error");
      }
   }

#ifdef DEBUG_BLOCK_ZEROING
   if (bp[0] == 0 && bp[1] == 0 && bp[2] == 0 && block->buf[12] == 0) {
      Jmsg0(jcr, M_ABORT, 0, _("Write block header zeroed.\n"));
   }
#endif

   if (stat != (ssize_t)wlen) {
      /* Some devices simply report EIO when the volume is full.
       * With a little more thought we may be able to check
       * capacity and distinguish real errors and EOT
       * conditions.  In any case, we probably want to
       * simulate an End of Medium.
       */
      if (stat == -1) {
         berrno be;
         dev->clrerror(-1);                 /* saves errno in dev->dev_errno */
         if (dev->dev_errno == 0) {
            dev->dev_errno = ENOSPC;        /* out of space */
         }
         if (dev->dev_errno != ENOSPC) {
            dev->VolCatInfo.VolCatErrors++;
            Jmsg4(jcr, M_ERROR, 0, _("Write error at %u:%u on device %s. ERR=%s.\n"),
               dev->file, dev->block_num, dev->print_name(), be.bstrerror());
         }
      } else {
        dev->dev_errno = ENOSPC;            /* out of space */
      }
      if (dev->dev_errno == ENOSPC) {
         dev->clear_nospace();
         Jmsg(jcr, M_INFO, 0, _("End of Volume \"%s\" at %u:%u on device %s. Write of %u bytes got %d.\n"),
            dev->getVolCatName(),
            dev->file, dev->block_num, dev->print_name(), wlen, stat);
      }
      if (chk_dbglvl(100)) {
         berrno be;
         Dmsg7(90, "==== Write error. fd=%d size=%u rtn=%d dev_blk=%d blk_blk=%d errno=%d: ERR=%s\n",
            dev->fd(), wlen, stat, dev->block_num, block->BlockNumber,
            dev->dev_errno, be.bstrerror(dev->dev_errno));
      }

      Dmsg0(40, "Calling terminate_writing_volume\n");
      ok = terminate_writing_volume(dcr);
      if (ok) {
         reread_last_block(dcr);
      }
      return false;
   }

   /* We successfully wrote the block, now do housekeeping */
   Dmsg2(1300, "VolCatBytes=%lld newVolCatBytes=%lld\n", dev->VolCatInfo.VolCatBytes,
      (dev->VolCatInfo.VolCatBytes+wlen));
   dev->updateVolCatBytes(wlen);
   Dmsg2(200, "AmetaBytes=%lld Bytes=%lld\n",
         dev->VolCatInfo.VolCatAmetaBytes, dev->VolCatInfo.VolCatBytes);
   dev->updateVolCatBlocks(1);
   dev->EndBlock = dev->block_num;
   dev->EndFile  = dev->file;
   dev->LastBlock = block->BlockNumber;
   block->BlockNumber++;

   /* Update dcr values */
   if (dev->is_tape()) {
      dcr->EndBlock = dev->EndBlock;
      dcr->EndFile  = dev->EndFile;
      dev->block_num++;
   } else {
      /* Save address of block just written */
      uint64_t addr = dev->file_addr + wlen - 1;
      if (dcr->EndBlock > (uint32_t)addr ||
          dcr->EndFile > (uint32_t)(addr >> 32)) {
         Pmsg4(000, "Incorrect EndBlock/EndFile oldEndBlock=%d newEndBlock=%d oldEndFile=%d newEndFile=%d\n",
            dcr->EndBlock, (uint32_t)addr, dcr->EndFile, (uint32_t)(addr >> 32));
      }
      dcr->EndBlock = (uint32_t)addr;
      dcr->EndFile = (uint32_t)(addr >> 32);
      dev->block_num = (uint32_t)addr;
      dev->file = (uint32_t)(addr >> 32);
      block->BlockAddr = dev->file_addr + wlen;
      Dmsg3(150, "Set block->BlockAddr=%lld wlen=%d block=%x\n",
         block->BlockAddr, wlen, block);
      Dmsg2(200, "AmetaBytes=%lld Bytes=%lld\n",
         dev->VolCatInfo.VolCatAmetaBytes, dev->VolCatInfo.VolCatBytes);
   }
   dcr->VolMediaId = dev->VolCatInfo.VolMediaId;
   Dmsg3(150, "VolFirstIndex=%d blockFirstIndex=%d Vol=%s\n",
     dcr->VolFirstIndex, block->FirstIndex, dcr->VolumeName);
   if (dcr->VolFirstIndex == 0 && block->FirstIndex > 0) {
      dcr->VolFirstIndex = block->FirstIndex;
   }
   if (block->LastIndex > 0) {
      dcr->VolLastIndex = block->LastIndex;
   }
   dcr->WroteVol = true;
   dev->file_addr += wlen;            /* update file address */
   dev->file_size += wlen;
   dev->part_size += wlen;
   dev->setVolCatInfo(false);         /* Needs update */

   Dmsg2(1300, "write_block: wrote block %d bytes=%d\n", dev->block_num, wlen);
   empty_block(block);
   return true;
}


/*
 * Read block with locking
 *
 */
bool DCR::read_block_from_device(bool check_block_numbers)
{
   bool ok;

   Dmsg0(250, "Enter read_block_from_device\n");
   dev->rLock(false);
   ok = read_block_from_dev(check_block_numbers);
   dev->rUnlock();
   Dmsg0(250, "Leave read_block_from_device\n");
   return ok;
}

/*
 * Read the next block into the block structure and unserialize
 *  the block header.  For a file, the block may be partially
 *  or completely in the current buffer.
 */
bool DCR::read_block_from_dev(bool check_block_numbers)
{
   ssize_t stat;
   int looping;
   int retry;
   DCR *dcr = this;

   if (job_canceled(jcr)) {
      Mmsg(dev->errmsg, _("Job failed or canceled.\n"));
      block->read_len = 0;
      return false;
   }

   if (dev->at_eot()) {
      Mmsg(dev->errmsg, _("Attempt to read past end of tape or file.\n"));
      block->read_len = 0;
      return false;
   }
   looping = 0;
   Dmsg1(250, "Full read in read_block_from_device() len=%d\n", block->buf_len);

   if (!dev->is_open()) {
      Mmsg4(dev->errmsg, _("Attempt to read closed device: fd=%d at file:blk %u:%u on device %s\n"),
         dev->fd(), dev->file, dev->block_num, dev->print_name());
      Jmsg(dcr->jcr, M_FATAL, 0, "%s", dev->errmsg);
      Pmsg2(000, "Fatal: dev=%p dcr=%p\n", dev, dcr);
      Pmsg1(000, "%s", dev->errmsg);
      block->read_len = 0;
      return false;
    }

reread:
   if (looping > 1) {
      dev->dev_errno = EIO;
      Mmsg1(dev->errmsg, _("Block buffer size looping problem on device %s\n"),
         dev->print_name());
      Jmsg(jcr, M_ERROR, 0, "%s", dev->errmsg);
      block->read_len = 0;
      return false;
   }

   dump_block(block, "before read");

   retry = 0;
   errno = 0;
   stat = 0;

   boffset_t pos = dev->lseek(dcr, (boffset_t)0, SEEK_CUR); /* get curr pos */
   do {
      if ((retry > 0 && stat == -1 && errno == EBUSY)) {
         berrno be;
         Dmsg4(100, "===== read retry=%d stat=%d errno=%d: ERR=%s\n",
               retry, stat, errno, be.bstrerror());
         bmicrosleep(10, 0);    /* pause a bit if busy or lots of errors */
         dev->clrerror(-1);
      }
      stat = dev->read(block->buf, (size_t)block->buf_len);

   } while (stat == -1 && (errno == EBUSY || errno == EINTR || errno == EIO) && retry++ < 3);
   Dmsg3(110, "Read() vol=%s nbytes=%d addr=%lld\n",
      dev->VolHdr.VolumeName, stat, pos);
   if (stat < 0) {
      berrno be;
      dev->clrerror(-1);
      Dmsg2(90, "Read device fd=%d got: ERR=%s\n", dev->fd(), be.bstrerror());
      block->read_len = 0;
      if (dev->file == 0 && dev->block_num == 0) { /* Attempt to read label */
         Mmsg(dev->errmsg, _("The Volume=%s on device=%s appears to be unlabeled.\n"),
            dev->VolCatInfo.VolCatName, dev->print_name());
      } else {
         Mmsg5(dev->errmsg, _("Read error on fd=%d at file:blk %u:%u on device %s. ERR=%s.\n"),
            dev->fd(), dev->file, dev->block_num, dev->print_name(), be.bstrerror());
      }
      Jmsg(jcr, M_ERROR, 0, "%s", dev->errmsg);
      if (dev->at_eof()) {        /* EOF just seen? */
         dev->set_eot();          /* yes, error => EOT */
      }
      return false;
   }
   if (stat == 0) {             /* Got EOF ! */
      if (dev->file == 0 && dev->block_num == 0) { /* Attempt to read label */
         Mmsg2(dev->errmsg, _("The Volume=%s on device=%s appears to be unlabeled.\n"),
            dev->VolCatInfo.VolCatName, dev->print_name());
      } else {
         Mmsg3(dev->errmsg, _("Read zero bytes Vol=%s at %lld on device %s.\n"),
               dev->VolCatInfo.VolCatName, pos, dev->print_name());
      }
      dev->block_num = 0;
      block->read_len = 0;
      Dmsg1(100, "%s", dev->errmsg);
      if (dev->at_eof()) {       /* EOF already read? */
         dev->set_eot();         /* yes, 2 EOFs => EOT */
         return false;
      }
      dev->set_ateof();
      Dmsg2(150, "==== Read zero bytes. vol=%s at %lld\n", dev->VolCatInfo.VolCatName, pos);
      return false;             /* return eof */
   }

   /* Continue here for successful read */

   block->read_len = stat;      /* save length read */
   if (block->read_len == 80 &&
        (dcr->VolCatInfo.LabelType != B_BACULA_LABEL ||
         dcr->device->label_type != B_BACULA_LABEL)) {
      /* ***FIXME*** should check label */
      Dmsg2(100, "Ignore 80 byte ANSI label at %u:%u\n", dev->file, dev->block_num);
      dev->clear_eof();
      goto reread;             /* skip ANSI/IBM label */
   }

   if (block->read_len < BLKHDR2_LENGTH) {
      dev->dev_errno = EIO;
      Mmsg4(dev->errmsg, _("Volume data error at %u:%u! Very short block of %d bytes on device %s discarded.\n"),
         dev->file, dev->block_num, block->read_len, dev->print_name());
      Jmsg(jcr, M_ERROR, 0, "%s", dev->errmsg);
      dev->set_short_block();
      block->read_len = block->binbuf = 0;
      Dmsg2(50, "set block=%p binbuf=%d\n", block, block->binbuf);
      return false;             /* return error */
   }

   //BlockNumber = block->BlockNumber + 1;
   if (!unser_block_header(jcr, dev, block)) {
      if (forge_on) {
         dev->file_addr += block->read_len;
         dev->file_size += block->read_len;
         goto reread;
      }
      return false;
   }

   /*
    * If the block is bigger than the buffer, we reposition for
    *  re-reading the block, allocate a buffer of the correct size,
    *  and go re-read.
    */
   Dmsg2(150, "block_len=%d buf_len=%d\n", block->block_len, block->buf_len);
   if (block->block_len > block->buf_len) {
      dev->dev_errno = EIO;
      Mmsg2(dev->errmsg,  _("Block length %u is greater than buffer %u. Attempting recovery.\n"),
         block->block_len, block->buf_len);
      Jmsg(jcr, M_ERROR, 0, "%s", dev->errmsg);
      Pmsg1(000, "%s", dev->errmsg);
      /* Attempt to reposition to re-read the block */
      if (dev->is_tape()) {
         Dmsg0(250, "BSR for reread; block too big for buffer.\n");
         if (!dev->bsr(1)) {
            Mmsg(dev->errmsg, "%s", dev->bstrerror());
            Jmsg(jcr, M_ERROR, 0, "%s", dev->errmsg);
            block->read_len = 0;
            return false;
         }
      } else {
         Dmsg0(250, "Seek to beginning of block for reread.\n");
         boffset_t pos = dev->lseek(dcr, (boffset_t)0, SEEK_CUR); /* get curr pos */
         pos -= block->read_len;
         dev->lseek(dcr, pos, SEEK_SET);
         dev->file_addr = pos;
      }
      Mmsg1(dev->errmsg, _("Setting block buffer size to %u bytes.\n"), block->block_len);
      Jmsg(jcr, M_INFO, 0, "%s", dev->errmsg);
      Pmsg1(000, "%s", dev->errmsg);
      /* Set new block length */
      dev->max_block_size = block->block_len;
      block->buf_len = block->block_len;
      free_memory(block->buf);
      block->buf = get_memory(block->buf_len);
      empty_block(block);
      looping++;
      goto reread;                    /* re-read block with correct block size */
   }

   if (block->block_len > block->read_len) {
      dev->dev_errno = EIO;
      Mmsg4(dev->errmsg, _("Volume data error at %u:%u! Short block of %d bytes on device %s discarded.\n"),
         dev->file, dev->block_num, block->read_len, dev->print_name());
      Jmsg(jcr, M_ERROR, 0, "%s", dev->errmsg);
      dev->set_short_block();
      block->read_len = block->binbuf = 0;
      return false;             /* return error */
   }

   dev->clear_short_block();
   dev->clear_eof();
   dev->updateVolCatReads(1);
   dev->updateVolCatReadBytes(block->read_len);

   dev->EndBlock = dev->block_num;
   dev->EndFile  = dev->file;
   dev->block_num++;

   /* Update dcr values */
   if (dev->is_tape()) {
      dcr->EndBlock = dev->EndBlock;
      dcr->EndFile  = dev->EndFile;
   } else {
      /* We need to take care about a short block in EndBlock/File
       * computation
       */
      uint32_t len = MIN(block->read_len, block->block_len);
      uint64_t addr = dev->file_addr + len - 1;
      dcr->EndBlock = (uint32_t)addr;
      dcr->EndFile = (uint32_t)(addr >> 32);
      dev->block_num = dev->EndBlock = (uint32_t)addr;
      dev->file = dev->EndFile = (uint32_t)(addr >> 32);
   }
   dcr->VolMediaId = dev->VolCatInfo.VolMediaId;
   dev->file_addr += block->read_len;
   dev->file_size += block->read_len;

   /*
    * If we read a short block on disk,
    * seek to beginning of next block. This saves us
    * from shuffling blocks around in the buffer. Take a
    * look at this from an efficiency stand point later, but
    * it should only happen once at the end of each job.
    *
    * I've been lseek()ing negative relative to SEEK_CUR for 30
    *   years now. However, it seems that with the new off_t definition,
    *   it is not possible to seek negative amounts, so we use two
    *   lseek(). One to get the position, then the second to do an
    *   absolute positioning -- so much for efficiency.  KES Sep 02.
    */
   Dmsg0(250, "At end of read block\n");
   if (block->read_len > block->block_len && !dev->is_tape()) {
      char ed1[50];
      boffset_t pos = dev->lseek(dcr, (boffset_t)0, SEEK_CUR); /* get curr pos */
      Dmsg1(250, "Current lseek pos=%s\n", edit_int64(pos, ed1));
      pos -= (block->read_len - block->block_len);
      dev->lseek(dcr, pos, SEEK_SET);
      Dmsg3(250, "Did lseek pos=%s blk_size=%d rdlen=%d\n",
         edit_int64(pos, ed1), block->block_len,
            block->read_len);
      dev->file_addr = pos;
      dev->file_size = pos;
   }
   Dmsg3(150, "Exit read_block read_len=%d block_len=%d binbuf=%d\n",
      block->read_len, block->block_len, block->binbuf);
   block->block_read = true;
   return true;
}
