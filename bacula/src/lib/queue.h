/*
 *  Written by John Walker MM
 *
 *   Version $Id$
 */
/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2000-2014 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from many
   others, a complete list can be found in the file AUTHORS.

   You may use this file and others of this release according to the
   license defined in the LICENSE file, which includes the Affero General
   Public License, v3.0 ("AGPLv3") and some additional permissions and
   terms pursuant to its AGPLv3 Section 7.

   Bacula® is a registered trademark of Kern Sibbald.
*/


/*  General purpose queue  */

struct b_queue {
        struct b_queue *qnext,     /* Next item in queue */
                     *qprev;       /* Previous item in queue */
};

typedef struct b_queue BQUEUE;

/*  Queue functions  */

void    qinsert(BQUEUE *qhead, BQUEUE *object);
BQUEUE *qnext(BQUEUE *qhead, BQUEUE *qitem);
BQUEUE *qdchain(BQUEUE *qitem);
BQUEUE *qremove(BQUEUE *qhead);
