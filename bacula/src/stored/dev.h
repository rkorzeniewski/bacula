/*
 * Definitions for using the Device functions in Bacula
 *  Tape and File storage access
 *
 *   Version $Id$
 *
 */
/*
   Copyright (C) 2000, 2001, 2002 Kern Sibbald and John Walker

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of
   the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA.

 */


#ifndef __DEV_H
#define __DEV_H 1

/* Arguments to open_dev() */
#define READ_WRITE 0
#define READ_ONLY  1

/* Generic status bits returned from status_dev() */
#define MT_TAPE      (1<<0)		   /* is tape device */
#define MT_EOF	     (1<<1)		   /* just read EOF */
#define MT_BOT	     (1<<2)		   /* at beginning of tape */
#define MT_EOT	     (1<<3)		   /* end of tape reached */
#define MT_SM	     (1<<4)		   /* DDS setmark */
#define MT_EOD	     (1<<5)		   /* DDS at end of data */
#define MT_WR_PROT   (1<<6)		   /* tape write protected */
#define MT_ONLINE    (1<<7)		   /* tape online */
#define MT_DR_OPEN   (1<<8)		   /* tape door open */
#define MT_IM_REP_EN (1<<9)		   /* immediate report enabled */


/* Bits for device capabilities */
#define CAP_EOF        0x001	      /* has MTWEOF */
#define CAP_BSR        0x002	      /* has MTBSR */
#define CAP_BSF        0x004	      /* has MTBSF */
#define CAP_FSR        0x008	      /* has MTFSR */
#define CAP_FSF        0x010	      /* has MTFSF */
#define CAP_EOM        0x020	      /* has MTEOM */
#define CAP_REM        0x040	      /* is removable media */
#define CAP_RACCESS    0x080	      /* is random access device */
#define CAP_AUTOMOUNT  0x100	      /* Read device at start to see what is there */
#define CAP_LABEL      0x200	      /* Label blank tapes */
#define CAP_ANONVOLS   0x400	      /* Mount without knowing volume name */
#define CAP_ALWAYSOPEN 0x800	      /* always keep device open */


/* Tape state bits */
#define ST_OPENED    0x001	      /* set when device opened */
#define ST_TAPE      0x002	      /* is a tape device */  
#define ST_LABEL     0x004	      /* label found */
#define ST_MALLOC    0x008            /* dev packet malloc'ed in init_dev() */
#define ST_APPEND    0x010	      /* ready for Bacula append */
#define ST_READ      0x020	      /* ready for Bacula read */
#define ST_EOT	     0x040	      /* at end of tape */
#define ST_WEOT      0x080	      /* Got EOT on write */
#define ST_EOF	     0x100	      /* Read EOF i.e. zero bytes */
#define ST_NEXTVOL   0x200	      /* Start writing on next volume */
#define ST_SHORT     0x400	      /* Short block read */

/* dev_blocked states (mutually exclusive) */
#define BST_NOT_BLOCKED       0       /* not blocked */
#define BST_UNMOUNTED	      1       /* User unmounted device */
#define BST_WAITING_FOR_SYSOP 2       /* Waiting for operator to mount tape */
#define BST_DOING_ACQUIRE     3       /* Opening/validating/moving tape */
#define BST_WRITING_LABEL     4       /* Labeling a tape */  
#define BST_UNMOUNTED_WAITING_FOR_SYSOP 5 /* Closed by user during mount request */

/* Volume Catalog Information structure definition */
typedef struct s_volume_catalog_info {
   /* Media info for the current Volume */
   uint32_t VolCatJobs; 	      /* number of jobs on this Volume */
   uint32_t VolCatFiles;	      /* Number of files */
   uint32_t VolCatBlocks;	      /* Number of blocks */
   uint64_t VolCatBytes;	      /* Number of bytes written */
   uint32_t VolCatMounts;	      /* Number of mounts this volume */
   uint32_t VolCatErrors;	      /* Number of errors this volume */
   uint32_t VolCatWrites;	      /* Number of writes this volume */
   uint32_t VolCatReads;	      /* Number of reads this volume */
   uint32_t VolCatRecycles;	      /* Number of recycles this volume */
   uint64_t VolCatMaxBytes;	      /* max bytes to write */
   uint64_t VolCatCapacityBytes;      /* capacity estimate */
   char VolCatStatus[20];	      /* Volume status */
   char VolCatName[MAX_NAME_LENGTH];  /* Desired volume to mount */
} VOLUME_CAT_INFO;


/* Device structure definition */
typedef struct s_device {
   struct s_device *next;	      /* pointer to next open device */
   pthread_mutex_t mutex;	      /* access control */
   pthread_cond_t wait; 	      /* thread wait variable */
   pthread_cond_t wait_next_vol;      /* wait for tape to be mounted */
   pthread_t no_wait_id;	      /* this thread must not wait */
   int dev_blocked;		      /* set if we must wait (i.e. change tape) */
   int num_waiting;		      /* number of threads waiting */
   int num_writers;		      /* number of writing threads */
   int use_count;		      /* usage count on this device */
   int fd;			      /* file descriptor */
   int capabilities;		      /* capabilities mask */
   int state;			      /* state mask */
   int dev_errno;		      /* Our own errno */
   int mode;			      /* read/write modes */
   char *dev_name;		      /* device name */
   char *errmsg;		      /* nicely edited error message */
   uint32_t block_num;		      /* current block number base 0 */
   uint32_t file;		      /* current file number base 0 */
   uint32_t LastBlockNumWritten;      /* last block written */
   uint32_t min_block_size;	      /* min block size */
   uint32_t max_block_size;	      /* max block size */
   uint32_t max_volume_jobs;	      /* max jobs to put on one volume */
   int64_t max_volume_files;	      /* max files to put on one volume */
   int64_t max_volume_size;	      /* max bytes to put on one volume */
   int64_t max_file_size;	      /* max file size in bytes */
   int64_t volume_capacity;	      /* advisory capacity */
   uint32_t max_rewind_wait;	      /* max secs to allow for rewind */
   void *device;		      /* pointer to Device Resource */

   VOLUME_CAT_INFO VolCatInfo;	      /* Volume Catalog Information */
   struct Volume_Label VolHdr;	      /* Actual volume label */

} DEVICE;




#ifdef SunOS
#define DEFAULT_TAPE_DRIVE "/dev/rmt/0cbn"
#endif
#ifdef AIX
#define DEFAULT_TAPE_DRIVE "/dev/rmt0.1"
#endif
#ifdef SGI
#define DEFAULT_TAPE_DRIVE "/dev/tps0d4nr"
#endif
#ifdef Linux
#define DEFAULT_TAPE_DRIVE "/dev/nst0"
#endif
#ifdef OSF
#define DEFAULT_TAPE_DRIVE "/dev/nrmt0"
#endif
#ifdef HPUX
#define DEFAULT_TAPE_DRIVE "/dev/rmt/0hnb"
#endif
#ifdef FreeBSD
#define DEFAULT_TAPE_DRIVE "/dev/nrst0"
#endif

/* Default default */
#ifndef DEFAULT_TAPE_DRIVE
#define DEFAULT_TAPE_DRIVE "/dev/nst0"
#endif

/* Get some definition of function to position
 *  to the end of the medium in MTEOM. System
 *  dependent. Arrgggg!
 */
#ifndef MTEOM
#ifdef	MTSEOD
#define MTEOM MTSEOD
#endif
#ifdef MTEOD
#undef MTEOM
#define MTEOM MTEOD
#endif
#endif

#endif
