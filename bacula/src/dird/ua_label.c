/*
 *
 *   Bacula Director -- Tape labeling commands
 *
 *     Kern Sibbald, April MMIII
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

/* Slot list definition */
typedef struct s_vol_list {
   struct s_vol_list *next;
   char *VolName;
   int Slot;
} vol_list_t;


/* Forward referenced functions */
static int do_label(UAContext *ua, char *cmd, int relabel);
static void label_from_barcodes(UAContext *ua);
static int send_label_request(UAContext *ua, MEDIA_DBR *mr, MEDIA_DBR *omr, 
	       POOL_DBR *pr, int relabel);
static vol_list_t *get_slot_list_from_SD(UAContext *ua);
static int is_cleaning_tape(UAContext *ua, MEDIA_DBR *mr, POOL_DBR *pr);


/*
 * Label a tape 
 *  
 *   label storage=xxx volume=vvv
 */
int labelcmd(UAContext *ua, char *cmd)
{
   return do_label(ua, cmd, 0);       /* standard label */
}

int relabelcmd(UAContext *ua, char *cmd)
{
   return do_label(ua, cmd, 1);      /* relabel tape */
}


/*
 * Update Slots corresponding to Volumes in autochanger 
 */
int update_slots(UAContext *ua)
{
   STORE *store;
   vol_list_t *vl, *vol_list = NULL;
   MEDIA_DBR mr;

   if (!open_db(ua)) {
      return 1;
   }
   store = get_storage_resource(ua, 1);
   if (!store) {
      return 1;
   }
   ua->jcr->store = store;

   vol_list = get_slot_list_from_SD(ua);


   if (!vol_list) {
      bsendmsg(ua, _("No Volumes found to label, or no barcodes.\n"));
      goto bail_out;
   }

   /* Walk through the list updating the media records */
   for (vl=vol_list; vl; vl=vl->next) {

      memset(&mr, 0, sizeof(mr));
      bstrncpy(mr.VolumeName, vl->VolName, sizeof(mr.VolumeName));
      if (db_get_media_record(ua->jcr, ua->db, &mr)) {
	  if (mr.Slot != vl->Slot) {
	     mr.Slot = vl->Slot;
	     if (!db_update_media_record(ua->jcr, ua->db, &mr)) {
                bsendmsg(ua, _("%s\n"), db_strerror(ua->db));
	     } else {
		bsendmsg(ua, _(
                  "Catalog record for Volume \"%s\" updated to reference slot %d.\n"),
		  mr.VolumeName, mr.Slot);
	     }
	  } else {
             bsendmsg(ua, _("Catalog record for Volume \"%s\" is up to date.\n"),
		mr.VolumeName);
	  }   
	  continue;
      } else {
          bsendmsg(ua, _("Record for Volume \"%s\" not found in catalog.\n"), 
	     mr.VolumeName);
      }
   }


bail_out:
   /* Free list */
   for (vl=vol_list; vl; ) {
      vol_list_t *ovl;
      free(vl->VolName);
      ovl = vl;
      vl = vl->next;
      free(ovl);
   }

   if (ua->jcr->store_bsock) {
      bnet_sig(ua->jcr->store_bsock, BNET_TERMINATE);
      bnet_close(ua->jcr->store_bsock);
      ua->jcr->store_bsock = NULL;
   }
   return 1;
}

/*
 * Common routine for both label and relabel
 */
