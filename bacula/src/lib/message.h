/*
 * Define Message Types for Bacula
 *    Kern Sibbald, 2000
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

#include "bits.h"

#undef	M_DEBUG
#undef	M_ABORT
#undef	M_FATAL
#undef	M_ERROR
#undef	M_WARNING
#undef	M_INFO
#undef	M_MOUNT
#undef	M_TERM

#define M_DEBUG     1		      /* debug message */
#define M_ABORT     2		      /* MUST abort immediately */
#define M_FATAL     3		      /* Fatal error, stopping job */
#define M_ERROR     4		      /* Error, but recoverable */
#define M_WARNING   5		      /* Warning message */
#define M_INFO	    6		      /* Informational message */
#define M_SAVED     7		      /* Info on saved file */
#define M_NOTSAVED  8		      /* Info on notsaved file */
#define M_SKIPPED   9		      /* File skipped by option setting */
#define M_MOUNT    10		      /* Mount requests */
#define M_TERM	   11		      /* Termination request */

#define M_MAX	   M_TERM	      /* keep this updated ! */

/* Define message destination structure */
/* *** FIXME **** where should be extended to handle multiple values */
typedef struct s_dest {
   struct s_dest *next;
   int dest_code;		      /* destination (one of the MD_ codes) */
   int max_len; 		      /* max mail line length */
   FILE *fd;			      /* file descriptor */
   char msg_types[nbytes_for_bits(M_MAX+1)]; /* message type mask */
   char *where; 		      /* filename/program name */
   char *mail_cmd;		      /* mail command */
   POOLMEM *mail_filename;	      /* unique mail filename */
} DEST;

/* Message Destination values for dest field of DEST */
#define MD_SYSLOG    1		      /* send msg to syslog */
#define MD_MAIL      2		      /* email group of messages */
#define MD_FILE      3		      /* write messages to a file */
#define MD_APPEND    4		      /* append messages to a file */
#define MD_STDOUT    5		      /* print messages */
#define MD_STDERR    6		      /* print messages to stderr */
#define MD_DIRECTOR  7		      /* send message to the Director */
#define MD_OPERATOR  8		      /* email a single message to the operator */
#define MD_CONSOLE   9		      /* send msg to UserAgent or console */
#define MD_MAIL_ON_ERROR 10	      /* email messages if job errors */


void d_msg(char *file, int line, int level, char *fmt,...);
void e_msg(char *file, int line, int type, int level, char *fmt,...);
void Jmsg(void *vjcr, int type, int level, char *fmt,...);

extern int debug_level;
extern char my_name[];
extern char *working_directory;
extern time_t daemon_start_time;
