/*
 *
 *   Bacula Director -- Update command processing
 *     Split from ua_cmds.c March 2005
 *
 *     Kern Sibbald, September MM
 *
 *   Version $Id$
 */

/*
   Copyright (C) 2000-2005 Kern Sibbald

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

/* External variables */
extern char *list_pool; 	      /* in sql_cmds.c */

/* Imported functions */
void set_pooldbr_from_poolres(POOL_DBR *pr, POOL *pool, e_pool_op op);
int update_slots(UAContext *ua);


/* Forward referenced functions */
static int update_volume(UAContext *ua);
static int update_pool(UAContext *ua);

/*
 * Update a Pool Record in the database.
 *  It is always updated from the Resource record.
 *
 *    update pool=<pool-name>
 *	   updates pool from Pool resource
 *    update media pool=<pool-name> volume=<volume-name>
 *	   changes pool info for volume
 *    update slots [scan=...]
 *	   updates autochanger slots
 */
int update_cmd(UAContext *ua, const char *cmd)
{
   static const char *kw[] = {
      N_("media"),  /* 0 */
      N_("volume"), /* 1 */
      N_("pool"),   /* 2 */
      N_("slots"),  /* 3 */
      NULL};

   if (!open_db(ua)) {
      return 1;
   }

   switch (find_arg_keyword(ua, kw)) {
   case 0:
   case 1:
      update_volume(ua);
      return 1;
   case 2:
      update_pool(ua);
      return 1;
   case 3:
      update_slots(ua);
      return 1;
   default:
      break;
   }

   start_prompt(ua, _("Update choice:\n"));
   add_prompt(ua, _("Volume parameters"));
   add_prompt(ua, _("Pool from resource"));
   add_prompt(ua, _("Slots from autochanger"));
   switch (do_prompt(ua, _("item"), _("Choose catalog item to update"), NULL, 0)) {
   case 0:
      update_volume(ua);
      break;
   case 1:
      update_pool(ua);
      break;
   case 2:
      update_slots(ua);
      break;
   default:
      break;
   }
   return 1;
}

static void update_volstatus(UAContext *ua, const char *val, MEDIA_DBR *mr)
{
   POOLMEM *query = get_pool_memory(PM_MESSAGE);
   const char *kw[] = {
      "Append",
      "Archive",
      "Disabled",
      "Full",
      "Used",
      "Cleaning",
      "Recycle",
      "Read-Only",
      NULL};
   bool found = false;
   int i;

   for (i=0; kw[i]; i++) {
      if (strcasecmp(val, kw[i]) == 0) {
	 found = true;
	 break;
      }
   }
   if (!found) {
      bsendmsg(ua, _("Invalid VolStatus specified: %s\n"), val);
   } else {
      char ed1[50];
      bstrncpy(mr->VolStatus, kw[i], sizeof(mr->VolStatus));
      Mmsg(query, "UPDATE Media SET VolStatus='%s' WHERE MediaId=%s",
	 mr->VolStatus, edit_int64(mr->MediaId,ed1));
      if (!db_sql_query(ua->db, query, NULL, NULL)) {
         bsendmsg(ua, "%s", db_strerror(ua->db));
      } else {
         bsendmsg(ua, _("New Volume status is: %s\n"), mr->VolStatus);
      }
   }
   free_pool_memory(query);
}

static void update_volretention(UAContext *ua, char *val, MEDIA_DBR *mr)
{
   char ed1[150], ed2[50];
   POOLMEM *query;
   if (!duration_to_utime(val, &mr->VolRetention)) {
      bsendmsg(ua, _("Invalid retention period specified: %s\n"), val);
      return;
   }
   query = get_pool_memory(PM_MESSAGE);
   Mmsg(query, "UPDATE Media SET VolRetention=%s WHERE MediaId=%s",
      edit_uint64(mr->VolRetention, ed1), edit_int64(mr->MediaId,ed2));
   if (!db_sql_query(ua->db, query, NULL, NULL)) {
      bsendmsg(ua, "%s", db_strerror(ua->db));
   } else {
      bsendmsg(ua, _("New retention period is: %s\n"),
	 edit_utime(mr->VolRetention, ed1, sizeof(ed1)));
   }
   free_pool_memory(query);
}

