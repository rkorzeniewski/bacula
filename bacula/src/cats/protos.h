/*
 *
 *    Version $Id$
 */
/*
   Copyright (C) 2000, 2001, 2002 Kern Sibbald and John Walker

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

#ifndef __SQL_PROTOS_H
#define __SQL_PROTOS_H

#include "cats.h"

/* Database prototypes */

/* sql.c */
B_DB *db_init_database(char *db_name, char *db_user, char *db_password);
int db_open_database(B_DB *db);
void db_close_database(B_DB *db);
void db_escape_string(char *snew, char *old, int len);
char *db_strerror(B_DB *mdb);
int get_sql_record_max(B_DB *mdb);
char *db_next_index(B_DB *mdb, char *table);
int db_sql_query(B_DB *mdb, char *cmd, DB_RESULT_HANDLER *result_handler, void *ctx);
int check_tables_version(B_DB *mdb);
void _db_unlock(char *file, int line, B_DB *mdb);
void _db_lock(char *file, int line, B_DB *mdb);

/* create.c */
int db_create_file_attributes_record(B_DB *mdb, ATTR_DBR *ar);
int db_create_job_record(B_DB *db, JOB_DBR *jr);
int db_create_media_record(B_DB *db, MEDIA_DBR *media_dbr);
int db_create_client_record(B_DB *db, CLIENT_DBR *cr);
int db_create_fileset_record(B_DB *db, FILESET_DBR *fsr);
int db_create_pool_record(B_DB *db, POOL_DBR *pool_dbr);
int db_create_jobmedia_record(B_DB *mdb, JOBMEDIA_DBR *jr);

/* delete.c */
int db_delete_pool_record(B_DB *db, POOL_DBR *pool_dbr);
int db_delete_media_record(B_DB *mdb, MEDIA_DBR *mr);

/* find.c */
int db_find_job_start_time(B_DB *mdb, JOB_DBR *jr, char *stime);
int db_find_last_full_verify(B_DB *mdb, JOB_DBR *jr);
int db_find_next_volume(B_DB *mdb, int index, MEDIA_DBR *mr);

/* get.c */
int db_get_pool_record(B_DB *db, POOL_DBR *pdbr);
int db_get_client_record(B_DB *mdb, CLIENT_DBR *cr);
int db_get_job_record(B_DB *mdb, JOB_DBR *jr);
int db_get_job_volume_names(B_DB *mdb, uint32_t JobId, char *VolumeNames);
int db_get_file_attributes_record(B_DB *mdb, char *fname, FILE_DBR *fdbr);
int db_get_fileset_record(B_DB *mdb, FILESET_DBR *fsr);
int db_get_media_record(B_DB *mdb, MEDIA_DBR *mr);
int db_get_num_media_records(B_DB *mdb);
int db_get_num_pool_records(B_DB *mdb);
int db_get_pool_ids(B_DB *mdb, int *num_ids, uint32_t **ids);
int db_get_media_ids(B_DB *mdb, int *num_ids, uint32_t **ids);


/* list.c */
void db_list_pool_records(B_DB *db, DB_LIST_HANDLER sendit, void *ctx);
void db_list_job_records(B_DB *db, JOB_DBR *jr, DB_LIST_HANDLER sendit, void *ctx);
void db_list_job_totals(B_DB *db, JOB_DBR *jr, DB_LIST_HANDLER sendit, void *ctx);
void db_list_files_for_job(B_DB *db, uint32_t jobid, DB_LIST_HANDLER sendit, void *ctx);
void db_list_media_records(B_DB *mdb, MEDIA_DBR *mdbr, DB_LIST_HANDLER *sendit, void *ctx);
void db_list_jobmedia_records(B_DB *mdb, uint32_t JobId, DB_LIST_HANDLER *sendit, void *ctx);
int  db_list_sql_query(B_DB *mdb, char *query, DB_LIST_HANDLER *sendit, void *ctx);

/* update.c */
int  db_update_job_start_record(B_DB *db, JOB_DBR *jr);
int  db_update_job_end_record(B_DB *db, JOB_DBR *jr);
int  db_update_pool_record(B_DB *db, POOL_DBR *pr);
int  db_update_media_record(B_DB *db, MEDIA_DBR *mr);
int  db_add_MD5_to_file_record(B_DB *mdb, FileId_t FileId, char *MD5);	
int  db_mark_file_record(B_DB *mdb, FileId_t FileId, int JobId);

#endif /* __SQL_PROTOS_H */
