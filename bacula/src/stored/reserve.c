/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2000-2014 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from many
   others, a complete list can be found in the file AUTHORS.

   You may use this file and others of this release according to the
   license defined in the LICENSE file, which includes the Affero General
   Public License, v3.0 ("AGPLv3") and some additional permissions and
   terms pursuant to its AGPLv3 Section 7.

   Bacula® is a registered trademark of Kern Sibbald.
*/
/*
 *   Drive reservation functions for Storage Daemon
 *
 *   Written by Kern Sibbald, MM
 *
 *   Split from job.c and acquire.c June 2005
 *
 */

#include "bacula.h"
#include "stored.h"

const int dbglvl = 150;

static brwlock_t reservation_lock;
int reservations_lock_count = 0;

/* Forward referenced functions */
static int  can_reserve_drive(DCR *dcr, RCTX &rctx);
static bool is_vol_in_autochanger(RCTX &rctx, VOLRES *vol);
static bool reserve_device_for_append(DCR *dcr, RCTX &rctx);
static bool reserve_device_for_read(DCR *dcr);
static bool use_device_cmd(JCR *jcr);
static int  reserve_device(RCTX &rctx);
static void pop_reserve_messages(JCR *jcr);
static void queue_reserve_message(JCR *jcr);
//void switch_device(DCR *dcr, DEVICE *dev);

/* Requests from the Director daemon */
static char use_storage[]  = "use storage=%127s media_type=%127s "
    "pool_name=%127s pool_type=%127s append=%d copy=%d stripe=%d\n";
static char use_device[]  = "use device=%127s\n";

/* Responses sent to Director daemon */
static char OK_device[] = "3000 OK use device device=%s\n";
static char NO_device[] = "3924 Device \"%s\" not in SD Device"
     " resources or no matching Media Type.\n";
static char BAD_use[]   = "3913 Bad use command: %s\n";

/*
 * This allows a given thread to recursively call lock_reservations.
 *   It must, of course, call unlock_... the same number of times.
 */
void init_reservations_lock()
{
   int errstat;
   if ((errstat=rwl_init(&reservation_lock)) != 0) {
      berrno be;
      Emsg1(M_ABORT, 0, _("Unable to initialize reservation lock. ERR=%s\n"),
            be.bstrerror(errstat));
   }

   init_vol_list_lock();
}


/* This applies to a drive and to Volumes */
void _lock_reservations(const char *file, int line)
{
   int errstat;
   reservations_lock_count++;
   if ((errstat=rwl_writelock_p(&reservation_lock, file, line)) != 0) {
      berrno be;
      Emsg2(M_ABORT, 0, "rwl_writelock failure. stat=%d: ERR=%s\n",
           errstat, be.bstrerror(errstat));
   }
}

void _unlock_reservations()
{
   int errstat;
   reservations_lock_count--;
   if ((errstat=rwl_writeunlock(&reservation_lock)) != 0) {
      berrno be;
      Emsg2(M_ABORT, 0, "rwl_writeunlock failure. stat=%d: ERR=%s\n",
           errstat, be.bstrerror(errstat));
   }
}

void term_reservations_lock()
{
   rwl_destroy(&reservation_lock);
   term_vol_list_lock();
}

void DCR::clear_reserved()
{
   if (m_reserved) {
      m_reserved = false;
      dev->dec_reserved();
      Dmsg2(dbglvl, "Dec reserve=%d dev=%s\n", dev->num_reserved(), dev->print_name());
   }
}

void DCR::set_reserved_for_append()
{
   m_reserved = true;
   dev->set_append_reserve();
   Dmsg2(dbglvl, "Inc reserve=%d dev=%s\n", dev->num_reserved(), dev->print_name());
   dev->inc_reserved();
}

void DCR::set_reserved_for_read()
{
   m_reserved = true;
   dev->set_read_reserve();
   Dmsg2(dbglvl, "Inc reserve=%d dev=%s\n", dev->num_reserved(), dev->print_name());
   dev->inc_reserved();
}

/*
 * Remove any reservation from a drive and tell the system
 *  that the volume is unused at least by us.
 */
void DCR::unreserve_device(bool locked)
{
   if (!locked) {
      dev->Lock();
   }
   if (is_reserved()) {
      clear_reserved();
      reserved_volume = false;
      /* If we set read mode in reserving, remove it */
      if (dev->can_read()) {
         remove_read_volume(jcr, this->VolumeName);
         dev->clear_read();
      }
      if (dev->num_writers < 0) {
         Jmsg1(jcr, M_ERROR, 0, _("Hey! num_writers=%d!!!!\n"), dev->num_writers);
         dev->num_writers = 0;
      }
      if (dev->num_reserved() == 0 && dev->num_writers == 0) {
         generate_plugin_event(jcr, bsdEventDeviceClose, this);
         volume_unused(this);
      }
   }
   if (!locked) {
      dev->Unlock();
   }
}

bool use_cmd(JCR *jcr)
{
   /*
    * Get the device, media, and pool information
    */
   if (!use_device_cmd(jcr)) {
      jcr->setJobStatus(JS_ErrorTerminated);
      memset(jcr->sd_auth_key, 0, strlen(jcr->sd_auth_key));
      return false;
   }
   return true;
}

