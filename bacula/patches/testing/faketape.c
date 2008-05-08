/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2000-2008 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from
   many others, a complete list can be found in the file AUTHORS.
   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version two of the GNU General Public
   License as published by the Free Software Foundation, which is 
   listed in the file LICENSE.

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

Device {
  Name = Drive-1                      #
  Maximum File Size = 800M
  Maximum Volume Size = 3G
  Device Type = TAPE
  Archive Device = /tmp/fake
  Media Type = DLT-8000
  AutomaticMount = yes;               # when device opened, read it
  AlwaysOpen = yes;
  RemovableMedia = yes;
  RandomAccess = no;
}

 */

#include "bacula.h"		/* define 64bit file usage */
#include "stored.h"
#include "faketape.h"

static int dbglevel = 10;
#define FILE_OFFSET 30
faketape *ftape_list[FTAPE_MAX_DRIVE];

static faketape *get_tape(int fd)
{
   ASSERT(fd >= 0);

   if (fd >= FTAPE_MAX_DRIVE) {
      /* error */
      return NULL;
   }

   return ftape_list[fd];
}

static bool put_tape(faketape *ftape)
{
   ASSERT(ftape != NULL);

   int fd = ftape->get_fd();
   if (fd >= FTAPE_MAX_DRIVE) {
      /* error */
      return false;
   }
   ftape_list[fd] = ftape;
   return true;
}

void faketape_debug(int level)
{
   dbglevel = level;
}

/****************************************************************/
/* theses function will replace open/read/write/close/ioctl
 * in bacula core
 */
int faketape_open(const char *pathname, int flags)
{
   ASSERT(pathname != NULL);

   int fd;
   faketape *tape = new faketape();
   fd = tape->open(pathname, flags);
   if (fd > 0) {
      put_tape(tape);
   }
   return fd;
}

int faketape_read(int fd, void *buffer, unsigned int count)
{
   faketape *tape = get_tape(fd);
   ASSERT(tape != NULL);
   return tape->read(buffer, count);
}

int faketape_write(int fd, const void *buffer, unsigned int count)
{
   faketape *tape = get_tape(fd);
   ASSERT(tape != NULL);
   return tape->write(buffer, count);
}

int faketape_close(int fd)
{
   faketape *tape = get_tape(fd);
   ASSERT(tape != NULL);
   tape->close();
   delete tape;
   return 0;
}

int faketape_ioctl(int fd, unsigned long int request, ...)
{
   va_list argp;
   int result=0;

   faketape *t = get_tape(fd);
   if (!t) {
      errno = EBADF;
      return -1;
   }

   va_start(argp, request);

   if (request == MTIOCTOP) {
      result = t->tape_op(va_arg(argp, mtop *));
   } else if (request == MTIOCGET) {
      result = t->tape_get(va_arg(argp, mtget *));
   } else if (request == MTIOCPOS) {
      result = t->tape_pos(va_arg(argp, mtpos *));
   } else {
      errno = ENOTTY;
      result = -1;
   }
   va_end(argp);

   return result;
}

/****************************************************************/

