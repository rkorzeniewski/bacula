/*
 *
 *   Bacula Director -- next_vol -- handles finding the next
 *    volume for append.  Split out of catreq.c August MMIII
 *    catalog request from the Storage daemon.

 *     Kern Sibbald, March MMI
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
 *  Items needed:
 *   jcr->PoolId
 *   jcr->store
 *   jcr->db
 *   jcr->pool
 *   MEDIA_DBR mr (zeroed out)
 *   create -- whether or not to create a new volume
 */
int find_next_volume_for_append(JCR *jcr, MEDIA_DBR *mr, int create)
{
   int ok, retry = 0;

   mr->PoolId = jcr->PoolId;
   bstrncpy(mr->MediaType, jcr->store->media_type, sizeof(mr->MediaType));
   Dmsg2(120, "CatReq FindMedia: Id=%d, MediaType=%s\n", mr->PoolId, mr->MediaType);
   /*
    * Find the Next Volume for Append
    */
   db_lock(jcr->db);
   for ( ;; ) {
      bstrncpy(mr->VolStatus, "Append", sizeof(mr->VolStatus));  /* want only appendable volumes */
      ok = db_find_next_volume(jcr, jcr->db, 1, mr);  
      Dmsg2(100, "catreq after find_next_vol ok=%d FW=%d\n", ok, mr->FirstWritten);
      if (!ok) {
	 /* Well, try finding recycled volumes */
	 ok = find_recycled_volume(jcr, mr);
         Dmsg2(100, "find_recycled_volume %d FW=%d\n", ok, mr->FirstWritten);
	 if (!ok) {
	    prune_volumes(jcr);  
	    ok = recycle_oldest_purged_volume(jcr, mr);
            Dmsg2(200, "find_recycled_volume2 %d FW=%d\n", ok, mr->FirstWritten);
	    if (!ok && create) {
	       /* See if we can create a new Volume */
	       ok = newVolume(jcr, mr);
	    }
	 }

	 if (!ok && (jcr->pool->purge_oldest_volume ||
		     jcr->pool->recycle_oldest_volume)) {
            Dmsg2(200, "No next volume found. PurgeOldest=%d\n RecyleOldest=%d",
		jcr->pool->purge_oldest_volume, jcr->pool->recycle_oldest_volume);
	    /* Find oldest volume to recycle */
	    ok = db_find_next_volume(jcr, jcr->db, -1, mr);
            Dmsg1(400, "Find oldest=%d\n", ok);
	    if (ok) {
	       UAContext *ua;
               Dmsg0(400, "Try purge.\n");
	       /* Try to purge oldest volume */
	       ua = new_ua_context(jcr);
	       if (jcr->pool->purge_oldest_volume) {
                  Jmsg(jcr, M_INFO, 0, _("Purging oldest volume \"%s\"\n"), mr->VolumeName);
		  ok = purge_jobs_from_volume(ua, mr);
	       } else {
                  Jmsg(jcr, M_INFO, 0, _("Pruning oldest volume \"%s\"\n"), mr->VolumeName);
		  ok = prune_volume(ua, mr);
	       }
	       free_ua_context(ua);
	       if (ok) {
		  ok = recycle_volume(jcr, mr);
                  Dmsg1(400, "Recycle after purge oldest=%d\n", ok);
	       }
	    }
	 }
      }
      /* Check if use duration applies, then if it has expired */
      Dmsg2(100, "VolJobs=%d FirstWritten=%d\n", mr->VolJobs, mr->FirstWritten);
      if (ok && mr->VolJobs > 0 && mr->VolUseDuration > 0 && 
           strcmp(mr->VolStatus, "Append") == 0) {
	 utime_t now = time(NULL);
	 if (mr->VolUseDuration <= (now - mr->FirstWritten)) {
	    ok = FALSE;
            Dmsg4(100, "Duration=%d now=%d start=%d now-start=%d\n",
	       (int)mr->VolUseDuration, (int)now, (int)mr->FirstWritten, 
	       (int)(now-mr->FirstWritten));
            Jmsg(jcr, M_INFO, 0, _("Max configured use duration exceeded. "       
               "Marking Volume \"%s\" as Used.\n"), mr->VolumeName);
            strcpy(mr->VolStatus, "Used");  /* yes, mark as used */
	    if (!db_update_media_record(jcr, jcr->db, mr)) {
               Jmsg(jcr, M_ERROR, 0, _("Catalog error updating volume \"%s\". ERR=%s"),
		    mr->VolumeName, db_strerror(jcr->db));
	    }		
	    if (retry++ < 200) {	    /* sanity check */
	       continue;		    /* try again from the top */
	    } else {
	       Jmsg(jcr, M_ERROR, 0, _(
"We seem to be looping trying to find the next volume. I give up.\n"));
	    }
	 }
      }
      break;
   } /* end for loop */
   db_unlock(jcr->db);
   return ok;
}
