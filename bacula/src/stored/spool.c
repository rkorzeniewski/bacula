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
static int read_block_from_spool_file(DCR *dcr, DEV_BLOCK *block);

struct spool_hdr {
   int32_t  FirstIndex;
   int32_t  LastIndex;
   uint32_t len;
};

enum {
   RB_EOT = 1,
   RB_ERROR,
   RB_OK
};

int begin_data_spool(JCR *jcr)
{
   int stat = 1;
   if (jcr->spool_data) {
      Dmsg0(100, "Turning on data spooling\n");
      jcr->dcr->spool_data = true;
      stat = open_data_spool_file(jcr);
      if (stat) {
	 jcr->dcr->spooling = true;
      }
   }
   return stat;
}

int discard_data_spool(JCR *jcr)
{
   if (jcr->dcr->spooling) {
      Dmsg0(100, "Data spooling discarded\n");
      return close_data_spool_file(jcr);
   }
   return 1;
}

int commit_data_spool(JCR *jcr)
{
   bool stat;
   if (jcr->dcr->spooling) {
      Dmsg0(100, "Committing spooled data\n");
      stat = despool_data(jcr->dcr);
      if (!stat) {
         Dmsg1(000, "Bad return from despool WroteVol=%d\n", jcr->dcr->WroteVol);
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
    Dmsg1(100, "Created spool file: %s\n", name);
    free_pool_memory(name);
    return 1;
}

static int close_data_spool_file(JCR *jcr)
{
    POOLMEM *name  = get_pool_memory(PM_MESSAGE);

    make_unique_data_spool_filename(jcr, &name);
    close(jcr->dcr->spool_fd);
    jcr->dcr->spool_fd = -1;
    jcr->dcr->spooling = false;
    unlink(name);
    Dmsg1(100, "Deleted spool file: %s\n", name);
    free_pool_memory(name);
    return 1;
}

static bool despool_data(DCR *dcr)
{
   DEVICE *rdev;
   DCR *rdcr;
   dcr->spooling = false;
   bool ok = true;
   DEV_BLOCK *block;
   JCR *jcr = dcr->jcr;
   int stat;

// lock_device(dcr->dev);
   Dmsg0(100, "Despooling data\n");
   /* Set up a dev structure to read */
   rdev = (DEVICE *)malloc(sizeof(DEVICE));
   memset(rdev, 0, sizeof(DEVICE));
   rdev->dev_name = get_memory(strlen("spool")+1);
   strcpy(rdev->dev_name, "spool");
   rdev->errmsg = get_pool_memory(PM_EMSG);
   *rdev->errmsg = 0;
   rdcr = new_dcr(NULL, rdev);
   rdcr->spool_fd = dcr->spool_fd; 
   rdcr->jcr = jcr;		      /* set a valid jcr */
   block = rdcr->block;
   lseek(rdcr->spool_fd, 0, SEEK_SET); /* rewind */
   for ( ; ok; ) {
      if (job_canceled(jcr)) {
	 ok = false;
	 break;
      }
      stat = read_block_from_spool_file(rdcr, block);
      if (stat == RB_EOT) {
	 break;
      } else if (stat == RB_ERROR) {
	 ok = false;
	 break;
      }
      ok = write_block_to_device(dcr, block);
      Dmsg3(100, "Write block ok=%d FI=%d LI=%d\n", ok, block->FirstIndex, block->LastIndex);
   }
   lseek(rdcr->spool_fd, 0, SEEK_SET); /* rewind */
   if (ftruncate(rdcr->spool_fd, 0) != 0) {
      Dmsg1(000, "Bad return from ftruncate. ERR=%s\n", strerror(errno));
      ok = false;
   }
   free_memory(rdev->dev_name);
   free_pool_memory(rdev->errmsg);
   free(rdev);
   rdcr->jcr = NULL;
   free_dcr(rdcr);
// unlock_device(dcr->dev);
   return ok;
}

/*
 * Read a block from the spool file
 * 
 *  Returns RB_OK on success
 *	    RB_EOT when file done
 *	    RB_ERROR on error
 */
static int read_block_from_spool_file(DCR *dcr, DEV_BLOCK *block)
{
   uint32_t rlen;
   ssize_t stat;
   spool_hdr hdr;

   rlen = sizeof(hdr);
   stat = read(dcr->spool_fd, (char *)&hdr, (size_t)rlen);
   if (stat == 0) {
      Dmsg0(100, "EOT on spool read.\n");
      return RB_EOT;
   } else if (stat != (ssize_t)rlen) {
      if (stat == -1) {
         Jmsg(dcr->jcr, M_FATAL, 0, "Spool read error. ERR=%s\n", strerror(errno));
      } else {
         Dmsg2(000, "Spool read error. Wanted %u bytes, got %u\n", rlen, stat);
         Jmsg2(dcr->jcr, M_FATAL, 0, "Spool read error. Wanted %u bytes, got %u\n", rlen, stat);
      }
      return RB_ERROR;
   }
   rlen = hdr.len;
   if (rlen > block->buf_len) {
      Dmsg2(000, "Spool block too big. Max %u bytes, got %u\n", block->buf_len, rlen);
      Jmsg2(dcr->jcr, M_FATAL, 0, "Spool block too big. Max %u bytes, got %u\n", block->buf_len, rlen);
      return RB_ERROR;
   }
   stat = read(dcr->spool_fd, (char *)block->buf, (size_t)rlen);
   if (stat != (ssize_t)rlen) {
      Dmsg2(000, "Spool read error. Wanted %u bytes, got %u\n", rlen, stat);
      Jmsg2(dcr->jcr, M_FATAL, 0, "Spool read error. Wanted %u bytes, got %u\n", rlen, stat);
      return RB_ERROR;
   }
   /* Setup write pointers */
   block->binbuf = rlen;
   block->bufp = block->buf + block->binbuf;
   block->FirstIndex = hdr.FirstIndex;
   block->LastIndex = hdr.LastIndex;
   block->VolSessionId = dcr->jcr->VolSessionId;
   block->VolSessionTime = dcr->jcr->VolSessionTime;
   Dmsg2(400, "Read block FI=%d LI=%d\n", block->FirstIndex, block->LastIndex);
   return RB_OK;
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
   int retry = 0;
   spool_hdr hdr;   

   ASSERT(block->binbuf == ((uint32_t) (block->bufp - block->buf)));

   if (block->binbuf <= WRITE_BLKHDR_LENGTH) {	/* Does block have data in it? */
      Dmsg0(100, "return write_block_to_dev no data to write\n");
      return true;
   }

   hdr.FirstIndex = block->FirstIndex;
   hdr.LastIndex = block->LastIndex;
   hdr.len = block->binbuf;
   wlen = sizeof(hdr);
write_hdr_again:
   stat = write(dcr->spool_fd, (char*)&hdr, (size_t)wlen);
   if (stat != (ssize_t)wlen) {
      if (!despool_data(dcr)) {
	 return false;
      }
      if (retry++ > 1) {
	 return false;
      }
      goto write_hdr_again;
   }


   wlen = block->binbuf;
   Dmsg2(300, "Wrote block FI=%d LI=%d\n", block->FirstIndex, block->LastIndex);
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
