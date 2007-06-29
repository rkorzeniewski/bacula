/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2004-2007 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from
   many others, a complete list can be found in the file AUTHORS.
   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version two of the GNU General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

   Bacula® is a registered trademark of John Walker.
   The licensor of Bacula is the Free Software Foundation Europe
   (FSFE), Fiduciary Program, Sumatrastrasse 25, 8006 Zürich,
   Switzerland, email:ftf@fsfeurope.org.
*/
/*
 *  Spooling code
 *
 *      Kern Sibbald, March 2004
 *
 *  Version $Id$
 */

#include "bacula.h"
#include "stored.h"

/* Forward referenced subroutines */
static void make_unique_data_spool_filename(DCR *dcr, POOLMEM **name);
static bool open_data_spool_file(DCR *dcr);
static bool close_data_spool_file(DCR *dcr);
static bool despool_data(DCR *dcr, bool commit);
static int  read_block_from_spool_file(DCR *dcr);
static bool open_attr_spool_file(JCR *jcr, BSOCK *bs);
static bool close_attr_spool_file(JCR *jcr, BSOCK *bs);
static bool write_spool_header(DCR *dcr);
static bool write_spool_data(DCR *dcr);

struct spool_stats_t {
   uint32_t data_jobs;                /* current jobs spooling data */
   uint32_t attr_jobs;
   uint32_t total_data_jobs;          /* total jobs to have spooled data */
   uint32_t total_attr_jobs;
   int64_t max_data_size;             /* max data size */
   int64_t max_attr_size;
   int64_t data_size;                 /* current data size (all jobs running) */
   int64_t attr_size;
};

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
spool_stats_t spool_stats;

/*
 * Header for data spool record */
struct spool_hdr {
   int32_t  FirstIndex;               /* FirstIndex for buffer */
   int32_t  LastIndex;                /* LastIndex for buffer */
   uint32_t len;                      /* length of next buffer */
};

enum {
   RB_EOT = 1,
   RB_ERROR,
   RB_OK
};

void list_spool_stats(void sendit(const char *msg, int len, void *sarg), void *arg)
{
   char ed1[30], ed2[30];
   POOL_MEM msg(PM_MESSAGE);
   int len;

   len = Mmsg(msg, _("Spooling statistics:\n"));

   if (spool_stats.data_jobs || spool_stats.max_data_size) {
      len = Mmsg(msg, _("Data spooling: %u active jobs, %s bytes; %u total jobs, %s max bytes/job.\n"),
         spool_stats.data_jobs, edit_uint64_with_commas(spool_stats.data_size, ed1),
         spool_stats.total_data_jobs,
         edit_uint64_with_commas(spool_stats.max_data_size, ed2));

      sendit(msg.c_str(), len, arg);
   }
   if (spool_stats.attr_jobs || spool_stats.max_attr_size) {
      len = Mmsg(msg, _("Attr spooling: %u active jobs, %s bytes; %u total jobs, %s max bytes.\n"),
         spool_stats.attr_jobs, edit_uint64_with_commas(spool_stats.attr_size, ed1),
         spool_stats.total_attr_jobs,
         edit_uint64_with_commas(spool_stats.max_attr_size, ed2));
   
      sendit(msg.c_str(), len, arg);
   }
   len = Mmsg(msg, "====\n");
   sendit(msg.c_str(), len, arg);
}

bool begin_data_spool(DCR *dcr)
{
   bool stat = true;
   if (!dcr->dev->is_dvd() && dcr->jcr->spool_data) {
      Dmsg0(100, "Turning on data spooling\n");
      dcr->spool_data = true;
      stat = open_data_spool_file(dcr);
      if (stat) {
         dcr->spooling = true;
         Jmsg(dcr->jcr, M_INFO, 0, _("Spooling data ...\n"));
         P(mutex);
         spool_stats.data_jobs++;
         V(mutex);
      }
   }
   return stat;
}

