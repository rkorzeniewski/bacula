/*
 *
 *  Configuration parser for Director Run Configuration
 *   directives, which are part of the Schedule Resource
 *
 *     Kern Sibbald, May MM
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

#include "bacula.h"
#include "dird.h"

extern URES res_all;
extern struct s_jl joblevels[];

/* Forward referenced subroutines */

enum e_state {
   s_none = 0,
   s_range,
   s_mday,
   s_month,
   s_time,
   s_at,
   s_wday,
   s_daily,
   s_weekly,
   s_monthly,
   s_hourly,
};  

struct s_keyw {
  char *name;			      /* keyword */
  enum e_state state;		      /* parser state */
  int code;			      /* state value */
};

/* Keywords understood by parser */
static struct s_keyw keyw[] = {
  {N_("on"),         s_none,    0},
  {N_("at"),         s_at,      0},

  {N_("sun"),        s_wday,    0},
  {N_("mon"),        s_wday,    1},
  {N_("tue"),        s_wday,    2},
  {N_("wed"),        s_wday,    3},
  {N_("thu"),        s_wday,    4},
  {N_("fri"),        s_wday,    5},
  {N_("sat"),        s_wday,    6},
  {N_("jan"),        s_month,   0},
  {N_("feb"),        s_month,   1},
  {N_("mar"),        s_month,   2},
  {N_("apr"),        s_month,   3},
  {N_("may"),        s_month,   4},
  {N_("jun"),        s_month,   5},
  {N_("jul"),        s_month,   6},
  {N_("aug"),        s_month,   7},
  {N_("sep"),        s_month,   8},
  {N_("oct"),        s_month,   9},
  {N_("nov"),        s_month,  10},
  {N_("dec"),        s_month,  11},

  {N_("sunday"),     s_wday,    0},
  {N_("monday"),     s_wday,    1},
  {N_("tuesday"),    s_wday,    2},
  {N_("wednesday"),  s_wday,    3},
  {N_("thursday"),   s_wday,    4},
  {N_("friday"),     s_wday,    5},
  {N_("saturday"),   s_wday,    6},
  {N_("january"),    s_month,   0},
  {N_("february"),   s_month,   1},
  {N_("march"),      s_month,   2},
  {N_("april"),      s_month,   3},
  {N_("june"),       s_month,   5},
  {N_("july"),       s_month,   6},
  {N_("august"),     s_month,   7},
  {N_("september"),  s_month,   8},
  {N_("october"),    s_month,   9},
  {N_("november"),   s_month,  10},
  {N_("december"),   s_month,  11},

  {N_("daily"),      s_daily,   0},
  {N_("weekly"),     s_weekly,  0},
  {N_("monthly"),    s_monthly, 0},
  {N_("hourly"),     s_hourly,  0},
  {NULL,	 s_none,    0}
};

static int have_hour, have_mday, have_wday, have_month;
static int have_at;
static RUN lrun;

static void clear_defaults()
{
   have_hour = have_mday = have_wday = have_month = TRUE;
   clear_bit(0,lrun.hour);
   clear_bits(0, 30, lrun.mday);
   clear_bits(0, 6, lrun.wday);
   clear_bits(0, 11, lrun.month);
}

static void set_defaults()
{
   have_hour = have_mday = have_wday = have_month = FALSE;
   have_at = FALSE;
   set_bit(0,lrun.hour);
   set_bits(0, 30, lrun.mday);
   set_bits(0, 6, lrun.wday);
   set_bits(0, 11, lrun.month);
}


/* Check if string is a number */
static int is_num(char *num)
{
   char *p = num;
   int ch;
   while ((ch = *p++)) {
      if (ch < '0' || ch > '9') {
	 return FALSE;
      }
   }
   return TRUE;
}


/* 
 * Store Schedule Run information   
 * 
 * Parse Run statement:
 *
 *  Run <full|incremental|...> [on] 2 january at 23:45
 *
 *   Default Run time is daily at 0:0
 *  
 *   There can be multiple run statements, they are simply chained
 *   together.
 *
 */
