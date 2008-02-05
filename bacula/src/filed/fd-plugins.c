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
#include "fd-plugins.h"

const int dbglvl = 50;
const char *plugin_type = "-fd.so";

/* Function pointers to be set here */
extern int     (*plugin_bopen)(JCR *jcr, const char *fname, int flags, mode_t mode);
extern int     (*plugin_bclose)(JCR *jcr);
extern ssize_t (*plugin_bread)(JCR *jcr, void *buf, size_t count);
extern ssize_t (*plugin_bwrite)(JCR *jcr, void *buf, size_t count);


/* Forward referenced functions */
static bRC baculaGetValue(bpContext *ctx, bVariable var, void *value);
static bRC baculaSetValue(bpContext *ctx, bVariable var, void *value);
static bRC baculaRegisterEvents(bpContext *ctx, ...);
static bRC baculaJobMsg(bpContext *ctx, const char *file, int line,
  int type, time_t mtime, const char *msg);
static bRC baculaDebugMsg(bpContext *ctx, const char *file, int line,
  int level, const char *msg);

static int     my_plugin_bopen(JCR *jcr, const char *fname, int flags, mode_t mode);
static int     my_plugin_bclose(JCR *jcr);
static ssize_t my_plugin_bread(JCR *jcr, void *buf, size_t count);
static ssize_t my_plugin_bwrite(JCR *jcr, void *buf, size_t count);


/* Bacula info */
static bInfo binfo = {
   sizeof(bFuncs),
   PLUGIN_INTERFACE_VERSION 
};

/* Bacula entry points */
static bFuncs bfuncs = {
   sizeof(bFuncs),
   PLUGIN_INTERFACE_VERSION,
   baculaRegisterEvents,
   baculaGetValue,
   baculaSetValue,
   baculaJobMsg,
   baculaDebugMsg
};


/*
 * Create a plugin event 
 */
void generate_plugin_event(JCR *jcr, bEventType eventType, void *value)     
{
   bEvent event;
   Plugin *plugin;
   int i = 0;
   int len;
   char *p;
   char *cmd = (char *)value;

   if (!plugin_list) {
      return;
   }

   bpContext *plugin_ctx = (bpContext *)jcr->plugin_ctx;
   event.eventType = eventType;

   Dmsg2(dbglvl, "plugin_ctx=%p JobId=%d\n", jcr->plugin_ctx, jcr->JobId);
   if (eventType != bEventPluginCommand) {
      /* Pass event to every plugin */
      foreach_alist(plugin, plugin_list) {
         plug_func(plugin)->handlePluginEvent(&plugin_ctx[i++], &event, value);
      }
      return;
   }

   /* Handle plugin command here (backup/restore of file) */
   if (!(p = strchr(cmd, ':'))) {
      Jmsg1(jcr, M_ERROR, 0, "Malformed plugin command: %s\n", cmd);
      return;
   }
   len = p - cmd;
   if (len > 0) {
      foreach_alist(plugin, plugin_list) {
         Dmsg3(000, "plugin=%s cmd=%s len=%d\n", plugin->file, cmd, len);
         if (strncmp(plugin->file, cmd, len) == 0) {
            Dmsg1(000, "Command plugin = %s\n", cmd);
            plug_func(plugin)->handlePluginEvent(&plugin_ctx[i], &event, value);
            return;
         }
         i++;
      }
   }
      
}

void load_fd_plugins(const char *plugin_dir)
{
   if (!plugin_dir) {
      return;
   }

   plugin_list = New(alist(10, not_owned_by_alist));
   load_plugins((void *)&binfo, (void *)&bfuncs, plugin_dir, plugin_type);
   plugin_bopen     = my_plugin_bopen;
   plugin_bclose    = my_plugin_bclose;
   plugin_bread     = my_plugin_bread;
   plugin_bwrite    = my_plugin_bwrite;

}

/*
 * Create a new instance of each plugin for this Job
 */
void new_plugins(JCR *jcr)
{
   Plugin *plugin;
   int i = 0;

   if (!plugin_list) {
      return;
   }

   int num = plugin_list->size();

   if (num == 0) {
      return;
   }

   jcr->plugin_ctx = (void *)malloc(sizeof(bpContext) * num);

   bpContext *plugin_ctx = (bpContext *)jcr->plugin_ctx;
   Dmsg2(dbglvl, "Instantiate plugin_ctx=%p JobId=%d\n", jcr->plugin_ctx, jcr->JobId);
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

   if (!plugin_list) {
      return;
   }

   bpContext *plugin_ctx = (bpContext *)jcr->plugin_ctx;
   Dmsg2(dbglvl, "Free instance plugin_ctx=%p JobId=%d\n", jcr->plugin_ctx, jcr->JobId);
   foreach_alist(plugin, plugin_list) {
      /* Free the plugin instance */
      plug_func(plugin)->freePlugin(&plugin_ctx[i++]);
   }
   free(plugin_ctx);
   jcr->plugin_ctx = NULL;
}

