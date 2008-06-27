/*************************************************************************************************
 * The win32 compat API of Tokyo Cabinet
 *                                                      Copyright (C) 2006-2008 Mikio Hirabayashi
 *
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

#ifndef COMPAT_H
#define COMPAT_H

#ifdef HAVE_WIN32
#include <sys/types.h>

typedef int regoff_t;

typedef struct {
   regoff_t rm_so;
   regoff_t rm_eo;
} regmatch_t;

typedef struct {
   
} regex_t;

#define REG_EXTENDED 0
#define REG_NOSUB    0
#define REG_ICASE    0
#define REG_NOTBOL   0
int regcomp(regex_t *preg, const char *regex, int cflags);
int regexec(const regex_t *preg, const char *string, size_t nmatch, regmatch_t pmatch[], int eflags);
size_t regerror(int errcode, const regex_t *preg, char *errbuf, size_t errbuf_size);
void regfree(regex_t *preg);


/* mmap implementation for tokyodbm */
#define PROT_WRITE 0x2             /* Page can be written.  */
#define PROT_READ  0x1             /* page can be read */
#define MAP_SHARED 0x01            /* Share changes.  */
#define MAP_FAILED ((void *) -1)
#define MS_SYNC 0

void *mmap(void *start, size_t length, int prot, int flags,
           int fd, off_t offset);
int munmap(void *start, size_t length);
int msync(void *start, size_t length, int flags);
int fsync(int fd);
ssize_t pread(int fd, void *buf, size_t count, off_t offset);
ssize_t pwrite(int fd, const void *buf, size_t count, off_t offset);
int rand_r(int *seedp);

/*
struct timezone {
   int tz_minuteswest;
   int tz_dsttime;
};

int gettimeofday(struct timeval *tv, struct timezone *tz);
*/

typedef struct {
   size_t   gl_pathc;    /* Count of paths matched so far  */
   char   **gl_pathv;    /* List of matched pathnames.  */
   size_t   gl_offs;     /* Slots to reserve in ‘gl_pathv’.  */
} glob_t;

int glob(const char *pattern, int flags,
	 int (*errfunc) (const char *epath, int eerrno),
	 glob_t *pglob);

void globfree(glob_t *pglob);

#define GLOB_ERR     0
#define GLOB_NOSORT  0
#define GLOB_NOMATCH 0

char *realpath(const char *path, char *resolved_path);

#define lstat stat

#endif	/* HAVE_WIN32 */
#endif	/* !COMPAT_H */
