/*
 *
 *   block.c -- tape block handling functions
 *
 *		Kern Sibbald, March MMI
 *		   added BB02 format October MMII
 *
 *   Version $Id$
 *
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


#include "bacula.h"
#include "stored.h"

extern int debug_level;

/*
 * Dump the block header, then walk through
 * the block printing out the record headers.
 */
void dump_block(DEV_BLOCK *b, char *msg)
{
   ser_declare;
   char *p;
   char Id[BLKHDR_ID_LENGTH+1];
   uint32_t CheckSum, BlockCheckSum;
   uint32_t block_len;
   uint32_t BlockNumber;
   uint32_t VolSessionId, VolSessionTime, data_len;
   int32_t  FileIndex;
   int32_t  Stream;
   int bhl, rhl;

   unser_begin(b->buf, BLKHDR1_LENGTH);
   unser_uint32(CheckSum);
   unser_uint32(block_len);
   unser_uint32(BlockNumber);
   unser_bytes(Id, BLKHDR_ID_LENGTH);
   ASSERT(unser_length(b->buf) == BLKHDR1_LENGTH);
   Id[BLKHDR_ID_LENGTH] = 0;
   if (Id[3] == '2') {
      unser_uint32(VolSessionId);
      unser_uint32(VolSessionTime);
      bhl = BLKHDR2_LENGTH;
      rhl = RECHDR2_LENGTH;
   } else {
      VolSessionId = VolSessionTime = 0;
      bhl = BLKHDR1_LENGTH;
      rhl = RECHDR1_LENGTH;
   }

   if (block_len > 100000) {
      Dmsg3(20, "Dump block %s 0x%x blocksize too big %u\n", msg, b, block_len);
      return;
   }

   BlockCheckSum = bcrc32((uint8_t *)b->buf+BLKHDR_CS_LENGTH,
			 block_len-BLKHDR_CS_LENGTH);
   Pmsg6(000, "Dump block %s %x: size=%d BlkNum=%d\n\
               Hdrcksum=%x cksum=%x\n",
      msg, b, block_len, BlockNumber, CheckSum, BlockCheckSum);
   p = b->buf + bhl;
   while (p < (b->buf + block_len+WRITE_RECHDR_LENGTH)) { 
      unser_begin(p, WRITE_RECHDR_LENGTH);
      if (rhl == RECHDR1_LENGTH) {
	 unser_uint32(VolSessionId);
	 unser_uint32(VolSessionTime);
      }
      unser_int32(FileIndex);
      unser_int32(Stream);
      unser_uint32(data_len);
      Pmsg6(000, "   Rec: VId=%u VT=%u FI=%s Strm=%s len=%d p=%x\n",
	   VolSessionId, VolSessionTime, FI_to_ascii(FileIndex), 
	   stream_to_ascii(Stream, FileIndex), data_len, p);
      p += data_len + rhl;
  }
}
    
/*
 * Create a new block structure.			   
 * We pass device so that the block can inherit the
 * min and max block sizes.
 */
DEV_BLOCK *new_block(DEVICE *dev)
{
   DEV_BLOCK *block = (DEV_BLOCK *)get_memory(sizeof(DEV_BLOCK));

   memset(block, 0, sizeof(DEV_BLOCK));

   /* If the user has specified a max_block_size, use it as the default */
   if (dev->max_block_size == 0) {
      block->buf_len = DEFAULT_BLOCK_SIZE;
   } else {
      block->buf_len = dev->max_block_size;
   }
   block->dev = dev;
   block->block_len = block->buf_len;  /* default block size */
   block->buf = get_memory(block->buf_len); 
   if (block->buf == NULL) {
      Mmsg0(&dev->errmsg, _("Unable to malloc block buffer.\n"));
      Emsg0(M_FATAL, 0, dev->errmsg);
      return NULL;
   }
   empty_block(block);
   block->BlockVer = BLOCK_VER;       /* default write version */
   Dmsg1(90, "Returning new block=%x\n", block);
   return block;
}

/*
 * Free block 
 */
void free_block(DEV_BLOCK *block)
{
   Dmsg1(199, "free_block buffer %x\n", block->buf);
   free_memory(block->buf);
   Dmsg1(199, "free_block block %x\n", block);
   free_memory((POOLMEM *)block);
}

