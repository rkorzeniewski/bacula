/*
 *   Master Configuration routines.
 *  
 *   This file contains the common parts of the Bacula
 *   configuration routines.
 *
 *   Note, the configuration file parser consists of three parts
 *
 *   1. The generic lexical scanner in lib/lex.c and lib/lex.h
 *
 *   2. The generic config  scanner in lib/parse_conf.c and 
 *	lib/parse_conf.h.
 *	These files contain the parser code, some utility
 *	routines, and the common store routines (name, int,
 *	string).
 *
 *   3. The daemon specific file, which contains the Resource
 *	definitions as well as any specific store routines
 *	for the resource records.
 *
 *    N.B. This is a two pass parser, so if you malloc() a string
 *         in a "store" routine, you must ensure to do it during
 *	   only one of the two passes, or to free it between.
 *	   Also, note that the resource record is malloced and
 *	   saved in save_resource() during pass 1.  Anything that
 *	   you want saved after pass two (e.g. resource pointers)
 *	   must explicitly be done in save_resource. Take a look
 *	   at the Job resource in src/dird/dird_conf.c to see how
 *	   it is done.
 *
 *     Kern Sibbald, January MM
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


#include "bacula.h"

extern int debug_level;

/* Each daemon has a slightly different set of 
 * resources, so it will define the following
 * global values.
 */
extern int r_first;
extern int r_last;
extern pthread_mutex_t res_mutex;
extern struct s_res resources[];
extern CURES res_all;
extern int res_all_size;

static int res_locked = 0;	       /* set when resource chains locked */

/* Forward referenced subroutines */
static void scan_types(LEX *lc, MSGS *msg, int dest, char *where, char *cmd);


/* Common Resource definitions */

/* Message resource directives
 *  name	 handler      value	  code	 flags	default_value
 */
struct res_items msgs_items[] = {
   {"name",        store_name,    ITEM(res_msgs.hdr.name),  0, 0, 0},
   {"description", store_str,     ITEM(res_msgs.hdr.desc),  0, 0, 0},
   {"mailcommand", store_str,     ITEM(res_msgs.mail_cmd),  0, 0, 0},
   {"operatorcommand", store_str, ITEM(res_msgs.operator_cmd), 0, 0, 0},
   {"syslog",      store_msgs, ITEM(res_msgs), MD_SYSLOG,   0, 0}, 
   {"mail",        store_msgs, ITEM(res_msgs), MD_MAIL,     0, 0},
   {"mailonerror", store_msgs, ITEM(res_msgs), MD_MAIL_ON_ERROR, 0, 0},
   {"file",        store_msgs, ITEM(res_msgs), MD_FILE,     0, 0},
   {"append",      store_msgs, ITEM(res_msgs), MD_APPEND,   0, 0},
   {"stdout",      store_msgs, ITEM(res_msgs), MD_STDOUT,   0, 0},
   {"stderr",      store_msgs, ITEM(res_msgs), MD_STDERR,   0, 0},
   {"director",    store_msgs, ITEM(res_msgs), MD_DIRECTOR, 0, 0},
   {"console",     store_msgs, ITEM(res_msgs), MD_CONSOLE,  0, 0},   
   {"operator",    store_msgs, ITEM(res_msgs), MD_OPERATOR, 0, 0},
   {NULL, NULL,    NULL, 0}
};

struct s_mtypes {	
   char *name;
   int token;	
};
/* Various message types */
static struct s_mtypes msg_types[] = {
   {"debug",         M_DEBUG},
   {"abort",         M_ABORT},
   {"fatal",         M_FATAL},
   {"error",         M_ERROR},
   {"warning",       M_WARNING},
   {"info",          M_INFO},
   {"saved",         M_SAVED},
   {"notsaved",      M_NOTSAVED},
   {"skipped",       M_SKIPPED},
   {"mount",         M_MOUNT},
   {"terminate",     M_TERM},
   {"restored",      M_RESTORED},
   {"all",           M_MAX+1},
   {NULL,	     0}
};


