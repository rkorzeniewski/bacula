/*
 *  Subroutines to handle Catalog reqests sent to the Director
 *   Reqests/commands from the Director are handled in dircmd.c
 *
 *   Kern Sibbald, December 2000
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

#include "bacula.h"                   /* pull in global headers */
#include "stored.h"                   /* pull in Storage Deamon headers */

/* Requests sent to the Director */
static char Find_media[]   = "CatReq Job=%s FindMedia=%d pool_name=%s media_type=%s\n";
static char Get_Vol_Info[] = "CatReq Job=%s GetVolInfo VolName=%s write=%d\n";
static char Update_media[] = "CatReq Job=%s UpdateMedia VolName=%s"
   " VolJobs=%u VolFiles=%u VolBlocks=%u VolBytes=%s VolMounts=%u"
   " VolErrors=%u VolWrites=%u MaxVolBytes=%s EndTime=%d VolStatus=%s"
   " Slot=%d relabel=%d InChanger=%d VolReadTime=%s VolWriteTime=%s"
   " VolParts=%u\n";
static char Create_job_media[] = "CatReq Job=%s CreateJobMedia"
   " FirstIndex=%u LastIndex=%u StartFile=%u EndFile=%u"
   " StartBlock=%u EndBlock=%u Copy=%d Strip=%d\n";
static char FileAttributes[] = "UpdCat Job=%s FileAttributes ";
static char Job_status[]     = "Status Job=%s JobStatus=%d\n";



/* Responses received from the Director */
static char OK_media[] = "1000 OK VolName=%127s VolJobs=%u VolFiles=%u"
   " VolBlocks=%u VolBytes=%" lld " VolMounts=%u VolErrors=%u VolWrites=%u"
   " MaxVolBytes=%" lld " VolCapacityBytes=%" lld " VolStatus=%20s"
   " Slot=%d MaxVolJobs=%u MaxVolFiles=%u InChanger=%d"
   " VolReadTime=%" lld " VolWriteTime=%" lld " EndFile=%u EndBlock=%u"
   " VolParts=%u LabelType=%d";


static char OK_create[] = "1000 OK CreateJobMedia\n";

#ifdef needed

static char Device_update[] = "DevUpd Job=%s device=%s "
   "append=%d read=%d num_writers=%d "
   "open=%d labeled=%d offline=%d "
   "reserved=%d max_writers=%d "
   "autoselect=%d autochanger=%d "
   "changer_name=%s media_type=%s volume_name=%s\n";


/* Send update information about a device to Director */
bool dir_update_device(JCR *jcr, DEVICE *dev)
{
   BSOCK *dir = jcr->dir_bsock;
   POOL_MEM dev_name, VolumeName, MediaType, ChangerName;
   DEVRES *device = dev->device;
   bool ok;
   
   pm_strcpy(dev_name, device->hdr.name);
   bash_spaces(dev_name);
   if (dev->is_labeled()) {
      pm_strcpy(VolumeName, dev->VolHdr.VolName);
   } else {
      pm_strcpy(VolumeName, "*");
   }
   bash_spaces(VolumeName);
   pm_strcpy(MediaType, device->media_type);
   bash_spaces(MediaType);
   if (device->changer_res) {
      pm_strcpy(ChangerName, device->changer_res->hdr.name);
      bash_spaces(ChangerName);
   } else {
      pm_strcpy(ChangerName, "*");
   }
   ok =bnet_fsend(dir, Device_update, 
      jcr->Job,
      dev_name.c_str(),
      dev->can_append()!=0,
      dev->can_read()!=0, dev->num_writers, 
      dev->is_open()!=0, dev->is_labeled()!=0,
      dev->is_offline()!=0, dev->reserved_device, 
      dev->is_tape()?100000:1,
      dev->autoselect, 0, 
      ChangerName.c_str(), MediaType.c_str(), VolumeName.c_str());
   Dmsg1(100, ">dird: %s\n", dir->msg);
   return ok;
}