bool discard_data_spool(DCR *dcr)
{
   if (dcr->spooling) {
      Dmsg0(100, "Data spooling discarded\n");
      return close_data_spool_file(dcr);
   }
   return true;
}

bool commit_data_spool(DCR *dcr)
{
   bool stat;

   if (dcr->spooling) {
      Dmsg0(100, "Committing spooled data\n");
      stat = despool_data(dcr, true /*commit*/);
      if (!stat) {
         Dmsg1(100, _("Bad return from despool WroteVol=%d\n"), dcr->WroteVol);
         close_data_spool_file(dcr);
         return false;
      }
      return close_data_spool_file(dcr);
   }
   return true;
}

static void make_unique_data_spool_filename(DCR *dcr, POOLMEM **name)
{
   const char *dir;
   if (dcr->dev->device->spool_directory) {
      dir = dcr->dev->device->spool_directory;
   } else {
      dir = working_directory;
   }
   Mmsg(name, "%s/%s.data.%u.%s.%s.spool", dir, my_name, dcr->jcr->JobId,
        dcr->jcr->Job, dcr->device->hdr.name);
}


static bool open_data_spool_file(DCR *dcr)
{
   POOLMEM *name  = get_pool_memory(PM_MESSAGE);
   int spool_fd;

   make_unique_data_spool_filename(dcr, &name);
   if ((spool_fd = open(name, O_CREAT|O_TRUNC|O_RDWR|O_BINARY, 0640)) >= 0) {
      dcr->spool_fd = spool_fd;
      dcr->jcr->spool_attributes = true;
   } else {
      berrno be;
      Jmsg(dcr->jcr, M_FATAL, 0, _("Open data spool file %s failed: ERR=%s\n"), name,
           be.bstrerror());
      free_pool_memory(name);
      return false;
   }
   Dmsg1(100, "Created spool file: %s\n", name);
   free_pool_memory(name);
   return true;
}

static bool close_data_spool_file(DCR *dcr)
{
   POOLMEM *name  = get_pool_memory(PM_MESSAGE);

   P(mutex);
   spool_stats.data_jobs--;
   spool_stats.total_data_jobs++;
   if (spool_stats.data_size < dcr->job_spool_size) {
      spool_stats.data_size = 0;
   } else {
      spool_stats.data_size -= dcr->job_spool_size;
   }
   dcr->job_spool_size = 0;
   V(mutex);

   make_unique_data_spool_filename(dcr, &name);
   close(dcr->spool_fd);
   dcr->spool_fd = -1;
   dcr->spooling = false;
   unlink(name);
   Dmsg1(100, "Deleted spool file: %s\n", name);
   free_pool_memory(name);
   return true;
}

static const char *spool_name = "*spool*";

/*
 * NB! This routine locks the device, but if committing will
 *     not unlock it. If not committing, it will be unlocked.
 */