static int do_label(UAContext *ua, char *cmd, int relabel)
{
   STORE *store;
   BSOCK *sd;
   sd = ua->jcr->store_bsock;
   char dev_name[MAX_NAME_LENGTH];
   MEDIA_DBR mr, omr;
   POOL_DBR pr;
   int ok = FALSE;
   int mounted = FALSE;
   int i;
   static char *barcode_keyword[] = {
      "barcode",
      "barcodes",
      NULL};


   memset(&pr, 0, sizeof(pr));
   if (!open_db(ua)) {
      return 1;
   }
   store = get_storage_resource(ua, 1);
   if (!store) {
      return 1;
   }
   ua->jcr->store = store;

   if (!relabel && find_arg_keyword(ua, barcode_keyword) >= 0) {
      label_from_barcodes(ua);
      return 1;
   }

   /* If relabel get name of Volume to relabel */
   if (relabel) {
      /* Check for volume=OldVolume */
      i = find_arg_with_value(ua, "volume"); 
      if (i >= 0) {
	 memset(&omr, 0, sizeof(omr));
	 bstrncpy(omr.VolumeName, ua->argv[i], sizeof(omr.VolumeName));
	 if (db_get_media_record(ua->jcr, ua->db, &omr)) {
	    goto checkVol;
	 } 
         bsendmsg(ua, "%s", db_strerror(ua->db));
      }
      /* No keyword or Vol not found, ask user to select */
      if (!select_media_dbr(ua, &omr)) {
	 return 1;
      }

      /* Require Volume to be Purged */
checkVol:
      if (strcmp(omr.VolStatus, "Purged") != 0) {
         bsendmsg(ua, _("Volume \"%s\" has VolStatus %s. It must be purged before relabeling.\n"),
	    omr.VolumeName, omr.VolStatus);
	 return 1;
      }
   }

   /* Check for name=NewVolume */
   i = find_arg_with_value(ua, "name");
   if (i >= 0) {
      pm_strcpy(&ua->cmd, ua->argv[i]);
      goto checkName;
   }

   /* Get a new Volume name */
   for ( ;; ) {
      if (!get_cmd(ua, _("Enter new Volume name: "))) {
	 return 1;
      }
checkName:
      if (!is_volume_name_legal(ua, ua->cmd)) {
	 continue;
      }

      memset(&mr, 0, sizeof(mr));
      bstrncpy(mr.VolumeName, ua->cmd, sizeof(mr.VolumeName));
      if (db_get_media_record(ua->jcr, ua->db, &mr)) {
          bsendmsg(ua, _("Media record for new Volume \"%s\" already exists.\n"), 
	     mr.VolumeName);
	  continue;
      }
      break;			      /* Got it */
   }

   /* If autochanger, request slot */
   if (store->autochanger) {
      i = find_arg_with_value(ua, "slot"); 
      if (i >= 0) {
	 mr.Slot = atoi(ua->argv[i]);
      } else if (!get_pint(ua, _("Enter slot (0 for none): "))) {
	 return 1;
      } else {
	 mr.Slot = ua->pint32_val;
      }
   }

   bstrncpy(mr.MediaType, store->media_type, sizeof(mr.MediaType));

   /* Must select Pool if not already done */
   if (pr.PoolId == 0) {
      memset(&pr, 0, sizeof(pr));
      if (!select_pool_dbr(ua, &pr)) {
	 return 1;
      }
   }

   bsendmsg(ua, _("Connecting to Storage daemon %s at %s:%d ...\n"), 
      store->hdr.name, store->address, store->SDport);
   if (!connect_to_storage_daemon(ua->jcr, 10, SDConnectTimeout, 1)) {
      bsendmsg(ua, _("Failed to connect to Storage daemon.\n"));
      return 1;   
   }
   sd = ua->jcr->store_bsock;

   ok = send_label_request(ua, &mr, &omr, &pr, relabel);

   if (ok) {
      if (relabel) {
	 if (!db_delete_media_record(ua->jcr, ua->db, &omr)) {
            bsendmsg(ua, _("Delete of Volume \"%s\" failed. ERR=%s"),
	       omr.VolumeName, db_strerror(ua->db));
	 } else {
            bsendmsg(ua, _("Old volume \"%s\" deleted from catalog.\n"), 
	       omr.VolumeName);
	 }
      }
      if (ua->automount) {
	 strcpy(dev_name, store->dev_name);
         bsendmsg(ua, _("Requesting mount %s ...\n"), dev_name);
	 bash_spaces(dev_name);
         bnet_fsend(sd, "mount %s", dev_name);
	 unbash_spaces(dev_name);
	 while (bnet_recv(sd) >= 0) {
            bsendmsg(ua, "%s", sd->msg);
	    /* Here we can get
	     *	3001 OK mount. Device=xxx      or
	     *	3001 Mounted Volume vvvv
	     */
            mounted = strncmp(sd->msg, "3001 ", 5) == 0;
	 }
      }
   }
   if (!mounted) {
      bsendmsg(ua, _("Do not forget to mount the drive!!!\n"));
   }
   bnet_sig(sd, BNET_TERMINATE);
   bnet_close(sd);
   ua->jcr->store_bsock = NULL;

   return 1;
}

/*
 * Request SD to send us the slot:barcodes, then wiffle
 *  through them all labeling them.
 */