bool dir_update_changer(JCR *jcr, AUTOCHANGER *changer)
{
   BSOCK *dir = jcr->dir_bsock;
   POOL_MEM dev_name, MediaType;
   DEVRES *device;
   bool ok;

   pm_strcpy(dev_name, changer->hdr.name);
   bash_spaces(dev_name);
   device = (DEVRES *)changer->device->first();
   pm_strcpy(MediaType, device->media_type);
   bash_spaces(MediaType);
   /* This is mostly to indicate that we are here */
   ok = bnet_fsend(dir, Device_update,
      jcr->Job,
      dev_name.c_str(),         /* Changer name */
      0, 0, 0,                  /* append, read, num_writers */
      0, 0, 0,                  /* is_open, is_labeled, offline */
      0, 0,                     /* reserved, max_writers */
      0,                        /* Autoselect */
      changer->device->size(),  /* Number of devices */
      "0",                      /* PoolId */
      "*",                      /* ChangerName */
      MediaType.c_str(),        /* MediaType */
      "*");                     /* VolName */
   Dmsg1(100, ">dird: %s\n", dir->msg);
   return ok;
}
#endif


/*
 * Send current JobStatus to Director
 */
bool dir_send_job_status(JCR *jcr)
{
   return bnet_fsend(jcr->dir_bsock, Job_status, jcr->Job, jcr->JobStatus);
}

/*
 * Common routine for:
 *   dir_get_volume_info()
 * and
 *   dir_find_next_appendable_volume()
 *
 *  Returns: true  on success and vol info in dcr->VolCatInfo
 *           false on failure
 */
static bool do_get_volume_info(DCR *dcr)
{
    JCR *jcr = dcr->jcr;
    BSOCK *dir = jcr->dir_bsock;
    VOLUME_CAT_INFO vol;
    int n;
    int InChanger;

    dcr->VolumeName[0] = 0;           /* No volume */
    if (bnet_recv(dir) <= 0) {
       Dmsg0(200, "getvolname error bnet_recv\n");
       Mmsg(jcr->errmsg, _("Network error on bnet_recv in req_vol_info.\n"));
       return false;
    }
    memset(&vol, 0, sizeof(vol));
    Dmsg1(100, "<dird %s", dir->msg);
    n = sscanf(dir->msg, OK_media, vol.VolCatName,
               &vol.VolCatJobs, &vol.VolCatFiles,
               &vol.VolCatBlocks, &vol.VolCatBytes,
               &vol.VolCatMounts, &vol.VolCatErrors,
               &vol.VolCatWrites, &vol.VolCatMaxBytes,
               &vol.VolCatCapacityBytes, vol.VolCatStatus,
               &vol.Slot, &vol.VolCatMaxJobs, &vol.VolCatMaxFiles,
               &InChanger, &vol.VolReadTime, &vol.VolWriteTime,
               &vol.EndFile, &vol.EndBlock, &vol.VolCatParts,
               &vol.LabelType);
    if (n != 21) {
       Dmsg2(100, "Bad response from Dir fields=%d: %s\n", n, dir->msg);
       Mmsg(jcr->errmsg, _("Error getting Volume info: %s\n"), dir->msg);
       return false;
    }
    vol.InChanger = InChanger;        /* bool in structure */
    unbash_spaces(vol.VolCatName);
    bstrncpy(dcr->VolumeName, vol.VolCatName, sizeof(dcr->VolumeName));
    memcpy(&dcr->VolCatInfo, &vol, sizeof(dcr->VolCatInfo));

    Dmsg2(300, "do_reqest_vol_info got slot=%d Volume=%s\n",
          vol.Slot, vol.VolCatName);
    return true;
}


/*
 * Get Volume info for a specific volume from the Director's Database
 *
 * Returns: true  on success   (not Director guarantees that Pool and MediaType
 *                             are correct and VolStatus==Append or
 *                             VolStatus==Recycle)
 *          false on failure
 *
 *          Volume information returned in dcr->VolCatInfo
 */
bool dir_get_volume_info(DCR *dcr, enum get_vol_info_rw writing)
{
    JCR *jcr = dcr->jcr;
    BSOCK *dir = jcr->dir_bsock;

    bstrncpy(dcr->VolCatInfo.VolCatName, dcr->VolumeName, sizeof(dcr->VolCatInfo.VolCatName));
    bash_spaces(dcr->VolCatInfo.VolCatName);
    bnet_fsend(dir, Get_Vol_Info, jcr->Job, dcr->VolCatInfo.VolCatName,
       writing==GET_VOL_INFO_FOR_WRITE?1:0);
    Dmsg1(100, ">dird: %s", dir->msg);
    return do_get_volume_info(dcr);
}