/*
 * We get the following type of information:
 *
 * use storage=xxx media_type=yyy pool_name=xxx pool_type=yyy append=1 copy=0 strip=0
 *  use device=zzz
 *  use device=aaa
 *  use device=bbb
 * use storage=xxx media_type=yyy pool_name=xxx pool_type=yyy append=0 copy=0 strip=0
 *  use device=bbb
 *
 */
static bool use_device_cmd(JCR *jcr)
{
   POOL_MEM store_name, dev_name, media_type, pool_name, pool_type;
   BSOCK *dir = jcr->dir_bsock;
   int32_t append;
   bool ok;
   int32_t Copy, Stripe;
   DIRSTORE *store;
   RCTX rctx;
   alist *dirstore;

   memset(&rctx, 0, sizeof(RCTX));
   rctx.jcr = jcr;
   /*
    * If there are multiple devices, the director sends us
    *   use_device for each device that it wants to use.
    */
   dirstore = New(alist(10, not_owned_by_alist));
   jcr->reserve_msgs = New(alist(10, not_owned_by_alist));
   do {
      Dmsg1(dbglvl, "<dird: %s", dir->msg);
      ok = sscanf(dir->msg, use_storage, store_name.c_str(),
                  media_type.c_str(), pool_name.c_str(),
                  pool_type.c_str(), &append, &Copy, &Stripe) == 7;
      if (!ok) {
         break;
      }
      if (append) {
         jcr->write_store = dirstore;
      } else {
         jcr->read_store = dirstore;
      }
      rctx.append = append;
      unbash_spaces(store_name);
      unbash_spaces(media_type);
      unbash_spaces(pool_name);
      unbash_spaces(pool_type);
      store = new DIRSTORE;
      dirstore->append(store);
      memset(store, 0, sizeof(DIRSTORE));
      store->device = New(alist(10));
      bstrncpy(store->name, store_name, sizeof(store->name));
      bstrncpy(store->media_type, media_type, sizeof(store->media_type));
      bstrncpy(store->pool_name, pool_name, sizeof(store->pool_name));
      bstrncpy(store->pool_type, pool_type, sizeof(store->pool_type));
      store->append = append;

      /* Now get all devices */
      while (dir->recv() >= 0) {
         Dmsg1(dbglvl, "<dird device: %s", dir->msg);
         ok = sscanf(dir->msg, use_device, dev_name.c_str()) == 1;
         if (!ok) {
            break;
         }
         unbash_spaces(dev_name);
         store->device->append(bstrdup(dev_name.c_str()));
      }
   }  while (ok && dir->recv() >= 0);

#ifdef xxxx
   /* Developers debug code */
   char *device_name;
   if (debug_level >= dbglvl) {
      foreach_alist(store, dirstore) {
         Dmsg5(dbglvl, "Storage=%s media_type=%s pool=%s pool_type=%s append=%d\n",
            store->name, store->media_type, store->pool_name,
            store->pool_type, store->append);
         foreach_alist(device_name, store->device) {
            Dmsg1(dbglvl, "     Device=%s\n", device_name);
         }
      }
   }
#endif

   init_jcr_device_wait_timers(jcr);
   jcr->dcr = new_dcr(jcr, NULL, NULL, !rctx.append);   /* get a dcr */
   if (!jcr->dcr) {
      BSOCK *dir = jcr->dir_bsock;
      dir->fsend(_("3939 Could not get dcr\n"));
      Dmsg1(dbglvl, ">dird: %s", dir->msg);
      ok = false;
   }
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
      int wait_for_device_retries = 0;
      int repeat = 0;
      bool fail = false;
      rctx.notify_dir = true;

      /* Put new dcr in proper location */
      if (rctx.append) {
         rctx.jcr->dcr = jcr->dcr;
      } else {
         rctx.jcr->read_dcr = jcr->dcr;
      }
      lock_reservations();
      for ( ; !fail && !job_canceled(jcr); ) {
         pop_reserve_messages(jcr);
         rctx.suitable_device = false;
         rctx.have_volume = false;
         rctx.VolumeName[0] = 0;
         rctx.any_drive = false;
         if (!jcr->PreferMountedVols) {
            /*
             * Here we try to find a drive that is not used.
             * This will maximize the use of available drives.
             *
             */
            rctx.num_writers = 20000000;   /* start with impossible number */
            rctx.low_use_drive = NULL;
            rctx.PreferMountedVols = false;
            rctx.exact_match = false;
            rctx.autochanger_only = true;
            if ((ok = find_suitable_device_for_job(jcr, rctx))) {
               break;
            }
            /* Look through all drives possibly for low_use drive */
            if (rctx.low_use_drive) {
               rctx.try_low_use_drive = true;
               if ((ok = find_suitable_device_for_job(jcr, rctx))) {
                  break;
               }
               rctx.try_low_use_drive = false;
            }
            rctx.autochanger_only = false;
            if ((ok = find_suitable_device_for_job(jcr, rctx))) {
               break;
            }
         }
         /*
          * Now we look for a drive that may or may not be in
          *  use.
          */
         /* Look for an exact Volume match all drives */
         rctx.PreferMountedVols = true;
         rctx.exact_match = true;
         rctx.autochanger_only = false;
         if ((ok = find_suitable_device_for_job(jcr, rctx))) {
            break;
         }
         /* Look for any mounted drive */
         rctx.exact_match = false;
         if ((ok = find_suitable_device_for_job(jcr, rctx))) {
            break;
         }
         /* Try any drive */
         rctx.any_drive = true;
         if ((ok = find_suitable_device_for_job(jcr, rctx))) {
            break;
         }
         /* Keep reservations locked *except* during wait_for_device() */
         unlock_reservations();
         /*
          * The idea of looping on repeat a few times it to ensure
          * that if there is some subtle timing problem between two
          * jobs, we will simply try again, and most likely succeed.
          * This can happen if one job reserves a drive or finishes using
          * a drive at the same time a second job wants it.
          */
         if (repeat++ > 1) {              /* try algorithm 3 times */
            bmicrosleep(30, 0);           /* wait a bit */
            Dmsg1(100, "repeat reserve algorithm JobId=%d\n", jcr->JobId);
         } else if (!rctx.suitable_device || !wait_for_any_device(jcr, wait_for_device_retries)) {
            Dmsg0(100, "Fail. !suitable_device || !wait_for_device\n");
            fail = true;
         }
         lock_reservations();
         dir->signal(BNET_HEARTBEAT);  /* Inform Dir that we are alive */
      }
      unlock_reservations();

      if (!ok) {
         /*
          * If we get here, there are no suitable devices available, which
          *  means nothing configured.  If a device is suitable but busy
          *  with another Volume, we will not come here.
          */
         unbash_spaces(dir->msg);
         pm_strcpy(jcr->errmsg, dir->msg);
         Jmsg(jcr, M_FATAL, 0, _("Device reservation failed for JobId=%d: %s\n"),
              jcr->JobId, jcr->errmsg);
         dir->fsend(NO_device, dev_name.c_str());

         Dmsg1(dbglvl, ">dird: %s", dir->msg);
      }
   } else {
      unbash_spaces(dir->msg);
      pm_strcpy(jcr->errmsg, dir->msg);
      Jmsg(jcr, M_FATAL, 0, _("Failed command: %s\n"), jcr->errmsg);
      dir->fsend(BAD_use, jcr->errmsg);
      Dmsg1(dbglvl, ">dird: %s", dir->msg);
   }

   release_reserve_messages(jcr);
   return ok;
}

