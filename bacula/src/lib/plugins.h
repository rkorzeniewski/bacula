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
 * Bacula plugin loader/unloader
 *
 * Kern Sibbald, October 2007
 */
#ifndef __PLUGINS_H
#define __PLUGINS_H

#include "bacula.h"
#ifndef HAVE_WIN32
#include <dlfcn.h>
#endif
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

/****************************************************************************
 *                                                                          *
 *                Common definitions for all plugins                        *
 *                                                                          *
 ****************************************************************************/

extern DLL_IMP_EXP alist *plugin_list;

/* Universal return codes from all functions */
typedef enum {
  bRC_OK    = 0,                         /* OK */
  bRC_Stop  = 1,                         /* Stop calling plugins */
  bRC_Error = 2,
} bRC;

/* Context packet as first argument of all functions */
typedef struct s_bpContext {
  void *bContext;                        /* Bacula private context */
  void *pContext;                        /* Plugin private context */
} bpContext;

extern "C" {
typedef bRC (*t_loadPlugin)(void *binfo, void *bfuncs, void **pinfo, void **pfuncs);
typedef bRC (*t_unloadPlugin)(void);
}

class Plugin {
public:
   char *file;
   t_unloadPlugin unloadPlugin;
   void *pinfo;
   void *pfuncs;
   void *pHandle;
};

/* Functions */
extern Plugin *new_plugin();
extern bool load_plugins(void *binfo, void *bfuncs, const char *plugin_dir, const char *type);
extern void unload_plugins();


#endif /* __PLUGINS_H */
