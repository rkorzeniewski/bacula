/*************************************************************************************************
 * The compat WIN32 API of Tokyo Cabinet
 *                                                      Copyright (C) 2006-2008 Mikio Hirabayashi
 * Tokyo Cabinet is free software; you can redistribute it and/or modify it under the terms of
 * the GNU Lesser General Public License as published by the Free Software Foundation; either
 * version 2.1 of the License or any later version.  Tokyo Cabinet is distributed in the hope
 * that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 * You should have received a copy of the GNU Lesser General Public License along with Tokyo
 * Cabinet; if not, write to the Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307 USA.
 *************************************************************************************************/

#ifdef HAVE_WIN32

#include "tcutil.h"
#include "myconf.h"
#include <windef.h>
#include <winbase.h>

int msync(void *start, size_t length, int flags)
{
   return 0;
}
int fsync(int fd)
{
   return 0;
}

/*
 * Keep track of HANDLE for CreateView
 */

struct mmap_priv
{
   HANDLE h;
   HANDLE mv;
};

static TCLIST *mmap_list=NULL;

static void mmap_register(HANDLE h, HANDLE mv)
{
   struct mmap_priv *priv;
/*
   if (!tcglobalmutexlock()) {
      return;
   }
*/
   if (!mmap_list) {
      mmap_list = tclistnew();
   }

   priv = tcmalloc(sizeof(struct mmap_priv));
   priv->h = h;
   priv->mv = mv;
   tclistpush(mmap_list, priv, sizeof(priv));
/*
   tcglobalmutexunlock();
*/
}

static HANDLE mmap_unregister(HANDLE mv)
{
   HANDLE h=NULL;
   struct mmap_priv *priv;
   int i, max, size;
   int nok=1;
/*
   if (!tcglobalmutexlock()) {
      return NULL;
   }
*/ 
   max = tclistnum(mmap_list);
   for(i=0; nok && i<max; i++) {
      priv = (struct mmap_priv *)tclistval(mmap_list, i, &size);
      if (priv && (priv->mv == mv)) {
	 tclistremove(mmap_list, i, &size);
	 h = priv->h;
	 tcfree(mmap_list);
	 nok=0;
      }
   }
/*
   tcglobalmutexunlock();
*/
   return h;
}


/*
 * Emulation of mmap and unmmap for tokyo dbm
 */
void *mmap(void *start, size_t length, int prot, int flags,
           int fd, off_t offset)
{
   DWORD fm_access = 0;
   DWORD mv_access = 0;
   HANDLE h;
   HANDLE mv;

   if (length == 0) {
      return MAP_FAILED;
   }
   if (!fd) {
      return MAP_FAILED;
   }

   if (flags & PROT_WRITE) {
      fm_access |= PAGE_READWRITE;
   } else if (flags & PROT_READ) {
      fm_access |= PAGE_READONLY;
   }

   if (flags & PROT_READ) {
      mv_access |= FILE_MAP_READ;
   }
   if (flags & PROT_WRITE) {
      mv_access |= FILE_MAP_WRITE;
   }

   h = CreateFileMapping((HANDLE)_get_osfhandle (fd),
                         NULL /* security */,
                         fm_access,
                         0 /* MaximumSizeHigh */,
                         0 /* MaximumSizeLow */,
                         NULL /* name of the file mapping object */);

   if (!h || h == INVALID_HANDLE_VALUE) {
      return MAP_FAILED;
   }

   mv = MapViewOfFile(h, mv_access,
                      0 /* offset hi */,
                      0 /* offset lo */,
                      length);
   if (!mv || mv == INVALID_HANDLE_VALUE) {
      CloseHandle(h);
      return MAP_FAILED;
   }

   mmap_register(h, mv);
   return (void *) mv;
}

int munmap(void *start, size_t length)
{
   HANDLE h;
   if (!start) {
      return -1;
   }
   h = mmap_unregister(start);
   UnmapViewOfFile(start);
   CloseHandle(h);
   return 0;
}

int regcomp(regex_t *preg, const char *regex, int cflags)
{
   return 0;
}

int regexec(const regex_t *preg, const char *string, size_t nmatch, regmatch_t pmatch[], int eflags)
{
   return 0;
}

size_t regerror(int errcode, const regex_t *preg, char *errbuf, size_t errbuf_size)
{
   *errbuf = 0;
   return 0;
}
/*
int gettimeofday(struct timeval *tv, struct timezone *tz)
{
   return -1;
}
*/
void regfree(regex_t *preg)
{
}

int glob(const char *pattern, int flags,
	 int (*errfunc) (const char *epath, int eerrno),
	 glob_t *pglob)
{
   return GLOB_NOMATCH;
}
void globfree(glob_t *pglob)
{
}

char *realpath(const char *path, char *resolved_path)
{
   strcpy(resolved_path, path);
   return resolved_path;
}

/*
int rand_r(unsigned int *seedp)
{
   *seedp = *seedp * 1103515245 + 12345;
   return((unsigned)(*seedp/65536) % 32768);
}
*/

/* read from a file descriptor at a given offset */
ssize_t pread(int fd, void *buf, size_t count, off_t offset)
{
   __int64 cur_pos;
   ssize_t num_read;

   if ((cur_pos = _lseeki64(fd, 0, SEEK_CUR)) == (off_t)-1)
      return -1;

   if (_lseeki64(fd, offset, SEEK_SET) == (off_t)-1)
      return -1;

   num_read = read(fd, buf, count);

   if (_lseeki64(fd, cur_pos, SEEK_SET) == (off_t)-1)
      return -1;

   return num_read;
}

/* write to a file descriptor at a given offset */
ssize_t pwrite(int fd, const void *buf, size_t count, off_t offset)
{
   __int64 cur_pos;
   ssize_t num_write;

   if ((cur_pos = _lseeki64(fd, 0, SEEK_CUR)) == (off_t)-1)
      return -1;

   if (_lseeki64(fd, offset, SEEK_SET) == (off_t)-1)
      return -1;

   num_write = write(fd, buf, count);

   if (_lseeki64(fd, cur_pos, SEEK_SET) == (off_t)-1)
      return -1;

   return num_write;
}

#endif
