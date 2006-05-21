/*
 * Process and thread timer routines, built on top of watchdogs.
 *
 *    Nic Bellamy <nic@bellamy.co.nz>, October 2003.
 *
*/
/*
   Copyright (C) 2000-2006 Kern Sibbald

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   version 2 as amended with additional clauses defined in the
   file LICENSE in the main source directory.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the 
   the file LICENSE for additional details.

 */

typedef struct s_btimer_t {
   watchdog_t *wd;                    /* Parent watchdog */
   int type;
   bool killed;
   pid_t pid;                         /* process id if TYPE_CHILD */
   pthread_t tid;                     /* thread id if TYPE_PTHREAD */
   BSOCK *bsock;                      /* Pointer to BSOCK */
} btimer_t;

/* EOF */
