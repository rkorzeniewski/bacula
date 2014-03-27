/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2002-2014 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from many
   others, a complete list can be found in the file AUTHORS.

   You may use this file and others of this release according to the
   license defined in the LICENSE file, which includes the Affero General
   Public License, v3.0 ("AGPLv3") and some additional permissions and
   terms pursuant to its AGPLv3 Section 7.

   Bacula® is a registered trademark of Kern Sibbald.
*/
/*
 *
 *   Bootstrap Record header file
 *
 *      BSR (bootstrap record) handling routines split from
 *        ua_restore.c July MMIII
 *
 *     Kern Sibbald, July MMII
 *
 *   Version $Id$
 */



/* FileIndex entry in restore bootstrap record */
struct RBSR_FINDEX {
   RBSR_FINDEX *next;
   int32_t findex;
   int32_t findex2;
};

/*
 * Restore bootstrap record -- not the real one, but useful here
 *  The restore bsr is a chain of BSR records (linked by next).
 *  Each BSR represents a single JobId, and within it, it
 *    contains a linked list of file indexes for that JobId.
 *    The complete_bsr() routine, will then add all the volumes
 *    on which the Job is stored to the BSR.
 */
struct RBSR {
   RBSR *next;                        /* next JobId */
   JobId_t JobId;                     /* JobId this bsr */
   uint32_t VolSessionId;
   uint32_t VolSessionTime;
   int      VolCount;                 /* Volume parameter count */
   VOL_PARAMS *VolParams;             /* Volume, start/end file/blocks */
   RBSR_FINDEX *fi;                   /* File indexes this JobId */
   char *fileregex;                   /* Only restore files matching regex */
};

