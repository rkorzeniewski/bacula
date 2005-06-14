/*
 *   Drive reservation functions for Storage Daemon
 *
 *   Kern Sibbald, MM
 *
 *   Split from job.c and acquire.c June 2005
 *
 *   Version $Id$
 *
 */
/*
   Copyright (C) 2000-2005 Kern Sibbald

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   version 2 as ammended with additional clauses defined in the
   file LICENSE in the main source directory.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the 
   the file LICENSE for additional details.

 */

#include "bacula.h"
#include "stored.h"

/*
 *   Use Device command from Director
 *   He tells is what Device Name to use, the Media Type,
 *      the Pool Name, and the Pool Type.
 *
 *    Ensure that the device exists and is opened, then store
 *      the media and pool info in the JCR.  This class is used
 *      only temporarily in this file.
 */
class DIRSTORE {
public:
   alist *device;
   bool append;
   char name[MAX_NAME_LENGTH];
   char media_type[MAX_NAME_LENGTH];
   char pool_name[MAX_NAME_LENGTH];
   char pool_type[MAX_NAME_LENGTH];
};

/* Reserve context */
class RCTX {
public:
   alist *errors;
   JCR *jcr;
   char *device_name;
   DIRSTORE *store;
   DEVRES   *device;
};

static dlist *vol_list = NULL;
static pthread_mutex_t vol_list_lock = PTHREAD_MUTEX_INITIALIZER;

/* Forward referenced functions */
static int can_reserve_drive(DCR *dcr); 
static int search_res_for_device(RCTX &rctx);
static int reserve_device(RCTX &rctx);
static bool reserve_device_for_read(DCR *dcr);
static bool reserve_device_for_append(DCR *dcr);
static bool use_storage_cmd(JCR *jcr);
bool find_suitable_device_for_job(JCR *jcr, RCTX &rctx);

/* Requests from the Director daemon */
static char use_storage[]  = "use storage=%127s media_type=%127s "
   "pool_name=%127s pool_type=%127s append=%d copy=%d stripe=%d\n";
static char use_device[]  = "use device=%127s\n";

/* Responses sent to Director daemon */
static char OK_device[] = "3000 OK use device device=%s\n";
static char NO_device[] = "3924 Device \"%s\" not in SD Device resources.\n";
static char BAD_use[]   = "3913 Bad use command: %s\n";

bool use_cmd(JCR *jcr) 
{
   /*
    * Wait for the device, media, and pool information
    */
   if (!use_storage_cmd(jcr)) {
      set_jcr_job_status(jcr, JS_ErrorTerminated);
      memset(jcr->sd_auth_key, 0, strlen(jcr->sd_auth_key));
      return false;
   }
   return true;
}

static int my_compare(void *item1, void *item2)
{
   return strcmp(((VOLRES *)item1)->vol_name, ((VOLRES *)item2)->vol_name);
}


/*
 * Put a new Volume entry in the Volume list. This
 *  effectively reserves the volume so that it will
 *  not be mounted again.
 *
 *  Return: VOLRES entry on success
 *          NULL if the Volume is already in the list
 */
VOLRES *new_volume(const char *VolumeName, DEVICE *dev)
{
   VOLRES *vol, *nvol;
   vol = (VOLRES *)malloc(sizeof(VOLRES));
   memset(vol, 0, sizeof(VOLRES));
   vol->vol_name = bstrdup(VolumeName);
   vol->dev = dev;
   P(vol_list_lock);
   nvol = (VOLRES *)vol_list->binary_insert(vol, my_compare);
   V(vol_list_lock);
   if (nvol != vol) {
      free(vol->vol_name);
      free(vol);
      if (dev) {
         nvol->dev = dev;
      }
      return NULL;
   }
   return vol;
}

/*
 * Search for a Volume name in the Volume list.
 *
 *  Returns: VOLRES entry on success
 *           NULL if the Volume is not in the list
 */
VOLRES *find_volume(const char *VolumeName)
{
   VOLRES vol, *fvol;
   vol.vol_name = bstrdup(VolumeName);
   P(vol_list_lock);
   fvol = (VOLRES *)vol_list->binary_search(&vol, my_compare);
   V(vol_list_lock);
   free(vol.vol_name);
   return fvol;
}

/*  
 * Free a Volume from the Volume list
 *
 *  Returns: true if the Volume found and removed from the list
 *           false if the Volume is not in the list
 */
