/*
 *
 *   Bacula Director -- newvol.c -- creates new Volumes in
 *    catalog Media table from the LabelFormat specification.
 *
 *     Kern Sibbald, May MMI
 *
 *    This routine runs as a thread and must be thread reentrant.
 *
 *  Basic tasks done here:
 *	If possible create a new Media entry
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
#include "dird.h"

/* Forward referenced functions */
static int create_simple_name(JCR *jcr, MEDIA_DBR *mr, POOL_DBR *pr);
static int perform_full_name_substitution(JCR *jcr, MEDIA_DBR *mr, POOL_DBR *pr);


/*
 * Really crude automatic Volume name creation using
 *  LabelFormat. We assume that if this routine is being
 *  called the Volume will be labeled, so we set the LabelDate.
 */
int newVolume(JCR *jcr, MEDIA_DBR *mr)
{
   POOL_DBR pr;

   memset(&pr, 0, sizeof(pr));

   /* See if we can create a new Volume */
   db_lock(jcr->db);
   pr.PoolId = jcr->PoolId;
   if (db_get_pool_record(jcr, jcr->db, &pr) && pr.LabelFormat[0] &&
       pr.LabelFormat[0] != '*') {
      if (pr.MaxVols == 0 || pr.NumVols < pr.MaxVols) {
	 memset(mr, 0, sizeof(MEDIA_DBR));
	 set_pool_dbr_defaults_in_media_dbr(mr, &pr);
	 mr->LabelDate = time(NULL);
	 bstrncpy(mr->MediaType, jcr->store->media_type, sizeof(mr->MediaType));
	 /* Check for special characters */
	 if (is_volume_name_legal(NULL, pr.LabelFormat)) {
	    /* No special characters, so apply simple algorithm */
	    if (!create_simple_name(jcr, mr, &pr)) {
	       goto bail_out;
	    }
	 } else {  /* try full substitution */
	    /* Found special characters, so try substitution */
	    if (!perform_full_name_substitution(jcr, mr, &pr)) { 
	       goto bail_out;
	    }
	    if (!is_volume_name_legal(NULL, mr->VolumeName)) {
               Jmsg(jcr, M_ERROR, 0, _("Illegal character in Volume name \"%s\"\n"),
		  mr->VolumeName);
	       goto bail_out;
	    }
	 }
	 pr.NumVols++;
	 if (db_create_media_record(jcr, jcr->db, mr) &&
	    db_update_pool_record(jcr, jcr->db, &pr)) {
	    db_unlock(jcr->db);
            Jmsg(jcr, M_INFO, 0, _("Created new Volume \"%s\" in catalog.\n"), mr->VolumeName);
            Dmsg1(90, "Created new Volume=%s\n", mr->VolumeName);
	    return 1;
	 } else {
            Jmsg(jcr, M_ERROR, 0, "%s", db_strerror(jcr->db));
	 }
      }
   }
bail_out:
   db_unlock(jcr->db);
   return 0;   
}

static int create_simple_name(JCR *jcr, MEDIA_DBR *mr, POOL_DBR *pr)
{
   char name[MAXSTRING];
   char num[20];

   /* See if volume already exists */
   mr->VolumeName[0] = 0;
   bstrncpy(name, pr->LabelFormat, sizeof(name));
   for (int i=pr->NumVols+1; i<(int)pr->NumVols+11; i++) {
      MEDIA_DBR tmr;
      memset(&tmr, 0, sizeof(tmr));
      sprintf(num, "%04d", i);
      bstrncpy(tmr.VolumeName, name, sizeof(tmr.VolumeName));
      bstrncat(tmr.VolumeName, num, sizeof(tmr.VolumeName));
      if (db_get_media_record(jcr, jcr->db, &tmr)) {
	 Jmsg(jcr, M_WARNING, 0, 
             _("Wanted to create Volume \"%s\", but it already exists. Trying again.\n"), 
	     tmr.VolumeName);
	 continue;
      }
      bstrncpy(mr->VolumeName, name, sizeof(mr->VolumeName));
      bstrncat(mr->VolumeName, num, sizeof(mr->VolumeName));
      break;			/* Got good name */
   }
   if (mr->VolumeName[0] == 0) {
      Jmsg(jcr, M_ERROR, 0, _("Too many failures. Giving up creating Volume name.\n"));
      return 0;
   }
   return 1;
}

