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

#ifdef USE_FAKETAPE

#define FTAPE_MAX_DRIVE 50

/* 
 * Theses functions will replace open/read/write
 */
int faketape_open(const char *pathname, int flags, ...);
int faketape_read(int fd, void *buffer, unsigned int count);
int faketape_write(int fd, const void *buffer, unsigned int count);
int faketape_close(int fd);
int faketape_ioctl(int fd, unsigned long int request, ...);
void faketape_debug(int level);

class faketape {
private:
   int         fd;              /* Our file descriptor */

   off_t       file_block;       /* size */
   off_t       max_block;

   bool        atEOF;           /* End of file */
   bool        atEOT;           /* End of media */
   bool        atEOD;           /* End of data */
   bool        atBOT;           /* Begin of tape */
   bool        online;          /* volume online */
   bool        inplace;         /* have to seek before writing ? */
   bool        needEOF;         /* check if last operation need eof */

   int32_t     last_file;       /* last file of the volume */
   int32_t     current_file;    /* current position */
   int32_t     current_block;   /* current position */

   void destroy();
   int find_maxfile();
   int offline();
   int truncate_file();
   int seek_file();
   void check_eof() { if(needEOF) weof(1);};
   void check_inplace() { if (!inplace) seek_file();};
   void update_pos();

public:
   int fsf(int count);
   int fsr(int count);
   int weof(int count);
   int bsf(int count);
   int bsr(int count);

   faketape();
   ~faketape();

   int get_fd();
   void dump();
   int open(const char *pathname, int flags);
   int read(void *buffer, unsigned int count);
   int write(const void *buffer, unsigned int count);
   int close();

   int tape_op(struct mtop *mt_com);
   int tape_get(struct mtget *mt_com);
   int tape_pos(struct mtpos *mt_com);
};

#endif /* USE_FAKETAPE */
#endif /* !FAKETAPE_H */
