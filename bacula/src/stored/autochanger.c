/*
 *
 *  Routines for handling the autochanger.
 *
 *   Kern Sibbald, August MMII
 *                            
 *   Version $Id$
 */
/*
   Copyright (C) 2002-2005 Kern Sibbald

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   version 2 as amended with additional clauses defined in the
   file LICENSE in the main source directory.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the 
   the file LICENSE for additional details.

 */

#include "bacula.h"                   /* pull in global headers */
#include "stored.h"                   /* pull in Storage Deamon headers */

/* Forward referenced functions */
static void lock_changer(DCR *dcr);
static void unlock_changer(DCR *dcr);

/*
 * Called here to do an autoload using the autochanger, if
 *  configured, and if a Slot has been defined for this Volume.
 *  On success this routine loads the indicated tape, but the
 *  label is not read, so it must be verified.
 *
 *  Note if dir is not NULL, it is the console requesting the
 *   autoload for labeling, so we respond directly to the
 *   dir bsock.
 *
 *  Returns: 1 on success
 *           0 on failure (no changer available)
 *          -1 on error on autochanger
 */
int autoload_device(DCR *dcr, int writing, BSOCK *dir)
{
   JCR *jcr = dcr->jcr;
   DEVICE *dev = dcr->dev;
   int slot;
   int drive = dev->drive_index;
   int rtn_stat = -1;                 /* error status */
   POOLMEM *changer;

   slot = dcr->VolCatInfo.InChanger ? dcr->VolCatInfo.Slot : 0;
   /*
    * Handle autoloaders here.  If we cannot autoload it, we
    *  will return 0 so that the sysop will be asked to load it.
    */
   if (writing && dev->is_autochanger() && slot <= 0) {
      if (dir) {
         return 0;                    /* For user, bail out right now */
      }
      if (dir_find_next_appendable_volume(dcr)) {
         slot = dcr->VolCatInfo.InChanger ? dcr->VolCatInfo.Slot : 0;
      } else {
         slot = 0;
      }
   }
   Dmsg1(400, "Want changer slot=%d\n", slot);

   changer = get_pool_memory(PM_FNAME);
   if (slot > 0 && dcr->device->changer_name && dcr->device->changer_command) {
      uint32_t timeout = dcr->device->max_changer_wait;
      int loaded, status;

      loaded = get_autochanger_loaded_slot(dcr);

      /* If tape we want is not loaded, load it. */
      if (loaded != slot) {
         if (!unload_autochanger(dcr, loaded)) {
            goto bail_out;
         }
         /*
          * Load the desired cassette
          */
         Dmsg1(400, "Doing changer load slot %d\n", slot);
         Jmsg(jcr, M_INFO, 0,
              _("3304 Issuing autochanger \"load slot %d, drive %d\" command.\n"),
              slot, drive);
         dcr->VolCatInfo.Slot = slot;    /* slot to be loaded */
         changer = edit_device_codes(dcr, changer, 
                      dcr->device->changer_command, "load");
         status = run_program(changer, timeout, NULL);
         if (status == 0) {
            Jmsg(jcr, M_INFO, 0, _("3305 Autochanger \"load slot %d, drive %d\", status is OK.\n"),
                    slot, drive);
            dev->Slot = slot;         /* set currently loaded slot */
         } else {
           berrno be;
           be.set_errno(status);
            Jmsg(jcr, M_FATAL, 0, _("3992 Bad autochanger \"load slot %d, drive %d\": ERR=%s.\n"),
                    slot, drive, be.strerror());
            goto bail_out;
         }
         unlock_changer(dcr);
         Dmsg2(400, "load slot %d status=%d\n", slot, status);
      } else {
         status = 0;                  /* we got what we want */
         dev->Slot = slot;            /* set currently loaded slot */
      }
      Dmsg1(400, "After changer, status=%d\n", status);
      if (status == 0) {              /* did we succeed? */
         rtn_stat = 1;                /* tape loaded by changer */
      }
   } else {
      rtn_stat = 0;                   /* no changer found */
   }
   free_pool_memory(changer);
   return rtn_stat;

bail_out:
   free_pool_memory(changer);
   unlock_changer(dcr);
   return -1;

}

/*
 * Returns: -1 if error from changer command
 *          slot otherwise
 */