int faketape::tape_op(struct mtop *mt_com)
{
   int result=0;
   
   switch (mt_com->mt_op)
   {
   case MTRESET:
   case MTNOP:
   case MTSETDRVBUFFER:
      break;

   default:
   case MTRAS1:
   case MTRAS2:
   case MTRAS3:
   case MTSETDENSITY:
      errno = ENOTTY;
      result = -1;
      break;

   case MTFSF:			/* Forward space over mt_count filemarks. */
      result = fsf(mt_com->mt_count);
      break;

   case MTBSF:			/* Backward space over mt_count filemarks. */
      result = bsf(mt_com->mt_count);
      break;

   case MTFSR:	    /* Forward space over mt_count records (tape blocks). */
/*
    file number = 1
    block number = 0
   
    file number = 1
    block number = 1
   
    mt: /dev/lto2: Erreur d'entree/sortie
   
    file number = 2
    block number = 0
*/
      /* tester si on se trouve a la fin du fichier */
      result = fsr(mt_com->mt_count);
      break;

   case MTBSR:      /* Backward space over mt_count records (tape blocks). */
      result = bsr(mt_com->mt_count);
      break;

   case MTWEOF:			/* Write mt_count filemarks. */
      result = weof(mt_com->mt_count);
      break;

   case MTREW:			/* Rewind. */
      Dmsg0(dbglevel, "rewind faketape\n");
      atEOF = atEOD = false;
      atBOT = true;
      current_file = 0;
      current_block = 0;
      seek_file();
      break;

   case MTOFFL:			/* put tape offline */
      result = offline();
      break;

   case MTRETEN:		/* Re-tension tape. */
      result = 0;
      break;

   case MTBSFM:			/* not used by bacula */
      errno = EIO;
      result = -1;
      break;

   case MTFSFM:			/* not used by bacula */
      errno = EIO;
      result = -1;
      break;

   case MTEOM:/* Go to the end of the recorded media (for appending files). */
/*
   file number = 3
   block number = -1
*/
      /* Can be at EOM */
      atBOT = false;
      atEOF = false;
      atEOD = true;
      atEOT = false;

      current_file = last_file;
      current_block = -1;
      seek_file();
      break;

   case MTERASE:		/* not used by bacula */
      atEOD = true;
      atEOF = false;
      atEOT = false;

      current_file = 0;
      current_block = -1;
      seek_file();
      truncate_file();
      break;

   case MTSETBLK:
      break;

   case MTSEEK:
      break;

   case MTTELL:
      break;

   case MTFSS:
      break;

   case MTBSS:
      break;

   case MTWSM:
      break;

   case MTLOCK:
      break;

   case MTUNLOCK:
      break;

   case MTLOAD:
      break;

   case MTUNLOAD:
      break;

   case MTCOMPRESSION:
      break;

   case MTSETPART:
      break;

   case MTMKPART:
      break;
   }

   return result == 0 ? 0 : -1;
}

int faketape::tape_get(struct mtget *mt_get)
{
   int density = 1;
   int block_size = 1024;

   mt_get->mt_type = MT_ISSCSI2;
   mt_get->mt_blkno = current_block;
   mt_get->mt_fileno = current_file;

   mt_get->mt_resid = -1;
//   pos_info.PartitionBlockValid ? pos_info.Partition : (ULONG)-1;

   /* TODO */
   mt_get->mt_dsreg = 
      ((density << MT_ST_DENSITY_SHIFT) & MT_ST_DENSITY_MASK) |
      ((block_size << MT_ST_BLKSIZE_SHIFT) & MT_ST_BLKSIZE_MASK);


   mt_get->mt_gstat = 0x00010000;  /* Immediate report mode.*/

   if (atEOF) {
      mt_get->mt_gstat |= 0x80000000;     // GMT_EOF
   }

   if (atBOT) {
      mt_get->mt_gstat |= 0x40000000;     // GMT_BOT
   }
   if (atEOT) {
      mt_get->mt_gstat |= 0x20000000;     // GMT_EOT
   }

   if (atEOD) {
      mt_get->mt_gstat |= 0x08000000;     // GMT_EOD
   }

   if (0) { //WriteProtected) {
      mt_get->mt_gstat |= 0x04000000;     // GMT_WR_PROT
   }

   if (online) {
      mt_get->mt_gstat |= 0x01000000;     // GMT_ONLINE
   } else {
      mt_get->mt_gstat |= 0x00040000;  // GMT_DR_OPEN
   }
   mt_get->mt_erreg = 0;

   return 0;
}

int faketape::tape_pos(struct mtpos *mt_pos)
{
   if (current_block >= 0) {
      mt_pos->mt_blkno = current_block;
      return 0;
   }

   return -1;
}

/*
 * This function try to emulate the append only behavior
 * of a tape. When you wrote something, data after the
 * current position are discarded.
 */
