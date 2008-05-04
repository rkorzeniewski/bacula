/*
   BaculaÂ® - The Network Backup Solution

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

   BaculaÂ® is a registered trademark of John Walker.
   The licensor of Bacula is the Free Software Foundation Europe
   (FSFE), Fiduciary Program, Sumatrastrasse 25, 8006 ZÃ¼rich,
   Switzerland, email:ftf@fsfeurope.org.
*/

#include "faketape.h"
#include <dirent.h>
#include <sys/mtio.h>
#include <ctype.h>

static int dbglevel = 10;

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

/****************************************************************/
/* theses function will replace open/read/write/close/ioctl
 * in bacula core
 */
int faketape_open(const char *pathname, int flags, int mode)
{
   ASSERT(pathname != NULL);

   int fd;
   faketape *tape = new faketape();
   fd = tape->open(pathname, flags, mode);
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
   }
//
//   case MTIOCPOS:
//      result = tape_pos(fd, va_arg(argp, mtpos *));
//      break;
//
   else {
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
      current_file = current_file - mt_com->mt_count;
      if (current_file < 0) {
	 current_file = 0;
	 errno = EIO;
	 result = -1;
      }
      atEOD = false;
      atEOF = false;
      atBOT = false;
      open_file();
      break;

   case MTFSR:	    /* Forward space over mt_count records (tape blocks). */
/*
    file number = 1
    block number = 0
   
    file number = 1
    block number = 1
   
    mt: /dev/lto2: Erreur d'entrée/sortie
   
    file number = 2
    block number = 0
*/
      /* tester si on se trouve a la fin du fichier */
      result = fsr(mt_com->mt_count);
      break;

   case MTBSR:      /* Backward space over mt_count records (tape blocks). */
      result = -1;
      break;

   case MTWEOF:			/* Write mt_count filemarks. */
      weof(mt_com->mt_count);
      break;

   case MTREW:			/* Rewind. */
      close_file();
      atEOF = atEOD = false;
      atBOT = true;
      current_file = 0;
      current_block = 0;
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
      atEOF = true;
      atEOD = true;
      close_file();
      current_file = last_file;
      current_block = -1;
      /* Ne pas creer le fichier si on est a la fin */

      break;

   case MTERASE:		/* not used by bacula */
      atEOD = true;
      atEOF = false;
      atEOT = false;
      current_file = 0;
      current_block = -1;
      delete_files(current_file);
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
//
//   switch (result) {
//   case NO_ERROR:
//   case -1:   /* Error has already been translated into errno */
//      break;
//
//   default:
//   case ERROR_FILEMARK_DETECTED:
//      errno = EIO;
//      break;
//
//   case ERROR_END_OF_MEDIA:
//      errno = EIO;
//      break;
//
//   case ERROR_NO_DATA_DETECTED:
//      errno = EIO;
//      break;
//
//   case ERROR_NO_MEDIA_IN_DRIVE:
//      errno = ENOMEDIUM;
//      break;
//
//   case ERROR_INVALID_HANDLE:
//   case ERROR_ACCESS_DENIED:
//   case ERROR_LOCK_VIOLATION:
//      errno = EBADF;
//      break;
//   }
//
   return result == 0 ? 0 : -1;
}

int faketape::tape_get(struct mtget *mt_get)
{
   int density = 1;

   mt_get->mt_type = MT_ISSCSI2;
   mt_get->mt_blkno = current_block;
   mt_get->mt_fileno = current_file;

   mt_get->mt_resid = -1;
//   pos_info.PartitionBlockValid ? pos_info.Partition : (ULONG)-1;

   /* TODO */
   mt_get->mt_dsreg = 
      ((density << MT_ST_DENSITY_SHIFT) & MT_ST_DENSITY_MASK) |
      ((tape_info.block_size << MT_ST_BLKSIZE_SHIFT) & MT_ST_BLKSIZE_MASK);


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
   return 0;
}

/*
 * This function try to emulate the append only behavior
 * of a tape. When you wrote something, data after the
 * current position are discarded.
 */
