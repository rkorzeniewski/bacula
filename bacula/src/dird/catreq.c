/*
 *
 *   Bacula Director -- catreq.c -- handles the message channel
 *    catalog request from the Storage daemon.
 *
 *     Kern Sibbald, March MMI
 *
 *    This routine runs as a thread and must be thread reentrant.
 *
 *  Basic tasks done here:
 *	Handle Catalog services.
 *
 *   Version $Id$
 */
/*
   Copyright (C) 2000, 2001, 2002 Kern Sibbald and John Walker

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

/*
 * Handle catalog request
 *  For now, we simply return next Volume to be used
 */

/* Requests from the Storage daemon */
static char Find_media[] = "CatReq Job=%127s FindMedia=%d\n";
static char Get_Vol_Info[] = "CatReq Job=%127s GetVolInfo VolName=%127s write=%d\n";

static char Update_media[] = "CatReq Job=%127s UpdateMedia VolName=%s\
 VolJobs=%u VolFiles=%u VolBlocks=%u VolBytes=%" lld " VolMounts=%u\
 VolErrors=%u VolWrites=%u MaxVolBytes=%" lld " EndTime=%d VolStatus=%10s\
 Slot=%d relabel=%d\n";

static char Create_job_media[] = "CatReq Job=%127s CreateJobMedia \
 FirstIndex=%u LastIndex=%u StartFile=%u EndFile=%u \
 StartBlock=%u EndBlock=%u\n";


/* Responses  sent to Storage daemon */
static char OK_media[] = "1000 OK VolName=%s VolJobs=%u VolFiles=%u\
 VolBlocks=%u VolBytes=%s VolMounts=%u VolErrors=%u VolWrites=%u\
 MaxVolBytes=%s VolCapacityBytes=%s VolStatus=%s Slot=%d\
 MaxVolJobs=%u MaxVolFiles=%u\n";

static char OK_update[] = "1000 OK UpdateMedia\n";

/* static char FileAttributes[] = "UpdCat Job=%127s FileAttributes "; */


