/*
 * Protypes for stored
 *
 *   Version $Id$
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
   
/* From stored.c */
uint32_t new_VolSessionId();

/* From acquire.c */
DEVICE	*acquire_device_for_append(JCR *jcr, DEVICE *dev, DEV_BLOCK *block);
int	 acquire_device_for_read(JCR *jcr, DEVICE *dev, DEV_BLOCK *block);
int	 release_device(JCR *jcr, DEVICE *dev);

/* From askdir.c */
enum get_vol_info_rw {
   GET_VOL_INFO_FOR_WRITE,
   GET_VOL_INFO_FOR_READ
};
int	dir_get_volume_info(JCR *jcr, enum get_vol_info_rw);
int	dir_find_next_appendable_volume(JCR *jcr);
int	dir_update_volume_info(JCR *jcr, VOLUME_CAT_INFO *vol, int label);
int	dir_ask_sysop_to_mount_next_volume(JCR *jcr, DEVICE *dev);
int	dir_ask_sysop_to_mount_volume(JCR *jcr, DEVICE *dev);
int	dir_update_file_attributes(JCR *jcr, DEV_RECORD *rec);
int	dir_send_job_status(JCR *jcr);
int	dir_create_jobmedia_record(JCR *jcr);

/* authenticate.c */
int	authenticate_director(JCR *jcr);
int	authenticate_filed(JCR *jcr);

/* From block.c */
void	dump_block(DEV_BLOCK *b, char *msg);
DEV_BLOCK *new_block(DEVICE *dev);
void	init_block_write(DEV_BLOCK *block);
void	empty_block(DEV_BLOCK *block);
void	free_block(DEV_BLOCK *block);
int	write_block_to_device(JCR *jcr, DEVICE *dev, DEV_BLOCK *block);
int	write_block_to_dev(JCR *jcr, DEVICE *dev, DEV_BLOCK *block);
void	print_block_errors(JCR *jcr, DEV_BLOCK *block);

#define CHECK_BLOCK_NUMBERS    true
#define NO_BLOCK_NUMBER_CHECK  false
int	read_block_from_device(JCR *jcr, DEVICE *dev, DEV_BLOCK *block, bool check_block_numbers);
int	read_block_from_dev(JCR *jcr, DEVICE *dev, DEV_BLOCK *block, bool check_block_numbers);

/* From butil.c -- utilities for SD tool programs */
void	print_ls_output(char *fname, char *link, int type, struct stat *statp);
JCR    *setup_jcr(char *name, char *device, BSR *bsr, char *VolumeName);
DEVICE *setup_to_access_device(JCR *jcr, int read_access);
void	display_tape_error_status(JCR *jcr, DEVICE *dev);
DEVRES *find_device_res(char *device_name, int read_access);


/* From dev.c */
DEVICE	*init_dev(DEVICE *dev, DEVRES *device);
int	 open_dev(DEVICE *dev, char *VolName, int mode);
void	 close_dev(DEVICE *dev);
void	 force_close_dev(DEVICE *dev);
int	 truncate_dev(DEVICE *dev);
void	 term_dev(DEVICE *dev);
char *	 strerror_dev(DEVICE *dev);
void	 clrerror_dev(DEVICE *dev, int func);
int	 update_pos_dev(DEVICE *dev);
int	 rewind_dev(DEVICE *dev);
int	 load_dev(DEVICE *dev);
int	 offline_dev(DEVICE *dev);
int	 flush_dev(DEVICE *dev);
int	 weof_dev(DEVICE *dev, int num);
int	 write_block(DEVICE *dev);
int	 write_dev(DEVICE *dev, char *buf, size_t len);
int	 read_dev(DEVICE *dev, char *buf, size_t len);
int	 status_dev(DEVICE *dev, uint32_t *status);
int	 eod_dev(DEVICE *dev);
int	 fsf_dev(DEVICE *dev, int num);
int	 fsr_dev(DEVICE *dev, int num);
int	 bsf_dev(DEVICE *dev, int num);
int	 bsr_dev(DEVICE *dev, int num);
void	 attach_jcr_to_device(DEVICE *dev, JCR *jcr);
void	 detach_jcr_from_device(DEVICE *dev, JCR *jcr);
JCR	*next_attached_jcr(DEVICE *dev, JCR *jcr);
int	 dev_can_write(DEVICE *dev);
int	 offline_or_rewind_dev(DEVICE *dev);
int	 reposition_dev(DEVICE *dev, uint32_t file, uint32_t block);


