/*
 *  This file handles accepting Director Commands
 *
 *    Most Director commands are handled here, with the 
 *    exception of the Job command command and subsequent 
 *    subcommands that are handled
 *    in job.c.  
 *
 *    N.B. in this file, in general we must use P(dev->mutex) rather
 *	than lock_device(dev) so that we can examine the blocked
 *      state rather than blocking ourselves. In some "safe" cases,
 *	we can do things to a blocked device. CAREFUL!!!!
 *
 *    File daemon commands are handled in fdcmd.c
 *
 *     Kern Sibbald, May MMI
 *
 *   Version $Id$
 *  
 */
/*
   Copyright (C) 2000-2004 Kern Sibbald and John Walker

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
#include "stored.h"

/* Exported variables */

/* Imported variables */
extern BSOCK *filed_chan;
extern int r_first, r_last;
extern struct s_res resources[];
extern char my_name[];
extern time_t daemon_start_time;
extern struct s_last_job last_job;

/* Static variables */
static char derrmsg[]       = "3900 Invalid command\n";
static char OKsetdebug[]   = "3000 OK setdebug=%d\n";


/* Imported functions */
extern void terminate_child();
extern int job_cmd(JCR *jcr);
extern int status_cmd(JCR *sjcr);

/* Forward referenced functions */
static int label_cmd(JCR *jcr);
static int relabel_cmd(JCR *jcr);
static int readlabel_cmd(JCR *jcr);
static int release_cmd(JCR *jcr);
static int setdebug_cmd(JCR *jcr);
static int cancel_cmd(JCR *cjcr);
static int mount_cmd(JCR *jcr);
static int unmount_cmd(JCR *jcr);
static int autochanger_cmd(JCR *sjcr);
static int do_label(JCR *jcr, int relabel);
static bool find_device(JCR *jcr, char *dname);
static void read_volume_label(JCR *jcr, DEVICE *dev, int Slot);
static void label_volume_if_ok(JCR *jcr, DEVICE *dev, char *oldname,
			       char *newname, char *poolname, 
			       int Slot, int relabel);

struct s_cmds {
   char *cmd;
   int (*func)(JCR *jcr);
};

/*  
 * The following are the recognized commands from the Director. 
 */
static struct s_cmds cmds[] = {
   {"JobId=",    job_cmd},            /* start Job */
   {"setdebug=", setdebug_cmd},       /* set debug level */
   {"cancel",    cancel_cmd},
   {"label",     label_cmd},          /* label a tape */
   {"relabel",   relabel_cmd},        /* relabel a tape */
   {"mount",     mount_cmd},
   {"unmount",   unmount_cmd},
   {"status",    status_cmd},
   {"autochanger", autochanger_cmd},
   {"release",   release_cmd},
   {"readlabel", readlabel_cmd},
   {NULL,	 NULL}		      /* list terminator */
};


/* 
 * Connection request. We accept connections either from the 
 *  Director or a Client (File daemon).
 * 
 * Note, we are running as a seperate thread of the Storage daemon.
 *  and it is because a Director has made a connection with
 *  us on the "Message" channel.    
 *
 * Basic tasks done here:  
 *  - Create a JCR record
 *  - If it was from the FD, call handle_filed_connection()
 *  - Authenticate the Director
 *  - We wait for a command
 *  - We execute the command
 *  - We continue or exit depending on the return status
 */
