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
 * Protypes for stored
 *
 *   Written by Kern Sibbald, MM
 */

/* From stored.c */
uint32_t new_VolSessionId();

/* From acquire.c */
DCR     *acquire_device_for_append(DCR *dcr);
bool     acquire_device_for_read(DCR *dcr);
bool     release_device(DCR *dcr);
bool     clean_device(DCR *dcr);
DCR     *new_dcr(JCR *jcr, DCR *dcr, DEVICE *dev, bool writing=true);
void     free_dcr(DCR *dcr);

/* From append.c */
bool send_attrs_to_dir(JCR *jcr, DEV_RECORD *rec);

/* From askdir.c */
enum get_vol_info_rw {
   GET_VOL_INFO_FOR_WRITE,
   GET_VOL_INFO_FOR_READ
};
bool    dir_get_volume_info(DCR *dcr, enum get_vol_info_rw);
bool    dir_find_next_appendable_volume(DCR *dcr);
bool    dir_update_volume_info(DCR *dcr, bool label, bool update_LastWritten);
bool    dir_ask_sysop_to_create_appendable_volume(DCR *dcr);
bool    dir_ask_sysop_to_mount_volume(DCR *dcr, bool read_access);
bool    dir_update_file_attributes(DCR *dcr, DEV_RECORD *rec);
bool    dir_send_job_status(JCR *jcr);
bool    dir_create_jobmedia_record(DCR *dcr, bool zero=false);
bool    dir_update_device(JCR *jcr, DEVICE *dev);
bool    dir_update_changer(JCR *jcr, AUTOCHANGER *changer);

/* authenticate.c */
int     authenticate_director(JCR *jcr);
int     authenticate_filed(JCR *jcr);
bool    authenticate_storagedaemon(JCR *jcr, char *Job);

/* From autochanger.c */
bool     init_autochangers();
int      autoload_device(DCR *dcr, bool writing, BSOCK *dir);
bool     autochanger_cmd(DCR *dcr, BSOCK *dir, const char *cmd);
bool     unload_autochanger(DCR *dcr, int loaded);
bool     unload_dev(DCR *dcr, DEVICE *dev);
char    *edit_device_codes(DCR *dcr, char *omsg, const char *imsg, const char *cmd);
int      get_autochanger_loaded_slot(DCR *dcr);

/* From block.c */
void    dump_block(DEV_BLOCK *b, const char *msg);
DEV_BLOCK *new_block(DEVICE *dev);
DEV_BLOCK *dup_block(DEV_BLOCK *eblock);
void    init_block_write(DEV_BLOCK *block);
void    empty_block(DEV_BLOCK *block);
void    free_block(DEV_BLOCK *block);
void    print_block_read_errors(JCR *jcr, DEV_BLOCK *block);
void    ser_block_header(DEV_BLOCK *block);
bool    is_block_empty(DEV_BLOCK *block);
bool    terminate_writing_volume(DCR *dcr);

/* From block_util.c */
bool    terminate_writing_volume(DCR *dcr);
bool    user_volume_size_reached(DCR *dcr, bool quiet);
bool    check_for_newvol_or_newfile(DCR *dcr);

/* From butil.c -- utilities for SD tool programs */
void    setup_me();
void    print_ls_output(const char *fname, const char *link, int type, struct stat *statp);
JCR    *setup_jcr(const char *name, char *dev_name, BSR *bsr,
                  const char *VolumeName, bool writing);
void    display_tape_error_status(JCR *jcr, DEVICE *dev);


/* From dev.c */
DEVICE  *init_dev(JCR *jcr, DEVRES *device);
bool     can_open_mounted_dev(DEVICE *dev);
bool     load_dev(DEVICE *dev);
int      write_block(DEVICE *dev);
uint32_t status_dev(DEVICE *dev);
void     attach_jcr_to_device(DEVICE *dev, JCR *jcr);
void     detach_jcr_from_device(DEVICE *dev, JCR *jcr);
JCR     *next_attached_jcr(DEVICE *dev, JCR *jcr);
void     init_device_wait_timers(DCR *dcr);
void     init_jcr_device_wait_timers(JCR *jcr);
bool     double_dev_wait_time(DEVICE *dev);

/* From dvd.c */
int     dvd_open_next_part(DCR *dcr);
bool    dvd_write_part(DCR *dcr);
bool    dvd_close_job(DCR *dcr);
void    make_mounted_dvd_filename(DEVICE *dev, POOL_MEM &archive_name);
void    make_spooled_dvd_filename(DEVICE *dev, POOL_MEM &archive_name);
bool    truncate_dvd(DCR *dcr);
bool    check_can_write_on_non_blank_dvd(DCR *dcr);
int     find_num_dvd_parts(DCR *dcr);
boffset_t   lseek_dvd(DCR *dcr, boffset_t offset, int whence);
void    dvd_remove_empty_part(DCR *dcr);

/* From device.c */
bool     open_dev(DCR *dcr);
bool     first_open_device(DCR *dcr);
bool     fixup_device_block_write_error(DCR *dcr, int retries=4);
void     set_start_vol_position(DCR *dcr);
void     set_new_volume_parameters(DCR *dcr);
void     set_new_file_parameters(DCR *dcr);

/* From dircmd.c */
void     *handle_connection_request(void *arg);


/* From fd_cmds.c */
void     run_job(JCR *jcr);
void     do_client_commands(JCR *jcr);

/* From job.c */
void     stored_free_jcr(JCR *jcr);
void     connection_from_filed(void *arg);
void     handle_filed_connection(BSOCK *fd, char *job_name,
           int fdversion, int sdversion);

