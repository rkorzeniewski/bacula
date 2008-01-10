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
 * Bacula File daemon core code for loading and running plugins.
 *
 * Kern Sibbald, October 2007
 */
#include "bacula.h"
#include "lib/plugin.h"
#include "fd-plugins.h"

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
    

void init_fd_plugins(const char *plugin_dir)
{
   Plugin *plugin;

   if (!plugin_dir) {
      return;
   }
    
   plugin_list = New(alist(10, not_owned_by_alist));

   load_plugins((void *)&binfo, (void *)&bfuncs, plugin_dir, plugin_type);
}

/*
 * Create a new instance of each plugin for this Job
 */
void new_fd_plugins(JCR *jcr)
{
   bpContext *ctx;
   Plugin *plugin;
   int i = 0;

   int num = plugin_list->size();

   if (num == 0) {
      return;
   }

   jcr->plugin_ctx = (bpContext **)malloc(sizeof(bpContext) * num);

   ctx = jcr->plugin_ctx;
   foreach_alist(plugin, plugin_list) {
      /* Start a new instance of each plugin */
      ctx[i].bContext = (void *)jcr;
      ctx[i].pContext = NULL;
      plug_func(plugin)->newPlugin(ctx[i++]);
   }
}

/*
 * Free the plugin instances for this Job
 */
void free_fd_plugins(JCR *jcr)
{
   bpContext *ctx;
   Plugin *plugin;
   int i = 0;

   ctx = jcr->plugin_ctx;
   foreach_alist(plugin, jcr->plugin_list) {
      /* Free the plugin instance */
      plug_func(plugin)->freePlugin(ctx[i++]);
   }
   free(ctx);
   jcr->plugin_ctx = NULL;
}

void term_fd_plugins()
{
   unload_plugins();
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
