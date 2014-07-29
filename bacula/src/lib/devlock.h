/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2001-2014 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from many
   others, a complete list can be found in the file AUTHORS.

   You may use this file and others of this release according to the
   license defined in the LICENSE file, which includes the Affero General
   Public License, v3.0 ("AGPLv3") and some additional permissions and
   terms pursuant to its AGPLv3 Section 7.

   Bacula® is a registered trademark of Kern Sibbald.
*/
/*
 * Bacula Thread Read/Write locking code. It permits
 *  multiple readers but only one writer.
 *
 *  Kern Sibbald, January MMI
 *
 *  This code adapted from "Programming with POSIX Threads", by
 *    David R. Butenhof
 *
 */

#ifndef __DEVLOCK_H
#define __DEVLOCK_H 1

struct take_lock_t {
   pthread_t  writer_id;              /* id of writer */
   int        reason;                 /* save reason */
   int        prev_reason;            /* previous reason */
};


class devlock {
private:
   pthread_mutex_t   mutex;
   pthread_cond_t    read;            /* wait for read */
   pthread_cond_t    write;           /* wait for write */
   pthread_t         writer_id;       /* writer's thread id */
   int               priority;        /* used in deadlock detection */
   int               valid;           /* set when valid */
   int               r_active;        /* readers active */
   int               w_active;        /* writers active */
   int               r_wait;          /* readers waiting */
   int               w_wait;          /* writers waiting */
   int               reason;          /* reason for lock */
   int               prev_reason;     /* previous reason */
   bool              can_take;        /* can the lock be taken? */


public:
   devlock(int reason, bool can_take=false);
   ~devlock();
   int init(int init_priority);
   int destroy();
   int take_lock(take_lock_t *hold, int reason);
   int return_lock(take_lock_t *hold);
   void new_reason(int nreason) { prev_reason = reason; reason = nreason; };
   void restore_reason() { reason = prev_reason; prev_reason = 0; };

   int writelock(int reason, bool can_take=false);
   int writetrylock();
   int writeunlock();
   void write_release();

   int readunlock();
   int readlock();
   int readtrylock();
   void read_release();

};


#define DEVLOCK_VALID  0xfadbec

#define DEVLOCK_INIIALIZER \
   {PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER, \
    PTHREAD_COND_INITIALIZER, DEVOCK_VALID, 0, 0, 0, 0}

#endif /* __DEVLOCK_H */
