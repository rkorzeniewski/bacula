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
 * Sample Plugin program
 *
 *  Kern Sibbald, October 2007
 *
 */
#include <stdio.h>
#include "fd-plugins.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PLUGIN_LICENSE      "GPL"
#define PLUGIN_AUTHOR       "Kern Sibbald"
#define PLUGIN_DATE         "January 2008"
#define PLUGIN_VERSION      "1"
#define PLUGIN_DESCRIPTION  "Test File Daemon Plugin"

/* Forward referenced functions */
static bpError newPlugin(bpContext *ctx);
static bpError freePlugin(bpContext *ctx);
static bpError getPluginValue(bpContext *ctx, pVariable var, void *value);
static bpError setPluginValue(bpContext *ctx, pVariable var, void *value);
static bpError handlePluginEvent(bpContext *ctx, bEvent *event);


/* Pointers to Bacula functions */
static bFuncs *bfuncs = NULL;
static bInfo  *binfo = NULL;

static pInfo pluginInfo = {
   sizeof(pluginInfo),
   PLUGIN_INTERFACE,
   PLUGIN_MAGIC,
   PLUGIN_LICENSE,
   PLUGIN_AUTHOR,
   PLUGIN_DATE,
   PLUGIN_VERSION,
   PLUGIN_DESCRIPTION,
};

static pFuncs pluginFuncs = {
   sizeof(pluginFuncs),
   PLUGIN_INTERFACE,

   /* Entry points into plugin */
   newPlugin,                         /* new plugin instance */
   freePlugin,                        /* free plugin instance */
   getPluginValue,
   setPluginValue,
   handlePluginEvent
};

bpError loadPlugin(bInfo *lbinfo, bFuncs *lbfuncs, pInfo **pinfo, pFuncs **pfuncs)
{
   bfuncs = lbfuncs;                  /* set Bacula funct pointers */
   binfo  = lbinfo;
   printf("plugin: Loaded: size=%d version=%d\n", bfuncs->size, bfuncs->interface);

   *pinfo  = &pluginInfo;             /* return pointer to our info */
   *pfuncs = &pluginFuncs;            /* return pointer to our functions */

   return 0;
}

bpError unloadPlugin() 
{
   printf("plugin: Unloaded\n");
   return 0;
}

static bpError newPlugin(bpContext *ctx)
{
   int JobId = 0;
   bfuncs->getBaculaValue(ctx, bVarJobId, (void *)&JobId);
   printf("plugin: newPlugin JobId=%d\n", JobId);
   bfuncs->registerBaculaEvents(ctx, 1, 2, 0);
   return 0;
}

static bpError freePlugin(bpContext *ctx)
{
   int JobId = 0;
   bfuncs->getBaculaValue(ctx, bVarJobId, (void *)&JobId);
   printf("plugin: freePlugin JobId=%d\n", JobId);
   return 0;
}

static bpError getPluginValue(bpContext *ctx, pVariable var, void *value) 
{
   printf("plugin: getPluginValue var=%d\n", var);
   return 0;
}

static bpError setPluginValue(bpContext *ctx, pVariable var, void *value) 
{
   printf("plugin: setPluginValue var=%d\n", var);
   return 0;
}

static bpError handlePluginEvent(bpContext *ctx, bEvent *event) 
{
   char *name;
   switch (event->eventType) {
   case bEventJobStart:
      printf("plugin: HandleEvent JobStart\n");
      break;
   case bEventJobEnd:
      printf("plugin: HandleEvent JobEnd\n");
      break;
   }
   bfuncs->getBaculaValue(ctx, bVarFDName, (void *)&name);
   printf("FD Name=%s\n", name);
   bfuncs->JobMessage(ctx, __FILE__, __LINE__, 1, 0, "JobMesssage message");
   bfuncs->DebugMessage(ctx, __FILE__, __LINE__, 1, "DebugMesssage message");
   return 0;
}

#ifdef __cplusplus
}
#endif
