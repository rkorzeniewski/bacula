/*
 *  This file handles the status command
 *
 *     Kern Sibbald, May MMIII
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


/* Forward referenced functions */
static void send_blocked_status(JCR *jcr, DEVICE *dev);


/*
 * Status command from Director
 */
int status_cmd(JCR *jcr)
{
   DEVRES *device;
   DEVICE *dev;
   int found, bps, sec, bpb;
   BSOCK *user = jcr->dir_bsock;
   char dt[MAX_TIME_LENGTH];
   char b1[30], b2[30], b3[30];

   bnet_fsend(user, "\n%s Version: " VERSION " (" BDATE ") %s %s %s\n", my_name,
	      HOST_OS, DISTNAME, DISTVER);
   bstrftime(dt, sizeof(dt), daemon_start_time);
   bnet_fsend(user, _("Daemon started %s, %d Job%s run.\n"), dt, last_job.NumJobs,
        last_job.NumJobs == 1 ? "" : "s");
   if (last_job.NumJobs > 0) {
      char termstat[30];

      bstrftime(dt, sizeof(dt), last_job.end_time);
      bnet_fsend(user, _("Last Job %s finished at %s\n"), last_job.Job, dt);

      jobstatus_to_ascii(last_job.JobStatus, termstat, sizeof(termstat));
      bnet_fsend(user, _("  Files=%s Bytes=%s Termination Status=%s\n"), 
	   edit_uint64_with_commas(last_job.JobFiles, b1),
	   edit_uint64_with_commas(last_job.JobBytes, b2),
	   termstat);
   }

   LockRes();
   for (device=NULL;  (device=(DEVRES *)GetNextRes(R_DEVICE, (RES *)device)); ) {
      for (dev=device->dev; dev; dev=dev->next) {
	 if (dev->state & ST_OPENED) {
	    if (dev->state & ST_LABEL) {
               bnet_fsend(user, _("Device %s is mounted with Volume \"%s\"\n"), 
		  dev_name(dev), dev->VolHdr.VolName);
	    } else {
               bnet_fsend(user, _("Device %s open but no Bacula volume is mounted.\n"), dev_name(dev));
	    }
	    send_blocked_status(jcr, dev);
	    if (dev->state & ST_APPEND) {
	       bpb = dev->VolCatInfo.VolCatBlocks;
	       if (bpb <= 0) {
		  bpb = 1;
	       }
	       bpb = dev->VolCatInfo.VolCatBytes / bpb;
               bnet_fsend(user, _("    Total Bytes=%s Blocks=%s Bytes/block=%s\n"),
		  edit_uint64_with_commas(dev->VolCatInfo.VolCatBytes, b1),
		  edit_uint64_with_commas(dev->VolCatInfo.VolCatBlocks, b2), 
		  edit_uint64_with_commas(bpb, b3));
	    } else {  /* reading */
	       bpb = dev->VolCatInfo.VolCatReads;
	       if (bpb <= 0) {
		  bpb = 1;
	       }
	       if (dev->VolCatInfo.VolCatRBytes > 0) {
		  bpb = dev->VolCatInfo.VolCatRBytes / bpb;
	       } else {
		  bpb = 0;
	       }
               bnet_fsend(user, _("    Total Bytes Read=%s Blocks Read=%s Bytes/block=%s\n"),
		  edit_uint64_with_commas(dev->VolCatInfo.VolCatRBytes, b1),
		  edit_uint64_with_commas(dev->VolCatInfo.VolCatReads, b2), 
		  edit_uint64_with_commas(bpb, b3));
	    }
            bnet_fsend(user, _("    Positioned at File=%s Block=%s\n"), 
	       edit_uint64_with_commas(dev->file, b1),
	       edit_uint64_with_commas(dev->block_num, b2));

	 } else {
            bnet_fsend(user, _("Device %s is not open.\n"), dev_name(dev));
	    send_blocked_status(jcr, dev);
	 }
      }
   }
   UnlockRes();

   found = 0;
   lock_jcr_chain();
   /* NOTE, we reuse a calling argument jcr. Be warned! */ 
   for (jcr=NULL; (jcr=get_next_jcr(jcr)); ) {
      if (jcr->JobStatus == JS_WaitFD) {
         bnet_fsend(user, _("%s Job %s waiting for Client connection.\n"),
	    job_type_to_str(jcr->JobType), jcr->Job);
      }
      if (jcr->device) {
         bnet_fsend(user, _("%s %s job %s using Volume \"%s\" on device %s\n"), 
		   job_level_to_str(jcr->JobLevel),
		   job_type_to_str(jcr->JobType),
		   jcr->Job,
		   jcr->VolumeName,
		   jcr->device->device_name);
	 sec = time(NULL) - jcr->run_time;
	 if (sec <= 0) {
	    sec = 1;
	 }
	 bps = jcr->JobBytes / sec;
         bnet_fsend(user, _("    Files=%s Bytes=%s Bytes/sec=%s\n"), 
	    edit_uint64_with_commas(jcr->JobFiles, b1),
	    edit_uint64_with_commas(jcr->JobBytes, b2),
	    edit_uint64_with_commas(bps, b3));
	 found = 1;
#ifdef DEBUG
	 if (jcr->file_bsock) {
            bnet_fsend(user, "    FDReadSeqNo=%s in_msg=%u out_msg=%d fd=%d\n", 
	       edit_uint64_with_commas(jcr->file_bsock->read_seqno, b1),
	       jcr->file_bsock->in_msg_no, jcr->file_bsock->out_msg_no,
	       jcr->file_bsock->fd);
	 } else {
            bnet_fsend(user, "    FDSocket closed\n");
	 }
#endif
      }
      free_locked_jcr(jcr);
   }
   unlock_jcr_chain();
   if (!found) {
      bnet_fsend(user, _("No jobs running.\n"));
   }

#ifdef full_status
   bnet_fsend(user, "\n\n");
   dump_resource(R_DEVICE, resources[R_DEVICE-r_first].res_head, sendit, user);
#endif
   bnet_fsend(user, "====\n");

   bnet_sig(user, BNET_EOD);
   return 1;
}