void *connection_request(void *arg)
{
   BSOCK *bs = (BSOCK *)arg;
   JCR *jcr;
   int i;
   bool found, quit;
   int bnet_stat = 0;
   char name[MAX_NAME_LENGTH];

   if (bnet_recv(bs) <= 0) {
      Emsg0(M_ERROR, 0, _("Connection request failed.\n"));
      return NULL;
   }

   /* 
    * Do a sanity check on the message received
    */
   if (bs->msglen < 25 || bs->msglen > (int)sizeof(name)-25) {
      Emsg1(M_ERROR, 0, _("Invalid Dir connection. Len=%d\n"), bs->msglen);
   }
   /* 
    * See if this is a File daemon connection. If so
    *	call FD handler.
    */
   if (sscanf(bs->msg, "Hello Start Job %127s calling\n", name) == 1) {
      handle_filed_connection(bs, name);
      return NULL;
   }
   
   jcr = new_jcr(sizeof(JCR), stored_free_jcr);     /* create Job Control Record */
   jcr->dir_bsock = bs; 	      /* save Director bsock */
   jcr->dir_bsock->jcr = jcr;

   Dmsg0(1000, "stored in start_job\n");

   /*
    * Authenticate the Director
    */
   if (!authenticate_director(jcr)) {
      Jmsg(jcr, M_FATAL, 0, _("Unable to authenticate Director\n"));
      free_jcr(jcr);
      return NULL;
   }
   Dmsg0(90, "Message channel init completed.\n");

   for (quit=0; !quit;) {
      /* Read command */
      if ((bnet_stat = bnet_recv(bs)) <= 0) {
	 break; 		      /* connection terminated */
      }
      Dmsg1(9, "<dird: %s\n", bs->msg);
      found = false;
      for (i=0; cmds[i].cmd; i++) {
	 if (strncmp(cmds[i].cmd, bs->msg, strlen(cmds[i].cmd)) == 0) {
	    if (!cmds[i].func(jcr)) {	 /* do command */
	       quit = true;		 /* error, get out */
               Dmsg1(90, "Command %s requsts quit\n", cmds[i].cmd);
	    }
	    found = true;	     /* indicate command found */
	    break;
	 }
      }
      if (!found) {		      /* command not found */
	 bnet_fsend(bs, derrmsg);
	 quit = true;
	 break;
      }
   }
   bnet_sig(bs, BNET_TERMINATE);
   free_jcr(jcr);
   return NULL;
}

/*
 * Set debug level as requested by the Director
 *
 */
static int setdebug_cmd(JCR *jcr)
{
   BSOCK *dir = jcr->dir_bsock;
   int level, trace_flag;

   Dmsg1(10, "setdebug_cmd: %s", dir->msg);
   if (sscanf(dir->msg, "setdebug=%d trace=%d", &level, &trace_flag) != 2 || level < 0) {
      bnet_fsend(dir, "3991 Bad setdebug command: %s\n", dir->msg);
      return 0;
   }
   debug_level = level;
   set_trace(trace_flag);
   return bnet_fsend(dir, OKsetdebug, level);
}


/*
 * Cancel a Job
 */
static int cancel_cmd(JCR *cjcr)
{
   BSOCK *dir = cjcr->dir_bsock;
   int oldStatus;
   char Job[MAX_NAME_LENGTH];
   JCR *jcr;

   if (sscanf(dir->msg, "cancel Job=%127s", Job) == 1) {
      if (!(jcr=get_jcr_by_full_name(Job))) {
         bnet_fsend(dir, _("3992 Job %s not found.\n"), Job);
      } else {
	 P(jcr->mutex);
	 oldStatus = jcr->JobStatus;
	 set_jcr_job_status(jcr, JS_Canceled);
	 if (!jcr->authenticated && oldStatus == JS_WaitFD) {
	    pthread_cond_signal(&jcr->job_start_wait); /* wake waiting thread */
	 }
	 V(jcr->mutex);
	 if (jcr->file_bsock) {
	    bnet_sig(jcr->file_bsock, BNET_TERMINATE);
	 }
	 /* If thread waiting on mount, wake him */
	 if (jcr->device && jcr->device->dev &&      
	      (jcr->device->dev->dev_blocked == BST_WAITING_FOR_SYSOP ||
	       jcr->device->dev->dev_blocked == BST_UNMOUNTED ||
	       jcr->device->dev->dev_blocked == BST_UNMOUNTED_WAITING_FOR_SYSOP)) {
	     pthread_cond_signal(&jcr->device->dev->wait_next_vol);
	 }
         bnet_fsend(dir, _("3000 Job %s marked to be canceled.\n"), jcr->Job);
	 free_jcr(jcr);
      }
   } else {
      bnet_fsend(dir, _("3993 Error scanning cancel command.\n"));
   }
   bnet_sig(dir, BNET_EOD);
   return 1;
}

/*
 * Label a tape
 *
 */
static int label_cmd(JCR *jcr) 
{
   return do_label(jcr, 0);
}

