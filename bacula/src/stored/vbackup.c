/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2006-2014 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from many
   others, a complete list can be found in the file AUTHORS.

   You may use this file and others of this release according to the
   license defined in the LICENSE file, which includes the Affero General
   Public License, v3.0 ("AGPLv3") and some additional permissions and
   terms pursuant to its AGPLv3 Section 7.

   Bacula® is a registered trademark of Kern Sibbald.
*/
/*
 * SD -- vbackup.c --  responsible for doing virtual backup jobs.
 *
 *     Kern Sibbald, January MMVI
 *
 */

#include "bacula.h"
#include "stored.h"

/* Import functions */
extern char Job_end[];

/* Forward referenced subroutines */
static bool record_cb(DCR *dcr, DEV_RECORD *rec);

/*
 *  Read Data and send to File Daemon
 *   Returns: false on failure
 *            true  on success
 */
bool do_vbackup(JCR *jcr)
{
   bool ok = true;
   BSOCK *dir = jcr->dir_bsock;
   const char *Type;
   char ec1[50];
   DEVICE *dev;

   switch(jcr->getJobType()) {
   case JT_MIGRATE:
      Type = "Migration";
      break;
   case JT_ARCHIVE:
      Type = "Archive";
      break;
   case JT_COPY:
      Type = "Copy";
      break;
   case JT_BACKUP:
      Type = "Virtual Backup";
      break;
   default:
      Type = "Unknown";
      break;
   }


   Dmsg0(20, "Start read data.\n");

   if (!jcr->read_dcr || !jcr->dcr) {
      Jmsg(jcr, M_FATAL, 0, _("Read and write devices not properly initialized.\n"));
      goto bail_out;
   }
   Dmsg2(100, "read_dcr=%p write_dcr=%p\n", jcr->read_dcr, jcr->dcr);

   if (jcr->NumReadVolumes == 0) {
      Jmsg(jcr, M_FATAL, 0, _("No Volume names found for %s.\n"), Type);
      goto bail_out;
   }

   Dmsg3(200, "Found %d volumes names for %s. First=%s\n", jcr->NumReadVolumes,
      jcr->VolList->VolumeName, Type);

   ASSERT(jcr->read_dcr != jcr->dcr);
   ASSERT(jcr->read_dcr->dev != jcr->dcr->dev);
   /* Ready devices for reading and writing */
   if (!acquire_device_for_read(jcr->read_dcr) ||
       !acquire_device_for_append(jcr->dcr)) {
      jcr->setJobStatus(JS_ErrorTerminated);
      goto bail_out;
   }

   Dmsg2(200, "===== After acquire pos %u:%u\n", jcr->dcr->dev->file, jcr->dcr->dev->block_num);

   jcr->sendJobStatus(JS_Running);

   begin_data_spool(jcr->dcr);
   begin_attribute_spool(jcr);

   jcr->dcr->VolFirstIndex = jcr->dcr->VolLastIndex = 0;
   jcr->run_time = time(NULL);
   set_start_vol_position(jcr->dcr);

   jcr->JobFiles = 0;
   ok = read_records(jcr->read_dcr, record_cb, mount_next_read_volume);
   goto ok_out;

bail_out:
   ok = false;

ok_out:
   if (jcr->dcr) {
      dev = jcr->dcr->dev;
      Dmsg1(100, "ok=%d\n", ok);
      if (ok || dev->can_write()) {
         /* Flush out final partial block of this session */
         if (!jcr->dcr->write_block_to_device()) {
            Jmsg2(jcr, M_FATAL, 0, _("Fatal append error on device %s: ERR=%s\n"),
                  dev->print_name(), dev->bstrerror());
            Dmsg0(100, _("Set ok=FALSE after write_block_to_device.\n"));
            ok = false;
         }
         Dmsg2(200, "Flush block to device pos %u:%u\n", dev->file, dev->block_num);
      }

      if (!ok) {
         discard_data_spool(jcr->dcr);
      } else {
         /* Note: if commit is OK, the device will remain blocked */
         commit_data_spool(jcr->dcr);
      }

      /*
       * Don't use time_t for job_elapsed as time_t can be 32 or 64 bits,
       *   and the subsequent Jmsg() editing will break
       */
      int32_t job_elapsed = time(NULL) - jcr->run_time;

      if (job_elapsed <= 0) {
         job_elapsed = 1;
      }

      Jmsg(jcr, M_INFO, 0, _("Elapsed time=%02d:%02d:%02d, Transfer rate=%s Bytes/second\n"),
            job_elapsed / 3600, job_elapsed % 3600 / 60, job_elapsed % 60,
            edit_uint64_with_suffix(jcr->JobBytes / job_elapsed, ec1));

#ifdef BUILD_DVD
      if (ok && dev->is_dvd()) {
         ok = dvd_close_job(jcr->dcr);   /* do DVD cleanup if any */
      }
#endif

      /* Release the device -- and send final Vol info to DIR */
      release_device(jcr->dcr);

      if (!ok || job_canceled(jcr)) {
         discard_attribute_spool(jcr);
      } else {
         commit_attribute_spool(jcr);
      }
   }

   if (jcr->read_dcr) {
      if (!release_device(jcr->read_dcr)) {
         ok = false;
      }
   }

   jcr->sendJobStatus();              /* update director */

   Dmsg0(30, "Done reading.\n");
   jcr->end_time = time(NULL);
   dequeue_messages(jcr);             /* send any queued messages */
   if (ok) {
      jcr->setJobStatus(JS_Terminated);
   }
   generate_daemon_event(jcr, "JobEnd");
   generate_plugin_event(jcr, bsdEventJobEnd);
   dir->fsend(Job_end, jcr->Job, jcr->JobStatus, jcr->JobFiles,
      edit_uint64(jcr->JobBytes, ec1), jcr->JobErrors);
   Dmsg4(100, Job_end, jcr->Job, jcr->JobStatus, jcr->JobFiles, ec1);

   dir->signal(BNET_EOD);             /* send EOD to Director daemon */
   free_plugins(jcr);                 /* release instantiated plugins */

   return ok;
}