/* Empty the block -- for writing */
void empty_block(DEV_BLOCK *block)
{
   block->binbuf = WRITE_BLKHDR_LENGTH;
   block->bufp = block->buf + block->binbuf;
   block->read_len = 0;
   block->failed_write = FALSE;
}

/*
 * Create block header just before write. The space
 * in the buffer should have already been reserved by
 * init_block.
 */
static void ser_block_header(DEV_BLOCK *block)
{
   ser_declare;
   uint32_t CheckSum = 0;
   uint32_t block_len = block->binbuf;
   
   Dmsg1(190, "ser_block_header: block_len=%d\n", block_len);
   ser_begin(block->buf, BLKHDR2_LENGTH);
   ser_uint32(CheckSum);
   ser_uint32(block_len);
   ser_uint32(block->BlockNumber);
   ser_bytes(WRITE_BLKHDR_ID, BLKHDR_ID_LENGTH);
   if (BLOCK_VER >= 2) {
      ser_uint32(block->VolSessionId);
      ser_uint32(block->VolSessionTime);
   }

   /* Checksum whole block except for the checksum */
   CheckSum = bcrc32((uint8_t *)block->buf+BLKHDR_CS_LENGTH, 
		 block_len-BLKHDR_CS_LENGTH);
   Dmsg1(190, "ser_bloc_header: checksum=%x\n", CheckSum);
   ser_begin(block->buf, BLKHDR2_LENGTH);
   ser_uint32(CheckSum);	      /* now add checksum to block header */
}

/*
 * Unserialized the block header for reading block.
 *  This includes setting all the buffer pointers correctly.
 *
 *  Returns: 0 on failure (not a block)
 *	     1 on success
 */
static int unser_block_header(DEVICE *dev, DEV_BLOCK *block)
{
   ser_declare;
   char Id[BLKHDR_ID_LENGTH+1];
   uint32_t CheckSum, BlockCheckSum;
   uint32_t block_len;
   uint32_t EndBlock;
   uint32_t BlockNumber;
   int bhl;

   unser_begin(block->buf, BLKHDR_LENGTH);
   unser_uint32(CheckSum);
   unser_uint32(block_len);
   unser_uint32(BlockNumber);
   unser_bytes(Id, BLKHDR_ID_LENGTH);
   ASSERT(unser_length(block->buf) == BLKHDR1_LENGTH);

   Id[BLKHDR_ID_LENGTH] = 0;
   if (Id[3] == '1') {
      bhl = BLKHDR1_LENGTH;
      block->BlockVer = 1;
      block->bufp = block->buf + bhl;
      if (strncmp(Id, BLKHDR1_ID, BLKHDR_ID_LENGTH) != 0) {
         Mmsg2(&dev->errmsg, _("Buffer ID error. Wanted: %s, got %s. Buffer discarded.\n"),
	    BLKHDR1_ID, Id);
	 Emsg0(M_ERROR, 0, dev->errmsg);
	 return 0;
      }
   } else if (Id[3] == '2') {
      unser_uint32(block->VolSessionId);
      unser_uint32(block->VolSessionTime);
      bhl = BLKHDR2_LENGTH;
      block->BlockVer = 2;
      block->bufp = block->buf + bhl;
      if (strncmp(Id, BLKHDR2_ID, BLKHDR_ID_LENGTH) != 0) {
         Mmsg2(&dev->errmsg, _("Buffer ID error. Wanted: %s, got %s. Buffer discarded.\n"),
	    BLKHDR2_ID, Id);
	 Emsg0(M_ERROR, 0, dev->errmsg);
	 return 0;
      }
   } else {
      Mmsg1(&dev->errmsg, _("Expected block-id BB01 or BB02, got %s. Buffer discarded.\n"), Id);
      Emsg0(M_ERROR, 0, dev->errmsg);
      return 0;
   }

   /* Sanity check */
   if (block_len > MAX_BLOCK_LENGTH) {
      Mmsg1(&dev->errmsg,  _("Block length %u is insane (too large), probably due to a bad archive.\n"),
	 block_len);
      Emsg0(M_ERROR, 0, dev->errmsg);
      return 0;
   }

   Dmsg1(190, "unser_block_header block_len=%d\n", block_len);
   /* Find end of block or end of buffer whichever is smaller */
   if (block_len > block->read_len) {
      EndBlock = block->read_len;
   } else {
      EndBlock = block_len;
   }
   block->binbuf = EndBlock - bhl;
   block->block_len = block_len;
   block->BlockNumber = BlockNumber;
   Dmsg3(190, "Read binbuf = %d %d block_len=%d\n", block->binbuf,
      bhl, block_len);
   if (block_len <= block->read_len) {
      BlockCheckSum = bcrc32((uint8_t *)block->buf+BLKHDR_CS_LENGTH,
			 block_len-BLKHDR_CS_LENGTH);
      if (BlockCheckSum != CheckSum) {
         Dmsg2(00, "Block checksum mismatch: calc=%x blk=%x\n", BlockCheckSum,
	    CheckSum);
         Mmsg2(&dev->errmsg, _("Block checksum mismatch: calc=%x blk=%x\n"), BlockCheckSum,
	    CheckSum);
	 return 0;
      }
   }
   return 1;
}