static bool despool_data(DCR *dcr, bool commit)
{
   DEVICE *rdev;
   DCR *rdcr;
   bool ok = true;
   DEV_BLOCK *block;
   JCR *jcr = dcr->jcr;
   int stat;
   char ec1[50];

   Dmsg0(100, "Despooling data\n");
   /*
    * Commit means that the job is done, so we commit, otherwise, we
    *  are despooling because of user spool size max or some error  
    *  (e.g. filesystem full).
    */
   if (commit) {
      Jmsg(jcr, M_INFO, 0, _("Committing spooled data to Volume \"%s\". Despooling %s bytes ...\n"),
         jcr->dcr->VolumeName,
         edit_uint64_with_commas(jcr->dcr->job_spool_size, ec1));
   } else {
      Jmsg(jcr, M_INFO, 0, _("Writing spooled data to Volume. Despooling %s bytes ...\n"),
         edit_uint64_with_commas(jcr->dcr->job_spool_size, ec1));
   }
   dcr->despool_wait = true;
   dcr->spooling = false;
   /*
    * We work with device blocked, but not locked so that
    *  other threads -- e.g. reservations can lock the device
    *  structure.
    */
   dcr->dblock(BST_DESPOOLING);
   dcr->despool_wait = false;
   dcr->despooling = true;

   /*
    * This is really quite kludgy and should be fixed some time.
    * We create a dev structure to read from the spool file
    * in rdev and rdcr.
    */
   rdev = (DEVICE *)malloc(sizeof(DEVICE));
   memset(rdev, 0, sizeof(DEVICE));
   rdev->dev_name = get_memory(strlen(spool_name)+1);
   bstrncpy(rdev->dev_name, spool_name, sizeof(rdev->dev_name));
   rdev->errmsg = get_pool_memory(PM_EMSG);
   *rdev->errmsg = 0;
   rdev->max_block_size = dcr->dev->max_block_size;
   rdev->min_block_size = dcr->dev->min_block_size;
   rdev->device = dcr->dev->device;
   rdcr = new_dcr(jcr, NULL, rdev);
   rdcr->spool_fd = dcr->spool_fd;
   block = dcr->block;                /* save block */
   dcr->block = rdcr->block;          /* make read and write block the same */

   Dmsg1(800, "read/write block size = %d\n", block->buf_len);
   lseek(rdcr->spool_fd, 0, SEEK_SET); /* rewind */

#if defined(HAVE_POSIX_FADVISE) && defined(POSIX_FADV_WILLNEED)
   posix_fadvise(rdcr->spool_fd, 0, 0, POSIX_FADV_WILLNEED);
#endif

   /* Add run time, to get current wait time */
   time_t despool_start = time(NULL) - jcr->run_time;

   for ( ; ok; ) {
      if (job_canceled(jcr)) {
         ok = false;
         break;
      }
      stat = read_block_from_spool_file(rdcr);
      if (stat == RB_EOT) {
         break;
      } else if (stat == RB_ERROR) {
         ok = false;
         break;
      }
      ok = write_block_to_device(dcr);
      if (!ok) {
         Jmsg2(jcr, M_FATAL, 0, _("Fatal append error on device %s: ERR=%s\n"),
               dcr->dev->print_name(), dcr->dev->bstrerror());
      }
      Dmsg3(800, "Write block ok=%d FI=%d LI=%d\n", ok, block->FirstIndex, block->LastIndex);
   }

   /* Subtracting run_time give us elapsed time - wait_time since we started despooling */
   time_t despool_elapsed = time(NULL) - despool_start - jcr->run_time;

   if (despool_elapsed <= 0) {
      despool_elapsed = 1;
   }

   Jmsg(dcr->jcr, M_INFO, 0, _("Despooling elapsed time = %02d:%02d:%02d, Transfer rate = %s bytes/second\n"),
         despool_elapsed / 3600, despool_elapsed % 3600 / 60, despool_elapsed % 60,
         edit_uint64_with_suffix(jcr->dcr->job_spool_size / despool_elapsed, ec1));

   dcr->block = block;                /* reset block */

   lseek(rdcr->spool_fd, 0, SEEK_SET); /* rewind */
   if (ftruncate(rdcr->spool_fd, 0) != 0) {
      berrno be;
      Jmsg(dcr->jcr, M_ERROR, 0, _("Ftruncate spool file failed: ERR=%s\n"),
         be.bstrerror());
      /* Note, try continuing despite ftruncate problem */
   }

   P(mutex);
   if (spool_stats.data_size < dcr->job_spool_size) {
      spool_stats.data_size = 0;
   } else {
      spool_stats.data_size -= dcr->job_spool_size;
   }
   V(mutex);
   P(dcr->dev->spool_mutex);
   dcr->dev->spool_size -= dcr->job_spool_size;
   dcr->job_spool_size = 0;            /* zap size in input dcr */
   V(dcr->dev->spool_mutex);
   free_memory(rdev->dev_name);
   free_pool_memory(rdev->errmsg);
   /* Be careful to NULL the jcr and free rdev after free_dcr() */
   rdcr->jcr = NULL;
   rdcr->dev = NULL;
   free_dcr(rdcr);
   free(rdev);
   dcr->spooling = true;           /* turn on spooling again */
   dcr->despooling = false;
   /* 
    * We are done, so unblock the device, but if we have done a 
    *  commit, leave it locked so that the job cleanup does not
    *  need to wait to release the device (no re-acquire of the lock).
    */
   dcr->dlock();
   unblock_device(dcr->dev);
   /* If doing a commit, leave the device locked -- unlocked in release_device() */
   if (!commit) {
      dcr->dunlock();
   }
   return ok;
}