/*
 * Search for a device suitable for this job.
 * Note, this routine sets sets rctx.suitable_device if any
 *   device exists within the SD.  The device may not be actually
 *   useable.
 * It also returns if it finds a useable device.
 */
bool find_suitable_device_for_job(JCR *jcr, RCTX &rctx)
{
   bool ok = false;
   DIRSTORE *store;
   char *device_name;
   alist *dirstore;
   DCR *dcr = jcr->dcr;

   if (rctx.append) {
      dirstore = jcr->write_store;
   } else {
      dirstore = jcr->read_store;
   }
   Dmsg5(dbglvl, "Start find_suit_dev PrefMnt=%d exact=%d suitable=%d chgronly=%d any=%d\n",
         rctx.PreferMountedVols, rctx.exact_match, rctx.suitable_device,
         rctx.autochanger_only, rctx.any_drive);

   /*
    * If the appropriate conditions of this if are met, namely that
    *  we are appending and the user wants mounted drive (or we
    *  force try a mounted drive because they are all busy), we
    *  start by looking at all the Volumes in the volume list.
    */
   if (!is_vol_list_empty() && rctx.append && rctx.PreferMountedVols) {
      dlist *temp_vol_list;
      VOLRES *vol = NULL;
      temp_vol_list = dup_vol_list(jcr);

      /* Look through reserved volumes for one we can use */
      Dmsg0(dbglvl, "look for vol in vol list\n");
      foreach_dlist(vol, temp_vol_list) {
         if (!vol->dev) {
            Dmsg1(dbglvl, "vol=%s no dev\n", vol->vol_name);
            continue;
         }
         /* Check with Director if this Volume is OK */
         bstrncpy(dcr->VolumeName, vol->vol_name, sizeof(dcr->VolumeName));
         if (!dir_get_volume_info(dcr, GET_VOL_INFO_FOR_WRITE)) {
            continue;
         }

         Dmsg1(dbglvl, "vol=%s OK for this job\n", vol->vol_name);
         foreach_alist(store, dirstore) {
            int stat;
            rctx.store = store;
            foreach_alist(device_name, store->device) {
               /* Found a device, try to use it */
               rctx.device_name = device_name;
               rctx.device = vol->dev->device;

               if (vol->dev->read_only) {
                  continue;
               }
               if (vol->dev->is_autochanger()) {
                  Dmsg1(dbglvl, "vol=%s is in changer\n", vol->vol_name);
                  if (!is_vol_in_autochanger(rctx, vol) || !vol->dev->autoselect) {
                     continue;
                  }
               } else if (strcmp(device_name, vol->dev->device->hdr.name) != 0) {
                  Dmsg2(dbglvl, "device=%s not suitable want %s\n",
                        vol->dev->device->hdr.name, device_name);
                  continue;
               }

               bstrncpy(rctx.VolumeName, vol->vol_name, sizeof(rctx.VolumeName));
               rctx.have_volume = true;
               /* Try reserving this device and volume */
               Dmsg2(dbglvl, "try vol=%s on device=%s\n", rctx.VolumeName, device_name);
               stat = reserve_device(rctx);
               if (stat == 1) {             /* found available device */
                  Dmsg1(dbglvl, "Suitable device found=%s\n", device_name);
                  ok = true;
                  break;
               } else if (stat == 0) {      /* device busy */
                  Dmsg1(dbglvl, "Suitable device=%s, busy: not use\n", device_name);
               } else {
                  /* otherwise error */
                  Dmsg0(dbglvl, "No suitable device found.\n");
               }
               rctx.have_volume = false;
               rctx.VolumeName[0] = 0;
            }
            if (ok) {
               break;
            }
         }
         if (ok) {
            break;
         }
      } /* end for loop over reserved volumes */

      Dmsg0(dbglvl, "lock volumes\n");
      free_temp_vol_list(temp_vol_list);
   }
   if (ok) {
      Dmsg1(dbglvl, "OK dev found. Vol=%s from in-use vols list\n", rctx.VolumeName);
      return true;
   }

   /*
    * No reserved volume we can use, so now search for an available device.
    *
    * For each storage device that the user specified, we
    *  search and see if there is a resource for that device.
    */
   foreach_alist(store, dirstore) {
      rctx.store = store;
      foreach_alist(device_name, store->device) {
         int stat;
         rctx.device_name = device_name;
         stat = search_res_for_device(rctx);
         if (stat == 1) {             /* found available device */
            Dmsg1(dbglvl, "available device found=%s\n", device_name);
            ok = true;
            break;
         } else if (stat == 0) {      /* device busy */
            Dmsg1(dbglvl, "No usable device=%s, busy: not use\n", device_name);
         } else {
            /* otherwise error */
            Dmsg0(dbglvl, "No usable device found.\n");
         }
      }
      if (ok) {
         break;
      }
   }
   if (ok) {
      Dmsg1(dbglvl, "OK dev found. Vol=%s\n", rctx.VolumeName);
   } else {
      Dmsg0(dbglvl, "Leave find_suit_dev: no dev found.\n");
   }
   return ok;
}