/*  
 * Write a block to the device, with locking and unlocking
 *
 * Returns: 1 on success
 *	  : 0 on failure
 *
 */
int write_block_to_device(JCR *jcr, DEVICE *dev, DEV_BLOCK *block)
{
   int stat = 1;
   lock_device(dev);
   if (!write_block_to_dev(dev, block)) {
       stat = fixup_device_block_write_error(jcr, dev, block);
   }
   unlock_device(dev);
   return stat;
}

/*
 * Write a block to the device 
 *
 *  Returns: 1 on success or EOT
 *	     0 on hard error
 */
int write_block_to_dev(DEVICE *dev, DEV_BLOCK *block)
{
   size_t stat = 0;
   uint32_t wlen;		      /* length to write */
   int hit_max1, hit_max2;

#ifdef NO_TAPE_WRITE_TEST
   empty_block(block);
   return 1;
#endif
   ASSERT(block->binbuf == ((uint32_t) (block->bufp - block->buf)));

   /* dump_block(block, "before write"); */
   if (dev->state & ST_WEOT) {
      Dmsg0(100, "return write_block_to_dev with ST_WEOT\n");
      return 0;
   }
   wlen = block->binbuf;
   if (wlen <= WRITE_BLKHDR_LENGTH) {  /* Does block have data in it? */
      Dmsg0(100, "return write_block_to_dev no data to write\n");
      return 1;
   }
   /* 
    * Clear to the end of the buffer if it is not full,
    *  and on tape devices, apply min and fixed blocking.
    */
   if (wlen != block->buf_len) {
      uint32_t blen;		      /* current buffer length */

      Dmsg2(200, "binbuf=%d buf_len=%d\n", block->binbuf, block->buf_len);
      blen = wlen;

      /* Adjust write size to min/max for tapes only */
      if (dev->state & ST_TAPE) {
	 if (wlen < dev->min_block_size) {
	    wlen =  ((dev->min_block_size + TAPE_BSIZE - 1) / TAPE_BSIZE) * TAPE_BSIZE;
	 }
	 /* check for fixed block size */
	 if (dev->min_block_size == dev->max_block_size) {
	    wlen = block->buf_len;    /* fixed block size already rounded */
	 }
      }
      if (wlen-blen > 0) {
	 memset(block->bufp, 0, wlen-blen); /* clear garbage */
      }
   }  

   ser_block_header(block);

   /* Limit maximum Volume size to value specified by user */
   hit_max1 = (dev->max_volume_size > 0) &&
       ((dev->VolCatInfo.VolCatBytes + block->binbuf)) >= dev->max_volume_size;
   hit_max2 = (dev->VolCatInfo.VolCatMaxBytes > 0) &&
       ((dev->VolCatInfo.VolCatBytes + block->binbuf)) >= dev->VolCatInfo.VolCatMaxBytes;
   if (hit_max1 || hit_max2) {	 
      char ed1[50];
      dev->state |= ST_WEOT;
      Dmsg0(10, "==== Output bytes Triggered medium max capacity.\n");
      if (hit_max1) {
         Mmsg2(&dev->errmsg, _("Max. Volume capacity %s exceeded on device %s.\n"),
	    edit_uint64(dev->max_volume_size, ed1),  dev->dev_name);
      } else {
         Mmsg2(&dev->errmsg, _("Max. Volume capacity %s exceeded on device %s.\n"),
	    edit_uint64(dev->VolCatInfo.VolCatMaxBytes, ed1),  dev->dev_name);
      }
      block->failed_write = TRUE;
/* ****FIXME**** write EOD record here */
      weof_dev(dev, 1); 	      /* end the tape */
      weof_dev(dev, 1); 	      /* write second eof */
      return 0;
   }

   dev->VolCatInfo.VolCatWrites++;
   Dmsg1(500, "Write block of %u bytes\n", wlen);      
   if ((uint32_t) (stat=write(dev->fd, block->buf, (size_t)wlen)) != wlen) {
      /* We should check for errno == ENOSPC, BUT many 
       * devices simply report EIO when it is full.
       * with a little more thought we may be able to check
       * capacity and distinguish real errors and EOT
       * conditions.  In any case, we probably want to
       * simulate an End of Medium.
       */
/***FIXME**** if we wrote a partial record, back up over it */
      dev->state |= ST_EOF | ST_EOT | ST_WEOT;
      clrerror_dev(dev, -1);

      if (dev->dev_errno == 0) {
	 dev->dev_errno = ENOSPC;	 /* out of space */
      }

      Dmsg4(10, "=== Write error. size=%u rtn=%d  errno=%d: ERR=%s\n", 
	 wlen, stat, dev->dev_errno, strerror(dev->dev_errno));

      Mmsg4(&dev->errmsg, _("Write error on device %s. Write of %u bytes got %d. ERR=%s.\n"), 
	 dev->dev_name, wlen, stat, strerror(dev->dev_errno));
      block->failed_write = TRUE;
      weof_dev(dev, 1); 	      /* end the tape */
      weof_dev(dev, 1); 	      /* write second eof */
      return 0;
   }
   dev->VolCatInfo.VolCatBytes += block->binbuf;
   dev->VolCatInfo.VolCatBlocks++;   
   dev->file_addr += wlen;

   /* Limit maximum File size on volume to user specified value */
   if (dev->state & ST_TAPE) {
      if ((dev->max_file_size > 0) && dev->file_addr >= dev->max_file_size) {
	 weof_dev(dev, 1);		 /* write eof */
      }
   }

   dev->block_num++;
   block->BlockNumber++;
   Dmsg2(190, "write_block: wrote block %d bytes=%d\n", dev->block_num,
      wlen);
   empty_block(block);
   return 1;
}