static int my_plugin_bopen(JCR *jcr, const char *fname, int flags, mode_t mode)
{
   struct io_pkt io;
   Dmsg0(000, "plugin_bopen\n");
   io.func = IO_OPEN;
   io.count = 0;
   io.buf = NULL;

   return 0;
}

static int my_plugin_bclose(JCR *jcr)
{
   struct io_pkt io;
   Dmsg0(000, "plugin_bclose\n");
   io.func = IO_CLOSE;
   io.count = 0;
   io.buf = NULL;
   return 0;
}

static ssize_t my_plugin_bread(JCR *jcr, void *buf, size_t count)
{
   struct io_pkt io;
   Dmsg0(000, "plugin_bread\n");
   io.func = IO_READ;
   io.count = count;
   io.buf = (char *)buf;
   return 0;
}

static ssize_t my_plugin_bwrite(JCR *jcr, void *buf, size_t count)
{
   struct io_pkt io;
   Dmsg0(000, "plugin_bwrite\n");
   io.func = IO_WRITE;
   io.count = count;
   io.buf = (char *)buf;
   return 0;
}

/* ==============================================================
 *
 * Callbacks from the plugin
 *
 * ==============================================================
 */
static bRC baculaGetValue(bpContext *ctx, bVariable var, void *value)
{
   JCR *jcr = (JCR *)(ctx->bContext);
// Dmsg1(dbglvl, "bacula: baculaGetValue var=%d\n", var);
   if (!value) {
      return bRC_Error;
   }
// Dmsg1(dbglvl, "Bacula: jcr=%p\n", jcr); 
   switch (var) {
   case bVarJobId:
      *((int *)value) = jcr->JobId;
      Dmsg1(dbglvl, "Bacula: return bVarJobId=%d\n", jcr->JobId);
      break;
   case bVarFDName:
      *((char **)value) = my_name;
      Dmsg1(dbglvl, "Bacula: return my_name=%s\n", my_name);
      break;
   case bVarLevel:
   case bVarType:
   case bVarClient:
   case bVarJobName:
   case bVarJobStatus:
   case bVarSinceTime:
      break;
   }
   return bRC_OK;
}

static bRC baculaSetValue(bpContext *ctx, bVariable var, void *value)
{
   Dmsg1(dbglvl, "bacula: baculaSetValue var=%d\n", var);
   return bRC_OK;
}

static bRC baculaRegisterEvents(bpContext *ctx, ...)
{
   va_list args;
   uint32_t event;

   va_start(args, ctx);
   while ((event = va_arg(args, uint32_t))) {
      Dmsg1(dbglvl, "Plugin wants event=%u\n", event);
   }
   va_end(args);
   return bRC_OK;
}

static bRC baculaJobMsg(bpContext *ctx, const char *file, int line,
  int type, time_t mtime, const char *msg)
{
   Dmsg5(dbglvl, "Job message: %s:%d type=%d time=%ld msg=%s\n",
      file, line, type, mtime, msg);
   return bRC_OK;
}

static bRC baculaDebugMsg(bpContext *ctx, const char *file, int line,
  int level, const char *msg)
{
   Dmsg4(dbglvl, "Debug message: %s:%d level=%d msg=%s\n",
      file, line, level, msg);
   return bRC_OK;
}

#ifdef TEST_PROGRAM

int     (*plugin_bopen)(JCR *jcr, const char *fname, int flags, mode_t mode) = NULL;
int     (*plugin_bclose)(JCR *jcr) = NULL;
ssize_t (*plugin_bread)(JCR *jcr, void *buf, size_t count) = NULL;
ssize_t (*plugin_bwrite)(JCR *jcr, void *buf, size_t count) = NULL;


int main(int argc, char *argv[])
{
   char plugin_dir[1000];
   JCR mjcr1, mjcr2;
   JCR *jcr1 = &mjcr1;
   JCR *jcr2 = &mjcr2;

   strcpy(my_name, "test-fd");
    
   getcwd(plugin_dir, sizeof(plugin_dir)-1);
   load_fd_plugins(plugin_dir);

   jcr1->JobId = 111;
   new_plugins(jcr1);

   jcr2->JobId = 222;
   new_plugins(jcr2);

   generate_plugin_event(jcr1, bEventJobStart);
   generate_plugin_event(jcr1, bEventJobEnd);
   generate_plugin_event(jcr2, bEventJobStart);
   free_plugins(jcr1);
   generate_plugin_event(jcr2, bEventJobEnd);
   free_plugins(jcr2);

   unload_plugins();

   Dmsg0(dbglvl, "bacula: OK ...\n");
   close_memory_pool();
   sm_dump(false);
   return 0;
}

#endif /* TEST_PROGRAM */
