/*
 * Record, and label definitions for Bacula
 *  media data format.
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


#ifndef __RECORD_H
#define __RECORD_H 1

/* Return codes from read_device_volume_label() */
#define VOL_NOT_READ      0               /* Volume label not read */
#define VOL_OK            1               /* volume name OK */
#define VOL_NO_LABEL      2               /* volume not labeled */
#define VOL_IO_ERROR      3               /* volume I/O error */
#define VOL_NAME_ERROR    4               /* Volume name mismatch */
#define VOL_CREATE_ERROR  5               /* Error creating label */
#define VOL_VERSION_ERROR 6               /* Bacula version error */
#define VOL_LABEL_ERROR   7               /* Bad label type */


/* Length of Record Header (5 * 4 bytes) */
#define RECHDR_LENGTH 20

/*
 * This is the Media structure for a record header.
 *  NB: when it is written it is serialized.
 */
typedef struct s_record_hdr {
   uint32_t VolSessionId;
   uint32_t VolSessionTime;
   int32_t  FileIndex;
   int32_t  Stream;
   uint32_t data_len;
} RECORD_HDR;

/* Record state bit definitions */
#define REC_NO_HEADER        0x01     /* No header read */
#define REC_PARTIAL_RECORD   0x02     /* returning partial record */
#define REC_BLOCK_EMPTY      0x04     /* not enough data in block */
#define REC_NO_MATCH         0x08     /* No match on continuation data */
#define REC_CONTINUATION     0x10     /* Continuation record found */

#define is_partial_record(r) ((r)->state & REC_PARTIAL_RECORD)
#define is_block_empty(r)    ((r)->state & REC_BLOCK_EMPTY)

/*
 * DEV_RECORD for reading and writing records.
 * It consists of a Record Header, and the Record Data
 * 
 *  This is the memory structure for the record header.
 */
typedef struct s_dev_rec {
   int      sync;                     /* synchronous */
   /* File and Block are always returned on reading records, but
    *  only returned on writing if sync is set (obviously).
    */
   uint32_t File;                     /* File number */
   uint32_t Block;                    /* Block number */
   uint32_t VolSessionId;             /* sequential id within this session */
   uint32_t VolSessionTime;           /* session start time */
   int32_t  FileIndex;                /* sequential file number */
   int32_t  Stream;                   /* stream number */
   uint32_t data_len;                 /* current record length */
   uint32_t remainder;                /* remaining bytes to read/write */
   uint32_t state;                    /* state bits */
   uint8_t  ser_buf[RECHDR_LENGTH];   /* serialized record header goes here */
   POOLMEM *data;                     /* Record data. This MUST be a memory pool item */
} DEV_RECORD;


/*
 * Values for LabelType that are put into the FileIndex field
 * Note, these values are negative to distinguish them
 * from user records where the FileIndex is forced positive.
 */
#define PRE_LABEL   -1                /* Vol label on unwritten tape */
#define VOL_LABEL   -2                /* Volume label first file */
#define EOM_LABEL   -3                /* Writen at end of tape */        
#define SOS_LABEL   -4                /* Start of Session */
#define EOS_LABEL   -5                /* End of Session */
 

/* 
 *   Volume Label Record
 */
struct Volume_Label {
  /*  
   * The first items in this structure are saved
   * in the DEVICE buffer, but are not actually written
   * to the tape.
   */
  int32_t LabelType;                  /* This is written in header only */
  uint32_t LabelSize;                 /* length of serialized label */
  /*
   * The items below this line are stored on 
   * the tape
   */
  char Id[32];                        /* Bacula Immortal ... */

  uint32_t VerNum;                    /* Label version number */

  float64_t label_date;               /* Date tape labeled */
  float64_t label_time;               /* Time tape labeled */
  float64_t write_date;               /* Date this label written */
  float64_t write_time;               /* Time this label written */

  char VolName[MAX_NAME_LENGTH];      /* Volume name */
  char PrevVolName[MAX_NAME_LENGTH];  /* Previous Volume Name */
  char PoolName[MAX_NAME_LENGTH];     /* Pool name */
  char PoolType[MAX_NAME_LENGTH];     /* Pool type */
  char MediaType[MAX_NAME_LENGTH];    /* Type of this media */

  char HostName[MAX_NAME_LENGTH];     /* Host name of writing computer */
  char LabelProg[32];                 /* Label program name */
  char ProgVersion[32];               /* Program version */
  char ProgDate[32];                  /* Program build date/time */
};

#define SER_LENGTH_Volume_Label 1024   /* max serialised length of volume label */
#define SER_LENGTH_Session_Label 1024  /* max serialised length of session label */

typedef struct Volume_Label VOLUME_LABEL;

/*
 * Session Start/End Label
 *  This record is at the beginning and end of each session
 */
struct Session_Label {
  char Id[32];                        /* Bacula Immortal ... */

  uint32_t VerNum;                    /* Label version number */

  uint32_t JobId;                     /* Job id */
  uint32_t VolumeIndex;               /* Sequence no of volume for this job */

  float64_t write_date;               /* Date this label written */
  float64_t write_time;               /* Time this label written */

  char PoolName[MAX_NAME_LENGTH];     /* Pool name */
  char PoolType[MAX_NAME_LENGTH];     /* Pool type */
  char JobName[MAX_NAME_LENGTH];      /* base Job name */
  char ClientName[MAX_NAME_LENGTH];
  char Job[MAX_NAME_LENGTH];          /* Unique name of this Job */
  char FileSetName[MAX_NAME_LENGTH];
  uint32_t JobType;
  uint32_t JobLevel;
  /* The remainder are part of EOS label only */
  uint32_t JobFiles;
  uint64_t JobBytes;
  uint32_t start_block;
  uint32_t end_block;
  uint32_t start_file;
  uint32_t end_file;
  uint32_t JobErrors;

};
typedef struct Session_Label SESSION_LABEL;

#define SERIAL_BUFSIZE  1024          /* volume serialisation buffer size */

#endif
