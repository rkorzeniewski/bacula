/*
 * Includes specific to the Director User Agent Server
 *
 *     Kern Sibbald, August MMI
 *
 *     Version $Id$
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

#define MAX_ARGS 30

typedef struct s_ua_context {
   BSOCK *UA_sock;
   BSOCK *sd;
   JCR *jcr;
   B_DB *db;
   CAT *catalog;
   POOLMEM *cmd;                      /* return command/name buffer */
   POOLMEM *args;                     /* command line arguments */
   char *argk[MAX_ARGS];              /* argument keywords */
   char *argv[MAX_ARGS];              /* argument values */
   int argc;                          /* number of arguments */
   char **prompt;                     /* list of prompts */
   int max_prompts;                   /* max size of list */
   int num_prompts;                   /* current number in list */
   int auto_display_messages;         /* if set, display messages */
   int user_notified_msg_pending;     /* set when user notified */
   int automount;                     /* if set, mount after label */
   int quit;                          /* if set, quit */
   int verbose;                       /* set for normal UA verbosity */
} UAContext;

/* ua_cmds.c */
int do_a_command(UAContext *ua, char *cmd);
int do_a_dot_command(UAContext *ua, char *cmd);
int qmessagescmd(UAContext *ua, char *cmd);
int open_db(UAContext *ua);
void close_db(UAContext *ua);

/* ua_input.c */
char *next_arg(char **s);
int get_cmd(UAContext *ua, char *prompt);
void parse_command_args(UAContext *ua);

/* ua_output.c */
void prtit(void *ctx, char *msg);

/* ua_server.c */
void bsendmsg(void *sock, char *fmt, ...);

/* ua_select.c */
STORE   *select_storage_resource(UAContext *ua);
JOB     *select_job_resource(UAContext *ua);
JOB     *select_restore_job_resource(UAContext *ua);
CLIENT  *select_client_resource(UAContext *ua);
FILESET *select_fileset_resource(UAContext *ua);
int     select_pool_and_media_dbr(UAContext *ua, POOL_DBR *pr, MEDIA_DBR *mr);
int     select_pool_dbr(UAContext *ua, POOL_DBR *pr);

void    start_prompt(UAContext *ua, char *msg);
void    add_prompt(UAContext *ua, char *prompt);
int     do_prompt(UAContext *ua, char *msg, char *prompt, int max_prompt);
CAT    *get_catalog_resource(UAContext *ua);           
STORE  *get_storage_resource(UAContext *ua, char *cmd);
int     get_media_type(UAContext *ua, char *MediaType, int max_media);
int     get_pool_dbr(UAContext *ua, POOL_DBR *pr);
POOL   *get_pool_resource(UAContext *ua);
CLIENT *get_client_resource(UAContext *ua);
int     get_job_dbr(UAContext *ua, JOB_DBR *jr);

int find_arg_keyword(UAContext *ua, char **list);
int do_keyword_prompt(UAContext *ua, char *msg, char **list);
int confirm_retention(UAContext *ua, utime_t *ret, char *msg);

/* ua_prune.c */
int prune_files(UAContext *ua, CLIENT *client);
int prune_jobs(UAContext *ua, CLIENT *client, int JobType);
int prune_volume(UAContext *ua, POOL_DBR *pr, MEDIA_DBR *mr);
