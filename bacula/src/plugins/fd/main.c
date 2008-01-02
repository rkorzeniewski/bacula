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
 * Main program to test loading and running Bacula plugins.
 *   Destined to become Bacula pluginloader, ...
 *
 * Kern Sibbald, October 2007
 */
#include "bacula.h"
#include <dlfcn.h>
#include "lib/plugin.h"
#include "plugin-fd.h"

const char *plugin_type = "-fd.so";


/* Forward referenced functions */
static bpError baculaGetValue(bpContext *ctx, bVariable var, void *value);
static bpError baculaSetValue(bpContext *ctx, bVariable var, void *value);
static bpError baculaRegisterEvents(bpContext *ctx, ...);
static bpError baculaJobMsg(bpContext *ctx, const char *file, int line,
  int type, time_t mtime, const char *msg);
static bpError baculaDebugMsg(bpContext *ctx, const char *file, int line,
  int level, const char *msg);

/* Bacula info */
static bInfo binfo = {
   sizeof(bFuncs),
   PLUGIN_INTERFACE,
};

/* Bacula entry points */
static bFuncs bfuncs = {
   sizeof(bFuncs),
   PLUGIN_INTERFACE,
   baculaRegisterEvents,
   baculaGetValue,
   baculaSetValue,
   baculaJobMsg,
   baculaDebugMsg
};
    



int main(int argc, char *argv[])
{
   char plugin_dir[1000];
   bpContext ctx;
   bEvent event;
   Plugin *plugin;
    
   plugin_list = New(alist(10, not_owned_by_alist));

   ctx.bContext = NULL;
   ctx.pContext = NULL;
   getcwd(plugin_dir, sizeof(plugin_dir)-1);

   load_plugins((void *)&binfo, (void *)&bfuncs, plugin_dir, plugin_type);

   foreach_alist(plugin, plugin_list) {
      printf("bacula: plugin_size=%d plugin_version=%d\n", 
              plug_func(plugin)->size, plug_func(plugin)->interface);
      printf("License: %s\nAuthor: %s\nDate: %s\nVersion: %s\nDescription: %s\n",
         plug_info(plugin)->plugin_license, plug_info(plugin)->plugin_author, 
         plug_info(plugin)->plugin_date, plug_info(plugin)->plugin_version, 
         plug_info(plugin)->plugin_description);

      /* Start a new instance of the plugin */
      plug_func(plugin)->newPlugin(&ctx);
      event.eventType = bEventJobStart;
      plug_func(plugin)->handlePluginEvent(&ctx, &event);
      event.eventType = bEventJobEnd;
      plug_func(plugin)->handlePluginEvent(&ctx, &event);
      /* Free the plugin instance */
      plug_func(plugin)->freePlugin(&ctx);

      /* Start a new instance of the plugin */
      plug_func(plugin)->newPlugin(&ctx);
      event.eventType = bEventJobStart;
      plug_func(plugin)->handlePluginEvent(&ctx, &event);
      event.eventType = bEventJobEnd;
      plug_func(plugin)->handlePluginEvent(&ctx, &event);
      /* Free the plugin instance */
      plug_func(plugin)->freePlugin(&ctx);
   }

   unload_plugins();

   printf("bacula: OK ...\n");
   close_memory_pool();
   sm_dump(false);
   return 0;
}

static bpError baculaGetValue(bpContext *ctx, bVariable var, void *value)
{
   printf("bacula: baculaGetValue var=%d\n", var);
   if (!value) {
      return 1;
   }
   switch (var) {
   case bVarJobId:
      *((int *)value) = 100;
      break;
   case bVarFDName:
      *((char **)value) = "FD Name";
      break;
   case bVarLevel:
   case bVarType:
   case bVarClient:
   case bVarJobName:
   case bVarJobStatus:
   case bVarSinceTime:
      break;
   }
   return 0;
}

static bpError baculaSetValue(bpContext *ctx, bVariable var, void *value)
{
   printf("bacula: baculaSetValue var=%d\n", var);
   return 0;
}

static bpError baculaRegisterEvents(bpContext *ctx, ...)
{
   va_list args;
   uint32_t event;

   va_start(args, ctx);
   while ((event = va_arg(args, uint32_t))) {
      printf("Plugin wants event=%u\n", event);
   }
   va_end(args);
   return 0;
}

static bpError baculaJobMsg(bpContext *ctx, const char *file, int line,
  int type, time_t mtime, const char *msg)
{
   printf("Job message: %s:%d type=%d time=%ld msg=%s\n",
      file, line, type, mtime, msg);
   return 0;
}

static bpError baculaDebugMsg(bpContext *ctx, const char *file, int line,
  int level, const char *msg)
{
   printf("Debug message: %s:%d level=%d msg=%s\n",
      file, line, level, msg);
   return 0;
}
