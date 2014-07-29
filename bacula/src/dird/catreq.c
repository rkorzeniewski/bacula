/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2001-2014 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from many
   others, a complete list can be found in the file AUTHORS.

   You may use this file and others of this release according to the
   license defined in the LICENSE file, which includes the Affero General
   Public License, v3.0 ("AGPLv3") and some additional permissions and
   terms pursuant to its AGPLv3 Section 7.

   Bacula® is a registered trademark of Kern Sibbald.
*/
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
 *      Handle Catalog services.
 *
 */

#include "bacula.h"
#include "dird.h"
#include "findlib/find.h"

/*
 * Handle catalog request
 *  For now, we simply return next Volume to be used
 */

/* Requests from the Storage daemon */
static char Find_media[] = "CatReq Job=%127s FindMedia=%d pool_name=%127s media_type=%127s\n";
static char Get_Vol_Info[] = "CatReq Job=%127s GetVolInfo VolName=%127s write=%d\n";

static char Update_media[] = "CatReq Job=%127s UpdateMedia VolName=%s"
   " VolJobs=%u VolFiles=%u VolBlocks=%u VolBytes=%lld VolMounts=%u"
   " VolErrors=%u VolWrites=%lld MaxVolBytes=%lld EndTime=%lld VolStatus=%10s"
   " Slot=%d relabel=%d InChanger=%d VolReadTime=%lld VolWriteTime=%lld"
   " VolFirstWritten=%lld VolParts=%u\n";

static char Create_job_media[] = "CatReq Job=%127s CreateJobMedia "
   " FirstIndex=%u LastIndex=%u StartFile=%u EndFile=%u "
   " StartBlock=%u EndBlock=%u Copy=%d Strip=%d MediaId=%" lld "\n";


/* Responses  sent to Storage daemon */
static char OK_media[] = "1000 OK VolName=%s VolJobs=%u VolFiles=%u"
   " VolBlocks=%u VolBytes=%s"
   " VolMounts=%u VolErrors=%u VolWrites=%s"
   " MaxVolBytes=%s VolCapacityBytes=%s VolStatus=%s Slot=%d"
   " MaxVolJobs=%u MaxVolFiles=%u InChanger=%d VolReadTime=%s"
   " VolWriteTime=%s EndFile=%u EndBlock=%u VolParts=%u LabelType=%d"
   " MediaId=%s ScratchPoolId=%s\n";

static char OK_create[] = "1000 OK CreateJobMedia\n";


static int send_volume_info_to_storage_daemon(JCR *jcr, BSOCK *sd, MEDIA_DBR *mr)
{
   int stat;
   char ed1[50], ed4[50], ed5[50], ed6[50], ed7[50], ed8[50],
        ed9[50], ed10[50];

   jcr->MediaId = mr->MediaId;
   pm_strcpy(jcr->VolumeName, mr->VolumeName);
   bash_spaces(mr->VolumeName);
   stat = sd->fsend(OK_media, mr->VolumeName, mr->VolJobs,
      mr->VolFiles, mr->VolBlocks, edit_uint64(mr->VolBytes, ed1),
      mr->VolMounts, mr->VolErrors,
      edit_uint64(mr->VolWrites, ed4),
      edit_uint64(mr->MaxVolBytes, ed5),
      edit_uint64(mr->VolCapacityBytes, ed6),
      mr->VolStatus, mr->Slot, mr->MaxVolJobs, mr->MaxVolFiles,
      mr->InChanger,
      edit_int64(mr->VolReadTime, ed7),
      edit_int64(mr->VolWriteTime, ed8),
      mr->EndFile, mr->EndBlock,
      mr->VolParts,
      mr->LabelType,
      edit_uint64(mr->MediaId, ed9),
      edit_uint64(mr->ScratchPoolId, ed10));
   unbash_spaces(mr->VolumeName);
   Dmsg2(100, "Vol Info for %s: %s", jcr->Job, sd->msg);
   return stat;
}