/*
 * Search for a particular storage device with particular storage
 *  characteristics (MediaType).
 */
int search_res_for_device(RCTX &rctx)
{
   AUTOCHANGER *changer;
   int stat;

   Dmsg1(dbglvl, "search res for %s\n", rctx.device_name);
   /* Look through Autochangers first */
   foreach_res(changer, R_AUTOCHANGER) {
      Dmsg1(dbglvl, "Try match changer res=%s\n", changer->hdr.name);
      /* Find resource, and make sure we were able to open it */
      if (strcmp(rctx.device_name, changer->hdr.name) == 0) {
         /* Try each device in this AutoChanger */
         foreach_alist(rctx.device, changer->device) {
            Dmsg1(dbglvl, "Try changer device %s\n", rctx.device->hdr.name);
            if (rctx.store->append && rctx.device->read_only) {
               continue;
            }
            if (!rctx.device->autoselect) {
               Dmsg1(100, "Device %s not autoselect skipped.\n",
               rctx.device->hdr.name);
               continue;              /* device is not available */
            }
            stat = reserve_device(rctx);
            if (stat != 1) {             /* try another device */
               continue;
            }
            /* Debug code */
            if (rctx.store->append) {
               Dmsg2(dbglvl, "Device %s reserved=%d for append.\n",
                  rctx.device->hdr.name, rctx.jcr->dcr->dev->num_reserved());
            } else {
               Dmsg2(dbglvl, "Device %s reserved=%d for read.\n",
                  rctx.device->hdr.name, rctx.jcr->read_dcr->dev->num_reserved());
            }
            return stat;
         }
      }
   }

   /* Now if requested look through regular devices */
   if (!rctx.autochanger_only) {
      foreach_res(rctx.device, R_DEVICE) {
         Dmsg1(dbglvl, "Try match res=%s\n", rctx.device->hdr.name);
         /* Find resource, and make sure we were able to open it */
         if (strcmp(rctx.device_name, rctx.device->hdr.name) == 0) {
            stat = reserve_device(rctx);
            if (stat != 1) {             /* try another device */
               continue;
            }
            /* Debug code */
            if (rctx.store->append) {
               Dmsg2(dbglvl, "Device %s reserved=%d for append.\n",
                  rctx.device->hdr.name, rctx.jcr->dcr->dev->num_reserved());
            } else {
               Dmsg2(dbglvl, "Device %s reserved=%d for read.\n",
                  rctx.device->hdr.name, rctx.jcr->read_dcr->dev->num_reserved());
            }
            return stat;
         }
      }
   }
   return -1;                    /* nothing found */
}

/*
 * Walk through the autochanger resources and check if
 *  the volume is in one of them.
 *
 * Returns:  true  if volume is in device
 *           false otherwise
 */
