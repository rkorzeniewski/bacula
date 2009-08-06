/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2000-2009 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from
   many others, a complete list can be found in the file AUTHORS.
   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version two of the GNU General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

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


#ifndef __BVFS_H_
#define __BVFS_H_ 1


/* 
 * This object can be use to browse the catalog
 *
 * Bvfs fs;
 * fs.set_jobid(10);
 * fs.update_cache();
 * fs.ch_dir("/");
 * fs.ls_dirs();
 * fs.ls_files();
 */

class Bvfs {

public:
   Bvfs(JCR *j, B_DB *mdb) {
      jcr = j;
      jcr->inc_use_count();
      db = mdb;                 /* need to inc ref count */
      jobids = get_pool_memory(PM_NAME);
      pattern = get_pool_memory(PM_NAME);
      *pattern = *jobids = 0;
      dir_filenameid = pwd_id = offset = 0;
      see_copies = see_all_version = false;
      limit = 1000;
   }

   virtual ~Bvfs() {
      free_pool_memory(jobids);
      free_pool_memory(pattern);
      jcr->dec_use_count();
   }

   void set_jobid(JobId_t id) {
      Mmsg(jobids, "%lld", (uint64_t)id);
   }

   void set_jobids(char *ids) {
      pm_strcpy(jobids, ids);
   }

   void set_limit(uint32_t max) {
      limit = max;
   }

   void set_offset(uint32_t nb) {
      offset = nb;
   }

   void set_pattern(char *p) {
      uint32_t len = strlen(p)*2+1;
      pattern = check_pool_memory_size(pattern, len);
      db_escape_string(jcr, db, pattern, p, len);
   }

   /* Get the root point */
   DBId_t get_root();

   /* It's much better to access Path though their PathId, it
    * avoids mistakes with string encoding
    */
   void ch_dir(DBId_t pathid) {
      pwd_id = pathid;
   }

   /* 
    * Returns true if the directory exists
    */
   bool ch_dir(char *path);

   void ls_files();
   void ls_dirs();
   void ls_special_dirs();      /* get . and .. */
   void get_all_file_versions(DBId_t pathid, DBId_t fnid, char *client);

   void update_cache();

   void set_see_all_version(bool val) {
      see_all_version = val;
   }

   void set_see_copies(bool val) {
      see_copies = val;
   }

private:   
   JCR *jcr;
   B_DB *db;
   POOLMEM *jobids;
   uint32_t limit;
   uint32_t offset;
   POOLMEM *pattern;
   DBId_t pwd_id;
   DBId_t dir_filenameid;

   bool see_all_version;
   bool see_copies;

   DBId_t get_dir_filenameid();
};

void bvfs_update_path_hierarchy_cache(JCR *jcr, B_DB *mdb, char *jobids);
void bvfs_update_cache(JCR *jcr, B_DB *mdb);
char *bvfs_parent_dir(char *path);

/* Return the basename of the with the trailing /  (update the given string)
 * TODO: see in the rest of bacula if we don't have
 * this function already
 */
char *bvfs_basename_dir(char *path);


#endif /* __BVFS_H_ */