int faketape::truncate_file()
{  
   Dmsg2(dbglevel, "truncate %i:%i\n", current_file, current_block);
   ftruncate(fd, lseek(fd, 0, SEEK_CUR));
   last_file = current_file;
   atEOD=true;
   return 0;
}

faketape::faketape()
{
   fd = -1;

   atEOF = false;
   atBOT = false;
   atEOT = false;
   atEOD = false;
   online = false;
   inplace = false;
   needEOF = false;

   file_size = 0;
   last_file = 0;
   current_file = 0;
   current_block = -1;
   current_pos = 0;

   max_block = 1024*1024*1024*1024*8;

   volume = get_pool_memory(PM_NAME);
}

faketape::~faketape()
{
   free_pool_memory(volume);
}

int faketape::get_fd()
{
   return this->fd;
}

/*
 * TODO: check if after a write op, and other tape op put a EOF
 */
int faketape::write(const void *buffer, unsigned int count)
{
   ASSERT(current_file >= 0);
   ASSERT(count > 0);
   ASSERT(buffer);

   unsigned int nb;
   Dmsg3(dbglevel, "write len=%i %i:%i\n", count, current_file,current_block);

   if (atEOT) {
      Dmsg0(dbglevel, "write nothing, EOT !\n");
      errno = ENOSPC;
      return -1;
   }

   if (!inplace) {
      seek_file();
   }

   if (!atEOD) {		/* if not at the end of the data */
      truncate_file();
   }
 
   if (current_block != -1) {
      current_block++;
   }

   atBOT = false;
   atEOF = false;
   atEOD = true;		/* End of data */

   needEOF = true;		/* next operation need EOF mark */

//   if ((count + file_size) > max_size) {
//      Dmsg2(dbglevel, 
//	    "EOT writing only %i of %i requested\n", 
//	    max_size - file_size, count);
//      count = max_size - file_size;
//      atEOT = true;
//   }

   uint32_t size = count;
   ::write(fd, &size, sizeof(uint32_t));
   nb = ::write(fd, buffer, count);
   
   file_size += sizeof(uint32_t) + nb;

   if (nb != count) {
      atEOT = true;
      Dmsg2(dbglevel, 
	    "Not enough space writing only %i of %i requested\n", 
	    nb, count);
   }

   return nb;
}

int faketape::weof(int count)
{
   ASSERT(current_file >= 0);
   Dmsg3(dbglevel, "Writing EOF %i:%i last=%i\n", 
	 current_file, current_block,last_file);
   if (atEOT) {
      current_block = -1;
      return -1;
   }
   needEOF = false;
   truncate_file();		/* nothing after this point */

   /* TODO: check this */
   current_file += count;
   current_block = 0;

   uint32_t c=0;
   seek_file();
   ::write(fd, &c, sizeof(uint32_t));
   seek_file();

   atEOD = false;
   atBOT = false;
   atEOF = true;

   return 0;
}

int faketape::fsf(int count)
{   
   ASSERT(current_file >= 0);
   ASSERT(fd >= 0);
/*
 * 1 0 -> fsf -> 2 0 -> fsf -> 2 -1
 */
   check_eof();

   int ret;
   if (atEOT) {
      current_block = -1;
      return -1;
   }

   atBOT = atEOF = false;
   Dmsg3(dbglevel+1, "fsf %i+%i <= %i\n", current_file, count, last_file);

   if ((current_file + count) <= last_file) {
      current_file += count;
      current_block = 0;
      ret = 0;
   } else {
      Dmsg0(dbglevel, "Try to FSF after EOT\n");
      current_file = last_file ;
      current_block = -1;
      atEOD=true;
      ret = -1;
   }
   seek_file();
   return ret;
}