static void send_blocked_status(JCR *jcr, DEVICE *dev) 
{
   BSOCK *user = jcr->dir_bsock;

   switch (dev->dev_blocked) {
   case BST_UNMOUNTED:
      bnet_fsend(user, _("    Device is BLOCKED. User unmounted.\n"));
      break;
   case BST_UNMOUNTED_WAITING_FOR_SYSOP:
      bnet_fsend(user, _("    Device is BLOCKED. User unmounted during wait for media/mount.\n"));
      break;
   case BST_WAITING_FOR_SYSOP:
      if (jcr->JobStatus == JS_WaitMount) {
         bnet_fsend(user, _("    Device is BLOCKED waiting for mount.\n"));
      } else {
         bnet_fsend(user, _("    Device is BLOCKED waiting for appendable media.\n"));
      }
      break;
   case BST_DOING_ACQUIRE:
      bnet_fsend(user, _("    Device is being initialized.\n"));
      break;
   case BST_WRITING_LABEL:
      bnet_fsend(user, _("    Device is blocked labeling a Volume.\n"));
      break;
   default:
      break;
   }
   if (debug_level > 1) {
      bnet_fsend(user, _("Configured device capabilities:\n"));
      bnet_fsend(user, "%sEOF ", dev->capabilities & CAP_EOF ? "" : "!");
      bnet_fsend(user, "%sBSR ", dev->capabilities & CAP_BSR ? "" : "!");
      bnet_fsend(user, "%sBSF ", dev->capabilities & CAP_BSF ? "" : "!");
      bnet_fsend(user, "%sFSR ", dev->capabilities & CAP_FSR ? "" : "!");
      bnet_fsend(user, "%sFSF ", dev->capabilities & CAP_FSF ? "" : "!");
      bnet_fsend(user, "%sEOM ", dev->capabilities & CAP_EOM ? "" : "!");
      bnet_fsend(user, "%sREM ", dev->capabilities & CAP_REM ? "" : "!");
      bnet_fsend(user, "%sRACCESS ", dev->capabilities & CAP_RACCESS ? "" : "!");
      bnet_fsend(user, "%sAUTOMOUNT ", dev->capabilities & CAP_AUTOMOUNT ? "" : "!");
      bnet_fsend(user, "%sLABEL ", dev->capabilities & CAP_LABEL ? "" : "!");
      bnet_fsend(user, "%sANONVOLS ", dev->capabilities & CAP_ANONVOLS ? "" : "!");
      bnet_fsend(user, "%sALWAYSOPEN ", dev->capabilities & CAP_ALWAYSOPEN ? "" : "!");
      bnet_fsend(user, "\n");

      bnet_fsend(user, _("Device status:\n"));
      bnet_fsend(user, "%sOPENED ", dev->state & ST_OPENED ? "" : "!");
      bnet_fsend(user, "%sTAPE ", dev->state & ST_TAPE ? "" : "!");
      bnet_fsend(user, "%sLABEL ", dev->state & ST_LABEL ? "" : "!");
      bnet_fsend(user, "%sMALLOC ", dev->state & ST_MALLOC ? "" : "!");
      bnet_fsend(user, "%sAPPEND ", dev->state & ST_APPEND ? "" : "!");
      bnet_fsend(user, "%sREAD ", dev->state & ST_READ ? "" : "!");
      bnet_fsend(user, "%sEOT ", dev->state & ST_EOT ? "" : "!");
      bnet_fsend(user, "%sWEOT ", dev->state & ST_WEOT ? "" : "!");
      bnet_fsend(user, "%sEOF ", dev->state & ST_EOF ? "" : "!");
      bnet_fsend(user, "%sNEXTVOL ", dev->state & ST_NEXTVOL ? "" : "!");
      bnet_fsend(user, "%sSHORT ", dev->state & ST_SHORT ? "" : "!");
      bnet_fsend(user, "\n");

      bnet_fsend(user, _("Device parameters:\n"));
      bnet_fsend(user, "Device name: %s\n", dev->dev_name);
      bnet_fsend(user, "File=%u block=%u\n", dev->file, dev->block_num);
      bnet_fsend(user, "Min block=%u Max block=%u\n", dev->min_block_size, dev->max_block_size);
   }

}
