
/*  

    Time and date structures and functions.
    Date and time are always represented internally
    as 64 bit floating point Julian day numbers and
    fraction.  The day number and fraction are kept
    as separate quantities to avoid round-off of
    day fraction. John Walker

     Version $Id$

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


#ifndef __btime_INCLUDED
#define __btime_INCLUDED

/* New btime definition -- use this */
extern btime_t get_current_btime(void);
extern time_t btime_to_unix(btime_t bt);   /* bacula time to epoch time */
extern utime_t btime_to_utime(btime_t bt); /* bacula time to utime_t */

extern void bstrftime(char *dt, int maxlen, utime_t tim);


/* =========================================================== */
/*        old code deprecated below. Do not use.               */

typedef float64_t fdate_t;             /* Date type */
typedef float64_t ftime_t;             /* Time type */

struct date_time {
    fdate_t julian_day_number;         /* Julian day number */
    ftime_t julian_day_fraction;       /* Julian day fraction */
};

/*  In arguments and results of the following functions,
    quantities are expressed as follows.

        year    Year in the Common Era.  The canonical
                date of adoption of the Gregorian calendar
                (October 5, 1582 in the Julian calendar)
                is assumed.

        month   Month index with January 0, December 11.

        day     Day number of month, 1 to 31.

*/


extern fdate_t date_encode(uint32_t year, uint8_t month, uint8_t day);
extern ftime_t time_encode(uint8_t hour, uint8_t minute, uint8_t second,
                          float32_t second_fraction);
extern void date_time_encode(struct date_time *dt,
                             uint32_t year, uint8_t month, uint8_t day,
                             uint8_t hour, uint8_t minute, uint8_t second,
                             float32_t second_fraction);

extern void date_decode(fdate_t date, uint32_t *year, uint8_t *month,
                        uint8_t *day);
extern void time_decode(ftime_t time, uint8_t *hour, uint8_t *minute,
                        uint8_t *second, float32_t *second_fraction);
extern void date_time_decode(struct date_time *dt,
                             uint32_t *year, uint8_t *month, uint8_t *day,
                             uint8_t *hour, uint8_t *minute, uint8_t *second,
                             float32_t *second_fraction);

extern int date_time_compare(struct date_time *dt1, struct date_time *dt2);

extern void tm_encode(struct date_time *dt, struct tm *tm);
extern void tm_decode(struct date_time *dt, struct tm *tm);
extern void get_current_time(struct date_time *dt);
extern utime_t str_to_utime(char *str);


#endif /* __btime_INCLUDED */
