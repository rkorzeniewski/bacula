/*
 *  Spooling code 
 *
 *	Kern Sibbald, March 2004
 *
 *  Version $Id$
 */
/*
   Copyright (C) 2000-2004 Kern Sibbald and John Walker

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

/* Forward referenced subroutines */
static void make_unique_data_spool_filename(JCR *jcr, POOLMEM **name);
static int open_data_spool_file(JCR *jcr);
static int close_data_spool_file(JCR *jcr);
static bool despool_data(DCR *dcr);


int begin_data_spool(JCR *jcr)
{
   if (jcr->dcr->spool_data) {
      return open_data_spool_file(jcr);
   }
   return 1;
}

int discard_data_spool(JCR *jcr)
{
   if (jcr->dcr->spool_data && jcr->dcr->spool_fd >= 0) {
      return close_data_spool_file(jcr);
   }
   return 1;
}

int commit_data_spool(JCR *jcr)
{
   bool stat;
   if (jcr->dcr->spool_data && jcr->dcr->spool_fd >= 0) {
      lock_device(jcr->dcr->dev);
      stat = despool_data(jcr->dcr);
      unlock_device(jcr->dcr->dev);
      if (!stat) {
	 close_data_spool_file(jcr);
	 return 0;
      }
      return close_data_spool_file(jcr);
   }
   return 1;
}

static void make_unique_data_spool_filename(JCR *jcr, POOLMEM **name)
{
   Mmsg(name, "%s/%s.data.spool.%s.%s", working_directory, my_name,
      jcr->Job, jcr->device->hdr.name);
}


static int open_data_spool_file(JCR *jcr)
{
   POOLMEM *name  = get_pool_memory(PM_MESSAGE);
   int spool_fd;

   make_unique_data_spool_filename(jcr, &name);
   if ((spool_fd = open(name, O_CREAT|O_TRUNC|O_RDWR|O_BINARY, 0640)) >= 0) {
      jcr->dcr->spool_fd = spool_fd;
      jcr->spool_attributes = true;
   } else {
      Jmsg(jcr, M_ERROR, 0, "open data spool file %s failed: ERR=%s\n", name, strerror(errno));
      free_pool_memory(name);
      return 0;
    }
    free_pool_memory(name);
    return 1;
}

static int close_data_spool_file(JCR *jcr)
{
    POOLMEM *name  = get_pool_memory(PM_MESSAGE);

    make_unique_data_spool_filename(jcr, &name);
    close(jcr->dcr->spool_fd);
    jcr->dcr->spool_fd = -1;
    unlink(name);
    free_pool_memory(name);
    return 1;
}

static bool despool_data(DCR *dcr)
{
   DEVICE *sdev;
   DCR *sdcr;
   dcr->spooling = false;
   bool ok = true;
   DEV_BLOCK *block = dcr->block;
   JCR *jcr = dcr->jcr;

   /* Set up a dev structure to read */
   sdev = (DEVICE *)malloc(sizeof(DEVICE));
   memset(sdev, 0, sizeof(DEVICE));
   sdev->fd = dcr->spool_fd;
   lseek(sdev->fd, 0, SEEK_SET); /* rewind */
   sdcr = new_dcr(jcr, sdev);
   for ( ; ok; ) {
      if (job_canceled(jcr)) {
	 ok = false;
	 break;
      }
      if (!read_block_from_dev(jcr, sdev, block, CHECK_BLOCK_NUMBERS)) {
	 if (dev_state(sdev, ST_EOT)) {
	    break;
	 }
	 ok = false;
	 break;
      }
      if (!write_block_to_dev(dcr, block)) {
	 ok = false;
	 break;
      }
   }
   lseek(sdev->fd, 0, SEEK_SET); /* rewind */
   if (ftruncate(sdev->fd, 0) != 0) {
      ok = false;
   }
   return ok;
}

/*
 * Write a block to the spool file
 *
 *  Returns: true on success or EOT
 *	     false on hard error
 */
bool write_block_to_spool_file(DCR *dcr, DEV_BLOCK *block)
{
   ssize_t stat = 0;
   uint32_t wlen;		      /* length to write */
   DEVICE *dev = dcr->dev;
   int retry = 0;

   ASSERT(block->binbuf == ((uint32_t) (block->bufp - block->buf)));

   wlen = block->binbuf;
   if (wlen <= WRITE_BLKHDR_LENGTH) {  /* Does block have data in it? */
      Dmsg0(100, "return write_block_to_dev no data to write\n");
      return true;
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

   Dmsg1(300, "Write block of %u bytes\n", wlen);      
write_again:
   stat = write(dcr->spool_fd, block->buf, (size_t)wlen);
   if (stat != (ssize_t)wlen) {
      if (!despool_data(dcr)) {
	 return false;
      }
      if (retry++ > 1) {
	 return false;
      }
      goto write_again;
   }

   empty_block(block);
   return true;
}



bool are_attributes_spooled(JCR *jcr)
{
   return jcr->spool_attributes && jcr->dir_bsock->spool_fd;
}

int begin_attribute_spool(JCR *jcr)
{
   if (!jcr->no_attributes && jcr->spool_attributes) {
      return open_spool_file(jcr, jcr->dir_bsock);
   }
   return 1;
}

int discard_attribute_spool(JCR *jcr)
{
   if (are_attributes_spooled(jcr)) {
      return close_spool_file(jcr, jcr->dir_bsock);
   }
   return 1;
}

int commit_attribute_spool(JCR *jcr)
{
   if (are_attributes_spooled(jcr)) {
      bnet_despool_to_bsock(jcr->dir_bsock);
      return close_spool_file(jcr, jcr->dir_bsock);
   }
   return 1;
}