static void label_from_barcodes(UAContext *ua)
{
   STORE *store = ua->jcr->store;
   POOL_DBR pr;
   MEDIA_DBR mr, omr;
   vol_list_t *vl, *vol_list = NULL;

   vol_list = get_slot_list_from_SD(ua);

   if (!vol_list) {
      bsendmsg(ua, _("No Volumes found to label, or no barcodes.\n"));
      goto bail_out;
   }

   /* Display list of Volumes and ask if he really wants to proceed */
   bsendmsg(ua, _("The following Volumes will be labeled:\n"
                  "Slot  Volume\n"
                  "==============\n"));
   for (vl=vol_list; vl; vl=vl->next) {
      bsendmsg(ua, "%4d  %s\n", vl->Slot, vl->VolName);
   }
   if (!get_cmd(ua, _("Do you want to continue? (y/n): ")) ||
       (ua->cmd[0] != 'y' && ua->cmd[0] != 'Y')) {
      goto bail_out;
   }
   /* Select a pool */
   memset(&pr, 0, sizeof(pr));
   if (!select_pool_dbr(ua, &pr)) {
      goto bail_out;
   }
   memset(&omr, 0, sizeof(omr));

   /* Fire off the label requests */
   for (vl=vol_list; vl; vl=vl->next) {

      memset(&mr, 0, sizeof(mr));
      bstrncpy(mr.VolumeName, vl->VolName, sizeof(mr.VolumeName));
      if (db_get_media_record(ua->jcr, ua->db, &mr)) {
          bsendmsg(ua, _("Media record for Slot %d Volume \"%s\" already exists.\n"), 
	     vl->Slot, mr.VolumeName);
	  continue;
      }
      if (is_cleaning_tape(ua, &mr, &pr)) {
	 set_pool_dbr_defaults_in_media_dbr(&mr, &pr);
	 if (db_create_media_record(ua->jcr, ua->db, &mr)) {
            bsendmsg(ua, _("Catalog record for cleaning tape \"%s\" successfully created.\n"),
	       mr.VolumeName);
	 } else {
            bsendmsg(ua, "Catalog error on cleaning tape: %s", db_strerror(ua->db));
	 }
	 continue;
      }
      bstrncpy(mr.MediaType, store->media_type, sizeof(mr.MediaType));
      if (ua->jcr->store_bsock) {
	 bnet_sig(ua->jcr->store_bsock, BNET_TERMINATE);
	 bnet_close(ua->jcr->store_bsock);
	 ua->jcr->store_bsock = NULL;
      }
      bsendmsg(ua, _("Connecting to Storage daemon %s at %s:%d ...\n"), 
	 store->hdr.name, store->address, store->SDport);
      if (!connect_to_storage_daemon(ua->jcr, 10, SDConnectTimeout, 1)) {
         bsendmsg(ua, _("Failed to connect to Storage daemon.\n"));
	 goto bail_out;
      }

      mr.Slot = vl->Slot;
      send_label_request(ua, &mr, &omr, &pr, 0);
   }


bail_out:
   /* Free list */
   for (vl=vol_list; vl; ) {
      vol_list_t *ovl;
      free(vl->VolName);
      ovl = vl;
      vl = vl->next;
      free(ovl);
   }

   if (ua->jcr->store_bsock) {
      bnet_sig(ua->jcr->store_bsock, BNET_TERMINATE);
      bnet_close(ua->jcr->store_bsock);
      ua->jcr->store_bsock = NULL;
   }

   return;
}

/* 
 * Check if the Volume name has legal characters
 * If ua is non-NULL send the message
 */
int is_volume_name_legal(UAContext *ua, char *name)
{
   int len;
   char *p;
   char *accept = ":.-_";

   /* Restrict the characters permitted in the Volume name */
   for (p=name; *p; p++) {
      if (B_ISALPHA(*p) || B_ISDIGIT(*p) || strchr(accept, (int)(*p))) {
	 continue;
      }
      if (ua) {
         bsendmsg(ua, _("Illegal character \"%c\" in a volume name.\n"), *p);
      }
      return 0;
   }
   len = strlen(name);
   if (len >= MAX_NAME_LENGTH) {
      if (ua) {
         bsendmsg(ua, _("Volume name too long.\n"));
      }
      return 0;
   }
   if (len == 0) {
      if (ua) {
         bsendmsg(ua, _("Volume name must be at least one character long.\n"));
      }
      return 0;
   }
   return 1;
}

