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
static char Get_Vol_Info[] = "CatReq Job=%127s GetVolInfo VolName=%127s\n";

static char Update_media[] = "CatReq Job=%127s UpdateMedia VolName=%s\
 VolJobs=%d VolFiles=%d VolBlocks=%d VolBytes=%" lld " VolMounts=%d\
 VolErrors=%d VolWrites=%d VolMaxBytes=%" lld " EndTime=%d VolStatus=%10s\
 FirstIndex=%d LastIndex=%d StartFile=%d EndFile=%d \
 StartBlock=%d EndBlock=%d\n";

/* Responses  sent to Storage daemon */
static char OK_media[] = "1000 OK VolName=%s VolJobs=%d VolFiles=%d\
 VolBlocks=%d VolBytes=%" lld " VolMounts=%d VolErrors=%d VolWrites=%d\
 VolMaxBytes=%" lld " VolCapacityBytes=%" lld " VolStatus=%s\n";

static char OK_update[] = "1000 OK UpdateMedia\n";

/* static char FileAttributes[] = "UpdCat Job=%127s FileAttributes "; */


void catalog_request(JCR *jcr, BSOCK *bs, char *msg)
{
   MEDIA_DBR mr; 
   JOBMEDIA_DBR jm;
   char Job[MAX_NAME_LENGTH];
   int index, ok;
   char *omsg;

   memset(&mr, 0, sizeof(mr));
   memset(&jm, 0, sizeof(jm));

   /*
    * Request to find next appendable Volume for this Job
    */
   Dmsg1(20, "catreq %s", bs->msg);
   if (sscanf(bs->msg, Find_media, &Job, &index) == 2) {
      mr.PoolId = jcr->PoolId;
      strcpy(mr.MediaType, jcr->store->media_type);
      strcpy(mr.VolStatus, "Append");
      Dmsg3(120, "CatReq FindMedia: Id=%d, MediaType=%s, Status=%s\n",
	 mr.PoolId, mr.MediaType, mr.VolStatus);
      /*
       * Find the Volume
       */
      ok = FALSE;
      if (db_find_next_volume(jcr->db, index, &mr)) {
	 jcr->MediaId = mr.MediaId;
         Dmsg1(20, "Find_next_vol MediaId=%d\n", jcr->MediaId);
	 strcpy(jcr->VolumeName, mr.VolumeName);
	 ok = TRUE;
      } else {
	 /* Well, try finding recycled tapes */
	 ok = find_recycled_volume(jcr, &mr);
	 if (!ok) {
	    if (prune_volumes(jcr)) {
	       ok = recycle_a_volume(jcr, &mr);
	    }
	    if (!ok) {
	       /* See if we can create a new Volume */
	       ok = newVolume(jcr);
	    }
	 }
      }
      /*
       * Send Find Media response to Storage daemon 
       */
      if (ok) {
	 bash_spaces(mr.VolumeName);
	 bnet_fsend(bs, OK_media, mr.VolumeName, mr.VolJobs,
	    mr.VolFiles, mr.VolBlocks, mr.VolBytes, mr.VolMounts, mr.VolErrors,
	    mr.VolWrites, mr.VolMaxBytes, mr.VolCapacityBytes,
	    mr.VolStatus);
      } else {
         bnet_fsend(bs, "1999 No Media\n");
      }

   /* 
    * Request to find specific Volume information
    */
   } else if (sscanf(bs->msg, Get_Vol_Info, &Job, &mr.VolumeName) == 2) {
      Dmsg1(120, "CatReq GetVolInfo Vol=%s\n", mr.VolumeName);
      /*
       * Find the Volume
       */
      unbash_spaces(mr.VolumeName);
      if (db_get_media_record(jcr->db, &mr)) {
	 jcr->MediaId = mr.MediaId;
         Dmsg1(20, "VolumeInfo MediaId=%d\n", jcr->MediaId);
	 strcpy(jcr->VolumeName, mr.VolumeName);
	 /* 
	  * Make sure this volume is suitable for this job, i.e.
	  *  it is either Append or Recycle and Media Type matches.
	  */
	 if (mr.PoolId == jcr->PoolId && 
             (strcmp(mr.VolStatus, "Append") == 0 ||
              strcmp(mr.VolStatus, "Recycle") == 0) &&
	     strcmp(mr.MediaType, jcr->store->media_type) == 0) {
	    /*
	     * Send Find Media response to Storage daemon 
	     */
	    bash_spaces(mr.VolumeName);
	    bnet_fsend(bs, OK_media, mr.VolumeName, mr.VolJobs,
	       mr.VolFiles, mr.VolBlocks, mr.VolBytes, mr.VolMounts, mr.VolErrors,
	       mr.VolWrites, mr.VolMaxBytes, mr.VolCapacityBytes,
	       mr.VolStatus);
	 } else { 
            Dmsg4(000, "get_media_record PoolId=%d wanted %d, Status=%s, \
MediaType=%s\n", mr.PoolId, jcr->PoolId, mr.VolStatus, mr.MediaType);
	    /* Not suitable volume */
            bnet_fsend(bs, "1998 Volume not appropriate.\n");
	 }

      } else {
         bnet_fsend(bs, "1999 Volume Not Found.\n");
      }

   
   /*
    * Request to update Media record. Comes typically at the end
    *  of a Storage daemon Job Session
    */
   } else if (sscanf(bs->msg, Update_media, &Job, &mr.VolumeName, &mr.VolJobs,
      &mr.VolFiles, &mr.VolBlocks, &mr.VolBytes, &mr.VolMounts, &mr.VolErrors,
      &mr.VolWrites, &mr.VolMaxBytes, &mr.LastWritten, &mr.VolStatus, 
      &jm.FirstIndex, &jm.LastIndex, &jm.StartFile, &jm.EndFile,
      &jm.StartBlock, &jm.EndBlock) == 18) {
      /*     
       * Update Media Record
       */
      if ((mr.VolMaxBytes > 0 && mr.VolBytes >= mr.VolMaxBytes) ||
	  (mr.VolBytes > 0 && jcr->pool->use_volume_once)) {
         strcpy(mr.VolStatus, "Full");
      }

      jm.JobId = jcr->JobId;
      jm.MediaId = jcr->MediaId;
      /*
       * A single write means we just labeled the tape,
       * so no need to create a jobmedia record.
       * Otherwise, record the fact that this job used this Volume 
       */
      if (mr.VolWrites > 1) {
         Dmsg4(20, "create_jobmedia JobId=%d MediaId=%d FI=%d LI=%d\n",
	    jm.JobId, jm.MediaId, jm.FirstIndex, jm.LastIndex);
	 if(!db_create_jobmedia_record(jcr->db, &jm)) {
            Jmsg(jcr, M_ERROR, 0, _("Catalog error creating JobMedia record. %s"),
	       db_strerror(jcr->db));
            bnet_fsend(bs, "1991 Update JobMedia error\n");
	 } else {
            Dmsg0(20, "JobMedia record created\n");
	 }
      }

      Dmsg0(20, "db_update_media_record\n");
      if (db_update_media_record(jcr->db, &mr)) {
	 bnet_fsend(bs, OK_update);
         Dmsg0(90, "send OK\n");
      } else {
         Jmsg(jcr, M_ERROR, 0, _("Catalog error updating Media record. %s"),
	    db_strerror(jcr->db));
         bnet_fsend(bs, "1992 Update Media error\n");
         Dmsg0(90, "send error\n");
      }
   } else {
      omsg = (char *) get_memory(bs->msglen+1);
      strcpy(omsg, bs->msg);
      bnet_fsend(bs, "1990 Invalid Catalog Request: %s", omsg);    
      free_memory(omsg);
   }
   Dmsg1(20, ">CatReq response: %s", bs->msg);
   return;
}