static int relabel_cmd(JCR *jcr) 
{
   return do_label(jcr, 1);
}

static int do_label(JCR *jcr, int relabel)  
{
   POOLMEM *dname, *newname, *oldname, *poolname, *mtype;
   BSOCK *dir = jcr->dir_bsock;
   DEVICE *dev;
   bool ok = false;
   int slot;	

   dname = get_memory(dir->msglen+1);
   newname = get_memory(dir->msglen+1);
   oldname = get_memory(dir->msglen+1);
   poolname = get_memory(dir->msglen+1);
   mtype = get_memory(dir->msglen+1);
   if (relabel) {
      if (sscanf(dir->msg, "relabel %s OldName=%s NewName=%s PoolName=%s MediaType=%s Slot=%d",
	  dname, oldname, newname, poolname, mtype, &slot) == 6) {
	 ok = true;
      }
   } else {
      *oldname = 0;
      if (sscanf(dir->msg, "label %s VolumeName=%s PoolName=%s MediaType=%s Slot=%d",
	  dname, newname, poolname, mtype, &slot) == 5) {
	 ok = true;
      }
   }
   if (ok) {
      unbash_spaces(newname);
      unbash_spaces(oldname);
      unbash_spaces(poolname);
      unbash_spaces(mtype);
      if (find_device(jcr, dname)) {
	 /******FIXME**** compare MediaTypes */
	 dev = jcr->device->dev;

	 P(dev->mutex); 	      /* Use P to avoid indefinite block */
	 if (!(dev->state & ST_OPENED)) {
	    label_volume_if_ok(jcr, dev, oldname, newname, poolname, slot, relabel);
	    force_close_dev(dev);
         /* Under certain "safe" conditions, we can steal the lock */
	 } else if (dev->dev_blocked && 
		    (dev->dev_blocked == BST_UNMOUNTED ||
		     dev->dev_blocked == BST_WAITING_FOR_SYSOP ||
		     dev->dev_blocked == BST_UNMOUNTED_WAITING_FOR_SYSOP)) {
	    label_volume_if_ok(jcr, dev, oldname, newname, poolname, slot, relabel);
	 } else if (dev_state(dev, ST_READ) || dev->num_writers) {
	    if (dev_state(dev, ST_READ)) {
                bnet_fsend(dir, _("3911 Device %s is busy with 1 reader.\n"),
		   dev_name(dev));
	    } else {
                bnet_fsend(dir, _("3912 Device %s is busy with %d writer(s).\n"),
		   dev_name(dev), dev->num_writers);
	    }
	 } else {		      /* device not being used */
	    label_volume_if_ok(jcr, dev, oldname, newname, poolname, slot, relabel);
	 }
	 V(dev->mutex);
      } else {
         bnet_fsend(dir, _("3999 Device %s not found\n"), dname);
      }
   } else {
      /* NB dir->msg gets clobbered in bnet_fsend, so save command */
      pm_strcpy(&jcr->errmsg, dir->msg);
      bnet_fsend(dir, _("3903 Error scanning label command: %s\n"), jcr->errmsg);
   }
   free_memory(dname);
   free_memory(oldname);
   free_memory(newname);
   free_memory(poolname);
   free_memory(mtype);
   bnet_sig(dir, BNET_EOD);
   return 1;
}

/* 
 * Read the tape label and determine if we can safely
 * label the tape (not a Bacula volume), then label it.
 *
 *  Enter with the mutex set
 */
