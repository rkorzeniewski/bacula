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
static bool open_data_spool_file(JCR *jcr);
static bool close_data_spool_file(JCR *jcr);
static bool despool_data(DCR *dcr);
static int  read_block_from_spool_file(DCR *dcr, DEV_BLOCK *block);
static bool open_attr_spool_file(JCR *jcr, BSOCK *bs);
static bool close_attr_spool_file(JCR *jcr, BSOCK *bs);

struct spool_stats_t {
   uint32_t data_jobs;		      /* current jobs spooling data */
   uint32_t attr_jobs;		      
   uint32_t total_data_jobs;	      /* total jobs to have spooled data */
   uint32_t total_attr_jobs;
   uint64_t max_data_size;	      /* max data size */
   uint64_t max_attr_size;
   uint64_t data_size;		      /* current data size (all jobs running) */
   int64_t attr_size;
};

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
spool_stats_t spool_stats;

/* 
 * Header for data spool record */
struct spool_hdr {
   int32_t  FirstIndex; 	      /* FirstIndex for buffer */
   int32_t  LastIndex;		      /* LastIndex for buffer */
   uint32_t len;		      /* length of next buffer */
};

enum {
   RB_EOT = 1,
   RB_ERROR,
   RB_OK
};

void list_spool_stats(BSOCK *bs)
{
   char ed1[30], ed2[30];
   if (spool_stats.data_jobs || spool_stats.max_data_size) {
      bnet_fsend(bs, "Data spooling: %u active jobs, %s bytes; %u total jobs, %s max bytes/job.\n",
	 spool_stats.data_jobs, edit_uint64_with_commas(spool_stats.data_size, ed1),
	 spool_stats.total_data_jobs, 
	 edit_uint64_with_commas(spool_stats.max_data_size, ed2));
   }
   if (spool_stats.attr_jobs || spool_stats.max_attr_size) {
      bnet_fsend(bs, "Attr spooling: %u active jobs, %s bytes; %u total jobs, %s max bytes.\n",
	 spool_stats.attr_jobs, edit_uint64_with_commas(spool_stats.attr_size, ed1), 
	 spool_stats.total_attr_jobs, 
	 edit_uint64_with_commas(spool_stats.max_attr_size, ed2));
   }
}

bool begin_data_spool(JCR *jcr)
{
   bool stat = true;
   if (jcr->spool_data) {
      Dmsg0(100, "Turning on data spooling\n");
      jcr->dcr->spool_data = true;
      stat = open_data_spool_file(jcr);
      if (stat) {
	 jcr->dcr->spooling = true;
         Jmsg(jcr, M_INFO, 0, _("Spooling data ...\n"));
	 P(mutex);
	 spool_stats.data_jobs++;
	 V(mutex);
      }
   }
   return stat;
}

bool discard_data_spool(JCR *jcr)
{
   if (jcr->dcr->spooling) {
      Dmsg0(100, "Data spooling discarded\n");
      return close_data_spool_file(jcr);
   }
   return true;
}

bool commit_data_spool(JCR *jcr)
{
   bool stat;
   char ec1[40];

   if (jcr->dcr->spooling) {
      Dmsg0(100, "Committing spooled data\n");
      Jmsg(jcr, M_INFO, 0, _("Writing spooled data to Volume. Despooling %s bytes ...\n"),
	    edit_uint64_with_commas(jcr->dcr->dev->spool_size, ec1));
      stat = despool_data(jcr->dcr);
      if (!stat) {
         Dmsg1(000, "Bad return from despool WroteVol=%d\n", jcr->dcr->WroteVol);
	 close_data_spool_file(jcr);
	 return false;
      }
      return close_data_spool_file(jcr);
   }
   return true;
}

static void make_unique_data_spool_filename(JCR *jcr, POOLMEM **name)
{
   char *dir;  
   if (jcr->dcr->dev->device->spool_directory) {
      dir = jcr->dcr->dev->device->spool_directory;
   } else {
      dir = working_directory;
   }
   Mmsg(name, "%s/%s.data.spool.%s.%s", dir, my_name, jcr->Job, jcr->device->hdr.name);
}


