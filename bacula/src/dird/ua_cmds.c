/*
 *
 *   Bacula Director -- User Agent Commands
 *
 *     Kern Sibbald, September MM
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

/* Imported subroutines */

/* Imported variables */
extern int r_first;
extern int r_last;
extern struct s_res resources[];
extern char my_name[];
extern jobq_t job_queue;	      /* job queue */

extern char *list_pool;

/* Imported functions */
extern int status_cmd(UAContext *ua, char *cmd);
extern int list_cmd(UAContext *ua, char *cmd);
extern int llist_cmd(UAContext *ua, char *cmd);
extern int show_cmd(UAContext *ua, char *cmd);
extern int messagescmd(UAContext *ua, char *cmd);
extern int autodisplaycmd(UAContext *ua, char *cmd);
extern int sqlquerycmd(UAContext *ua, char *cmd);
extern int querycmd(UAContext *ua, char *cmd);
extern int run_cmd(UAContext *ua, char *cmd);
extern int retentioncmd(UAContext *ua, char *cmd);
extern int prunecmd(UAContext *ua, char *cmd);
extern int purgecmd(UAContext *ua, char *cmd);
extern int restore_cmd(UAContext *ua, char *cmd);
extern int label_cmd(UAContext *ua, char *cmd);
extern int relabel_cmd(UAContext *ua, char *cmd);
extern int update_slots(UAContext *ua);  /* ua_label.c */

/* Forward referenced functions */
static int add_cmd(UAContext *ua, char *cmd);  
static int create_cmd(UAContext *ua, char *cmd); 
static int cancel_cmd(UAContext *ua, char *cmd); 
static int setdebug_cmd(UAContext *ua, char *cmd);
static int var_cmd(UAContext *ua, char *cmd);
static int estimate_cmd(UAContext *ua, char *cmd);
static int help_cmd(UAContext *ua, char *cmd);
static int delete_cmd(UAContext *ua, char *cmd);
static int use_cmd(UAContext *ua, char *cmd),  unmount_cmd(UAContext *ua, char *cmd);
static int version_cmd(UAContext *ua, char *cmd), automount_cmd(UAContext *ua, char *cmd);
static int time_cmd(UAContext *ua, char *cmd);
static int update_volume(UAContext *ua);
static int update_pool(UAContext *ua);
static int delete_volume(UAContext *ua);
static int delete_pool(UAContext *ua);
static int delete_job(UAContext *ua);
static int mount_cmd(UAContext *ua, char *cmd);
static int release_cmd(UAContext *ua, char *cmd);
static int update_cmd(UAContext *ua, char *cmd);
static int wait_cmd(UAContext *ua, char *cmd);

int quit_cmd(UAContext *ua, char *cmd);


struct cmdstruct { char *key; int (*func)(UAContext *ua, char *cmd); char *help; }; 
static struct cmdstruct commands[] = {
 { N_("add"),        add_cmd,         _("add media to a pool")},
 { N_("autodisplay"), autodisplaycmd, _("autodisplay [on/off] -- console messages")},
 { N_("automount"),   automount_cmd,  _("automount [on/off] -- after label")},
 { N_("cancel"),     cancel_cmd,    _("cancel job=nnn -- cancel a job")},
 { N_("create"),     create_cmd,    _("create DB Pool from resource")},  
 { N_("delete"),     delete_cmd,    _("delete [pool=<pool-name> | media volume=<volume-name>]")},    
 { N_("estimate"),   estimate_cmd,  _("performs FileSet estimate, listing gives full listing")},
 { N_("exit"),       quit_cmd,      _("exit = quit")},
 { N_("help"),       help_cmd,      _("print this command")},
 { N_("list"),       list_cmd,      _("list [pools | jobs | jobtotals | media <pool> | files jobid=<nn>]; from catalog")},
 { N_("label"),      label_cmd,     _("label a tape")},
 { N_("llist"),      llist_cmd,     _("full or long list like list command")},
 { N_("messages"),   messagescmd,   _("messages")},
 { N_("mount"),      mount_cmd,     _("mount <storage-name>")},
 { N_("prune"),      prunecmd,      _("prune expired records from catalog")},
 { N_("purge"),      purgecmd,      _("purge records from catalog")},
 { N_("quit"),       quit_cmd,      _("quit")},
 { N_("query"),      querycmd,      _("query catalog")},
 { N_("restore"),    restore_cmd,   _("restore files")},
 { N_("relabel"),    relabel_cmd,   _("relabel a tape")},
 { N_("release"),    release_cmd,   _("release <storage-name>")},
 { N_("run"),        run_cmd,       _("run <job-name>")},
 { N_("status"),     status_cmd,    _("status [storage | client]=<name>")},
 { N_("setdebug"),   setdebug_cmd,  _("sets debug level")},
 { N_("show"),       show_cmd,      _("show (resource records) [jobs | pools | ... | all]")},
 { N_("sqlquery"),   sqlquerycmd,   _("use SQL to query catalog")}, 
 { N_("time"),       time_cmd,      _("print current time")},
 { N_("unmount"),    unmount_cmd,   _("unmount <storage-name>")},
 { N_("update"),     update_cmd,    _("update Volume or Pool")},
 { N_("use"),        use_cmd,       _("use catalog xxx")},
 { N_("var"),        var_cmd,       _("does variable expansion")},
 { N_("version"),    version_cmd,   _("print Director version")},
 { N_("wait"),       wait_cmd,      _("wait until no jobs are running")},
	     };
#define comsize (sizeof(commands)/sizeof(struct cmdstruct))

/*
 * Execute a command from the UA
 */