int faketape::delete_files(int startfile)
{  
   int cur,max=0;
   char *p;
   POOL_MEM tmp;
   DIR *fp_dir;
   struct dirent *dir;
   struct stat statp;

   Dmsg1(dbglevel, "delete_files %i\n", startfile);

   fp_dir = opendir(this->volume);
   if (!fp_dir) {
      this->last_file=0;
      this->size = 0;
      return -1;
   }

   this->size = 0;

   /* search for all digit files 
    * and we remove all ones that are greater than
    * startfile
    */
   while ((dir = readdir (fp_dir)) != NULL)
   {
      Mmsg(tmp, "%s/%s", this->volume, dir->d_name);
      cur = 0;
      /* check if d_name contains only digits */
      for(p = dir->d_name; *p && isdigit(*p); p++)
      {
	 cur *= 10;
	 cur += *p - '0';
      }
      if (!*p && cur > 0) {
	 if (cur >= startfile) { /* remove it */
	    unlink(tmp.c_str());
	 } else {
	    if (lstat(tmp.c_str(), &statp) == 0) {
	       this->size += statp.st_size;
	    }
	    max = (max > cur)?max:cur;
	 }
      }
   }

   closedir(fp_dir);
   this->last_file = max;
   return max;
}

faketape::faketape()
{
   fd = -1;
   cur_fd = -1;

   atEOF = false;
   atBOT = false;
   atEOT = false;
   atEOD = false;
   online = false;
   
   size = 0;
   last_file = 0;
   current_file = 0;
   current_block = -1;

   volume = get_pool_memory(PM_NAME);
   cur_file = get_pool_memory(PM_NAME);
   cur_info = get_pool_memory(PM_NAME);
}

faketape::~faketape()
{
   free_pool_memory(volume);
   free_pool_memory(cur_file);
   free_pool_memory(cur_info);
}

int faketape::get_fd()
{
   return this->fd;
}

int faketape::write(const void *buffer, unsigned int count)
{
   unsigned int nb;
   Dmsg2(dbglevel, "write len=%i blocks=%i\n", count, current_block);
   check_file();

   if (atEOT) {
      Dmsg0(dbglevel, "write nothing, EOT !\n");
      return 0;
   }

   if (current_block != -1) {
      current_block++;
   }

   atBOT = false;
   atEOD = true;

   /* TODO: remove all files > current_file and 
    * remove blocks > current_block 
    */
   if (count + size > tape_info.max_size) {
      Dmsg2(dbglevel, 
	    "EOT writing only %i of %i requested\n", 
	    tape_info.max_size - size, count);
      count = tape_info.max_size - size;
      atEOT = true;
   }

   ::write(cur_fd, &count, sizeof(count));
   nb = ::write(cur_fd, buffer, count);
   
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
   Dmsg2(dbglevel, "Writing EOF %i:%i\n", current_file, current_block);
   if (atEOT) {
      current_block = -1;
      return -1;
   }

   count--;			/* end this file */
   ftruncate(cur_fd, lseek(cur_fd, 0, SEEK_CUR));
   current_file++;
   open_file();

   /* we erase all previous information */
   if (last_file > current_file) {
      delete_files(current_file);
   }
   atEOF = true;
   atEOD = false;

   if (count > 0) {
      current_block = -1;
      return -1;
   } else {
      current_block = 0;
   }

   return 0;
}

int faketape::fsf(int count)
{

/*
 * 1 0 -> fsf -> 2 0 -> fsf -> 2 -1
 */
   if (atEOT) {
      current_block = -1;
      return -1;
   }

   close_file();

   atEOF=1;
   Dmsg3(dbglevel+1, "fsf %i+%i <= %i\n", current_file, count, last_file);

   if ((current_file + count) <= last_file) {
      current_file += count;
      current_block = 0;
      return 0;
   } else {
      Dmsg0(dbglevel, "Try to FSF after EOT\n");
      current_file = last_file ;
      current_block = -1;
      atEOD=true;
      return -1;
   }
}

int faketape::fsr(int count)
{
   int i,nb;
   off_t where=0, size;
   Dmsg2(dbglevel, "fsr current_block=%i count=%i\n", current_block, count);

   if (atEOT) {
      current_block = -1;
      return -1;
   }

   check_file();

   for(i=0; (i < count) && (where != -1) ; i++) {
      Dmsg3(dbglevel,"  fsr ok count=%i i=%i where=%i\n", count, i, where);
      nb = ::read(cur_fd, &size, sizeof(size));
      if (nb == sizeof(size)) {
	 where = lseek(cur_fd, size, SEEK_CUR);
	 if (where == -1) {
	    errno = EIO;
	    return -1;
	 }
	 current_block++;
      } else {
	 errno = EIO;
	 return -1;
      }
   }
   Dmsg2(dbglevel,"  fsr ok i=%i where=%i\n", i, where);
   return 0;
}