/* Simply print a message */
static void prtmsg(void *sock, char *fmt, ...)
{
   va_list arg_ptr;

   va_start(arg_ptr, fmt);
   vfprintf(stdout, fmt, arg_ptr);
   va_end(arg_ptr);
}

char *res_to_str(int rcode)
{
   if (rcode < r_first || rcode > r_last) {
      return "***UNKNOWN***";
   } else {
      return resources[rcode-r_first].name;
   }
}


/* 
 * Initialize the static structure to zeros, then
 *  apply all the default values.
 */
void init_resource(int type, struct res_items *items)
{
   int i;
   int rindex = type - r_first;

   memset(&res_all, 0, res_all_size);
   res_all.hdr.rcode = type;
   res_all.hdr.refcnt = 1;

   for (i=0; items[i].name; i++) {
      Dmsg3(300, "Item=%s def=%s defval=%d\n", items[i].name,
            (items[i].flags & ITEM_DEFAULT) ? "yes" : "no",      
	    items[i].default_value);
      if (items[i].flags & ITEM_DEFAULT && items[i].default_value != 0) {
	 if (items[i].handler == store_yesno) {
	    *(int *)(items[i].value) |= items[i].code;
	 } else if (items[i].handler == store_pint || 
		    items[i].handler == store_int) {
	    *(int *)(items[i].value) = items[i].default_value;
	 } else if (items[i].handler == store_int64) {
	    *(int64_t *)(items[i].value) = items[i].default_value;
	 } else if (items[i].handler == store_size) {
	    *(uint64_t *)(items[i].value) = (uint64_t)items[i].default_value;
	 } else if (items[i].handler == store_time) {
	    *(utime_t *)(items[i].value) = (utime_t)items[i].default_value;
	 }
      }
      /* If this triggers, take a look at lib/parse_conf.h */
      if (i >= MAX_RES_ITEMS) {
         Emsg1(M_ERROR_TERM, 0, _("Too many items in %s resource\n"), resources[rindex]);
      }
   }
}


/* Store Messages Destination information */
void store_msgs(LEX *lc, struct res_items *item, int index, int pass)
{
   int token;
   char *cmd;
   POOLMEM *dest;
   int dest_len;    

   Dmsg2(200, "store_msgs pass=%d code=%d\n", pass, item->code);
   if (pass == 1) {
      switch (item->code) {
	 case MD_STDOUT:
	 case MD_STDERR:
	 case MD_SYSLOG:	      /* syslog */
	 case MD_CONSOLE:
	    scan_types(lc, (MSGS *)(item->value), item->code, NULL, NULL);
	    break;
	 case MD_OPERATOR:	      /* send to operator */
	 case MD_DIRECTOR:	      /* send to Director */
	 case MD_MAIL:		      /* mail */
	 case MD_MAIL_ON_ERROR:       /* mail if Job errors */
	    if (item->code == MD_OPERATOR) {
	       cmd = res_all.res_msgs.operator_cmd;
	    } else {
	       cmd = res_all.res_msgs.mail_cmd;
	    }
	    dest = get_pool_memory(PM_MESSAGE);
	    dest[0] = 0;
	    dest_len = 0;
	    /* Pick up comma separated list of destinations */
	    for ( ;; ) {
	       token = lex_get_token(lc, T_NAME);   /* scan destination */
	       dest = check_pool_memory_size(dest, dest_len + lc->str_len + 2);
	       if (dest[0] != 0) {
                  strcat(dest, " ");  /* separate multiple destinations with space */
		  dest_len++;
	       }
	       strcat(dest, lc->str);
	       dest_len += lc->str_len;
               Dmsg2(100, "store_msgs newdest=%s: dest=%s:\n", lc->str, NPRT(dest));
	       token = lex_get_token(lc, T_ALL);
	       if (token == T_COMMA) { 
		  continue;	      /* get another destination */
	       }
	       if (token != T_EQUALS) {
                  scan_err1(lc, "expected an =, got: %s", lc->str); 
	       }
	       break;
	    }
            Dmsg1(200, "mail_cmd=%s\n", NPRT(cmd));
	    scan_types(lc, (MSGS *)(item->value), item->code, dest, cmd);
	    free_pool_memory(dest);
            Dmsg0(200, "done with dest codes\n");
	    break;
	 case MD_FILE:		      /* file */
	 case MD_APPEND:	      /* append */
	    dest = get_pool_memory(PM_MESSAGE);
	    /* Pick up a single destination */
	    token = lex_get_token(lc, T_NAME);	 /* scan destination */
	    pm_strcpy(&dest, lc->str);
	    dest_len = lc->str_len;
	    token = lex_get_token(lc, T_ALL);
            Dmsg1(200, "store_msgs dest=%s:\n", NPRT(dest));
	    if (token != T_EQUALS) {
               scan_err1(lc, "expected an =, got: %s", lc->str); 
	    }
	    scan_types(lc, (MSGS *)(item->value), item->code, dest, NULL);
	    free_pool_memory(dest);
            Dmsg0(200, "done with dest codes\n");
	    break;

	 default:
            scan_err1(lc, "Unknown item code: %d\n", item->code);
	    break;
      }
   }
   scan_to_eol(lc);
   set_bit(index, res_all.hdr.item_present);
   Dmsg0(200, "Done store_msgs\n");
}