static void update_voluseduration(UAContext *ua, char *val, MEDIA_DBR *mr)
{
   char ed1[150], ed2[50];
   POOLMEM *query;

   if (!duration_to_utime(val, &mr->VolUseDuration)) {
      bsendmsg(ua, _("Invalid use duration specified: %s\n"), val);
      return;
   }
   query = get_pool_memory(PM_MESSAGE);
   Mmsg(query, "UPDATE Media SET VolUseDuration=%s WHERE MediaId=%s",
      edit_uint64(mr->VolUseDuration, ed1), edit_int64(mr->MediaId,ed2));
   if (!db_sql_query(ua->db, query, NULL, NULL)) {
      bsendmsg(ua, "%s", db_strerror(ua->db));
   } else {
      bsendmsg(ua, _("New use duration is: %s\n"),
	 edit_utime(mr->VolUseDuration, ed1, sizeof(ed1)));
   }
   free_pool_memory(query);
}

static void update_volmaxjobs(UAContext *ua, char *val, MEDIA_DBR *mr)
{
   POOLMEM *query = get_pool_memory(PM_MESSAGE);
   char ed1[50];
   Mmsg(query, "UPDATE Media SET MaxVolJobs=%s WHERE MediaId=%s",
      val, edit_int64(mr->MediaId,ed1));
   if (!db_sql_query(ua->db, query, NULL, NULL)) {
      bsendmsg(ua, "%s", db_strerror(ua->db));
   } else {
      bsendmsg(ua, _("New max jobs is: %s\n"), val);
   }
   free_pool_memory(query);
}

static void update_volmaxfiles(UAContext *ua, char *val, MEDIA_DBR *mr)
{
   POOLMEM *query = get_pool_memory(PM_MESSAGE);
   char ed1[50];
   Mmsg(query, "UPDATE Media SET MaxVolFiles=%s WHERE MediaId=%s",
      val, edit_int64(mr->MediaId, ed1));
   if (!db_sql_query(ua->db, query, NULL, NULL)) {
      bsendmsg(ua, "%s", db_strerror(ua->db));
   } else {
      bsendmsg(ua, _("New max files is: %s\n"), val);
   }
   free_pool_memory(query);
}

static void update_volmaxbytes(UAContext *ua, char *val, MEDIA_DBR *mr)
{
   uint64_t maxbytes;
   char ed1[50], ed2[50];
   POOLMEM *query;

   if (!size_to_uint64(val, strlen(val), &maxbytes)) {
      bsendmsg(ua, _("Invalid max. bytes specification: %s\n"), val);
      return;
   }
   query = get_pool_memory(PM_MESSAGE);
   Mmsg(query, "UPDATE Media SET MaxVolBytes=%s WHERE MediaId=%s",
      edit_uint64(maxbytes, ed1), edit_int64(mr->MediaId, ed2));
   if (!db_sql_query(ua->db, query, NULL, NULL)) {
      bsendmsg(ua, "%s", db_strerror(ua->db));
   } else {
      bsendmsg(ua, _("New Max bytes is: %s\n"), edit_uint64(maxbytes, ed1));
   }
   free_pool_memory(query);
}

