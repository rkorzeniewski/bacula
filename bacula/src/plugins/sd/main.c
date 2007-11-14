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
 * Main program to test loading and running Bacula plugins.
 *   Destined to become Bacula pluginloader, ...
 *
 * Kern Sibbald, October 2007
 */
#include "bacula.h"
#include <dlfcn.h>
#include "plugin-sd.h"
#ifdef HAVE_DIRENT_H
#include <dirent.h>
#define NAMELEN(dirent) (strlen((dirent)->d_name))
#endif
#ifndef HAVE_READDIR_R
int readdir_r(DIR *dirp, struct dirent *entry, struct dirent **result);
#endif

#ifndef RTLD_NOW
#define RTLD_NOW 2
#endif

class Plugin {
public:
   char *file;
   t_bpShutdown p_bpShutdown;
   pFuncs pfuncs;
   void *pHandle;
};

/* All loaded plugins */
alist *plugin_list;

/* Forward referenced functions */
static Plugin *new_plugin();
bool load_plugins(const char *plugin_dir);
void unload_plugins();
static bpError baculaGetValue(bpContext *ctx, bVariable var, void *value);
static bpError baculaSetValue(bpContext *ctx, bVariable var, void *value);

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
   load_plugins(plugin_dir);

   foreach_alist(plugin, plugin_list) {
      /* Start a new instance of the plugin */
      plugin->pfuncs.pNew(&ctx);
      event.eventType = bEventNewVolume;   
      plugin->pfuncs.pHandleEvent(&ctx, &event);
      /* Destroy the instance */
      plugin->pfuncs.pDestroy(&ctx);

      /* Start a new instance of the plugin */
      plugin->pfuncs.pNew(&ctx);
      event.eventType = bEventNewVolume;   
      plugin->pfuncs.pHandleEvent(&ctx, &event);
      /* Destroy the instance */
      plugin->pfuncs.pDestroy(&ctx);
   }

   unload_plugins();

   printf("bacula: OK ...\n");
   sm_dump(false);
   return 0;
}

/*
 * Create a new plugin "class" entry and enter it in the
 *  list of plugins.  Note, this is not the same as
 *  an instance of the plugin. 
 */
static Plugin *new_plugin()
{
   Plugin *plugin;

   plugin = (Plugin *)malloc(sizeof(Plugin));
   memset(plugin, 0, sizeof(Plugin));
   plugin_list->append(plugin);
   return plugin;
}


/*
 * Load all the plugins in the specified directory.
 */
bool load_plugins(const char *plugin_dir)
{
   t_bpInitialize p_bpInitialize;
   bFuncs bfuncs;
   Plugin *plugin;
   char *error;
   DIR* dp = NULL;
   struct dirent *entry, *result;
   int name_max;
   struct stat statp;
   bool found = false;
   POOL_MEM fname(PM_FNAME);
   bool need_slash = false;
   int len;

   /* Setup pointers to Bacula functions */
   bfuncs.size = sizeof(bFuncs);
   bfuncs.version = 1;
   bfuncs.bGetValue = baculaGetValue;
   bfuncs.bSetValue = baculaSetValue;

   plugin = new_plugin();

   name_max = pathconf(".", _PC_NAME_MAX);
   if (name_max < 1024) {
      name_max = 1024;
   }

   if (!(dp = opendir(plugin_dir))) {
      berrno be;
      Dmsg2(29, "load_plugins: failed to open dir %s: ERR=%s\n", 
            plugin_dir, be.bstrerror());
      goto get_out;
   }
   
   len = strlen(plugin_dir);
   if (len > 0) {
      need_slash = !IsPathSeparator(plugin_dir[len - 1]);
   }
   entry = (struct dirent *)malloc(sizeof(struct dirent) + name_max + 1000);
   for ( ;; ) {
      if ((readdir_r(dp, entry, &result) != 0) || (result == NULL)) {
         Dmsg1(129, "load_plugins: failed to find suitable file in dir %s\n", 
               plugin_dir);
         break;
      }
      if (strcmp(result->d_name, ".") == 0 || 
          strcmp(result->d_name, "..") == 0) {
         continue;
      }

      len = strlen(result->d_name);
      if (len < 7 || strcmp(&result->d_name[len-6], "-sd.so") != 0) {
         continue;
      }
      printf("Got: name=%s len=%d\n", result->d_name, len);
       
      pm_strcpy(fname, plugin_dir);
      if (need_slash) {
         pm_strcat(fname, "/");
      }
      pm_strcat(fname, result->d_name);
      if (lstat(fname.c_str(), &statp) != 0 || !S_ISREG(statp.st_mode)) {
         continue;                 /* ignore directories & special files */
      }

      plugin->file = bstrdup(result->d_name);
      plugin->pHandle = dlopen(fname.c_str(), RTLD_NOW);
      if (!plugin->pHandle) {
         printf("dlopen of %s failed: ERR=%s\n", fname.c_str(), dlerror());
         goto get_out;
      }

      /* Get two global entry points */
      p_bpInitialize = (t_bpInitialize)dlsym(plugin->pHandle, "bpInitialize");
      if ((error=dlerror()) != NULL) {
         printf("dlsym failed: ERR=%s\n", error);
         goto get_out;
      }
      plugin->p_bpShutdown = (t_bpShutdown)dlsym(plugin->pHandle, "bpShutdown");
      if ((error=dlerror()) != NULL) {
         printf("dlsym failed: ERR=%s\n", error);
         goto get_out;
      }

      /* Initialize the plugin */
      p_bpInitialize(&bfuncs, &plugin->pfuncs);
      printf("bacula: plugin_size=%d plugin_version=%d\n", 
              plugin->pfuncs.size, plugin->pfuncs.version);

      found = true;                /* found a plugin */
   }

get_out:
   free(entry);
   if (dp) {
      closedir(dp);
   }
   return found;
}

/*
 * Unload all the loaded plugins 
 */
void unload_plugins()
{
   Plugin *plugin;

   foreach_alist(plugin, plugin_list) {
      /* Shut it down and unload it */
      plugin->p_bpShutdown();
      dlclose(plugin->pHandle);
      if (plugin->file) {
         free(plugin->file);
      }
      free(plugin);
   }
   delete plugin_list;
   plugin_list = NULL;
}

static bpError baculaGetValue(bpContext *ctx, bVariable var, void *value)
{
   printf("bacula: GetValue var=%d\n", var);
   if (value) {
      *((int *)value) = 100;
   }
   return 0;
}

static bpError baculaSetValue(bpContext *ctx, bVariable var, void *value)
{
   printf("baculaSetValue var=%d\n", var);
   return 0;
}