/* 
 * Scan for message types and add them to the message
 * destination. The basic job here is to connect message types
 *  (WARNING, ERROR, FATAL, INFO, ...) with an appropriate
 *  destination (MAIL, FILE, OPERATOR, ...)
 */
static void scan_types(LEX *lc, MSGS *msg, int dest_code, char *where, char *cmd)
{
   int i, found, quit, is_not;
   int msg_type = 0;
   char *str;

   for (quit=0; !quit;) {
      lex_get_token(lc, T_NAME);	    /* expect at least one type */	 
      found = FALSE;
      if (lc->str[0] == '!') {
	 is_not = TRUE;
	 str = &lc->str[1];
      } else {
	 is_not = FALSE;
	 str = &lc->str[0];
      }
      for (i=0; msg_types[i].name; i++) {
	 if (strcmp(str, msg_types[i].name) == 0) {
	    msg_type = msg_types[i].token;
	    found = TRUE;
	    break;
	 }
      }
      if (!found) {
         scan_err1(lc, "message type: %s not found", str);
	 /* NOT REACHED */
      }

      if (msg_type == M_MAX+1) {	 /* all? */
	 for (i=1; i<=M_MAX; i++) {	 /* yes set all types */
	    add_msg_dest(msg, dest_code, i, where, cmd);
	 }
      } else {
	 if (is_not) {
	    rem_msg_dest(msg, dest_code, msg_type, where);
	 } else {
	    add_msg_dest(msg, dest_code, msg_type, where, cmd);
	 }
      }
      if (lc->ch != ',') {
	 break;
      }
      Dmsg0(200, "call lex_get_token() to eat comma\n");
      lex_get_token(lc, T_ALL); 	 /* eat comma */
   }
   Dmsg0(200, "Done scan_types()\n");
}


/* 
 * This routine is ONLY for resource names
 *  Store a name at specified address.
 */
void store_name(LEX *lc, struct res_items *item, int index, int pass)
{
   lex_get_token(lc, T_NAME);
   /* Store the name both pass 1 and pass 2 */
   *(item->value) = bstrdup(lc->str);
   scan_to_eol(lc);
   set_bit(index, res_all.hdr.item_present);
}


/*
 * Store a name string at specified address
 * A name string is limited to MAX_RES_NAME_LENGTH
 */
