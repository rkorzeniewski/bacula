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
 
#ifndef __PLUGIN_FD_H 
#define __PLUGIN_FD_H

#include <sys/types.h>
#ifndef __CONFIG_H
#define __CONFIG_H
#include "config.h"
#endif
#include "bc_types.h"
#include "lib/plugin.h"

#ifdef __cplusplus
extern "C" {
#endif


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
} bEventType;

typedef struct s_bEvent {
   uint32_t eventType;
} bEvent;

typedef struct s_baculaInfo {
   uint32_t size;
   uint32_t interface;
} bInfo;

/* Bacula interface version and function pointers */
typedef struct s_baculaFuncs {  
   uint32_t size;
   uint32_t interface;
   bpError (*registerBaculaEvents)(bpContext *ctx, ...);
   bpError (*getBaculaValue)(bpContext *ctx, bVariable var, void *value);
   bpError (*setBaculaValue)(bpContext *ctx, bVariable var, void *value);
   bpError (*JobMessage)(bpContext *ctx, const char *file, int line, 
       int type, time_t mtime, const char *msg);     
   bpError (*DebugMessage)(bpContext *ctx, const char *file, int line,
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
#define PLUGIN_INTERFACE  1

typedef struct s_pluginInfo {
   uint32_t size;
   uint32_t interface;
   char *plugin_magic;
   char *plugin_license;
   char *plugin_author;
   char *plugin_date;
   char *plugin_version;
   char *plugin_description;
} pInfo;

typedef struct s_pluginFuncs {  
   uint32_t size;
   uint32_t interface;
   bpError (*newPlugin)(bpContext *ctx);
   bpError (*freePlugin)(bpContext *ctx);
   bpError (*getPluginValue)(bpContext *ctx, pVariable var, void *value);
   bpError (*setPluginValue)(bpContext *ctx, pVariable var, void *value);
   bpError (*handlePluginEvent)(bpContext *ctx, bEvent *event);
} pFuncs;

#define plug_func(plugin) ((pFuncs *)(plugin->pfuncs))
#define plug_info(plugin) ((pInfo *)(plugin->pinfo))

#ifdef __cplusplus
}
#endif

#endif /* __PLUGIN_FD_H */