static void label_volume_if_ok(JCR *jcr, DEVICE *dev, char *oldname, 
			       char *newname, char *poolname,
			       int slot, int relabel)
{
   BSOCK *dir = jcr->dir_bsock;
   DEV_BLOCK *block;
   bsteal_lock_t hold;
   
   steal_device_lock(dev, &hold, BST_WRITING_LABEL);
   block = new_block(dev);
   
   pm_strcpy(&jcr->VolumeName, newname);
   jcr->VolCatInfo.Slot = slot;
   if (autoload_device(jcr, dev, 0, dir) < 0) {    /* autoload if possible */
      goto bail_out;
   }

   /* Ensure that the device is open -- autoload_device() closes it */
   for ( ; !(dev->state & ST_OPENED); ) {
      if (open_dev(dev, jcr->VolumeName, OPEN_READ_WRITE) < 0) {
         bnet_fsend(dir, _("3910 Unable to open device %s. ERR=%s\n"), 
	    dev_name(dev), strerror_dev(dev));
	 goto bail_out;
      }
   }

   /* See what we have for a Volume */
   switch (read_dev_volume_label(jcr, dev, block)) {		    
   case VOL_NAME_ERROR:
   case VOL_VERSION_ERROR:
   case VOL_LABEL_ERROR:
   case VOL_OK:
      if (!relabel) {
	 bnet_fsend(dir, _(
            "3911 Cannot label Volume because it is already labeled: \"%s\"\n"), 
	     dev->VolHdr.VolName);
	 break;
      }
      /* Relabel request. If oldname matches, continue */
      if (strcmp(oldname, dev->VolHdr.VolName) != 0) {
         bnet_fsend(dir, _("Wrong volume mounted.\n"));
	 break;
      }
      /* Fall through wanted! */
   case VOL_IO_ERROR:
   case VOL_NO_LABEL:
      if (!write_volume_label_to_dev(jcr, jcr->device, newname, poolname)) {
         bnet_fsend(dir, _("3912 Failed to label Volume: ERR=%s\n"), strerror_dev(dev));
	 break;
      }
      pm_strcpy(&jcr->VolumeName, newname);
      bnet_fsend(dir, _("3000 OK label. Volume=%s Device=%s\n"), 
	 newname, dev_name(dev));
      break;
   case VOL_NO_MEDIA:
      bnet_fsend(dir, _("3912 Failed to label Volume: ERR=%s\n"), strerror_dev(dev));
      break;
   default:
      bnet_fsend(dir, _("3913 Cannot label Volume. \
Unknown status %d from read_volume_label()\n"), jcr->label_status);
      break;
   }
bail_out:
   free_block(block);
   give_back_device_lock(dev, &hold);

   return;
}


/* 
 * Read the tape label
 *
 *  Enter with the mutex set
 */
static int read_label(JCR *jcr, DEVICE *dev)
{
   int stat;
   BSOCK *dir = jcr->dir_bsock;
   DEV_BLOCK *block;
   bsteal_lock_t hold;
   
   steal_device_lock(dev, &hold, BST_DOING_ACQUIRE);
   
   jcr->VolumeName[0] = 0;
   block = new_block(dev);
   dev->state &= ~ST_LABEL;	      /* force read of label */
   switch (read_dev_volume_label(jcr, dev, block)) {		    
   case VOL_OK:
      bnet_fsend(dir, _("3001 Mounted Volume: %s\n"), dev->VolHdr.VolName);
      stat = 1;
      break;
   default:
      bnet_fsend(dir, _("3902 Cannot mount Volume on Storage Device \"%s\" because:\n%s"),
	 dev_name(dev), jcr->errmsg);
      stat = 0;
      break;
   }
   free_block(block);
   give_back_device_lock(dev, &hold);
   return stat;
}

static bool find_device(JCR *jcr, char *dname)
{
   DEVRES *device = NULL;
   bool found = false;

   unbash_spaces(dname);
   LockRes();
   while ((device=(DEVRES *)GetNextRes(R_DEVICE, (RES *)device))) {
      /* Find resource, and make sure we were able to open it */
      if (strcmp(device->hdr.name, dname) == 0 && device->dev) {
         Dmsg1(20, "Found device %s\n", device->hdr.name);
	 jcr->device = device;
	 found = true;
	 break;
      }
   }
   if (found) {
      jcr->dcr = new_dcr(jcr, device->dev);
   }
   UnlockRes();
   return found;
}


/*
 * Mount command from Director
 */