void store_strname(LEX *lc, struct res_items *item, int index, int pass)
{
   lex_get_token(lc, T_NAME);
   /* Store the name */
   if (pass == 1) {
      *(item->value) = bstrdup(lc->str);
   }
   scan_to_eol(lc);
   set_bit(index, res_all.hdr.item_present);
}



/* Store a string at specified address */
void store_str(LEX *lc, struct res_items *item, int index, int pass)
{
   lex_get_token(lc, T_STRING);
   if (pass == 1) {
      *(item->value) = bstrdup(lc->str);
   }
   scan_to_eol(lc);
   set_bit(index, res_all.hdr.item_present);
}

/*
 * Store a directory name at specified address. Note, we do
 *   shell expansion except if the string begins with a vertical
 *   bar (i.e. it will likely be passed to the shell later).
 */
void store_dir(LEX *lc, struct res_items *item, int index, int pass)
{
   lex_get_token(lc, T_STRING);
   if (pass == 1) {
      if (lc->str[0] != '|') {
	 do_shell_expansion(lc->str, sizeof(lc->str));
      }
      *(item->value) = bstrdup(lc->str);
   }
   scan_to_eol(lc);
   set_bit(index, res_all.hdr.item_present);
}


/* Store a password specified address in MD5 coding */
void store_password(LEX *lc, struct res_items *item, int index, int pass)
{
   unsigned int i, j;
   struct MD5Context md5c;
   unsigned char signature[16];
   char sig[100];


   lex_get_token(lc, T_STRING);
   if (pass == 1) {
      MD5Init(&md5c);
      MD5Update(&md5c, (unsigned char *) (lc->str), lc->str_len);
      MD5Final(signature, &md5c);
      for (i = j = 0; i < sizeof(signature); i++) {
         sprintf(&sig[j], "%02x", signature[i]); 
	 j += 2;
      }
      *(item->value) = bstrdup(sig);
   }
   scan_to_eol(lc);
   set_bit(index, res_all.hdr.item_present);
}


/* Store a resource at specified address.
 * If we are in pass 2, do a lookup of the 
 * resource.
 */
void store_res(LEX *lc, struct res_items *item, int index, int pass)
{
   RES *res;

   lex_get_token(lc, T_NAME);
   if (pass == 2) {
     res = GetResWithName(item->code, lc->str);
     if (res == NULL) {
        scan_err3(lc, "Could not find Resource %s referenced on line %d : %s\n", 
	   lc->str, lc->line_no, lc->line);
     }
     *(item->value) = (char *)res;
   }
   scan_to_eol(lc);
   set_bit(index, res_all.hdr.item_present);
}


/* Store an integer at specified address */
void store_int(LEX *lc, struct res_items *item, int index, int pass)
{
   lex_get_token(lc, T_INT32);
   *(int *)(item->value) = lc->int32_val;
   scan_to_eol(lc);
   set_bit(index, res_all.hdr.item_present);
}

/* Store a positive integer at specified address */
void store_pint(LEX *lc, struct res_items *item, int index, int pass)
{
   lex_get_token(lc, T_PINT32);
   *(int *)(item->value) = lc->pint32_val;
   scan_to_eol(lc);
   set_bit(index, res_all.hdr.item_present);
}


/* Store an 64 bit integer at specified address */
void store_int64(LEX *lc, struct res_items *item, int index, int pass)
{
   lex_get_token(lc, T_INT64);
   *(int64_t *)(item->value) = lc->int64_val;
   scan_to_eol(lc);
   set_bit(index, res_all.hdr.item_present);
}

