/*
 * Protypes for stored
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
   
/* From stored.c */
uint32_t new_VolSessionId();

/* From askdir.c */
int	dir_get_volume_info(JCR *jcr);
int	dir_find_next_appendable_volume(JCR *jcr);
int	dir_update_volume_info(JCR *jcr, VOLUME_CAT_INFO *vol, int relabel);
int	dir_ask_sysop_to_mount_next_volume(JCR *jcr, DEVICE *dev);
int	dir_ask_sysop_to_mount_volume(JCR *jcr, DEVICE *dev);
int	dir_update_file_attributes(JCR *jcr, DEV_RECORD *rec);
int	dir_send_job_status(JCR *jcr);

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
int	write_block_to_dev(DEVICE *dev, DEV_BLOCK *block);
int	read_block_from_device(DEVICE *dev, DEV_BLOCK *block);
int	read_block_from_dev(DEVICE *dev, DEV_BLOCK *block);


/* From dev.c */
DEVICE	*init_dev(DEVICE *dev, char *device);
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

/* Get info about device */
char *	 dev_name(DEVICE *dev);
char *	 dev_vol_name(DEVICE *dev);
uint32_t dev_block(DEVICE *dev);
uint32_t dev_file(DEVICE *dev);
int	 dev_is_tape(DEVICE *dev);

/* From device.c */
int	 open_device(DEVICE *dev);
int	 acquire_device_for_append(JCR *jcr, DEVICE *dev, DEV_BLOCK *block);
int	 acquire_device_for_read(JCR *jcr, DEVICE *dev, DEV_BLOCK *block);
int	 ready_dev_for_read(JCR *jcr, DEVICE *dev, DEV_BLOCK *block);
int	 release_device(JCR *jcr, DEVICE *dev, DEV_BLOCK *block);
void	 block_device(DEVICE *dev, int state);
void	 unblock_device(DEVICE *dev);
void	 lock_device(DEVICE *dev);
void	 unlock_device(DEVICE *dev);
int	 fixup_device_block_write_error(JCR *jcr, DEVICE *dev, DEV_BLOCK *block);

/* From dircmd.c */
void	 connection_request(void *arg); 


/* From fd_cmds.c */
void	 run_job(JCR *jcr);

/* From fdmsg.c */
int	 bget_msg(BSOCK *sock);

/* From job.c */
void	 stored_free_jcr(JCR *jcr);
void	 connection_from_filed(void *arg);     
void	 handle_filed_connection(BSOCK *fd, char *job_name);

/* From label.c */
int	 read_dev_volume_label(JCR *jcr, DEVICE *dev, DEV_BLOCK *block);
void	 create_session_label(JCR *jcr, DEV_RECORD *rec, int label);
int	 write_volume_label_to_dev(JCR *jcr, DEVRES *device, char *VolName, char *PoolName);
int	 write_session_label(JCR *jcr, DEV_BLOCK *block, int label);
int	 write_volume_label_to_block(JCR *jcr, DEVICE *dev, DEV_BLOCK *block);
void	 dump_volume_label(DEVICE *dev);
void	 dump_label_record(DEVICE *dev, DEV_RECORD *rec, int verbose);
int	 unser_volume_label(DEVICE *dev, DEV_RECORD *rec);
int	 unser_session_label(SESSION_LABEL *label, DEV_RECORD *rec);

/* From record.c */
char   *FI_to_ascii(int fi);
char   *stream_to_ascii(int stream);
int	write_record_to_block(DEV_BLOCK *block, DEV_RECORD *rec);
int	read_record_from_block(DEV_BLOCK *block, DEV_RECORD *rec); 
DEV_RECORD *new_record();
void	free_record(DEV_RECORD *rec);
int	read_record(DEVICE *dev, DEV_BLOCK *block, DEV_RECORD *record);
int	write_record_device(JCR *jcr, DEVICE *dev, DEV_BLOCK *block, DEV_RECORD *record);