/*
 * Get info on the next appendable volume in the Director's database
 * Returns: true  on success
 *          false on failure
 *
 *          Volume information returned in dcr
 *
 */
bool dir_find_next_appendable_volume(DCR *dcr)
{
    JCR *jcr = dcr->jcr;
    BSOCK *dir = jcr->dir_bsock;
    JCR *njcr;

    Dmsg0(200, "dir_find_next_appendable_volume\n");
    /*
     * Try the three oldest or most available volumes.  Note,
     *   the most available could already be mounted on another
     *   drive, so we continue looking for a not in use Volume.
     */
    for (int vol_index=1;  vol_index < 3; vol_index++) {
       bash_spaces(dcr->media_type);
       bash_spaces(dcr->pool_name);
       bnet_fsend(dir, Find_media, jcr->Job, vol_index, dcr->pool_name, dcr->media_type);
       unbash_spaces(dcr->media_type);
       unbash_spaces(dcr->pool_name);
       Dmsg1(100, ">dird: %s", dir->msg);
       if (do_get_volume_info(dcr)) {
          Dmsg2(300, "JobId=%d got possible Vol=%s\n", jcr->JobId, dcr->VolumeName);
          bool found = false;
          /*
           * Walk through all jobs and see if the volume is
           *  already mounted. If so, try a different one.
           * This would be better done by walking through
           *  all the devices.
           */
          lock_jcr_chain();
          foreach_jcr(njcr) {
             if (jcr == njcr) {
                free_locked_jcr(njcr);
                continue;             /* us */
             }
             Dmsg2(300, "Compare to JobId=%d using Vol=%s\n", njcr->JobId, njcr->dcr->VolumeName);
             if (njcr->dcr && strcmp(dcr->VolumeName, njcr->dcr->VolumeName) == 0) {
                found = true;
                Dmsg1(400, "Vol in use by JobId=%u\n", njcr->JobId);
                free_locked_jcr(njcr);
                break;
             }
             free_locked_jcr(njcr);
          }
          unlock_jcr_chain();
          if (!found) {
             Dmsg0(400, "dir_find_next_appendable_volume return true\n");
             return true;             /* Got good Volume */
          }
       } else {
          Dmsg0(200, "No volume info, return false\n");
          return false;
       }
    }
    Dmsg0(400, "dir_find_next_appendable_volume return true\n");
    return true;
}


/*
 * After writing a Volume, send the updated statistics
 * back to the director. The information comes from the
 * dev record.
 */
bool dir_update_volume_info(DCR *dcr, bool label)
{
   JCR *jcr = dcr->jcr;
   BSOCK *dir = jcr->dir_bsock;
   DEVICE *dev = dcr->dev;
   time_t LastWritten = time(NULL);
   char ed1[50], ed2[50], ed3[50], ed4[50];
   VOLUME_CAT_INFO *vol = &dev->VolCatInfo;
   int InChanger;
   POOL_MEM VolumeName;

   if (vol->VolCatName[0] == 0) {
      Jmsg0(jcr, M_FATAL, 0, _("NULL Volume name. This shouldn't happen!!!\n"));
      Pmsg0(000, "NULL Volume name. This shouldn't happen!!!\n");
      return false;
   }
   if (dev->can_read()) {
      Jmsg0(jcr, M_FATAL, 0, _("Attempt to update_volume_info in read mode!!!\n"));
      Pmsg0(000, "Attempt to update_volume_info in read mode!!!\n");
      return false;
   }

   Dmsg1(300, "Update cat VolFiles=%d\n", dev->file);
   /* Just labeled or relabeled the tape */
   if (label) {
      bstrncpy(vol->VolCatStatus, "Append", sizeof(vol->VolCatStatus));
      vol->VolCatBytes = 1;           /* indicates tape labeled */
   }
   pm_strcpy(VolumeName, vol->VolCatName);
   bash_spaces(VolumeName);
   InChanger = vol->InChanger;
   bnet_fsend(dir, Update_media, jcr->Job,
      VolumeName.c_str(), vol->VolCatJobs, vol->VolCatFiles,
      vol->VolCatBlocks, edit_uint64(vol->VolCatBytes, ed1),
      vol->VolCatMounts, vol->VolCatErrors,
      vol->VolCatWrites, edit_uint64(vol->VolCatMaxBytes, ed2),
      LastWritten, vol->VolCatStatus, vol->Slot, label,
      InChanger,                      /* bool in structure */
      edit_uint64(vol->VolReadTime, ed3),
      edit_uint64(vol->VolWriteTime, ed4),
      vol->VolCatParts);
    Dmsg1(100, ">dird: %s", dir->msg);

   if (!do_get_volume_info(dcr)) {
      Jmsg(jcr, M_FATAL, 0, "%s", jcr->errmsg);
      Pmsg2(000, "Didn't get vol info vol=%s: ERR=%s", 
         vol->VolCatName, jcr->errmsg);
      return false;
   }
   Dmsg1(420, "get_volume_info(): %s", dir->msg);
   /* Update dev Volume info in case something changed (e.g. expired) */
   memcpy(&dev->VolCatInfo, &dcr->VolCatInfo, sizeof(dev->VolCatInfo));
   return true;
}

