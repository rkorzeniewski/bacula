/*
 * Director external function prototypes
 *
 *   Version $Id$
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

/* authenticate.c */
extern int authenticate_storage_daemon(JCR *jcr);
extern int authenticate_file_daemon(JCR *jcr);
extern int authenticate_user_agent(BSOCK *ua);

/* autoprune.c */
extern int do_autoprune(JCR *jcr);
extern int prune_volumes(JCR *jcr);

/* autorecycle.c */
extern int recycle_a_volume(JCR *jcr, MEDIA_DBR *mr);
extern int find_recycled_volume(JCR *jcr, MEDIA_DBR *mr);


/* catreq.c */
extern void catalog_request(JCR *jcr, BSOCK *bs, char *buf);
extern void catalog_update(JCR *jcr, BSOCK *bs, char *buf);

/* dird_conf.c */
extern char *level_to_str(int level);

/* fd_cmds.c */
extern int connect_to_file_daemon(JCR *jcr, int retry_interval,
                                  int max_retry_time, int verbose);
extern int send_include_list(JCR *jcr);
extern int send_exclude_list(JCR *jcr);
extern int get_attributes_and_put_in_catalog(JCR *jcr);
extern int get_attributes_and_compare_to_catalog(JCR *jcr, JobId_t JobId);
extern int put_file_into_catalog(JCR *jcr, long file_index, char *fname, 
                          char *link, char *attr, int stream);

/* job.c */
extern void set_jcr_defaults(JCR *jcr, JOB *job);
extern void create_unique_job_name(JCR *jcr, char *base_name);
extern void update_job_end_record(JCR *jcr);
extern int get_or_create_client_record(JCR *jcr);

/* mountreq.c */
extern void mount_request(JCR *jcr, BSOCK *bs, char *buf);

/* msgchan.c */
extern int connect_to_storage_daemon(JCR *jcr, int retry_interval,    
                              int max_retry_time, int verbose);
extern int start_storage_daemon_job(JCR *jcr);
extern int start_storage_daemon_message_thread(JCR *jcr);
extern int32_t bget_msg(BSOCK *bs, int type);
extern int response(BSOCK *fd, char *resp, char *cmd);
extern void wait_for_storage_daemon_termination(JCR *jcr);

/* newvol.c */
extern int newVolume(JCR *jcr, MEDIA_DBR *mr);

/* ua_cmd.c */
extern int create_pool(JCR *jcr, B_DB *db, POOL *pool);
extern void set_pool_dbr_defaults_in_media_dbr(MEDIA_DBR *mr, POOL_DBR *pr);
