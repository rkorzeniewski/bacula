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

/* Helper for result handler */
typedef enum {
   BVFS_FILE_RECORD  = 5,
   BVFS_DIR_RECORD   = 4,
   BVFS_FILE_VERSION = 6
} bvfs_handler_type;

typedef enum {
   BVFS_Id      = 0,
   BVFS_Name    = 1,
   BVFS_JobId   = 2,
   BVFS_LStat   = 3,

   /* Only if File record */
   BVFS_FileId  = 4,

   /* Only if File Version record */
   BVFS_Md5     = 1,
   BVFS_VolName = 4,
   BVFS_VolInchanger = 5
} bvfs_row_index;

class Bvfs {

public:
   Bvfs(JCR *j, B_DB *mdb);
   virtual ~Bvfs();

   void set_jobid(JobId_t id) {
      Mmsg(jobids.list, "%lld", (uint64_t)id);
   }

   void set_jobids(char *ids) {
      pm_strcpy(jobids.list, ids);
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
      reset_offset();
      pwd_id = pathid;
   }

   /* 
    * Returns true if the directory exists
    */
   bool ch_dir(const char *path);

   bool ls_files();             /* Returns true if we have more files to read */
   bool ls_dirs();              /* Returns true if we have more dir to read */
   void ls_special_dirs();      /* get . and .. */
   void get_all_file_versions(DBId_t pathid, DBId_t fnid, const char *client);

   void update_cache();

   void set_see_all_version(bool val) {
      see_all_version = val;
   }

   void set_see_copies(bool val) {
      see_copies = val;
   }

   void set_handler(DB_RESULT_HANDLER *h, void *ctx) {
      list_entries = h;
      user_data = ctx;
   }

   DBId_t get_pwd() {
      return pwd_id;
   }

   ATTR *get_attr() {
      return attr;
   }

   JCR *get_jcr() {
      return jcr;
   }

   void reset_offset() {
      offset=0;
   }

   void next_offset() {
      offset+=limit;
   }

   /* for internal use */
   int _handle_path(void *, int, char **);
   
private:   
   JCR *jcr;
   B_DB *db;
   db_list_ctx jobids;
   uint32_t limit;
   uint32_t offset;
   uint32_t nb_record;          /* number of records of the last query */
   POOLMEM *pattern;
   DBId_t pwd_id;               /* Current pathid */
   DBId_t dir_filenameid;       /* special FilenameId where Name='' */
   POOLMEM *prev_dir; /* ls_dirs query returns all versions, take the 1st one */
   ATTR *attr;        /* Can be use by handler to call decode_stat() */

   bool see_all_version;
   bool see_copies;

   DBId_t get_dir_filenameid();

   DB_RESULT_HANDLER *list_entries;
   void *user_data;
};

void bvfs_update_path_hierarchy_cache(JCR *jcr, B_DB *mdb, db_list_ctx *jobids);
void bvfs_update_cache(JCR *jcr, B_DB *mdb);
char *bvfs_parent_dir(char *path);

/* Return the basename of the with the trailing /  (update the given string)
 * TODO: see in the rest of bacula if we don't have
 * this function already
 */
char *bvfs_basename_dir(char *path);


#endif /* __BVFS_H_ */
