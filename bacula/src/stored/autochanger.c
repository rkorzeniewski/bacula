/*
 *
 *  Routines for handling the autochanger.
 *
 *   Kern Sibbald, August MMII
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

#include "bacula.h"                   /* pull in global headers */
#include "stored.h"                   /* pull in Storage Deamon headers */

/* Forward referenced functions */
static char *edit_device_codes(JCR *jcr, char *omsg, char *imsg, char *cmd);


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
 *	     0 on failure
 */
int autoload_device(JCR *jcr, DEVICE *dev, int writing, BSOCK *dir)
{
   int slot = jcr->VolCatInfo.Slot;
   int rtn_stat = 0;
     
   /*
    * Handle autoloaders here.	If we cannot autoload it, we
    *  will return FALSE to ask the sysop.
    */
   if (writing && dev_cap(dev, CAP_AUTOCHANGER) && slot <= 0) {
      if (dir) {
	 return 0;		      /* For user, bail out right now */
      }
      if (dir_find_next_appendable_volume(jcr)) {
	 slot = jcr->VolCatInfo.Slot; 
      }
   }
   Dmsg1(100, "Want changer slot=%d\n", slot);

   if (slot > 0 && jcr->device->changer_name && jcr->device->changer_command) {
      uint32_t timeout = jcr->device->max_changer_wait;
      POOLMEM *changer, *results;
      int status, loaded;

      results = get_pool_memory(PM_MESSAGE);
      changer = get_pool_memory(PM_FNAME);

      /* Find out what is loaded, zero means device is unloaded */
      changer = edit_device_codes(jcr, changer, jcr->device->changer_command, 
                   "loaded");
      status = run_program(changer, timeout, results);
      Dmsg3(100, "run_prog: %s stat=%d result=%s\n", changer, status, results);
      if (status == 0) {
	 loaded = atoi(results);
      } else {
	 if (dir) {
            bnet_fsend(dir, _("3991 Bad autochanger \"loaded\" status = %d.\n"),
	       status);
	 } else {
            Jmsg(jcr, M_INFO, 0, _("Bad autochanger \"load slot\" status = %d.\n"),
	       status);
	 }
	 loaded = -1;		   /* force unload */
      }
      Dmsg1(100, "loaded=%s\n", results);

      /* If bad status or tape we want is not loaded, load it. */
      if (status != 0 || loaded != slot) { 
	 if (dev_cap(dev, CAP_OFFLINEUNMOUNT)) {
	    offline_dev(dev);
	 }
	 /* We are going to load a new tape, so close the device */
	 force_close_dev(dev);
	 if (loaded != 0) {	   /* must unload drive */
            Dmsg0(100, "Doing changer unload.\n");
	    if (dir) {
               bnet_fsend(dir, _("3902 Issuing autochanger \"unload\" command.\n"));
	    } else {
               Jmsg(jcr, M_INFO, 0, _("Issuing autochanger \"unload\" command.\n"));
	    }
	    changer = edit_device_codes(jcr, changer, 
                        jcr->device->changer_command, "unload");
	    status = run_program(changer, timeout, NULL);
            Dmsg1(100, "unload status=%d\n", status);
	 }
	 /*
	  * Load the desired cassette	 
	  */
         Dmsg1(100, "Doing changer load slot %d\n", slot);
	 if (dir) {
            bnet_fsend(dir, _("3903 Issuing autochanger \"load slot %d\" command.\n"),
	       slot);
	 } else {
            Jmsg(jcr, M_INFO, 0, _("Issuing autochanger \"load slot %d\" command.\n"),
	       slot);
	 }
	 changer = edit_device_codes(jcr, changer, 
                      jcr->device->changer_command, "load");
	 status = run_program(changer, timeout, NULL);
	 if (status == 0) {
	    if (dir) {
               bnet_fsend(dir, _("3904 Autochanger \"load slot\" status is OK.\n"));
	    } else {
               Jmsg(jcr, M_INFO, 0, _("Autochanger \"load slot\" status is OK.\n"));
	    }
	 } else {
	    if (dir) {
               bnet_fsend(dir, _("3992 Bad autochanger \"load slot\" status = %d.\n"),
		  status);
	    } else {
               Jmsg(jcr, M_INFO, 0, _("Bad autochanger \"load slot\" status = %d.\n"),
		  status);
	    }
	 }
         Dmsg2(100, "load slot %d status=%d\n", slot, status);
      }
      free_pool_memory(changer);
      free_pool_memory(results);
      Dmsg1(100, "After changer, status=%d\n", status);
      if (status == 0) {	   /* did we succeed? */
	 rtn_stat = 1;		   /* tape loaded by changer */
      }
   }
   return rtn_stat;
}

