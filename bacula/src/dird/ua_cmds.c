/*
 *
 *   Bacula Director -- User Agent Commands
 *
 *     Kern Sibbald, September MM
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

#include "bacula.h"
#include "dird.h"
#include "ua.h"

/* Imported subroutines */
extern void run_job(JCR *jcr);

/* Imported variables */
extern int r_first;
extern int r_last;
extern struct s_res resources[];
extern char my_name[];

/* Imported functions */
extern int statuscmd(UAContext *ua, char *cmd);
extern int listcmd(UAContext *ua, char *cmd);
extern int showcmd(UAContext *ua, char *cmd);
extern int messagescmd(UAContext *ua, char *cmd);
extern int autodisplaycmd(UAContext *ua, char *cmd);
extern int sqlquerycmd(UAContext *ua, char *cmd);
extern int querycmd(UAContext *ua, char *cmd);
extern int runcmd(UAContext *ua, char *cmd);
extern int retentioncmd(UAContext *ua, char *cmd);
extern int prunecmd(UAContext *ua, char *cmd);
extern int purgecmd(UAContext *ua, char *cmd);
extern int restorecmd(UAContext *ua, char *cmd);

/* Forward referenced functions */
static int addcmd(UAContext *ua, char *cmd),  createcmd(UAContext *ua, char *cmd), cancelcmd(UAContext *ua, char *cmd);
static int setdebugcmd(UAContext *ua, char *cmd);
static int helpcmd(UAContext *ua, char *cmd);
static int deletecmd(UAContext *ua, char *cmd);
static int usecmd(UAContext *ua, char *cmd),  unmountcmd(UAContext *ua, char *cmd);
static int labelcmd(UAContext *ua, char *cmd), mountcmd(UAContext *ua, char *cmd), updatecmd(UAContext *ua, char *cmd);
static int versioncmd(UAContext *ua, char *cmd), automountcmd(UAContext *ua, char *cmd);
static int update_volume(UAContext *ua);
static int update_pool(UAContext *ua);
static int delete_volume(UAContext *ua);
static int delete_pool(UAContext *ua);

int quitcmd(UAContext *ua, char *cmd);


struct cmdstruct { char *key; int (*func)(UAContext *ua, char *cmd); char *help; }; 
static struct cmdstruct commands[] = {
 { N_("add"),        addcmd,       _("add media to a pool")},
 { N_("autodisplay"), autodisplaycmd, _("autodisplay [on/off] -- console messages")},
 { N_("automount"),   automountcmd,   _("automount [on/off] -- after label")},
 { N_("cancel"),     cancelcmd,    _("cancel job=nnn -- cancel a job")},
 { N_("create"),     createcmd,    _("create DB Pool from resource")},  
 { N_("delete"),     deletecmd,    _("delete [pool=<pool-name> | media volume=<volume-name>]")},    
 { N_("help"),       helpcmd,      _("print this command")},
 { N_("label"),      labelcmd,     _("label a tape")},
 { N_("list"),       listcmd,      _("list [pools | jobs | jobtotals | media <pool> | files job=<nn>]; from catalog")},
 { N_("messages"),   messagescmd,  _("messages")},
 { N_("mount"),      mountcmd,     _("mount <storage-name>")},
 { N_("restore"),    restorecmd,   _("restore files")},
 { N_("prune"),      prunecmd,     _("prune expired records from catalog")},
 { N_("purge"),      purgecmd,     _("purge records from catalog")},
 { N_("run"),        runcmd,       _("run <job-name>")},
 { N_("setdebug"),   setdebugcmd,  _("sets debug level")},
 { N_("show"),       showcmd,      _("show (resource records) [jobs | pools | ... | all]")},
 { N_("sqlquery"),   sqlquerycmd,  _("use SQL to query catalog")}, 
 { N_("status"),     statuscmd,    _("status [storage | client]=<name>")},
 { N_("unmount"),    unmountcmd,   _("unmount <storage-name>")},
 { N_("update"),     updatecmd,    _("update Volume or Pool")},
 { N_("use"),        usecmd,       _("use catalog xxx")},
 { N_("version"),    versioncmd,   _("print Director version")},
 { N_("quit"),       quitcmd,      _("quit")},
 { N_("query"),      querycmd,     _("query catalog")},
 { N_("exit"),       quitcmd,      _("exit = quit")},
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
   for (i=0; i<comsize; i++)	   /* search for command */
      if (strncasecmp(ua->argk[0],  _(commands[i].key), len) == 0) {
	 stat = (*commands[i].func)(ua, cmd);	/* go execute command */
	 found = 1;
	 break;
      }
   if (!found) {
      strcat(ua->UA_sock->msg, _(": is an illegal command\n"));
      ua->UA_sock->msglen = strlen(ua->UA_sock->msg);
      bnet_send(ua->UA_sock);
   }
   return stat;
}


