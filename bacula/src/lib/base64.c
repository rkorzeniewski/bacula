/*   
 *   Generic base 64 input and output routines
 *
 *    Written by Kern E. Sibbald, March MM.
 *
 *   Version $Id$
 */

/*
   Copyright (C) 2000, 2001, 2002 Kern Sibbald and John Walker

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

#ifdef TEST_MODE
#include <glob.h>
#endif


static uint8_t const base64_digits[64] =
{
  'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
  'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
  'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
  'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/'
};

static int base64_inited = 0;
static uint8_t base64_map[128];
  

/* Initialize the Base 64 conversion routines */
void
base64_init(void)
{     
   int i; 
   memset(base64_map, 0, sizeof(base64_map));
   for (i=0; i<64; i++)
      base64_map[(uint8_t)base64_digits[i]] = i;
   base64_inited = 1;
}

/* Convert a value to base64 characters.
 * The result is stored in where, which
 * must be at least 8 characters long.
 *
 * Returns the number of characters
 * stored (not including the EOS).
 */
int
to_base64(intmax_t value, char *where)
{
   uintmax_t val;
   int i = 0;
   int n;

   /* Handle negative values */
   if (value < 0) {
      where[i++] = '-';
      value = -value;
   }

   /* Determine output size */
   val = value;
   do {
      val >>= 6;
      i++;
   } while (val);
   n = i;

   /* Output characters */
   val = value;
   where[i] = 0;
   do {
      where[--i] = base64_digits[val & (uintmax_t)0x3F];
      val >>= 6;
   } while (val);
   return n;
}

/*
 * Convert the Base 64 characters in where to
 * a value. No checking is done on the validity
 * of the characters!!
 *
 * Returns the value.
 */
int
from_base64(intmax_t *value, char *where)
{ 
   uintmax_t val = 0;
   int i, neg;

   if (!base64_inited) 
      base64_init();
   /* Check if it is negative */
   i = neg = 0;
   if (where[i] == '-') {
      i++;
      neg = 1;
   }
   /* Construct value */
   while (where[i] != 0 && where[i] != ' ') {
      val <<= 6;
      val += base64_map[(uint8_t)where[i++]];
   }
	 
   *value = neg ? -(intmax_t)val : (intmax_t)val;
   return i;
}

/* Encode a stat structure into a base64 character string */
void
encode_stat(char *buf, struct stat *statp)
{
   char *p = buf;
   /*
    * NOTE: we should use rdev as major and minor device if
    * it is a block or char device (S_ISCHR(statp->st_mode)
    * or S_ISBLK(statp->st_mode)).  In all other cases,
    * it is not used.	
    *
    */
   p += to_base64((intmax_t)statp->st_dev, p);
   *p++ = ' ';                        /* separate fields with a space */
   p += to_base64((intmax_t)statp->st_ino, p);
   *p++ = ' ';
   p += to_base64((intmax_t)statp->st_mode, p);
   *p++ = ' ';
   p += to_base64((intmax_t)statp->st_nlink, p);
   *p++ = ' ';
   p += to_base64((intmax_t)statp->st_uid, p);
   *p++ = ' ';
   p += to_base64((intmax_t)statp->st_gid, p);
   *p++ = ' ';
   p += to_base64((intmax_t)statp->st_rdev, p);
   *p++ = ' ';
   p += to_base64((intmax_t)statp->st_size, p);
   *p++ = ' ';
   p += to_base64((intmax_t)statp->st_blksize, p);
   *p++ = ' ';
   p += to_base64((intmax_t)statp->st_blocks, p);
   *p++ = ' ';
   p += to_base64((intmax_t)statp->st_atime, p);
   *p++ = ' ';
   p += to_base64((intmax_t)statp->st_mtime, p);
   *p++ = ' ';
   p += to_base64((intmax_t)statp->st_ctime, p);
   *p++ = 0;
   return;
}


