/*
 *   Configuration file parser for Include, Exclude, Finclude
 *    and FExclude records.
 *
 *     Kern Sibbald, March MMIII
 *
 *     Version $Id$
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
#include "dird.h"

/* Forward referenced subroutines */

void store_inc(LEX *lc, struct res_items *item, int index, int pass);
void store_finc(LEX *lc, struct res_items *item, int index, int pass);

static void store_match(LEX *lc, struct res_items *item, int index, int pass);
static void store_opts(LEX *lc, struct res_items *item, int index, int pass);
static void store_fname(LEX *lc, struct res_items *item, int index, int pass);
static void store_base(LEX *lc, struct res_items *item, int index, int pass);
static void setup_current_opts(void);


/* We build the current resource here as we are
 * scanning the resource configuration definition,
 * then move it to allocated memory when the resource
 * scan is complete.
 */
extern URES res_all;
extern int  res_all_size;

/* We build the current Finclude Fexclude item here */
static INCEXE res_incexe;

/* 
 * FInclude/FExclude items
 *   name	      handler	  value 		       code flags default_value
 */
static struct res_items finc_items[] = {
   {"compression",     store_opts,    NULL,     0, 0, 0},
   {"signature",       store_opts,    NULL,     0, 0, 0},
   {"verify",          store_opts,    NULL,     0, 0, 0},
   {"onefs",           store_opts,    NULL,     0, 0, 0},
   {"recurse",         store_opts,    NULL,     0, 0, 0},
   {"sparse",          store_opts,    NULL,     0, 0, 0},
   {"readfifo",        store_opts,    NULL,     0, 0, 0},
   {"replace",         store_opts,    NULL,     0, 0, 0},
   {"match",           store_match,   NULL,     0, 0, 0},
   {"file",            store_fname,   NULL,     0, 0, 0},
   {"base",            store_base,    NULL,     0, 0, 0},
   {NULL, NULL, NULL, 0, 0, 0} 
};

/* Define FileSet KeyWord values */

#define INC_KW_NONE	    0
#define INC_KW_COMPRESSION  1
#define INC_KW_SIGNATURE    2
#define INC_KW_ENCRYPTION   3
#define INC_KW_VERIFY	    4
#define INC_KW_ONEFS	    5
#define INC_KW_RECURSE	    6
#define INC_KW_SPARSE	    7
#define INC_KW_REPLACE	    8	      /* restore options */
#define INC_KW_READFIFO     9	      /* Causes fifo data to be read */

/* Include keywords -- these are keywords that can appear
 *    in the options lists of an include ( Include = compression= ...)
 */
static struct s_kw FS_option_kw[] = {
   {"compression", INC_KW_COMPRESSION},
   {"signature",   INC_KW_SIGNATURE},
   {"encryption",  INC_KW_ENCRYPTION},
   {"verify",      INC_KW_VERIFY},
   {"onefs",       INC_KW_ONEFS},
   {"recurse",     INC_KW_RECURSE},
   {"sparse",      INC_KW_SPARSE},
   {"replace",     INC_KW_REPLACE},
   {"readfifo",    INC_KW_READFIFO},
   {NULL,	   0}
};

/* Options for FileSet keywords */

struct s_fs_opt {
   char *name;
   int keyword;
   char *option;
};

/*
 * Options permitted for each keyword and resulting value.     
 * The output goes into opts, which are then transmitted to
 * the FD for application as options to the following list of
 * included files.
 */
