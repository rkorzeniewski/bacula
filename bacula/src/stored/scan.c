/*
 *
 *   scan.c scan a directory (on a removable file) for a valid
 *      Volume name. If found, open the file for append.
 *
 *    Kern Sibbald, MMVI
 *
 *   Version $Id$
 */
/*
   Copyright (C) 2006 Kern Sibbald

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   version 2 as amended with additional clauses defined in the
   file LICENSE in the main source directory.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the 
   the file LICENSE for additional details.

 */

#include "bacula.h"
#include "stored.h"

/* Forward referenced functions */
static bool is_volume_name_legal(char *name);


bool DEVICE::scan_dir_for_volume(DCR *dcr)
{
   DIR* dp;
   struct dirent *entry, *result;
   int name_max;
   char *mount_point;
   VOLUME_CAT_INFO dcrVolCatInfo, devVolCatInfo;
   struct stat statp;
   bool found = false;
   POOL_MEM fname(PM_FNAME);
   bool need_slash = false;
   int len;

   
   name_max = pathconf(".", _PC_NAME_MAX);
   if (name_max < 1024) {
      name_max = 1024;
   }

   if (device->mount_point) {
      mount_point = device->mount_point;
   } else {
      mount_point = device->device_name;
   }
      
   if (!(dp = opendir(mount_point))) {
      berrno be;
      dev_errno = errno;
      Dmsg3(29, "scan_dir_for_vol: failed to open dir %s (dev=%s), ERR=%s\n", 
            mount_point, print_name(), be.strerror());
      goto get_out;
   }
   
   len = strlen(mount_point);
   if (len > 0) {
      need_slash = !IsPathSeparator(mount_point[len - 1]);
   }
   entry = (struct dirent *)malloc(sizeof(struct dirent) + name_max + 1000);
   for ( ;; ) {
      if ((readdir_r(dp, entry, &result) != 0) || (result == NULL)) {
         dev_errno = EIO;
         Dmsg2(129, "scan_dir_for_vol: failed to find suitable file in dir %s (dev=%s)\n", 
               mount_point, print_name());
         break;
      }
      if (strcmp(result->d_name, ".") == 0 || 
          strcmp(result->d_name, "..") == 0) {
         continue;
      }
       
      if (!is_volume_name_legal(result->d_name)) {
         continue;
      }
      pm_strcpy(fname, mount_point);
      if (need_slash) {
         pm_strcat(fname, "/");
      }
      pm_strcat(fname, result->d_name);
      if (lstat(fname.c_str(), &statp) != 0 ||
          !S_ISREG(statp.st_mode)) {
         continue;                 /* ignore directories & special files */
      }

      /*
       * OK, we got a different volume mounted. First save the
       *  requested Volume info (dcr) structure, then query if
       *  this volume is really OK. If not, put back the desired
       *  volume name, mark it not in changer and continue.
       */
      dcrVolCatInfo = dcr->VolCatInfo;     /* structure assignment */
      devVolCatInfo = VolCatInfo;          /* structure assignment */
      /* Check if this is a valid Volume in the pool */
      bstrncpy(dcr->VolumeName, result->d_name, sizeof(dcr->VolumeName));
      if (!dir_get_volume_info(dcr, GET_VOL_INFO_FOR_WRITE)) {
         continue;
      }
      /* This was not the volume we expected, but it is OK with
       * the Director, so use it.
       */
      VolCatInfo = dcr->VolCatInfo;       /* structure assignment */
      found = true;
      break;                /* got a Volume */
   }
   free(entry);
   closedir(dp);
   
get_out:
   sm_check(__FILE__, __LINE__, false);
   return found;
}

/*
 * Check if the Volume name has legal characters
 * If ua is non-NULL send the message
 */
static bool is_volume_name_legal(char *name)
{
   int len;
   const char *p;
   const char *accept = ":.-_";

   /* Restrict the characters permitted in the Volume name */
   for (p=name; *p; p++) {
      if (B_ISALPHA(*p) || B_ISDIGIT(*p) || strchr(accept, (int)(*p))) {
         continue;
      }
      return false;
   }
   len = strlen(name);
   if (len >= MAX_NAME_LENGTH) {
      return false;
   }
   if (len == 0) {
      return false;
   }
   return true;
}