/*
 * Update File Attributes in the catalog with data
 *  sent by the Storage daemon.
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

   if (Stream == STREAM_UNIX_ATTRIBUTES) {
      skip_nonspaces(&p);	      /* skip FileIndex */
      skip_spaces(&p);
      skip_nonspaces(&p);	      /* skip FileType */
      skip_spaces(&p);
      fname = p;
      len = strlen(fname);	  /* length before attributes */
      attr = &fname[len+1];

      Dmsg2(99, "dird<stored: stream=%d %s\n", Stream, fname);
      Dmsg1(99, "dird<stored: attr=%s\n", attr);
      ar.attr = attr; 
      ar.fname = fname;
      ar.FileIndex = FileIndex;
      ar.Stream = Stream;
      ar.link = NULL;
      ar.JobId = jcr->JobId;

      Dmsg2(11, "dird<filed: stream=%d %s\n", Stream, fname);
      Dmsg1(20, "dird<filed: attr=%s\n", attr);

      /* ***FIXME*** fix link field */
      if (!db_create_file_attributes_record(jcr->db, &ar)) {
         Jmsg1(jcr, M_FATAL, 0, _("Attribute create error. %s"), db_strerror(jcr->db));
      }
      /* Save values for MD5 update */
      jcr->FileId = ar.FileId;
      jcr->FileIndex = FileIndex;
   } else if (Stream == STREAM_MD5_SIGNATURE) {
      fname = p;
      if (jcr->FileIndex != FileIndex) {    
         Jmsg(jcr, M_WARNING, 0, "Got MD5 but not same block as attributes\n");
      } else {
	 /* Update MD5 signature in catalog */
	 char MD5buf[50];	    /* 24 bytes should be enough */
	 bin_to_base64(MD5buf, fname, 16);
         Dmsg2(90, "MD5len=%d MD5=%s\n", strlen(MD5buf), MD5buf);
	 if (!db_add_MD5_to_file_record(jcr->db, jcr->FileId, MD5buf)) {
            Jmsg(jcr, M_ERROR, 0, _("Catalog error updating MD5. %s"), 
	       db_strerror(jcr->db));
	 }
      }
   }
}