static struct s_fs_opt FS_options[] = {
   {"md5",      INC_KW_SIGNATURE,    "M"},
   {"sha1",     INC_KW_SIGNATURE,    "S"},
   {"gzip",     INC_KW_COMPRESSION,  "Z6"},
   {"gzip1",    INC_KW_COMPRESSION,  "Z1"},
   {"gzip2",    INC_KW_COMPRESSION,  "Z2"},
   {"gzip3",    INC_KW_COMPRESSION,  "Z3"},
   {"gzip4",    INC_KW_COMPRESSION,  "Z4"},
   {"gzip5",    INC_KW_COMPRESSION,  "Z5"},
   {"gzip6",    INC_KW_COMPRESSION,  "Z6"},
   {"gzip7",    INC_KW_COMPRESSION,  "Z7"},
   {"gzip8",    INC_KW_COMPRESSION,  "Z8"},
   {"gzip9",    INC_KW_COMPRESSION,  "Z9"},
   {"blowfish", INC_KW_ENCRYPTION,    "B"},   /* ***FIXME*** not implemented */
   {"3des",     INC_KW_ENCRYPTION,    "3"},   /* ***FIXME*** not implemented */
   {"yes",      INC_KW_ONEFS,         "0"},
   {"no",       INC_KW_ONEFS,         "f"},
   {"yes",      INC_KW_RECURSE,       "0"},
   {"no",       INC_KW_RECURSE,       "h"},
   {"yes",      INC_KW_SPARSE,        "s"},
   {"no",       INC_KW_SPARSE,        "0"},
   {"always",   INC_KW_REPLACE,       "a"},
   {"ifnewer",  INC_KW_REPLACE,       "w"},
   {"never",    INC_KW_REPLACE,       "n"},
   {"yes",      INC_KW_READFIFO,      "r"},
   {"no",       INC_KW_READFIFO,      "0"},
   {NULL,	0,		     0}
};



/* 
 * Scan for Include options (keyword=option) is converted into one or
 *  two characters. Verifyopts=xxxx is Vxxxx:
 */
static void scan_include_options(LEX *lc, int keyword, char *opts, int optlen)
{
   int token, i;
   char option[3];

   option[0] = 0;		      /* default option = none */
   option[2] = 0;		      /* terminate options */
   token = lex_get_token(lc, T_NAME);		  /* expect at least one option */	 
   if (keyword == INC_KW_VERIFY) { /* special case */
      /* ***FIXME**** ensure these are in permitted set */
      bstrncat(opts, "V", optlen);         /* indicate Verify */
      bstrncat(opts, lc->str, optlen);
      bstrncat(opts, ":", optlen);         /* terminate it */

   /*
    * Standard keyword options for Include/Exclude 
    */
   } else {
      for (i=0; FS_options[i].name; i++) {
	 if (strcasecmp(lc->str, FS_options[i].name) == 0 && FS_options[i].keyword == keyword) {
	    /* NOTE! maximum 2 letters here or increase option[3] */
	    option[0] = FS_options[i].option[0];
	    option[1] = FS_options[i].option[1];
	    i = 0;
	    break;
	 }
      }
      if (i != 0) {
         scan_err1(lc, "Expected a FileSet option keyword, got:%s:", lc->str);
      } else { /* add option */
	 bstrncat(opts, option, optlen);
         Dmsg3(200, "Catopts=%s option=%s optlen=%d\n", opts, option,optlen);
      }
   }

   /* If option terminated by comma, eat it */
   if (lc->ch == ',') {
      token = lex_get_token(lc, T_ALL);      /* yes, eat comma */
   }
}