static int mount_cmd(JCR *jcr)
{
   POOLMEM *dname;
   BSOCK *dir = jcr->dir_bsock;
   DEVICE *dev;

   dname = get_memory(dir->msglen+1);
   if (sscanf(dir->msg, "mount %s", dname) == 1) {
      if (find_device(jcr, dname)) {
	 DEV_BLOCK *block;
	 dev = jcr->device->dev;
	 P(dev->mutex); 	      /* Use P to avoid indefinite block */
	 switch (dev->dev_blocked) {	     /* device blocked? */
	 case BST_WAITING_FOR_SYSOP:
	    /* Someone is waiting, wake him */
            Dmsg0(100, "Waiting for mount. Attempting to wake thread\n");
	    dev->dev_blocked = BST_MOUNT;
            bnet_fsend(dir, "3001 OK mount. Device=%s\n", dev_name(dev));
	    pthread_cond_signal(&dev->wait_next_vol);
	    break;

	 /* In both of these two cases, we (the user) unmounted the Volume */
	 case BST_UNMOUNTED_WAITING_FOR_SYSOP:
	 case BST_UNMOUNTED:
	    /* We freed the device, so reopen it and wake any waiting threads */
	    if (open_dev(dev, NULL, OPEN_READ_WRITE) < 0) {
               bnet_fsend(dir, _("3901 open device failed: ERR=%s\n"), 
		  strerror_dev(dev));
	       break;
	    }
	    block = new_block(dev);
	    read_dev_volume_label(jcr, dev, block);
	    free_block(block);
	    if (dev->dev_blocked == BST_UNMOUNTED) {
	       /* We blocked the device, so unblock it */
               Dmsg0(100, "Unmounted. Unblocking device\n");
	       read_label(jcr, dev);  /* this should not be necessary */
	       unblock_device(dev);
	    } else {
               Dmsg0(100, "Unmounted waiting for mount. Attempting to wake thread\n");
	       dev->dev_blocked = BST_MOUNT;
	    }
	    if (dev_state(dev, ST_LABEL)) {
               bnet_fsend(dir, _("3001 Device %s is mounted with Volume \"%s\"\n"), 
		  dev_name(dev), dev->VolHdr.VolName);
	    } else {
               bnet_fsend(dir, _("3905 Device %s open but no Bacula volume is mounted.\n"
                                 "If this is not a blank tape, try unmounting and remounting the Volume.\n"),
			  dev_name(dev));
	    }
	    pthread_cond_signal(&dev->wait_next_vol);
	    break;

	 case BST_DOING_ACQUIRE:
            bnet_fsend(dir, _("3001 Device %s is mounted; doing acquire.\n"), 
		       dev_name(dev));
	    break;

	 case BST_WRITING_LABEL:
            bnet_fsend(dir, _("3903 Device %s is being labeled.\n"), dev_name(dev));
	    break;

	 case BST_NOT_BLOCKED:
	    if (dev_state(dev, ST_OPENED)) {
	       if (dev_state(dev, ST_LABEL)) {
                  bnet_fsend(dir, _("3001 Device %s is mounted with Volume \"%s\"\n"),
		     dev_name(dev), dev->VolHdr.VolName);
	       } else {
                  bnet_fsend(dir, _("3905 Device %s open but no Bacula volume is mounted.\n"   
                                 "If this is not a blank tape, try unmounting and remounting the Volume.\n"),
			     dev_name(dev));
	       }
	    } else {
	       if (!dev_is_tape(dev)) {
                  bnet_fsend(dir, _("3906 cannot mount non-tape.\n"));
		  break;
	       }
	       if (open_dev(dev, NULL, OPEN_READ_WRITE) < 0) {
                  bnet_fsend(dir, _("3901 open device failed: ERR=%s\n"), 
		     strerror_dev(dev));
		  break;
	       }
	       read_label(jcr, dev);
	       if (dev_state(dev, ST_LABEL)) {
                  bnet_fsend(dir, _("3001 Device %s is already mounted with Volume \"%s\"\n"), 
		     dev_name(dev), dev->VolHdr.VolName);
	       } else {
                  bnet_fsend(dir, _("3905 Device %s open but no Bacula volume is mounted.\n"
                                    "If this is not a blank tape, try unmounting and remounting the Volume.\n"),
			     dev_name(dev));
	       }
	    }
	    break;

	 default:
            bnet_fsend(dir, _("3905 Bizarre wait state %d\n"), dev->dev_blocked);
	    break;
	 }
	 V(dev->mutex);
      } else {
         bnet_fsend(dir, _("3999 Device %s not found\n"), dname);
      }
   } else {
      pm_strcpy(&jcr->errmsg, dir->msg);
      bnet_fsend(dir, _("3909 Error scanning mount command: %s\n"), jcr->errmsg);
   }
   free_memory(dname);
   bnet_sig(dir, BNET_EOD);
   return 1;
}

