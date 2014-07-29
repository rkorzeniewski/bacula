/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2000-2014 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from many
   others, a complete list can be found in the file AUTHORS.

   You may use this file and others of this release according to the
   license defined in the LICENSE file, which includes the Affero General
   Public License, v3.0 ("AGPLv3") and some additional permissions and
   terms pursuant to its AGPLv3 Section 7.

   Bacula® is a registered trademark of Kern Sibbald.
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
   BVFS_FILE_RECORD  = 'F',
   BVFS_DIR_RECORD   = 'D',
   BVFS_FILE_VERSION = 'V',
   BVFS_VOLUME_LIST  = 'L'
} bvfs_handler_type;

typedef enum {
   BVFS_Type    = 0,            /* Could be D, F, V, L */
   BVFS_PathId  = 1,
   BVFS_FilenameId = 2,

   BVFS_Name    = 3,
   BVFS_JobId   = 4,

   BVFS_LStat   = 5,            /* Can be empty for missing directories */
   BVFS_FileId  = 6,            /* Can be empty for missing directories */

   /* Only if File Version record */
   BVFS_Md5     = 3,
   BVFS_VolName = 7,
   BVFS_VolInchanger = 8
} bvfs_row_index;

class Bvfs {

public:
   Bvfs(JCR *j, B_DB *mdb);
   virtual ~Bvfs();

   void set_jobid(JobId_t id);
   void set_jobids(char *ids);

   char *get_jobids() {
      return jobids;
   }

   void set_limit(uint32_t max) {
      limit = max;
   }

   void set_offset(uint32_t nb) {
      offset = nb;
   }

   void set_pattern(char *p) {
      uint32_t len = strlen(p);
      pattern = check_pool_memory_size(pattern, len*2+1);
      db_escape_string(jcr, db, pattern, p, len);
   }

   void set_filename(char *p) {
      uint32_t len = strlen(p);
      filename = check_pool_memory_size(filename, len*2+1);
      db_escape_string(jcr, db, filename, p, len);
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

   /* bfileview */
   void fv_update_cache();

   void set_see_all_versions(bool val) {
      see_all_versions = val;
   }

   void set_see_copies(bool val) {
      see_copies = val;
   }

   void filter_jobid();         /* Call after set_username */

   void set_username(char *user) {
      if (user) {
         username = bstrdup(user);
      }
   }

   char *escape_list(alist *list);

   bool copy_acl(alist *list) {
      if (!list ||
          (list->size() > 0 &&
           (strcasecmp((char *)list->get(0), "*all*") == 0)))
      {
         return false;
      }
      return true;
   }

   /* Keep a pointer to various ACLs */
   void set_job_acl(alist *lst) {
      job_acl = copy_acl(lst)?lst:NULL;
   }
   void set_fileset_acl(alist *lst) {
      fileset_acl = copy_acl(lst)?lst:NULL;
   }
   void set_client_acl(alist *lst) {
      client_acl = copy_acl(lst)?lst:NULL;
   }
   void set_pool_acl(alist *lst) {
      pool_acl = copy_acl(lst)?lst:NULL;
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

   /* Clear all cache */
   void clear_cache();

   /* Compute restore list */
   bool compute_restore_list(char *fileid, char *dirid, char *hardlink,
                             char *output_table);

   /* Drop previous restore list */
   bool drop_restore_list(char *output_table);

   /* for internal use */
   int _handle_path(void *, int, char **);

   /* Handle Delta parts if any */

   /* Get a list of volumes */
   void get_volumes(DBId_t fileid);

private:
   Bvfs(const Bvfs &);               /* prohibit pass by value */
   Bvfs & operator = (const Bvfs &); /* prohibit class assignment */

   JCR *jcr;
   B_DB *db;
   POOLMEM *jobids;
   char *username;              /* Used with Bweb */

   POOLMEM *prev_dir; /* ls_dirs query returns all versions, take the 1st one */
   POOLMEM *pattern;
   POOLMEM *filename;

   POOLMEM *tmp;
   POOLMEM *escaped_list;

   /* Pointer to Console ACL */
   alist *job_acl;
   alist *client_acl;
   alist *fileset_acl;
   alist *pool_acl;

   ATTR *attr;        /* Can be use by handler to call decode_stat() */

   uint32_t limit;
   uint32_t offset;
   uint32_t nb_record;          /* number of records of the last query */
   DBId_t pwd_id;               /* Current pathid */
   DBId_t dir_filenameid;       /* special FilenameId where Name='' */

   bool see_all_versions;
   bool see_copies;

   DBId_t get_dir_filenameid();

   /* bfileview */
   void fv_get_big_files(int64_t pathid, int64_t min_size, int32_t limit);
   void fv_update_size_and_count(int64_t pathid, int64_t size, int64_t count);
   void fv_compute_size_and_count(int64_t pathid, int64_t *size, int64_t *count);
   void fv_get_current_size_and_count(int64_t pathid, int64_t *size, int64_t *count);
   void fv_get_size_and_count(int64_t pathid, int64_t *size, int64_t *count);

   DB_RESULT_HANDLER *list_entries;
   void *user_data;
};

#define bvfs_is_dir(row) ((row)[BVFS_Type][0] == BVFS_DIR_RECORD)
#define bvfs_is_file(row) ((row)[BVFS_Type][0] == BVFS_FILE_RECORD)
#define bvfs_is_version(row) ((row)[BVFS_Type][0] == BVFS_FILE_VERSION)
#define bvfs_is_volume_list(row) ((row)[BVFS_Type][0] == BVFS_VOLUME_LIST)

void bvfs_update_fv_cache(JCR *jcr, B_DB *mdb, char *jobids);
int bvfs_update_path_hierarchy_cache(JCR *jcr, B_DB *mdb, char *jobids);
void bvfs_update_cache(JCR *jcr, B_DB *mdb);
char *bvfs_parent_dir(char *path);
extern const char *bvfs_select_delta_version_with_basejob_and_delta[];

/* Return the basename of the with the trailing /  (update the given string)
 * TODO: see in the rest of bacula if we don't have
 * this function already
 */
char *bvfs_basename_dir(char *path);


#endif /* __BVFS_H_ */