int get_autochanger_loaded_slot(DCR *dcr)
{
   JCR *jcr = dcr->jcr;
   POOLMEM *changer, *results;
   int status, loaded;
   uint32_t timeout = dcr->device->max_changer_wait;
   int drive = dcr->dev->drive_index;

   results = get_pool_memory(PM_MESSAGE);
   changer = get_pool_memory(PM_FNAME);

   lock_changer(dcr);

   /* Find out what is loaded, zero means device is unloaded */
   Jmsg(jcr, M_INFO, 0, _("3301 Issuing autochanger \"loaded drive %d\" command.\n"),
        drive);
   changer = edit_device_codes(dcr, changer, dcr->device->changer_command, "loaded");
   *results = 0;
   status = run_program(changer, timeout, results);
   Dmsg3(50, "run_prog: %s stat=%d result=%s\n", changer, status, results);
   if (status == 0) {
      loaded = atoi(results);
      if (loaded > 0) {
         Jmsg(jcr, M_INFO, 0, _("3302 Autochanger \"loaded drive %d\", result is Slot %d.\n"),
              drive, loaded);
         dcr->dev->Slot = loaded;
      } else {
         Jmsg(jcr, M_INFO, 0, _("3302 Autochanger \"loaded drive %d\", result: nothing loaded.\n"),
              drive);
         dcr->dev->Slot = 0;
      }
   } else {
      berrno be;
      be.set_errno(status);
      Jmsg(jcr, M_INFO, 0, _("3991 Bad autochanger \"loaded drive %d\" command: ERR=%s.\n"),
           drive, be.strerror());
      loaded = -1;              /* force unload */
   }
   unlock_changer(dcr);
   free_pool_memory(changer);
   free_pool_memory(results);
   return loaded;
}

static void lock_changer(DCR *dcr)
{
   AUTOCHANGER *changer_res = dcr->device->changer_res;
   if (changer_res) {
      Dmsg1(100, "Locking changer %s\n", changer_res->hdr.name);
      P(changer_res->changer_mutex);  /* Lock changer script */
   }
}

static void unlock_changer(DCR *dcr)
{
   AUTOCHANGER *changer_res = dcr->device->changer_res;
   if (changer_res) {
      Dmsg1(100, "Unlocking changer %s\n", changer_res->hdr.name);
      V(changer_res->changer_mutex);  /* Unlock changer script */
   }
}

/*
 * Unload the volume, if any, in this drive
 */
bool unload_autochanger(DCR *dcr, int loaded)
{
   DEVICE *dev = dcr->dev;
   JCR *jcr = dcr->jcr;
   int slot;
   uint32_t timeout = dcr->device->max_changer_wait;

   if (!dev->is_autochanger() || !dcr->device->changer_name ||
       !dcr->device->changer_command) {
      return false;
   }

   offline_or_rewind_dev(dev);
   /* We are going to load a new tape, so close the device */
   force_close_device(dev);

   if (loaded <= 0) {
      loaded = get_autochanger_loaded_slot(dcr);
   }
   if (loaded > 0) {
      POOLMEM *changer = get_pool_memory(PM_FNAME);
      Jmsg(jcr, M_INFO, 0,
           _("3307 Issuing autochanger \"unload slot %d, drive %d\" command.\n"),
           loaded, dev->drive_index);
      slot = dcr->VolCatInfo.Slot;
      dcr->VolCatInfo.Slot = loaded;
      changer = edit_device_codes(dcr, changer, 
                   dcr->device->changer_command, "unload");
      lock_changer(dcr);
      int stat = run_program(changer, timeout, NULL);
      unlock_changer(dcr);
      dcr->VolCatInfo.Slot = slot;
      if (stat != 0) {
         berrno be;
         be.set_errno(stat);
         Jmsg(jcr, M_INFO, 0, _("3995 Bad autochanger \"unload slot %d, drive %d\": ERR=%s.\n"),
                 slot, dev->drive_index, be.strerror());
         free_pool_memory(changer);
         return false;
      } else {
         dev->Slot = 0;            /* nothing loaded */
      }
      free_pool_memory(changer);
   }
   return true;
}


/*
 * List the Volumes that are in the autoloader possibly
 *   with their barcodes.
 *   We assume that it is always the Console that is calling us.
 */
