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
 * Bacula Sock Class definition
 *   Note, the old non-class code is in bnet.c, and the
 *   new class code associated with this file is in bsock.c
 *
 * Kern Sibbald, May MM
 *
 * Zero msglen from other end indicates soft eof (usually
 *   end of some binary data stream, but not end of conversation).
 *
 * Negative msglen, is special "signal" (no data follows).
 *   See below for SIGNAL codes.
 *
 */

#ifndef __BSOCK_H_
#define __BSOCK_H_

struct btimer_t;                      /* forward reference */
class BSOCK;
/* Effectively disable the bsock time out */
#define BSOCK_TIMEOUT  3600 * 24 * 200;  /* default 200 days */
btimer_t *start_bsock_timer(BSOCK *bs, uint32_t wait);
void stop_bsock_timer(btimer_t *wid);


class BSOCK {
/*
 * Note, keep this public part before the private otherwise
 *  bat breaks on some systems such as RedHat.
 */
public:
   uint64_t read_seqno;               /* read sequence number */
   POOLMEM *msg;                      /* message pool buffer */
   POOLMEM *errmsg;                   /* edited error message */
   RES *res;                          /* Resource to which we are connected */
   FILE *m_spool_fd;                  /* spooling file */
   TLS_CONNECTION *tls;               /* associated tls connection */
   IPADDR *src_addr;                  /* IP address to source connections from */
   uint32_t in_msg_no;                /* input message number */
   uint32_t out_msg_no;               /* output message number */
   int32_t msglen;                    /* message length */
   volatile time_t timer_start;       /* time started read/write */
   volatile time_t timeout;           /* timeout BSOCK after this interval */
   int m_fd;                          /* socket file descriptor */
   int b_errno;                       /* bsock errno */
   int m_blocking;                    /* blocking state (0 = nonblocking, 1 = blocking) */
   volatile int errors;               /* incremented for each error on socket */
   volatile bool m_suppress_error_msgs; /* set to suppress error messages */

   struct sockaddr client_addr;       /* client's IP address */
   struct sockaddr_in peer_addr;      /* peer's IP address */

private:
   BSOCK *m_next;                     /* next BSOCK if duped */
   JCR *m_jcr;                        /* jcr or NULL for error msgs */
   pthread_mutex_t m_mutex;           /* for locking if use_locking set */
   char *m_who;                       /* Name of daemon to which we are talking */
   char *m_host;                      /* Host name/IP */
   int m_port;                        /* desired port */
   btimer_t *m_tid;                   /* timer id */
   boffset_t m_data_end;              /* offset of data written */
   boffset_t m_last_data_end;          /* offset of last valid data written */
   int32_t m_FileIndex;               /* attr spool FI */
   int32_t m_lastFileIndex;           /* last valid attr spool FI */
   volatile bool m_timed_out: 1;      /* timed out in read/write */
   volatile bool m_terminated: 1;     /* set when BNET_TERMINATE arrives */
   bool m_closed: 1;                  /* set when socket is closed */
   bool m_duped: 1;                   /* set if duped BSOCK */
   bool m_spool: 1;                   /* set for spooling */
   bool m_use_locking: 1;             /* set to use locking */

   int64_t m_bwlimit;                 /* set to limit bandwidth */
   int64_t m_nb_bytes;                /* bytes sent/recv since the last tick */
   btime_t m_last_tick;               /* last tick used by bwlimit */

   void fin_init(JCR * jcr, int sockfd, const char *who, const char *host, int port,
               struct sockaddr *lclient_addr);
   bool open(JCR *jcr, const char *name, char *host, char *service,
               int port, utime_t heart_beat, int *fatal);

public:
   /* methods -- in bsock.c */
   void init();
   void free_tls();
   bool connect(JCR * jcr, int retry_interval, utime_t max_retry_time,
                utime_t heart_beat, const char *name, char *host,
                char *service, int port, int verbose);
   int32_t recv();
   bool send();
   bool fsend(const char*, ...);
   bool signal(int signal);
   void close();                      /* close connection and destroy packet */
   void destroy();                    /* destroy socket packet */
   const char *bstrerror();           /* last error on socket */
   int get_peer(char *buf, socklen_t buflen);
   bool despool(void update_attr_spool_size(ssize_t size), ssize_t tsize);
   bool set_buffer_size(uint32_t size, int rw);
   int set_nonblocking();
   int set_blocking();
   void restore_blocking(int flags);
   void set_killable(bool killable);
   int wait_data(int sec, int usec=0);
   int wait_data_intr(int sec, int usec=0);
   bool authenticate_director(const char *name, const char *password,
                  TLS_CONTEXT *tls_ctx, char *response, int response_len);
   bool set_locking();                /* in bsock.c */
   void clear_locking();              /* in bsock.c */
   void set_source_address(dlist *src_addr_list);
   void control_bwlimit(int bytes);   /* in bsock.c */