/*  
 * Read block with locking
 *
 */
int read_block_from_device(DEVICE *dev, DEV_BLOCK *block)
{
   int stat;
   Dmsg0(90, "Enter read_block_from_device\n");
   lock_device(dev);
   stat = read_block_from_dev(dev, block);
   unlock_device(dev);
   Dmsg0(90, "Leave read_block_from_device\n");
   return stat;
}

/*
 * Read the next block into the block structure and unserialize
 *  the block header.  For a file, the block may be partially
 *  or completely in the current buffer.
 */
int read_block_from_dev(DEVICE *dev, DEV_BLOCK *block)
{
   size_t stat;
   int looping;

   looping = 0;
   Dmsg1(100, "Full read() in read_block_from_device() len=%d\n",
	 block->buf_len);
reread:
   if (looping > 1) {
      Mmsg1(&dev->errmsg, _("Block buffer size looping problem on device %s\n"),
	 dev->dev_name);
      block->read_len = 0;
      return 0;
   }
   if ((stat=read(dev->fd, block->buf, (size_t)block->buf_len)) < 0) {
      Dmsg1(90, "Read device got: ERR=%s\n", strerror(errno));
      clrerror_dev(dev, -1);
      block->read_len = 0;
      Mmsg2(&dev->errmsg, _("Read error on device %s. ERR=%s.\n"), 
	 dev->dev_name, strerror(dev->dev_errno));
      return 0;
   }
   Dmsg1(90, "Read device got %d bytes\n", stat);
   if (stat == 0) {		/* Got EOF ! */
      dev->block_num = block->read_len = 0;
      Mmsg1(&dev->errmsg, _("Read zero bytes on device %s.\n"), dev->dev_name);
      if (dev->state & ST_EOF) { /* EOF alread read? */
	 dev->state |= ST_EOT;	/* yes, 2 EOFs => EOT */
	 return 0;
      }
      dev->file++;		/* increment file */
      dev->state |= ST_EOF;	/* set EOF read */
      return 0; 		/* return eof */
   }
   /* Continue here for successful read */
   block->read_len = stat;	/* save length read */
   if (block->read_len < BLKHDR2_LENGTH) {
      Mmsg2(&dev->errmsg, _("Very short block of %d bytes on device %s discarded.\n"), 
	 block->read_len, dev->dev_name);
      dev->state |= ST_SHORT;	/* set short block */
      block->read_len = block->binbuf = 0;
      return 0; 		/* return error */
   }  

   if (!unser_block_header(dev, block)) {
      return 0;
   }

   /*
    * If the block is bigger than the buffer, we reposition for
    *  re-reading the block, allocate a buffer of the correct size,
    *  and go re-read.
    */
   if (block->block_len > block->buf_len) {
      Mmsg2(&dev->errmsg,  _("Block length %u is greater than buffer %u. Attempting recovery.\n"),
	 block->block_len, block->buf_len);
      Emsg0(M_WARNING, 0, dev->errmsg);
      Dmsg1(000, "%s", dev->errmsg);
      /* Attempt to reposition to re-read the block */
      if (dev->state & ST_TAPE) {
         Dmsg0(000, "Backspace record for reread.\n");
	 if (bsf_dev(dev, 1) != 0) {
	    Emsg0(M_ERROR, 0, dev->errmsg);
	    return 0;
	 }
      } else {
         Dmsg0(000, "Seek to beginning of block for reread.\n");
	 off_t pos = lseek(dev->fd, (off_t)0, SEEK_CUR); /* get curr pos */
	 pos -= block->read_len;
	 lseek(dev->fd, pos, SEEK_SET);   
      }
      Mmsg1(&dev->errmsg, _("Resetting buffer size to %u bytes.\n"), block->block_len);
      Emsg0(M_WARNING, 0, dev->errmsg);
      Dmsg1(000, "%s", dev->errmsg);
      /* Set new block length */
      dev->max_block_size = block->block_len;
      block->buf_len = block->block_len;
      free_memory(block->buf);
      block->buf = get_memory(block->buf_len);
      empty_block(block);
      looping++;
      goto reread;		      /* re-read block with correct block size */
   }

   if (block->block_len > block->read_len) {
      Mmsg2(&dev->errmsg, _("Short block of %d bytes on device %s discarded.\n"), 
	 block->read_len, dev->dev_name);
      dev->state |= ST_SHORT;	/* set short block */
      block->read_len = block->binbuf = 0;
      return 0; 		/* return error */
   }  

   dev->state &= ~(ST_EOF|ST_SHORT); /* clear EOF and short block */
   dev->block_num++;

   /*
    * If we read a short block on disk,
    * seek to beginning of next block. This saves us
    * from shuffling blocks around in the buffer. Take a
    * look at this from an efficiency stand point later, but
    * it should only happen once at the end of each job.
    *
    * I've been lseek()ing negative relative to SEEK_CUR for 30
    *	years now. However, it seems that with the new off_t definition,
    *	it is not possible to seek negative amounts, so we use two
    *	lseek(). One to get the position, then the second to do an
    *	absolute positioning -- so much for efficiency.  KES Sep 02.
    */
   Dmsg0(200, "At end of read block\n");
   if (block->read_len > block->block_len && !(dev->state & ST_TAPE)) {
      off_t pos = lseek(dev->fd, (off_t)0, SEEK_CUR); /* get curr pos */
      pos -= (block->read_len - block->block_len);
      lseek(dev->fd, pos, SEEK_SET);   
      Dmsg2(100, "Did lseek blk_size=%d rdlen=%d\n", block->block_len,
	    block->read_len);
   }
   Dmsg2(200, "Exit read_block read_len=%d block_len=%d\n",
      block->read_len, block->block_len);
   return 1;
}
