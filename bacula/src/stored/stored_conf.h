/*
 * Resource codes -- they must be sequential for indexing   
 *
 *   Version $Id$
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

#define R_FIRST                       3001

#define R_DIRECTOR                    3001
#define R_STORAGE                     3002
#define R_DEVICE                      3003
#define R_MSGS                        3004

#define R_LAST                        R_MSGS


#define R_NAME                        3020
#define R_ADDRESS                     3021
#define R_PASSWORD                    3022
#define R_TYPE                        3023
#define R_BACKUP                      3024

/* Definition of the contents of each Resource */
struct DIRRES {
   RES   hdr;

   char *password;                    /* Director password */
   char *address;                     /* Director IP address or zero */
   int enable_ssl;                    /* Use SSL with this Director */
};


/* Storage daemon "global" definitions */
struct s_res_store {
   RES   hdr;

   char *address;                     /* deprecated */
   char *SDaddr;                      /* bind address */
   int   SDport;                      /* Where we listen for Directors */
   int   SDDport;                     /* "Data" port where we listen for File daemons */
   char *working_directory;           /* working directory for checkpoints */
   char *pid_directory;
   char *subsys_directory;
   int require_ssl;                   /* Require SSL on all connections */
   uint32_t max_concurrent_jobs;      /* maximum concurrent jobs to run */
   MSGS *messages;                    /* Daemon message handler */
   utime_t heartbeat_interval;        /* Interval to send hb to FD */
};
typedef struct s_res_store STORES;

/* Device specific definitions */
struct DEVRES {
   RES   hdr;

   char *media_type;                  /* User assigned media type */
   char *device_name;                 /* Archive device name */
   char *changer_name;                /* Changer device name */
   char *changer_command;             /* Changer command  -- external program */
   uint32_t cap_bits;                 /* Capabilities of this device */
   uint32_t max_changer_wait;         /* Changer timeout */
   uint32_t max_rewind_wait;          /* maximum secs to wait for rewind */
   uint32_t max_open_wait;            /* maximum secs to wait for open */
   uint32_t max_open_vols;            /* maximum simultaneous open volumes */
   uint32_t min_block_size;           /* min block size */
   uint32_t max_block_size;           /* max block size */
   uint32_t max_volume_jobs;          /* max jobs to put on one volume */
   int64_t max_volume_files;          /* max files to put on one volume */
   int64_t max_volume_size;           /* max bytes to put on one volume */
   int64_t max_file_size;             /* max file size in bytes */
   int64_t volume_capacity;           /* advisory capacity */
   DEVICE *dev;                       /* Pointer to phyical dev -- set at runtime */
};

union URES {
   DIRRES res_dir;
   STORES res_store;
   DEVRES res_dev;
   MSGS   res_msgs;
   RES    hdr;
};