static int date_item(JCR *jcr, int code, 
	      const char **val_ptr, int *val_len, int *val_size)
{
   struct tm tm;
   time_t now = time(NULL);
   localtime_r(&now, &tm);
   int val = 0;
   char buf[10];

   switch (code) {
   case 1:			      /* year */
      val = tm.tm_year + 1900;
      break;
   case 2:			      /* month */
      val = tm.tm_mon + 1;
      break;
   case 3:			      /* day */
      val = tm.tm_mday;
      break;
   case 4:			      /* hour */
      val = tm.tm_hour;
      break;
   case 5:			      /* minute */
      val = tm.tm_min;
      break;
   case 6:			      /* second */
      val = tm.tm_sec;
      break;
   case 7:			      /* Week day */
      val = tm.tm_wday;
      break;
   }
   bsnprintf(buf, sizeof(buf), "%d", val);
   *val_ptr = bstrdup(buf);
   *val_len = strlen(buf);
   *val_size = *val_len;
   return 1;
}

static int job_item(JCR *jcr, int code, 
	      const char **val_ptr, int *val_len, int *val_size)
{
   char *str = " ";
   char buf[20];

   switch (code) {
   case 1:			      /* Job */
      str = jcr->Job;
      break;
   case 2:                            /* Director's name */
      str = my_name;
      break;
   case 3:			      /* level */
      str = job_level_to_str(jcr->JobLevel);
      break;
   case 4:			      /* type */
      str = job_type_to_str(jcr->JobType);
      break;
   case 5:			      /* JobId */
      bsnprintf(buf, sizeof(buf), "%d", jcr->JobId);
      str = buf;
      break;
   case 6:			      /* Client */
      str = jcr->client_name;
      if (!str) {
         str = " ";
      }
      break;
   case 7:			      /* NumVols */
      bsnprintf(buf, sizeof(buf), "%d", jcr->NumVols);
      str = buf;
      break;
   case 8:			      /* Pool */
      str = jcr->client->hdr.name;   
      break;
   }
   *val_ptr = bstrdup(str);
   *val_len = strlen(str);
   *val_size = *val_len;
   return 1;
}


struct s_built_in_vars {char *var_name; int code; int (*func)(JCR *jcr, int code,
			 const char **val_ptr, int *val_len, int *val_size);};

static struct s_built_in_vars built_in_vars[] = {
   { N_("Year"),    1, date_item},
   { N_("Month"),   2, date_item},
   { N_("Day"),     3, date_item},
   { N_("Hour"),    4, date_item},
   { N_("Minute"),  5, date_item},
   { N_("Second"),  6, date_item},
   { N_("WeekDay"), 7, date_item},

   { N_("Job"),     1, job_item},
   { N_("Dir"),     2, job_item},
   { N_("Level"),   3, job_item},
   { N_("Type"),    4, job_item},
   { N_("JobId"),   5, job_item},
   { N_("Client"),  6, job_item},
   { N_("NumVols"), 7, job_item},
   { N_("Pool"),    8, job_item},

   { NULL, 0, NULL}
};


static var_rc_t lookup_built_in_var(var_t *ctx, void *my_ctx, 
	  const char *var_ptr, int var_len, int var_index, 
	  const char **val_ptr, int *val_len, int *val_size)
{
   JCR *jcr = (JCR *)my_ctx;
   int stat;

   for (int i=0; _(built_in_vars[i].var_name); i++) {
      if (strncmp(_(built_in_vars[i].var_name), var_ptr, var_len) == 0) {
	 stat = (*built_in_vars[i].func)(jcr, built_in_vars[i].code,
	    val_ptr, val_len, val_size);
	 if (stat) {
	    return VAR_OK;
	 }
	 break;
      }
   }
   return VAR_ERR_UNDEFINED_VARIABLE;
}


