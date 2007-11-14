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
 * Sample Plugin program
 *
 *  Kern Sibbald, October 2007
 *
 */
#include <stdio.h>
#include "plugin-dir.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward referenced functions */
bpError pNew(bpContext *ctx);
bpError pDestroy(bpContext *ctx);
bpError pGetValue(bpContext *ctx, pVariable var, void *value);
bpError pSetValue(bpContext *ctx, pVariable var, void *value);
bpError pHandleEvent(bpContext *ctx, bEvent *event);


/* Pointers to Bacula functions */
bpError (*p_bGetValue)(bpContext *ctx, bVariable var, void *value);
bpError (*p_bSetValue)(bpContext *ctx, bVariable var, void *value);

pFuncs pluginFuncs = {
   sizeof(pluginFuncs),
   1,
   pNew,
   pDestroy,
   pGetValue,
   pSetValue,
   pHandleEvent
};

bpError bpInitialize(bFuncs *bfuncs, pFuncs *pfuncs) 
{
   printf("plugin: Initializing. size=%d version=%d\n", bfuncs->size, bfuncs->version);
   p_bGetValue = bfuncs->bGetValue;
   p_bSetValue = bfuncs->bSetValue;

   pfuncs->pNew = pNew;
   pfuncs->pDestroy = pDestroy;
   pfuncs->pGetValue = pGetValue;
   pfuncs->pSetValue = pSetValue;
   pfuncs->pHandleEvent = pHandleEvent;
   pfuncs->size = sizeof(pFuncs);
   pfuncs->version = 1;

   return 0;
}

bpError bpShutdown() 
{
   printf("plugin: Shutting down\n");
   return 0;
}

bpError pNew(bpContext *ctx)
{
   int JobId = 0;
   p_bGetValue(ctx, bVarJobId, (void *)&JobId);
   printf("plugin: New JobId=%d\n", JobId);
   return 0;
}

bpError pDestroy(bpContext *ctx)
{
   int JobId = 0;
   p_bGetValue(ctx, bVarJobId, (void *)&JobId);
   printf("plugin: Destroy JobId=%d\n", JobId);
   return 0;
}

bpError pGetValue(bpContext *ctx, pVariable var, void *value) 
{
   printf("plugin: GetValue var=%d\n", var);
   return 0;
}

bpError pSetValue(bpContext *ctx, pVariable var, void *value) 
{
   printf("plugin: PutValue var=%d\n", var);
   return 0;
}

bpError pHandleEvent(bpContext *ctx, bEvent *event) 
{
   printf("plugin: HandleEvent Event=%d\n", event->eventType);
   return 0;
}

#ifdef __cplusplus
}
#endif