/* Store a size in bytes */
void store_size(LEX *lc, struct res_items *item, int index, int pass)
{
   int token;
   double dvalue;
   uint64_t uvalue;

   Dmsg0(400, "Enter store_size\n");
   token = lex_get_token(lc, T_ALL);
   errno = 0;
   switch (token) {
   case T_NUMBER:
      Dmsg2(400, "size num=:%s: %f\n", lc->str, strtod(lc->str, NULL)); 
      dvalue = strtod(lc->str, NULL);
      if (errno != 0 || token < 0) {
         scan_err1(lc, "expected a size number, got: %s", lc->str);
      }
      *(uint64_t *)(item->value) = (uint64_t)dvalue;
      break;
   case T_IDENTIFIER:
   case T_UNQUOTED_STRING:
      if (!size_to_uint64(lc->str, lc->str_len, &uvalue)) {
         scan_err1(lc, "expected a size number, got: %s", lc->str);
      }
      *(uint64_t *)(item->value) = uvalue;
      break;
   default:
      scan_err1(lc, "expected a size, got: %s", lc->str);
      break;
   }
   scan_to_eol(lc);
   set_bit(index, res_all.hdr.item_present);
   Dmsg0(400, "Leave store_size\n");
}


/* Store a time period in seconds */
void store_time(LEX *lc, struct res_items *item, int index, int pass)
{
   int token; 
   double value;
   utime_t utime;

   token = lex_get_token(lc, T_ALL);
   errno = 0;
   switch (token) {
   case T_NUMBER:
      value = strtod(lc->str, NULL);
      if (errno != 0 || value < 0) {
         scan_err1(lc, "expected a time period, got: %s", lc->str);
      }
      *(utime_t *)(item->value) = (utime_t)value;
      break;
   case T_IDENTIFIER:
   case T_UNQUOTED_STRING:
      if (!duration_to_utime(lc->str, &utime)) {
         scan_err1(lc, "expected a time period, got: %s", lc->str);
      }
      *(utime_t *)(item->value) = utime;
      break;
   default:
      scan_err1(lc, "expected a time period, got: %s", lc->str);
      break;
   }
   scan_to_eol(lc);
   set_bit(index, res_all.hdr.item_present);
}


/* Store a yes/no in a bit field */
void store_yesno(LEX *lc, struct res_items *item, int index, int pass)
{
   lex_get_token(lc, T_NAME);
   if (strcasecmp(lc->str, "yes") == 0) {
      *(int *)(item->value) |= item->code;
   } else if (strcasecmp(lc->str, "no") == 0) {
      *(int *)(item->value) &= ~(item->code);
   } else {
      scan_err1(lc, "Expect a YES or NO, got: %s", lc->str);
   }
   scan_to_eol(lc);
   set_bit(index, res_all.hdr.item_present);
}



void LockRes()
{
   P(res_mutex);
   res_locked = 1;
}

void UnlockRes()
{
   res_locked = 0;
   V(res_mutex);
}

/*
 * Return resource of type rcode that matches name
 */
RES *
GetResWithName(int rcode, char *name)
{
   RES *res;
   int rindex = rcode - r_first;

   LockRes();
   res = resources[rindex].res_head;
   while (res) {
      if (strcmp(res->name, name) == 0) {
	 break;
      }
      res = res->next;
   }
   UnlockRes();
   return res;
   
}

/*
 * Return next resource of type rcode. On first
 * call second arg (res) is NULL, on subsequent
 * calls, it is called with previous value.
 */
RES *
GetNextRes(int rcode, RES *res)
{
   RES *nres;
   int rindex = rcode - r_first;
       

   if (!res_locked) {
      Emsg0(M_ABORT, 0, "Resource chain not locked.\n");
   }
   if (res == NULL) {
      nres = resources[rindex].res_head;
   } else {
      nres = res->next;
   }
   return nres;
}


/* Parser state */
enum parse_state {
   p_none,
   p_resource
};

/*********************************************************************
 *
 *	Parse configuration file
 *
 */