/*
 * Search counter variables 
 */
static var_rc_t lookup_counter_var(var_t *ctx, void *my_ctx, 
	  const char *var_ptr, int var_len, int var_inc, int var_index, 
	  const char **val_ptr, int *val_len, int *val_size)
{
   char buf[MAXSTRING];
   var_rc_t stat = VAR_ERR_UNDEFINED_VARIABLE;

   if (var_len > (int)sizeof(buf) - 1) {
       return VAR_ERR_OUT_OF_MEMORY;
   }
   memcpy(buf, var_ptr, var_len);
   buf[var_len] = 0;
   LockRes();
   for (COUNTER *counter=NULL; (counter = (COUNTER *)GetNextRes(R_COUNTER, (RES *)counter)); ) {
      if (strcmp(counter->hdr.name, buf) == 0) {
         bsnprintf(buf, sizeof(buf), "%d", counter->CurrentValue);
	 *val_ptr = bstrdup(buf);
	 *val_len = strlen(buf);
	 *val_size = *val_len;
	 if (var_inc) {
	    COUNTER_DBR cr;
	    JCR *jcr = (JCR *)my_ctx;
	    memset(&cr, 0, sizeof(cr));
	    bstrncpy(cr.Counter, counter->hdr.name, sizeof(cr.Counter));
	    cr.MinValue = counter->MinValue;
	    cr.MaxValue = counter->MaxValue;
	    cr.CurrentValue = ++counter->CurrentValue;
	    bstrncpy(cr.WrapCounter, counter->WrapCounter->hdr.name, sizeof(cr.WrapCounter));
	    if (!db_update_counter_record(jcr, jcr->db, &cr)) {
               Jmsg(jcr, M_ERROR, 0, _("Count not update counter %s: ERR=%s\n"),
		  counter->hdr.name, db_strerror(jcr->db));
	    }
	 }  
	 stat = VAR_OK;
	 break;
      }
   }
   UnlockRes();
   return stat;
}


/*
 * Called here to look up a variable   
 */
static var_rc_t lookup_var(var_t *ctx, void *my_ctx, 
	  const char *var_ptr, int var_len, int var_inc, int var_index, 
	  const char **val_ptr, int *val_len, int *val_size)
{
   char buf[MAXSTRING];
   var_rc_t stat;

   if ((stat = lookup_built_in_var(ctx, my_ctx, var_ptr, var_len, var_index,
	val_ptr, val_len, val_size)) == VAR_OK) {
      return VAR_OK;
   }

   if ((stat = lookup_counter_var(ctx, my_ctx, var_ptr, var_len, var_inc, var_index,
	val_ptr, val_len, val_size)) == VAR_OK) {
      return VAR_OK;
   }

   /* Look in environment */
   if (var_index != 0) {
       return VAR_ERR_ARRAY_LOOKUPS_ARE_UNSUPPORTED;
   }
   if (var_len > (int)sizeof(buf) - 1) {
       return VAR_ERR_OUT_OF_MEMORY;
   }
   memcpy(buf, var_ptr, var_len+1);
   buf[var_len+1] = 0;
   Dmsg1(000, "Var=%s\n", buf);

   if ((*val_ptr = getenv(buf)) == NULL) {
       return VAR_ERR_UNDEFINED_VARIABLE;
   }
   *val_len = strlen(*val_ptr);
   *val_size = 0;
   return VAR_OK;
}

/*
 * Called here to do a special operation on a variable
 *   op_ptr  points to the special operation code (not EOS terminated)
 *   arg_ptr points to argument to special op code
 *   val_ptr points to the value string
 *   out_ptr points to string to be returned
 */
