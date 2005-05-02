/*
 * Windows APIs that are different for each system.
 *   We use pointers to the entry points so that a
 *   single binary will run on all Windows systems.
 *
 *     Kern Sibbald MMIII
 */
/*
   Copyright (C) 2000-2003 Kern Sibbald and John Walker

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of
   the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA.

 */

#include "bacula.h"
#include "find.h"

#if defined(HAVE_CYGWIN) || defined(HAVE_WIN32)

/* API Pointers */

t_OpenProcessToken	p_OpenProcessToken = NULL;
t_AdjustTokenPrivileges p_AdjustTokenPrivileges = NULL;
t_LookupPrivilegeValue	p_LookupPrivilegeValue = NULL;

t_SetProcessShutdownParameters p_SetProcessShutdownParameters = NULL;

t_CreateFileA	p_CreateFileA = NULL;
t_CreateFileW	p_CreateFileW = NULL;

t_wunlink p_wunlink = NULL;
t_wmkdir p_wmkdir = NULL;
t_wopen p_wopen = NULL;
t_GetFileAttributesA	p_GetFileAttributesA = NULL;
t_GetFileAttributesW	p_GetFileAttributesW = NULL;

t_GetFileAttributesExA	p_GetFileAttributesExA = NULL;
t_GetFileAttributesExW	p_GetFileAttributesExW = NULL;

t_SetFileAttributesA	p_SetFileAttributesA = NULL;
t_SetFileAttributesW	p_SetFileAttributesW = NULL;
t_BackupRead		p_BackupRead = NULL;
t_BackupWrite		p_BackupWrite = NULL;
t_WideCharToMultiByte p_WideCharToMultiByte = NULL;
t_MultiByteToWideChar p_MultiByteToWideChar = NULL;

t_FindFirstFileA p_FindFirstFileA = NULL;
t_FindFirstFileW p_FindFirstFileW = NULL;

t_FindNextFileA p_FindNextFileA = NULL;
t_FindNextFileW p_FindNextFileW = NULL;

t_SetCurrentDirectoryA p_SetCurrentDirectoryA = NULL;
t_SetCurrentDirectoryW p_SetCurrentDirectoryW = NULL;

t_GetCurrentDirectoryA p_GetCurrentDirectoryA = NULL;
t_GetCurrentDirectoryW p_GetCurrentDirectoryW = NULL;
#endif