/*
 * After writing a Volume, create the JobMedia record.
 */
bool dir_create_jobmedia_record(DCR *dcr)
{
   JCR *jcr = dcr->jcr;
   BSOCK *dir = jcr->dir_bsock;

   if (!dcr->WroteVol) {
      return true;                    /* nothing written to tape */
   }

   dcr->WroteVol = false;
   bnet_fsend(dir, Create_job_media, jcr->Job,
      dcr->VolFirstIndex, dcr->VolLastIndex,
      dcr->StartFile, dcr->EndFile,
      dcr->StartBlock, dcr->EndBlock, 
      dcr->Copy, dcr->Stripe);
    Dmsg1(100, ">dird: %s", dir->msg);
   if (bnet_recv(dir) <= 0) {
      Dmsg0(190, "create_jobmedia error bnet_recv\n");
      Jmsg(jcr, M_FATAL, 0, _("Error creating JobMedia record: ERR=%s\n"),
           bnet_strerror(dir));
      return false;
   }
   Dmsg1(100, "<dir: %s", dir->msg);
   if (strcmp(dir->msg, OK_create) != 0) {
      Dmsg1(130, "Bad response from Dir: %s\n", dir->msg);
      Jmsg(jcr, M_FATAL, 0, _("Error creating JobMedia record: %s\n"), dir->msg);
      return false;
   }
   return true;
}


/*
 * Update File Attribute data
 */
bool dir_update_file_attributes(DCR *dcr, DEV_RECORD *rec)
{
   JCR *jcr = dcr->jcr;
   BSOCK *dir = jcr->dir_bsock;
   ser_declare;

#ifdef NO_ATTRIBUTES_TEST
   return true;
#endif

   dir->msglen = sprintf(dir->msg, FileAttributes, jcr->Job);
   dir->msg = check_pool_memory_size(dir->msg, dir->msglen +
                sizeof(DEV_RECORD) + rec->data_len);
   ser_begin(dir->msg + dir->msglen, 0);
   ser_uint32(rec->VolSessionId);
   ser_uint32(rec->VolSessionTime);
   ser_int32(rec->FileIndex);
   ser_int32(rec->Stream);
   ser_uint32(rec->data_len);
   ser_bytes(rec->data, rec->data_len);
   dir->msglen = ser_length(dir->msg);
   Dmsg1(1800, ">dird: %s\n", dir->msg);    /* Attributes */
   return bnet_send(dir);
}


/*
 *   Request the sysop to create an appendable volume
 *
 *   Entered with device blocked.
 *   Leaves with device blocked.
 *
 *   Returns: true  on success (operator issues a mount command)
 *            false on failure
 *              Note, must create dev->errmsg on error return.
 *
 *    On success, dcr->VolumeName and dcr->VolCatInfo contain
 *      information on suggested volume, but this may not be the
 *      same as what is actually mounted.
 *
 *    When we return with success, the correct tape may or may not
 *      actually be mounted. The calling routine must read it and
 *      verify the label.
 */
