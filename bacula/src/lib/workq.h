/*
 * Bacula work queue routines. Permits passing work to
 *  multiple threads.
 *
 *  Kern Sibbald, January MMI
 *
 *  This code adapted from "Programming with POSIX Threads", by
 *    David R. Butenhof
 *
 *   Version $Id$
 */
/*
   Copyright (C) 2001-2006 Kern Sibbald

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   version 2 as amended with additional clauses defined in the
   file LICENSE in the main source directory.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the 
   the file LICENSE for additional details.

 */

#ifndef __WORKQ_H
#define __WORKQ_H 1

/*
 * Structure to keep track of work queue request
 */
typedef struct workq_ele_tag {
   struct workq_ele_tag *next;
   void                 *data;
} workq_ele_t;

/*
 * Structure describing a work queue
 */
typedef struct workq_tag {
   pthread_mutex_t   mutex;           /* queue access control */
   pthread_cond_t    work;            /* wait for work */
   pthread_attr_t    attr;            /* create detached threads */
   workq_ele_t       *first, *last;   /* work queue */
   int               valid;           /* queue initialized */
   int               quit;            /* workq should quit */
   int               max_workers;     /* max threads */
   int               num_workers;     /* current threads */
   int               idle_workers;    /* idle threads */
   void             *(*engine)(void *arg); /* user engine */
} workq_t;

#define WORKQ_VALID  0xdec1992

extern int workq_init(
              workq_t *wq,
              int     threads,        /* maximum threads */
              void   *(*engine)(void *)   /* engine routine */
                    );
extern int workq_destroy(workq_t *wq);
extern int workq_add(workq_t *wq, void *element, workq_ele_t **work_item, int priority);
extern int workq_remove(workq_t *wq, workq_ele_t *work_item);

#endif /* __WORKQ_H */