static bool is_vol_in_autochanger(RCTX &rctx, VOLRES *vol)
{
   AUTOCHANGER *changer = vol->dev->device->changer_res;

   /* Find resource, and make sure we were able to open it */
   if (changer && strcmp(rctx.device_name, changer->hdr.name) == 0) {
      Dmsg1(dbglvl, "Found changer device %s\n", vol->dev->device->hdr.name);
      return true;
   }
   Dmsg1(dbglvl, "Incorrect changer device %s\n", changer->hdr.name);
   return false;
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

   /* Make sure MediaType is OK */
   Dmsg2(dbglvl, "chk MediaType device=%s request=%s\n",
         rctx.device->media_type, rctx.store->media_type);
   if (strcmp(rctx.device->media_type, rctx.store->media_type) != 0) {
      return -1;
   }

   /* Make sure device exists -- i.e. we can stat() it */
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

   rctx.suitable_device = true;
   Dmsg1(dbglvl, "try reserve %s\n", rctx.device->hdr.name);
   if (rctx.store->append) {
      dcr = new_dcr(rctx.jcr, rctx.jcr->dcr, rctx.device->dev, SD_APPEND);
   } else {
      dcr = new_dcr(rctx.jcr, rctx.jcr->read_dcr, rctx.device->dev, SD_READ);
   }
   if (!dcr) {
      BSOCK *dir = rctx.jcr->dir_bsock;
      dir->fsend(_("3926 Could not get dcr for device: %s\n"), rctx.device_name);
      Dmsg1(dbglvl, ">dird: %s", dir->msg);
      return -1;
   }
   bstrncpy(dcr->pool_name, rctx.store->pool_name, name_len);
   bstrncpy(dcr->pool_type, rctx.store->pool_type, name_len);
   bstrncpy(dcr->media_type, rctx.store->media_type, name_len);
   bstrncpy(dcr->dev_name, rctx.device_name, name_len);
   if (rctx.store->append) {
      Dmsg2(dbglvl, "call reserve for append: have_vol=%d vol=%s\n", rctx.have_volume, rctx.VolumeName);
      ok = reserve_device_for_append(dcr, rctx);
      if (!ok) {
         goto bail_out;
      }

      rctx.jcr->dcr = dcr;
      Dmsg5(dbglvl, "Reserved=%d dev_name=%s mediatype=%s pool=%s ok=%d\n",
               dcr->dev->num_reserved(),
               dcr->dev_name, dcr->media_type, dcr->pool_name, ok);
      Dmsg3(dbglvl, "Vol=%s num_writers=%d, have_vol=%d\n",
         rctx.VolumeName, dcr->dev->num_writers, rctx.have_volume);
      if (rctx.have_volume) {
         Dmsg0(dbglvl, "Call reserve_volume for append.\n");
         if (reserve_volume(dcr, rctx.VolumeName)) {
            Dmsg1(dbglvl, "Reserved vol=%s\n", rctx.VolumeName);
         } else {
            Dmsg1(dbglvl, "Could not reserve vol=%s\n", rctx.VolumeName);
            goto bail_out;
         }
      } else {
         dcr->any_volume = true;
         Dmsg0(dbglvl, "no vol, call find_next_appendable_vol.\n");
         if (dir_find_next_appendable_volume(dcr)) {
            bstrncpy(rctx.VolumeName, dcr->VolumeName, sizeof(rctx.VolumeName));
            rctx.have_volume = true;
            Dmsg1(dbglvl, "looking for Volume=%s\n", rctx.VolumeName);
         } else {
            dcr->dev->clear_wait();
            Dmsg0(dbglvl, "No next volume found\n");
            rctx.have_volume = false;
            rctx.VolumeName[0] = 0;
            /*
             * If there is at least one volume that is valid and in use,
             *   but we get here, check if we are running with prefers
             *   non-mounted drives.  In that case, we have selected a
             *   non-used drive and our one and only volume is mounted
             *   elsewhere, so we bail out and retry using that drive.
             */
            if (dcr->found_in_use() && !rctx.PreferMountedVols) {
               rctx.PreferMountedVols = true;
               if (dcr->VolumeName[0]) {
                  dcr->unreserve_device(false);
               }
               goto bail_out;
            }
            /*
             * Note. Under some circumstances, the Director can hand us
             *  a Volume name that is not the same as the one on the current
             *  drive, and in that case, the call above to find the next
             *  volume will fail because in attempting to reserve the Volume
             *  the code will realize that we already have a tape mounted,
             *  and it will fail.  This *should* only happen if there are
             *  writers, thus the following test.  In that case, we simply
             *  bail out, and continue waiting, rather than plunging on
             *  and hoping that the operator can resolve the problem.
             */
            if (dcr->dev->num_writers != 0) {
               if (dcr->VolumeName[0]) {
                  dcr->unreserve_device(false);
               }
               goto bail_out;
            }
         }
      }
   } else {
      ok = reserve_device_for_read(dcr);
      if (ok) {
         rctx.jcr->read_dcr = dcr;
         Dmsg5(dbglvl, "Read reserved=%d dev_name=%s mediatype=%s pool=%s ok=%d\n",
               dcr->dev->num_reserved(),
               dcr->dev_name, dcr->media_type, dcr->pool_name, ok);
      }
   }
   if (!ok) {
      goto bail_out;
   }

   if (rctx.notify_dir) {
      POOL_MEM dev_name;
      BSOCK *dir = rctx.jcr->dir_bsock;
      pm_strcpy(dev_name, rctx.device->hdr.name);
      bash_spaces(dev_name);
      ok = dir->fsend(OK_device, dev_name.c_str());  /* Return real device name */
      Dmsg1(dbglvl, ">dird: %s", dir->msg);
      if (!ok) {
         dcr->unreserve_device(false);
      }
   } else {
      ok = true;
   }
   return ok ? 1 : -1;

bail_out:
   rctx.have_volume = false;
   rctx.VolumeName[0] = 0;
   Dmsg0(dbglvl, "Not OK.\n");
   return 0;
}