void catalog_request(JCR *jcr, BSOCK *bs, char *msg)
{
   MEDIA_DBR mr, sdmr; 
   JOBMEDIA_DBR jm;
   char Job[MAX_NAME_LENGTH];
   int index, ok, relabel, writing, retry = 0;
   POOLMEM *omsg;

   memset(&mr, 0, sizeof(mr));
   memset(&sdmr, 0, sizeof(sdmr));
   memset(&jm, 0, sizeof(jm));

   /*
    * Request to find next appendable Volume for this Job
    */
   Dmsg1(200, "catreq %s", bs->msg);
   if (sscanf(bs->msg, Find_media, &Job, &index) == 2) {
      mr.PoolId = jcr->PoolId;
      bstrncpy(mr.MediaType, jcr->store->media_type, sizeof(mr.MediaType));
      Dmsg2(120, "CatReq FindMedia: Id=%d, MediaType=%s\n",
	 mr.PoolId, mr.MediaType);
      /*
       * Find the Next Volume for Append
       */
next_volume:
      strcpy(mr.VolStatus, "Append");  /* want only appendable volumes */
      ok = db_find_next_volume(jcr, jcr->db, index, &mr);  
      Dmsg2(100, "catreq after find_next_vol ok=%d FW=%d\n", ok, mr.FirstWritten);
      if (!ok) {
	 /* Well, try finding recycled tapes */
	 ok = find_recycled_volume(jcr, &mr);
         Dmsg2(100, "find_recycled_volume1 %d FW=%d\n", ok, mr.FirstWritten);
	 if (!ok) {
	    prune_volumes(jcr);  
	    ok = recycle_oldest_purged_volume(jcr, &mr);
            Dmsg2(200, "find_recycled_volume2 %d FW=%d\n", ok, mr.FirstWritten);
	    if (!ok) {
	       /* See if we can create a new Volume */
	       ok = newVolume(jcr, &mr);
	    }
	 }

	 if (!ok && jcr->pool->purge_oldest_volume) {
            Dmsg1(200, "No next volume found. PurgeOldest=%d\n",
		jcr->pool->purge_oldest_volume);
	    /* Find oldest volume to recycle */
	    ok = db_find_next_volume(jcr, jcr->db, -1, &mr);
            Dmsg1(400, "Find oldest=%d\n", ok);
	    if (ok) {
	       UAContext *ua;
               Dmsg0(400, "Try purge.\n");
	       /* Try to purge oldest volume */
	       ua = new_ua_context(jcr);
               Jmsg(jcr, M_INFO, 0, _("Purging oldest volume \"%s\"\n"), mr.VolumeName);
	       ok = purge_jobs_from_volume(ua, &mr);
	       free_ua_context(ua);
	       if (ok) {
		  ok = recycle_oldest_purged_volume(jcr, &mr);
                  Dmsg1(400, "Recycle after recycle oldest=%d\n", ok);
	       }
	    }
	 }
      }
      /* Check if use duration has expired */
      Dmsg2(100, "VolJobs=%d FirstWritten=%d\n", mr.VolJobs, mr.FirstWritten);
      if (ok && mr.VolJobs > 0 && mr.VolUseDuration > 0 && 
           strcmp(mr.VolStatus, "Append") == 0) {
	 utime_t now = time(NULL);
	 if (mr.VolUseDuration <= (now - mr.FirstWritten)) {
            Dmsg4(100, "Duration=%d now=%d start=%d now-start=%d\n",
	       (int)mr.VolUseDuration, (int)now, (int)mr.FirstWritten, 
	       (int)(now-mr.FirstWritten));
            Jmsg(jcr, M_INFO, 0, _("Max configured use duration exceeded. "       
               "Marking Volume \"%s\" as Used.\n"), mr.VolumeName);
            strcpy(mr.VolStatus, "Used");  /* yes, mark as used */
	    if (!db_update_media_record(jcr, jcr->db, &mr)) {
               Jmsg(jcr, M_ERROR, 0, _("Catalog error updating Media record. %s"),
		    db_strerror(jcr->db));
	    } else if (retry++ < 200) {     /* sanity check */
	       goto next_volume;
	    } else {
	       Jmsg(jcr, M_ERROR, 0, _(
"We seem to be looping trying to find the next volume. I give up. Ask operator.\n"));
	    }
	    ok = FALSE; 	      /* give up */
	 }
      }

      /*
       * Send Find Media response to Storage daemon 
       */
      if (ok) {
	 char ed1[50], ed2[50], ed3[50];
	 jcr->MediaId = mr.MediaId;
	 pm_strcpy(&jcr->VolumeName, mr.VolumeName);
	 bash_spaces(mr.VolumeName);
	 bnet_fsend(bs, OK_media, mr.VolumeName, mr.VolJobs,
	    mr.VolFiles, mr.VolBlocks, edit_uint64(mr.VolBytes, ed1),
	    mr.VolMounts, mr.VolErrors, mr.VolWrites, 
	    edit_uint64(mr.MaxVolBytes, ed2), 
	    edit_uint64(mr.VolCapacityBytes, ed3),
	    mr.VolStatus, mr.Slot, mr.MaxVolJobs, mr.MaxVolFiles);
         Dmsg2(100, "Find media for %s: %s", jcr->Job, bs->msg);
      } else {
         bnet_fsend(bs, "1901 No Media.\n");
      }

   /* 
    * Request to find specific Volume information
    */
   } else if (sscanf(bs->msg, Get_Vol_Info, &Job, &mr.VolumeName, &writing) == 3) {
      Dmsg1(400, "CatReq GetVolInfo Vol=%s\n", mr.VolumeName);
      /*
       * Find the Volume
       */
      unbash_spaces(mr.VolumeName);
      if (db_get_media_record(jcr, jcr->db, &mr)) {
	 int VolSuitable = 0;
         char *reason = "";           /* detailed reason for rejection */
	 jcr->MediaId = mr.MediaId;
         Dmsg1(120, "VolumeInfo MediaId=%d\n", jcr->MediaId);
	 pm_strcpy(&jcr->VolumeName, mr.VolumeName);
	 if (!writing) {
	    VolSuitable = 1;	      /* accept anything for read */
	 } else {
	    /* 
	     * SD wants to write this Volume, so make
	     *	 sure it is suitable for this job, i.e.
	     *	 Pool matches, and it is either Append or Recycle 
	     *	 and Media Type matches and Pool allows any volume.
	     */
	    if (mr.PoolId != jcr->PoolId) {
               reason = "not in Pool";
            } else if (strcmp(mr.VolStatus, "Append") != 0 &&
                       strcmp(mr.VolStatus, "Recycle") != 0) {
               reason = "not Append or Recycle";
	    } else if (strcmp(mr.MediaType, jcr->store->media_type) != 0) {
               reason = "not correct MediaType";
	    } else if (!jcr->pool->accept_any_volume) {
               reason = "Volume not in sequence";
	    } else {
	       VolSuitable = 1;
	    }
	 }
	 if (VolSuitable) {
	    char ed1[50], ed2[50], ed3[50];
	    /*
	     * Send Find Media response to Storage daemon 
	     */
	    bash_spaces(mr.VolumeName);
	    bnet_fsend(bs, OK_media, mr.VolumeName, mr.VolJobs,
	       mr.VolFiles, mr.VolBlocks, edit_uint64(mr.VolBytes, ed1),
	       mr.VolMounts, mr.VolErrors, mr.VolWrites, 
	       edit_uint64(mr.MaxVolBytes, ed2), 
	       edit_uint64(mr.VolCapacityBytes, ed3),
	       mr.VolStatus, mr.Slot, mr.MaxVolJobs, mr.MaxVolFiles);
            Dmsg2(100, "Vol Info for %s: %s", jcr->Job, bs->msg);
	 } else { 
	    /* Not suitable volume */
            bnet_fsend(bs, "1998 Volume \"%s\" %s.\n",
	       mr.VolumeName, reason);
	 }

      } else {
         bnet_fsend(bs, "1997 Volume \"%s\" not in catalog.\n", mr.VolumeName);
      }

   
   /*
    * Request to update Media record. Comes typically at the end
    *  of a Storage daemon Job Session
    */
   } else if (sscanf(bs->msg, Update_media, &Job, &sdmr.VolumeName, &sdmr.VolJobs,
      &sdmr.VolFiles, &sdmr.VolBlocks, &sdmr.VolBytes, &sdmr.VolMounts, &sdmr.VolErrors,
      &sdmr.VolWrites, &sdmr.MaxVolBytes, &sdmr.LastWritten, &sdmr.VolStatus, 
      &sdmr.Slot, &relabel) == 14) {

      Dmsg3(400, "Update media %s oldStat=%s newStat=%s\n", sdmr.VolumeName,
	 mr.VolStatus, sdmr.VolStatus);
      bstrncpy(mr.VolumeName, sdmr.VolumeName, sizeof(mr.VolumeName)); /* copy Volume name */
      unbash_spaces(mr.VolumeName);
      if (!db_get_media_record(jcr, jcr->db, &mr)) {
         Jmsg(jcr, M_ERROR, 0, _("Unable to get Media record for Volume %s: ERR=%s\n"),
	      mr.VolumeName, db_strerror(jcr->db));
         bnet_fsend(bs, "1991 Catalog Request failed: %s", db_strerror(jcr->db));
	 return;
      }
      /* Set first written time if this is first job */
      if (mr.VolJobs == 0 || sdmr.VolJobs == 1) {
	 mr.FirstWritten = jcr->start_time;   /* use Job start time as first write */
      }
      Dmsg2(200, "Update media: BefVolJobs=%u After=%u\n", mr.VolJobs, sdmr.VolJobs);
      /* Copy updated values to original media record */
      mr.VolJobs     = sdmr.VolJobs;
      mr.VolFiles    = sdmr.VolFiles;
      mr.VolBlocks   = sdmr.VolBlocks;
      mr.VolBytes    = sdmr.VolBytes;
      mr.VolMounts   = sdmr.VolMounts;
      mr.VolErrors   = sdmr.VolErrors;
      mr.VolWrites   = sdmr.VolWrites;
      mr.LastWritten = sdmr.LastWritten;
      bstrncpy(mr.VolStatus, sdmr.VolStatus, sizeof(mr.VolStatus));
      mr.Slot = sdmr.Slot;

      /*     
       * Update Media Record
       */

      /* Check limits and expirations if "Append" and not a relable request */
      if (strcmp(mr.VolStatus, "Append") == 0 && !relabel) {
	 /* First handle Max Volume Bytes */
	 if ((mr.MaxVolBytes > 0 && mr.VolBytes >= mr.MaxVolBytes)) {
            Jmsg(jcr, M_INFO, 0, _("Max Volume bytes exceeded. "             
                "Marking Volume \"%s\" as Full.\n"), mr.VolumeName);
            strcpy(mr.VolStatus, "Full");

	 /* Now see if Volume should only be used once */
	 } else if (mr.VolBytes > 0 && jcr->pool->use_volume_once) {
            Jmsg(jcr, M_INFO, 0, _("Volume used once. "             
                "Marking Volume \"%s\" as Used.\n"), mr.VolumeName);
            strcpy(mr.VolStatus, "Used");

	 /* Now see if Max Jobs written to volume */
	 } else if (mr.MaxVolJobs > 0 && mr.MaxVolJobs <= mr.VolJobs) {
            Jmsg(jcr, M_INFO, 0, _("Max Volume jobs exceeded. "       
                "Marking Volume \"%s\" as Used.\n"), mr.VolumeName);
            strcpy(mr.VolStatus, "Used");

	 /* Now see if Max Files written to volume */
	 } else if (mr.MaxVolFiles > 0 && mr.MaxVolFiles <= mr.VolFiles) {
            Jmsg(jcr, M_INFO, 0, _("Max Volume files exceeded. "       
                "Marking Volume \"%s\" as Used.\n"), mr.VolumeName);
            strcpy(mr.VolStatus, "Used");

	 /* Finally, check Use duration expiration */
	 } else if (mr.VolUseDuration > 0) {
	    utime_t now = time(NULL);
	    /* See if Vol Use has expired */
	    if (mr.VolUseDuration <= (now - mr.FirstWritten)) {
               Jmsg(jcr, M_INFO, 0, _("Max configured use duration exceeded. "       
                  "Marking Volume \"%s\"as Used.\n"), mr.VolumeName);
               strcpy(mr.VolStatus, "Used");  /* yes, mark as used */
	    }
	 }
      }
      Dmsg2(200, "db_update_media_record. Stat=%s Vol=%s\n", mr.VolStatus, mr.VolumeName);
      if (db_update_media_record(jcr, jcr->db, &mr)) {
	 bnet_fsend(bs, OK_update);
         Dmsg0(190, "send OK\n");
      } else {
         Jmsg(jcr, M_ERROR, 0, _("Catalog error updating Media record. %s"),
	    db_strerror(jcr->db));
         bnet_fsend(bs, "1992 Update Media error\n");
         Dmsg0(190, "send error\n");
      }

   /*
    * Request to create a JobMedia record
    */
   } else if (sscanf(bs->msg, Create_job_media, &Job,
      &jm.FirstIndex, &jm.LastIndex, &jm.StartFile, &jm.EndFile,
      &jm.StartBlock, &jm.EndBlock) == 7) {

      jm.JobId = jcr->JobId;
      jm.MediaId = jcr->MediaId;
      Dmsg6(100, "create_jobmedia JobId=%d MediaId=%d SF=%d EF=%d FI=%d LI=%d\n",
	 jm.JobId, jm.MediaId, jm.StartFile, jm.EndFile, jm.FirstIndex, jm.LastIndex);
      if (!db_create_jobmedia_record(jcr, jcr->db, &jm)) {
         Jmsg(jcr, M_ERROR, 0, _("Catalog error creating JobMedia record. %s"),
	    db_strerror(jcr->db));
         bnet_fsend(bs, "1991 Update JobMedia error\n");
      } else {
         Dmsg0(100, "JobMedia record created\n");
	 bnet_fsend(bs, OK_update);
      }

   } else {
      omsg = get_memory(bs->msglen+1);
      pm_strcpy(&omsg, bs->msg);
      bnet_fsend(bs, "1990 Invalid Catalog Request: %s", omsg);    
      Jmsg1(jcr, M_ERROR, 0, _("Invalid Catalog request: %s"), omsg);
      free_memory(omsg);
   }
   Dmsg1(120, ">CatReq response: %s", bs->msg);
   Dmsg1(200, "Leave catreq jcr 0x%x\n", jcr);
   return;
}

