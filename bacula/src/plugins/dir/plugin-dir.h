/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2007-2007 Free Software Foundation Europe e.V.

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
 
#ifndef __PLUGIN_H 
#define __PLUGIN_H

#include <sys/types.h>
#ifndef __CONFIG_H
#define __CONFIG_H
#include "config.h"
#endif
#include "bc_types.h"

#ifdef __cplusplus
extern "C" {
#endif


/****************************************************************************
 *                                                                          *
 *                Common definitions                                        *
 *                                                                          *
 ****************************************************************************/

/* Universal return code from all functions */
typedef int32_t bpError;

/* Context packet as first argument of all functions */
typedef struct s_bpContext {
  void *bContext;                        /* Bacula private context */
  void *pContext;                        /* Plugin private context */
} bpContext;


/****************************************************************************
 *                                                                          *
 *                Bacula definitions                                        *
 *                                                                          *
 ****************************************************************************/

/* Bacula Variable Ids */
typedef enum {
  bVarJobId = 1
} bVariable;

typedef enum {
  bEventJobStart      = 1,
  bEventJobInit       = 2,
  bEventJobRun        = 3,
  bEventJobEnd        = 4,
  bEventNewVolume     = 5,
  bEventVolumePurged  = 6,
  bEventReload        = 7
} bEventType;

typedef struct s_bEvent {
   uint32_t eventType;
} bEvent;


/* Bacula interface version and function pointers */
typedef struct s_baculaFuncs {  
   uint32_t size;
   uint32_t version; 
   bpError (*bGetValue)(bpContext *ctx, bVariable var, void *value);
   bpError (*bSetValue)(bpContext *ctx, bVariable var, void *value);
   bpError (*bMemAlloc)(bpContext *ctx, uint32_t size, char *addr);
   bpError (*bMemFree)(bpContext *ctx, char *addr);
   bpError (*bMemFlush)(bpContext *ctx);
   bpError (*bVersion)(bVariable var, void *value);
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


typedef struct s_pluginFuncs {  
   uint32_t size;
   uint32_t version; 
   bpError (*pNew)(bpContext *ctx);
   bpError (*pDestroy)(bpContext *ctx);
   bpError (*pGetValue)(bpContext *ctx, pVariable var, void *value);
   bpError (*pSetValue)(bpContext *ctx, pVariable var, void *value);
   bpError (*pHandleEvent)(bpContext *ctx, bEvent *event);
} pFuncs;

typedef bpError (*t_bpInitialize)(bFuncs *bfuncs, pFuncs *pfuncs);
typedef bpError (*t_bpShutdown)(void);


#ifdef __cplusplus
}
#endif

#endif /* __PLUGIN_H */
