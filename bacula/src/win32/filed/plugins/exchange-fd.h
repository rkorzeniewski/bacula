/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2008-2008 Free Software Foundation Europe e.V.

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
 *  Written by James Harper, October 2008
 */

#ifndef _EXCHANGE_FD_H
#define _EXCHANGE_FD_H

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "bacula.h"
#include "fd_plugins.h"
#include "api.h"

#define EXCHANGE_PLUGIN_VERSION 1

#define DLLEXPORT __declspec(dllexport)

#define JOB_TYPE_BACKUP 1
#define JOB_TYPE_RESTORE 2

#define JOB_LEVEL_FULL ((int)'F')
#define JOB_LEVEL_INCREMENTAL ((int)'I')
#define JOB_LEVEL_DIFFERENTIAL ((int)'D')

struct exchange_fd_context_t;

#include "node.h"

struct exchange_fd_context_t {
        struct bpContext *bpContext;
        WCHAR *computer_name;
        char *path_bits[6];
        root_node_t *root_node;
        node_t *current_node;
        int job_type;
        int job_level;
        time_t job_since;
        bool notrunconfull_option;
        bool truncate_logs;
};

static inline char *tocharstring(WCHAR *src)
{
        char *tmp = new char[wcslen(src) + 1];
        wcstombs(tmp, src, wcslen(src) + 1);
        return tmp;
}

static inline WCHAR *towcharstring(char *src)
{
        WCHAR *tmp = new WCHAR[strlen(src) + 1];
        mbstowcs(tmp, src, strlen(src) + 1);
        return tmp;
}


extern bFuncs *bfuncs;
extern bInfo  *binfo;

#define _DebugMessage(level, message, ...) bfuncs->DebugMessage(context->bpContext, __FILE__, __LINE__, level, message, ##__VA_ARGS__)
#define _JobMessage(type, message, ...) bfuncs->JobMessage(context->bpContext, __FILE__, __LINE__, type, 0, message, ##__VA_ARGS__)
#define _JobMessageNull(type, message, ...) bfuncs->JobMessage(NULL, __FILE__, __LINE__, type, 0, message, ##__VA_ARGS__)

#define PLUGIN_PATH_PREFIX_BASE "@EXCHANGE"
#define PLUGIN_PATH_PREFIX_SERVICE "Microsoft Information Store"
#define PLUGIN_PATH_PREFIX_SERVICE_W L"Microsoft Information Store"

#endif /* _EXCHANGE_FD_H */
