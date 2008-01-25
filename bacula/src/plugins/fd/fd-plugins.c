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
#include "jcr.h"
#include "lib/plugin.h"
#include "fd-plugins.h"

const int dbglvl = 0;
const char *plugin_type = "-fd.so";


/* Forward referenced functions */
static bpError baculaGetValue(bpContext *ctx, bVariable var, void *value);
static bpError baculaSetValue(bpContext *ctx, bVariable var, void *value);
static bpError baculaRegisterEvents(bpContext *ctx, ...);
static bpError baculaJobMsg(bpContext *ctx, const char *file, int line,
  int type, time_t mtime, const char *msg);
static bpError baculaDebugMsg(bpContext *ctx, const char *file, int line,
  int level, const char *msg);

void load_fd_plugins(const char *plugin_dir);
void new_plugins(JCR *jcr);
void free_plugins(JCR *jcr);
void plugin_event(JCR *jcr, bEventType event);


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
    


void plugin_event(JCR *jcr, bEventType eventType) 
{
   bEvent event;
   Plugin *plugin;
   int i = 0;

   bpContext *plugin_ctx = (bpContext *)jcr->plugin_ctx;
   Dmsg2(dbglvl, "plugin_ctx=%p JobId=%d\n", jcr->plugin_ctx, jcr->JobId);
   event.eventType = eventType;
   foreach_alist(plugin, plugin_list) {
      plug_func(plugin)->handlePluginEvent(&plugin_ctx[i++], &event);
   }
}

void load_fd_plugins(const char *plugin_dir)
{
   if (!plugin_dir) {
      return;
   }

   plugin_list = New(alist(10, not_owned_by_alist));

   load_plugins((void *)&binfo, (void *)&bfuncs, plugin_dir, plugin_type);
}

/*
 * Create a new instance of each plugin for this Job
 */
void new_plugins(JCR *jcr)
{
   Plugin *plugin;
   int i = 0;

   int num = plugin_list->size();

   if (num == 0) {
      return;
   }

   jcr->plugin_ctx = (void *)malloc(sizeof(bpContext) * num);

   bpContext *plugin_ctx = (bpContext *)jcr->plugin_ctx;
   Dmsg2(dbglvl, "plugin_ctx=%p JobId=%d\n", jcr->plugin_ctx, jcr->JobId);
   foreach_alist(plugin, plugin_list) {
      /* Start a new instance of each plugin */
      plugin_ctx[i].bContext = (void *)jcr;
      plugin_ctx[i].pContext = NULL;
      plug_func(plugin)->newPlugin(&plugin_ctx[i++]);
   }
}

/*
 * Free the plugin instances for this Job
 */
void free_plugins(JCR *jcr)
{
   Plugin *plugin;
   int i = 0;

   bpContext *plugin_ctx = (bpContext *)jcr->plugin_ctx;
   foreach_alist(plugin, plugin_list) {
      /* Free the plugin instance */
      plug_func(plugin)->freePlugin(&plugin_ctx[i++]);
   }
   free(plugin_ctx);
   jcr->plugin_ctx = NULL;
}


static bpError baculaGetValue(bpContext *ctx, bVariable var, void *value)
{
   JCR *jcr = (JCR *)(ctx->bContext);
   Dmsg1(dbglvl, "bacula: baculaGetValue var=%d\n", var);
   if (!value) {
      return 1;
   }
   Dmsg1(dbglvl, "Bacula: jcr=%p\n", jcr); 
   switch (var) {
   case bVarJobId:
      *((int *)value) = jcr->JobId;
      Dmsg1(dbglvl, "Bacula: return bVarJobId=%d\n", jcr->JobId);
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
   Dmsg1(dbglvl, "bacula: baculaSetValue var=%d\n", var);
   return 0;
}

static bpError baculaRegisterEvents(bpContext *ctx, ...)
{
   va_list args;
   uint32_t event;

   va_start(args, ctx);
   while ((event = va_arg(args, uint32_t))) {
      Dmsg1(dbglvl, "Plugin wants event=%u\n", event);
   }
   va_end(args);
   return 0;
}

static bpError baculaJobMsg(bpContext *ctx, const char *file, int line,
  int type, time_t mtime, const char *msg)
{
   Dmsg5(dbglvl, "Job message: %s:%d type=%d time=%ld msg=%s\n",
      file, line, type, mtime, msg);
   return 0;
}

static bpError baculaDebugMsg(bpContext *ctx, const char *file, int line,
  int level, const char *msg)
{
   Dmsg4(dbglvl, "Debug message: %s:%d level=%d msg=%s\n",
      file, line, level, msg);
   return 0;
}

#ifdef TEST_PROGRAM

int main(int argc, char *argv[])
{
   char plugin_dir[1000];
   JCR mjcr1, mjcr2;
   JCR *jcr1 = &mjcr1;
   JCR *jcr2 = &mjcr2;
    
   getcwd(plugin_dir, sizeof(plugin_dir)-1);
   load_fd_plugins(plugin_dir);

   jcr1->JobId = 111;
   new_plugins(jcr1);

   jcr2->JobId = 222;
   new_plugins(jcr2);

   plugin_event(jcr1, bEventJobStart);
   plugin_event(jcr1, bEventJobEnd);
   plugin_event(jcr2, bEventJobStart);
   free_plugins(jcr1);
   plugin_event(jcr2, bEventJobEnd);
   free_plugins(jcr2);

   unload_plugins();

   Dmsg0(dbglvl, "bacula: OK ...\n");
   close_memory_pool();
   sm_dump(false);
   return 0;
}

#endif /* TEST_PROGRAM */