/*
 * unmount command from Director
 */
static int unmount_cmd(JCR *jcr)
{
   POOLMEM *dname;
   BSOCK *dir = jcr->dir_bsock;
   DEVICE *dev;

   dname = get_memory(dir->msglen+1);
   if (sscanf(dir->msg, "unmount %s", dname) == 1) {
      if (find_device(jcr, dname)) {
	 dev = jcr->device->dev;
	 P(dev->mutex); 	      /* Use P to avoid indefinite block */
	 if (!(dev->state & ST_OPENED)) {
            Dmsg0(90, "Device already unmounted\n");
            bnet_fsend(dir, _("3901 Device \"%s\" is already unmounted.\n"), dev_name(dev));

	 } else if (dev->dev_blocked == BST_WAITING_FOR_SYSOP) {
            Dmsg2(90, "%d waiter dev_block=%d. doing unmount\n", dev->num_waiting,
	       dev->dev_blocked);
	    open_dev(dev, NULL, 0);	/* fake open for close */
	    offline_or_rewind_dev(dev);
	    force_close_dev(dev);
	    dev->dev_blocked = BST_UNMOUNTED_WAITING_FOR_SYSOP;
            bnet_fsend(dir, _("3001 Device \"%s\" unmounted.\n"), dev_name(dev));

	 } else if (dev->dev_blocked == BST_DOING_ACQUIRE) {
            bnet_fsend(dir, _("3902 Device \"%s\" is busy in acquire.\n"), dev_name(dev));

	 } else if (dev->dev_blocked == BST_WRITING_LABEL) {
            bnet_fsend(dir, _("3903 Device \"%s\" is being labeled.\n"), dev_name(dev));

	 } else if (dev_state(dev, ST_READ) || dev->num_writers) {
	    if (dev_state(dev, ST_READ)) {
                Dmsg0(90, "Device in read mode\n");
                bnet_fsend(dir, _("3904 Device \"%s\" is busy reading.\n"), dev_name(dev));
	    } else {
                Dmsg1(90, "Device busy with %d writers\n", dev->num_writers);
                bnet_fsend(dir, _("3905 Device %s is busy with %d writer(s).\n"),
		   dev_name(dev), dev->num_writers);
	    }

	 } else {		      /* device not being used */
            Dmsg0(90, "Device not in use, unmounting\n");
	    /* On FreeBSD, I am having ASSERT() failures in block_device()
	     * and I can only imagine that the thread id that we are
	     * leaving in no_wait_id is being re-used. So here,
	     * we simply do it by hand.  Gross, but a solution.
	     */
	    /*	block_device(dev, BST_UNMOUNTED); replace with 2 lines below */
	    dev->dev_blocked = BST_UNMOUNTED;
	    dev->no_wait_id = 0;
	    open_dev(dev, NULL, 0);	/* fake open for close */
	    offline_or_rewind_dev(dev);
	    force_close_dev(dev);
            bnet_fsend(dir, _("3002 Device %s unmounted.\n"), dev_name(dev));
	 }
	 V(dev->mutex);
      } else {
         bnet_fsend(dir, _("3999 Device %s not found\n"), dname);
      }
   } else {
      /* NB dir->msg gets clobbered in bnet_fsend, so save command */
      pm_strcpy(&jcr->errmsg, dir->msg);
      bnet_fsend(dir, _("3907 Error scanning unmount command: %s\n"), jcr->errmsg);
   }
   free_memory(dname);
   bnet_sig(dir, BNET_EOD);
   return 1;
}

/*
 * Release command from Director. This rewinds the device and if
 *   configured does a offline and ensures that Bacula will
 *   re-read the label of the tape before continuing. This gives
 *   the operator the chance to change the tape anytime before the
 *   next job starts.
 */
