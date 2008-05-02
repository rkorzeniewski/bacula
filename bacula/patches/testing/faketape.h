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
 * faketape.h - Emulate the Linux st (scsi tape) driver on file.
 * for regression and bug hunting purpose
 *
 */

#ifndef FAKETAPE_H
#define FAKETAPE_H

#include <stdarg.h>
#include <stddef.h>
#include "bacula.h"

typedef struct
{
   /* format infos */
   int16_t     version;
   int16_t     max_file_mark;
   int16_t     block_size;
   int32_t     block_max;
} FTAPE_FORMAT;

#define FTAPE_MAX_DRIVE 20

/* 
 * Theses functions will replace open/read/write
 */
int faketape_open(const char *pathname, int flags, int mode);
int faketape_read(int fd, void *buffer, unsigned int count);
int faketape_write(int fd, const void *buffer, unsigned int count);
int faketape_close(int fd);
int faketape_ioctl(int fd, unsigned long int request, ...);

class faketape {
private:
   int         fd;		/* Our file descriptor */

   int         cur_fd;		/* OS file descriptor */
   off_t       size;		/* size */

   bool        atEOF;		/* End of file */
   bool        atEOM;		/* End of media */
   bool        atEOD;		/* End of data */

   POOLMEM     *volume;		/* volume name */
   POOLMEM     *cur_info;	/* volume info */
   POOLMEM     *cur_file;	/* current file name */

   int16_t     max_file;
   int16_t     current_file;	/* max 65000 files */
   int32_t     current_block;	/* max 4G blocks of 1KB */

   FTAPE_FORMAT tape_info;

   void destroy();
   int read_volinfo();		     /* read current volume format */
   int find_maxfile();
   int open_file();
   int delete_files();

public:
   faketape();
   ~faketape();

   int get_fd();
   void dump();
   int open(const char *pathname, int flags, int mode);
   int read(void *buffer, unsigned int count);
   int write(const void *buffer, unsigned int count);
   int close();

   int tape_op(struct mtop *mt_com);
   int tape_get(struct mtget *mt_com);
   int tape_pos(struct mtpos *mt_com);
};

#endif /* !FAKETAPE_H */