/*
 * Update File Attributes in the catalog with data
 *  sent by the Storage daemon.  Note, we receive the whole
 *  attribute record, but we select out only the stat packet,
 *  VolSessionId, VolSessionTime, FileIndex, and file name 
 *  to store in the catalog.
 */
void catalog_update(JCR *jcr, BSOCK *bs, char *msg)
{
   unser_declare;
   uint32_t VolSessionId, VolSessionTime;
   int32_t Stream;
   uint32_t FileIndex;
   uint32_t data_len;
   char *p = bs->msg;
   int len;
   char *fname, *attr;
   ATTR_DBR ar;

   if (!jcr->pool->catalog_files) {
      return;
   }
   db_start_transaction(jcr, jcr->db);	   /* start transaction if not already open */
   skip_nonspaces(&p);		      /* UpdCat */
   skip_spaces(&p);
   skip_nonspaces(&p);		      /* Job=nnn */
   skip_spaces(&p);
   skip_nonspaces(&p);		      /* FileAttributes */
   p += 1;
   unser_begin(p, 0);
   unser_uint32(VolSessionId);
   unser_uint32(VolSessionTime);
   unser_int32(FileIndex);
   unser_int32(Stream);
   unser_uint32(data_len);
   p += unser_length(p);

   Dmsg1(99, "UpdCat msg=%s\n", bs->msg);
   Dmsg5(99, "UpdCat VolSessId=%d VolSessT=%d FI=%d Strm=%d data_len=%d\n",
      VolSessionId, VolSessionTime, FileIndex, Stream, data_len);

   if (Stream == STREAM_UNIX_ATTRIBUTES || Stream == STREAM_UNIX_ATTRIBUTES_EX) {
      skip_nonspaces(&p);	      /* skip FileIndex */
      skip_spaces(&p);
      skip_nonspaces(&p);	      /* skip FileType */
      skip_spaces(&p);
      fname = p;
      len = strlen(fname);	  /* length before attributes */
      attr = &fname[len+1];

      Dmsg2(109, "dird<stored: stream=%d %s\n", Stream, fname);
      Dmsg1(109, "dird<stored: attr=%s\n", attr);
      ar.attr = attr; 
      ar.fname = fname;
      ar.FileIndex = FileIndex;
      ar.Stream = Stream;
      ar.link = NULL;
      ar.JobId = jcr->JobId;

      Dmsg2(111, "dird<filed: stream=%d %s\n", Stream, fname);
      Dmsg1(120, "dird<filed: attr=%s\n", attr);

      if (!db_create_file_attributes_record(jcr, jcr->db, &ar)) {
         Jmsg1(jcr, M_FATAL, 0, _("Attribute create error. %s"), db_strerror(jcr->db));
      }
      /* Save values for SIG update */
      jcr->FileId = ar.FileId;
      jcr->FileIndex = FileIndex;
   } else if (Stream == STREAM_MD5_SIGNATURE || Stream == STREAM_SHA1_SIGNATURE) {
      fname = p;
      if (jcr->FileIndex != FileIndex) {    
         Jmsg(jcr, M_WARNING, 0, "Got MD5/SHA1 but not same File as attributes\n");
      } else {
	 /* Update signature in catalog */
	 char SIGbuf[50];	    /* 24 bytes should be enough */
	 int len, type;
	 if (Stream == STREAM_MD5_SIGNATURE) {
	    len = 16;
	    type = MD5_SIG;
	 } else {
	    len = 20;
	    type = SHA1_SIG;
	 }
	 bin_to_base64(SIGbuf, fname, len);
         Dmsg3(190, "SIGlen=%d SIG=%s type=%d\n", strlen(SIGbuf), SIGbuf, Stream);
	 if (!db_add_SIG_to_file_record(jcr, jcr->db, jcr->FileId, SIGbuf, type)) {
            Jmsg(jcr, M_ERROR, 0, _("Catalog error updating MD5/SHA1. %s"), 
	       db_strerror(jcr->db));
	 }
      }
   }
}
