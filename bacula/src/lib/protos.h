/*
 * Prototypes for lib directory of Bacula
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

/* base64.c */
void      base64_init            (void);
int       to_base64              (intmax_t value, char *where);
int       from_base64            (intmax_t *value, char *where);
int       bin_to_base64          (char *buf, char *bin, int len);

/* bmisc.c */
char     *bstrncpy               (char *dest, char *src, int maxlen);
char     *bstrncat               (char *dest, char *src, int maxlen);
void     *b_malloc               (char *file, int line, size_t size);
#ifndef DEBUG
void     *bmalloc                (size_t size);
#endif
void     *brealloc               (void *buf, size_t size);
void     *bcalloc                (size_t size1, size_t size2);
int       bsnprintf              (char *str, size_t size, const  char  *format, ...);
int       bvsnprintf             (char *str, size_t size, const char  *format, va_list ap);
int       pool_sprintf           (char *pool_buf, char *fmt, ...);
void      create_pid_file        (char *dir, char *progname, int port);
int       delete_pid_file        (char *dir, char *progname, int port);


/* bnet.c */
int32_t    bnet_recv             (BSOCK *bsock);
int        bnet_send             (BSOCK *bsock);
int        bnet_fsend              (BSOCK *bs, char *fmt, ...);
int        bnet_set_buffer_size    (BSOCK *bs, uint32_t size, int rw);
int        bnet_sig                (BSOCK *bs, int sig);
BSOCK *    bnet_connect            (void *jcr, int retry_interval,
               int max_retry_time, char *name, char *host, char *service, 
               int port, int verbose);
int        bnet_wait_data         (BSOCK *bsock, int sec);
void       bnet_close            (BSOCK *bsock);
BSOCK *    init_bsock            (void *jcr, int sockfd, char *who, char *ip, int port);
BSOCK *    dup_bsock             (BSOCK *bsock);
void       term_bsock            (BSOCK *bsock);
char *     bnet_strerror         (BSOCK *bsock);
char *     bnet_sig_to_ascii     (BSOCK *bsock);
int        bnet_wait_data        (BSOCK *bsock, int sec);
int        bnet_despool          (BSOCK *bsock);
int        is_bnet_stop          (BSOCK *bsock);
int        is_bnet_error         (BSOCK *bsock);


/* cram-md5.c */
int cram_md5_get_auth(BSOCK *bs, char *password);
int cram_md5_auth(BSOCK *bs, char *password);
void hmac_md5(uint8_t* text, int text_len, uint8_t*  key,
              int key_len, uint8_t *hmac);

/* crc32.c */
uint32_t bcrc32(uint8_t *buf, int len);

/* daemon.c */
void     daemon_start            ();

/* lex.c */
LEX *     lex_close_file         (LEX *lf);
LEX *     lex_open_file          (LEX *lf, char *fname, LEX_ERROR_HANDLER *scan_error);
int       lex_get_char           (LEX *lf);
void      lex_unget_char         (LEX *lf);
char *    lex_tok_to_str         (int token);
int       lex_get_token          (LEX *lf, int expect);

/* message.c */
void       my_name_is            (int argc, char *argv[], char *name);
void       init_msg              (void *jcr, MSGS *msg);
void       term_msg              (void);
void       close_msg             (void *jcr);
void       add_msg_dest          (MSGS *msg, int dest, int type, char *where, char *dest_code);
void       rem_msg_dest          (MSGS *msg, int dest, int type, char *where);
void       Jmsg                  (void *jcr, int type, int level, char *fmt, ...);
void       dispatch_message      (void *jcr, int type, int level, char *buf);
void       init_console_msg      (char *wd);
void       free_msgs_res         (MSGS *msgs);
int        open_spool_file       (void *jcr, BSOCK *bs);
int        close_spool_file      (void *vjcr, BSOCK *bs);


/* bnet_server.c */
void       bnet_thread_server(char *bind_addr, int port, int max_clients, workq_t *client_wq, 
                   void handle_client_request(void *bsock));
void             bnet_server             (int port, void handle_client_request(BSOCK *bsock));
int              net_connect             (int port);
BSOCK *          bnet_bind               (int port);
BSOCK *          bnet_accept             (BSOCK *bsock, char *who);

/* signal.c */
void             init_signals             (void terminate(int sig));
void             init_stack_dump          (void);

/* util.c */
void             lcase                   (char *str);
void             bash_spaces             (char *str);
void             unbash_spaces           (char *str);
void             strip_trailing_junk     (char *str);
void             strip_trailing_slashes  (char *dir);
int              skip_spaces             (char **msg);
int              skip_nonspaces          (char **msg);
int              fstrsch                 (char *a, char *b);
char *           encode_time             (time_t time, char *buf);
char *           encode_mode             (mode_t mode, char *buf);
char *           edit_uint64_with_commas   (uint64_t val, char *buf);
char *           add_commas              (char *val, char *buf);
char *           edit_uint64             (uint64_t val, char *buf);
int              do_shell_expansion      (char *name);
int              is_a_number             (const char *num);
int              is_buf_zero             (char *buf, int len);
int              duration_to_utime       (char *str, utime_t *value);
char             *edit_utime             (utime_t val, char *buf);
void             jobstatus_to_ascii      (int JobStatus, char *msg, int maxlen);
void             pm_strcat               (POOLMEM **pm, char *str);
void             pm_strcpy               (POOLMEM **pm, char *str);
int              run_program             (char *prog, int wait, POOLMEM *results);
char *           job_type_to_str         (int type);
char *           job_status_to_str       (int stat);
char *           job_level_to_str        (int level);
void             makeSessionKey          (char *key, char *seed, int mode);



/* watchdog.c */
int start_watchdog(void);
int stop_watchdog(void);