/*
 * Called here for each record from read_records()
 *  Returns: true if OK
 *           false if error
 */
static bool record_cb(DCR *dcr, DEV_RECORD *rec)
{
   JCR *jcr = dcr->jcr;
   DEVICE *dev = jcr->dcr->dev;
   char buf1[100], buf2[100];

#ifdef xxx
   Pmsg5(000, "on entry     JobId=%d FI=%s SessId=%d Strm=%s len=%d\n",
      jcr->JobId,
      FI_to_ascii(buf1, rec->FileIndex), rec->VolSessionId,
      stream_to_ascii(buf2, rec->Stream, rec->FileIndex), rec->data_len);
#endif
   /* If label and not for us, discard it */
   if (rec->FileIndex < 0 && rec->match_stat <= 0) {
      return true;
   }
   /* We want to write SOS_LABEL and EOS_LABEL discard all others */
   switch (rec->FileIndex) {
   case PRE_LABEL:
   case VOL_LABEL:
   case EOT_LABEL:
   case EOM_LABEL:
      return true;                    /* don't write vol labels */
   }

   /*
    * For normal migration jobs, FileIndex values are sequential because
    *  we are dealing with one job.  However, for Vbackup (consolidation),
    *  we will be getting records from multiple jobs and writing them back
    *  out, so we need to ensure that the output FileIndex is sequential.
    *  We do so by detecting a FileIndex change and incrementing the
    *  JobFiles, which we then use as the output FileIndex.
    */
   if (rec->FileIndex >= 0) {
      /* If something changed, increment FileIndex */
      if (rec->VolSessionId != rec->last_VolSessionId ||
          rec->VolSessionTime != rec->last_VolSessionTime ||
          rec->FileIndex != rec->last_FileIndex) {
         jcr->JobFiles++;
         rec->last_VolSessionId = rec->VolSessionId;
         rec->last_VolSessionTime = rec->VolSessionTime;
         rec->last_FileIndex = rec->FileIndex;
      }
      rec->FileIndex = jcr->JobFiles;     /* set sequential output FileIndex */
   }

   /*
    * Modify record SessionId and SessionTime to correspond to
    * output.
    */
   rec->VolSessionId = jcr->VolSessionId;
   rec->VolSessionTime = jcr->VolSessionTime;
   Dmsg5(200, "before write JobId=%d FI=%s SessId=%d Strm=%s len=%d\n",
      jcr->JobId,
      FI_to_ascii(buf1, rec->FileIndex), rec->VolSessionId,
      stream_to_ascii(buf2, rec->Stream, rec->FileIndex), rec->data_len);
   if (!jcr->dcr->write_record(rec)) {
      Jmsg2(jcr, M_FATAL, 0, _("Fatal append error on device %s: ERR=%s\n"),
            dev->print_name(), dev->bstrerror());
      return false;
   }
   /* Restore packet */
   rec->VolSessionId = rec->last_VolSessionId;
   rec->VolSessionTime = rec->last_VolSessionTime;
   if (rec->FileIndex < 0) {
      return true;                    /* don't send LABELs to Dir */
   }
   jcr->JobBytes += rec->data_len;   /* increment bytes this job */
   Dmsg5(500, "wrote_record JobId=%d FI=%s SessId=%d Strm=%s len=%d\n",
      jcr->JobId,
      FI_to_ascii(buf1, rec->FileIndex), rec->VolSessionId,
      stream_to_ascii(buf2, rec->Stream, rec->FileIndex), rec->data_len);

   send_attrs_to_dir(jcr, rec);

   return true;
}