void catalog_request(JCR *jcr, BSOCK *bs)
{
   MEDIA_DBR mr, sdmr;
   JOBMEDIA_DBR jm;
   char Job[MAX_NAME_LENGTH];
   char pool_name[MAX_NAME_LENGTH];
   int index, ok, label, writing;
   POOLMEM *omsg;
   POOL_DBR pr;
   uint32_t Stripe, Copy;
   uint64_t MediaId;
   utime_t VolFirstWritten;
   utime_t VolLastWritten;
   int n;

   memset(&sdmr, 0, sizeof(sdmr));
   memset(&jm, 0, sizeof(jm));
   Dsm_check(100);

   /*
    * Request to find next appendable Volume for this Job
    */
   Dmsg1(200, "catreq %s", bs->msg);
   if (!jcr->db) {
      omsg = get_memory(bs->msglen+1);
      pm_strcpy(omsg, bs->msg);
      bs->fsend(_("1990 Invalid Catalog Request: %s"), omsg);
      Jmsg1(jcr, M_FATAL, 0, _("Invalid Catalog request; DB not open: %s"), omsg);
      free_memory(omsg);
      return;
   }
   /*
    * Find next appendable medium for SD
    */
   n = sscanf(bs->msg, Find_media, &Job, &index, &pool_name, &mr.MediaType);
   if (n == 4) {
      memset(&pr, 0, sizeof(pr));
      bstrncpy(pr.Name, pool_name, sizeof(pr.Name));
      unbash_spaces(pr.Name);
      ok = db_get_pool_record(jcr, jcr->db, &pr);
      if (ok) {
         mr.PoolId = pr.PoolId;
         set_storageid_in_mr(jcr->wstore, &mr);
         mr.ScratchPoolId = pr.ScratchPoolId;
         ok = find_next_volume_for_append(jcr, &mr, index, fnv_create_vol, fnv_prune);
         Dmsg3(050, "find_media ok=%d idx=%d vol=%s\n", ok, index, mr.VolumeName);
      } else {
         /* Report problem finding pool */
         Jmsg1(jcr, M_WARNING, 0, _("Pool \"%s\" not found for SD find media request.\n"),
            pr.Name);
      }
      /*
       * Send Find Media response to Storage daemon
       */
      if (ok) {
         send_volume_info_to_storage_daemon(jcr, bs, &mr);
      } else {
         bs->fsend(_("1901 No Media.\n"));
         Dmsg0(500, "1901 No Media.\n");
      }
      goto ok_out;
   }
   Dmsg1(1000, "Tried find_media. fields wanted=4, got=%d\n", n);

   /*
    * Request to find specific Volume information
    */
   n = sscanf(bs->msg, Get_Vol_Info, &Job, &mr.VolumeName, &writing);
   if (n == 3) {
      Dmsg1(100, "CatReq GetVolInfo Vol=%s\n", mr.VolumeName);
      /*
       * Find the Volume
       */
      unbash_spaces(mr.VolumeName);
      if (db_get_media_record(jcr, jcr->db, &mr)) {
         const char *reason = NULL;           /* detailed reason for rejection */
         /*
          * If we are reading, accept any volume (reason == NULL)
          * If we are writing, check if the Volume is valid
          *   for this job, and do a recycle if necessary
          */
         if (writing) {
            /*
             * SD wants to write this Volume, so make
             *   sure it is suitable for this job, i.e.
             *   Pool matches, and it is either Append or Recycle
             *   and Media Type matches and Pool allows any volume.
             */
            if (mr.PoolId != jcr->jr.PoolId) {
               reason = _("not in Pool");
            } else if (strcmp(mr.MediaType, jcr->wstore->media_type) != 0) {
               reason = _("not correct MediaType");
            } else {
               /*
                * Now try recycling if necessary
                *   reason set non-NULL if we cannot use it
                */
               check_if_volume_valid_or_recyclable(jcr, &mr, &reason);
            }
         }
         if (!reason && mr.Enabled != 1) {
            reason = _("is not Enabled");
         }
         if (reason == NULL) {
            /*
             * Send Find Media response to Storage daemon
             */
            send_volume_info_to_storage_daemon(jcr, bs, &mr);
         } else {
            /* Not suitable volume */
            bs->fsend(_("1998 Volume \"%s\" catalog status is %s, %s.\n"), mr.VolumeName,
               mr.VolStatus, reason);
         }

      } else {
         bs->fsend(_("1997 Volume \"%s\" not in catalog.\n"), mr.VolumeName);
         Dmsg1(100, "1997 Volume \"%s\" not in catalog.\n", mr.VolumeName);
      }
      goto ok_out;
   }
   Dmsg1(1000, "Tried get_vol_info. fields wanted=3, got=%d\n", n);


   /*
    * Request to update Media record. Comes typically at the end
    *  of a Storage daemon Job Session, when labeling/relabeling a
    *  Volume, or when an EOF mark is written.
    */
   n = sscanf(bs->msg, Update_media, &Job, &sdmr.VolumeName,
      &sdmr.VolJobs, &sdmr.VolFiles, &sdmr.VolBlocks, &sdmr.VolBytes,
      &sdmr.VolMounts, &sdmr.VolErrors, &sdmr.VolWrites, &sdmr.MaxVolBytes,
      &VolLastWritten, &sdmr.VolStatus, &sdmr.Slot, &label, &sdmr.InChanger,
      &sdmr.VolReadTime, &sdmr.VolWriteTime, &VolFirstWritten,
      &sdmr.VolParts);
    if (n == 19) {
      db_lock(jcr->db);
      Dmsg3(400, "Update media %s oldStat=%s newStat=%s\n", sdmr.VolumeName,
         mr.VolStatus, sdmr.VolStatus);
      bstrncpy(mr.VolumeName, sdmr.VolumeName, sizeof(mr.VolumeName)); /* copy Volume name */
      unbash_spaces(mr.VolumeName);
      if (!db_get_media_record(jcr, jcr->db, &mr)) {
         Jmsg(jcr, M_ERROR, 0, _("Unable to get Media record for Volume %s: ERR=%s\n"),
              mr.VolumeName, db_strerror(jcr->db));
         bs->fsend(_("1991 Catalog Request for vol=%s failed: %s"),
            mr.VolumeName, db_strerror(jcr->db));
         db_unlock(jcr->db);
         return;
      }
      /* Set first written time if this is first job */
      if (mr.FirstWritten == 0) {
         if (VolFirstWritten == 0) {
            mr.FirstWritten = jcr->start_time;   /* use Job start time as first write */
         } else {
            mr.FirstWritten = VolFirstWritten;
         }
         mr.set_first_written = true;
      }
      /* If we just labeled the tape set time */
      if (label || mr.LabelDate == 0) {
         mr.LabelDate = jcr->start_time;
         mr.set_label_date = true;
         if (mr.InitialWrite == 0) {
            mr.InitialWrite = jcr->start_time;
         }
         Dmsg2(400, "label=%d labeldate=%d\n", label, mr.LabelDate);
      } else {
         /*
          * Insanity check for VolFiles get set to a smaller value
          */
         if (sdmr.VolFiles < mr.VolFiles) {
            Jmsg(jcr, M_INFO, 0, _("Attempt to set Volume Files from %u to %u"
                 " for Volume \"%s\". Ignored.\n"),
               mr.VolFiles, sdmr.VolFiles, mr.VolumeName);
            sdmr.VolFiles = mr.VolFiles;  /* keep orginal value */
         }
      }
      Dmsg2(400, "Update media: BefVolJobs=%u After=%u\n", mr.VolJobs, sdmr.VolJobs);

      /*
       * Check if the volume has been written by the job,
       * and update the LastWritten field if needed.
       */
      if (mr.VolBlocks != sdmr.VolBlocks && VolLastWritten != 0) {
         mr.LastWritten = VolLastWritten;
      }

      /*
       * Update to point to the last device used to write the Volume.
       *   However, do so only if we are writing the tape, i.e.
       *   the number of VolWrites has increased.
       */
      if (jcr->wstore && sdmr.VolWrites > mr.VolWrites) {
         Dmsg2(050, "Update StorageId old=%d new=%d\n",
               mr.StorageId, jcr->wstore->StorageId);
         /* Update StorageId after write */
         set_storageid_in_mr(jcr->wstore, &mr);
      } else {
         /* Nothing written, reset same StorageId */
         set_storageid_in_mr(NULL, &mr);
      }

      /* Copy updated values to original media record */
      mr.VolJobs      = sdmr.VolJobs;
      mr.VolFiles     = sdmr.VolFiles;
      mr.VolBlocks    = sdmr.VolBlocks;
      mr.VolBytes     = sdmr.VolBytes;
      mr.VolMounts    = sdmr.VolMounts;
      mr.VolErrors    = sdmr.VolErrors;
      mr.VolWrites    = sdmr.VolWrites;
      mr.Slot         = sdmr.Slot;
      mr.InChanger    = sdmr.InChanger;
      mr.VolParts     = sdmr.VolParts;
      bstrncpy(mr.VolStatus, sdmr.VolStatus, sizeof(mr.VolStatus));
      if (sdmr.VolReadTime >= 0) {
         mr.VolReadTime  = sdmr.VolReadTime;
      }
      if (sdmr.VolWriteTime >= 0) {
         mr.VolWriteTime = sdmr.VolWriteTime;
      }

      Dmsg2(400, "db_update_media_record. Stat=%s Vol=%s\n", mr.VolStatus, mr.VolumeName);
      /*
       * Update the database, then before sending the response to the
       *  SD, check if the Volume has expired.
       */
      if (!db_update_media_record(jcr, jcr->db, &mr)) {
         Jmsg(jcr, M_FATAL, 0, _("Catalog error updating Media record. %s"),
            db_strerror(jcr->db));
         bs->fsend(_("1993 Update Media error\n"));
         Pmsg0(000, "1993 Update Media error\n");
      } else {
         (void)has_volume_expired(jcr, &mr);
         send_volume_info_to_storage_daemon(jcr, bs, &mr);
      }
      db_unlock(jcr->db);
      goto ok_out;
   }
   Dmsg1(1000, "Tried update_media. fields wanted=20, got=%d\n", n);

   /*
    * Request to create a JobMedia record
    */
   n = sscanf(bs->msg, Create_job_media, &Job,
      &jm.FirstIndex, &jm.LastIndex, &jm.StartFile, &jm.EndFile,
      &jm.StartBlock, &jm.EndBlock, &Copy, &Stripe, &MediaId);
   if (n == 10) {
      if (jcr->wjcr) {
         jm.JobId = jcr->wjcr->JobId;
      } else {
         jm.JobId = jcr->JobId;
      }
      jm.MediaId = MediaId;
      Dmsg6(400, "create_jobmedia JobId=%d MediaId=%d SF=%d EF=%d FI=%d LI=%d\n",
         jm.JobId, jm.MediaId, jm.StartFile, jm.EndFile, jm.FirstIndex, jm.LastIndex);
      if (!db_create_jobmedia_record(jcr, jcr->db, &jm)) {
         Jmsg(jcr, M_FATAL, 0, _("Catalog error creating JobMedia record. %s"),
            db_strerror(jcr->db));
         bs->fsend(_("1992 Create JobMedia error\n"));
      } else {
         Dmsg0(400, "JobMedia record created\n");
         bs->fsend(OK_create);
      }
      goto ok_out;
   }
   Dmsg1(1000, "Tried create_jobmedia. fields wanted=10, got=%d\n", n);

   /* Everything failed. Send error message. */
   omsg = get_memory(bs->msglen+1);
   pm_strcpy(omsg, bs->msg);
   bs->fsend(_("1990 Invalid Catalog Request: %s"), omsg);
   Jmsg1(jcr, M_FATAL, 0, _("Invalid Catalog request: %s"), omsg);
   free_memory(omsg);

ok_out:
   Dmsg1(400, ">CatReq response: %s", bs->msg);
   Dmsg1(400, "Leave catreq jcr 0x%x\n", jcr);
   return;
}