/*
 *  Add Volumes to an existing Pool
 *
 */
static int addcmd(UAContext *ua, char *cmd) 
{
   POOL_DBR pr;
   MEDIA_DBR mr;
   int num, i, max, startnum;
   int first_id = 0;
   char name[MAX_NAME_LENGTH];

   bsendmsg(ua, _(
"You probably don't want to be using this command since it\n"
"creates database records without labeling the Volumes.\n"
"You probably want to use the label command.\n\n"));

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
         if (!get_cmd(ua, _("Enter new maximum (zero for unlimited): "))) {
	    return 1;
	 }
	 pr.MaxVols = atoi(ua->cmd);
	 if (pr.MaxVols < 0) {
            bsendmsg(ua, _("Max vols must be zero or greater.\n"));
	    continue;
	 }
	 break;
      }
   }

   if (!get_media_type(ua, mr.MediaType)) {
      return 1;
   }

   if (pr.MaxVols == 0) {
      max = 1000;
   } else {
      max = pr.MaxVols - pr.NumVols;
   }
   for (;;) {
      char buf[100]; 
      sprintf(buf, _("Enter number of Volumes to create. 0=>fixed name. Max=%d: "), max);
      if (!get_cmd(ua, buf)) {
	 return 1;
      }
      num = atoi(ua->cmd);
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
   if (strchr(ua->cmd, '|')) {
      bsendmsg(ua, _("Illegal character | in a volume name.\n"));
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

   strcpy(name, ua->cmd);
   if (num > 0) {
      strcat(name, "%04d");

      for (;;) {
         if (!get_cmd(ua, _("Enter the starting number: "))) {
	    return 1;
	 }
	 startnum = atoi(ua->cmd);
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
	   
   mr.PoolId = pr.PoolId;
   strcpy(mr.VolStatus, "Append");
   mr.Recycle = pr.Recycle;
   mr.VolRetention = pr.VolRetention;
   for (i=startnum; i < num+startnum; i++) { 
      sprintf(mr.VolumeName, name, i);
      Dmsg1(200, "Create Volume %s\n", mr.VolumeName);
      if (!db_create_media_record(ua->db, &mr)) {
	 bsendmsg(ua, db_strerror(ua->db));
	 return 1;
      }
      if (i == startnum) {
	 first_id = mr.PoolId;
      }
   }
   pr.NumVols += num;
   Dmsg0(200, "Update pool record.\n"); 
   if (db_update_pool_record(ua->db, &pr) != 1) {
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
int automountcmd(UAContext *ua, char *cmd)
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
static int cancelcmd(UAContext *ua, char *cmd)
{
   int i;
   int njobs = 0;
   BSOCK *sd, *fd;
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

      if (do_prompt(ua, _("Choose Job to cancel"), JobName) < 0) {
	 return 1;
      }
      if (njobs == 1) {
         if (!get_cmd(ua, _("Confirm cancel (yes/no): "))) {
	    return 1;
	 }
         if (strcasecmp(ua->cmd, _("yes")) != 0) {
	    return 1;
	 }
      }
      jcr = get_jcr_by_full_name(JobName);
      if (!jcr) {
         bsendmsg(ua, _("Job %s not found.\n"), JobName);
	 return 1;
      }
   }
     
   switch (jcr->JobStatus) {
   case JS_Created:
      jcr->JobStatus = JS_Cancelled;
      bsendmsg(ua, _("JobId %d, Job %s marked to be cancelled.\n"),
	      jcr->JobId, jcr->Job);
      free_jcr(jcr);
      return 1;
	 
   default:

      jcr->JobStatus = JS_Cancelled;
      /* Cancel File daemon */
      ua->jcr->client = jcr->client;
      if (!connect_to_file_daemon(ua->jcr, 10, FDConnectTimeout, 1)) {
         bsendmsg(ua, _("Failed to connect to File daemon.\n"));
	 free_jcr(jcr);
	 return 1;
      }
      Dmsg0(200, "Connected to file daemon\n");
      fd = ua->jcr->file_bsock;
      bnet_fsend(fd, "cancel Job=%s\n", jcr->Job);
      while (bnet_recv(fd) > 0) {
         bsendmsg(ua, "%s", fd->msg);
      }
      bnet_sig(fd, BNET_TERMINATE);
      bnet_close(fd);
      ua->jcr->file_bsock = NULL;

      /* Cancel Storage daemon */
      ua->jcr->store = jcr->store;
      if (!connect_to_storage_daemon(ua->jcr, 10, SDConnectTimeout, 1)) {
         bsendmsg(ua, _("Failed to connect to Storage daemon.\n"));
	 free_jcr(jcr);
	 return 1;
      }
      Dmsg0(200, "Connected to storage daemon\n");
      sd = ua->jcr->store_bsock;
      bnet_fsend(sd, "cancel Job=%s\n", jcr->Job);
      while (bnet_recv(sd) > 0) {
         bsendmsg(ua, "%s", sd->msg);
      }
      bnet_sig(sd, BNET_TERMINATE);
      bnet_close(sd);
      ua->jcr->store_bsock = NULL;

   }
   free_jcr(jcr);

   return 1; 
}

/*
 * Create a pool record from a given Pool resource
 *   Also called from backup.c
 * Returns: -1	on error
 *	     0	record already exists
 *	     1	record created
 */

int create_pool(B_DB *db, POOL *pool)
{
   POOL_DBR  pr;

   memset(&pr, 0, sizeof(POOL_DBR));

   strcpy(pr.Name, pool->hdr.name);

   if (db_get_pool_record(db, &pr)) {
      return 0; 		      /* exists */
   }

   strcpy(pr.PoolType, pool->pool_type);
   pr.MaxVols = pool->max_volumes;
   pr.NumVols = 0;
   pr.UseOnce = pool->use_volume_once;
   pr.UseCatalog = pool->use_catalog;
   pr.AcceptAnyVolume = pool->accept_any_volume;
   pr.Recycle = pool->Recycle;
   pr.VolRetention = pool->VolRetention;
   pr.AutoPrune = pool->AutoPrune;
   if (pool->label_format) {
      strcpy(pr.LabelFormat, pool->label_format);
   } else {
      strcpy(pr.LabelFormat, "*");    /* none */
   }

   if (!db_create_pool_record(db, &pr)) {
      return -1;		      /* error */
   }
   return 1;
}



/*
 * Create a Pool Record in the database.
 *  It is always created from the Resource record.
 */
static int createcmd(UAContext *ua, char *cmd) 
{
   POOL *pool;

   if (!open_db(ua)) {
      return 1;
   }

   pool = get_pool_resource(ua);
   if (!pool) {
      return 1;
   }

   switch (create_pool(ua->db, pool)) {
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
static int updatecmd(UAContext *ua, char *cmd) 
{
   static char *kw[] = {
      N_("media"),
      N_("volume"),
      N_("pool"),
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
      default:
	 break;
   }
    
   start_prompt(ua, _("Update choice:\n"));
   add_prompt(ua, _("Volume parameters"));
   add_prompt(ua, _("Pool from resource"));
   switch (do_prompt(ua, _("Choose catalog item to update"), NULL)) {
      case 0:
	 update_volume(ua);
	 break;
      case 1:
	 update_pool(ua);
	 break;
      default:
	 break;
   }
   return 1;
}

/*
 * Update a media record -- allows you to change the
 *  Volume status. E.g. if you want Bacula to stop
 *  writing on the volume, set it to anything other
 *  than Append.
 */		 
static int update_volume(UAContext *ua)
{
   POOL_DBR pr;
   MEDIA_DBR mr;
   POOLMEM *query;
   char ed1[30];

   if (!select_pool_and_media_dbr(ua, &pr, &mr)) {
      return 0;
   }

   for (int done=0; !done; ) {
      if (!db_get_media_record(ua->db, &mr)) {
	 if (mr.MediaId != 0) {
            bsendmsg(ua, _("Volume record for MediaId %d not found.\n"), mr.MediaId);
	 } else {
            bsendmsg(ua, _("Volume record for %s not found.\n"), mr.VolumeName);
	 }
	 return 0;
      }
      bsendmsg(ua, _("Updating Volume %s\n"), mr.VolumeName);
      start_prompt(ua, _("Parameters to modify:\n"));
      add_prompt(ua, _("Volume Status"));
      add_prompt(ua, _("Volume Retention Period"));
      add_prompt(ua, _("Recycle Flag"));
      add_prompt(ua, _("Slot"));
      add_prompt(ua, _("Done"));
      switch (do_prompt(ua, _("Select parameter to modify"), NULL)) {
      case 0:			      /* Volume Status */
	 /* Modify Volume Status */
         bsendmsg(ua, _("Current value is: %s\n"), mr.VolStatus);
         start_prompt(ua, _("Possible Values are:\n"));
         add_prompt(ua, "Append");
         add_prompt(ua, "Archive");
         add_prompt(ua, "Disabled");
         add_prompt(ua, "Full");
         if (strcmp(mr.VolStatus, "Purged") == 0) {
            add_prompt(ua, "Recycle");
	 }
         add_prompt(ua, "Read-Only");
         if (do_prompt(ua, _("Choose new Volume Status"), ua->cmd) < 0) {
	    return 1;
	 }
	 strcpy(mr.VolStatus, ua->cmd);
	 query = get_pool_memory(PM_MESSAGE);
         Mmsg(&query, "UPDATE Media SET VolStatus='%s' WHERE MediaId=%d",
	    mr.VolStatus, mr.MediaId);
	 if (!db_sql_query(ua->db, query, NULL, NULL)) {  
            bsendmsg(ua, "%s", db_strerror(ua->db));
	 }	 
	 free_pool_memory(query);
	 break;
      case 1:			      /* Retention */
         bsendmsg(ua, _("Current value is: %s\n"),
	    edit_btime(mr.VolRetention, ed1));
         if (!get_cmd(ua, _("Enter Volume Retention period: "))) {
	    return 0;
	 }
	 if (!string_to_btime(ua->cmd, &mr.VolRetention)) {
            bsendmsg(ua, _("Invalid retention period specified.\n"));
	    break;
	 }
	 query = get_pool_memory(PM_MESSAGE);
         Mmsg(&query, "UPDATE Media SET VolRetention=%s WHERE MediaId=%d",
	    edit_uint64(mr.VolRetention, ed1), mr.MediaId);
	 if (!db_sql_query(ua->db, query, NULL, NULL)) {  
            bsendmsg(ua, "%s", db_strerror(ua->db));
	 }	 
	 free_pool_memory(query);
	 break;
      case 2:			      /* Recycle */
	 int recycle;
         bsendmsg(ua, _("Current value is: %s\n"),
            mr.Recycle==1?_("yes"):_("no"));
         if (!get_cmd(ua, _("Enter new Recycle status: "))) {
	    return 0;
	 }
         if (strcasecmp(ua->cmd, _("yes")) == 0) {
	    recycle = 1;
         } else if (strcasecmp(ua->cmd, _("no")) == 0) {
	    recycle = 0;
	 } else {
            bsendmsg(ua, _("Invalid recycle status specified.\n"));
	    break;
	 }
	 query = get_pool_memory(PM_MESSAGE);
         Mmsg(&query, "UPDATE Media SET Recycle=%d WHERE MediaId=%d",
	    recycle, mr.MediaId);
	 if (!db_sql_query(ua->db, query, NULL, NULL)) {  
            bsendmsg(ua, "%s", db_strerror(ua->db));
	 }	 
	 free_pool_memory(query);
	 break;

      case 3:			      /* Slot */
	 int slot;
         bsendmsg(ua, _("Current value is: %d\n"), mr.Slot);
         if (!get_cmd(ua, _("Enter new Slot: "))) {
	    return 0;
	 }
	 slot = atoi(ua->cmd);
	 if (slot < 0) {
            bsendmsg(ua, _("Invalid slot, it must be 0 or greater\n"));
	    break;
	 } else if (pr.MaxVols > 0 && slot >(int)pr.MaxVols) {
            bsendmsg(ua, _("Invalid slot, it must be between 0 and %d\n"),
	       pr.MaxVols);
	    break;
	 }
	 query = get_pool_memory(PM_MESSAGE);
         Mmsg(&query, "UPDATE Media SET Slot=%d WHERE MediaId=%d",
	    slot, mr.MediaId);
	 if (!db_sql_query(ua->db, query, NULL, NULL)) {  
            bsendmsg(ua, "%s", db_strerror(ua->db));
	 } else {
            bsendmsg(ua, "New value is: %d\n", slot);
	 }
	 free_pool_memory(query);
	 break;

	 
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
   
   
   pool = get_pool_resource(ua);
   if (!pool) {
      return 0;
   }

   memset(&pr, 0, sizeof(pr));
   strcpy(pr.Name, pool->hdr.name);
   if (!get_pool_dbr(ua, &pr)) {
      return 0;
   }
   strcpy(pr.PoolType, pool->pool_type);
   if (pr.MaxVols != (uint32_t) (pool->max_volumes)) {
      pr.MaxVols = pool->max_volumes;
   }
   if (pr.MaxVols != 0 && pr.MaxVols < pr.NumVols) {
      pr.MaxVols = pr.NumVols;
   }
   pr.UseOnce = pool->use_volume_once;
   pr.UseCatalog = pool->use_catalog;
   pr.AcceptAnyVolume = pool->accept_any_volume;
   if (pool->label_format) {
      strcpy(pr.LabelFormat, pool->label_format);
   } else {
      strcpy(pr.LabelFormat, "*");    /* none */
   }
   id = db_update_pool_record(ua->db, &pr);
   if (id <= 0) {
      bsendmsg(ua, _("db_update_pool_record returned %d. ERR=%s\n"),
	 id, db_strerror(ua->db));
   }
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
   if (bnet_recv(sd) > 0) {
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
   if (bnet_recv(fd) > 0) {
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
static int setdebugcmd(UAContext *ua, char *cmd)
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
   for (i=1; i<ua->argc; i++) {
      if (strcasecmp(ua->argk[i], _("level")) == 0 && ua->argv[i]) {
	 level = atoi(ua->argv[i]);
	 break;
      }
   }
   if (level < 0) {
      if (!get_cmd(ua, _("Enter new debug level: "))) {
	 return 1;
      }
      level = atoi(ua->cmd);
   }
   if (level < 0) {
      bsendmsg(ua, _("level cannot be negative.\n"));
      return 1;
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
      if (strcasecmp(ua->argk[i], _("client")) == 0) {
	 client = NULL;
	 if (ua->argv[i]) {
	    client = (CLIENT *) GetResWithName(R_CLIENT, ua->argv[i]);
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

	 store = get_storage_resource(ua, cmd);
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
   switch(do_prompt(ua, _("Select daemon type to set debug level"), NULL)) {
      case 0:			      /* Director */
	 debug_level = level;
	 break;
      case 1:
	 store = get_storage_resource(ua, cmd);
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



/*
 * Delete Pool records (should purge Media with it).
 *
 *  delete pool=<pool-name>
 *  delete media pool=<pool-name> volume=<name>
 */
static int deletecmd(UAContext *ua, char *cmd)
{
   static char *keywords[] = {
      N_("volume"),
      N_("pool"),
      NULL};

   if (!open_db(ua)) {
      return 1;
   }

   bsendmsg(ua, _(
"In general it is not a good idea to delete either a\n"
"Pool or a Volume since they may contain data.\n\n"));
     
   switch (find_arg_keyword(ua, keywords)) {
      case 0:
	 delete_volume(ua);	
	 return 1;
      case 1:
	 delete_pool(ua);
	 return 1;
      default:
	 break;
   }
   switch (do_keyword_prompt(ua, _("Choose catalog item to delete"), keywords)) {
      case 0:
	 delete_volume(ua);
	 break;
      case 1:
	 delete_pool(ua);
	 break;
      default:
         bsendmsg(ua, _("Nothing done.\n"));
	 break;
   }
   return 1;
}

/*
 * Delete media records from database -- dangerous 
 */
static int delete_volume(UAContext *ua)
{
   POOL_DBR pr;
   MEDIA_DBR mr;

   if (!select_pool_and_media_dbr(ua, &pr, &mr)) {
      return 1;
   }
   bsendmsg(ua, _("\nThis command will delete volume %s\n"
      "and all Jobs saved on that volume from the Catalog\n"),
      mr.VolumeName);

   if (!get_cmd(ua, _("Are you sure you want to delete this Volume? (yes/no): "))) {
      return 1;
   }
   if (strcasecmp(ua->cmd, _("yes")) == 0) {
      db_delete_media_record(ua->db, &mr);
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
   if (!get_cmd(ua, _("Are you sure you want to delete this Pool? (yes/no): "))) {
      return 1;
   }
   if (strcasecmp(ua->cmd, _("yes")) == 0) {
      db_delete_pool_record(ua->db, &pr);
   }
   return 1;
}


/*
 * Label a tape 
 *  
 *   label storage=xxx volume=vvv
 */
static int labelcmd(UAContext *ua, char *cmd)
{
   STORE *store;
   BSOCK *sd;
   char dev_name[MAX_NAME_LENGTH];
   MEDIA_DBR mr;
   POOL_DBR pr;
   int ok = FALSE;
   int mounted = FALSE;
   int i;
   static char *keyword[] = {
      "volume",
      NULL};

   if (!open_db(ua)) {
      return 1;
   }
   store = get_storage_resource(ua, cmd);
   if (!store) {
      return 1;
   }

   i = find_arg_keyword(ua, keyword);
   if (i >=0 && ua->argv[i]) {
      strcpy(ua->cmd, ua->argv[i]);
      goto gotVol;
   }

getVol:
   if (!get_cmd(ua, _("Enter new Volume name: "))) {
      return 1;
   }
gotVol:
   if (strchr(ua->cmd, '|')) {
      bsendmsg(ua, _("Illegal character | in a volume name.\n"));
      goto getVol;
   }
   if (strlen(ua->cmd) >= MAX_NAME_LENGTH) {
      bsendmsg(ua, _("Volume name too long.\n"));
      goto getVol;
   }
   if (strlen(ua->cmd) == 0) {
      bsendmsg(ua, _("Volume name must be at least one character long.\n"));
      goto getVol;
   }

   memset(&mr, 0, sizeof(mr));
   strcpy(mr.VolumeName, ua->cmd);
   if (db_get_media_record(ua->db, &mr)) {
       bsendmsg(ua, _("Media record for Volume %s already exists.\n"), 
	  mr.VolumeName);
       return 1;
   }
   strcpy(mr.MediaType, store->media_type);

   memset(&pr, 0, sizeof(pr));
   if (!select_pool_dbr(ua, &pr)) {
      return 1;
   }
   mr.PoolId = pr.PoolId;
   strcpy(mr.VolStatus, "Append");
   mr.Recycle = pr.Recycle;
   mr.VolRetention = pr.VolRetention;

   ua->jcr->store = store;
   bsendmsg(ua, _("Connecting to Storage daemon %s at %s:%d ...\n"), 
      store->hdr.name, store->address, store->SDport);
   if (!connect_to_storage_daemon(ua->jcr, 10, SDConnectTimeout, 1)) {
      bsendmsg(ua, _("Failed to connect to Storage daemon.\n"));
      return 1;   
   }
   sd = ua->jcr->store_bsock;
   strcpy(dev_name, store->dev_name);
   bash_spaces(dev_name);
   bash_spaces(mr.VolumeName);
   bash_spaces(mr.MediaType);
   bash_spaces(pr.Name);
   bnet_fsend(sd, _("label %s VolumeName=%s PoolName=%s MediaType=%s"), 
      dev_name, mr.VolumeName, pr.Name, mr.MediaType);
   bsendmsg(ua, "Sending label command ...\n");
   while (bget_msg(sd, 0) > 0) {
      bsendmsg(ua, "%s", sd->msg);
      if (strncmp(sd->msg, "3000 OK label.", 14) == 0) {
	 ok = TRUE;
      }
   }
   ua->jcr->store_bsock = NULL;
   unbash_spaces(dev_name);
   unbash_spaces(mr.VolumeName);
   unbash_spaces(mr.MediaType);
   unbash_spaces(pr.Name);
   if (ok) {
      if (db_create_media_record(ua->db, &mr)) {
         bsendmsg(ua, _("Media record for Volume=%s successfully created.\n"),
	    mr.VolumeName);
	 if (ua->automount) {
            bsendmsg(ua, _("Requesting mount %s ...\n"), dev_name);
	    bash_spaces(dev_name);
            bnet_fsend(sd, "mount %s", dev_name);
	    unbash_spaces(dev_name);
	    while (bnet_recv(sd) > 0) {
               bsendmsg(ua, "%s", sd->msg);
	       /* Here we can get
		*  3001 OK mount. Device=xxx	  or
		*  3001 Mounted Volume vvvv
		*/
               if (strncmp(sd->msg, "3001 ", 5) == 0) {
		  mounted = TRUE;
		  /***** ****FIXME***** find job waiting for  
		   ***** mount, and change to waiting for SD  
		   */
	       }
	    }
	 }
      } else {
         bsendmsg(ua, "%s", db_strerror(ua->db));
      }
   }
   if (!mounted) {
      bsendmsg(ua, _("Do not forget to mount the drive!!!\n"));
   }
   bnet_sig(sd, BNET_TERMINATE);
   bnet_close(sd);
   return 1;
}

static void do_mount_cmd(int mount, UAContext *ua, char *cmd)
{
   STORE *store;
   BSOCK *sd;
   char dev_name[MAX_NAME_LENGTH];


   if (!open_db(ua)) {
      return;
   }
   Dmsg1(120, "mount: %s\n", ua->UA_sock->msg);

   store = get_storage_resource(ua, cmd);
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
   if (mount) {
      bnet_fsend(sd, "mount %s", dev_name);
   } else {
      bnet_fsend(sd, "unmount %s", dev_name);
   }
   while (bnet_recv(sd) > 0) {
      bsendmsg(ua, "%s", sd->msg);
      if (strncmp(sd->msg, "3001 OK mount.", 14) == 0) {
	  /***** ****FIXME**** fix JobStatus */
      }
   }
   bnet_sig(sd, BNET_TERMINATE);
   bnet_close(sd);
   ua->jcr->store_bsock = NULL;
}

/*
 * mount [storage | device] <name>
 */
static int mountcmd(UAContext *ua, char *cmd)
{
   do_mount_cmd(1, ua, cmd);	      /* mount */
   return 1;
}


/*
 * unmount [storage | device] <name>
 */
static int unmountcmd(UAContext *ua, char *cmd)
{
   do_mount_cmd(0, ua, cmd);	      /* unmount */
   return 1;
}


/*
 * Switch databases
 *   use catalog=<name>
 */
static int usecmd(UAContext *ua, char *cmd)
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

int quitcmd(UAContext *ua, char *cmd) 
{
   ua->quit = TRUE;
   return 1;
}

static int helpcmd(UAContext *ua, char *cmd)
{
   unsigned int i;

/* usage(); */
   bsendmsg(ua, _("  Command    Description\n  =======    ===========\n"));
   for (i=0; i<comsize; i++) {
      bsendmsg(ua, _("  %-10s %s\n"), _(commands[i].key), _(commands[i].help));
   }
   bsendmsg(ua, "\n");
   return 1;
}

static int versioncmd(UAContext *ua, char *cmd)
{
   bsendmsg(ua, "%s Version: " VERSION " (" DATE ")\n", my_name);
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
         bnet_fsend(ua->UA_sock, _("Could not find a Catalog resource\n"));
	 return 0;
      } else {
         bnet_fsend(ua->UA_sock, _("Using default Catalog name=%s DB=%s\n"), 
	    ua->catalog->hdr.name, ua->catalog->db_name);
      }
   }

   Dmsg0(150, "Open database\n");
   ua->db = db_init_database(NULL, ua->catalog->db_name, ua->catalog->db_user,
			     ua->catalog->db_password);
   if (!db_open_database(ua->db)) {
      bnet_fsend(ua->UA_sock, _("Could not open DB %s: ERR=%s"), 
	 ua->catalog->db_name, db_strerror(ua->db));
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
      db_close_database(ua->db);
   }
   ua->db = NULL;
   ua->jcr->db = NULL;
}
