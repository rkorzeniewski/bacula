/*
 * Definitions for using the Device functions in Bacula
 *  Tape and File storage access
 *
 *   Version $Id$
 *
 */
/*
   Copyright (C) 2000-2003 Kern Sibbald and John Walker

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

/* #define NEW_LOCK 1 */

#define new_lock_device(dev)             _new_lock_device(__FILE__, __LINE__, (dev)) 
#define new_lock_device_state(dev,state) _new_lock_device(__FILE__, __LINE__, (dev), (state))
#define new_unlock_device(dev)           _new_unlock_device(__FILE__, __LINE__, (dev))

#define lock_device(d) _lock_device(__FILE__, __LINE__, (d))
#define unlock_device(d) _unlock_device(__FILE__, __LINE__, (d))
#define block_device(d, s) _block_device(__FILE__, __LINE__, (d), s)
#define unblock_device(d) _unblock_device(__FILE__, __LINE__, (d))
#define steal_device_lock(d, p, s) _steal_device_lock(__FILE__, __LINE__, (d), (p), s)
#define give_back_device_lock(d, p) _give_back_device_lock(__FILE__, __LINE__, (d), (p))

/* Arguments to open_dev() */
#define READ_WRITE       0
#define READ_ONLY        1
#define OPEN_READ_WRITE  0
#define OPEN_READ_ONLY   1
#define OPEN_WRITE_ONLY  2

/* Generic status bits returned from status_dev() */
#define BMT_TAPE           (1<<0)     /* is tape device */
#define BMT_EOF            (1<<1)     /* just read EOF */
#define BMT_BOT            (1<<2)     /* at beginning of tape */
#define BMT_EOT            (1<<3)     /* end of tape reached */
#define BMT_SM             (1<<4)     /* DDS setmark */
#define BMT_EOD            (1<<5)     /* DDS at end of data */
#define BMT_WR_PROT        (1<<6)     /* tape write protected */
#define BMT_ONLINE         (1<<7)     /* tape online */
#define BMT_DR_OPEN        (1<<8)     /* tape door open */
#define BMT_IM_REP_EN      (1<<9)     /* immediate report enabled */


/* Test capabilities */
#define dev_cap(dev, cap) ((dev)->capabilities & (cap))

/* Bits for device capabilities */
#define CAP_EOF            (1<<0)     /* has MTWEOF */
#define CAP_BSR            (1<<1)     /* has MTBSR */
#define CAP_BSF            (1<<2)     /* has MTBSF */
#define CAP_FSR            (1<<3)     /* has MTFSR */
#define CAP_FSF            (1<<4)     /* has MTFSF */
#define CAP_EOM            (1<<5)     /* has MTEOM */
#define CAP_REM            (1<<6)     /* is removable media */
#define CAP_RACCESS        (1<<7)     /* is random access device */
#define CAP_AUTOMOUNT      (1<<8)     /* Read device at start to see what is there */
#define CAP_LABEL          (1<<9)     /* Label blank tapes */
#define CAP_ANONVOLS       (1<<10)    /* Mount without knowing volume name */
#define CAP_ALWAYSOPEN     (1<<11)    /* always keep device open */
#define CAP_AUTOCHANGER    (1<<12)    /* AutoChanger */
#define CAP_OFFLINEUNMOUNT (1<<13)    /* Offline before unmount */
#define CAP_STREAM         (1<<14)    /* Stream device */
#define CAP_BSFATEOM       (1<<15)    /* Backspace file at EOM */
#define CAP_FASTFSF        (1<<16)    /* Fast forward space file */
#define CAP_TWOEOF         (1<<17)    /* Write two eofs for EOM */

/* Test state */
#define dev_state(dev, st_state) ((dev)->state & (st_state))

/* Device state bits */
#define ST_OPENED          (1<<0)     /* set when device opened */
#define ST_TAPE            (1<<1)     /* is a tape device */  
#define ST_FILE            (1<<2)     /* is a file device */
#define ST_FIFO            (1<<3)     /* is a fifo device */
#define ST_PROG            (1<<4)     /* is a program device */
#define ST_LABEL           (1<<5)     /* label found */
#define ST_MALLOC          (1<<6)     /* dev packet malloc'ed in init_dev() */
#define ST_APPEND          (1<<7)     /* ready for Bacula append */
#define ST_READ            (1<<8)     /* ready for Bacula read */
#define ST_EOT             (1<<9)     /* at end of tape */
#define ST_WEOT            (1<<10)    /* Got EOT on write */
#define ST_EOF             (1<<11)    /* Read EOF i.e. zero bytes */
#define ST_NEXTVOL         (1<<12)    /* Start writing on next volume */
#define ST_SHORT           (1<<13)    /* Short block read */