/* From label.c */
int      read_dev_volume_label(DCR *dcr);
int      read_dvd_volume_label(DCR *dcr, bool write);
void     create_session_label(DCR *dcr, DEV_RECORD *rec, int label);
void     create_volume_header(DEVICE *dev, const char *VolName, const char *PoolName, bool dvdnow);
#define ANSI_VOL_LABEL 0
#define ANSI_EOF_LABEL 1
#define ANSI_EOV_LABEL 2
bool     write_ansi_ibm_labels(DCR *dcr, int type, const char *VolName);
int      read_ansi_ibm_label(DCR *dcr);
bool     write_session_label(DCR *dcr, int label);
void     dump_volume_label(DEVICE *dev);
void     dump_label_record(DEVICE *dev, DEV_RECORD *rec, int verbose);
bool     unser_volume_label(DEVICE *dev, DEV_RECORD *rec);
bool     unser_session_label(SESSION_LABEL *label, DEV_RECORD *rec);
bool     write_new_volume_label_to_dev(DCR *dcr, const char *VolName,
            const char *PoolName, bool relabel, bool dvdnow);

/* From locks.c */
void     _lock_device(const char *file, int line, DEVICE *dev);
void     _unlock_device(const char *file, int line, DEVICE *dev);
void     _block_device(const char *file, int line, DEVICE *dev, int state);
void     _unblock_device(const char *file, int line, DEVICE *dev);
void     _steal_device_lock(const char *file, int line, DEVICE *dev, bsteal_lock_t *hold, int state);
void     _give_back_device_lock(const char *file, int line, DEVICE *dev, bsteal_lock_t *hold);


/* From match_bsr.c */
int      match_bsr(BSR *bsr, DEV_RECORD *rec, VOLUME_LABEL *volrec,
              SESSION_LABEL *sesrec, JCR *jcr);
int      match_bsr_block(BSR *bsr, DEV_BLOCK *block);
void     position_bsr_block(BSR *bsr, DEV_BLOCK *block);
BSR     *find_next_bsr(BSR *root_bsr, DEVICE *dev);
bool     is_this_bsr_done(BSR *bsr, DEV_RECORD *rec);
uint64_t get_bsr_start_addr(BSR *bsr,
                            uint32_t *file=NULL,
                            uint32_t *block=NULL);


/* From mount.c */
bool     mount_next_read_volume(DCR *dcr);

/* From parse_bsr.c */
BSR     *parse_bsr(JCR *jcr, char *lf);
void     dump_bsr(BSR *bsr, bool recurse);
void     free_bsr(BSR *bsr);
void     free_restore_volume_list(JCR *jcr);
void     create_restore_volume_list(JCR *jcr);

/* From record.c */
const char *FI_to_ascii(char *buf, int fi);
const char *stream_to_ascii(char *buf, int stream, int fi);
bool        write_record_to_block(DCR *dcr, DEV_RECORD *rec);
bool        can_write_record_to_block(DEV_BLOCK *block, DEV_RECORD *rec);
bool        read_record_from_block(DCR *dcr, DEV_RECORD *rec);
DEV_RECORD *new_record();
void        free_record(DEV_RECORD *rec);
void        empty_record(DEV_RECORD *rec);
uint64_t    get_record_address(DEV_RECORD *rec);

/* From read_record.c */
bool read_records(DCR *dcr,
       bool record_cb(DCR *dcr, DEV_RECORD *rec),
       bool mount_cb(DCR *dcr));

/* From reserve.c */
void    init_reservations_lock();
void    term_reservations_lock();
void    send_drive_reserve_messages(JCR *jcr, void sendit(const char *msg, int len, void *sarg), void *arg);
bool    find_suitable_device_for_job(JCR *jcr, RCTX &rctx);
int     search_res_for_device(RCTX &rctx);
void    release_reserve_messages(JCR *jcr);

extern int reservations_lock_count;

/* From status.c */
void    _dbg_list_one_device(DEVICE *dev, const char *f, int l);
#define dbg_list_one_device(x, dev) if (chk_dbglvl(x))     \
        _dbg_list_one_device(dev, __FILE__, __LINE__)

/* From vol_mgr.c */
void    init_vol_list_lock();
void    term_vol_list_lock();
VOLRES *reserve_volume(DCR *dcr, const char *VolumeName);
bool    free_volume(DEVICE *dev);
bool    is_vol_list_empty();
dlist  *dup_vol_list(JCR *jcr);
void    free_temp_vol_list(dlist *temp_vol_list);
bool    volume_unused(DCR *dcr);
void    create_volume_lists();
void    free_volume_lists();
void    list_volumes(void sendit(const char *msg, int len, void *sarg), void *arg);
bool    is_volume_in_use(DCR *dcr);
extern  int vol_list_lock_count;
void    add_read_volume(JCR *jcr, const char *VolumeName);
void    remove_read_volume(JCR *jcr, const char *VolumeName);


/* From spool.c */
bool    begin_data_spool          (DCR *dcr);
bool    discard_data_spool        (DCR *dcr);
bool    commit_data_spool         (DCR *dcr);
bool    are_attributes_spooled    (JCR *jcr);
bool    begin_attribute_spool     (JCR *jcr);
bool    discard_attribute_spool   (JCR *jcr);
bool    commit_attribute_spool    (JCR *jcr);
bool    write_block_to_spool_file (DCR *dcr);
void    list_spool_stats          (void sendit(const char *msg, int len, void *sarg), void *arg);

/* From wait.c */
int wait_for_sysop(DCR *dcr);
bool wait_for_any_device(JCR *jcr, int &retries);
bool wait_for_device(DCR *dcr, int &retries);

/* stored_conf.c */
void store_devtype(LEX *lc, RES_ITEM *item, int index, int pass);
void store_maxblocksize(LEX *lc, RES_ITEM *item, int index, int pass);