void store_run(LEX *lc, struct res_items *item, int index, int pass)
{
   int token, state, state2, i, code, code2;
   int options = lc->options;
   RUN **run = (RUN **)(item->value);	
   RUN *trun;
   char *p;


   lc->options |= LOPT_NO_IDENT;      /* want only "strings" */

   /* clear local copy of run record */
   memset(&lrun, 0, sizeof(RUN));

   /* scan for Job level "full", "incremental", ... */
   token = lex_get_token(lc);
   if (token != T_STRING) {
      scan_err1(lc, _("Expected a Job level identifier, got: %s"), lc->str);
   } else {
      if (strcasecmp(lc->str, "level")) {
	 if (lex_get_token(lc) != T_EQUALS) {
            scan_err1(lc, "Expected an equals, got: %s", lc->str);
	 }
	 token = lex_get_token(lc);
      }
      for (i=0; joblevels[i].level_name; i++) {
	 if (strcasecmp(lc->str, joblevels[i].level_name) == 0) {
	    lrun.level = joblevels[i].level;
	    lrun.job_type = joblevels[i].job_type;
	    i = 0;
	    break;
	 }
      }
      if (i != 0) {
         scan_err1(lc, _("Job level field: %s not found in run record"), lc->str);
      }
   }

   /*
    * Scan schedule times.
    * Default is: daily at 0:0
    */
   state = s_none;
   set_defaults();

   while ((token = lex_get_token(lc)) != T_EOL) {
      int len, pm;
      switch (token) {
	 case T_NUMBER:
	    state = s_mday;
	    code = atoi(lc->str) - 1;
	    if (code < 0 || code > 30) {
               scan_err0(lc, _("Day number out of range (1-31)"));
	    }
	    break;
	 case T_STRING:
            if (strchr(lc->str, (int)'-')) {
	       state = s_range;
	       break;
	    }
            if (strchr(lc->str, (int)':')) {
	       state = s_time;
	       break;
	    }
	    /* everything else must be a keyword */
	    lcase(lc->str);
	    for (i=0; keyw[i].name; i++) {
	       if (strcmp(lc->str, keyw[i].name) == 0) {
		  state = keyw[i].state;
		  code	 = keyw[i].code;
		  i = 0;
		  break;
	       }
	    }
	    if (i != 0) {
               scan_err1(lc, _("Job type field: %s in run record not found"), lc->str);
	    }
	    break;
	 case T_COMMA:
	    continue;
	 default:
            scan_err1(lc, _("Unexpected token: %s"), lc->str);
	    break;
      }
      switch (state) {
	 case s_none:
	    continue;
	 case s_mday:		      /* day of month */
	    if (!have_mday) {
	       clear_bits(0, 30, lrun.mday);
	       clear_bits(0, 6, lrun.wday);
	       have_mday = TRUE;
	    }
	    set_bit(code, lrun.mday);
	    break;
	 case s_month:		      /* month of year */
	    if (!have_month) {
	       clear_bits(0, 11, lrun.month);
	       have_month = TRUE;
	    }
	    set_bit(code, lrun.month);
	    break;
	 case s_wday:		      /* week day */
	    if (!have_wday) {
	       clear_bits(0, 6, lrun.wday);
	       clear_bits(0, 30, lrun.mday);
	       have_wday = TRUE;
	    }
	    set_bit(code, lrun.wday);
	    break;
	 case s_time:		      /* time */
	    if (!have_at) {
               scan_err0(lc, _("Time must be preceded by keyword AT."));
	    }
	    if (!have_hour) {
	       clear_bit(0, lrun.hour);
	       have_hour = TRUE;
	    }
            p = strchr(lc->str, ':');
	    if (!p)  {
               scan_err0(lc, _("Time logic error.\n"));
	    }
	    *p++ = 0;		      /* separate two halves */
	    code = atoi(lc->str);
	    len = strlen(p);
            if (len > 2 && p[len-1] == 'm') {
               if (p[len-2] == 'a') {
		  pm = 0;
               } else if (p[len-2] == 'p') {
		  pm = 1;
	       } else {
                  scan_err0(lc, _("Bad time specification."));
	       }
	    } else {
	       pm = 0;
	    }
	    code2 = atoi(p);
	    if (pm) {
	       code += 12;
	    }
	    if (code < 0 || code > 23 || code2 < 0 || code2 > 59) {
               scan_err0(lc, _("Bad time specification."));
	    }
	    set_bit(code, lrun.hour);
	    lrun.minute = code2;
	    break;
	 case s_at:
	    have_at = TRUE;
	    break;
	 case s_range:
            p = strchr(lc->str, '-');
	    if (!p) {
               scan_err0(lc, _("Range logic error.\n"));
	    }
	    *p++ = 0;		      /* separate two halves */

	    /* Check for day range */
	    if (is_num(lc->str) && is_num(p)) {
	       code = atoi(lc->str) - 1;
	       code2 = atoi(p) - 1;
	       if (code < 0 || code > 30 || code2 < 0 || code2 > 30) {
                  scan_err0(lc, _("Bad day range specification."));
	       }
	       if (!have_mday) {
		  clear_bits(0, 30, lrun.mday);
		  clear_bits(0, 6, lrun.wday);
		  have_mday = TRUE;
	       }
	       if (code < code2) {
		  set_bits(code, code2, lrun.mday);
	       } else {
		  set_bits(code, 30, lrun.mday);
		  set_bits(0, code2, lrun.mday);
	       }
	       break;
	    }

	    /* lookup first half of keyword range (week days or months) */
	    lcase(lc->str);
	    for (i=0; keyw[i].name; i++) {
	       if (strcmp(lc->str, keyw[i].name) == 0) {
		  state = keyw[i].state;
		  code	 = keyw[i].code;
		  i = 0;
		  break;
	       }
	    }
	    if (i != 0 || (state != s_month && state != s_wday)) {
               scan_err0(lc, _("Invalid month or week day range"));
	    }

	    /* Lookup end of range */
	    lcase(p);
	    for (i=0; keyw[i].name; i++) {
	       if (strcmp(p, keyw[i].name) == 0) {
		  state2  = keyw[i].state;
		  code2   = keyw[i].code;
		  i = 0;
		  break;
	       }
	    }
	    if (i != 0 || state != state2 || 
	       (state2 != s_month && state2 != s_wday) || code == code2) {
               scan_err0(lc, _("Invalid month or weekday range"));
	    }
	    if (state == s_wday) {
	       if (!have_wday) {
		  clear_bits(0, 6, lrun.wday);
		  clear_bits(0, 30, lrun.mday);
		  have_wday = TRUE;
	       }
	       if (code < code2) {
		  set_bits(code, code2, lrun.wday);
	       } else {
		  set_bits(code, 6, lrun.wday);
		  set_bits(0, code2, lrun.wday);
	       }
	    } else {
	       /* must be s_month */
	       if (!have_month) {
		  clear_bits(0, 30, lrun.month);
		  have_month = TRUE;
	       }
	       if (code < code2) {
		  set_bits(code, code2, lrun.month);
	       } else {
		  /* this is a bit odd, but we accept it anyway */
		  set_bits(code, 30, lrun.month);
		  set_bits(0, code2, lrun.month);
	       }
	    }
	    break;
	 case s_hourly:
	    clear_defaults();
	    set_bits(0, 23, lrun.hour);
	    set_bits(0, 30, lrun.mday);
	    set_bits(0, 11, lrun.month);
	    break;
	 case s_weekly:
	    clear_defaults();
	    set_bit(0, lrun.wday);
	    set_bits(0, 11, lrun.month);
	    break;
	 case s_daily:
	    clear_defaults();
	    set_bits(0, 30, lrun.mday);
	    set_bits(0, 11, lrun.month);
	    break;
	 case s_monthly:
	    clear_defaults();
	    set_bit(0, lrun.mday);
	    set_bits(0, 11, lrun.month);
	    break;
	 default:
            scan_err0(lc, _("Unexpected run state\n"));
	    break;
      }
   }

   /* Allocate run record, copy new stuff into it,
    * and link it into the list of run records 
    * in the schedule resource.
    */
   if (pass == 1) {
      trun = (RUN *) malloc(sizeof(RUN));
      memcpy(trun, &lrun, sizeof(RUN));
      if (*run) {
	 trun->next = *run;
      }
      *run = trun;
   }

   lc->options = options;	      /* restore scanner options */
   set_bit(index, res_all.res_sch.hdr.item_present);
}