bool free_volume(DEVICE *dev)
{
   VOLRES vol, *fvol;

   if (dev->VolHdr.VolName[0] == 0) {
      return false;
   }
   vol.vol_name = bstrdup(dev->VolHdr.VolName);
   P(vol_list_lock);
   fvol = (VOLRES *)vol_list->binary_search(&vol, my_compare);
   if (fvol) {
      vol_list->remove(fvol);
      free(fvol->vol_name);
      free(fvol);
   }
   V(vol_list_lock);
   free(vol.vol_name);
   dev->VolHdr.VolName[0] = 0;
   return fvol != NULL;
}

/*
 * List Volumes -- this should be moved to status.c
 */
void list_volumes(BSOCK *user)  
{
   VOLRES *vol;
   for (vol=(VOLRES *)vol_list->first(); vol; vol=(VOLRES *)vol_list->next(vol)) {
      bnet_fsend(user, "%s\n", vol->vol_name);
   }
}
      
/* Create the Volume list */
void create_volume_list()
{
   VOLRES *dummy;
   if (vol_list == NULL) {
      vol_list = New(dlist(dummy, &dummy->link));
   }
}

/* Release all Volumes from the list */
void free_volume_list()
{
   VOLRES *vol;
   for (vol=(VOLRES *)vol_list->first(); vol; vol=(VOLRES *)vol_list->next(vol)) {
      Dmsg1(000, "Unreleased Volume=%s\n", vol->vol_name);
   }
   delete vol_list;
   vol_list = NULL;
}

bool is_volume_in_use(const char *VolumeName) 
{
   VOLRES *vol = find_volume(VolumeName);
   if (!vol) {
      return false;                   /* vol not in list */
   }
   if (!vol->dev) {                   /* vol not attached to device */
      return false;
   }
   if (!vol->dev->is_busy()) {
      return false;
   }
   return true;
}


static bool use_storage_cmd(JCR *jcr)
{
   POOL_MEM store_name, dev_name, media_type, pool_name, pool_type;
   BSOCK *dir = jcr->dir_bsock;
   int append;
   bool ok;       
   int Copy, Stripe;
   DIRSTORE *store;
   char *device_name;
   RCTX rctx;
   rctx.jcr = jcr;
#ifdef implemented
   char *error;
#endif

   /*
    * If there are multiple devices, the director sends us
    *   use_device for each device that it wants to use.
    */
   Dmsg1(100, "<dird: %s", dir->msg);
   jcr->dirstore = New(alist(10, not_owned_by_alist));
   do {
      ok = sscanf(dir->msg, use_storage, store_name.c_str(), 
                  media_type.c_str(), pool_name.c_str(), 
                  pool_type.c_str(), &append, &Copy, &Stripe) == 7;
      if (!ok) {
         break;
      }
      unbash_spaces(store_name);
      unbash_spaces(media_type);
      unbash_spaces(pool_name);
      unbash_spaces(pool_type);
      store = new DIRSTORE;
      jcr->dirstore->append(store);
      memset(store, 0, sizeof(DIRSTORE));
      store->device = New(alist(10));
      bstrncpy(store->name, store_name, sizeof(store->name));
      bstrncpy(store->media_type, media_type, sizeof(store->media_type));
      bstrncpy(store->pool_name, pool_name, sizeof(store->pool_name));
      bstrncpy(store->pool_type, pool_type, sizeof(store->pool_type));
      store->append = append;

      /* Now get all devices */
      while (bnet_recv(dir) >= 0) {
         ok = sscanf(dir->msg, use_device, dev_name.c_str()) == 1;
         if (!ok) {
            break;
         }
         unbash_spaces(dev_name);
         store->device->append(bstrdup(dev_name.c_str()));
      }
   }  while (ok && bnet_recv(dir) >= 0);

#ifdef DEVELOPER
   /* This loop is debug code and can be removed */
   /* ***FIXME**** remove after 1.38 release */
   foreach_alist(store, jcr->dirstore) {
      Dmsg4(100, "Storage=%s media_type=%s pool=%s pool_type=%s\n", 
         store->name, store->media_type, store->pool_name, 
         store->pool_type);
      foreach_alist(device_name, store->device) {
         Dmsg1(100, "   Device=%s\n", device_name);
      }
   }
#endif

   /*                    
    * At this point, we have a list of all the Director's Storage
    *  resources indicated for this Job, which include Pool, PoolType,
    *  storage name, and Media type.     
    * Then for each of the Storage resources, we have a list of
    *  device names that were given.
    *
    * Wiffle through them and find one that can do the backup.
    */
   if (ok) {
      ok = find_suitable_device_for_job(jcr, rctx);
      if (ok) {
         goto done;
      }
      if (verbose) {
         unbash_spaces(dir->msg);
         pm_strcpy(jcr->errmsg, dir->msg);
         Jmsg(jcr, M_INFO, 0, _("Failed command: %s\n"), jcr->errmsg);
      }
      Jmsg(jcr, M_FATAL, 0, _("\n"
         "     Device \"%s\" with MediaType \"%s\" requested by DIR not found in SD Device resources.\n"),
           dev_name.c_str(), media_type.c_str());
      bnet_fsend(dir, NO_device, dev_name.c_str());
#ifdef implemented
      for (error=(char*)rctx->errors.first(); error;
           error=(char*)rctx->errors.next()) {
         Jmsg(jcr, M_INFO, 0, "%s", error);
      }
#endif
      Dmsg1(100, ">dird: %s\n", dir->msg);
   } else {
      unbash_spaces(dir->msg);
      pm_strcpy(jcr->errmsg, dir->msg);
      if (verbose) {
         Jmsg(jcr, M_INFO, 0, _("Failed command: %s\n"), jcr->errmsg);
      }
      bnet_fsend(dir, BAD_use, jcr->errmsg);
      Dmsg1(100, ">dird: %s\n", dir->msg);
   }

done:
   foreach_alist(store, jcr->dirstore) {
      delete store->device;
      delete store;
   }
   delete jcr->dirstore;
#ifdef implemented
   for (error=(char*)rctx->errors.first(); error;
        error=(char*)rctx->errors.next()) {
      free(error);
   }
#endif
   return ok;
}