static int release_cmd(JCR *jcr)
{
   POOLMEM *dname;
   BSOCK *dir = jcr->dir_bsock;
   DEVICE *dev;

   dname = get_memory(dir->msglen+1);
   if (sscanf(dir->msg, "release %s", dname) == 1) {
      if (find_device(jcr, dname)) {
	 dev = jcr->device->dev;
	 P(dev->mutex); 	      /* Use P to avoid indefinite block */
	 if (!(dev->state & ST_OPENED)) {
            Dmsg0(90, "Device already released\n");
            bnet_fsend(dir, _("3911 Device %s already released.\n"), dev_name(dev));

	 } else if (dev->dev_blocked == BST_WAITING_FOR_SYSOP ||
		    dev->dev_blocked == BST_UNMOUNTED_WAITING_FOR_SYSOP) {
            Dmsg2(90, "%d waiter dev_block=%d. doing unmount\n", dev->num_waiting,
	       dev->dev_blocked);
            bnet_fsend(dir, _("3912 Device %s waiting for mount.\n"), dev_name(dev));

	 } else if (dev->dev_blocked == BST_DOING_ACQUIRE) {
            bnet_fsend(dir, _("3913 Device %s is busy in acquire.\n"), dev_name(dev));

	 } else if (dev->dev_blocked == BST_WRITING_LABEL) {
            bnet_fsend(dir, _("3914 Device %s is being labeled.\n"), dev_name(dev));

	 } else if (dev_state(dev, ST_READ) || dev->num_writers) {
	    if (dev_state(dev, ST_READ)) {
                Dmsg0(90, "Device in read mode\n");
                bnet_fsend(dir, _("3915 Device %s is busy with 1 reader.\n"), dev_name(dev));
	    } else {
                Dmsg1(90, "Device busy with %d writers\n", dev->num_writers);
                bnet_fsend(dir, _("3916 Device %s is busy with %d writer(s).\n"),
		   dev_name(dev), dev->num_writers);
	    }

	 } else {		      /* device not being used */
            Dmsg0(90, "Device not in use, unmounting\n");
	    release_volume(jcr, dev);
            bnet_fsend(dir, _("3012 Device %s released.\n"), dev_name(dev));
	 }
	 V(dev->mutex);
      } else {
         bnet_fsend(dir, _("3999 Device %s not found\n"), dname);
      }
   } else {
      /* NB dir->msg gets clobbered in bnet_fsend, so save command */
      pm_strcpy(&jcr->errmsg, dir->msg);
      bnet_fsend(dir, _("3917 Error scanning release command: %s\n"), jcr->errmsg);
   }
   free_memory(dname);
   bnet_sig(dir, BNET_EOD);
   return 1;
}



/*
 * Autochanger command from Director
 */
static int autochanger_cmd(JCR *jcr)
{
   POOLMEM *dname;
   BSOCK *dir = jcr->dir_bsock;
   DEVICE *dev;

   dname = get_memory(dir->msglen+1);
   if (sscanf(dir->msg, "autochanger list %s ", dname) == 1) {
      if (find_device(jcr, dname)) {
	 dev = jcr->device->dev;
	 P(dev->mutex); 	      /* Use P to avoid indefinite block */
	 if (!dev_is_tape(dev)) {
            bnet_fsend(dir, _("3995 Device %s is not an autochanger.\n"), dev_name(dev));
	 } else if (!(dev->state & ST_OPENED)) {
	    autochanger_list(jcr, dev, dir);
         /* Under certain "safe" conditions, we can steal the lock */
	 } else if (dev->dev_blocked && 
		    (dev->dev_blocked == BST_UNMOUNTED ||
		     dev->dev_blocked == BST_WAITING_FOR_SYSOP ||
		     dev->dev_blocked == BST_UNMOUNTED_WAITING_FOR_SYSOP)) {
	    autochanger_list(jcr, dev, dir);
	 } else if (dev_state(dev, ST_READ) || dev->num_writers) {
	    if (dev_state(dev, ST_READ)) {
                bnet_fsend(dir, _("3901 Device %s is busy with 1 reader.\n"), dev_name(dev));
	    } else {
                bnet_fsend(dir, _("3902 Device %s is busy with %d writer(s).\n"),
		   dev_name(dev), dev->num_writers);
	    }
	 } else {		      /* device not being used */
	    autochanger_list(jcr, dev, dir);
	 }
	 V(dev->mutex);
      } else {
         bnet_fsend(dir, _("3999 Device %s not found\n"), dname);
      }
   } else {  /* error on scanf */
      pm_strcpy(&jcr->errmsg, dir->msg);
      bnet_fsend(dir, _("3908 Error scanning autocharger list command: %s\n"),
	 jcr->errmsg);
   }
   free_memory(dname);
   bnet_sig(dir, BNET_EOD);
   return 1;
}