static var_rc_t operate_var(var_t *var, void *my_ctx, 
	  const char *op_ptr, int op_len, 
	  const char *arg_ptr, int arg_len,
	  const char *val_ptr, int val_len, 
	  char **out_ptr, int *out_len, int *out_size)
{
   var_rc_t stat = VAR_ERR_UNDEFINED_OPERATION;
   Dmsg0(000, "Enter operate_var\n");
   if (!val_ptr) {
      *out_size = 0;
      return stat;
   }
   if (op_len == 3 && strncmp(op_ptr, "inc", 3) == 0) {
      char buf[MAXSTRING];
      if (val_len > (int)sizeof(buf) - 1) {
	  return VAR_ERR_OUT_OF_MEMORY;
      }
      memcpy(buf, arg_ptr, arg_len);
      buf[arg_len] = 0;
      Dmsg1(000, "Arg=%s\n", buf);
      memcpy(buf, val_ptr, val_len);
      buf[val_len] = 0;   
      Dmsg1(000, "Val=%s\n", buf);
      LockRes();
      for (COUNTER *counter=NULL; (counter = (COUNTER *)GetNextRes(R_COUNTER, (RES *)counter)); ) {
	 if (strcmp(counter->hdr.name, buf) == 0) {
            Dmsg2(000, "counter=%s val=%s\n", counter->hdr.name, buf);
	    break;
	 }
      }
      UnlockRes();
      return stat;
   }
   *out_size = 0;
   return stat;
}

/*
 * Perform full substitution on Label
 */
static int perform_full_name_substitution(JCR *jcr, MEDIA_DBR *mr, POOL_DBR *pr)
{
   var_t *var_ctx;
   var_rc_t stat;
   char *inp, *outp;
   int in_len, out_len;
   int rtn_stat = 0;

   inp = pr->LabelFormat;
   in_len = strlen(inp);

   outp = NULL;
   out_len = 0;

   jcr->NumVols = pr->NumVols;
   /* create context */
   if ((stat = var_create(&var_ctx)) != VAR_OK) {
       Jmsg(jcr, M_ERROR, 0, _("Cannot create var context: ERR=%s\n"), var_strerror(var_ctx, stat));
       goto bail_out;
   }
   /* define callback */
   if ((stat = var_config(var_ctx, VAR_CONFIG_CB_VALUE, lookup_var, (void *)jcr)) != VAR_OK) {
       Jmsg(jcr, M_ERROR, 0, _("Cannot set var callback: ERR=%s\n"), var_strerror(var_ctx, stat));
       goto bail_out;
   }


   /* define special operations */
   if ((stat = var_config(var_ctx, VAR_CONFIG_CB_OPERATION, operate_var, (void *)jcr)) != VAR_OK) {
       Jmsg(jcr, M_ERROR, 0, _("Cannot set var operate: ERR=%s\n"), var_strerror(var_ctx, stat));
       goto bail_out;
   }

   /* unescape in place */
   if ((stat = var_unescape(var_ctx, inp, in_len, inp, in_len+1, 0)) != VAR_OK) {
       Jmsg(jcr, M_ERROR, 0, _("Cannot unescape string: ERR=%s\n"), var_strerror(var_ctx, stat));
       goto bail_out;
   }

   in_len = strlen(inp);

   /* expand variables */
   if ((stat = var_expand(var_ctx, inp, in_len, &outp, &out_len, 1)) != VAR_OK) {
       Jmsg(jcr, M_ERROR, 0, _("Cannot expand LabelFormat \"%s\": ERR=%s\n"), 
	  inp, var_strerror(var_ctx, stat));
       goto bail_out;
   }

   /* unescape once more in place */
   if ((stat = var_unescape(var_ctx, outp, out_len, outp, out_len+1, 1)) != VAR_OK) {
       Jmsg(jcr, M_ERROR, 0, _("Cannot unescape string: ERR=%s\n"), var_strerror(var_ctx, stat));
       goto bail_out;
   }

   bstrncpy(mr->VolumeName, outp, sizeof(mr->VolumeName));

   rtn_stat = 1;  

bail_out:
   /* destroy expansion context */
   if ((stat = var_destroy(var_ctx)) != VAR_OK) {
       Jmsg(jcr, M_ERROR, 0, _("Cannot destroy var context: ERR=%s\n"), var_strerror(var_ctx, stat));
   }
   if (outp) {
      free(outp);
   }
   return rtn_stat;
}