/*
 * Search for a device suitable for this job.
 */
bool find_suitable_device_for_job(JCR *jcr, RCTX &rctx)
{
   bool first = true;
   bool ok = false;
   DCR *dcr = NULL;
   DIRSTORE *store;
   char *device_name;

   init_jcr_device_wait_timers(jcr);
   for ( ;; ) {
      int need_wait = false;
      foreach_alist(store, jcr->dirstore) {
         rctx.store = store;
         foreach_alist(device_name, store->device) {
            int stat;
            rctx.device_name = device_name;
            stat = search_res_for_device(rctx); 
            if (stat == 1) {             /* found available device */
               dcr = jcr->dcr;
               ok = true;
               break;
            } else if (stat == 0) {      /* device busy */
               need_wait = true;
            }
            /* otherwise error */
//             rctx->errors.push(bstrdup(jcr->errmsg));
         }
      }
      /*
       * If there is some device for which we can wait, then
       *  wait and try again until the wait time expires
       */
      if (!need_wait || !wait_for_device(jcr, first)) {
         break;
      }
      first = false;
#ifdef implemented
      for (error=(char*)rctx->errors.first(); error;
           error=(char*)rctx->errors.next()) {
         free(error);
      }
#endif
   }
   if (!ok && dcr) {
      free_dcr(dcr);
   }

   return ok;
}

/*
 * Search for a particular storage device with particular storage
 *  characteristics (MediaType).
 */
static int search_res_for_device(RCTX &rctx) 
{
   AUTOCHANGER *changer;
   BSOCK *dir = rctx.jcr->dir_bsock;
   bool ok;
   int stat;

   Dmsg1(100, "Search res for %s\n", rctx.device_name);
   foreach_res(rctx.device, R_DEVICE) {
      Dmsg1(100, "Try res=%s\n", rctx.device->hdr.name);
      /* Find resource, and make sure we were able to open it */
      if (fnmatch(rctx.device_name, rctx.device->hdr.name, 0) == 0 &&
          strcmp(rctx.device->media_type, rctx.store->media_type) == 0) {
         stat = reserve_device(rctx);
         if (stat != 1) {
            return stat;
         }
         Dmsg1(220, "Got: %s", dir->msg);
         bash_spaces(rctx.device_name);
         ok = bnet_fsend(dir, OK_device, rctx.device_name);
         Dmsg1(100, ">dird: %s\n", dir->msg);
         return ok ? 1 : -1;
      }
   }
   foreach_res(changer, R_AUTOCHANGER) {
      Dmsg1(100, "Try changer res=%s\n", changer->hdr.name);
      /* Find resource, and make sure we were able to open it */
      if (fnmatch(rctx.device_name, changer->hdr.name, 0) == 0) {
         /* Try each device in this AutoChanger */
         foreach_alist(rctx.device, changer->device) {
            Dmsg1(100, "Try changer device %s\n", rctx.device->hdr.name);
            stat = reserve_device(rctx);
            if (stat == -1) {            /* hard error */
               return -1;
            }
            if (stat == 0) {             /* must wait, try next one */
               continue;
            }
            POOL_MEM dev_name;
            Dmsg1(100, "Device %s opened.\n", rctx.device_name);
            pm_strcpy(dev_name, rctx.device->hdr.name);
            bash_spaces(dev_name);
            ok = bnet_fsend(dir, OK_device, dev_name.c_str());  /* Return real device name */
            Dmsg1(100, ">dird: %s\n", dir->msg);
            return ok ? 1 : -1;
         }
      }
   }
   return 0;                    /* nothing found */
}