/*
 * Read a block from the spool file
 *
 *  Returns RB_OK on success
 *          RB_EOT when file done
 *          RB_ERROR on error
 */
static int read_block_from_spool_file(DCR *dcr)
{
   uint32_t rlen;
   ssize_t stat;
   spool_hdr hdr;
   DEV_BLOCK *block = dcr->block;

   rlen = sizeof(hdr);
   stat = read(dcr->spool_fd, (char *)&hdr, (size_t)rlen);
   if (stat == 0) {
      Dmsg0(100, "EOT on spool read.\n");
      return RB_EOT;
   } else if (stat != (ssize_t)rlen) {
      if (stat == -1) {
         berrno be;
         Jmsg(dcr->jcr, M_FATAL, 0, _("Spool header read error. ERR=%s\n"),
              be.bstrerror());
      } else {
         Pmsg2(000, _("Spool read error. Wanted %u bytes, got %d\n"), rlen, stat);
         Jmsg2(dcr->jcr, M_FATAL, 0, _("Spool header read error. Wanted %u bytes, got %d\n"), rlen, stat);
      }
      return RB_ERROR;
   }
   rlen = hdr.len;
   if (rlen > block->buf_len) {
      Pmsg2(000, _("Spool block too big. Max %u bytes, got %u\n"), block->buf_len, rlen);
      Jmsg2(dcr->jcr, M_FATAL, 0, _("Spool block too big. Max %u bytes, got %u\n"), block->buf_len, rlen);
      return RB_ERROR;
   }
   stat = read(dcr->spool_fd, (char *)block->buf, (size_t)rlen);
   if (stat != (ssize_t)rlen) {
      Pmsg2(000, _("Spool data read error. Wanted %u bytes, got %d\n"), rlen, stat);
      Jmsg2(dcr->jcr, M_FATAL, 0, _("Spool data read error. Wanted %u bytes, got %d\n"), rlen, stat);
      return RB_ERROR;
   }
   /* Setup write pointers */
   block->binbuf = rlen;
   block->bufp = block->buf + block->binbuf;
   block->FirstIndex = hdr.FirstIndex;
   block->LastIndex = hdr.LastIndex;
   block->VolSessionId = dcr->jcr->VolSessionId;
   block->VolSessionTime = dcr->jcr->VolSessionTime;
   Dmsg2(800, "Read block FI=%d LI=%d\n", block->FirstIndex, block->LastIndex);
   return RB_OK;
}

/*
 * Write a block to the spool file
 *
 *  Returns: true on success or EOT
 *           false on hard error
 */