bool autochanger_cmd(DCR *dcr, BSOCK *dir, const char *cmd)  
{
   DEVICE *dev = dcr->dev;
   uint32_t timeout = dcr->device->max_changer_wait;
   POOLMEM *changer;
   BPIPE *bpipe;
   int len = sizeof_pool_memory(dir->msg) - 1;
   bool ok = false;
   int stat;

   if (!dev->is_autochanger() || !dcr->device->changer_name ||
       !dcr->device->changer_command) {
      bnet_fsend(dir, _("3993 Device %s not an autochanger device.\n"),
         dev->print_name());
      return false;
   }

   changer = get_pool_memory(PM_FNAME);
   /* List command? */
   if (strcmp(cmd, "list") == 0) {
      unload_autochanger(dcr, 0);
   }

   /* Now issue the command */
   changer = edit_device_codes(dcr, changer, 
                 dcr->device->changer_command, cmd);
   bnet_fsend(dir, _("3306 Issuing autochanger \"%s\" command.\n"), cmd);
   lock_changer(dcr);
   bpipe = open_bpipe(changer, timeout, "r");
   if (!bpipe) {
      unlock_changer(dcr);
      bnet_fsend(dir, _("3996 Open bpipe failed.\n"));
      goto bail_out;
   }
   if (strcmp(cmd, "list") == 0) {
      /* Get output from changer */
      while (fgets(dir->msg, len, bpipe->rfd)) {
         dir->msglen = strlen(dir->msg);
         Dmsg1(100, "<stored: %s\n", dir->msg);
         bnet_send(dir);
      }
   } else {
      /* For slots command, read a single line */
      bstrncpy(dir->msg, "slots=", len);
      fgets(dir->msg+6, len-6, bpipe->rfd);
      dir->msglen = strlen(dir->msg);
      Dmsg1(100, "<stored: %s", dir->msg);
      bnet_send(dir);
   }
                 
   stat = close_bpipe(bpipe);
   unlock_changer(dcr);
   if (stat != 0) {
      berrno be;
      be.set_errno(stat);
      bnet_fsend(dir, _("Autochanger error: ERR=%s\n"), be.strerror());
   }
   bnet_sig(dir, BNET_EOD);
   ok = true;

bail_out:
   free_pool_memory(changer);
   return true;
}


/*
 * Edit codes into ChangerCommand
 *  %% = %
 *  %a = archive device name
 *  %c = changer device name
 *  %d = changer drive index
 *  %f = Client's name
 *  %j = Job name
 *  %o = command
 *  %s = Slot base 0
 *  %S = Slot base 1
 *  %v = Volume name
 *
 *
 *  omsg = edited output message
 *  imsg = input string containing edit codes (%x)
 *  cmd = command string (load, unload, ...)
 *
 */
char *edit_device_codes(DCR *dcr, char *omsg, const char *imsg, const char *cmd)
{
   const char *p;
   const char *str;
   char add[20];

   *omsg = 0;
   Dmsg1(1800, "edit_device_codes: %s\n", imsg);
   for (p=imsg; *p; p++) {
      if (*p == '%') {
         switch (*++p) {
         case '%':
            str = "%";
            break;
         case 'a':
            str = dcr->dev->archive_name();
            break;
         case 'c':
            str = NPRT(dcr->device->changer_name);
            break;
         case 'd':
            sprintf(add, "%d", dcr->dev->drive_index);
            str = add;
            break;
         case 'o':
            str = NPRT(cmd);
            break;
         case 's':
            sprintf(add, "%d", dcr->VolCatInfo.Slot - 1);
            str = add;
            break;
         case 'S':
            sprintf(add, "%d", dcr->VolCatInfo.Slot);
            str = add;
            break;
         case 'j':                    /* Job name */
            str = dcr->jcr->Job;
            break;
         case 'v':
            str = NPRT(dcr->VolumeName);
            break;
         case 'f':
            str = NPRT(dcr->jcr->client_name);
            break;

         default:
            add[0] = '%';
            add[1] = *p;
            add[2] = 0;
            str = add;
            break;
         }
      } else {
         add[0] = *p;
         add[1] = 0;
         str = add;
      }
      Dmsg1(1900, "add_str %s\n", str);
      pm_strcat(&omsg, (char *)str);
      Dmsg1(1800, "omsg=%s\n", omsg);
   }
   Dmsg1(800, "omsg=%s\n", omsg);
   return omsg;
}