static void update_volrecycle(UAContext *ua, char *val, MEDIA_DBR *mr)
{
   int recycle;
   char ed1[50];
   POOLMEM *query;
   if (strcasecmp(val, _("yes")) == 0) {
      recycle = 1;
   } else if (strcasecmp(val, _("no")) == 0) {
      recycle = 0;
   } else {
      bsendmsg(ua, _("Invalid value. It must by yes or no.\n"));
      return;
   }
   query = get_pool_memory(PM_MESSAGE);
   Mmsg(query, "UPDATE Media SET Recycle=%d WHERE MediaId=%s",
      recycle, edit_int64(mr->MediaId, ed1));
   if (!db_sql_query(ua->db, query, NULL, NULL)) {
      bsendmsg(ua, "%s", db_strerror(ua->db));
   } else {
      bsendmsg(ua, _("New Recycle flag is: %s\n"),
         mr->Recycle==1?_("yes"):_("no"));
   }
   free_pool_memory(query);
}

/* Modify the Pool in which this Volume is located */
static void update_vol_pool(UAContext *ua, char *val, MEDIA_DBR *mr, POOL_DBR *opr)
{
   POOL_DBR pr;
   POOLMEM *query;
   char ed1[50], ed2[50];

   memset(&pr, 0, sizeof(pr));
   bstrncpy(pr.Name, val, sizeof(pr.Name));
   if (!get_pool_dbr(ua, &pr)) {
      return;
   }
   mr->PoolId = pr.PoolId;	      /* set new PoolId */
   /*
    */
   query = get_pool_memory(PM_MESSAGE);
   db_lock(ua->db);
   Mmsg(query, "UPDATE Media SET PoolId=%s WHERE MediaId=%s",
      edit_int64(mr->PoolId, ed1),
      edit_int64(mr->MediaId, ed2));
   if (!db_sql_query(ua->db, query, NULL, NULL)) {
      bsendmsg(ua, "%s", db_strerror(ua->db));
   } else {
      bsendmsg(ua, _("New Pool is: %s\n"), pr.Name);
      opr->NumVols--;
      if (!db_update_pool_record(ua->jcr, ua->db, opr)) {
         bsendmsg(ua, "%s", db_strerror(ua->db));
      }
      pr.NumVols++;
      if (!db_update_pool_record(ua->jcr, ua->db, &pr)) {
         bsendmsg(ua, "%s", db_strerror(ua->db));
      }
   }
   db_unlock(ua->db);
   free_pool_memory(query);
}

/*
 * Refresh the Volume information from the Pool record
 */
static void update_volfrompool(UAContext *ua, MEDIA_DBR *mr)
{
   POOL_DBR pr;

   memset(&pr, 0, sizeof(pr));
   pr.PoolId = mr->PoolId;
   if (!db_get_pool_record(ua->jcr, ua->db, &pr) ||
       !acl_access_ok(ua, Pool_ACL, pr.Name)) {
      return;
   }
   set_pool_dbr_defaults_in_media_dbr(mr, &pr);
   if (!db_update_media_defaults(ua->jcr, ua->db, mr)) {
      bsendmsg(ua, _("Error updating Volume record: ERR=%s"), db_strerror(ua->db));
   } else {
      bsendmsg(ua, _("Volume defaults updated from \"%s\" Pool record.\n"),
	 pr.Name);
   }
}

/*
 * Refresh the Volume information from the Pool record
 *   for all Volumes
 */
static void update_all_vols_from_pool(UAContext *ua)
{
   POOL_DBR pr;
   MEDIA_DBR mr;

   memset(&pr, 0, sizeof(pr));
   memset(&mr, 0, sizeof(mr));
   if (!get_pool_dbr(ua, &pr)) {
      return;
   }
   set_pool_dbr_defaults_in_media_dbr(&mr, &pr);
   mr.PoolId = pr.PoolId;
   if (!db_update_media_defaults(ua->jcr, ua->db, &mr)) {
      bsendmsg(ua, _("Error updating Volume records: ERR=%s"), db_strerror(ua->db));
   } else {
      bsendmsg(ua, _("All Volume defaults updated from Pool record.\n"));
   }
}


/*
 * Update a media record -- allows you to change the
 *  Volume status. E.g. if you want Bacula to stop
 *  writing on the volume, set it to anything other
 *  than Append.
 */