int do_a_command(UAContext *ua, char *cmd)
{
   unsigned int i;
   int len, stat;
   int found;

   found = 0;
   stat = 1;

   Dmsg1(120, "Command: %s\n", ua->UA_sock->msg);
   if (ua->argc == 0) {
      return 1;
   }

   len = strlen(ua->argk[0]);
   for (i=0; i<comsize; i++) {	   /* search for command */
      if (strncasecmp(ua->argk[0],  _(commands[i].key), len) == 0) {
	 stat = (*commands[i].func)(ua, cmd);	/* go execute command */
	 found = 1;
	 break;
      }
   }
   if (!found) {
      pm_strcat(&ua->UA_sock->msg, _(": is an illegal command\n"));
      ua->UA_sock->msglen = strlen(ua->UA_sock->msg);
      bnet_send(ua->UA_sock);
   }
   return stat;
}

/*
 * This is a common routine used to stuff the Pool DB record defaults
 *   into the Media DB record just before creating a media (Volume) 
 *   record.
 */
void set_pool_dbr_defaults_in_media_dbr(MEDIA_DBR *mr, POOL_DBR *pr)
{
   mr->PoolId = pr->PoolId;
   bstrncpy(mr->VolStatus, "Append", sizeof(mr->VolStatus));
   mr->Recycle = pr->Recycle;
   mr->VolRetention = pr->VolRetention;
   mr->VolUseDuration = pr->VolUseDuration;
   mr->MaxVolJobs = pr->MaxVolJobs;
   mr->MaxVolFiles = pr->MaxVolFiles;
   mr->MaxVolBytes = pr->MaxVolBytes;
}


/*
 *  Add Volumes to an existing Pool
 */
static int add_cmd(UAContext *ua, char *cmd) 
{
   POOL_DBR pr;
   MEDIA_DBR mr;
   int num, i, max, startnum;
   int first_id = 0;
   char name[MAX_NAME_LENGTH];
   STORE *store;
   int Slot = 0, InChanger = 0;

   bsendmsg(ua, _(
"You probably don't want to be using this command since it\n"
"creates database records without labeling the Volumes.\n"
"You probably want to use the \"label\" command.\n\n"));

   if (!open_db(ua)) {
      return 1;
   }

   memset(&pr, 0, sizeof(pr));
   memset(&mr, 0, sizeof(mr));

   if (!get_pool_dbr(ua, &pr)) {
      return 1;
   }

   Dmsg4(120, "id=%d Num=%d Max=%d type=%s\n", pr.PoolId, pr.NumVols,
      pr.MaxVols, pr.PoolType);

   while (pr.MaxVols > 0 && pr.NumVols >= pr.MaxVols) {
      bsendmsg(ua, _("Pool already has maximum volumes = %d\n"), pr.MaxVols);
      for (;;) {
         if (!get_pint(ua, _("Enter new maximum (zero for unlimited): "))) {
	    return 1;
	 }
	 pr.MaxVols = ua->pint32_val;
      }
   }

   /* Get media type */
   if ((store = get_storage_resource(ua, 0)) != NULL) {
      bstrncpy(mr.MediaType, store->media_type, sizeof(mr.MediaType));
   } else if (!get_media_type(ua, mr.MediaType, sizeof(mr.MediaType))) {
      return 1;
   }
      
   if (pr.MaxVols == 0) {
      max = 1000;
   } else {
      max = pr.MaxVols - pr.NumVols;
   }
   for (;;) {
      char buf[100]; 
      bsnprintf(buf, sizeof(buf), _("Enter number of Volumes to create. 0=>fixed name. Max=%d: "), max);
      if (!get_pint(ua, buf)) {
	 return 1;
      }
      num = ua->pint32_val;
      if (num < 0 || num > max) {
         bsendmsg(ua, _("The number must be between 0 and %d\n"), max);
	 continue;
      }
      break;
   }
getVolName:
   if (num == 0) {
      if (!get_cmd(ua, _("Enter Volume name: "))) {
	 return 1;
      }
   } else {
      if (!get_cmd(ua, _("Enter base volume name: "))) {
	 return 1;
      }
   }
   /* Don't allow | in Volume name because it is the volume separator character */
   if (!is_volume_name_legal(ua, ua->cmd)) {
      goto getVolName;
   }
   if (strlen(ua->cmd) >= MAX_NAME_LENGTH-10) {
      bsendmsg(ua, _("Volume name too long.\n"));
      goto getVolName;
   }
   if (strlen(ua->cmd) == 0) {
      bsendmsg(ua, _("Volume name must be at least one character long.\n"));
      goto getVolName;
   }

   bstrncpy(name, ua->cmd, sizeof(name));
   if (num > 0) {
      strcat(name, "%04d");

      for (;;) {
         if (!get_pint(ua, _("Enter the starting number: "))) {
	    return 1;
	 }
	 startnum = ua->pint32_val;
	 if (startnum < 1) {
            bsendmsg(ua, _("Start number must be greater than zero.\n"));
	    continue;
	 }
	 break;
      }
   } else {
      startnum = 1;
      num = 1;
   }

   if (store && store->autochanger) {
      if (!get_pint(ua, _("Enter slot (0 for none): "))) {
	 return 1;
      }
      Slot = ua->pint32_val;
      if (!get_yesno(ua, _("InChanger? yes/no: "))) {
	 return 1;
      }
      InChanger = ua->pint32_val;
   }
	   
   set_pool_dbr_defaults_in_media_dbr(&mr, &pr);
   for (i=startnum; i < num+startnum; i++) { 
      bsnprintf(mr.VolumeName, sizeof(mr.VolumeName), name, i);
      mr.Slot = Slot++;
      mr.InChanger = InChanger;
      Dmsg1(200, "Create Volume %s\n", mr.VolumeName);
      if (!db_create_media_record(ua->jcr, ua->db, &mr)) {
	 bsendmsg(ua, db_strerror(ua->db));
	 return 1;
      }
      if (i == startnum) {
	 first_id = mr.PoolId;
      }
   }
   pr.NumVols += num;
   Dmsg0(200, "Update pool record.\n"); 
   if (db_update_pool_record(ua->jcr, ua->db, &pr) != 1) {
      bsendmsg(ua, db_strerror(ua->db));
      return 1;
   }
   bsendmsg(ua, _("%d Volumes created in pool %s\n"), num, pr.Name);

   return 1;
}