/* Get info about device */
char *	 dev_name(DEVICE *dev);
char *	 dev_vol_name(DEVICE *dev);
uint32_t dev_block(DEVICE *dev);
uint32_t dev_file(DEVICE *dev);
int	 dev_is_tape(DEVICE *dev);

/* From device.c */
int	 open_device(DEVICE *dev);
int	 fixup_device_block_write_error(JCR *jcr, DEVICE *dev, DEV_BLOCK *block);
void _lock_device(char *file, int line, DEVICE *dev);
void _unlock_device(char *file, int line, DEVICE *dev);
void _block_device(char *file, int line, DEVICE *dev, int state);
void _unblock_device(char *file, int line, DEVICE *dev);
void _steal_device_lock(char *file, int line, DEVICE *dev, bsteal_lock_t *hold, int state);
void _give_back_device_lock(char *file, int line, DEVICE *dev, bsteal_lock_t *hold);
void set_new_volume_parameters(JCR *jcr, DEVICE *dev);
void set_new_file_parameters(JCR *jcr, DEVICE *dev); 
int  device_is_unmounted(DEVICE *dev);

/* From dircmd.c */
void	 *connection_request(void *arg); 


/* From fd_cmds.c */
void	 run_job(JCR *jcr);

/* From job.c */
void	 stored_free_jcr(JCR *jcr);
void	 connection_from_filed(void *arg);     
void	 handle_filed_connection(BSOCK *fd, char *job_name);

/* From label.c */
int	 read_dev_volume_label(JCR *jcr, DEVICE *dev, DEV_BLOCK *block);
void	 create_session_label(JCR *jcr, DEV_RECORD *rec, int label);
void	 create_volume_label(DEVICE *dev, char *VolName, char *PoolName);
int	 write_volume_label_to_dev(JCR *jcr, DEVRES *device, char *VolName, char *PoolName);
int	 write_session_label(JCR *jcr, DEV_BLOCK *block, int label);
int	 write_volume_label_to_block(JCR *jcr, DEVICE *dev, DEV_BLOCK *block);
void	 dump_volume_label(DEVICE *dev);
void	 dump_label_record(DEVICE *dev, DEV_RECORD *rec, int verbose);
int	 unser_volume_label(DEVICE *dev, DEV_RECORD *rec);
int	 unser_session_label(SESSION_LABEL *label, DEV_RECORD *rec);

/* From match_bsr.c */
int match_bsr(BSR *bsr, DEV_RECORD *rec, VOLUME_LABEL *volrec, 
	      SESSION_LABEL *sesrec);
int match_bsr_block(BSR *bsr, DEV_BLOCK *block);
void position_bsr_block(BSR *bsr, DEV_BLOCK *block);
BSR *find_next_bsr(BSR *root_bsr, DEVICE *dev);

/* From mount.c */
int	 mount_next_write_volume(JCR *jcr, DEVICE *dev, DEV_BLOCK *block, int release);
int	 mount_next_read_volume(JCR *jcr, DEVICE *dev, DEV_BLOCK *block);
void	 release_volume(JCR *jcr, DEVICE *dev);

/* From autochanger.c */
int	 autoload_device(JCR *jcr, DEVICE *dev, int writing, BSOCK *dir);
int	 autochanger_list(JCR *jcr, DEVICE *dev, BSOCK *dir);
void	 invalidate_slot_in_catalog(JCR *jcr);


/* From parse_bsr.c */
extern BSR *parse_bsr(JCR *jcr, char *lf);
void dump_bsr(BSR *bsr, bool recurse);
extern void free_bsr(BSR *bsr);
extern VOL_LIST *new_vol();
extern int add_vol(JCR *jcr, VOL_LIST *vol);
extern void free_vol_list(JCR *jcr);
extern void create_vol_list(JCR *jcr);

/* From record.c */
char   *FI_to_ascii(int fi);
char   *stream_to_ascii(int stream, int fi);
int	write_record_to_block(DEV_BLOCK *block, DEV_RECORD *rec);
int	can_write_record_to_block(DEV_BLOCK *block, DEV_RECORD *rec);
int	read_record_from_block(DEV_BLOCK *block, DEV_RECORD *rec); 
DEV_RECORD *new_record();
void	free_record(DEV_RECORD *rec);

/* From read_record.c */
int read_records(JCR *jcr,  DEVICE *dev, 
       int record_cb(JCR *jcr, DEVICE *dev, DEV_BLOCK *block, DEV_RECORD *rec),
       int mount_cb(JCR *jcr, DEVICE *dev, DEV_BLOCK *block));
