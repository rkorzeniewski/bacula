/*
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

struct res_items;		    /* Declare forward referenced structure */ 
typedef void (MSG_RES_HANDLER)(LEX *lc, struct res_items *item, int index, int pass);

/* This is the structure that defines
 * the record types (items) permitted within each
 * resource. It is used to define the configuration
 * tables.
 */
struct res_items {
   char *name;			      /* Resource name i.e. Director, ... */
   MSG_RES_HANDLER *handler;	      /* Routine storing the resource item */
   void **value;		      /* Where to store the item */
   int	code;			      /* item code/additional info */
   int	flags;			      /* flags: default, required, ... */
   int	default_value;		      /* default value */
};

/* For storing name_addr items in res_items table */
#define ITEM(x) ((void **)&res_all.x)

#define MAX_RES_ITEMS 32	      /* maximum resource items per RES */

/* This is the universal header that is
 * at the beginning of every resource
 * record.
 */
struct s_reshdr {
   char *name;			      /* resource name */
   char *desc;			      /* resource description */
   int	 rcode; 		      /* resource id or type */
   int	 refcnt;		      /* reference count for releasing */
   char  item_present[MAX_RES_ITEMS]; /* set if item is present in conf file */
   struct s_reshdr *next;	      /* pointer to next resource of this type */
};

typedef struct s_reshdr RES;

/* 
 * Master Resource configuration structure definition
 * This is the structure that defines the
 * resources that are available to this daemon.
 */
struct s_res {	     
   char *name;			      /* resource name */
   struct res_items *items;	      /* list of resource keywords */
   int rcode;			      /* code if needed */
   RES *res_head;		      /* where to store it */
};

/* Common Resource definitions */

#define MAX_RES_NAME_LENGTH MAX_NAME_LENGTH-1	    /* maximum resource name length */

#define ITEM_REQUIRED 0x1	      /* item required */
#define ITEM_DEFAULT  0x2	      /* default supplied */

/* Message Resource */
struct s_res_msgs {
   RES	 hdr;
   char *mail_cmd;		      /* mail command */
   char *operator_cmd;		      /* Operator command */
   DEST *dest_chain;		      /* chain of destinations */
   char send_msg[nbytes_for_bits(M_MAX+1)];  /* bit array of types */
};
typedef struct s_res_msgs MSGS;


/* Define the Union of all the above common
 * resource structure definitions.
 */
union cu_res {
   struct s_res_msgs	res_msgs;
   RES hdr;
};

typedef union cu_res CURES;


/* Configuration routines */
void  parse_config	      __PROTO((char *cf));
void  free_config_resources   __PROTO(());

/* Resource routines */
RES *GetResWithName(int rcode, char *name);
RES *GetNextRes(int rcode, RES *res);
void LockRes(void);
void UnlockRes(void);
void dump_resource(int type, RES *res, void sendmsg(void *sock, char *fmt, ...), void *sock);
void free_resource(int type);
void init_resource(int type, struct res_items *item);
void save_resource(int type, struct res_items *item, int pass);


void scan_error(LEX *lc, char *msg, ...);  /* old way, do not use */
void scan_to_eol(LEX *lc);
char *res_to_str(int rcode);

void store_str(LEX *lc, struct res_items *item, int index, int pass);
void store_dir(LEX *lc, struct res_items *item, int index, int pass);
void store_password(LEX *lc, struct res_items *item, int index, int pass);
void store_name(LEX *lc, struct res_items *item, int index, int pass);
void store_strname(LEX *lc, struct res_items *item, int index, int pass);
void store_res(LEX *lc, struct res_items *item, int index, int pass);
void store_int(LEX *lc, struct res_items *item, int index, int pass);
void store_pint(LEX *lc, struct res_items *item, int index, int pass);
void store_msgs(LEX *lc, struct res_items *item, int index, int pass);
void store_int64(LEX *lc, struct res_items *item, int index, int pass);
void store_yesno(LEX *lc, struct res_items *item, int index, int pass);
void store_time(LEX *lc, struct res_items *item, int index, int pass);
void store_size(LEX *lc, struct res_items *item, int index, int pass);

/* Lexical scanning errors in parsing conf files */
#define scan_err0(lc, msg) s_err(__FILE__, __LINE__, lc, msg)
#define scan_err1(lc, msg, a1) s_err(__FILE__, __LINE__, lc, msg, a1)
#define scan_err2(lc, msg, a1, a2) s_err(__FILE__, __LINE__, lc, msg, a1, a2)
#define scan_err3(lc, msg, a1, a2, a3) s_err(__FILE__, __LINE__, lc, msg, a1, a2, a3)
#define scan_err4(lc, msg, a1, a2, a3, a4) s_err(__FILE__, __LINE__, lc, msg, a1, a2, a3, a4)
#define scan_err5(lc, msg, a1, a2, a3, a4, a5) s_err(__FILE__, __LINE__, lc, msg, a1, a2, a3, a4, a5)
#define scan_err6(lc, msg, a1, a2, a3, a4, a5, a6) s_err(__FILE__, __LINE__, lc, msg, a1, a2, a3, a4, a5, a6)

void s_err(char *file, int line, LEX *lc, char *msg,...);