/*
 * Turn auto mount on/off  
 * 
 *  automount on 
 *  automount off
 */
int automount_cmd(UAContext *ua, char *cmd)
{
   char *onoff;

   if (ua->argc != 2) {
      if (!get_cmd(ua, _("Turn on or off? "))) {
	    return 1;
      }
      onoff = ua->cmd;
   } else {
      onoff = ua->argk[1];
   }

   ua->automount = (strcasecmp(onoff, _("off")) == 0) ? 0 : 1;
   return 1; 
}


/*
 * Cancel a job
 */
static int cancel_cmd(UAContext *ua, char *cmd)
{
   int i, ret;
   int njobs = 0;
   JCR *jcr = NULL;
   char JobName[MAX_NAME_LENGTH];

   if (!open_db(ua)) {
      return 1;
   }

   for (i=1; i<ua->argc; i++) {
      if (strcasecmp(ua->argk[i], _("jobid")) == 0) {
	 if (!ua->argv[i]) {
	    break;
	 }
	 if (!(jcr=get_jcr_by_id(atoi(ua->argv[i])))) {
            bsendmsg(ua, _("JobId %d is not running.\n"), atoi(ua->argv[i]));
	    return 1;
	 }
	 break;
      } else if (strcasecmp(ua->argk[i], _("job")) == 0) {
	 if (!ua->argv[i]) {
	    break;
	 }
	 if (!(jcr=get_jcr_by_partial_name(ua->argv[i]))) {
            bsendmsg(ua, _("Job %s is not running.\n"), ua->argv[i]);
	    return 1;
	 }
	 break;
      }
   }
   /* If we still do not have a jcr,
    *	throw up a list and ask the user to select one.
    */
   if (!jcr) {
      /* Count Jobs running */
      lock_jcr_chain();
      for (jcr=NULL; (jcr=get_next_jcr(jcr)); njobs++) {
	 if (jcr->JobId == 0) {      /* this is us */
	    free_locked_jcr(jcr);
	    njobs--;
	    continue;
	 }
	 free_locked_jcr(jcr);
      }
      unlock_jcr_chain();

      if (njobs == 0) {
         bsendmsg(ua, _("No Jobs running.\n"));
	 return 1;
      }
      start_prompt(ua, _("Select Job:\n"));
      lock_jcr_chain();
      for (jcr=NULL; (jcr=get_next_jcr(jcr)); ) {
	 if (jcr->JobId == 0) {      /* this is us */
	    free_locked_jcr(jcr);
	    continue;
	 }
	 add_prompt(ua, jcr->Job);
	 free_locked_jcr(jcr);
      }
      unlock_jcr_chain();

      if (do_prompt(ua, _("Job"),  _("Choose Job to cancel"), JobName, sizeof(JobName)) < 0) {
	 return 1;
      }
      if (njobs == 1) {
         if (!get_yesno(ua, _("Confirm cancel (yes/no): ")) || ua->pint32_val == 0) {
	    return 1;
	 }
      }
      /* NOTE! This increments the ref_count */
      jcr = get_jcr_by_full_name(JobName);
      if (!jcr) {
         bsendmsg(ua, _("Job %s not found.\n"), JobName);
	 return 1;
      }
   }

   ret = cancel_job(ua, jcr);
   free_jcr(jcr);

   return ret;
}

/*
 * This is a common routine to create or update a
 *   Pool DB base record from a Pool Resource. We handle
 *   the setting of MaxVols and NumVols slightly differently
 *   depending on if we are creating the Pool or we are
 *   simply bringing it into agreement with the resource (updage).
 */
static void set_pooldbr_from_poolres(POOL_DBR *pr, POOL *pool, e_pool_op op)
{
   strcpy(pr->PoolType, pool->pool_type);
   if (op == POOL_OP_CREATE) {
      pr->MaxVols = pool->max_volumes;
      pr->NumVols = 0;
   } else {	     /* update pool */
      if (pr->MaxVols != pool->max_volumes) {
	 pr->MaxVols = pool->max_volumes;
      }
      if (pr->MaxVols != 0 && pr->MaxVols < pr->NumVols) {
	 pr->MaxVols = pr->NumVols;
      }
   }
   pr->UseOnce = pool->use_volume_once;
   pr->UseCatalog = pool->use_catalog;
   pr->AcceptAnyVolume = pool->accept_any_volume;
   pr->Recycle = pool->Recycle;
   pr->VolRetention = pool->VolRetention;
   pr->VolUseDuration = pool->VolUseDuration;
   pr->MaxVolJobs = pool->MaxVolJobs;
   pr->MaxVolFiles = pool->MaxVolFiles;
   pr->MaxVolBytes = pool->MaxVolBytes;
   pr->AutoPrune = pool->AutoPrune;
   pr->Recycle = pool->Recycle;
   if (pool->label_format) {
      strcpy(pr->LabelFormat, pool->label_format);
   } else {
      strcpy(pr->LabelFormat, "*");    /* none */
   }
}


/*
 * Create a pool record from a given Pool resource
 *   Also called from backup.c
 * Returns: -1	on error
 *	     0	record already exists
 *	     1	record created
 */