/*
 *  Try to reserve a specific device.
 *
 *  Returns: 1 -- OK, have DCR
 *           0 -- must wait
 *          -1 -- fatal error
 */
static int reserve_device(RCTX &rctx)
{
   bool ok;
   DCR *dcr;
   const int name_len = MAX_NAME_LENGTH;
   if (!rctx.device->dev) {
      rctx.device->dev = init_dev(rctx.jcr, rctx.device);
   }
   if (!rctx.device->dev) {
      if (rctx.device->changer_res) {
        Jmsg(rctx.jcr, M_WARNING, 0, _("\n"
           "     Device \"%s\" in changer \"%s\" requested by DIR could not be opened or does not exist.\n"),
             rctx.device->hdr.name, rctx.device_name);
      } else {
         Jmsg(rctx.jcr, M_WARNING, 0, _("\n"
            "     Device \"%s\" requested by DIR could not be opened or does not exist.\n"),
              rctx.device_name);
      }
      return -1;  /* no use waiting */
   }  
   Dmsg1(100, "Found device %s\n", rctx.device->hdr.name);
   dcr = new_dcr(rctx.jcr, rctx.device->dev);
   if (!dcr) {
      BSOCK *dir = rctx.jcr->dir_bsock;
      bnet_fsend(dir, _("3926 Could not get dcr for device: %s\n"), rctx.device_name);
      Dmsg1(100, ">dird: %s\n", dir->msg);
      return -1;
   }
   rctx.jcr->dcr = dcr;
   bstrncpy(dcr->pool_name, rctx.store->pool_name, name_len);
   bstrncpy(dcr->pool_type, rctx.store->pool_type, name_len);
   bstrncpy(dcr->media_type, rctx.store->media_type, name_len);
   bstrncpy(dcr->dev_name, rctx.device_name, name_len);
   if (rctx.store->append == SD_APPEND) {
      ok = reserve_device_for_append(dcr);
      Dmsg3(200, "dev_name=%s mediatype=%s ok=%d\n", dcr->dev_name, dcr->media_type, ok);
   } else {
      ok = reserve_device_for_read(dcr);
   }
   if (!ok) {
      free_dcr(rctx.jcr->dcr);
      return 0;
   }
   return 1;
}

/*
 * We "reserve" the drive by setting the ST_READ bit. No one else
 *  should touch the drive until that is cleared.
 *  This allows the DIR to "reserve" the device before actually
 *  starting the job. 
 */
static bool reserve_device_for_read(DCR *dcr)
{
   DEVICE *dev = dcr->dev;
   JCR *jcr = dcr->jcr;
   bool ok = false;

   ASSERT(dcr);

   dev->block(BST_DOING_ACQUIRE);

   if (device_is_unmounted(dev)) {             
      Dmsg1(200, "Device %s is BLOCKED due to user unmount.\n", dev->print_name());
      Mmsg(jcr->errmsg, _("Device %s is BLOCKED due to user unmount.\n"),
           dev->print_name());
      goto bail_out;
   }

   if (dev->is_busy()) {
      Dmsg4(200, "Device %s is busy ST_READ=%d num_writers=%d reserved=%d.\n", dev->print_name(),
         dev->state & ST_READ?1:0, dev->num_writers, dev->reserved_device);
      Mmsg1(jcr->errmsg, _("Device %s is busy.\n"),
            dev->print_name());
      goto bail_out;
   }

   dev->clear_append();
   dev->set_read();
   ok = true;

bail_out:
   dev->unblock();
   return ok;
}


