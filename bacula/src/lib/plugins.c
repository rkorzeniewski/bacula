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

   Bacula® is a registered trademark of Kern Sibbald.
   The licensor of Bacula is the Free Software Foundation Europe
   (FSFE), Fiduciary Program, Sumatrastrasse 25, 8006 Zürich,
   Switzerland, email:ftf@fsfeurope.org.
*/
/*
 *    Plugin load/unloader for all Bacula daemons
 *
 * Kern Sibbald, October 2007
 */
#include "bacula.h"
#include "plugins.h"

/* All loaded plugins */
alist *plugin_list = NULL;

/*
 * Create a new plugin "class" entry and enter it in the
 *  list of plugins.  Note, this is not the same as
 *  an instance of the plugin. 
 */
Plugin *new_plugin()
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
bool load_plugins(void *binfo, void *bfuncs, const char *plugin_dir, const char *type)
{
   bool found = false;
//#ifndef HAVE_WIN32
   t_loadPlugin loadPlugin;
   Plugin *plugin;
   DIR* dp = NULL;
   struct dirent *entry = NULL, *result;
   int name_max;
   struct stat statp;
   POOL_MEM fname(PM_FNAME);
   bool need_slash = false;
   int len, type_len;


   name_max = pathconf(".", _PC_NAME_MAX);
   if (name_max < 1024) {
      name_max = 1024;
   }

   if (!(dp = opendir(plugin_dir))) {
      berrno be;
      Jmsg(NULL, M_ERROR, 0, _("Failed to open Plugin directory %s: ERR=%s\n"), 
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
         if (!found) {
            Jmsg(NULL, M_INFO, 0, _("Failed to find any plugins in %s\n"), 
                  plugin_dir);
         }
         break;
      }
      if (strcmp(result->d_name, ".") == 0 || 
          strcmp(result->d_name, "..") == 0) {
         continue;
      }

      len = strlen(result->d_name);
      type_len = strlen(type);
      if (len < type_len+1 || strcmp(&result->d_name[len-type_len], type) != 0) {
         Dmsg3(100, "Rejected plugin: want=%s name=%s len=%d\n", type, result->d_name, len);
         continue;
      }
      Dmsg2(100, "Loaded plugin: name=%s len=%d\n", result->d_name, len);
       
      pm_strcpy(fname, plugin_dir);
      if (need_slash) {
         pm_strcat(fname, "/");
      }
      pm_strcat(fname, result->d_name);
      if (lstat(fname.c_str(), &statp) != 0 || !S_ISREG(statp.st_mode)) {
         continue;                 /* ignore directories & special files */
      }

      plugin = new_plugin();
      plugin->file = bstrdup(result->d_name);
      plugin->pHandle = dlopen(fname.c_str(), RTLD_NOW);
      if (!plugin->pHandle) {
         Jmsg(NULL, M_ERROR, 0, _("Plugin load %s failed: ERR=%s\n"), 
              fname.c_str(), NPRT(dlerror()));
         goto get_out;
      }

      /* Get two global entry points */
      loadPlugin = (t_loadPlugin)dlsym(plugin->pHandle, "loadPlugin");
      if (!loadPlugin) {
         Jmsg(NULL, M_ERROR, 0, _("Lookup of loadPlugin in plugin %s failed: ERR=%s\n"),
            fname.c_str(), NPRT(dlerror()));
         goto get_out;
      }
      plugin->unloadPlugin = (t_unloadPlugin)dlsym(plugin->pHandle, "unloadPlugin");
      if (!plugin->unloadPlugin) {
         Jmsg(NULL, M_ERROR, 0, _("Lookup of unloadPlugin in plugin %s failed: ERR=%s\n"),
            fname.c_str(), NPRT(dlerror()));
         goto get_out;
      }

      /* Initialize the plugin */
      loadPlugin(binfo, bfuncs, &plugin->pinfo, &plugin->pfuncs);

      found = true;                /* found a plugin */
   }

get_out:
   if (entry) {
      free(entry);
   }
   if (dp) {
      closedir(dp);
   }
//#endif
   return found;
}

/*
 * Unload all the loaded plugins 
 */
void unload_plugins()
{
//#ifndef HAVE_WIN32
   Plugin *plugin;

   if (!plugin_list) {
      return;
   }
   foreach_alist(plugin, plugin_list) {
      /* Shut it down and unload it */
      plugin->unloadPlugin();
      dlclose(plugin->pHandle);
      if (plugin->file) {
         free(plugin->file);
      }
      free(plugin);
   }
   delete plugin_list;
   plugin_list = NULL;
//#endif
}