int faketape::fsr(int count)
{
   ASSERT(current_file >= 0);
   ASSERT(fd >= 0);
   
   int i,nb, ret=0;
   off_t where=0;
   uint32_t s;
   Dmsg3(dbglevel, "fsr %i:%i count=%i\n", current_file,current_block, count);

   check_eof();

   if (atEOT) {
      errno = EIO;
      current_block = -1;
      return -1;
   }

   if (atEOD) {
      errno = EIO;
      return -1;
   }

   atBOT = atEOF = false;   

   /* check all block record */
   for(i=0; (i < count) && !atEOF ; i++) {
      nb = ::read(fd, &s, sizeof(uint32_t)); /* get size of next block */
      if (nb == sizeof(uint32_t) && s) {
	 current_block++;
	 where = lseek(fd, s, SEEK_CUR);     /* seek after this block */
      } else {
	 Dmsg4(dbglevel, "read EOF %i:%i nb=%i s=%i\n",
	       current_file, current_block, nb,s);
	 errno = EIO;
	 ret = -1;
	 if (current_file < last_file) {
	    current_block = 0;
	    current_file++;
	    seek_file();
	 }
	 atEOF = true;		/* stop the loop */
      }
   }

   find_maxfile();		/* refresh stats */

   if (where == file_size) {
      atEOD = true;
   }
   return ret;
}

int faketape::bsr(int count)
{
   Dmsg2(dbglevel, "bsr current_block=%i count=%i\n", 
	 current_block, count);

   ASSERT(current_file >= 0);
   ASSERT(count == 1);
   ASSERT(fd >= 0);

   check_eof();

   if (!count) {
      return 0;
   }

   int ret=0;
   int last_f=0;
   int last_b=0;

   off_t last=-1, last2=-1;
   off_t orig = lseek(fd, 0, SEEK_CUR);
   int orig_f = current_file;
   int orig_b = current_block;

   current_block=0;
   seek_file();

   do {
      if (!atEOF) {
	 last2 = last;
	 last = lseek(fd, 0, SEEK_CUR);
	 last_f = current_file;
	 last_b = current_block;
	 Dmsg5(dbglevel, "EOF=%i last=%lli orig=%lli %i:%i\n", 
	       atEOF, last, orig, current_file, current_block);
      }
      ret = fsr(1);
   } while ((lseek(fd, 0, SEEK_CUR) < orig) && (ret == 0));

   if (last2 > 0 && atEOF) {	/* we take the previous position */
      lseek(fd, last2, SEEK_SET);
      current_file = last_f;
      current_block = last_b - 1;
      Dmsg3(dbglevel, "set offset2=%lli %i:%i\n", 
	    last, current_file, current_block);

   } else if (last > 0) {
      lseek(fd, last, SEEK_SET);
      current_file = last_f;
      current_block = last_b;
      Dmsg3(dbglevel, "set offset=%lli %i:%i\n", 
	    last, current_file, current_block);
   } else {
      lseek(fd, orig, SEEK_SET);
      current_file = orig_f;
      current_block = orig_b;
      return -1;
   }

   Dmsg2(dbglevel, "bsr %i:%i\n", current_file, current_block);
   atEOT = atEOF = atEOD = false;

   return 0;
}

int faketape::bsf(int count)
{
   ASSERT(current_file >= 0);
   Dmsg3(dbglevel, "bsf %i:%i count=%i\n", current_file, current_block, count);
   int ret = 0;

   check_eof();
   atBOT = atEOF = atEOT = atEOD = false;

   if (current_file - count < 0) {
      current_file = 0;
      current_block = 0;
      atBOT = true;
      errno = EIO;
      ret = -1;
   } else {
      current_file = current_file - count + 1;
      current_block = -1;
      seek_file();
      current_file--;
      /* go just before last EOF */
      lseek(fd, lseek(fd, 0, SEEK_CUR) - sizeof(uint32_t), SEEK_SET);
   }
   return ret;
}

/* 
 * Put faketape in offline mode
 */
int faketape::offline()
{
   close();
   
   atEOF = false;		/* End of file */
   atEOT = false;		/* End of tape */
   atEOD = false;		/* End of data */
   atBOT = false;		/* Begin of tape */

   current_file = -1;
   current_block = -1;
   last_file = -1;
   return 0;
}