static bool open_data_spool_file(JCR *jcr)
{
   POOLMEM *name  = get_pool_memory(PM_MESSAGE);
   int spool_fd;

   make_unique_data_spool_filename(jcr, &name);
   if ((spool_fd = open(name, O_CREAT|O_TRUNC|O_RDWR|O_BINARY, 0640)) >= 0) {
      jcr->dcr->spool_fd = spool_fd;
      jcr->spool_attributes = true;
   } else {
      Jmsg(jcr, M_ERROR, 0, _("Open data spool file %s failed: ERR=%s\n"), name, strerror(errno));
      free_pool_memory(name);
      return false;
   }
   Dmsg1(100, "Created spool file: %s\n", name);
   free_pool_memory(name);
   return true;
}

static bool close_data_spool_file(JCR *jcr)
{
   POOLMEM *name  = get_pool_memory(PM_MESSAGE);

   P(mutex);
   spool_stats.data_jobs--;
   spool_stats.total_data_jobs++;
   if (spool_stats.data_size < jcr->dcr->spool_size) {
      spool_stats.data_size = 0;
   } else {
      spool_stats.data_size -= jcr->dcr->spool_size;
   }
   jcr->dcr->spool_size = 0;
   V(mutex);

   make_unique_data_spool_filename(jcr, &name);
   close(jcr->dcr->spool_fd);
   jcr->dcr->spool_fd = -1;
   jcr->dcr->spooling = false;
   unlink(name);
   Dmsg1(100, "Deleted spool file: %s\n", name);
   free_pool_memory(name);
   return true;
}