/*
 * We reserve the device for appending by incrementing
 *  num_reserved(). We do virtually all the same work that
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
static bool reserve_device_for_append(DCR *dcr, RCTX &rctx)
{
   JCR *jcr = dcr->jcr;
   DEVICE *dev = dcr->dev;
   bool ok = false;

   ASSERT2(dcr, "No dcr in reserve_device_for_append!");
   if (job_canceled(jcr)) {
      return false;
   }

   dev->Lock();

   /* If device is being read or reserved for read, we cannot write it */
   if (dev->can_read() || dev->is_reserved_for_read()) {
      Mmsg(jcr->errmsg, _("3603 JobId=%u %s device %s is busy reading.\n"),
         jcr->JobId, dev->print_type(), dev->print_name());
      queue_reserve_message(jcr);
      Dmsg1(dbglvl, "Failed: %s", jcr->errmsg);
      goto bail_out;
   }

   /* If device is unmounted, we are out of luck */
   if (dev->is_device_unmounted()) {
      Mmsg(jcr->errmsg, _("3604 JobId=%u %s device %s is BLOCKED due to user unmount.\n"),
         jcr->JobId, dev->print_type(), dev->print_name());
      queue_reserve_message(jcr);
      Dmsg1(dbglvl, "Failed: %s", jcr->errmsg);
      goto bail_out;
   }

   Dmsg2(dbglvl, "reserve_append %s device is %s\n", dev->print_type(), dev->print_name());

   /* Now do detailed tests ... */
   if (can_reserve_drive(dcr, rctx) != 1) {
      Dmsg0(dbglvl, "can_reserve_drive!=1\n");
      goto bail_out;
   }

   /* Note: on failure this returns jcr->errmsg properly edited */
   if (generate_plugin_event(jcr, bsdEventDeviceTryOpen, dcr) != bRC_OK) {
      queue_reserve_message(jcr);
      goto bail_out;
   }
   dcr->set_reserved_for_append();
   ok = true;

bail_out:
   dev->Unlock();
   return ok;
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

   ASSERT2(dcr, "No dcr in reserve_device_for_read!");
   if (job_canceled(jcr)) {
      return false;
   }

   dev->Lock();

   if (dev->is_device_unmounted()) {
      Mmsg(jcr->errmsg, _("3601 JobId=%u %s device %s is BLOCKED due to user unmount.\n"),
           jcr->JobId, dev->print_type(), dev->print_name());
      queue_reserve_message(jcr);
      Dmsg1(dbglvl, "Device %s is BLOCKED due to user unmount.\n", dev->print_name());
      goto bail_out;
   }

   if (dev->is_busy()) {
      Mmsg(jcr->errmsg, _("3602 JobId=%u %s device %s is busy (already reading/writing)."
            " read=%d, writers=%d reserved=%d\n"),
            jcr->JobId, dev->print_type(), dev->print_name(),
            dev->state & ST_READ?1:0, dev->num_writers, dev->num_reserved());
      queue_reserve_message(jcr);
      Dmsg4(dbglvl, "Device %s is busy ST_READ=%d num_writers=%d reserved=%d.\n",
         dev->print_name(),
         dev->state & ST_READ?1:0, dev->num_writers, dev->num_reserved());
      goto bail_out;
   }

   /* Note: on failure this returns jcr->errmsg properly edited */
   if (generate_plugin_event(jcr, bsdEventDeviceTryOpen, dcr) != bRC_OK) {
      queue_reserve_message(jcr);
      goto bail_out;
   }
   dev->clear_append();
   dcr->set_reserved_for_read();
   ok = true;

bail_out:
   dev->Unlock();
   return ok;
}

static bool is_max_jobs_ok(DCR *dcr)
{
   DEVICE *dev = dcr->dev;
   JCR *jcr = dcr->jcr;

   Dmsg5(dbglvl, "MaxJobs=%d Jobs=%d reserves=%d Status=%s Vol=%s\n",
         dcr->VolCatInfo.VolCatMaxJobs,
         dcr->VolCatInfo.VolCatJobs, dev->num_reserved(),
         dcr->VolCatInfo.VolCatStatus,
         dcr->VolumeName);
   /* Limit max concurrent jobs on this drive */
   if (dev->max_concurrent_jobs > 0 && dev->max_concurrent_jobs <=
              (uint32_t)(dev->num_writers + dev->num_reserved())) {
      /* Max Concurrent Jobs depassed or already reserved */
      Mmsg(jcr->errmsg, _("3609 JobId=%u Max concurrent jobs=%d exceeded on %s device %s.\n"),
            (uint32_t)jcr->JobId, dev->max_concurrent_jobs,
             dev->print_type(), dev->print_name());
      queue_reserve_message(jcr);
      Dmsg1(dbglvl, "Failed: %s", jcr->errmsg);
      return false;
   }
   if (strcmp(dcr->VolCatInfo.VolCatStatus, "Recycle") == 0) {
      return true;
   }

   if (dcr->VolCatInfo.VolCatMaxJobs > 0 && dcr->VolCatInfo.VolCatMaxJobs <=
        (dcr->VolCatInfo.VolCatJobs + dev->num_reserved())) {
      /* Max Job Vols depassed or already reserved */
      Mmsg(jcr->errmsg, _("3611 JobId=%u Volume max jobs=%d exceeded on %s device %s.\n"),
            (uint32_t)jcr->JobId, dcr->VolCatInfo.VolCatMaxJobs,
            dev->print_type(), dev->print_name());
      queue_reserve_message(jcr);
      Dmsg1(dbglvl, "reserve dev failed: %s", jcr->errmsg);
      return false;                /* wait */
   }
   return true;
}