int faketape::close()
{
   check_eof();
   ::close(fd);
   fd = -1;
   return 0;
}
/*
 **rb
 **status
 * EOF Bacula status: file=2 block=0
 * Device status: EOF ONLINE IM_REP_EN file=2 block=0
 **rb
 **status
 * EOD EOF Bacula status: file=2 block=0
 * Device status: EOD ONLINE IM_REP_EN file=2 block=-1
 *
 */

int faketape::read(void *buffer, unsigned int count)
{
   ASSERT(current_file >= 0);
   unsigned int nb;
   uint32_t s;
   
   Dmsg2(dbglevel, "read %i:%i\n", current_file, current_block);

   if (atEOT || atEOD) {
      return 0;
   }

   if (atEOF) {
      current_file++;
      current_block=0;
      inplace = false;
      atEOF = false;
   }

   if (!inplace) {
      seek_file();
   }

   atEOD = atBOT = false;
   current_block++;

   nb = ::read(fd, &s, sizeof(uint32_t));
   if (nb <= 0) {
      atEOF = true;
      return 0;
   }
   if (s > count) {		/* not enough buffer to read block */
      lseek(fd, s, SEEK_CUR);
      errno = ENOMEM;
      return -1;
   }
   if (!s) {			/* EOF */
      atEOF = true;
      lseek(fd, lseek(fd, 0, SEEK_CUR) - sizeof(uint32_t), SEEK_SET);
      return 0;
   }
   nb = ::read(fd, buffer, s);
   if (s != nb) {
      atEOF = true;
      if (current_file == last_file) {
	 atEOD = true;
	 current_block = -1;
      }
      Dmsg0(dbglevel, "EOF during reading\n");
   }
   return nb;
}

int faketape::open(const char *pathname, int uflags)
{
   Dmsg2(dbglevel, "faketape::open(%s, %i)\n", pathname, uflags);
   pm_strcpy(volume, pathname);

   struct stat statp;   
   if (stat(volume, &statp) != 0) {
      Dmsg1(dbglevel, "Can't stat on %s\n", volume);
      return -1;
   }

   fd = ::open(pathname, O_CREAT | O_RDWR | O_LARGEFILE, 0700);
   if (fd < 0) {
      return -1;
   }

   /* open volume descriptor and get this->fd */
   if (find_maxfile() < 0) {
      return -1;
   }

   current_block = 0;
   current_file = 0;
   needEOF = false;
   online = inplace = true;
   atBOT = true;
   atEOT = atEOD = false;

   return fd;
}

/*
 * read volume to get the last file number
 */
int faketape::find_maxfile()
{
   struct stat statp;
   if (fstat(fd, &statp) != 0) {
      return 0;
   }
   last_file = statp.st_size>>FILE_OFFSET;
   file_size = statp.st_size;
      
   current_pos = lseek(fd, 0, SEEK_CUR); /* get current position */
   Dmsg3(dbglevel+1, "last_file=%i file_size=%u current_pos=%i\n", 
	 last_file, file_size, current_pos);

   return last_file;
}

int faketape::seek_file()
{
   ASSERT(current_file >= 0);
   Dmsg2(dbglevel, "seek_file %i:%i\n", current_file, current_block);

   off_t pos = ((off_t)current_file)<<FILE_OFFSET;
   if(lseek(fd, pos, SEEK_SET) == -1) {
      return -1;
   }
   last_file = (last_file > current_file)?last_file:current_file;
   if (current_block > 0) {
      fsr(current_block);
   }
   inplace = true;

   return 0;
}

void faketape::dump()
{
   Dmsg0(dbglevel+1, "===================\n");
   Dmsg2(dbglevel, "file:block = %i:%i\n", current_file, current_block);
   Dmsg1(dbglevel+1, "last_file=%i\n", last_file);
   Dmsg1(dbglevel+1, "volume=%s\n", volume);
   Dmsg1(dbglevel+1, "file_size=%i\n", file_size);  
   Dmsg4(dbglevel+1, "EOF=%i EOT=%i EOD=%i BOT=%i\n", 
	 atEOF, atEOT, atEOD, atBOT);  
}
