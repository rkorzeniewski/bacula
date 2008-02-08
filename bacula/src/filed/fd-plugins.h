/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2007-2008 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from
   many others, a complete list can be found in the file AUTHORS.
   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version two of the GNU General Public
   License as published by the Free Software Foundation, which is 
   listed in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

   Bacula® is a registered trademark of John Walker.
   The licensor of Bacula is the Free Software Foundation Europe
   (FSFE), Fiduciary Program, Sumatrastrasse 25, 8006 Zürich,
   Switzerland, email:ftf@fsfeurope.org.
*/
/*
 * Interface definition for Bacula Plugins
 *
 * Kern Sibbald, October 2007
 *
 */
 
#ifndef __FD_PLUGINS_H 
#define __FD_PLUGINS_H

#ifndef _BACULA_H
#ifdef __cplusplus
/* Workaround for SGI IRIX 6.5 */
#define _LANGUAGE_C_PLUS_PLUS 1
#endif
#define _REENTRANT    1
#define _THREAD_SAFE  1
#define _POSIX_PTHREAD_SEMANTICS 1
#define _FILE_OFFSET_BITS 64
#define _LARGEFILE_SOURCE 1
#define _LARGE_FILES 1
#endif

#include <sys/types.h>
#ifndef __CONFIG_H
#define __CONFIG_H
#include "config.h"
#endif
#include "bc_types.h"
#include "lib/plugins.h"
#include <sys/stat.h>

/*
 * This packet is used for file save/restore info transfer */
struct save_pkt {
  char *fname;                        /* Full path and filename */
  char *link;                         /* Link name if any */
  struct stat statp;                  /* System stat() packet for file */
  int32_t type;                       /* FT_xx for this file */             
  uint32_t flags;                     /* Bacula internal flags */
  bool portable;                      /* set if data format is portable */
  char *cmd;                          /* command */
};

#define IO_OPEN  1
#define IO_READ  2
#define IO_WRITE 3
#define IO_CLOSE 4
#define IO_SEEK  5

struct io_pkt {
   int32_t func;                      /* Function code */
   int32_t count;                     /* read/write count */
   char *buf;                         /* read/write buffer */
   int32_t status;                    /* return status */
   int32_t io_errno;                  /* errno code */  
   int32_t whence;
   boffset_t offset;
};

/****************************************************************************
 *                                                                          *
 *                Bacula definitions                                        *
 *                                                                          *
 ****************************************************************************/

/* Bacula Variable Ids */
typedef enum {
  bVarJobId     = 1,
  bVarFDName    = 2,
  bVarLevel     = 3,
  bVarType      = 4,
  bVarClient    = 5,
  bVarJobName   = 6,
  bVarJobStatus = 7,
  bVarSinceTime = 8
} bVariable;

typedef enum {
  bEventJobStart      = 1,
  bEventJobEnd        = 2,
  bEventBackupStart   = 3,
  bEventBackupEnd     = 4,
  bEventRestoreStart  = 5,
  bEventRestoreEnd    = 6,
  bEventVerifyStart   = 7,
  bEventVerifyEnd     = 8,
  bEventPluginCommand = 9,
  bEventPluginFile    = 10,
  bEventLevel         = 11,
  bEventSince         = 12,
} bEventType;

typedef struct s_bEvent {
   uint32_t eventType;
} bEvent;

typedef struct s_baculaInfo {
   uint32_t size;
   uint32_t version;
} bInfo;

/* Bacula Core Routines -- not used by plugins */
void load_fd_plugins(const char *plugin_dir);
void new_plugins(JCR *jcr);
void free_plugins(JCR *jcr);
void generate_plugin_event(JCR *jcr, bEventType event, void *value=NULL);
bool send_plugin_name(JCR *jcr, BSOCK *sd, bool start);

#ifdef __cplusplus
extern "C" {
#endif

/* Bacula interface version and function pointers */
typedef struct s_baculaFuncs {  
   uint32_t size;
   uint32_t version;
   bRC (*registerBaculaEvents)(bpContext *ctx, ...);
   bRC (*getBaculaValue)(bpContext *ctx, bVariable var, void *value);
   bRC (*setBaculaValue)(bpContext *ctx, bVariable var, void *value);
   bRC (*JobMessage)(bpContext *ctx, const char *file, int line, 
       int type, time_t mtime, const char *msg);     
   bRC (*DebugMessage)(bpContext *ctx, const char *file, int line,
       int level, const char *msg);
} bFuncs;




/****************************************************************************
 *                                                                          *
 *                Plugin definitions                                        *
 *                                                                          *
 ****************************************************************************/

typedef enum {
  pVarName = 1,
  pVarDescription = 2
} pVariable;


#define PLUGIN_MAGIC     "*PluginData*" 
#define PLUGIN_INTERFACE_VERSION  1

typedef struct s_pluginInfo {
   uint32_t size;
   uint32_t version;
   char *plugin_magic;
   char *plugin_license;
   char *plugin_author;
   char *plugin_date;
   char *plugin_version;
   char *plugin_description;
} pInfo;

typedef struct s_pluginFuncs {  
   uint32_t size;
   uint32_t version;
   bRC (*newPlugin)(bpContext *ctx);
   bRC (*freePlugin)(bpContext *ctx);
   bRC (*getPluginValue)(bpContext *ctx, pVariable var, void *value);
   bRC (*setPluginValue)(bpContext *ctx, pVariable var, void *value);
   bRC (*handlePluginEvent)(bpContext *ctx, bEvent *event, void *value);
   bRC (*startPluginBackup)(bpContext *ctx, struct save_pkt *sp);
   bRC (*pluginIO)(bpContext *ctx, struct io_pkt *io);
} pFuncs;

#define plug_func(plugin) ((pFuncs *)(plugin->pfuncs))
#define plug_info(plugin) ((pInfo *)(plugin->pinfo))

#ifdef __cplusplus
}
#endif

#endif /* __FD_PLUGINS_H */
