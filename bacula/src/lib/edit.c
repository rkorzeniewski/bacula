/*
 *   edit.c  edit string to ascii, and ascii to internal 
 * 
 *    Kern Sibbald, December MMII
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

/* We assume ASCII input and don't worry about overflow */
uint64_t str_to_uint64(char *str) 
{
   register char *p = str;
   register uint64_t value = 0;

   while (B_ISSPACE(*p)) {
      p++;
   }
   if (*p == '+') {
      p++;
   }
   while (B_ISDIGIT(*p)) {
      value = value * 10 + *p - '0';
      p++;
   }
   return value;
}

int64_t str_to_int64(char *str) 
{
   register char *p = str;
   register int64_t value;
   int negative = FALSE;

   while (B_ISSPACE(*p)) {
      p++;
   }
   if (*p == '+') {
      p++;
   } else if (*p == '-') {
      negative = TRUE;
      p++;
   }
   value = str_to_uint64(p);
   if (negative) {
      value = -value;
   }
   return value;
}



/*
 * Edit an integer number with commas, the supplied buffer
 * must be at least 27 bytes long.  The incoming number
 * is always widened to 64 bits.
 */
char *edit_uint64_with_commas(uint64_t val, char *buf)
{
   sprintf(buf, "%" lld, val);
   return add_commas(buf, buf);
}

/*
 * Edit an integer number, the supplied buffer
 * must be at least 27 bytes long.  The incoming number
 * is always widened to 64 bits.
 */
char *edit_uint64(uint64_t val, char *buf)
{
   sprintf(buf, "%" lld, val);
   return buf;
}


/*
 * Convert a string duration to utime_t (64 bit seconds)
 * Returns 0: if error
	   1: if OK, and value stored in value
 */
int duration_to_utime(char *str, utime_t *value)
{
   int i, ch, len;
   double val;
   static int  mod[] = {'*', 's', 'n', 'h', 'd', 'w', 'm', 'q', 'y', 0};
   static int mult[] = {1,    1,  60, 60*60, 60*60*24, 60*60*24*7, 60*60*24*30, 
		  60*60*24*91, 60*60*24*365};

   /* Look for modifier */
   len = strlen(str);
   ch = str[len - 1];
   i = 0;
   if (B_ISALPHA(ch)) {
      if (B_ISUPPER(ch)) {
	 ch = tolower(ch);
      }
      while (mod[++i] != 0) {
	 if (ch == mod[i]) {
	    len--;
	    str[len] = 0; /* strip modifier */
	    break;
	 }
      }
   }
   if (mod[i] == 0 || !is_a_number(str)) {
      return 0;
   }
   val = strtod(str, NULL);
   if (errno != 0 || val < 0) {
      return 0;
   }
   *value = (utime_t)(val * mult[i]);
   return 1;

}

/*
 * Edit a utime "duration" into ASCII
 */
char *edit_utime(utime_t val, char *buf)
{
   char mybuf[30];
   static int mult[] = {60*60*24*365, 60*60*24*30, 60*60*24, 60*60, 60};
   static char *mod[]  = {"year",  "month",  "day", "hour", "min"};
   int i;
   uint32_t times;

   *buf = 0;
   for (i=0; i<5; i++) {
      times = val / mult[i];
      if (times > 0) {
	 val = val - (utime_t)times * mult[i];
         sprintf(mybuf, "%d %s%s ", times, mod[i], times>1?"s":"");
	 strcat(buf, mybuf);
      }
   }
   if (val == 0 && strlen(buf) == 0) {	   
      strcat(buf, "0 secs");
   } else if (val != 0) {
      sprintf(mybuf, "%d sec%s", (uint32_t)val, val>1?"s":"");
      strcat(buf, mybuf);
   }
   return buf;
}

/*
 * Convert a size size in bytes to uint64_t
 * Returns 0: if error
	   1: if OK, and value stored in value
 */
int size_to_uint64(char *str, int str_len, uint64_t *rtn_value)
{
   int i, ch;
   double value;
   int mod[]  = {'*', 'k', 'm', 'g', 0}; /* first item * not used */
   uint64_t mult[] = {1,	     /* byte */
		      1024,	     /* kilobyte */
		      1048576,	     /* megabyte */
		      1073741824};   /* gigabyte */

#ifdef we_have_a_compiler_that_works
   int mod[]  = {'*', 'k', 'm', 'g', 't', 0};
   uint64_t mult[] = {1,	     /* byte */
		      1024,	     /* kilobyte */
		      1048576,	     /* megabyte */
		      1073741824,    /* gigabyte */
		      1099511627776};/* terabyte */
#endif

   Dmsg0(400, "Enter sized to uint64\n");

   /* Look for modifier */
   ch = str[str_len - 1];
   i = 0;
   if (B_ISALPHA(ch)) {
      if (B_ISUPPER(ch)) {
	 ch = tolower(ch);
      }
      while (mod[++i] != 0) {
	 if (ch == mod[i]) {
	    str_len--;
	    str[str_len] = 0; /* strip modifier */
	    break;
	 }
      }
   }
   if (mod[i] == 0 || !is_a_number(str)) {
      return 0;
   }
   Dmsg3(400, "size str=:%s: %f i=%d\n", str, strtod(str, NULL), i);

   value = (uint64_t)strtod(str, NULL);
   Dmsg1(400, "Int value = %d\n", (int)value);
   if (errno != 0 || value < 0) {
      return 0;
   }
   *rtn_value = (uint64_t)(value * mult[i]);
   Dmsg2(400, "Full value = %f %" lld "\n", strtod(str, NULL) * mult[i],
       value *mult[i]);
   return 1;
}

/*
 * Check if specified string is a number or not.
 *  Taken from SQLite, cool, thanks.
 */
int is_a_number(const char *n)
{
   int digit_seen = 0;

   if( *n == '-' || *n == '+' ) {
      n++;
   }
   while (B_ISDIGIT(*n)) {
      digit_seen = 1;
      n++;
   }
   if (digit_seen && *n == '.') {
      n++;
      while (B_ISDIGIT(*n)) { n++; }
   }
   if (digit_seen && (*n == 'e' || *n == 'E')
       && (B_ISDIGIT(n[1]) || ((n[1]=='-' || n[1] == '+') && B_ISDIGIT(n[2])))) {
      n += 2;			      /* skip e- or e+ or e digit */
      while (B_ISDIGIT(*n)) { n++; }
   }
   return digit_seen && *n==0;
}

/*
 * Check if the specified string is an integer	 
 */
int is_an_integer(const char *n)
{
   int digit_seen = 0;
   while (B_ISDIGIT(*n)) {
      digit_seen = 1;
      n++;
   }
   return digit_seen && *n==0;
}

/*
 * Add commas to a string, which is presumably
 * a number.  
 */
char *add_commas(char *val, char *buf)
{
   int len, nc;
   char *p, *q;
   int i;

   if (val != buf) {
      strcpy(buf, val);
   }
   len = strlen(buf);
   if (len < 1) {
      len = 1;
   }
   nc = (len - 1) / 3;
   p = buf+len;
   q = p + nc;
   *q-- = *p--;
   for ( ; nc; nc--) {
      for (i=0; i < 3; i++) {
	  *q-- = *p--;
      }
      *q-- = ',';
   }   
   return buf;
}
