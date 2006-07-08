/*
 * Watchdog timer routines
 *
 *    Kern Sibbald, December MMII
 *
*/
/*
   Copyright (C) 2002-2006 Kern Sibbald

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   version 2 as amended with additional clauses defined in the
   file LICENSE in the main source directory.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the 
   the file LICENSE for additional details.

 */

enum {
   TYPE_CHILD = 1,
   TYPE_PTHREAD,
   TYPE_BSOCK
};

#define TIMEOUT_SIGNAL SIGUSR2

struct s_watchdog_t {
        bool one_shot;
        time_t interval;
        void (*callback)(struct s_watchdog_t *wd);
        void (*destructor)(struct s_watchdog_t *wd);
        void *data;
        /* Private data below - don't touch outside of watchdog.c */
        dlink link;
        time_t next_fire;
};
typedef struct s_watchdog_t watchdog_t;

/* Exported globals */
extern time_t DLL_IMP_EXP watchdog_time;             /* this has granularity of SLEEP_TIME */
extern time_t DLL_IMP_EXP watchdog_sleep_time;      /* examine things every 60 seconds */