bool dir_ask_sysop_to_create_appendable_volume(DCR *dcr)
{
   int stat = 0, jstat;
   bool unmounted;
   bool first = true;
   DEVICE *dev = dcr->dev;
   JCR *jcr = dcr->jcr;

   Dmsg0(400, "enter dir_ask_sysop_to_create_appendable_volume\n");
   ASSERT(dev->dev_blocked);
   for ( ;; ) {
      if (job_canceled(jcr)) {
         Mmsg(dev->errmsg,
              _("Job %s canceled while waiting for mount on Storage Device \"%s\".\n"),
              jcr->Job, dev->print_name());
         Jmsg(jcr, M_INFO, 0, "%s", dev->errmsg);
         return false;
      }
      /* First pass, we *know* there are no appendable volumes, so no need to call */
      if (!first && dir_find_next_appendable_volume(dcr)) { /* get suggested volume */
         unmounted = (dev->dev_blocked == BST_UNMOUNTED) ||
                     (dev->dev_blocked == BST_UNMOUNTED_WAITING_FOR_SYSOP);
         /*
          * If we have a valid volume name and we are not
          *   removable media, return now, or if we have a
          *   Slot for an autochanger, otherwise wait
          *   for the operator to mount the media.
          */
         if (!unmounted && ((dcr->VolumeName[0] && !dev_cap(dev, CAP_REM) &&
                dev_cap(dev, CAP_LABEL)) ||
                 (dcr->VolumeName[0] && dcr->VolCatInfo.Slot))) {
            Dmsg0(400, "Return 1 from mount without wait.\n");
            return true;
         }
         jstat = JS_WaitMount;
         if (!dev->poll) {
            Jmsg(jcr, M_MOUNT, 0, _(
"Please mount Volume \"%s\" on Storage Device %s for Job %s\n"
"Use \"mount\" command to release Job.\n"),
              dcr->VolumeName, dev->print_name(), jcr->Job);
            Dmsg3(400, "Mount %s on %s for Job %s\n",
                  dcr->VolumeName, dcr->dev_name, jcr->Job);
         }
      } else {
         jstat = JS_WaitMedia;
         if (!dev->poll) {
            Jmsg(jcr, M_MOUNT, 0, _(
"Job %s waiting. Cannot find any appendable volumes.\n"
"Please use the \"label\"  command to create a new Volume for:\n"
"    Storage:      %s\n"
"    Media type:   %s\n"
"    Pool:         %s\n"),
               jcr->Job,
               dev->print_name(),
               dcr->media_type,
               dcr->pool_name);
         }
      }
      first = false;

      jcr->JobStatus = jstat;
      dir_send_job_status(jcr);

      stat = wait_for_sysop(dcr);
      if (dev->poll) {
         Dmsg1(400, "Poll timeout in create append vol on device %s\n", dev->print_name());
         continue;
      }

      if (stat == ETIMEDOUT) {
         if (!double_dev_wait_time(dev)) {
            Mmsg(dev->errmsg, _("Max time exceeded waiting to mount Storage Device %s for Job %s\n"),
               dev->print_name(), jcr->Job);
            Jmsg(jcr, M_FATAL, 0, "%s", dev->errmsg);
            Dmsg1(400, "Gave up waiting on device %s\n", dev->print_name());
            return false;             /* exceeded maximum waits */
         }
         continue;
      }
      if (stat == EINVAL) {
         berrno be;
         Mmsg2(dev->errmsg, _("pthread error in mount_next_volume stat=%d ERR=%s\n"),
               stat, be.strerror(stat));
         Jmsg(jcr, M_FATAL, 0, "%s", dev->errmsg);
         return false;
      }
      if (stat != 0) {
         berrno be;
         Jmsg(jcr, M_WARNING, 0, _("pthread error in mount_next_volume stat=%d ERR=%s\n"), stat,
            be.strerror(stat));
      }
      Dmsg1(400, "Someone woke me for device %s\n", dev->print_name());

      /* If no VolumeName, and cannot get one, try again */
      if (dcr->VolumeName[0] == 0 && !job_canceled(jcr) &&
          !dir_find_next_appendable_volume(dcr)) {
         Jmsg(jcr, M_MOUNT, 0, _(
"Someone woke me up, but I cannot find any appendable\n"
"volumes for Job=%s.\n"), jcr->Job);
         /* Restart wait counters after user interaction */
         init_device_wait_timers(dcr);
         continue;
      }
      unmounted = (dev->dev_blocked == BST_UNMOUNTED) ||
                  (dev->dev_blocked == BST_UNMOUNTED_WAITING_FOR_SYSOP);
      if (unmounted) {
         continue;                    /* continue to wait */
      }

      /*
       * Device mounted, we have a volume, break and return
       */
      break;
   }
   set_jcr_job_status(jcr, JS_Running);
   dir_send_job_status(jcr);
   Dmsg0(400, "leave dir_ask_sysop_to_mount_create_appendable_volume\n");
   return true;
}