/*
 * Read and return the Volume label
 */
static int readlabel_cmd(JCR *jcr)
{
   POOLMEM *dname;
   BSOCK *dir = jcr->dir_bsock;
   DEVICE *dev;
   int Slot;

   dname = get_memory(dir->msglen+1);
   if (sscanf(dir->msg, "readlabel %s Slot=%d", dname, &Slot) == 2) {
      if (find_device(jcr, dname)) {
	 dev = jcr->device->dev;

	 P(dev->mutex); 	      /* Use P to avoid indefinite block */
	 if (!dev_state(dev, ST_OPENED)) {
	    read_volume_label(jcr, dev, Slot);
	    force_close_dev(dev);
         /* Under certain "safe" conditions, we can steal the lock */
	 } else if (dev->dev_blocked && 
		    (dev->dev_blocked == BST_UNMOUNTED ||
		     dev->dev_blocked == BST_WAITING_FOR_SYSOP ||
		     dev->dev_blocked == BST_UNMOUNTED_WAITING_FOR_SYSOP)) {
	    read_volume_label(jcr, dev, Slot);
	 } else if (dev_state(dev, ST_READ) || dev->num_writers) {
	    if (dev_state(dev, ST_READ)) {
                bnet_fsend(dir, _("3911 Device %s is busy with 1 reader.\n"),
			    dev_name(dev));
	    } else {
                bnet_fsend(dir, _("3912 Device %s is busy with %d writer(s).\n"),
		   dev_name(dev), dev->num_writers);
	    }
	 } else {		      /* device not being used */
	    read_volume_label(jcr, dev, Slot);
	 }
	 V(dev->mutex);
      } else {
         bnet_fsend(dir, _("3999 Device %s not found\n"), dname);
      }
   } else {
      pm_strcpy(&jcr->errmsg, dir->msg);
      bnet_fsend(dir, _("3909 Error scanning readlabel command: %s\n"), jcr->errmsg);
   }
   free_memory(dname);
   bnet_sig(dir, BNET_EOD);
   return 1;
}

/* 
 * Read the tape label
 *
 *  Enter with the mutex set
 */
static void read_volume_label(JCR *jcr, DEVICE *dev, int Slot)
{
   BSOCK *dir = jcr->dir_bsock;
   DEV_BLOCK *block;
   bsteal_lock_t hold;
   
   steal_device_lock(dev, &hold, BST_WRITING_LABEL);
   block = new_block(dev);
   
   jcr->VolumeName[0] = 0;
   jcr->VolCatInfo.Slot = Slot;
   if (autoload_device(jcr, dev, 0, dir) < 0) {    /* autoload if possible */
      goto bail_out;
   }

   /* Ensure that the device is open -- autoload_device() closes it */
   for ( ; !dev_state(dev, ST_OPENED); ) {
      if (open_dev(dev, jcr->VolumeName, OPEN_READ_WRITE) < 0) {
         bnet_fsend(dir, _("3910 Unable to open device \"%s\". ERR=%s\n"), 
	    dev_name(dev), strerror_dev(dev));
	 goto bail_out;
      }
   }

   dev->state &= ~ST_LABEL;	      /* force read of label */
   switch (read_dev_volume_label(jcr, dev, block)) {		    
   case VOL_OK:
      /* DO NOT add quotes around the Volume name. It is scanned in the DIR */
      bnet_fsend(dir, _("3001 Volume=%s Slot=%d\n"), dev->VolHdr.VolName, Slot);
      Dmsg1(100, "Volume: %s\n", dev->VolHdr.VolName);
      break;
   default:
      bnet_fsend(dir, _("3902 Cannot mount Volume on Storage Device \"%s\" because:\n%s"),
		 dev_name(dev), jcr->errmsg);
      break;
   }

bail_out:
   free_block(block);
   give_back_device_lock(dev, &hold);
   return;
}