   /* Inline functions */
   void suppress_error_messages(bool flag) { m_suppress_error_msgs = flag; };
   void set_jcr(JCR *jcr) { m_jcr = jcr; };
   void set_who(char *who) { m_who = who; };
   void set_host(char *host) { m_host = host; };
   void set_port(int port) { m_port = port; };
   char *who() const { return m_who; };
   char *host() const { return m_host; };
   int port() const { return m_port; };
   JCR *jcr() const { return m_jcr; };
   JCR *get_jcr() const { return m_jcr; };
   bool is_spooling() const { return m_spool; };
   bool is_duped() const { return m_duped; };
   bool is_terminated() const { return m_terminated; };
   bool is_timed_out() const { return m_timed_out; };
   bool is_closed() const { return m_closed; };
   bool is_open() const { return !m_closed; };
   bool is_stop() const { return errors || is_terminated() || is_closed(); }
   bool is_error() { errno = b_errno; return errors; };
   void set_data_end(int32_t FileIndex) {
          if (m_spool && FileIndex > m_FileIndex) {
              m_lastFileIndex = m_FileIndex;
              m_last_data_end = m_data_end;
              m_FileIndex = FileIndex;
              m_data_end = ftello(m_spool_fd);
           }
        };
   boffset_t get_last_data_end() { return m_last_data_end; };
   int32_t get_lastFileIndex() { return m_lastFileIndex; };
   void set_bwlimit(int64_t maxspeed) { m_bwlimit = maxspeed; };
   bool use_bwlimit() { return m_bwlimit > 0;};
   void set_spooling() { m_spool = true; };
   void clear_spooling() { m_spool = false; };
   void set_duped() { m_duped = true; };
   void set_timed_out() { m_timed_out = true; };
   void clear_timed_out() { m_timed_out = false; };
   void set_terminated() { m_terminated = true; };
   void set_closed() { m_closed = true; };
   void start_timer(int sec) { m_tid = start_bsock_timer(this, sec); };
   void stop_timer() { stop_bsock_timer(m_tid); };
   void swap_msgs();
};

/*
 *  Signal definitions for use in bsock->signal()
 *  Note! These must be negative.  There are signals that are generated
 *   by the bsock software not by the OS ...
 */
enum {
   BNET_EOD            = -1,          /* End of data stream, new data may follow */
   BNET_EOD_POLL       = -2,          /* End of data and poll all in one */
   BNET_STATUS         = -3,          /* Send full status */
   BNET_TERMINATE      = -4,          /* Conversation terminated, doing close() */
   BNET_POLL           = -5,          /* Poll request, I'm hanging on a read */
   BNET_HEARTBEAT      = -6,          /* Heartbeat Response requested */
   BNET_HB_RESPONSE    = -7,          /* Only response permited to HB */
   BNET_xxxxxxPROMPT   = -8,          /* No longer used -- Prompt for subcommand */
   BNET_BTIME          = -9,          /* Send UTC btime */
   BNET_BREAK          = -10,         /* Stop current command -- ctl-c */
   BNET_START_SELECT   = -11,         /* Start of a selection list */
   BNET_END_SELECT     = -12,         /* End of a select list */
   BNET_INVALID_CMD    = -13,         /* Invalid command sent */
   BNET_CMD_FAILED     = -14,         /* Command failed */
   BNET_CMD_OK         = -15,         /* Command succeeded */
   BNET_CMD_BEGIN      = -16,         /* Start command execution */
   BNET_MSGS_PENDING   = -17,         /* Messages pending */
   BNET_MAIN_PROMPT    = -18,         /* Server ready and waiting */
   BNET_SELECT_INPUT   = -19,         /* Return selection input */
   BNET_WARNING_MSG    = -20,         /* Warning message */
   BNET_ERROR_MSG      = -21,         /* Error message -- command failed */
   BNET_INFO_MSG       = -22,         /* Info message -- status line */
   BNET_RUN_CMD        = -23,         /* Run command follows */
   BNET_YESNO          = -24,         /* Request yes no response */
   BNET_START_RTREE    = -25,         /* Start restore tree mode */
   BNET_END_RTREE      = -26,         /* End restore tree mode */
   BNET_SUB_PROMPT     = -27,         /* Indicate we are at a subprompt */
   BNET_TEXT_INPUT     = -28          /* Get text input from user */
};

#define BNET_SETBUF_READ  1           /* Arg for bnet_set_buffer_size */
#define BNET_SETBUF_WRITE 2           /* Arg for bnet_set_buffer_size */

/*
 * Return status from bnet_recv()
 * Note, the HARDEOF and ERROR refer to comm status/problems
 *  rather than the BNET_xxx above, which are software signals.
 */
enum {
   BNET_SIGNAL         = -1,
   BNET_HARDEOF        = -2,
   BNET_ERROR          = -3
};

/*
 * TLS enabling values. Value is important for comparison, ie:
 * if (tls_remote_need < BNET_TLS_REQUIRED) { ... }
 */
enum {
   BNET_TLS_NONE        = 0,          /* cannot do TLS */
   BNET_TLS_OK          = 1,          /* can do, but not required on my end */
   BNET_TLS_REQUIRED    = 2           /* TLS is required */
};

int32_t read_nbytes(BSOCK * bsock, char *ptr, int32_t nbytes);
int32_t write_nbytes(BSOCK * bsock, char *ptr, int32_t nbytes);

BSOCK *new_bsock();
/*
 * Completely release the socket packet, and NULL the pointer
 */
#define free_bsock(a) do{if(a){(a)->close(); (a)->destroy(); (a)=NULL;}} while(0)

/*
 * Does the socket exist and is it open?
 */
#define is_bsock_open(a) ((a) && (a)->is_open())

#endif /* __BSOCK_H_ */