/*
 * Note, we receive the whole attribute record, but we select out only the stat
 * packet, VolSessionId, VolSessionTime, FileIndex, file type, and file name to
 * store in the catalog.
 */
static void update_attribute(JCR *jcr, char *msg, int32_t msglen)
{
   unser_declare;
   uint32_t VolSessionId, VolSessionTime;
   int32_t Stream;
   uint32_t FileIndex;
   char *p;
   int len;
   char *fname, *attr;
   ATTR_DBR *ar = NULL;
   uint32_t reclen;

   /* Start transaction allocates jcr->attr and jcr->ar if needed */
   db_start_transaction(jcr, jcr->db);     /* start transaction if not already open */
   ar = jcr->ar;

   /*
    * Start by scanning directly in the message buffer to get Stream
    *  there may be a cached attr so we cannot yet write into
    *  jcr->attr or jcr->ar
    */
   p = msg;
   skip_nonspaces(&p);                /* UpdCat */
   skip_spaces(&p);
   skip_nonspaces(&p);                /* Job=nnn */
   skip_spaces(&p);
   skip_nonspaces(&p);                /* "FileAttributes" */
   p += 1;
   /* The following "SD header" fields are serialized */
   unser_begin(p, 0);
   unser_uint32(VolSessionId);        /* VolSessionId */
   unser_uint32(VolSessionTime);      /* VolSessionTime */
   unser_int32(FileIndex);            /* FileIndex */
   unser_int32(Stream);               /* Stream */
   unser_uint32(reclen);              /* Record length */
   p += unser_length(p);              /* Raw record follows */

   /**
    * At this point p points to the raw record, which varies according
    *  to what kind of a record (Stream) was sent.  Note, the integer
    *  fields at the beginning of these "raw" records are in ASCII with
    *  spaces between them so one can use scanf or manual scanning to
    *  extract the fields.
    *
    * File Attributes
    *   File_index
    *   File type
    *   Filename (full path)
    *   Encoded attributes
    *   Link name (if type==FT_LNK or FT_LNKSAVED)
    *   Encoded extended-attributes (for Win32)
    *   Delta sequence number (32 bit int)
    *
    * Restore Object
    *   File_index
    *   File_type
    *   Object_index
    *   Object_len (possibly compressed)
    *   Object_full_len (not compressed)
    *   Object_compression
    *   Plugin_name
    *   Object_name
    *   Binary Object data
    */

   Dmsg1(400, "UpdCat msg=%s\n", msg);
   Dmsg5(400, "UpdCat VolSessId=%d VolSessT=%d FI=%d Strm=%d reclen=%d\n",
      VolSessionId, VolSessionTime, FileIndex, Stream, reclen);

   if (Stream == STREAM_UNIX_ATTRIBUTES || Stream == STREAM_UNIX_ATTRIBUTES_EX) {
      if (jcr->cached_attribute) {
         Dmsg2(400, "Cached attr. Stream=%d fname=%s\n", ar->Stream, ar->fname);
         if (!db_create_attributes_record(jcr, jcr->db, ar)) {
            Jmsg1(jcr, M_FATAL, 0, _("Attribute create error: ERR=%s"), db_strerror(jcr->db));
         }
         jcr->cached_attribute = false;
      }
      /* Any cached attr is flushed so we can reuse jcr->attr and jcr->ar */
      jcr->attr = check_pool_memory_size(jcr->attr, msglen);
      memcpy(jcr->attr, msg, msglen);
      p = jcr->attr - msg + p;    /* point p into jcr->attr */
      skip_nonspaces(&p);         /* skip FileIndex */
      skip_spaces(&p);
      ar->FileType = str_to_int32(p);
      skip_nonspaces(&p);         /* skip FileType */
      skip_spaces(&p);
      fname = p;
      len = strlen(fname);        /* length before attributes */
      attr = &fname[len+1];
      ar->DeltaSeq = 0;
      if (ar->FileType == FT_REG) {
         p = attr + strlen(attr) + 1;  /* point to link */
         p = p + strlen(p) + 1;        /* point to extended attributes */
         p = p + strlen(p) + 1;        /* point to delta sequence */
         /*
          * Older FDs don't have a delta sequence, so check if it is there
          */
         if (p - jcr->attr < msglen) {
            ar->DeltaSeq = str_to_int32(p); /* delta_seq */
         }
      }

      Dmsg2(400, "dird<stored: stream=%d %s\n", Stream, fname);
      Dmsg1(400, "dird<stored: attr=%s\n", attr);
      ar->attr = attr;
      ar->fname = fname;
      if (ar->FileType == FT_DELETED) {
         ar->FileIndex = 0;     /* special value */
      } else {
         ar->FileIndex = FileIndex;
      }
      ar->Stream = Stream;
      ar->link = NULL;
      if (jcr->wjcr) {
         ar->JobId = jcr->wjcr->JobId;
         Dmsg1(100, "=== set JobId=%d\n", ar->JobId);
      } else {
         ar->JobId = jcr->JobId;
      }
      ar->Digest = NULL;
      ar->DigestType = CRYPTO_DIGEST_NONE;
      jcr->cached_attribute = true;

      Dmsg2(400, "dird<filed: stream=%d %s\n", Stream, fname);
      Dmsg1(400, "dird<filed: attr=%s\n", attr);

   } else if (Stream == STREAM_RESTORE_OBJECT) {
      ROBJECT_DBR ro;

      memset(&ro, 0, sizeof(ro));
      ro.Stream = Stream;
      ro.FileIndex = FileIndex;
      if (jcr->wjcr) {
         ro.JobId = jcr->wjcr->JobId;
         Dmsg1(100, "=== set JobId=%d\n", ar->JobId);
      } else {
         ro.JobId = jcr->JobId;
      }

      Dmsg1(100, "Robj=%s\n", p);

      skip_nonspaces(&p);                  /* skip FileIndex */
      skip_spaces(&p);
      ro.FileType = str_to_int32(p);        /* FileType */
      skip_nonspaces(&p);
      skip_spaces(&p);
      ro.object_index = str_to_int32(p);    /* Object Index */
      skip_nonspaces(&p);
      skip_spaces(&p);
      ro.object_len = str_to_int32(p);      /* object length possibly compressed */
      skip_nonspaces(&p);
      skip_spaces(&p);
      ro.object_full_len = str_to_int32(p); /* uncompressed object length */
      skip_nonspaces(&p);
      skip_spaces(&p);
      ro.object_compression = str_to_int32(p); /* compression */
      skip_nonspaces(&p);
      skip_spaces(&p);

      ro.plugin_name = p;                      /* point to plugin name */
      len = strlen(ro.plugin_name);
      ro.object_name = &ro.plugin_name[len+1]; /* point to object name */
      len = strlen(ro.object_name);
      ro.object = &ro.object_name[len+1];      /* point to object */
      ro.object[ro.object_len] = 0;            /* add zero for those who attempt printing */
      Dmsg7(100, "oname=%s stream=%d FT=%d FI=%d JobId=%d, obj_len=%d\nobj=\"%s\"\n",
         ro.object_name, ro.Stream, ro.FileType, ro.FileIndex, ro.JobId,
         ro.object_len, ro.object);
      /* Send it */
      if (!db_create_restore_object_record(jcr, jcr->db, &ro)) {
         Jmsg1(jcr, M_FATAL, 0, _("Restore object create error. %s"), db_strerror(jcr->db));
         jcr->cached_attribute = false;
      }

   } else if (crypto_digest_stream_type(Stream) != CRYPTO_DIGEST_NONE) {
      fname = p;
      if (ar->FileIndex != FileIndex) {
         Jmsg3(jcr, M_WARNING, 0, _("%s not same File=%d as attributes=%d\n"),
            stream_to_ascii(Stream), FileIndex, ar->FileIndex);
      } else {
         /* Update digest in catalog */
         char digestbuf[BASE64_SIZE(CRYPTO_DIGEST_MAX_SIZE)];
         int len = 0;
         int type = CRYPTO_DIGEST_NONE;

         switch(Stream) {
         case STREAM_MD5_DIGEST:
            len = CRYPTO_DIGEST_MD5_SIZE;
            type = CRYPTO_DIGEST_MD5;
            break;
         case STREAM_SHA1_DIGEST:
            len = CRYPTO_DIGEST_SHA1_SIZE;
            type = CRYPTO_DIGEST_SHA1;
            break;
         case STREAM_SHA256_DIGEST:
            len = CRYPTO_DIGEST_SHA256_SIZE;
            type = CRYPTO_DIGEST_SHA256;
            break;
         case STREAM_SHA512_DIGEST:
            len = CRYPTO_DIGEST_SHA512_SIZE;
            type = CRYPTO_DIGEST_SHA512;
            break;
         default:
            /* Never reached ... */
            Jmsg(jcr, M_ERROR, 0, _("Catalog error updating file digest. Unsupported digest stream type: %d"),
                 Stream);
         }

         bin_to_base64(digestbuf, sizeof(digestbuf), fname, len, true);
         Dmsg3(400, "DigestLen=%d Digest=%s type=%d\n", strlen(digestbuf),
               digestbuf, Stream);
         if (jcr->cached_attribute) {
            ar->Digest = digestbuf;
            ar->DigestType = type;
            Dmsg2(400, "Cached attr with digest. Stream=%d fname=%s\n",
                  ar->Stream, ar->fname);

            /* Update BaseFile table */
            if (!db_create_attributes_record(jcr, jcr->db, ar)) {
               Jmsg1(jcr, M_FATAL, 0, _("attribute create error. %s"),
                        db_strerror(jcr->db));
            }
            jcr->cached_attribute = false;
         } else {
            if (!db_add_digest_to_file_record(jcr, jcr->db, ar->FileId, digestbuf, type)) {
               Jmsg(jcr, M_ERROR, 0, _("Catalog error updating file digest. %s"),
                  db_strerror(jcr->db));
            }
         }
      }
   }
}

