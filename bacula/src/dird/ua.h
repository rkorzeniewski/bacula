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

#ifndef __UA_H_
#define __UA_H_ 1

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

#endif