/*
 * List the Volumes that are in the autoloader possibly
 *   with their barcodes.
 *   We assume that it is always the Console that is calling us.
 */
int autochanger_list(JCR *jcr, DEVICE *dev, BSOCK *dir)
{
   uint32_t timeout = jcr->device->max_changer_wait;
   POOLMEM *changer;
   BPIPE *bpipe;
   int len = sizeof_pool_memory(dir->msg) - 1;

   if (!dev_cap(dev, CAP_AUTOCHANGER) || !jcr->device->changer_name ||
       !jcr->device->changer_command) {
      bnet_fsend(dir, _("3993 Not a changer device.\n"));
      return 0;
   }

   changer = get_pool_memory(PM_FNAME);
   if (dev_cap(dev, CAP_OFFLINEUNMOUNT)) {
      offline_dev(dev);
   }
   /* We are going to load a new tape, so close the device */
   force_close_dev(dev);

   /* First unload any tape */
   bnet_fsend(dir, _("3902 Issuing autochanger \"unload\" command.\n"));
   changer = edit_device_codes(jcr, changer, jcr->device->changer_command, "unload");
   run_program(changer, timeout, NULL);

   /* Now list slots occupied */
   changer = edit_device_codes(jcr, changer, jcr->device->changer_command, "list");
   bnet_fsend(dir, _("3903 Issuing autochanger \"list\" command.\n"));
   bpipe = open_bpipe(changer, timeout, "r");
   if (!bpipe) {
      bnet_fsend(dir, _("3994 Open bpipe failed.\n"));
      goto bail_out;
   }
   /* Get output from changer */
   while (fgets(dir->msg, len, bpipe->rfd)) { 
      dir->msglen = strlen(dir->msg);
      bnet_send(dir);
   }
   bnet_sig(dir, BNET_EOD);
   close_bpipe(bpipe);

bail_out:
   free_pool_memory(changer);
   return 1;
}



/*
 * Edit codes into ChangerCommand
 *  %% = %
 *  %a = archive device name
 *  %c = changer device name
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
static char *edit_device_codes(JCR *jcr, char *omsg, char *imsg, char *cmd) 
{
   char *p;
   const char *str;
   char add[20];

   *omsg = 0;
   Dmsg1(200, "edit_device_codes: %s\n", imsg);
   for (p=imsg; *p; p++) {
      if (*p == '%') {
	 switch (*++p) {
         case '%':
            str = "%";
	    break;
         case 'a':
	    str = jcr->device->dev->dev_name;
	    break;
         case 'c':
	    str = NPRT(jcr->device->changer_name);
	    break;
         case 'o':
	    str = NPRT(cmd);
	    break;
         case 's':
            sprintf(add, "%d", jcr->VolCatInfo.Slot - 1);
	    str = add;
	    break;
         case 'S':
            sprintf(add, "%d", jcr->VolCatInfo.Slot);
	    str = add;
	    break;
         case 'j':                    /* Job name */
	    str = jcr->Job;
	    break;
         case 'v':
	    str = NPRT(jcr->VolumeName);
	    break;
         case 'f':
	    str = NPRT(jcr->client_name);
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
      Dmsg1(200, "add_str %s\n", str);
      pm_strcat(&omsg, (char *)str);
      Dmsg1(200, "omsg=%s\n", omsg);
   }
   return omsg;
}