static int is_pool_ok(DCR *dcr)
{
   DEVICE *dev = dcr->dev;
   JCR *jcr = dcr->jcr;

   /* Now check if we want the same Pool and pool type */
   if (strcmp(dev->pool_name, dcr->pool_name) == 0 &&
       strcmp(dev->pool_type, dcr->pool_type) == 0) {
      /* OK, compatible device */
      Dmsg1(dbglvl, "OK dev: %s num_writers=0, reserved, pool matches\n", dev->print_name());
      return 1;
   } else {
      /* Drive Pool not suitable for us */
      Mmsg(jcr->errmsg, _(
"3608 JobId=%u wants Pool=\"%s\" but have Pool=\"%s\" nreserve=%d on %s device %s.\n"),
            (uint32_t)jcr->JobId, dcr->pool_name, dev->pool_name,
            dev->num_reserved(), dev->print_type(), dev->print_name());
      Dmsg1(dbglvl, "Failed: %s", jcr->errmsg);
      queue_reserve_message(jcr);
   }
   return 0;
}

/*
 * Returns: 1 if drive can be reserved
 *          0 if we should wait
 *         -1 on error or impossibility
 */
static int can_reserve_drive(DCR *dcr, RCTX &rctx)
{
   DEVICE *dev = dcr->dev;
   JCR *jcr = dcr->jcr;

   Dmsg5(dbglvl, "PrefMnt=%d exact=%d suitable=%d chgronly=%d any=%d\n",
         rctx.PreferMountedVols, rctx.exact_match, rctx.suitable_device,
         rctx.autochanger_only, rctx.any_drive);

   /* Check for max jobs on this Volume */
   if (!is_max_jobs_ok(dcr)) {
      return 0;
   }

   /* setting any_drive overrides PreferMountedVols flag */
   if (!rctx.any_drive) {
      /*
       * When PreferMountedVols is set, we keep track of the
       *  drive in use that has the least number of writers, then if
       *  no unmounted drive is found, we try that drive. This
       *  helps spread the load to the least used drives.
       */
      if (rctx.try_low_use_drive && dev == rctx.low_use_drive) {
         Dmsg2(dbglvl, "OK dev=%s == low_drive=%s.\n",
            dev->print_name(), rctx.low_use_drive->print_name());
         return 1;
      }
      /* If he wants a free drive, but this one is busy, no go */
      if (!rctx.PreferMountedVols && dev->is_busy()) {
         /* Save least used drive */
         if ((dev->num_writers + dev->num_reserved()) < rctx.num_writers) {
            rctx.num_writers = dev->num_writers + dev->num_reserved();
            rctx.low_use_drive = dev;
            Dmsg2(dbglvl, "set low use drive=%s num_writers=%d\n",
               dev->print_name(), rctx.num_writers);
         } else {
            Dmsg1(dbglvl, "not low use num_writers=%d\n", dev->num_writers+dev->num_reserved());
         }
         Mmsg(jcr->errmsg, _("3605 JobId=%u wants free drive but %s device %s is busy.\n"),
            jcr->JobId, dev->print_type(), dev->print_name());
         queue_reserve_message(jcr);
         Dmsg1(dbglvl, "Failed: %s", jcr->errmsg);
         return 0;
      }

      /* Check for prefer mounted volumes */
      if (rctx.PreferMountedVols && !dev->vol && dev->is_tape()) {
         Mmsg(jcr->errmsg, _("3606 JobId=%u prefers mounted drives, but %s device %s has no Volume.\n"),
            jcr->JobId, dev->print_type(), dev->print_name());
         queue_reserve_message(jcr);
         Dmsg1(dbglvl, "Failed: %s", jcr->errmsg);
         return 0;                 /* No volume mounted */
      }

      /* Check for exact Volume name match */
      /* ***FIXME*** for Disk, we can accept any volume that goes with this
       *    drive.
       */
      if (rctx.exact_match && rctx.have_volume) {
         bool ok;
         Dmsg5(dbglvl, "PrefMnt=%d exact=%d suitable=%d chgronly=%d any=%d\n",
               rctx.PreferMountedVols, rctx.exact_match, rctx.suitable_device,
               rctx.autochanger_only, rctx.any_drive);
         Dmsg4(dbglvl, "have_vol=%d have=%s resvol=%s want=%s\n",
                  rctx.have_volume, dev->VolHdr.VolumeName,
                  dev->vol?dev->vol->vol_name:"*none*", rctx.VolumeName);
         ok = strcmp(dev->VolHdr.VolumeName, rctx.VolumeName) == 0 ||
                 (dev->vol && strcmp(dev->vol->vol_name, rctx.VolumeName) == 0);
         if (!ok) {
            Mmsg(jcr->errmsg, _("3607 JobId=%u wants Vol=\"%s\" drive has Vol=\"%s\" on %s device %s.\n"),
               jcr->JobId, rctx.VolumeName, dev->VolHdr.VolumeName,
               dev->print_type(), dev->print_name());
            queue_reserve_message(jcr);
            Dmsg3(dbglvl, "not OK: dev have=%s resvol=%s want=%s\n",
                  dev->VolHdr.VolumeName, dev->vol?dev->vol->vol_name:"*none*", rctx.VolumeName);
            return 0;
         }
         if (!dcr->can_i_use_volume()) {
            return 0;              /* fail if volume on another drive */
         }
      }
   }

   /* Check for unused autochanger drive */
   if (rctx.autochanger_only && !dev->is_busy() &&
       dev->VolHdr.VolumeName[0] == 0) {
      /* Device is available but not yet reserved, reserve it for us */
      Dmsg1(dbglvl, "OK Res Unused autochanger %s.\n", dev->print_name());
      bstrncpy(dev->pool_name, dcr->pool_name, sizeof(dev->pool_name));
      bstrncpy(dev->pool_type, dcr->pool_type, sizeof(dev->pool_type));
      return 1;                       /* reserve drive */
   }

   /*
    * Handle the case that there are no writers
    */
   if (dev->num_writers == 0) {
      /* Now check if there are any reservations on the drive */
      if (dev->num_reserved()) {
         return is_pool_ok(dcr);
      } else if (dev->can_append()) {
         if (is_pool_ok(dcr)) {
            return 1;
         } else {
            /* Changing pool, unload old tape if any in drive */
            Dmsg0(dbglvl, "OK dev: num_writers=0, not reserved, pool change, unload changer\n");
            /* ***FIXME*** use set_unload() */
            unload_autochanger(dcr, -1);
         }
      }
      /* Device is available but not yet reserved, reserve it for us */
      Dmsg1(dbglvl, "OK Dev avail reserved %s\n", dev->print_name());
      bstrncpy(dev->pool_name, dcr->pool_name, sizeof(dev->pool_name));
      bstrncpy(dev->pool_type, dcr->pool_type, sizeof(dev->pool_type));
      return 1;                       /* reserve drive */
   }

   /*
    * Check if the device is in append mode with writers (i.e.
    *  available if pool is the same).
    */
   if (dev->can_append() || dev->num_writers > 0) {
      return is_pool_ok(dcr);
   } else {
      Pmsg1(000, _("Logic error!!!! JobId=%u Should not get here.\n"), (int)jcr->JobId);
      Mmsg(jcr->errmsg, _("3910 JobId=%u Logic error!!!! %s device %s Should not get here.\n"),
            jcr->JobId, dev->print_type(), dev->print_name());
      queue_reserve_message(jcr);
      Jmsg0(jcr, M_FATAL, 0, _("Logic error!!!! Should not get here.\n"));
      return -1;                      /* error, should not get here */
   }
   Mmsg(jcr->errmsg, _("3911 JobId=%u failed reserve %s device %s.\n"),
         jcr->JobId, dev->print_type(), dev->print_name());
   queue_reserve_message(jcr);
   Dmsg1(dbglvl, "Failed: No reserve %s\n", dev->print_name());
   return 0;
}