void 
parse_config(char *cf)
{
   LEX *lc = NULL;
   int token, i, pass;
   int res_type = 0;
   enum parse_state state = p_none;
   struct res_items *items = NULL;
   int level = 0;

   /* Make two passes. The first builds the name symbol table,
    * and the second picks up the items. 
    */
   Dmsg0(200, "Enter parse_config()\n");
   for (pass=1; pass <= 2; pass++) {
      Dmsg1(200, "parse_config pass %d\n", pass);
      lc = lex_open_file(lc, cf, NULL);
      while ((token=lex_get_token(lc, T_ALL)) != T_EOF) {
         Dmsg1(150, "parse got token=%s\n", lex_tok_to_str(token));
	 switch (state) {
	    case p_none:
	       if (token == T_EOL) {
		  break;
	       }
	       if (token != T_IDENTIFIER) {
                  scan_err1(lc, "Expected a Resource name identifier, got: %s", lc->str);
		  /* NOT REACHED */
	       }
	       for (i=0; resources[i].name; i++)
		  if (strcasecmp(resources[i].name, lc->str) == 0) {
		     state = p_resource;
		     items = resources[i].items;
		     res_type = resources[i].rcode;
		     init_resource(res_type, items);
		     break;
		  }
	       if (state == p_none) {
                  scan_err1(lc, "expected resource name, got: %s", lc->str);
		  /* NOT REACHED */
	       }
	       break;
	    case p_resource:
	       switch (token) {
		  case T_BOB:
		     level++;
		     break;
		  case T_IDENTIFIER:
		     if (level != 1) {
                        scan_err1(lc, "not in resource definition: %s", lc->str);
			/* NOT REACHED */
		     }
		     for (i=0; items[i].name; i++) {
			if (strcasecmp(items[i].name, lc->str) == 0) {
			   /* If the ITEM_NO_EQUALS flag is set we do NOT	       
			    *	scan for = after the keyword  */
			   if (!(items[i].flags & ITEM_NO_EQUALS)) {
			      token = lex_get_token(lc, T_ALL);
                              Dmsg1 (150, "in T_IDENT got token=%s\n", lex_tok_to_str(token));
			      if (token != T_EQUALS) {
                                 scan_err1(lc, "expected an equals, got: %s", lc->str);
				 /* NOT REACHED */
			      }
			   }
                           Dmsg1(150, "calling handler for %s\n", items[i].name);
			   /* Call item handler */
			   items[i].handler(lc, &items[i], i, pass);
			   i = -1;
			   break;
			}
		     }
		     if (i >= 0) {
                        Dmsg2(150, "level=%d id=%s\n", level, lc->str);
                        Dmsg1(150, "Keyword = %s\n", lc->str);
                        scan_err1(lc, "Keyword \"%s\" not permitted in this resource", lc->str);
			/* NOT REACHED */
		     }
		     break;

		  case T_EOB:
		     level--;
		     state = p_none;
                     Dmsg0(150, "T_EOB => define new resource\n");
		     save_resource(res_type, items, pass);  /* save resource */
		     break;

		  case T_EOL:
		     break;

		  default:
                     scan_err2(lc, "unexpected token %d %s in resource definition",    
			token, lex_tok_to_str(token));
		     /* NOT REACHED */
	       }
	       break;
	    default:
               scan_err1(lc, "Unknown parser state %d\n", state);
	       /* NOT REACHED */
	 }
      }
      if (state != p_none) {
         scan_err0(lc, "End of conf file reached with unclosed resource.");
      }
      if (debug_level > 50 && pass == 2) {
	 int i;
	 for (i=r_first; i<=r_last; i++) {
	    dump_resource(i, resources[i-r_first].res_head, prtmsg, NULL);
	 }
      }
      lc = lex_close_file(lc);
   }
   Dmsg0(200, "Leave parse_config()\n");
}

/*********************************************************************
 *
 *	Free configuration resources
 *
 */
void 
free_config_resources()
{
   int i;
   for (i=r_first; i<=r_last; i++) {
      free_resource(i);
   }
}