bool write_block_to_spool_file(DCR *dcr)
{
   uint32_t wlen, hlen;               /* length to write */
   bool despool = false;
   DEV_BLOCK *block = dcr->block;

   ASSERT(block->binbuf == ((uint32_t) (block->bufp - block->buf)));
   if (block->binbuf <= WRITE_BLKHDR_LENGTH) {  /* Does block have data in it? */
      return true;
   }

   hlen = sizeof(spool_hdr);
   wlen = block->binbuf;
   P(dcr->dev->spool_mutex);
   dcr->job_spool_size += hlen + wlen;
   dcr->dev->spool_size += hlen + wlen;
   if ((dcr->max_job_spool_size > 0 && dcr->job_spool_size >= dcr->max_job_spool_size) ||
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
#ifdef xDEBUG
      char ec1[30], ec2[30], ec3[30], ec4[30];
      Dmsg4(100, "Despool in write_block_to_spool_file max_size=%s size=%s "
            "max_job_size=%s job_size=%s\n",
            edit_uint64_with_commas(dcr->max_job_spool_size, ec1),
            edit_uint64_with_commas(dcr->job_spool_size, ec2),
            edit_uint64_with_commas(dcr->dev->max_spool_size, ec3),
            edit_uint64_with_commas(dcr->dev->spool_size, ec4));
#endif
      Jmsg(dcr->jcr, M_INFO, 0, _("User specified spool size reached.\n"));
      if (!despool_data(dcr, false)) {
         Pmsg0(000, _("Bad return from despool in write_block.\n"));
         return false;
      }
      /* Despooling cleared these variables so reset them */
      P(dcr->dev->spool_mutex);
      dcr->job_spool_size += hlen + wlen;
      dcr->dev->spool_size += hlen + wlen;
      V(dcr->dev->spool_mutex);
      Jmsg(dcr->jcr, M_INFO, 0, _("Spooling data again ...\n"));
   }


   if (!write_spool_header(dcr)) {
      return false;
   }
   if (!write_spool_data(dcr)) {
     return false;
   }

   Dmsg2(800, "Wrote block FI=%d LI=%d\n", block->FirstIndex, block->LastIndex);
   empty_block(block);
   return true;
}

static bool write_spool_header(DCR *dcr)
{
   spool_hdr hdr;
   ssize_t stat;
   DEV_BLOCK *block = dcr->block;

   hdr.FirstIndex = block->FirstIndex;
   hdr.LastIndex = block->LastIndex;
   hdr.len = block->binbuf;

   /* Write header */
   for (int retry=0; retry<=1; retry++) {
      stat = write(dcr->spool_fd, (char*)&hdr, sizeof(hdr));
      if (stat == -1) {
         berrno be;
         Jmsg(dcr->jcr, M_FATAL, 0, _("Error writing header to spool file. ERR=%s\n"),
              be.bstrerror());
      }
      if (stat != (ssize_t)sizeof(hdr)) {
         /* If we wrote something, truncate it, then despool */
         if (stat != -1) {
#if defined(HAVE_WIN32)
            boffset_t   pos = _lseeki64(dcr->spool_fd, (__int64)0, SEEK_CUR);
#else
            boffset_t   pos = lseek(dcr->spool_fd, (off_t)0, SEEK_CUR);
#endif
            if (ftruncate(dcr->spool_fd, pos - stat) != 0) {
               berrno be;
               Jmsg(dcr->jcr, M_ERROR, 0, _("Ftruncate spool file failed: ERR=%s\n"),
                  be.bstrerror());
              /* Note, try continuing despite ftruncate problem */
            }
         }
         if (!despool_data(dcr, false)) {
            Jmsg(dcr->jcr, M_FATAL, 0, _("Fatal despooling error."));
            return false;
         }
         continue;                    /* try again */
      }
      return true;
   }
   Jmsg(dcr->jcr, M_FATAL, 0, _("Retrying after header spooling error failed.\n"));
   return false;
}