static bool despool_data(DCR *dcr)
{
   DEVICE *rdev;
   DCR *rdcr;
   bool ok = true;
   DEV_BLOCK *block;
   JCR *jcr = dcr->jcr;
   int stat;

   Dmsg0(100, "Despooling data\n");
   dcr->spooling = false;
   lock_device(dcr->dev);
   dcr->dev_locked = true; 

   /* Setup a dev structure to read */
   rdev = (DEVICE *)malloc(sizeof(DEVICE));
   memset(rdev, 0, sizeof(DEVICE));
   rdev->dev_name = get_memory(strlen("spool")+1);
   strcpy(rdev->dev_name, "spool");
   rdev->errmsg = get_pool_memory(PM_EMSG);
   *rdev->errmsg = 0;
   rdev->max_block_size = dcr->dev->max_block_size;
   rdev->min_block_size = dcr->dev->min_block_size;
   rdev->device = dcr->dev->device;
   rdcr = new_dcr(NULL, rdev);
   rdcr->spool_fd = dcr->spool_fd; 
   rdcr->jcr = jcr;		      /* set a valid jcr */
   block = rdcr->block;
   Dmsg1(800, "read/write block size = %d\n", block->buf_len);
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
      Jmsg(dcr->jcr, M_FATAL, 0, _("Ftruncate spool file error. ERR=%s\n"), 
	 strerror(errno));
      Dmsg1(000, "Bad return from ftruncate. ERR=%s\n", strerror(errno));
      ok = false;
   }

   P(mutex);
   if (spool_stats.data_size < dcr->spool_size) {
      spool_stats.data_size = 0;
   } else {
      spool_stats.data_size -= dcr->spool_size;
   }
   V(mutex);
   P(dcr->dev->spool_mutex);
   dcr->dev->spool_size -= dcr->spool_size;
   dcr->spool_size = 0; 	      /* zap size in input dcr */
   V(dcr->dev->spool_mutex);
   free_memory(rdev->dev_name);
   free_pool_memory(rdev->errmsg);
   free(rdev);
   rdcr->jcr = NULL;
   free_dcr(rdcr);
   unlock_device(dcr->dev);
   dcr->dev_locked = false;
   dcr->spooling = true;	   /* turn on spooling again */
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
         Jmsg(dcr->jcr, M_FATAL, 0, _("Spool header read error. ERR=%s\n"), strerror(errno));
      } else {
         Dmsg2(000, "Spool read error. Wanted %u bytes, got %u\n", rlen, stat);
         Jmsg2(dcr->jcr, M_FATAL, 0, _("Spool header read error. Wanted %u bytes, got %u\n"), rlen, stat);
      }
      return RB_ERROR;
   }
   rlen = hdr.len;
   if (rlen > block->buf_len) {
      Dmsg2(000, "Spool block too big. Max %u bytes, got %u\n", block->buf_len, rlen);
      Jmsg2(dcr->jcr, M_FATAL, 0, _("Spool block too big. Max %u bytes, got %u\n"), block->buf_len, rlen);
      return RB_ERROR;
   }
   stat = read(dcr->spool_fd, (char *)block->buf, (size_t)rlen);
   if (stat != (ssize_t)rlen) {
      Dmsg2(000, "Spool data read error. Wanted %u bytes, got %u\n", rlen, stat);
      Jmsg2(dcr->jcr, M_FATAL, 0, _("Spool data read error. Wanted %u bytes, got %u\n"), rlen, stat);
      return RB_ERROR;
   }
   /* Setup write pointers */
   block->binbuf = rlen;
   block->bufp = block->buf + block->binbuf;
   block->FirstIndex = hdr.FirstIndex;
   block->LastIndex = hdr.LastIndex;
   block->VolSessionId = dcr->jcr->VolSessionId;
   block->VolSessionTime = dcr->jcr->VolSessionTime;
   Dmsg2(100, "Read block FI=%d LI=%d\n", block->FirstIndex, block->LastIndex);
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
   uint32_t wlen, hlen; 	      /* length to write */
   int retry = 0;
   spool_hdr hdr;   
   bool despool = false;

   ASSERT(block->binbuf == ((uint32_t) (block->bufp - block->buf)));
   if (block->binbuf <= WRITE_BLKHDR_LENGTH) {	/* Does block have data in it? */
      return true;
   }

   hlen = sizeof(hdr);
   wlen = block->binbuf;
   P(dcr->dev->spool_mutex);
   dcr->spool_size += hlen + wlen;
   dcr->dev->spool_size += hlen + wlen;
   if ((dcr->max_spool_size > 0 && dcr->spool_size >= dcr->max_spool_size) ||
       (dcr->dev->max_spool_size > 0 && dcr->dev->spool_size >= dcr->dev->max_spool_size)) {
      despool = true;
   }
   V(dcr->dev->spool_mutex);
   P(mutex);
   spool_stats.data_size += hlen + wlen;
   if (spool_stats.data_size > spool_stats.max_data_size) {
      spool_stats.max_data_size = spool_stats.data_size;
   }
   V(mutex);
   if (despool) {
      char ec1[30];
#ifdef xDEBUG 
      char ec2[30], ec3[30], ec4[30];
      Dmsg4(100, "Despool in write_block_to_spool_file max_size=%s size=%s "
            "max_job_size=%s job_size=%s\n", 
	    edit_uint64_with_commas(dcr->max_spool_size, ec1),
	    edit_uint64_with_commas(dcr->spool_size, ec2),
	    edit_uint64_with_commas(dcr->dev->max_spool_size, ec3),
	    edit_uint64_with_commas(dcr->dev->spool_size, ec4));
#endif
      Jmsg(dcr->jcr, M_INFO, 0, _("User specified spool size reached. Despooling %s bytes ...\n"),
	    edit_uint64_with_commas(dcr->dev->spool_size, ec1));
      if (!despool_data(dcr)) {
         Dmsg0(000, "Bad return from despool in write_block.\n");
	 return false;
      }
      /* Despooling cleared these variables so reset them */
      P(dcr->dev->spool_mutex);
      dcr->spool_size += hlen + wlen;
      dcr->dev->spool_size += hlen + wlen;
      V(dcr->dev->spool_mutex);
      Jmsg(dcr->jcr, M_INFO, 0, _("Spooling data again ...\n"));
   }  

   hdr.FirstIndex = block->FirstIndex;
   hdr.LastIndex = block->LastIndex;
   hdr.len = block->binbuf;

   /* Write header */
   for ( ;; ) {
      stat = write(dcr->spool_fd, (char*)&hdr, (size_t)hlen);
      if (stat == -1) {
         Jmsg(dcr->jcr, M_INFO, 0, _("Error writing header to spool file. ERR=%s\n"), strerror(errno));
      }
      if (stat != (ssize_t)hlen) {
	 if (!despool_data(dcr)) {
            Jmsg(dcr->jcr, M_FATAL, 0, _("Fatal despooling error."));
	    return false;
	 }
	 if (retry++ > 1) {
	    return false;
	 }
	 continue;
      }
      break;
   }

   /* Write data */
   for ( ;; ) {
      stat = write(dcr->spool_fd, block->buf, (size_t)wlen);
      if (stat == -1) {
         Jmsg(dcr->jcr, M_INFO, 0, _("Error writing data to spool file. ERR=%s\n"), strerror(errno));
      }
      if (stat != (ssize_t)wlen) {
	 if (!despool_data(dcr)) {
            Jmsg(dcr->jcr, M_FATAL, 0, _("Fatal despooling error."));
	    return false;
	 }
	 if (retry++ > 1) {
	    return false;
	 }
	 continue;
      }
      break;
   }
   Dmsg2(100, "Wrote block FI=%d LI=%d\n", block->FirstIndex, block->LastIndex);

   empty_block(block);
   return true;
}


