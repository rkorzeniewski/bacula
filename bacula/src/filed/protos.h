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
/*
 * Written by Kern Sibbald, MM
 *
 */

extern bool blast_data_to_storage_daemon(JCR *jcr, char *addr);
extern void do_verify_volume(JCR *jcr);
extern void do_restore(JCR *jcr);
extern int authenticate_director(JCR *jcr);
extern int authenticate_storagedaemon(JCR *jcr);
extern int make_estimate(JCR *jcr);

/* From verify.c */
int digest_file(JCR *jcr, FF_PKT *ff_pkt, DIGEST *digest);
void do_verify(JCR *jcr);

/* From heartbeat.c */
void start_heartbeat_monitor(JCR *jcr);
void stop_heartbeat_monitor(JCR *jcr);
void start_dir_heartbeat(JCR *jcr);
void stop_dir_heartbeat(JCR *jcr);

/* From acl.c */
bacl_exit_code build_acl_streams(JCR *jcr, FF_PKT *ff_pkt);
bacl_exit_code parse_acl_streams(JCR *jcr, int stream, char *content, uint32_t content_length);

/* from accurate.c */
bool accurate_finish(JCR *jcr);
bool accurate_check_file(JCR *jcr, FF_PKT *ff_pkt);
bool accurate_mark_file_as_seen(JCR *jcr, char *fname);
void accurate_free(JCR *jcr);

/* from backup.c */
bool encode_and_send_attributes(JCR *jcr, FF_PKT *ff_pkt, int &data_stream);
void strip_path(FF_PKT *ff_pkt);
void unstrip_path(FF_PKT *ff_pkt);

/* from xattr.c */
bxattr_exit_code build_xattr_streams(JCR *jcr, FF_PKT *ff_pkt);
bxattr_exit_code parse_xattr_streams(JCR *jcr, int stream, char *content, uint32_t content_length);

/* from job.c */
findINCEXE *new_exclude(JCR *jcr);
findINCEXE *new_preinclude(JCR *jcr);
findINCEXE *get_incexe(JCR *jcr);
void set_incexe(JCR *jcr, findINCEXE *incexe);
void new_options(JCR *jcr, findINCEXE *incexe);
void add_file_to_fileset(JCR *jcr, const char *fname, bool is_file);
int add_options_to_fileset(JCR *jcr, const char *item);
int add_wild_to_fileset(JCR *jcr, const char *item, int type);
int add_regex_to_fileset(JCR *jcr, const char *item, int type);
findINCEXE *new_include(JCR *jcr);