static bool write_spool_data(DCR *dcr)
{
   ssize_t stat;
   DEV_BLOCK *block = dcr->block;

   /* Write data */
   for (int retry=0; retry<=1; retry++) {
      stat = write(dcr->spool_fd, block->buf, (size_t)block->binbuf);
      if (stat == -1) {
         berrno be;
         Jmsg(dcr->jcr, M_FATAL, 0, _("Error writing data to spool file. ERR=%s\n"),
              be.bstrerror());
      }
      if (stat != (ssize_t)block->binbuf) {
         /*
          * If we wrote something, truncate it and the header, then despool
          */
         if (stat != -1) {
#if defined(HAVE_WIN32)
            boffset_t   pos = _lseeki64(dcr->spool_fd, (__int64)0, SEEK_CUR);
#else
            boffset_t   pos = lseek(dcr->spool_fd, (off_t)0, SEEK_CUR);
#endif
            if (ftruncate(dcr->spool_fd, pos - stat - sizeof(spool_hdr)) != 0) {
               berrno be;
               Jmsg(dcr->jcr, M_ERROR, 0, _("Ftruncate spool file failed: ERR=%s\n"),
                  be.bstrerror());
               /* Note, try continuing despite ftruncate problem */
            }
         }
         if (!despool_data(dcr, false)) {
            Jmsg(dcr->jcr, M_FATAL, 0, _("Fatal despooling error."));
            return false;
         }
         if (!write_spool_header(dcr)) {
            return false;
         }
         continue;                    /* try again */
      }
      return true;
   }
   Jmsg(dcr->jcr, M_FATAL, 0, _("Retrying after data spooling error failed.\n"));
   return false;
}



bool are_attributes_spooled(JCR *jcr)
{
   return jcr->spool_attributes && jcr->dir_bsock->m_spool_fd;
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
   off_t size;
   char ec1[30];

   if (are_attributes_spooled(jcr)) {
      if (fseeko(jcr->dir_bsock->m_spool_fd, 0, SEEK_END) != 0) {
         berrno be;
         Jmsg(jcr, M_FATAL, 0, _("Fseek on attributes file failed: ERR=%s\n"),
              be.bstrerror());
         goto bail_out;
      }
      size = ftello(jcr->dir_bsock->m_spool_fd);
      if (size < 0) {
         berrno be;
         Jmsg(jcr, M_FATAL, 0, _("Fseek on attributes file failed: ERR=%s\n"),
              be.bstrerror());
         goto bail_out;
      }
      P(mutex);
      if (spool_stats.attr_size + size > spool_stats.max_attr_size) {
         spool_stats.max_attr_size = spool_stats.attr_size + size;
      }
      spool_stats.attr_size += size;
      V(mutex);
      Jmsg(jcr, M_INFO, 0, _("Sending spooled attrs to the Director. Despooling %s bytes ...\n"),
            edit_uint64_with_commas(size, ec1));
      jcr->dir_bsock->despool(update_attr_spool_size, size);
      return close_attr_spool_file(jcr, jcr->dir_bsock);
   }
   return true;

bail_out:
   close_attr_spool_file(jcr, jcr->dir_bsock);
   return false;
}

static void make_unique_spool_filename(JCR *jcr, POOLMEM **name, int fd)
{
   Mmsg(name, "%s/%s.attr.%s.%d.spool", working_directory, my_name,
      jcr->Job, fd);
}


bool open_attr_spool_file(JCR *jcr, BSOCK *bs)
{
   POOLMEM *name  = get_pool_memory(PM_MESSAGE);

   make_unique_spool_filename(jcr, &name, bs->m_fd);
   bs->m_spool_fd = fopen(name, "w+b");
   if (!bs->m_spool_fd) {
      berrno be;
      Jmsg(jcr, M_FATAL, 0, _("fopen attr spool file %s failed: ERR=%s\n"), name,
           be.bstrerror());
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

   if (!bs->m_spool_fd) {
      return true;
   }
   name = get_pool_memory(PM_MESSAGE);
   P(mutex);
   spool_stats.attr_jobs--;
   spool_stats.total_attr_jobs++;
   V(mutex);
   make_unique_spool_filename(jcr, &name, bs->m_fd);
   fclose(bs->m_spool_fd);
   unlink(name);
   free_pool_memory(name);
   bs->m_spool_fd = NULL;
   bs->clear_spooling();
   return true;
}