static int send_label_request(UAContext *ua, MEDIA_DBR *mr, MEDIA_DBR *omr, 
			      POOL_DBR *pr, int relabel)
{
   BSOCK *sd;
   char dev_name[MAX_NAME_LENGTH];
   int ok = FALSE;

   sd = ua->jcr->store_bsock;
   bstrncpy(dev_name, ua->jcr->store->dev_name, sizeof(dev_name));
   bash_spaces(dev_name);
   bash_spaces(mr->VolumeName);
   bash_spaces(mr->MediaType);
   bash_spaces(pr->Name);
   if (relabel) {
      bash_spaces(omr->VolumeName);
      bnet_fsend(sd, _("relabel %s OldName=%s NewName=%s PoolName=%s MediaType=%s Slot=%d"), 
	 dev_name, omr->VolumeName, mr->VolumeName, pr->Name, mr->MediaType, mr->Slot);
      bsendmsg(ua, _("Sending relabel command from \"%s\" to \"%s\" ...\n"),
	 omr->VolumeName, mr->VolumeName);
   } else {
      bnet_fsend(sd, _("label %s VolumeName=%s PoolName=%s MediaType=%s Slot=%d"), 
	 dev_name, mr->VolumeName, pr->Name, mr->MediaType, mr->Slot);
      bsendmsg(ua, _("Sending label command for Volume \"%s\" Slot %d ...\n"), 
	 mr->VolumeName, mr->Slot);
      Dmsg5(200, "label %s VolumeName=%s PoolName=%s MediaType=%s Slot=%d\n", 
	 dev_name, mr->VolumeName, pr->Name, mr->MediaType, mr->Slot);
   }

   while (bnet_recv(sd) >= 0) {
      bsendmsg(ua, "%s", sd->msg);
      if (strncmp(sd->msg, "3000 OK label.", 14) == 0) {
	 ok = TRUE;
      } 
   }
   unbash_spaces(mr->VolumeName);
   unbash_spaces(mr->MediaType);
   unbash_spaces(pr->Name);
   mr->LabelDate = time(NULL);
   if (ok) {
      set_pool_dbr_defaults_in_media_dbr(mr, pr);
      if (db_create_media_record(ua->jcr, ua->db, mr)) {
         bsendmsg(ua, _("Catalog record for Volume \"%s\", Slot %d  successfully created.\n"),
	    mr->VolumeName, mr->Slot);
      } else {
         bsendmsg(ua, "%s", db_strerror(ua->db));
	 ok = FALSE;
      }
   } else {
      bsendmsg(ua, _("Label command failed.\n"));
   }
   return ok;
}

static vol_list_t *get_slot_list_from_SD(UAContext *ua)
{
   STORE *store = ua->jcr->store;
   char dev_name[MAX_NAME_LENGTH];
   BSOCK *sd;
   vol_list_t *vl;
   vol_list_t *vol_list = NULL;


   bsendmsg(ua, _("Connecting to Storage daemon %s at %s:%d ...\n"), 
      store->hdr.name, store->address, store->SDport);
   if (!connect_to_storage_daemon(ua->jcr, 10, SDConnectTimeout, 1)) {
      bsendmsg(ua, _("Failed to connect to Storage daemon.\n"));
      return NULL;
   }
   sd  = ua->jcr->store_bsock;

   bstrncpy(dev_name, store->dev_name, sizeof(dev_name));
   bash_spaces(dev_name);
   /* Ask for autochanger list of volumes */
   bnet_fsend(sd, _("autochanger list %s \n"), dev_name);

   /* Read and organize list of Volumes */
   while (bnet_recv(sd) >= 0) {
      char *p;
      int Slot;
      strip_trailing_junk(sd->msg);

      /* Check for returned SD messages */
      if (sd->msg[0] == '3'     && B_ISDIGIT(sd->msg[1]) &&
	  B_ISDIGIT(sd->msg[2]) && B_ISDIGIT(sd->msg[3]) &&
          sd->msg[4] == ' ') {
         bsendmsg(ua, "%s\n", sd->msg);   /* pass them on to user */
	 continue;
      }

      /* Validate Slot:Barcode */
      p = strchr(sd->msg, ':');
      if (p && strlen(p) > 1) {
	 *p++ = 0;
	 if (!is_an_integer(sd->msg)) {
	    continue;
	 }
      } else {
	 continue;
      }
      Slot = atoi(sd->msg);
      if (Slot <= 0 || !is_volume_name_legal(ua, p)) {
	 continue;
      }

      /* Add Slot and VolumeName to list */
      vl = (vol_list_t *)malloc(sizeof(vol_list_t));
      vl->Slot = Slot;
      vl->VolName = bstrdup(p);
      if (!vol_list) {
	 vl->next = vol_list;
	 vol_list = vl;
      } else {
	 /* Add new entry to end of list */
	 for (vol_list_t *tvl=vol_list; tvl; tvl=tvl->next) {
	    if (!tvl->next) {
	       tvl->next = vl;
	       vl->next = NULL;
	       break;
	    }
	 }
      }
   }
   return vol_list;
}

/*
 * Check if this is a cleaning tape by comparing the Volume name
 *  with the Cleaning Prefix. If they match, this is a cleaning 
 *  tape.
 */
static int is_cleaning_tape(UAContext *ua, MEDIA_DBR *mr, POOL_DBR *pr)
{
   if (!ua->jcr->pool) {
      /* Find Pool resource */
      ua->jcr->pool = (POOL *)GetResWithName(R_POOL, pr->Name);
      if (!ua->jcr->pool) {
         bsendmsg(ua, _("Pool %s resource not found!\n"), pr->Name);
	 return 1;
      }
   }
   if (ua->jcr->pool->cleaning_prefix == NULL) {
      return 0;
   }
   return strncmp(mr->VolumeName, ua->jcr->pool->cleaning_prefix,
		  strlen(ua->jcr->pool->cleaning_prefix)) == 0;
}