int create_pool(JCR *jcr, B_DB *db, POOL *pool, e_pool_op op)
{
   POOL_DBR  pr;

   memset(&pr, 0, sizeof(POOL_DBR));

   strcpy(pr.Name, pool->hdr.name);

   if (db_get_pool_record(jcr, db, &pr)) {
      /* Pool Exists */
      if (op == POOL_OP_UPDATE) {  /* update request */
	 set_pooldbr_from_poolres(&pr, pool, op);
	 db_update_pool_record(jcr, db, &pr);
      }
      return 0; 		      /* exists */
   }

   set_pooldbr_from_poolres(&pr, pool, op);

   if (!db_create_pool_record(jcr, db, &pr)) {
      return -1;		      /* error */
   }
   return 1;
}



/*
 * Create a Pool Record in the database.
 *  It is always created from the Resource record.
 */
static int create_cmd(UAContext *ua, char *cmd) 
{
   POOL *pool;

   if (!open_db(ua)) {
      return 1;
   }

   pool = get_pool_resource(ua);
   if (!pool) {
      return 1;
   }

   switch (create_pool(ua->jcr, ua->db, pool, POOL_OP_CREATE)) {
   case 0:
      bsendmsg(ua, _("Error: Pool %s already exists.\n\
Use update to change it.\n"), pool->hdr.name);
      break;

   case -1:
      bsendmsg(ua, db_strerror(ua->db));
      break;

   default:
     break;
   }
   bsendmsg(ua, _("Pool %s created.\n"), pool->hdr.name);
   return 1;
}




/*
 * Update a Pool Record in the database.
 *  It is always updated from the Resource record.
 *
 *    update pool=<pool-name>
 *	   updates pool from Pool resource
 *    update media pool=<pool-name> volume=<volume-name>
 *	   changes pool info for volume
 */
