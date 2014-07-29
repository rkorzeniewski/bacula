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
/*
 * Block definitions for Bacula media data format.
 *
 *    Written by Kern Sibbald, MM
 *
 */


#ifndef __BLOCK_H
#define __BLOCK_H 1

#define MAX_BLOCK_LENGTH  20000000      /* this is a sort of sanity check */
#define DEFAULT_BLOCK_SIZE (512 * 126)  /* 64,512 N.B. do not use 65,636 here */

/* Block Header definitions. */
#define BLKHDR1_ID       "BB01"
#define BLKHDR2_ID       "BB02"
#define BLKHDR_ID_LENGTH  4
#define BLKHDR_CS_LENGTH  4             /* checksum length */
#define BLKHDR1_LENGTH   16             /* Total length */
#define BLKHDR2_LENGTH   24             /* Total length */

#define WRITE_BLKHDR_ID     BLKHDR2_ID
#define WRITE_BLKHDR_LENGTH BLKHDR2_LENGTH
#define BLOCK_VER               2

/* Record header definitions */
#define RECHDR1_LENGTH      20
/*
 * Record header consists of:
 *  int32_t FileIndex
 *  int32_t Stream
 *  uint32_t data_length
 */
#define RECHDR2_LENGTH  (3*sizeof(int32_t))
#define WRITE_RECHDR_LENGTH RECHDR2_LENGTH

/* Tape label and version definitions */
#define BaculaId         "Bacula 1.0 immortal\n"
#define OldBaculaId      "Bacula 0.9 mortal\n"
#define BaculaTapeVersion                11
#define OldCompatibleBaculaTapeVersion1  10
#define OldCompatibleBaculaTapeVersion2   9

/*
 * This is the Media structure for a block header
 *  Note, when written, it is serialized.
   16 bytes

   uint32_t CheckSum;
   uint32_t block_len;
   uint32_t BlockNumber;
   char     Id[BLKHDR_ID_LENGTH];

 * for BB02 block, we have
   24 bytes

   uint32_t CheckSum;
   uint32_t block_len;
   uint32_t BlockNumber;
   char     Id[BLKHDR_ID_LENGTH];
   uint32_t VolSessionId;
   uint32_t VolSessionTime;

 */

class DEVICE;                         /* for forward reference */

/*
 * DEV_BLOCK for reading and writing blocks.
 * This is the basic unit that is written to the device, and
 * it contains a Block Header followd by Records.  Note,
 * at times (when reading a file), this block may contain
 * multiple blocks.
 *
 *  This is the memory structure for a device block.
 */
struct DEV_BLOCK {
   DEV_BLOCK *next;                   /* pointer to next one */
   DEVICE *dev;                       /* pointer to device */
   /* binbuf is the number of bytes remaining in the buffer.
    *   For writes, it is bytes not yet written.
    *   For reads, it is remaining bytes not yet read.
    */
   uint64_t BlockAddr;                /* Block address */
   uint32_t binbuf;                   /* bytes in buffer */
   uint32_t block_len;                /* length of current block read */
   uint32_t buf_len;                  /* max/default block length */
   uint32_t reclen;                   /* Last record length put in block */
   uint32_t BlockNumber;              /* sequential Bacula block number */
   uint32_t read_len;                 /* bytes read into buffer, if zero, block empty */
   uint32_t VolSessionId;             /* */
   uint32_t VolSessionTime;           /* */
   uint32_t read_errors;              /* block errors (checksum, header, ...) */
   uint32_t CheckSum;                 /* Block checksum */
   int      BlockVer;                 /* block version 1 or 2 */
   bool     write_failed;             /* set if write failed */
   bool     block_read;               /* set when block read */
   bool     needs_write;              /* block must be written */
   bool     no_header;                /* Set if no block header */
   bool     new_fi;                   /* New FI arrived */
   int32_t  FirstIndex;               /* first index this block */
   int32_t  LastIndex;                /* last index this block */
   int32_t  rechdr_items;             /* number of items in rechdr queue */
   char    *bufp;                     /* pointer into buffer */
   char     ser_buf[BLKHDR2_LENGTH];  /* Serial buffer for data */
   POOLMEM *rechdr_queue;             /* record header queue */
   POOLMEM *buf;                      /* actual data buffer */
};

#define block_is_empty(block) ((block)->read_len == 0)

#endif