int faketape::bsf(int count)
{
   close_file();
   atEOT = atEOD = false;

   if (current_file - count < 0) {
      current_file = 0;
      current_block = 0;
      atBOT = true;
      return -1;
   }

   current_file = current_file - count;
   current_block = 0;
   if (!current_file) {
      atBOT = true;
   }

   return 1;
}

/* 
 * Put faketape in offline mode
 */
int faketape::offline()
{
   ASSERT(cur_fd > 0);

   close_file();

   cur_fd = -1;
   atEOF = false;
   atEOT = false;
   atEOD = false;
   atBOT = false;

   current_file = -1;
   current_block = -1;
   last_file = 0;
   return 0;
}

int faketape::close_file()
{
   Dmsg0(dbglevel, "close_file\n");
   if (cur_fd > 0) {
      ::close(cur_fd);
      cur_fd = -1;
   }
   return 0;
}

int faketape::close()
{
   close_file();
   ::close(fd);

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
   unsigned int nb, size;
   check_file();

   if (atEOT || atEOD) {
      return 0;
   }

   atBOT = false;
   current_block++;

   nb = ::read(cur_fd, &size, sizeof(size));
   if (size > count) {
      lseek(cur_fd, size, SEEK_CUR);
      errno = ENOMEM;
      return -1;
   }
   nb = ::read(cur_fd, buffer, size);
   if (size != nb) {
      atEOF = true;
      if (current_file == last_file) {
	 atEOD = true;
	 current_block = -1;
      }
      Dmsg0(dbglevel, "EOF during reading\n");
   }
   return nb;
}

int faketape::read_volinfo()
{
   struct stat statp;
   memset(&tape_info, 0, sizeof(FTAPE_FORMAT));

   Dmsg2(dbglevel, "read_volinfo %p %p\n", cur_info, volume);
   Mmsg(cur_info, "%s/info", volume);
   fd = ::open(cur_info, O_CREAT | O_RDWR | O_BINARY, 0640);
   
   if (fd < 0) {
      return -1;
   }
   
   fstat(fd, &statp);
   
   /* read volume info */
   int nb = ::read(fd, &tape_info, sizeof(FTAPE_FORMAT));
   if (nb != sizeof(FTAPE_FORMAT)) { /* new tape ? */
      Dmsg1(dbglevel, "Initialize %s\n", volume);
      tape_info.version = 1;
      tape_info.block_max = 2000000;
      tape_info.block_size = statp.st_blksize;
      tape_info.max_size = 10000000;

      lseek(fd, SEEK_SET, 0);
      nb = ::write(fd, &tape_info, sizeof(FTAPE_FORMAT));

      if (nb != sizeof(FTAPE_FORMAT)) {
	 ::close(fd);
	 return -1;
      }
   }

   Dmsg0(dbglevel, "read_volinfo OK\n");
   find_maxfile();

   return fd;
}

int faketape::open(const char *pathname, int uflags, int umode)
{
   Dmsg3(dbglevel, "faketape::open(%s, %i, %i)\n", pathname, uflags, umode);
   pm_strcpy(volume, pathname);

   struct stat statp;   
   if (stat(volume, &statp) != 0) {
      Dmsg1(dbglevel, "Can't stat on %s\n", volume);
      return -1;
   }

   if (!S_ISDIR(statp.st_mode)) {
      Dmsg1(dbglevel, "%s is not a directory\n", volume);
      errno = EACCES;
      return -1;
   }

   /* open volume descriptor and get this->fd */
   if (read_volinfo() < 0) {
      return -1;
   }

   current_block=-1;

   return fd;
}

/*
 * read volume directory to get the last file number
 */
int faketape::find_maxfile()
{
   int max=0;
   int cur;
   char *p;
   POOL_MEM tmp;
   DIR *fp_dir;
   struct dirent *dir;
   struct stat statp;

   fp_dir = opendir(this->volume);
   if (!fp_dir) {
      last_file=0;
      return -1;
   }

   this->size = 0;

   /* search for all digit file */
   while ((dir = readdir (fp_dir)) != NULL)
   {
      Mmsg(tmp, "%s/%s", this->volume, dir->d_name);
      if (lstat(tmp.c_str(), &statp) == 0) {
	 this->size += statp.st_size;
      } else {
	 Dmsg1(dbglevel, "Can't stat %s\n", tmp.c_str());
      }
      cur = 0;
      /* TODO: compute size */
      for(p = dir->d_name; *p && isdigit(*p); p++)
      {
	 cur *= 10;
	 cur += *p;
      }
      if (!*p && cur) {
	 max = (max > cur)?max:cur;
      }
   }

   closedir(fp_dir);
   this->last_file = max;
   return max;
}