/*
 * Queue a reservation error or failure message for this jcr
 */
static void queue_reserve_message(JCR *jcr)
{
   int i;
   alist *msgs;
   char *msg;

   jcr->lock();

   msgs = jcr->reserve_msgs;
   if (!msgs) {
      goto bail_out;
   }
   /*
    * Look for duplicate message.  If found, do
    * not insert
    */
   for (i=msgs->size()-1; i >= 0; i--) {
      msg = (char *)msgs->get(i);
      if (!msg) {
         goto bail_out;
      }
      /* Comparison based on 4 digit message number */
      if (strncmp(msg, jcr->errmsg, 4) == 0) {
         goto bail_out;
      }
   }
   /* Message unique, so insert it */
   jcr->reserve_msgs->push(bstrdup(jcr->errmsg));

bail_out:
   jcr->unlock();
}

/*
 * Send any reservation messages queued for this jcr
 */
void send_drive_reserve_messages(JCR *jcr, void sendit(const char *msg, int len, void *sarg), void *arg)
{
   int i;
   alist *msgs;
   char *msg;

   jcr->lock();
   msgs = jcr->reserve_msgs;
   if (!msgs || msgs->size() == 0) {
      goto bail_out;
   }
   for (i=msgs->size()-1; i >= 0; i--) {
      msg = (char *)msgs->get(i);
      if (msg) {
         sendit("   ", 3, arg);
         sendit(msg, strlen(msg), arg);
      } else {
         break;
      }
   }

bail_out:
   jcr->unlock();
}

/*
 * Pop and release any reservations messages
 */
static void pop_reserve_messages(JCR *jcr)
{
   alist *msgs;
   char *msg;

   jcr->lock();
   msgs = jcr->reserve_msgs;
   if (!msgs) {
      goto bail_out;
   }
   while ((msg = (char *)msgs->pop())) {
      free(msg);
   }
bail_out:
   jcr->unlock();
}

/*
 * Also called from acquire.c
 */
void release_reserve_messages(JCR *jcr)
{
   pop_reserve_messages(jcr);
   jcr->lock();
   if (!jcr->reserve_msgs) {
      goto bail_out;
   }
   delete jcr->reserve_msgs;
   jcr->reserve_msgs = NULL;

bail_out:
   jcr->unlock();
}