/*
 * Update File Attributes in the catalog with data
 *  sent by the Storage daemon.
 */
void catalog_update(JCR *jcr, BSOCK *bs)
{
   if (!jcr->pool->catalog_files) {
      return;                         /* user disabled cataloging */
   }
   if (jcr->is_job_canceled()) {
      goto bail_out;
   }
   if (!jcr->db) {
      POOLMEM *omsg = get_memory(bs->msglen+1);
      pm_strcpy(omsg, bs->msg);
      bs->fsend(_("1994 Invalid Catalog Update: %s"), omsg);
      Jmsg1(jcr, M_FATAL, 0, _("Invalid Catalog Update; DB not open: %s"), omsg);
      free_memory(omsg);
      goto bail_out;
   }
   update_attribute(jcr, bs->msg, bs->msglen);

bail_out:
   if (jcr->is_job_canceled()) {
      jcr->cached_attribute = false;
      cancel_storage_daemon_job(jcr);
   }
}

/*
 * Update File Attributes in the catalog with data read from
 * the storage daemon spool file. We receive the filename and
 * we try to read it.
 */
bool despool_attributes_from_file(JCR *jcr, const char *file)
{
   bool ret=false;
   int32_t pktsiz;
   size_t nbytes;
   ssize_t size = 0;
   int32_t msglen;                    /* message length */
   POOLMEM *msg = get_pool_memory(PM_MESSAGE);
   FILE *spool_fd=NULL;

   Dmsg1(100, "Begin despool_attributes_from_file\n", file);

   if (jcr->is_job_canceled() || !jcr->pool->catalog_files || !jcr->db) {
      goto bail_out;                  /* user disabled cataloging */
   }

   spool_fd = fopen(file, "rb");
   if (!spool_fd) {
      Dmsg0(100, "cancel despool_attributes_from_file\n");
      /* send an error message */
      goto bail_out;
   }
#if defined(HAVE_POSIX_FADVISE) && defined(POSIX_FADV_WILLNEED)
   posix_fadvise(fileno(spool_fd), 0, 0, POSIX_FADV_WILLNEED);
#endif

   while (fread((char *)&pktsiz, 1, sizeof(int32_t), spool_fd) ==
          sizeof(int32_t)) {
      size += sizeof(int32_t);
      msglen = ntohl(pktsiz);
      if (msglen > 0) {
         if (msglen > (int32_t) sizeof_pool_memory(msg)) {
            msg = realloc_pool_memory(msg, msglen + 1);
         }
         nbytes = fread(msg, 1, msglen, spool_fd);
         if (nbytes != (size_t) msglen) {
            berrno be;
            Dmsg2(400, "nbytes=%d msglen=%d\n", nbytes, msglen);
            Qmsg1(jcr, M_FATAL, 0, _("fread attr spool error. ERR=%s\n"),
                  be.bstrerror());
            goto bail_out;
         }
         size += nbytes;
      }
      if (!jcr->is_job_canceled()) {
         update_attribute(jcr, msg, msglen);
         if (jcr->is_job_canceled() || (jcr->wjcr && jcr->wjcr->is_job_canceled())) {
            goto bail_out;
         }
      }
   }
   if (ferror(spool_fd)) {
      berrno be;
      Qmsg1(jcr, M_FATAL, 0, _("fread attr spool error. ERR=%s\n"),
            be.bstrerror());
      Dmsg1(050, "fread attr spool error. ERR=%s\n", be.bstrerror());
      goto bail_out;
   }
   ret = true;

bail_out:
   if (spool_fd) {
      fclose(spool_fd);
   }

   if (jcr->is_job_canceled()) {
      jcr->cached_attribute = false;
      cancel_storage_daemon_job(jcr);
   }

   free_pool_memory(msg);
   Dmsg1(100, "End despool_attributes_from_file ret=%i\n", ret);
   return ret;
}