/* dev_blocked states (mutually exclusive) */
#define BST_NOT_BLOCKED       0       /* not blocked */
#define BST_UNMOUNTED         1       /* User unmounted device */
#define BST_WAITING_FOR_SYSOP 2       /* Waiting for operator to mount tape */
#define BST_DOING_ACQUIRE     3       /* Opening/validating/moving tape */
#define BST_WRITING_LABEL     4       /* Labeling a tape */  
#define BST_UNMOUNTED_WAITING_FOR_SYSOP 5 /* Closed by user during mount request */
#define BST_MOUNT             6       /* Mount request */

/* Volume Catalog Information structure definition */
struct VOLUME_CAT_INFO {
   /* Media info for the current Volume */
   uint32_t VolCatJobs;               /* number of jobs on this Volume */
   uint32_t VolCatFiles;              /* Number of files */
   uint32_t VolCatBlocks;             /* Number of blocks */
   uint64_t VolCatBytes;              /* Number of bytes written */
   uint32_t VolCatMounts;             /* Number of mounts this volume */
   uint32_t VolCatErrors;             /* Number of errors this volume */
   uint32_t VolCatWrites;             /* Number of writes this volume */
   uint32_t VolCatReads;              /* Number of reads this volume */
   uint64_t VolCatRBytes;             /* Number of bytes read */
   uint32_t VolCatRecycles;           /* Number of recycles this volume */
   int32_t  Slot;                     /* Slot in changer */
   bool     InChanger;                /* Set if vol in current magazine */
   uint32_t VolCatMaxJobs;            /* Maximum Jobs to write to volume */
   uint32_t VolCatMaxFiles;           /* Maximum files to write to volume */
   uint64_t VolCatMaxBytes;           /* Max bytes to write to volume */
   uint64_t VolCatCapacityBytes;      /* capacity estimate */
   uint64_t VolReadTime;              /* time spent reading */
   uint64_t VolWriteTime;             /* time spent writing this Volume */
   char VolCatStatus[20];             /* Volume status */
   char VolCatName[MAX_NAME_LENGTH];  /* Desired volume to mount */
};                


typedef struct s_steal_lock {
   pthread_t         no_wait_id;      /* id of no wait thread */
   int               dev_blocked;     /* state */
} bsteal_lock_t;

struct DEVRES;                        /* Device resource defined in stored_conf.h */

/* Device structure definition */
struct DEVICE {
public:
   DEVICE *next;                      /* pointer to next open device */
   DEVICE *prev;                      /* pointer to prev open device */
   JCR *attached_jcrs;                /* attached JCR list */
   pthread_mutex_t mutex;             /* access control */
   pthread_cond_t wait;               /* thread wait variable */
   pthread_cond_t wait_next_vol;      /* wait for tape to be mounted */
   pthread_t no_wait_id;              /* this thread must not wait */
   int dev_blocked;                   /* set if we must wait (i.e. change tape) */
   int num_waiting;                   /* number of threads waiting */
   int num_writers;                   /* number of writing threads */
   int use_count;                     /* usage count on this device */
   int fd;                            /* file descriptor */
   int capabilities;                  /* capabilities mask */
   int state;                         /* state mask */
   int dev_errno;                     /* Our own errno */
   int mode;                          /* read/write modes */
   char *dev_name;                    /* device name */
   char *errmsg;                      /* nicely edited error message */
   uint32_t block_num;                /* current block number base 0 */
   uint32_t file;                     /* current file number base 0 */
   uint64_t file_addr;                /* Current file read/write address */
   uint32_t EndBlock;                 /* last block written */
   uint32_t EndFile;                  /* last file written */
   uint32_t min_block_size;           /* min block size */
   uint32_t max_block_size;           /* max block size */
   uint64_t max_volume_size;          /* max bytes to put on one volume */
   uint64_t max_file_size;            /* max file size to put in one file on volume */
   uint64_t volume_capacity;          /* advisory capacity */
   uint32_t max_rewind_wait;          /* max secs to allow for rewind */
   uint32_t max_open_wait;            /* max secs to allow for open */
   uint32_t max_open_vols;            /* max simultaneous open volumes */
   DEVRES *device;                    /* pointer to Device Resource */
   btimer_id tid;                     /* timer id */

   VOLUME_CAT_INFO VolCatInfo;        /* Volume Catalog Information */
   VOLUME_LABEL VolHdr;               /* Actual volume label */
   
};


/* Get some definition of function to position
 *  to the end of the medium in MTEOM. System
 *  dependent. Arrgggg!
 */
#ifndef MTEOM
#ifdef  MTSEOD
#define MTEOM MTSEOD
#endif
#ifdef MTEOD
#undef MTEOM
#define MTEOM MTEOD
#endif
#endif

#endif
