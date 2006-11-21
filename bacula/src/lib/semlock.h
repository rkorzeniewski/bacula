/*
 * Bacula Semaphore code. This code permits setting up
 *  a semaphore that lets through a specified number
 *  of callers simultaneously. Once the number of callers
 *  exceed the limit, they block.
 *
 *  Kern Sibbald, March MMIII
 *
 *   Derived from rwlock.h which was in turn derived from code in
 *     "Programming with POSIX Threads" By David R. Butenhof
 *
 *   Version $Id$
 */
/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2003-2006 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from
   many others, a complete list can be found in the file AUTHORS.
   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version two of the GNU General Public
   License as published by the Free Software Foundation plus additions
   that are listed in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

   Bacula® is a registered trademark o John Walker.
   The licensor of Bacula is the Free Software Foundation Europe
   (FSFE), Fiduciary Program, Sumatrastrasse 25, 8006 Zürich,
   Switzerland, email:ftf@fsfeurope.org.
*/

#ifndef __SEMLOCK_H
#define __SEMLOCK_H 1

typedef struct s_semlock_tag {
   pthread_mutex_t   mutex;           /* main lock */
   pthread_cond_t    wait;            /* wait for available slot */
   int               valid;           /* set when valid */
   int               waiting;         /* number of callers waiting */
   int               max_active;      /* maximum active callers */
   int               active;          /* number of active callers */
} semlock_t;

#define SEMLOCK_VALID  0xfacade

#define SEM_INIIALIZER \
   {PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER, \
    PTHREAD_COND_INITIALIZER, SEMLOCK_VALID, 0, 0, 0, 0}

/*
 * semaphore lock prototypes
 */
extern int sem_init(semlock_t *sem, int max_active);
extern int sem_destroy(semlock_t *sem);
extern int sem_lock(semlock_t *sem);
extern int sem_trylock(semlock_t *sem);
extern int sem_unlock(semlock_t *sem);

#endif /* __SEMLOCK_H */