/*
 * We reserve the device for appending by incrementing the 
 *  reserved_device. We do virtually all the same work that
 *  is done in acquire_device_for_append(), but we do
 *  not attempt to mount the device. This routine allows
 *  the DIR to reserve multiple devices before *really* 
 *  starting the job. It also permits the SD to refuse 
 *  certain devices (not up, ...).
 *
 * Note, in reserving a device, if the device is for the
 *  same pool and the same pool type, then it is acceptable.
 *  The Media Type has already been checked. If we are
 *  the first tor reserve the device, we put the pool
 *  name and pool type in the device record.
 */
static bool reserve_device_for_append(DCR *dcr)
{
   JCR *jcr = dcr->jcr;
   DEVICE *dev = dcr->dev;
   bool ok = false;

   ASSERT(dcr);

   dev->block(BST_DOING_ACQUIRE);

   if (dev->can_read()) {
      Mmsg1(jcr->errmsg, _("Device %s is busy reading.\n"), dev->print_name());
      goto bail_out;
   }

   if (device_is_unmounted(dev)) {
      Mmsg(jcr->errmsg, _("Device %s is BLOCKED due to user unmount.\n"), dev->print_name());
      goto bail_out;
   }

   Dmsg1(190, "reserve_append device is %s\n", dev->is_tape()?"tape":"disk");

   if (can_reserve_drive(dcr) != 1) {
      Mmsg1(jcr->errmsg, _("Device %s is busy writing on another Volume.\n"), dev->print_name());
      goto bail_out;
   }

   dev->reserved_device++;
   Dmsg1(200, "============= Inc reserve=%d\n", dev->reserved_device);
   dcr->reserved_device = true;
   ok = true;

bail_out:
   dev->unblock();
   return ok;
}

/*
 * Returns: 1 if drive can be reserved
 *          0 if we should wait
 *         -1 on error
 */
static int can_reserve_drive(DCR *dcr) 
{
   DEVICE *dev = dcr->dev;
   JCR *jcr = dcr->jcr;
   /*
    * First handle the case that the drive is not yet in append mode
    */
   if (!dev->can_append() && dev->num_writers == 0) {
      /* Now check if there are any reservations on the drive */
      if (dev->reserved_device) {           
         /* Yes, now check if we want the same Pool and pool type */
         if (strcmp(dev->pool_name, dcr->pool_name) == 0 &&
             strcmp(dev->pool_type, dcr->pool_type) == 0) {
            /* OK, compatible device */
         } else {
            /* Drive not suitable for us */
            return 0;                 /* wait */
         }
      } else {
         /* Device is available but not yet reserved, reserve it for us */
         bstrncpy(dev->pool_name, dcr->pool_name, sizeof(dev->pool_name));
         bstrncpy(dev->pool_type, dcr->pool_type, sizeof(dev->pool_type));
      }
      return 1;                       /* reserve drive */
   }

   /*
    * Check if device in append mode with no writers (i.e. available)
    */
   if (dev->can_append() && dev->num_writers == 0) {
      /* Device is available but not yet reserved, reserve it for us */
      bstrncpy(dev->pool_name, dcr->pool_name, sizeof(dev->pool_name));
      bstrncpy(dev->pool_type, dcr->pool_type, sizeof(dev->pool_type));
      return 1;
   }

   /*
    * Now check if the device is in append mode with writers (i.e.
    *  available if pool is the same).
    */
   if (dev->can_append() || dev->num_writers > 0) {
      Dmsg0(190, "device already in append.\n");
      /* Yes, now check if we want the same Pool and pool type */
      if (strcmp(dev->pool_name, dcr->pool_name) == 0 &&
          strcmp(dev->pool_type, dcr->pool_type) == 0) {
         /* OK, compatible device */
      } else {
         /* Drive not suitable for us */
         Jmsg(jcr, M_WARNING, 0, _("Device %s is busy writing on another Volume.\n"), dev->print_name());
         return 0;                    /* wait */
      }
   } else {
      Pmsg0(000, "Logic error!!!! Should not get here.\n");
      Jmsg0(jcr, M_FATAL, 0, _("Logic error!!!! Should not get here.\n"));
      return -1;                      /* error, should not get here */
   }
   return 1;                          /* reserve drive */
}
