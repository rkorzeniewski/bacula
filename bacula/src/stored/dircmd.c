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
static int setdebug_cmd(JCR *jcr);
static int cancel_cmd(JCR *cjcr);
static int mount_cmd(JCR *jcr);
static int unmount_cmd(JCR *jcr);
static int autochanger_cmd(JCR *sjcr);
static int do_label(JCR *jcr, int relabel);
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
   {NULL,	 NULL}		      /* list terminator */
};


/* 
 * Connection request. We accept connections either from the 
 *  Director or a Client.
 * 
 * Note, we are running as a seperate thread of the Storage daemon.
 *  and it is because a Director has made a connection with
 *  us on the "Message" channel.    
 *
 * Basic tasks done here:  
 *  - Create a JCR record
 *  - Authenticate the Director
 *  - We wait for a command
 *  - We execute the command
 *  - We continue or exit depending on the return status
 */
void *connection_request(void *arg)
{
   BSOCK *bs = (BSOCK *)arg;
   JCR *jcr;
   int i, found, quit;
   int bnet_stat = 0;
   char name[MAX_NAME_LENGTH];

   if (bnet_recv(bs) <= 0) {
      Emsg0(M_ERROR, 0, _("Connection request failed.\n"));
      return NULL;
   }

   /* 
    * See if this is a File daemon connection
    */
   if (bs->msglen < 25 || bs->msglen > (int)sizeof(name)+25) {
      Emsg1(M_ERROR, 0, _("Invalid Dir connection. Len=%d\n"), bs->msglen);
   }
   if (sscanf(bs->msg, "Hello Start Job %127s calling\n", name) == 1) {
      handle_filed_connection(bs, name);
      return NULL;
   }
   
   jcr = new_jcr(sizeof(JCR), stored_free_jcr);     /* create Job Control Record */
   jcr->dir_bsock = bs; 	      /* save Director bsock */

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
      found = FALSE;
      for (i=0; cmds[i].cmd; i++) {
	 if (strncmp(cmds[i].cmd, bs->msg, strlen(cmds[i].cmd)) == 0) {
	    if (!cmds[i].func(jcr)) {	 /* do command */
	       quit = TRUE;		 /* error, get out */
               Dmsg1(90, "Command %s requsts quit\n", cmds[i].cmd);
	    }
	    found = TRUE;	     /* indicate command found */
	    break;
	 }
      }
      if (!found) {		      /* command not found */
	 bnet_fsend(bs, derrmsg);
	 quit = TRUE;
	 break;
      }
   }
   if (bnet_stat != BNET_TERMINATE) {
      bnet_sig(bs, BNET_TERMINATE);
   }
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
   int level;

   Dmsg1(10, "setdebug_cmd: %s", dir->msg);
   if (sscanf(dir->msg, "setdebug=%d", &level) != 1 || level < 0) {
      bnet_fsend(dir, "3991 Bad setdebug command: %s\n", dir->msg);
      return 0;
   }
   debug_level = level;
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
   DEVRES *device;
   DEVICE *dev;
   int found = 0, ok = 0;
   int slot;	

   dname = get_memory(dir->msglen+1);
   newname = get_memory(dir->msglen+1);
   oldname = get_memory(dir->msglen+1);
   poolname = get_memory(dir->msglen+1);
   mtype = get_memory(dir->msglen+1);
   if (relabel) {
      if (sscanf(dir->msg, "relabel %s OldName=%s NewName=%s PoolName=%s MediaType=%s Slot=%d",
	  dname, oldname, newname, poolname, mtype, &slot) == 6) {
	 ok = 1;
      }
   } else {
      *oldname = 0;
      if (sscanf(dir->msg, "label %s VolumeName=%s PoolName=%s MediaType=%s Slot=%d",
	  dname, newname, poolname, mtype, &slot) == 5) {
	 ok = 1;
      }
   }
   if (ok) {
      unbash_spaces(dname);
      unbash_spaces(newname);
      unbash_spaces(oldname);
      unbash_spaces(poolname);
      unbash_spaces(mtype);
      device = NULL;
      LockRes();
      while ((device=(DEVRES *)GetNextRes(R_DEVICE, (RES *)device))) {
	 /* Find resource, and make sure we were able to open it */
	 if (strcmp(device->hdr.name, dname) == 0 && device->dev) {
            Dmsg1(20, "Found device %s\n", device->hdr.name);
	    found = 1;
	    break;
	 }
      }
      UnlockRes();
      if (found) {
	 /******FIXME**** compare MediaTypes */
	 jcr->device = device;
	 dev = device->dev;

	 P(dev->mutex); 	      /* Use P to avoid indefinite block */
	 if (!(dev->state & ST_OPENED)) {
	    if (open_dev(dev, newname, READ_WRITE) < 0) {
               bnet_fsend(dir, _("3994 Connot open device: %s\n"), strerror_dev(dev));
	    } else {
	       label_volume_if_ok(jcr, dev, oldname, newname, poolname, slot, relabel);
	       force_close_dev(dev);
	    }
         /* Under certain "safe" conditions, we can steal the lock */
	 } else if (dev->dev_blocked && 
		    (dev->dev_blocked == BST_UNMOUNTED ||
		     dev->dev_blocked == BST_WAITING_FOR_SYSOP ||
		     dev->dev_blocked == BST_UNMOUNTED_WAITING_FOR_SYSOP)) {
	    label_volume_if_ok(jcr, dev, oldname, newname, poolname, slot, relabel);
	 } else if (dev->state & ST_READ || dev->num_writers) {
	    if (dev->state & ST_READ) {
                bnet_fsend(dir, _("3901 Device %s is busy with 1 reader.\n"),
		   dev_name(dev));
	    } else {
                bnet_fsend(dir, _("3902 Device %s is busy with %d writer(s).\n"),
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
   
   pm_strcpy(&jcr->VolumeName, newname);
   jcr->VolCatInfo.Slot = slot;
   autoload_device(jcr, dev, 0, dir);	   /* autoload if possible */
   block = new_block(dev);

   /* Ensure that the device is open -- autoload_device() closes it */
   for ( ; !(dev->state & ST_OPENED); ) {
      if (open_dev(dev, jcr->VolumeName, READ_WRITE) < 0) {
	 if (dev->dev_errno == EAGAIN || dev->dev_errno == EBUSY) {
	    bmicrosleep(30, 0);
	 }
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
               "3911 Cannot label Volume because it is already labeled: %s\n"), 
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
	 strcpy(jcr->VolumeName, newname);
         bnet_fsend(dir, _("3000 OK label. Volume=%s Device=%s\n"), 
	    newname, dev->dev_name);
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
   return_device_lock(dev, &hold);
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
         bnet_fsend(dir, _("3902 Cannot mount Volume on Storage Device \"%s\" because:\n%s\n"),
	    dev->dev_name, jcr->errmsg);
	 stat = 0;
	 break;
   }
   free_block(block);
   return_device_lock(dev, &hold);
   return stat;
}

/*
 * Mount command from Director
 */
static int mount_cmd(JCR *jcr)
{
   POOLMEM *dev_name;
   BSOCK *dir = jcr->dir_bsock;
   DEVRES *device;
   DEVICE *dev;
   int found = 0;

   dev_name = get_memory(dir->msglen+1);
   if (sscanf(dir->msg, "mount %s", dev_name) == 1) {
      unbash_spaces(dev_name);
      device = NULL;
      LockRes();
      while ((device=(DEVRES *)GetNextRes(R_DEVICE, (RES *)device))) {
	 /* Find resource, and make sure we were able to open it */
	 if (strcmp(device->hdr.name, dev_name) == 0 && device->dev) {
            Dmsg1(20, "Found device %s\n", device->hdr.name);
	    found = 1;
	    break;
	 }
      }
      UnlockRes();
      if (found) {
	 jcr->device = device;
	 dev = device->dev;
	 P(dev->mutex); 	      /* Use P to avoid indefinite block */
	 switch (dev->dev_blocked) {	     /* device blocked? */
	    DEV_BLOCK *block;
	    case BST_WAITING_FOR_SYSOP:
	       /* Someone is waiting, wake him */
               Dmsg0(100, "Waiting for mount. Attempting to wake thread\n");
	       dev->dev_blocked = BST_MOUNT;
	       pthread_cond_signal(&dev->wait_next_vol);
               bnet_fsend(dir, "3001 OK mount. Device=%s\n", dev->dev_name);
	       break;

	    case BST_UNMOUNTED_WAITING_FOR_SYSOP:
	    case BST_UNMOUNTED:
	       /* We freed the device, so reopen it and wake any waiting threads */
	       if (open_dev(dev, NULL, READ_WRITE) < 0) {
                  bnet_fsend(dir, _("3901 open device failed: ERR=%s\n"), 
		     strerror_dev(dev));
		  break;
	       }
	       block = new_block(dev);
	       read_dev_volume_label(jcr, dev, block);
	       free_block(block);
	       if (dev->dev_blocked == BST_UNMOUNTED) {
                  Dmsg0(100, "Unmounted. Unblocking device\n");
		  read_label(jcr, dev);
		  unblock_device(dev);
	       } else {
                  Dmsg0(100, "Unmounted waiting for mount. Attempting to wake thread\n");
		  dev->dev_blocked = BST_MOUNT;
		  pthread_cond_signal(&dev->wait_next_vol);
	       }
	       if (dev->state & ST_LABEL) {
                  bnet_fsend(dir, _("3001 Device %s is mounted with Volume \"%s\"\n"), 
		     dev->dev_name, dev->VolHdr.VolName);
	       } else {
                  bnet_fsend(dir, _("3905 Device %s open but no Bacula volume is mounted.\n"
                                    "Try unmounting and remounting the Volume.\n"),
			     dev->dev_name);
	       }
	       break;

	    case BST_DOING_ACQUIRE:
               bnet_fsend(dir, _("3001 Device %s is mounted; doing acquire.\n"), 
			  dev->dev_name);
	       break;

	    case BST_WRITING_LABEL:
               bnet_fsend(dir, _("3903 Device %s is being labeled.\n"), dev->dev_name);
	       break;

	    case BST_NOT_BLOCKED:
	       if (dev->state & ST_OPENED) {
		  if (dev->state & ST_LABEL) {
                     bnet_fsend(dir, _("3001 Device %s is mounted with Volume \"%s\"\n"),
			dev->dev_name, dev->VolHdr.VolName);
		  } else {
                     bnet_fsend(dir, _("3905 Device %s open but no Bacula volume is mounted.\n"   
                                    "Try unmounting and remounting the Volume.\n"),
				dev->dev_name);
		  }
	       } else {
		  if (!dev_is_tape(dev)) {
                     bnet_fsend(dir, _("3906 cannot mount non-tape.\n"));
		     break;
		  }
		  if (open_dev(dev, NULL, READ_WRITE) < 0) {
                     bnet_fsend(dir, _("3901 open device failed: ERR=%s\n"), 
			strerror_dev(dev));
		     break;
		  }
		  read_label(jcr, dev);
		  if (dev->state & ST_LABEL) {
                     bnet_fsend(dir, _("3001 Device %s is mounted with Volume \"%s\"\n"), 
			dev->dev_name, dev->VolHdr.VolName);
		  } else {
                     bnet_fsend(dir, _("3905 Device %s open but no Bacula volume is mounted.\n"
                                       "Try unmounting and remounting the Volume.\n"),
				dev->dev_name);
		  }
	       }
	       break;

	    default:
               bnet_fsend(dir, _("3905 Bizarre wait state %d\n"), dev->dev_blocked);
	       break;
	 }
	 V(dev->mutex);
      } else {
         bnet_fsend(dir, _("3999 Device %s not found\n"), dev_name);
      }
   } else {
      pm_strcpy(&jcr->errmsg, dir->msg);
      bnet_fsend(dir, _("3906 Error scanning mount command: %s\n"), jcr->errmsg);
   }
   free_memory(dev_name);
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
   DEVRES *device;
   DEVICE *dev;
   int found = 0;

   dname = get_memory(dir->msglen+1);
   if (sscanf(dir->msg, "unmount %s", dname) == 1) {
      unbash_spaces(dname);
      device = NULL;
      LockRes();
      while ((device=(DEVRES *)GetNextRes(R_DEVICE, (RES *)device))) {
	 /* Find resource, and make sure we were able to open it */
	 if (strcmp(device->hdr.name, dname) == 0 && device->dev) {
            Dmsg1(20, "Found device %s\n", device->hdr.name);
	    found = 1;
	    break;
	 }
      }
      UnlockRes();
      if (found) {
	 jcr->device = device;
	 dev = device->dev;
	 P(dev->mutex); 	      /* Use P to avoid indefinite block */
	 if (!(dev->state & ST_OPENED)) {
            Dmsg0(90, "Device already unmounted\n");
            bnet_fsend(dir, _("3901 Device %s is already unmounted.\n"), dev_name(dev));

	 } else if (dev->dev_blocked == BST_WAITING_FOR_SYSOP) {
            Dmsg2(90, "%d waiter dev_block=%d. doing unmount\n", dev->num_waiting,
	       dev->dev_blocked);
	    open_dev(dev, NULL, 0);	/* fake open for close */
	    offline_or_rewind_dev(dev);
	    force_close_dev(dev);
	    dev->dev_blocked = BST_UNMOUNTED_WAITING_FOR_SYSOP;
            bnet_fsend(dir, _("3001 Device %s unmounted.\n"), dev_name(dev));

	 } else if (dev->dev_blocked == BST_DOING_ACQUIRE) {
            bnet_fsend(dir, _("3902 Device %s is busy in acquire.\n"),
	       dev_name(dev));

	 } else if (dev->dev_blocked == BST_WRITING_LABEL) {
            bnet_fsend(dir, _("3903 Device %s is being labeled.\n"),
	       dev_name(dev));

	 } else if (dev->state & ST_READ || dev->num_writers) {
	    if (dev->state & ST_READ) {
                Dmsg0(90, "Device in read mode\n");
                bnet_fsend(dir, _("3904 Device %s is busy with 1 reader.\n"),
		   dev_name(dev));
	    } else {
                Dmsg1(90, "Device busy with %d writers\n", dev->num_writers);
                bnet_fsend(dir, _("3905 Device %s is busy with %d writer(s).\n"),
		   dev_name(dev), dev->num_writers);
	    }

	 } else {		      /* device not being used */
            Dmsg0(90, "Device not in use, unmounting\n");
	    block_device(dev, BST_UNMOUNTED);
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
 * Autochanger command from Director
 */
static int autochanger_cmd(JCR *jcr)
{
   POOLMEM *devname;
   BSOCK *dir = jcr->dir_bsock;
   DEVRES *device;
   DEVICE *dev;
   int found = 0;

   devname = get_memory(dir->msglen+1);
   if (sscanf(dir->msg, "autochanger list %s ", devname) == 1) {
      unbash_spaces(devname);
      device = NULL;
      LockRes();
      while ((device=(DEVRES *)GetNextRes(R_DEVICE, (RES *)device))) {
	 /* Find resource, and make sure we were able to open it */
	 if (strcmp(device->hdr.name, devname) == 0 && device->dev) {
            Dmsg1(20, "Found device %s\n", device->hdr.name);
	    found = 1;
	    break;
	 }
      }
      UnlockRes();
      if (found) {
	 jcr->device = device;
	 dev = device->dev;
	 P(dev->mutex); 	      /* Use P to avoid indefinite block */
	 if (!dev_is_tape(dev)) {
            bnet_fsend(dir, _("3995 Device %s is not an autochanger.\n"), 
	       dev_name(dev));
	 } else if (!(dev->state & ST_OPENED)) {
	    if (open_dev(dev, NULL, READ_WRITE) < 0) {
               bnet_fsend(dir, _("3994 Connot open device: %s\n"), strerror_dev(dev));
	    } else {
	       autochanger_list(jcr, dev, dir);
	       force_close_dev(dev);
	    }
         /* Under certain "safe" conditions, we can steal the lock */
	 } else if (dev->dev_blocked && 
		    (dev->dev_blocked == BST_UNMOUNTED ||
		     dev->dev_blocked == BST_WAITING_FOR_SYSOP ||
		     dev->dev_blocked == BST_UNMOUNTED_WAITING_FOR_SYSOP)) {
	    autochanger_list(jcr, dev, dir);
	 } else if (dev->state & ST_READ || dev->num_writers) {
	    if (dev->state & ST_READ) {
                bnet_fsend(dir, _("3901 Device %s is busy with 1 reader.\n"),
		   dev_name(dev));
	    } else {
                bnet_fsend(dir, _("3902 Device %s is busy with %d writer(s).\n"),
		   dev_name(dev), dev->num_writers);
	    }
	 } else {		      /* device not being used */
	    autochanger_list(jcr, dev, dir);
	 }
	 V(dev->mutex);
      } else {
         bnet_fsend(dir, _("3999 Device %s not found\n"), devname);
      }
   } else {  /* error on scanf */
      pm_strcpy(&jcr->errmsg, dir->msg);
      bnet_fsend(dir, _("3907 Error scanning autocharger list command: %s\n"),
	 jcr->errmsg);
   }
   free_memory(devname);
   bnet_sig(dir, BNET_EOD);
   return 1;
}