bool are_attributes_spooled(JCR *jcr)
{
   return jcr->spool_attributes && jcr->dir_bsock->spool_fd;
}

/* 
 * Create spool file for attributes.
 *  This is done by "attaching" to the bsock, and when
 *  it is called, the output is written to a file.
 *  The actual spooling is turned on and off in
 *  append.c only during writing of the attributes.
 */
bool begin_attribute_spool(JCR *jcr)
{
   if (!jcr->no_attributes && jcr->spool_attributes) {
      return open_attr_spool_file(jcr, jcr->dir_bsock);
   }
   return true;
}

bool discard_attribute_spool(JCR *jcr)
{
   if (are_attributes_spooled(jcr)) {
      return close_attr_spool_file(jcr, jcr->dir_bsock);
   }
   return true;
}

static void update_attr_spool_size(ssize_t size)
{
   P(mutex);
   if (size > 0) {
     if ((spool_stats.attr_size - size) > 0) {
	spool_stats.attr_size -= size;
     } else {
	spool_stats.attr_size = 0;
     }
   }
   V(mutex);
}

bool commit_attribute_spool(JCR *jcr)
{
   ssize_t size;
   char ec1[30];

   if (are_attributes_spooled(jcr)) {
      fseek(jcr->dir_bsock->spool_fd, 0, SEEK_END);
      size = ftell(jcr->dir_bsock->spool_fd);
      P(mutex);
      if (size > 0) {
	if (spool_stats.attr_size + size > spool_stats.max_attr_size) {
	   spool_stats.max_attr_size = spool_stats.attr_size + size;
	} 
      }
      spool_stats.attr_size += size;
      V(mutex);
      Jmsg(jcr, M_INFO, 0, _("Sending spooled attrs to DIR. Despooling %s bytes ...\n"),
	    edit_uint64_with_commas(size, ec1));
      bnet_despool_to_bsock(jcr->dir_bsock, update_attr_spool_size, size);
      return close_attr_spool_file(jcr, jcr->dir_bsock);
   }
   return true;
}

static void make_unique_spool_filename(JCR *jcr, POOLMEM **name, int fd)
{
   Mmsg(name, "%s/%s.attr.spool.%s.%d", working_directory, my_name,
      jcr->Job, fd);
}


bool open_attr_spool_file(JCR *jcr, BSOCK *bs)
{
   POOLMEM *name  = get_pool_memory(PM_MESSAGE);

   make_unique_spool_filename(jcr, &name, bs->fd);
   bs->spool_fd = fopen(mp_chr(name), "w+");
   if (!bs->spool_fd) {
      Jmsg(jcr, M_ERROR, 0, _("fopen attr spool file %s failed: ERR=%s\n"), name, strerror(errno));
      free_pool_memory(name);
      return false;
   }
   P(mutex);
   spool_stats.attr_jobs++;
   V(mutex);
   free_pool_memory(name);
   return true;
}

bool close_attr_spool_file(JCR *jcr, BSOCK *bs)
{
   POOLMEM *name;
    
   if (!bs->spool_fd) {
      return true;
   }
   name = get_pool_memory(PM_MESSAGE);
   P(mutex);
   spool_stats.attr_jobs--;
   spool_stats.total_attr_jobs++;
   V(mutex);
   make_unique_spool_filename(jcr, &name, bs->fd);
   fclose(bs->spool_fd);
   unlink(mp_chr(name));
   free_pool_memory(name);
   bs->spool_fd = NULL;
   bs->spool = false;
   return true;
}