static int update_cmd(UAContext *ua, char *cmd) 
{
   static char *kw[] = {
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

static void update_volstatus(UAContext *ua, char *val, MEDIA_DBR *mr)
{
   POOLMEM *query = get_pool_memory(PM_MESSAGE);
   char *kw[] = {
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
      bstrncpy(mr->VolStatus, kw[i], sizeof(mr->VolStatus));
      Mmsg(&query, "UPDATE Media SET VolStatus='%s' WHERE MediaId=%u",
	 mr->VolStatus, mr->MediaId);
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
   char ed1[50];
   POOLMEM *query;
   if (!duration_to_utime(val, &mr->VolRetention)) {
      bsendmsg(ua, _("Invalid retention period specified: %s\n"), val);
      return;
   }
   query = get_pool_memory(PM_MESSAGE);
   Mmsg(&query, "UPDATE Media SET VolRetention=%s WHERE MediaId=%u",
      edit_uint64(mr->VolRetention, ed1), mr->MediaId);
   if (!db_sql_query(ua->db, query, NULL, NULL)) {  
      bsendmsg(ua, "%s", db_strerror(ua->db));
   } else {
      bsendmsg(ua, _("New retention seconds is: %s\n"),
	 edit_utime(mr->VolRetention, ed1));
   }
   free_pool_memory(query);
}

static void update_voluseduration(UAContext *ua, char *val, MEDIA_DBR *mr)
{
   char ed1[50];
   POOLMEM *query;

   if (!duration_to_utime(val, &mr->VolUseDuration)) {
      bsendmsg(ua, _("Invalid use duration specified: %s\n"), val);
      return;
   }
   query = get_pool_memory(PM_MESSAGE);
   Mmsg(&query, "UPDATE Media SET VolUseDuration=%s WHERE MediaId=%u",
      edit_uint64(mr->VolUseDuration, ed1), mr->MediaId);
   if (!db_sql_query(ua->db, query, NULL, NULL)) {  
      bsendmsg(ua, "%s", db_strerror(ua->db));
   } else {
      bsendmsg(ua, _("New use duration is: %s\n"),
	 edit_utime(mr->VolUseDuration, ed1));
   }
   free_pool_memory(query);
}

static void update_volmaxjobs(UAContext *ua, char *val, MEDIA_DBR *mr)
{
   POOLMEM *query = get_pool_memory(PM_MESSAGE);
   Mmsg(&query, "UPDATE Media SET MaxVolJobs=%s WHERE MediaId=%u",
      val, mr->MediaId);
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
   Mmsg(&query, "UPDATE Media SET MaxVolFiles=%s WHERE MediaId=%u",
      val, mr->MediaId);
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
   char ed1[50];
   POOLMEM *query;

   if (!size_to_uint64(val, strlen(val), &maxbytes)) {
      bsendmsg(ua, _("Invalid max. bytes specification: %s\n"), val);
      return;
   } 
   query = get_pool_memory(PM_MESSAGE);
   Mmsg(&query, "UPDATE Media SET MaxVolBytes=%s WHERE MediaId=%u",
      edit_uint64(maxbytes, ed1), mr->MediaId);
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
   Mmsg(&query, "UPDATE Media SET Recycle=%d WHERE MediaId=%u",
      recycle, mr->MediaId);
   if (!db_sql_query(ua->db, query, NULL, NULL)) {  
      bsendmsg(ua, "%s", db_strerror(ua->db));
   } else {	  
      bsendmsg(ua, _("New Recycle flag is: %s\n"),
         mr->Recycle==1?_("yes"):_("no"));
   }
   free_pool_memory(query);
}

/* Modify the Pool in which this Volume is located */
static void update_volpool(UAContext *ua, char *val, MEDIA_DBR *mr)
{
   POOL_DBR pr;

   memset(&pr, 0, sizeof(pr));
   bstrncpy(pr.Name, val, sizeof(pr.Name));
   if (!get_pool_dbr(ua, &pr)) {
      return;
   }
   mr->PoolId = pr.PoolId;	      /* set new PoolId */
   /*
    * Make sure to use db_update... rather than doing this directly,
    *	so that any Slot is handled correctly. 
    */
   if (!db_update_media_record(ua->jcr, ua->db, mr)) {
      bsendmsg(ua, _("Error updating media record Pool: ERR=%s"), db_strerror(ua->db));
   } else {
      bsendmsg(ua, _("New Pool is: %s\n"), pr.Name);
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
   char ed1[30];
   bool done = false;
   char *kw[] = {
      N_("VolStatus"),                /* 0 */
      N_("VolRetention"),             /* 1 */
      N_("VolUse"),                   /* 2 */
      N_("MaxVolJobs"),               /* 3 */
      N_("MaxVolFiles"),              /* 4 */
      N_("MaxVolBytes"),              /* 5 */
      N_("Recycle"),                  /* 6 */
      N_("Pool"),                     /* 7 */
      NULL };

   for (int i=0; kw[i]; i++) {
      int j;
      if ((j=find_arg_with_value(ua, kw[i])) > 0) {
	 if (!select_media_dbr(ua, &mr)) {
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
	    update_volpool(ua, ua->argv[j], &mr);
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
         bsendmsg(ua, _("Current retention seconds is: %s\n"),
	    edit_utime(mr.VolRetention, ed1));
         if (!get_cmd(ua, _("Enter Volume Retention period: "))) {
	    return 0;
	 }
	 update_volretention(ua, ua->cmd, &mr);
	 break;

      case 2:			      /* Use Duration */
         bsendmsg(ua, _("Current use duration is: %s\n"),
	    edit_utime(mr.VolUseDuration, ed1));
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
            bsendmsg(ua, _("New Slot is: %s\n"), mr.Slot);
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
            bsendmsg(ua, _("New InChanger flag is: %s\n"), mr.InChanger);
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
         Mmsg(&query, "UPDATE Media SET VolFiles=%u WHERE MediaId=%u",
	    VolFiles, mr.MediaId);
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
	 update_volpool(ua, ua->cmd, &mr);
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
   strcpy(pr.Name, pool->hdr.name);
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
   Mmsg(&query, list_pool, pr.PoolId);
   db_list_sql_query(ua->jcr, ua->db, query, prtit, ua, 1, HORZ_LIST);
   free_pool_memory(query);
   bsendmsg(ua, _("Pool DB record updated from resource.\n"));
   return 1;
}


static void do_storage_setdebug(UAContext *ua, STORE *store, int level)
{
   BSOCK *sd;

   ua->jcr->store = store;
   /* Try connecting for up to 15 seconds */
   bsendmsg(ua, _("Connecting to Storage daemon %s at %s:%d\n"), 
      store->hdr.name, store->address, store->SDport);
   if (!connect_to_storage_daemon(ua->jcr, 1, 15, 0)) {
      bsendmsg(ua, _("Failed to connect to Storage daemon.\n"));
      return;
   }
   Dmsg0(120, _("Connected to storage daemon\n"));
   sd = ua->jcr->store_bsock;
   bnet_fsend(sd, "setdebug=%d\n", level);
   if (bnet_recv(sd) >= 0) {
      bsendmsg(ua, "%s", sd->msg);
   }
   bnet_sig(sd, BNET_TERMINATE);
   bnet_close(sd);
   ua->jcr->store_bsock = NULL;
   return;  
}
   
static void do_client_setdebug(UAContext *ua, CLIENT *client, int level)
{
   BSOCK *fd;

   /* Connect to File daemon */

   ua->jcr->client = client;
   /* Try to connect for 15 seconds */
   bsendmsg(ua, _("Connecting to Client %s at %s:%d\n"), 
      client->hdr.name, client->address, client->FDport);
   if (!connect_to_file_daemon(ua->jcr, 1, 15, 0)) {
      bsendmsg(ua, _("Failed to connect to Client.\n"));
      return;
   }
   Dmsg0(120, "Connected to file daemon\n");
   fd = ua->jcr->file_bsock;
   bnet_fsend(fd, "setdebug=%d\n", level);
   if (bnet_recv(fd) >= 0) {
      bsendmsg(ua, "%s", fd->msg);
   }
   bnet_sig(fd, BNET_TERMINATE);
   bnet_close(fd);
   ua->jcr->file_bsock = NULL;
   return;  
}


static void do_all_setdebug(UAContext *ua, int level)
{
   STORE *store, **unique_store;
   CLIENT *client, **unique_client;
   int i, j, found;

   /* Director */
   debug_level = level;

   /* Count Storage items */
   LockRes();
   store = NULL;
   for (i=0; (store = (STORE *)GetNextRes(R_STORAGE, (RES *)store)); i++)
      { }
   unique_store = (STORE **) malloc(i * sizeof(STORE));
   /* Find Unique Storage address/port */	  
   store = (STORE *)GetNextRes(R_STORAGE, NULL);
   i = 0;
   unique_store[i++] = store;
   while ((store = (STORE *)GetNextRes(R_STORAGE, (RES *)store))) {
      found = 0;
      for (j=0; j<i; j++) {
	 if (strcmp(unique_store[j]->address, store->address) == 0 &&
	     unique_store[j]->SDport == store->SDport) {
	    found = 1;
	    break;
	 }
      }
      if (!found) {
	 unique_store[i++] = store;
         Dmsg2(140, "Stuffing: %s:%d\n", store->address, store->SDport);
      }
   }
   UnlockRes();

   /* Call each unique Storage daemon */
   for (j=0; j<i; j++) {
      do_storage_setdebug(ua, unique_store[j], level);
   }
   free(unique_store);

   /* Count Client items */
   LockRes();
   client = NULL;
   for (i=0; (client = (CLIENT *)GetNextRes(R_CLIENT, (RES *)client)); i++)
      { }
   unique_client = (CLIENT **) malloc(i * sizeof(CLIENT));
   /* Find Unique Client address/port */	 
   client = (CLIENT *)GetNextRes(R_CLIENT, NULL);
   i = 0;
   unique_client[i++] = client;
   while ((client = (CLIENT *)GetNextRes(R_CLIENT, (RES *)client))) {
      found = 0;
      for (j=0; j<i; j++) {
	 if (strcmp(unique_client[j]->address, client->address) == 0 &&
	     unique_client[j]->FDport == client->FDport) {
	    found = 1;
	    break;
	 }
      }
      if (!found) {
	 unique_client[i++] = client;
         Dmsg2(140, "Stuffing: %s:%d\n", client->address, client->FDport);
      }
   }
   UnlockRes();

   /* Call each unique File daemon */
   for (j=0; j<i; j++) {
      do_client_setdebug(ua, unique_client[j], level);
   }
   free(unique_client);
}

/*
 * setdebug level=nn all
 */
static int setdebug_cmd(UAContext *ua, char *cmd)
{
   STORE *store;
   CLIENT *client;
   int level;
   int i;

   if (!open_db(ua)) {
      return 1;
   }
   Dmsg1(120, "setdebug:%s:\n", cmd);

   level = -1;
   i = find_arg_with_value(ua, _("level"));
   if (i >= 0) {
      level = atoi(ua->argv[i]);
   }
   if (level < 0) {
      if (!get_pint(ua, _("Enter new debug level: "))) {
	 return 1;
      }
      level = ua->pint32_val;
   }

   /* General debug? */
   for (i=1; i<ua->argc; i++) {
      if (strcasecmp(ua->argk[i], _("all")) == 0) {
	 do_all_setdebug(ua, level);
	 return 1;
      }
      if (strcasecmp(ua->argk[i], _("dir")) == 0 ||
          strcasecmp(ua->argk[i], _("director")) == 0) {
	 debug_level = level;
	 return 1;
      }
      if (strcasecmp(ua->argk[i], _("client")) == 0 ||
          strcasecmp(ua->argk[i], _("fd")) == 0) {
	 client = NULL;
	 if (ua->argv[i]) {
	    client = (CLIENT *)GetResWithName(R_CLIENT, ua->argv[i]);
	    if (client) {
	       do_client_setdebug(ua, client, level);
	       return 1;
	    }
	 }
	 client = select_client_resource(ua);	
	 if (client) {
	    do_client_setdebug(ua, client, level);
	    return 1;
	 }
      }

      if (strcasecmp(ua->argk[i], _("store")) == 0 ||
          strcasecmp(ua->argk[i], _("storage")) == 0 ||
          strcasecmp(ua->argk[i], _("sd")) == 0) {
	 store = NULL;
	 if (ua->argv[i]) {
	    store = (STORE *)GetResWithName(R_STORAGE, ua->argv[i]);
	    if (store) {
	       do_storage_setdebug(ua, store, level);
	       return 1;
	    }
	 }
	 store = get_storage_resource(ua, 0);
	 if (store) {
	    do_storage_setdebug(ua, store, level);
	    return 1;
	 }
      }
   } 
   /*
    * We didn't find an appropriate keyword above, so
    * prompt the user.
    */
   start_prompt(ua, _("Available daemons are: \n"));
   add_prompt(ua, _("Director"));
   add_prompt(ua, _("Storage"));
   add_prompt(ua, _("Client"));
   add_prompt(ua, _("All"));
   switch(do_prompt(ua, "", _("Select daemon type to set debug level"), NULL, 0)) {
   case 0:			   /* Director */
      debug_level = level;
      break;
   case 1:
      store = get_storage_resource(ua, 0);
      if (store) {
	 do_storage_setdebug(ua, store, level);
      }
      break;
   case 2:
      client = select_client_resource(ua);
      if (client) {
	 do_client_setdebug(ua, client, level);
      }
      break;
   case 3:
      do_all_setdebug(ua, level);
      break;
   default:
      break;
   }
   return 1;
}

static int var_cmd(UAContext *ua, char *cmd)
{
   POOLMEM *val = get_pool_memory(PM_FNAME);
   char *var;

   if (!open_db(ua)) {
      return 1;
   }
   for (var=ua->cmd; *var != ' '; ) {    /* skip command */
      var++;
   }
   while (*var == ' ') {                 /* skip spaces */
      var++;
   }
   Dmsg1(100, "Var=%s:\n", var);
   variable_expansion(ua->jcr, var, &val);  
   bsendmsg(ua, "%s\n", val);
   free_pool_memory(val);
   return 1;
}

static int estimate_cmd(UAContext *ua, char *cmd)
{
   JOB *job = NULL;
   CLIENT *client = NULL;
   FILESET *fileset = NULL;
   int listing = 0;
   BSOCK *fd;
   char since[MAXSTRING];

   for (int i=1; i<ua->argc; i++) {
      if (strcasecmp(ua->argk[i], _("client")) == 0 ||
          strcasecmp(ua->argk[i], _("fd")) == 0) {
	 if (ua->argv[i]) {
	    client = (CLIENT *)GetResWithName(R_CLIENT, ua->argv[i]);
	    continue;
	 }
      }
      if (strcasecmp(ua->argk[i], _("job")) == 0) {
	 if (ua->argv[i]) {
	    job = (JOB *)GetResWithName(R_JOB, ua->argv[i]);
	    continue;
	 }
      }
      if (strcasecmp(ua->argk[i], _("fileset")) == 0) {
	 if (ua->argv[i]) {
	    fileset = (FILESET *)GetResWithName(R_FILESET, ua->argv[i]);
	    continue;
	 }
      }
      if (strcasecmp(ua->argk[i], _("listing")) == 0) {
	 listing = 1;
      }
   } 
   if (!job && !(client && fileset)) {
      if (!(job = select_job_resource(ua))) {
	 return 1;
      }
   }
   if (!job) {
      job = (JOB *)GetResWithName(R_JOB, ua->argk[1]);
   }
   if (!client) {
      client = job->client;
   }
   if (!fileset) {
      fileset = job->fileset;
   }
   ua->jcr->client = client;
   ua->jcr->fileset = fileset;
   close_db(ua);
   ua->catalog = client->catalog;

   if (!open_db(ua)) {
      return 1;
   }

   get_level_since_time(ua->jcr, since, sizeof(since));

   bsendmsg(ua, _("Connecting to Client %s at %s:%d\n"),
      job->client->hdr.name, job->client->address, job->client->FDport);
   if (!connect_to_file_daemon(ua->jcr, 1, 15, 0)) {
      bsendmsg(ua, _("Failed to connect to Client.\n"));
      return 1;
   }
   fd = ua->jcr->file_bsock;

   if (!send_include_list(ua->jcr)) {
      bsendmsg(ua, _("Error sending include list.\n"));
      return 1;
   }

   if (!send_exclude_list(ua->jcr)) {
      bsendmsg(ua, _("Error sending exclude list.\n"));
      return 1;
   }

   if (!send_level_command(ua->jcr)) {
      return 1;
   }

   bnet_fsend(fd, "estimate listing=%d\n", listing);
   while (bnet_recv(fd) >= 0) {
      bsendmsg(ua, "%s", fd->msg);
   }

   bnet_sig(fd, BNET_TERMINATE);
   bnet_close(fd);
   ua->jcr->file_bsock = NULL;

   return 1;
}


/*
 * print time
 */
static int time_cmd(UAContext *ua, char *cmd)
{
   char sdt[50];
   time_t ttime = time(NULL);
   struct tm tm;
   localtime_r(&ttime, &tm);
   strftime(sdt, sizeof(sdt), "%d-%b-%Y %H:%M:%S", &tm);
   bsendmsg(ua, "%s\n", sdt);
   return 1;
}



/*
 * Delete Pool records (should purge Media with it).
 *
 *  delete pool=<pool-name>
 *  delete media pool=<pool-name> volume=<name>
 */
static int delete_cmd(UAContext *ua, char *cmd)
{
   static char *keywords[] = {
      N_("volume"),
      N_("pool"),
      N_("job"),
      NULL};

   if (!open_db(ua)) {
      return 1;
   }

     
   switch (find_arg_keyword(ua, keywords)) {
   case 0:
      delete_volume(ua);     
      return 1;
   case 1:
      delete_pool(ua);
      return 1;
   case 2:
      delete_job(ua);
      return 1;
   default:
      break;
   }

   bsendmsg(ua, _(
"In general it is not a good idea to delete either a\n"
"Pool or a Volume since they may contain data.\n\n"));

   switch (do_keyword_prompt(ua, _("Choose catalog item to delete"), keywords)) {
   case 0:
      delete_volume(ua);
      break;
   case 1:
      delete_pool(ua);
      break;
   case 2:
      delete_job(ua);
      return 1;
   default:
      bsendmsg(ua, _("Nothing done.\n"));
      break;
   }
   return 1;
}

static int delete_job(UAContext *ua)
{
   POOLMEM *query = get_pool_memory(PM_MESSAGE);
   JobId_t JobId;

   int i = find_arg_with_value(ua, "jobid");
   if (i >= 0) {
      JobId = str_to_int64(ua->argv[i]);
   } else if (!get_pint(ua, _("Enter JobId to delete: "))) {
      return 0;
   } else {
      JobId = ua->pint32_val; 
   }
   Mmsg(&query, "DELETE FROM Job WHERE JobId=%u", JobId);
   db_sql_query(ua->db, query, NULL, (void *)NULL);
   Mmsg(&query, "DELETE FROM File WHERE JobId=%u", JobId);
   db_sql_query(ua->db, query, NULL, (void *)NULL);
   Mmsg(&query, "DELETE FROM JobMedia WHERE JobId=%u", JobId);
   db_sql_query(ua->db, query, NULL, (void *)NULL);
   free_pool_memory(query);
   bsendmsg(ua, _("Job %u and associated records deleted from the catalog.\n"), JobId);
   return 1;
}

/*
 * Delete media records from database -- dangerous 
 */
static int delete_volume(UAContext *ua)
{
   MEDIA_DBR mr;

   if (!select_media_dbr(ua, &mr)) {
      return 1;
   }
   bsendmsg(ua, _("\nThis command will delete volume %s\n"
      "and all Jobs saved on that volume from the Catalog\n"),
      mr.VolumeName);

   if (!get_yesno(ua, _("Are you sure you want to delete this Volume? (yes/no): "))) {
      return 1;
   }
   if (ua->pint32_val) {
      db_delete_media_record(ua->jcr, ua->db, &mr);
   }
   return 1;
}

/*
 * Delete a pool record from the database -- dangerous	 
 */
static int delete_pool(UAContext *ua)
{
   POOL_DBR  pr;
   
   memset(&pr, 0, sizeof(pr));

   if (!get_pool_dbr(ua, &pr)) {
      return 1;
   }
   if (!get_yesno(ua, _("Are you sure you want to delete this Pool? (yes/no): "))) {
      return 1;
   }
   if (ua->pint32_val) {
      db_delete_pool_record(ua->jcr, ua->db, &pr);
   }
   return 1;
}


static void do_mount_cmd(UAContext *ua, char *command)
{
   STORE *store;
   BSOCK *sd;
   char dev_name[MAX_NAME_LENGTH];


   if (!open_db(ua)) {
      return;
   }
   Dmsg2(120, "%s: %s\n", command, ua->UA_sock->msg);

   store = get_storage_resource(ua, 1);
   if (!store) {
      return;
   }

   Dmsg2(120, "Found storage, MediaType=%s DevName=%s\n",
      store->media_type, store->dev_name);

   ua->jcr->store = store;
   if (!connect_to_storage_daemon(ua->jcr, 10, SDConnectTimeout, 1)) {
      bsendmsg(ua, _("Failed to connect to Storage daemon.\n"));
      return;
   }
   sd = ua->jcr->store_bsock;
   strcpy(dev_name, store->dev_name);
   bash_spaces(dev_name);
   bnet_fsend(sd, "%s %s", command, dev_name);
   while (bnet_recv(sd) >= 0) {
      bsendmsg(ua, "%s", sd->msg);
   }
   bnet_sig(sd, BNET_TERMINATE);
   bnet_close(sd);
   ua->jcr->store_bsock = NULL;
}

/*
 * mount [storage | device] <name>
 */
static int mount_cmd(UAContext *ua, char *cmd)
{
   do_mount_cmd(ua, "mount");          /* mount */
   return 1;
}


/*
 * unmount [storage | device] <name>
 */
static int unmount_cmd(UAContext *ua, char *cmd)
{
   do_mount_cmd(ua, "unmount");          /* unmount */
   return 1;
}


/*
 * release [storage | device] <name>
 */
static int release_cmd(UAContext *ua, char *cmd)
{
   do_mount_cmd(ua, "release");          /* release */
   return 1;
}


/*
 * Switch databases
 *   use catalog=<name>
 */
static int use_cmd(UAContext *ua, char *cmd)
{
   CAT *oldcatalog, *catalog;


   close_db(ua);		      /* close any previously open db */
   oldcatalog = ua->catalog;

   if (!(catalog = get_catalog_resource(ua))) {
      ua->catalog = oldcatalog;
   } else {
      ua->catalog = catalog;
   }
   if (open_db(ua)) {
      bsendmsg(ua, _("Using Catalog name=%s DB=%s\n"),
	 ua->catalog->hdr.name, ua->catalog->db_name);
   }
   return 1;
}

int quit_cmd(UAContext *ua, char *cmd) 
{
   ua->quit = TRUE;
   return 1;
}

/*
 * Wait until no job is running 
 */
int wait_cmd(UAContext *ua, char *cmd) 
{
   bmicrosleep(0, 200000);	      /* let job actually start */
   for (int running=1; running; ) {
      running = 0;
      lock_jcr_chain();
      for (JCR *jcr=NULL; (jcr=get_next_jcr(jcr)); ) {
	 if (jcr->JobId != 0) {
	    running = 1;
	    free_locked_jcr(jcr);
	    break;
	 }
	 free_locked_jcr(jcr);
      }
      unlock_jcr_chain();
      if (running) {
	 bmicrosleep(1, 0);
      }
   }
   return 1;
}


static int help_cmd(UAContext *ua, char *cmd)
{
   unsigned int i;

   bsendmsg(ua, _("  Command    Description\n  =======    ===========\n"));
   for (i=0; i<comsize; i++) {
      bsendmsg(ua, _("  %-10s %s\n"), _(commands[i].key), _(commands[i].help));
   }
   bsendmsg(ua, _("\nWhen at a prompt, entering a period cancels the command.\n\n"));
   return 1;
}

static int version_cmd(UAContext *ua, char *cmd)
{
   bsendmsg(ua, "%s Version: " VERSION " (" BDATE ")\n", my_name);
   return 1;
}


/* A bit brain damaged in that if the user has not done
 * a "use catalog xxx" command, we simply find the first
 * catalog resource and open it.
 */
int open_db(UAContext *ua)
{
   if (ua->db) {
      return 1;
   }
   if (!ua->catalog) {
      LockRes();
      ua->catalog = (CAT *)GetNextRes(R_CATALOG, NULL);
      UnlockRes();
      if (!ua->catalog) {    
         bsendmsg(ua, _("Could not find a Catalog resource\n"));
	 return 0;
      } else {
         bsendmsg(ua, _("Using default Catalog name=%s DB=%s\n"), 
	    ua->catalog->hdr.name, ua->catalog->db_name);
      }
   }

   ua->jcr->catalog = ua->catalog;

   Dmsg0(150, "Open database\n");
   ua->db = db_init_database(ua->jcr, ua->catalog->db_name, ua->catalog->db_user,
			     ua->catalog->db_password, ua->catalog->db_address,
			     ua->catalog->db_port, ua->catalog->db_socket);
   if (!ua->db || !db_open_database(ua->jcr, ua->db)) {
      bsendmsg(ua, _("Could not open database \"%s\".\n"),
		 ua->catalog->db_name);
      if (ua->db) {
         bsendmsg(ua, "%s", db_strerror(ua->db));
      }
      close_db(ua);
      return 0;
   }
   ua->jcr->db = ua->db;
   Dmsg1(150, "DB %s opened\n", ua->catalog->db_name);
   return 1;
}

void close_db(UAContext *ua)
{
   if (ua->db) {
      db_close_database(ua->jcr, ua->db);
      ua->db = NULL;
      if (ua->jcr) {
	 ua->jcr->db = NULL;
      }
   }
}