/* Decode a stat packet from base64 characters */
void
decode_stat(char *buf, struct stat *statp)
{
   char *p = buf;
   intmax_t val;

   p += from_base64(&val, p);
   statp->st_dev = val;
   p++; 			      /* skip space */
   p += from_base64(&val, p);
   statp->st_ino = val;
   p++;
   p += from_base64(&val, p);
   statp->st_mode = val;
   p++;
   p += from_base64(&val, p);
   statp->st_nlink = val;
   p++;
   p += from_base64(&val, p);
   statp->st_uid = val;
   p++;
   p += from_base64(&val, p);
   statp->st_gid = val;
   p++;
   p += from_base64(&val, p);
   statp->st_rdev = val;
   p++;
   p += from_base64(&val, p);
   statp->st_size = val;
   p++;
   p += from_base64(&val, p);
   statp->st_blksize = val;
   p++;
   p += from_base64(&val, p);
   statp->st_blocks = val;
   p++;
   p += from_base64(&val, p);
   statp->st_atime = val;
   p++;
   p += from_base64(&val, p);
   statp->st_mtime = val;
   p++;
   p += from_base64(&val, p);
   statp->st_ctime = val;
   p++;
}

/*
 * Encode binary data in bin of len bytes into
 * buf as base64 characters.
 *
 *  Returns: the number of characters stored not
 *	     including the EOS
 */
int
bin_to_base64(char *buf, char *bin, int len)
{
   uint32_t reg, save, mask;
   int rem, i;
   int j = 0;

   reg = 0;
   rem = 0;
   for (i=0; i<len; ) {
      if (rem < 6) {
	 reg <<= 8;
	 reg |= (uint8_t)bin[i++];
	 rem += 8;
      }
      save = reg;
      reg >>= (rem - 6);
      buf[j++] = base64_digits[reg & (uint32_t)0x3F];
      reg = save;
      rem -= 6;
   }
   if (rem) {
      mask = 1;
      for (i=1; i<rem; i++) {
	 mask = (mask << 1) | 1;
      }
      buf[j++] = base64_digits[reg & mask];
   }
   buf[j] = 0;
   return j;
}

#ifdef BIN_TEST
int main(int argc, char *argv[])
{
   int xx = 0;
   int len;
   char buf[100];
   char junk[100];
   int i;

   for (i=0; i < 100; i++) {
      bin_to_base64(buf, (char *)&xx, 4);
      printf("xx=%s\n", buf);
      xx++;
   }
   len = bin_to_base64(buf, junk, 16);
   printf("len=%d junk=%s\n", len, buf);
   return 0;
}
#endif

#ifdef TEST_MODE
static int errfunc(const char *epath, int eernoo)
{
  Dmsg0(-1, "in errfunc\n");
  return 1;
}


/*
 * Test the base64 routines by encoding and decoding
 * lstat() packets.
 */
int main(int argc, char *argv[]) 
{
   char where[500];
   int i;
   glob_t my_glob;
   char *fname;
   struct stat statp;
   struct stat statn;
   int debug_level = 0;

   if (argc > 1 && strcmp(argv[1], "-v") == 0)
      debug_level++;  

   base64_init();

   my_glob.gl_offs = 0;
   glob("/etc/*", GLOB_MARK, errfunc, &my_glob);

   for (i=0; my_glob.gl_pathv[i]; i++) {
      fname = my_glob.gl_pathv[i];
      if (lstat(fname, &statp) < 0) {
         printf("Cannot stat %s: %s\n", fname, strerror(errno));
	 continue;
      }
      encode_stat(where, &statp);

      if (debug_level)
         printf("%s: len=%d val=%s\n", fname, strlen(where), where);
      
      decode_stat(where, &statn);

      if (statp.st_dev != statn.st_dev || 
	  statp.st_ino != statn.st_ino ||
	  statp.st_mode != statn.st_mode ||
	  statp.st_nlink != statn.st_nlink ||
	  statp.st_uid != statn.st_uid ||
	  statp.st_gid != statn.st_gid ||
	  statp.st_rdev != statn.st_rdev ||
	  statp.st_size != statn.st_size ||
	  statp.st_blksize != statn.st_blksize ||
	  statp.st_blocks != statn.st_blocks ||
	  statp.st_atime != statn.st_atime ||
	  statp.st_mtime != statn.st_mtime ||
	  statp.st_ctime != statn.st_ctime) {

         printf("%s: %s\n", fname, where);
	 encode_stat(where, &statn);
         printf("%s: %s\n", fname, where);
         printf("NOT EQAL\n");
      }

   }
   globfree(&my_glob);

   printf("%d files examined\n", i);

   return 0;
}   
#endif