int faketape::open_file() 
{
   ASSERT(current_file >= 0);
   close_file();

   Mmsg(cur_file, "%s/%i", volume, current_file);
   cur_fd = ::open(cur_file, O_CREAT | O_RDWR | O_BINARY, 0640);
   if (cur_fd < 0) {
      return -1;
   }
   current_block = 0;
   last_file = (last_file > current_file)?last_file:current_file;

   Dmsg1(dbglevel, "open_file %s\n", cur_file);

   return cur_fd;
}

void faketape::dump()
{
   Dmsg0(dbglevel+1, "===================\n");
   Dmsg2(dbglevel, "file:block = %i:%i\n", current_file, current_block);
   Dmsg1(dbglevel+1, "last_file=%i\n", last_file);
   Dmsg1(dbglevel+1, "volume=%s\n", volume);
   Dmsg1(dbglevel+1, "cur_file=%s\n", cur_file);  
   Dmsg1(dbglevel+1, "size=%i\n", size);  
   Dmsg1(dbglevel+1, "EOF=%i\n", atEOF);  
   Dmsg1(dbglevel+1, "EOT=%i\n", atEOT);  
   Dmsg1(dbglevel+1, "EOD=%i\n", atEOD);  
}

/****************************************************************

#define GMT_EOF(x)              ((x) & 0x80000000)
#define GMT_BOT(x)              ((x) & 0x40000000)
#define GMT_EOT(x)              ((x) & 0x20000000)
#define GMT_SM(x)               ((x) & 0x10000000)
#define GMT_EOD(x)              ((x) & 0x08000000)


 GMT_EOF(x) : La bande est positionnée juste après une filemark (toujours faux
     après une opération MTSEEK).

 GMT_BOT(x) : La bande est positionnée juste au début du premier fichier
     (toujours faux après une opération MTSEEK).
 
 GMT_EOT(x) : Une opération a atteint la fin physique de la bande (End Of
 Tape).

 GMT_SM(x) : La bande est positionnée sur une setmark (toujours faux après une
 opération MTSEEK).

 GMT_EOD(x) : La bande est positionnée à la fin des données enregistrées.


blkno = -1 (after MTBSF MTBSS or MTSEEK)
fileno = -1 (after MTBSS or MTSEEK)

*** mtx load
drive type = Generic SCSI-2 tape
drive status = 0
sense key error = 0
residue count = 0
file number = 0
block number = 0
Tape block size 0 bytes. Density code 0x0 (default).
Soft error count since last status=0
General status bits on (41010000):
 BOT ONLINE IM_REP_EN

*** read empty block
dd if=/dev/lto2 of=/tmp/toto count=1
dd: lecture de `/dev/lto2': Ne peut allouer de la mémoire
0+0 enregistrements lus
0+0 enregistrements écrits
1 octet (1B) copié, 4,82219 seconde, 0,0 kB/s

file number = 0
block number = 1

*** read file mark
dd if=/dev/lto2 of=/tmp/toto count=1
0+0 enregistrements lus
0+0 enregistrements écrits
1 octet (1B) copié, 0,167274 seconde, 0,0 kB/s

file number = 1
block number = 0

 *** write 2 blocks after rewind
dd if=/dev/zero of=/dev/lto2 count=2
2+0 enregistrements lus
2+0 enregistrements écrits
1024 octets (1,0 kB) copiés, 6,57402 seconde, 0,2 kB/s

file number = 1
block number = 0

*** write 2 blocks
file number = 2
block number = 0

*** rewind and fsr
file number = 0
block number = 1

*** rewind and 2x fsr (we have just 2 blocks)
file number = 0
block number = 2

*** fsr
mt: /dev/lto2: Erreur
file number = 1
block number = 0


 ****************************************************************/


#ifdef TEST

int main()
{
   int fd;
   char buf[500];
   printf("Starting FakeTape\n");

   mkdir("/tmp/fake", 0700);




   return 0;
}

#endif
