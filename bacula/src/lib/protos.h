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

struct JCR;

/* attr.c */
ATTR     *new_attr();
void      free_attr(ATTR *attr);
int       unpack_attributes_record(JCR *jcr, int32_t stream, char *rec, ATTR *attr);
void      build_attr_output_fnames(JCR *jcr, ATTR *attr);
void      print_ls_output(JCR *jcr, ATTR *attr);

/* base64.c */
void      base64_init            (void);
int       to_base64              (intmax_t value, char *where);
int       from_base64            (intmax_t *value, char *where);
int       bin_to_base64          (char *buf, char *bin, int len);

/* bsys.c */
char     *bstrncpy               (char *dest, const char *src, int maxlen);
char     *bstrncat               (char *dest, const char *src, int maxlen);
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
void      drop                   (char *uid, char *gid);
int       bmicrosleep            (time_t sec, long msec);
char     *bfgets                 (char *s, int size, FILE *fd);
#ifndef HAVE_STRTOLL
long long int strtoll            (const char *ptr, char **endptr, int base);
#endif

/* bnet.c */
int32_t    bnet_recv             (BSOCK *bsock);
int        bnet_send             (BSOCK *bsock);
int        bnet_fsend            (BSOCK *bs, char *fmt, ...);
int        bnet_set_buffer_size  (BSOCK *bs, uint32_t size, int rw);
int        bnet_sig              (BSOCK *bs, int sig);
int        bnet_ssl_server       (BSOCK *bsock, char *password, int ssl_need, int ssl_has);
int        bnet_ssl_client       (BSOCK *bsock, char *password, int ssl_need);
BSOCK *    bnet_connect            (JCR *jcr, int retry_interval,
               int max_retry_time, char *name, char *host, char *service, 
               int port, int verbose);
void       bnet_close            (BSOCK *bsock);
BSOCK *    init_bsock            (JCR *jcr, int sockfd, char *who, char *ip, int port);
BSOCK *    dup_bsock             (BSOCK *bsock);
void       term_bsock            (BSOCK *bsock);
char *     bnet_strerror         (BSOCK *bsock);
char *     bnet_sig_to_ascii     (BSOCK *bsock);
int        bnet_wait_data        (BSOCK *bsock, int sec);
int        bnet_wait_data_intr   (BSOCK *bsock, int sec);
int        bnet_despool          (BSOCK *bsock);
int        is_bnet_stop          (BSOCK *bsock);
int        is_bnet_error         (BSOCK *bsock);
void       bnet_suppress_error_messages(BSOCK *bsock, int flag);

/* bget_msg.c */
int      bget_msg(BSOCK *sock);

/* bpipe.c */
BPIPE *          open_bpipe(char *prog, int wait, char *mode);
int              close_wpipe(BPIPE *bpipe);
int              close_bpipe(BPIPE *bpipe);

/* cram-md5.c */
int cram_md5_get_auth(BSOCK *bs, char *password, int ssl_need);
int cram_md5_auth(BSOCK *bs, char *password, int ssl_need);
void hmac_md5(uint8_t* text, int text_len, uint8_t*  key,
              int key_len, uint8_t *hmac);

/* crc32.c */

uint32_t bcrc32(uint8_t *buf, int len);

/* daemon.c */
void     daemon_start            ();

/* edit.c */
uint64_t         str_to_uint64(char *str);
int64_t          str_to_int64(char *str);
char *           edit_uint64_with_commas   (uint64_t val, char *buf);
char *           add_commas              (char *val, char *buf);
char *           edit_uint64             (uint64_t val, char *buf);
int              duration_to_utime       (char *str, utime_t *value);
int              size_to_uint64(char *str, int str_len, uint64_t *rtn_value);
char             *edit_utime             (utime_t val, char *buf);
int              is_a_number             (const char *num);
int              is_an_integer           (const char *n);

/* lex.c */
LEX *     lex_close_file         (LEX *lf);
LEX *     lex_open_file          (LEX *lf, char *fname, LEX_ERROR_HANDLER *scan_error);
int       lex_get_char           (LEX *lf);
void      lex_unget_char         (LEX *lf);
char *    lex_tok_to_str         (int token);
int       lex_get_token          (LEX *lf, int expect);

/* message.c */
void       my_name_is            (int argc, char *argv[], char *name);
void       init_msg              (JCR *jcr, MSGS *msg);
void       term_msg              (void);
void       close_msg             (JCR *jcr);
void       add_msg_dest          (MSGS *msg, int dest, int type, char *where, char *dest_code);
void       rem_msg_dest          (MSGS *msg, int dest, int type, char *where);
void       Jmsg                  (JCR *jcr, int type, int level, char *fmt, ...);
void       dispatch_message      (JCR *jcr, int type, int level, char *buf);
void       init_console_msg      (char *wd);
void       free_msgs_res         (MSGS *msgs);
int        open_spool_file       (JCR *jcr, BSOCK *bs);
int        close_spool_file      (JCR *jcr, BSOCK *bs);


/* bnet_server.c */
void       bnet_thread_server(char *bind_addr, int port, int max_clients, workq_t *client_wq, 
                   void *handle_client_request(void *bsock));
void             bnet_server             (int port, void handle_client_request(BSOCK *bsock));
int              net_connect             (int port);
BSOCK *          bnet_bind               (int port);
BSOCK *          bnet_accept             (BSOCK *bsock, char *who);

/* signal.c */
void             init_signals             (void terminate(int sig));
void             init_stack_dump          (void);

/* scan.c */
void             strip_trailing_junk     (char *str);
void             strip_trailing_slashes  (char *dir);
int              skip_spaces             (char **msg);
int              skip_nonspaces          (char **msg);
int              fstrsch                 (char *a, char *b);
int              parse_args(POOLMEM *cmd, POOLMEM **args, int *argc, 
                        char **argk, char **argv, int max_args);
char            *next_arg(char **s);

/* util.c */
int              is_buf_zero             (char *buf, int len);
void             lcase                   (char *str);
void             bash_spaces             (char *str);
void             unbash_spaces           (char *str);
char *           encode_time             (time_t time, char *buf);
char *           encode_mode             (mode_t mode, char *buf);
int              do_shell_expansion      (char *name, int name_len);
void             jobstatus_to_ascii      (int JobStatus, char *msg, int maxlen);
void             pm_strcat               (POOLMEM **pm, char *str);
void             pm_strcpy               (POOLMEM **pm, char *str);
int              run_program             (char *prog, int wait, POOLMEM *results);
char *           job_type_to_str         (int type);
char *           job_status_to_str       (int stat);
char *           job_level_to_str        (int level);
void             make_session_key        (char *key, char *seed, int mode);
POOLMEM         *edit_job_codes(JCR *jcr, char *omsg, char *imsg, char *to);  
void             set_working_directory(char *wd);


/* watchdog.c */
int start_watchdog(void);
int stop_watchdog(void);
btimer_t *start_child_timer(pid_t pid, uint32_t wait);
void stop_child_timer(btimer_id wid);
btimer_id start_thread_timer(pthread_t tid, uint32_t wait);
void stop_thread_timer(btimer_id wid);