/*
 *   Request to mount specific Volume
 *
 *   Entered with device blocked and dcr->VolumeName is desired
 *      volume.
 *   Leaves with device blocked.
 *
 *   Returns: true  on success (operator issues a mount command)
 *            false on failure
 *                  Note, must create dev->errmsg on error return.
 *
 */
bool dir_ask_sysop_to_mount_volume(DCR *dcr)
{
   int stat = 0;
   const char *msg;
   DEVICE *dev = dcr->dev;
   JCR *jcr = dcr->jcr;

   Dmsg0(400, "enter dir_ask_sysop_to_mount_volume\n");
   if (!dcr->VolumeName[0]) {
      Mmsg0(dev->errmsg, _("Cannot request another volume: no volume name given.\n"));
      return false;
   }
   ASSERT(dev->dev_blocked);
   for ( ;; ) {
      if (job_canceled(jcr)) {
         Mmsg(dev->errmsg, _("Job %s canceled while waiting for mount on Storage Device %s.\n"),
              jcr->Job, dev->print_name());
         return false;
      }

      if (!dev->poll) {
         msg = _("Please mount");
         Jmsg(jcr, M_MOUNT, 0, _("%s Volume \"%s\" on Storage Device %s for Job %s\n"),
              msg, dcr->VolumeName, dev->print_name(), jcr->Job);
         Dmsg3(400, "Mount \"%s\" on device \"%s\" for Job %s\n",
               dcr->VolumeName, dcr->dev_name, jcr->Job);
      }

      jcr->JobStatus = JS_WaitMount;
      dir_send_job_status(jcr);

      stat = wait_for_sysop(dcr);    ;     /* wait on device */
      if (dev->poll) {
         Dmsg1(400, "Poll timeout in mount vol on device %s\n", dev->print_name());
         Dmsg1(400, "Blocked=%s\n", edit_blocked_reason(dev));
         return true;
      }

      if (stat == ETIMEDOUT) {
         if (!double_dev_wait_time(dev)) {
            Mmsg(dev->errmsg, _("Max time exceeded waiting to mount Storage Device %s for Job %s\n"),
               dev->print_name(), jcr->Job);
            Jmsg(jcr, M_FATAL, 0, "%s", dev->errmsg);
            Dmsg1(400, "Gave up waiting on device %s\n", dev->print_name());
            return false;             /* exceeded maximum waits */
         }
         continue;
      }
      if (stat == EINVAL) {
         berrno be;
         Mmsg2(dev->errmsg, _("pthread error in mount_volume stat=%d ERR=%s\n"),
               stat, be.strerror(stat));
         Jmsg(jcr, M_FATAL, 0, "%s", dev->errmsg);
         return false;
      }
      if (stat != 0) {
         berrno be;
         Jmsg(jcr, M_FATAL, 0, _("pthread error in mount_next_volume stat=%d: ERR=%s\n"), stat,
            be.strerror(stat));
      }
      Dmsg1(400, "Someone woke me for device %s\n", dev->print_name());
      break;
   }
   set_jcr_job_status(jcr, JS_Running);
   dir_send_job_status(jcr);
   Dmsg0(400, "leave dir_ask_sysop_to_mount_volume\n");
   return true;
}
