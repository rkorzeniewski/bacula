/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2009-2014 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from many
   others, a complete list can be found in the file AUTHORS.

   You may use this file and others of this release according to the
   license defined in the LICENSE file, which includes the Affero General
   Public License, v3.0 ("AGPLv3") and some additional permissions and
   terms pursuant to its AGPLv3 Section 7.

   Bacula® is a registered trademark of Kern Sibbald.
*/

/*
 *  Marco van Wieringen, August 2009
 */

#ifndef _MNTENT_CACHE_H
#define _MNTENT_CACHE_H 1

/*
 * Don't use the mountlist data when its older then this amount
 * of seconds but perform a rescan of the mountlist.
 */
#define MNTENT_RESCAN_INTERVAL		1800

/*
 * Initial size of number of hash entries we expect in the cache.
 * If more are needed the hash table will grow as needed.
 */
#define NR_MNTENT_CACHE_ENTRIES		256

/*
 * Number of pages to allocate for the big_buffer used by htable.
 */
#define NR_MNTENT_HTABLE_PAGES		32

struct mntent_cache_entry_t {
   hlink link;
   uint32_t dev;
   char *special;
   char *mountpoint;
   char *fstype;
   char *mntopts;
};

mntent_cache_entry_t *find_mntent_mapping(uint32_t dev);
void flush_mntent_cache(void);

#endif /* _MNTENT_CACHE_H */
