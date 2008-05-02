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

static int tape_get(int fd, struct mtget *mt_get);
static int tape_op(int fd, struct mtop *mt_com);
static int tape_pos(int fd, struct mtpos *mt_pos);

static int dbglevel = 0;


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

int faketape_open(const char *pathname, int flags, int mode)
{
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

//  switch (request) {
//  case MTIOCTOP:
        result = t->tape_op(va_arg(argp, mtop *));
//      break;

//   case MTIOCGET:
//      result = tape_get(fd, va_arg(argp, mtget *));
//      break;
//
//   case MTIOCPOS:
//      result = tape_pos(fd, va_arg(argp, mtpos *));
//      break;
//
//   default:
//      errno = ENOTTY;
//      result = -1;
//      break;
//   }

   va_end(argp);

   return result;
}

int faketape::tape_op(struct mtop *mt_com)
{
   int result=0;
   struct stat statp;
   
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
      /* we are already at EOT */
      if (current_file > max_file) {
	 atEOF = true;
	 errno = EIO;
	 result = -1;
      }

      /* we are at the last file */
      if (current_file == max_file) {
	 current_file++;
	 atEOF = true;
      }

      break;

   case MTBSF:			/* Backward space over mt_count filemarks. */
      current_file = current_file - mt_op->mt_count;
      if (current_file < 0) {
	 current_file = 0;
	 errno = EIO;
	 result = -1;
      }
      atEOD = false;
      atEOF = false;
      atEOM = false;
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
      fseek(cur_fd, SEEK_CUR, tape_info.block_size*mt_op->mt_count);
      break;

   case MTBSR:      /* Backward space over mt_count records (tape blocks). */

/*
   file number = 1
   block number = -1

   mt: /dev/lto2: Erreur d'entrée/sortie

   file number = 0
   block number = -1
*/

      fstat(cur_file, &statp);
      off_t cur_pos = lseek(cur_fd, 0, SEEK_CUR);

      fseek(cur_fd, SEEK_CUR, tape_info.block_size*mt_op->mt_count);
      break;

   case MTWEOF:			/* Write mt_count filemarks. */
      break;

   case MTREW:			/* Rewind. */
      atEOF = atEOD = atEOM = false;
      current_file = 0;
      open_file();
      break;

   case MTOFFL:			/* put tape offline */
      // check if can_read/can_write
      result = 0;
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
      atEOF = true;
      current_block = -1;
      /* Ne pas creer le fichier si on est a la fin */

      break;

   case MTERASE:
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
   return 0;
}

int faketape::tape_pos(struct mtpos *mt_pos)
{
   return 0;
}

int faketape::delete_files(int startfile)
{  
   int cur,max=0;
   char *p;
   POOL_MEM tmp;
   DIR *fp_dir;
   struct dirent *dir;
   struct stat statp;

   fp_dir = opendir(this->volume);
   if (!fp_dir) {
      this->max_file=0;
      this->size = 0;
      return -1;
   }

   this->size = 0;

   /* search for all digit file */
   while ((dir = readdir (fp_dir)) != NULL)
   {
      Mmsg(tmp, "%s/%s", this->volume, dir->d_name);
      cur = 0;
      /* check if d_name contains only digits */
      for(p = dir->d_name; *p && isdigit(*p); p++)
      {
	 cur *= 10;
	 cur += *p;
      }
      if (!*p && cur) {
	 if (cur >= startfile) { /* remove it */
	    unlink(tmp);
	 } else {
	    if (lstat(tmp.c_str(), &statp) == 0) {
	       this->size += statp.st_size;
	    }
	    max = (max > cur)?max:cur;
	 }
      }
   }

   closedir(fp_dir);
   this->max_file = max;
   return max;
}

faketape::faketape()
{
   fd = 0;
   atEOF = 0;
   atEOM = 0;
   current_file = 0;
   current_block = 0;
   max_file = 0;

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
   ASSERT(cur_fd > 0);
   if (current_block == -1) {
      open_file();
   }
   /* remove all file > current_file */
   return ::write(cur_fd, buffer, count);
}

int faketape::close()
{
   ASSERT(cur_fd > 0);
   ::close(cur_fd);
   ::close(fd);

   return 0;
}

int faketape::read(void *buffer, unsigned int count)
{
   ASSERT(cur_fd > 0);
   if (current_block == -1) {
      open_file();
   }
   return ::read(cur_fd, buffer, count);
}

int faketape::read_volinfo()
{
   struct stat statp;
   memset(&tape_info, 0, sizeof(FTAPE_FORMAT));

   Dmsg2(0, "Info %p %p\n", cur_info, volume);
   Mmsg(cur_info, "%s/info", volume);
   fd = ::open(cur_info, O_CREAT | O_RDWR | O_BINARY, 0640);
   
   if (fd < 0) {
      return -1;
   }
   
   fstat(cur_info, &statp);
   
   /* read volume info */
   int nb = ::read(fd, &tape_info, sizeof(FTAPE_FORMAT));
   if (nb != sizeof(FTAPE_FORMAT)) { /* new tape ? */
      Dmsg1(dbglevel, "Initialize %s\n", volume);
      tape_info.version = 1;
      tape_info.block_max = 2000000;
      tape_info.block_size = statp.st_blksize;
      tape_info.max_file_mark = 2000;

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
   if (lstat(volume, &statp) != 0) {
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
      max_file=0;
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
   this->max_file = max;
   return max;
}

int faketape::open_file() 
{
   if (cur_fd) {
      ::close(cur_fd);
   }

   Mmsg(cur_file, "%s/%i", volume, current_file);
   cur_fd = ::open(cur_file, O_CREAT | O_RDWR | O_BINARY, 0640);
   if (cur_fd < 0) {
      return -1;
   }
   current_block = 0;
   max_file = (max_file > current_file)?max_file:current_file;

   return cur_fd;

}

void faketape::dump()
{
   Dmsg1(dbglevel, "max_file=%i\n", max_file);
   Dmsg1(dbglevel, "current_file=%i\n", current_file);
   Dmsg1(dbglevel, "volume=%s\n", volume);
   Dmsg1(dbglevel, "cur_info=%s\n", cur_info);
   Dmsg1(dbglevel, "cur_file=%s\n", cur_file);  
   Dmsg1(dbglevel, "size=%i\n", size);  
}

#ifdef TEST

int main()
{
   int fd;
   printf("Starting FakeTape\n");

   mkdir("/tmp/fake", 0777);
   fd = faketape_open("/tmp/fake", O_CREAT | O_RDWR | O_BINARY, 0666);
   if (fd < 0) {
      perror("open ERR");
      return 1;
   }

   faketape *tape = get_tape(fd);
   tape->dump();

   faketape_close(fd);

   return 0;
}

#endif
