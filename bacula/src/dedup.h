/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2005-2014 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from many
   others, a complete list can be found in the file AUTHORS.

   You may use this file and others of this release according to the
   license defined in the LICENSE file, which includes the Affero General
   Public License, v3.0 ("AGPLv3") and some additional permissions and
   terms pursuant to its AGPLv3 Section 7.

   Bacula® is a registered trademark of Kern Sibbald.
*/
/*
 * Block Level Global Deduplication common definitions 
 *
 * The main author of Bacula Deduplication code is Radoslaw Korzeniewski
 * 
 *  (c) Radosław Korzeniewski, MMXIV
 *  radoslaw@korzeniewski.net
 * 
 */

#ifndef _DEDUP_H
#define _DEDUP_H

#if defined (DEDUP_ENABLE)

/**
 * data deduplication block size
 */
#define DDUP_BLOCK_MAXLEN        65536
#define DDUP_BLOCK_MINLEN        1024

/**
 * deduplication block record type mask
 */
#define  DBLOCKREC_TYPE_MASK     0x000000FF
#define  DBLOCKREC_OPTIONS_MASK  0xFFFFFF00

/**
 * deduplication block record type
 * it will be used as lowest 8bit of 32bit deduplication record meta
 * written to volume
 */
#define  DBT_RAW_DATA            0
#define  DBT_DEDUP_DATA          1
#define  DBT_METADATA            2

/**
 * deduplication block record options
 * it will be used as a highest 24bit of 32bit deduplication record meta
 * written to volume
 */
#define  DBT_OPTION_LZO          3
#define  DBT_OPTION_LZ4          4

/**
 * DDUP metadata block struct 
 * 
 * block information record consist of:
 *    serialized block record type as uint32
 *    serialized size of the ddup block as uint32
 *    serialized size of the original backup block as uint32
 *    serialized block offset (position) in the file as uint64
 */
#define DBLOCKREC_LEN   ( sizeof (uint32_t)    /* record type */ \
                        + sizeof (uint32_t)    /* ddup block size */ \
                        + sizeof (uint32_t)    /* block length */ \
                        + sizeof (uint64_t) )  /* block offset */ \


/**
 * some dedupication command strings
 */
#define DDUP_CMD_CHECK_STORAGE         "check storage\n"
#define DDUP_CMD_CHECK_UNREF           "check unref\n"

/**
 * some dedupication response strings
 */
#define DDUP_RSP_CHECK_UNCHANGE        "3000 OK check unchange\n"

#endif /* DEDUP_ENABLE */
#endif /* _DEDUP_H */