/* Store FileSet Include/Exclude info */
void store_inc(LEX *lc, struct res_items *item, int index, int pass)
{
   int token, i;
   int options = lc->options;
   int keyword;
   char inc_opts[100];
   int inc_opts_len;

   lc->options |= LOPT_NO_IDENT;      /* make spaces significant */
   memset(&res_incexe, 0, sizeof(INCEXE));

   /* Get include options */
   inc_opts[0] = 0;
   while ((token=lex_get_token(lc, T_ALL)) != T_BOB) {
      keyword = INC_KW_NONE;
      for (i=0; FS_option_kw[i].name; i++) {
	 if (strcasecmp(lc->str, FS_option_kw[i].name) == 0) {
	    keyword = FS_option_kw[i].token;
	    break;
	 }
      }
      if (keyword == INC_KW_NONE) {
         scan_err1(lc, _("Expected a FileSet keyword, got: %s"), lc->str);
      }
      /* Option keyword should be following by = <option> */
      if ((token=lex_get_token(lc, T_ALL)) != T_EQUALS) {
         scan_err1(lc, _("expected an = following keyword, got: %s"), lc->str);
      } else {
	 scan_include_options(lc, keyword, inc_opts, sizeof(inc_opts));
      }
      if (token == T_BOB) {
	 break;
      }
   }
   if (!inc_opts[0]) {
      strcat(inc_opts, "0");          /* set no options */
   }
   inc_opts_len = strlen(inc_opts);

   if (pass == 1) {
      INCEXE *incexe;
      if (!res_all.res_fs.have_MD5) {
	 MD5Init(&res_all.res_fs.md5c);
	 res_all.res_fs.have_MD5 = TRUE;
      }
      setup_current_opts();
      bstrncpy(res_incexe.current_opts->opts, inc_opts, MAX_FOPTS);
      Dmsg1(200, "incexe opts=%s\n", res_incexe.current_opts->opts);

      /* Create incexe structure */
      Dmsg0(200, "Create INCEXE structure\n");
      incexe = (INCEXE *)malloc(sizeof(INCEXE));
      memcpy(incexe, &res_incexe, sizeof(INCEXE));
      memset(&res_incexe, 0, sizeof(INCEXE));
      if (item->code == 0) { /* include */
	 if (res_all.res_fs.num_includes == 0) {
	    res_all.res_fs.include_items = (INCEXE **)malloc(sizeof(INCEXE *));
	  } else {
	    res_all.res_fs.include_items = (INCEXE **)realloc(res_all.res_fs.include_items,
			   sizeof(INCEXE *) * res_all.res_fs.num_includes + 1);
	  }
	  res_all.res_fs.include_items[res_all.res_fs.num_includes++] = incexe;
          Dmsg1(200, "num_includes=%d\n", res_all.res_fs.num_includes);
      } else {	  /* exclude */
	 if (res_all.res_fs.num_excludes == 0) {
	    res_all.res_fs.exclude_items = (INCEXE **)malloc(sizeof(INCEXE *));
	  } else {
	    res_all.res_fs.exclude_items = (INCEXE **)realloc(res_all.res_fs.exclude_items,
			   sizeof(INCEXE *) * res_all.res_fs.num_excludes + 1);
	  }
	  res_all.res_fs.exclude_items[res_all.res_fs.num_excludes++] = incexe;
          Dmsg1(200, "num_excludes=%d\n", res_all.res_fs.num_excludes);
      }

      /* Pickup include/exclude names.	They are stored in INCEXE
       *  structures which contains the options and the name.
       */
      while ((token = lex_get_token(lc, T_ALL)) != T_EOB) {
	 switch (token) {
	    case T_COMMA:
	    case T_EOL:
	       continue;

	    case T_IDENTIFIER:
	    case T_UNQUOTED_STRING:
	    case T_QUOTED_STRING:
	       if (res_all.res_fs.have_MD5) {
		  MD5Update(&res_all.res_fs.md5c, (unsigned char *)lc->str, lc->str_len);
	       }
	       if (incexe->num_names == incexe->max_names) {
		  incexe->max_names += 10;
		  if (incexe->name_list == NULL) {
		     incexe->name_list = (char **)malloc(sizeof(char *) * incexe->max_names);
		  } else {
		     incexe->name_list = (char **)realloc(incexe->name_list,
			   sizeof(char *) * incexe->max_names);
		  }
	       }
	       incexe->name_list[incexe->num_names++] = bstrdup(lc->str);
               Dmsg1(200, "Add to name_list %s\n", incexe->name_list[incexe->num_names -1]);
	       break;
	    default:
               scan_err1(lc, "Expected a filename, got: %s", lc->str);
	 }				   
      }
      /* Note, MD5Final is done in backup.c */
   } else { /* pass 2 */
      while (lex_get_token(lc, T_ALL) != T_EOB) 
	 {}
   }
   scan_to_eol(lc);
   lc->options = options;
   set_bit(index, res_all.hdr.item_present);
}


