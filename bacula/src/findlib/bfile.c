/*
 *  Bacula low level File I/O routines.  This routine simulates
 *    open(), read(), write(), and close(), but using native routines.
 *    I.e. on Windows, we use Windows APIs.
 *
 *    Kern Sibbald, April MMIII
 *
 *   Version $Id$
 *
 */
/*
   Copyright (C) 2000-2003 Kern Sibbald and John Walker

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
#include "find.h"


void binit(BFILE *bfd)
{
   bfd->fid = -1;
}

int bopen(BFILE *bfd, const char *pathname, int flags, mode_t mode)
{
   return bfd->fid = open(pathname, flags, mode);
}

int bclose(BFILE *bfd)
{ 
   int stat = close(bfd->fid);
   bfd->fid = -1;
   return stat;
}

ssize_t bread(BFILE *bfd, void *buf, size_t count)
{
   return read(bfd->fid, buf, count);
}

ssize_t bwrite(BFILE *bfd, void *buf, size_t count)
{
   return write(bfd->fid, buf, count);
}

int is_bopen(BFILE *bfd)
{
   return bfd->fid >= 0;
}

off_t blseek(BFILE *bfd, off_t offset, int whence)
{
   return lseek(bfd->fid, offset, whence);
}