static int update_volume(UAContext *ua)
{
   MEDIA_DBR mr;
   POOL_DBR pr;
   POOLMEM *query;
   char ed1[130];
   bool done = false;
   const char *kw[] = {
      N_("VolStatus"),                /* 0 */
      N_("VolRetention"),             /* 1 */
      N_("VolUse"),                   /* 2 */
      N_("MaxVolJobs"),               /* 3 */
      N_("MaxVolFiles"),              /* 4 */
      N_("MaxVolBytes"),              /* 5 */
      N_("Recycle"),                  /* 6 */
      N_("Pool"),                     /* 7 */
      N_("FromPool"),                 /* 8 */
      N_("AllFromPool"),              /* 9 */
      NULL };

   for (int i=0; kw[i]; i++) {
      int j;
      POOL_DBR pr;
      if ((j=find_arg_with_value(ua, kw[i])) > 0) {
	 if (i != 9 && !select_media_dbr(ua, &mr)) {
	    return 0;
	 }
	 switch (i) {
	 case 0:
	    update_volstatus(ua, ua->argv[j], &mr);
	    break;
	 case 1:
	    update_volretention(ua, ua->argv[j], &mr);
	    break;
	 case 2:
	    update_voluseduration(ua, ua->argv[j], &mr);
	    break;
	 case 3:
	    update_volmaxjobs(ua, ua->argv[j], &mr);
	    break;
	 case 4:
	    update_volmaxfiles(ua, ua->argv[j], &mr);
	    break;
	 case 5:
	    update_volmaxbytes(ua, ua->argv[j], &mr);
	    break;
	 case 6:
	    update_volrecycle(ua, ua->argv[j], &mr);
	    break;
	 case 7:
	    memset(&pr, 0, sizeof(POOL_DBR));
	    pr.PoolId = mr.PoolId;
	    if (!db_get_pool_record(ua->jcr, ua->db, &pr)) {
               bsendmsg(ua, "%s", db_strerror(ua->db));
	       break;
	    }
	    update_vol_pool(ua, ua->argv[j], &mr, &pr);
	    break;
	 case 8:
	    update_volfrompool(ua, &mr);
	    return 1;
	 case 9:
	    update_all_vols_from_pool(ua);
	    return 1;
	 }
	 done = true;
      }
   }

   for ( ; !done; ) {
      if (!select_media_dbr(ua, &mr)) {
	 return 0;
      }
      bsendmsg(ua, _("Updating Volume \"%s\"\n"), mr.VolumeName);
      start_prompt(ua, _("Parameters to modify:\n"));
      add_prompt(ua, _("Volume Status"));
      add_prompt(ua, _("Volume Retention Period"));
      add_prompt(ua, _("Volume Use Duration"));
      add_prompt(ua, _("Maximum Volume Jobs"));
      add_prompt(ua, _("Maximum Volume Files"));
      add_prompt(ua, _("Maximum Volume Bytes"));
      add_prompt(ua, _("Recycle Flag"));
      add_prompt(ua, _("Slot"));
      add_prompt(ua, _("InChanger Flag"));
      add_prompt(ua, _("Volume Files"));
      add_prompt(ua, _("Pool"));
      add_prompt(ua, _("Volume from Pool"));
      add_prompt(ua, _("All Volumes from Pool"));
      add_prompt(ua, _("Done"));
      switch (do_prompt(ua, "", _("Select parameter to modify"), NULL, 0)) {
      case 0:			      /* Volume Status */
	 /* Modify Volume Status */
         bsendmsg(ua, _("Current Volume status is: %s\n"), mr.VolStatus);
         start_prompt(ua, _("Possible Values are:\n"));
         add_prompt(ua, "Append");      /* Better not translate these as */
         add_prompt(ua, "Archive");     /* They are known in the database code */
         add_prompt(ua, "Disabled");
         add_prompt(ua, "Full");
         add_prompt(ua, "Used");
         add_prompt(ua, "Cleaning");
         if (strcmp(mr.VolStatus, "Purged") == 0) {
            add_prompt(ua, "Recycle");
	 }
         add_prompt(ua, "Read-Only");
         if (do_prompt(ua, "", _("Choose new Volume Status"), ua->cmd, sizeof(mr.VolStatus)) < 0) {
	    return 1;
	 }
	 update_volstatus(ua, ua->cmd, &mr);
	 break;
      case 1:			      /* Retention */
         bsendmsg(ua, _("Current retention period is: %s\n"),
	    edit_utime(mr.VolRetention, ed1, sizeof(ed1)));
         if (!get_cmd(ua, _("Enter Volume Retention period: "))) {
	    return 0;
	 }
	 update_volretention(ua, ua->cmd, &mr);
	 break;

      case 2:			      /* Use Duration */
         bsendmsg(ua, _("Current use duration is: %s\n"),
	    edit_utime(mr.VolUseDuration, ed1, sizeof(ed1)));
         if (!get_cmd(ua, _("Enter Volume Use Duration: "))) {
	    return 0;
	 }
	 update_voluseduration(ua, ua->cmd, &mr);
	 break;

      case 3:			      /* Max Jobs */
         bsendmsg(ua, _("Current max jobs is: %u\n"), mr.MaxVolJobs);
         if (!get_pint(ua, _("Enter new Maximum Jobs: "))) {
	    return 0;
	 }
	 update_volmaxjobs(ua, ua->cmd, &mr);
	 break;

      case 4:			      /* Max Files */
         bsendmsg(ua, _("Current max files is: %u\n"), mr.MaxVolFiles);
         if (!get_pint(ua, _("Enter new Maximum Files: "))) {
	    return 0;
	 }
	 update_volmaxfiles(ua, ua->cmd, &mr);
	 break;

      case 5:			      /* Max Bytes */
         bsendmsg(ua, _("Current value is: %s\n"), edit_uint64(mr.MaxVolBytes, ed1));
         if (!get_cmd(ua, _("Enter new Maximum Bytes: "))) {
	    return 0;
	 }
	 update_volmaxbytes(ua, ua->cmd, &mr);
	 break;


      case 6:			      /* Recycle */
         bsendmsg(ua, _("Current recycle flag is: %s\n"),
            mr.Recycle==1?_("yes"):_("no"));
         if (!get_yesno(ua, _("Enter new Recycle status: "))) {
	    return 0;
	 }
	 update_volrecycle(ua, ua->cmd, &mr);
	 break;

      case 7:			      /* Slot */
	 int Slot;

	 memset(&pr, 0, sizeof(POOL_DBR));
	 pr.PoolId = mr.PoolId;
	 if (!db_get_pool_record(ua->jcr, ua->db, &pr)) {
            bsendmsg(ua, "%s", db_strerror(ua->db));
	    return 0;
	 }
         bsendmsg(ua, _("Current Slot is: %d\n"), mr.Slot);
         if (!get_pint(ua, _("Enter new Slot: "))) {
	    return 0;
	 }
	 Slot = ua->pint32_val;
	 if (pr.MaxVols > 0 && Slot > (int)pr.MaxVols) {
            bsendmsg(ua, _("Invalid slot, it must be between 0 and %d\n"),
	       pr.MaxVols);
	    break;
	 }
	 mr.Slot = Slot;
	 /*
	  * Make sure to use db_update... rather than doing this directly,
	  *   so that any Slot is handled correctly.
	  */
	 if (!db_update_media_record(ua->jcr, ua->db, &mr)) {
            bsendmsg(ua, _("Error updating media record Slot: ERR=%s"), db_strerror(ua->db));
	 } else {
            bsendmsg(ua, _("New Slot is: %d\n"), mr.Slot);
	 }
	 break;

      case 8:			      /* InChanger */
         bsendmsg(ua, _("Current InChanger flag is: %d\n"), mr.InChanger);
         if (!get_yesno(ua, _("Set InChanger flag? yes/no: "))) {
	    return 0;
	 }
	 mr.InChanger = ua->pint32_val;
	 /*
	  * Make sure to use db_update... rather than doing this directly,
	  *   so that any Slot is handled correctly.
	  */
	 if (!db_update_media_record(ua->jcr, ua->db, &mr)) {
            bsendmsg(ua, _("Error updating media record Slot: ERR=%s"), db_strerror(ua->db));
	 } else {
            bsendmsg(ua, _("New InChanger flag is: %d\n"), mr.InChanger);
	 }
	 break;


      case 9:			      /* Volume Files */
	 int32_t VolFiles;
         bsendmsg(ua, _("Warning changing Volume Files can result\n"
                        "in loss of data on your Volume\n\n"));
         bsendmsg(ua, _("Current Volume Files is: %u\n"), mr.VolFiles);
         if (!get_pint(ua, _("Enter new number of Files for Volume: "))) {
	    return 0;
	 }
	 VolFiles = ua->pint32_val;
	 if (VolFiles != (int)(mr.VolFiles + 1)) {
            bsendmsg(ua, _("Normally, you should only increase Volume Files by one!\n"));
            if (!get_yesno(ua, _("Continue? (yes/no): ")) || ua->pint32_val == 0) {
	       break;
	    }
	 }
	 query = get_pool_memory(PM_MESSAGE);
         Mmsg(query, "UPDATE Media SET VolFiles=%u WHERE MediaId=%s",
	    VolFiles, edit_int64(mr.MediaId, ed1));
	 if (!db_sql_query(ua->db, query, NULL, NULL)) {
            bsendmsg(ua, "%s", db_strerror(ua->db));
	 } else {
            bsendmsg(ua, _("New Volume Files is: %u\n"), VolFiles);
	 }
	 free_pool_memory(query);
	 break;

      case 10:                        /* Volume's Pool */
	 memset(&pr, 0, sizeof(POOL_DBR));
	 pr.PoolId = mr.PoolId;
	 if (!db_get_pool_record(ua->jcr, ua->db, &pr)) {
            bsendmsg(ua, "%s", db_strerror(ua->db));
	    return 0;
	 }
         bsendmsg(ua, _("Current Pool is: %s\n"), pr.Name);
         if (!get_cmd(ua, _("Enter new Pool name: "))) {
	    return 0;
	 }
	 update_vol_pool(ua, ua->cmd, &mr, &pr);
	 return 1;

      case 11:
	 update_volfrompool(ua, &mr);
	 return 1;
      case 12:
	 update_all_vols_from_pool(ua);
	 return 1;
      default:			      /* Done or error */
         bsendmsg(ua, "Selection done.\n");
	 return 1;
      }
   }
   return 1;
}

/*
 * Update pool record -- pull info from current POOL resource
 */
static int update_pool(UAContext *ua)
{
   POOL_DBR  pr;
   int id;
   POOL *pool;
   POOLMEM *query;

   pool = get_pool_resource(ua);
   if (!pool) {
      return 0;
   }

   memset(&pr, 0, sizeof(pr));
   bstrncpy(pr.Name, pool->hdr.name, sizeof(pr.Name));
   if (!get_pool_dbr(ua, &pr)) {
      return 0;
   }

   set_pooldbr_from_poolres(&pr, pool, POOL_OP_UPDATE); /* update */

   id = db_update_pool_record(ua->jcr, ua->db, &pr);
   if (id <= 0) {
      bsendmsg(ua, _("db_update_pool_record returned %d. ERR=%s\n"),
	 id, db_strerror(ua->db));
   }
   query = get_pool_memory(PM_MESSAGE);
   Mmsg(query, list_pool, pr.PoolId);
   db_list_sql_query(ua->jcr, ua->db, query, prtit, ua, 1, HORZ_LIST);
   free_pool_memory(query);
   bsendmsg(ua, _("Pool DB record updated from resource.\n"));
   return 1;
}