/*
 * Store FileSet FInclude/FExclude info   
 *  Note, when this routine is called, we are inside a FileSet
 *  resource.  We treat the Finclude/Fexeclude like a sort of
 *  mini-resource within the FileSet resource.
 */
void store_finc(LEX *lc, struct res_items *item, int index, int pass)
{
   int token, i;
   INCEXE *incexe;

   if (!res_all.res_fs.have_MD5) {
      MD5Init(&res_all.res_fs.md5c);
      res_all.res_fs.have_MD5 = TRUE;
   }
   res_all.res_fs.finclude = TRUE;
   token = lex_get_token(lc, T_ALL);		
   if (token != T_BOB) {
      scan_err1(lc, _("Expecting a beginning brace, got: %s\n"), lc->str);
   }
   memset(&res_incexe, 0, sizeof(INCEXE));
   while ((token = lex_get_token(lc, T_ALL)) != T_EOF) {
      if (token == T_EOL) {
	 continue;
      }
      if (token == T_EOB) {
	 break;
      }
      if (token != T_IDENTIFIER) {
         scan_err1(lc, _("Expecting keyword, got: %s\n"), lc->str);
      }
      for (i=0; finc_items[i].name; i++) {
	 if (strcasecmp(finc_items[i].name, lc->str) == 0) {
	    token = lex_get_token(lc, T_ALL);
	    if (token != T_EQUALS) {
               scan_err1(lc, "expected an equals, got: %s", lc->str);
	    }
	    /* Call item handler */
	    finc_items[i].handler(lc, &finc_items[i], i, pass);
	    i = -1;
	    break;
	 }
      }
      if (i >=0) {
         scan_err1(lc, "Keyword %s not permitted in this resource", lc->str);
      }
   }
   if (pass == 1) {
      incexe = (INCEXE *)malloc(sizeof(INCEXE));
      memcpy(incexe, &res_incexe, sizeof(INCEXE));
      memset(&res_incexe, 0, sizeof(INCEXE));
      if (item->code == 0) { /* include */
	 if (res_all.res_fs.num_includes == 0) {
	    res_all.res_fs.include_items = (INCEXE **)malloc(sizeof(INCEXE *));
	 } else {
	    res_all.res_fs.include_items = (INCEXE **)realloc(res_all.res_fs.include_items,
			   sizeof(INCEXE *) * res_all.res_fs.num_includes + 1);
	 }
	 res_all.res_fs.include_items[res_all.res_fs.num_includes++] = incexe;
         Dmsg1(200, "num_includes=%d\n", res_all.res_fs.num_includes);
      } else {	  /* exclude */
	 if (res_all.res_fs.num_excludes == 0) {
	    res_all.res_fs.exclude_items = (INCEXE **)malloc(sizeof(INCEXE *));
	 } else {
	    res_all.res_fs.exclude_items = (INCEXE **)realloc(res_all.res_fs.exclude_items,
			   sizeof(INCEXE *) * res_all.res_fs.num_excludes + 1);
	 }
	 res_all.res_fs.exclude_items[res_all.res_fs.num_excludes++] = incexe;
         Dmsg1(200, "num_excludes=%d\n", res_all.res_fs.num_excludes);
      }
   }
   scan_to_eol(lc);
   set_bit(index, res_all.hdr.item_present);
}


/* Store Match info */
static void store_match(LEX *lc, struct res_items *item, int index, int pass)
{
   int token;

   if (pass == 1) {
      /* Pickup Match string
       */
      token = lex_get_token(lc, T_ALL); 	   
      switch (token) {
	 case T_IDENTIFIER:
	 case T_UNQUOTED_STRING:
	 case T_QUOTED_STRING:
	    setup_current_opts();
	    if (res_incexe.current_opts->match) {
               scan_err0(lc, _("More than one match specified.\n")); 
	    }
	    res_incexe.current_opts->match = bstrdup(lc->str);
	    break;
	 default:
            scan_err1(lc, _("Expected a filename, got: %s\n"), lc->str);
      } 				
   } else { /* pass 2 */
      lex_get_token(lc, T_ALL); 	 
   }
   scan_to_eol(lc);
}

/* Store Base info */
static void store_base(LEX *lc, struct res_items *item, int index, int pass)
{
   int token;
   FOPTS *copt;

   if (pass == 1) {
      setup_current_opts();
      /*
       * Pickup Base Job Name
       */
      token = lex_get_token(lc, T_NAME);	   
      copt = res_incexe.current_opts;
      if (copt->base_list == NULL) {
	 copt->base_list = (char **)malloc(sizeof(char *));			
      } else {
	 copt->base_list = (char **)realloc(copt->base_list,
	    sizeof(char *) * (copt->num_base+1));
      }
      copt->base_list[copt->num_base++] = bstrdup(lc->str);
   } else { /* pass 2 */
      lex_get_token(lc, T_ALL); 	 
   }
   scan_to_eol(lc);
}
/*
 * Store Filename info. Note, for minor efficiency reasons, we
 * always increase the name buffer by 10 items because we expect
 * to add more entries.
 */
static void store_fname(LEX *lc, struct res_items *item, int index, int pass)
{
   int token;
   INCEXE *incexe;

   if (pass == 1) {
      /* Pickup Filename string
       */
      token = lex_get_token(lc, T_ALL); 	   
      switch (token) {
	 case T_IDENTIFIER:
	 case T_UNQUOTED_STRING:
	 case T_QUOTED_STRING:
	    if (res_all.res_fs.have_MD5) {
	       MD5Update(&res_all.res_fs.md5c, (unsigned char *)lc->str, lc->str_len);
	    }
	    incexe = &res_incexe;
	    if (incexe->num_names == incexe->max_names) {
	       incexe->max_names += 10;
	       if (incexe->name_list == NULL) {
		  incexe->name_list = (char **)malloc(sizeof(char *) * incexe->max_names);
	       } else {
		  incexe->name_list = (char **)realloc(incexe->name_list,
			sizeof(char *) * incexe->max_names);
	       }
	    }
	    incexe->name_list[incexe->num_names++] = bstrdup(lc->str);
            Dmsg1(200, "Add to name_list %s\n", incexe->name_list[incexe->num_names -1]);
	    break;
	 default:
            scan_err1(lc, _("Expected a filename, got: %s"), lc->str);
      } 				
   } else { /* pass 2 */
      lex_get_token(lc, T_ALL); 	 
   }
   scan_to_eol(lc);
}


static void store_opts(LEX *lc, struct res_items *item, int index, int pass)
{
   int i;
   int keyword;
   char inc_opts[100];

   inc_opts[0] = 0;
   keyword = INC_KW_NONE;
   for (i=0; FS_option_kw[i].name; i++) {
      if (strcasecmp(item->name, FS_option_kw[i].name) == 0) {
	 keyword = FS_option_kw[i].token;
	 break;
      }
   }
   if (keyword == INC_KW_NONE) {
      scan_err1(lc, "Expected a FileSet keyword, got: %s", lc->str);
   }
   Dmsg2(200, "keyword=%d %s\n", keyword, FS_option_kw[keyword].name);
   scan_include_options(lc, keyword, inc_opts, sizeof(inc_opts));

   if (pass == 1) {
      setup_current_opts();

      bstrncat(res_incexe.current_opts->opts, inc_opts, MAX_FOPTS);
   }

   scan_to_eol(lc);
}



/* If current_opts not defined, create first entry */
static void setup_current_opts(void)
{
   if (res_incexe.current_opts == NULL) {
      res_incexe.current_opts = (FOPTS *)malloc(sizeof(FOPTS));
      memset(res_incexe.current_opts, 0, sizeof(FOPTS));
      res_incexe.num_opts = 1;
      res_incexe.opts_list = (FOPTS **)malloc(sizeof(FOPTS *));
      res_incexe.opts_list[0] = res_incexe.current_opts;
   }
}
