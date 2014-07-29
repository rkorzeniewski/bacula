/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2002-2014 Free Software Foundation Europe e.V.

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
 *   Bacula Director -- Automatic Pruning
 *      Applies retention periods
 *
 *     Kern Sibbald, May MMII
 *
 *   Version $Id$
 */

#include "bacula.h"
#include "dird.h"
#include "ua.h"

/* Forward referenced functions */


/*
 * Auto Prune Jobs and Files. This is called at the end of every
 *   Job.  We do not prune volumes here.
 */
void do_autoprune(JCR *jcr)
{
   UAContext *ua;
   CLIENT *client;
   POOL *pool;
   bool pruned;

   if (!jcr->client) {                /* temp -- remove me */
      return;
   }

   ua = new_ua_context(jcr);
   client = jcr->client;
   pool = jcr->pool;

   if (jcr->job->PruneJobs || jcr->client->AutoPrune) {
      prune_jobs(ua, client, pool, jcr->getJobType());
      pruned = true;
   } else {
      pruned = false;
   }

   if (jcr->job->PruneFiles || jcr->client->AutoPrune) {
      prune_files(ua, client, pool);
      pruned = true;
   }
   if (pruned) {
      Jmsg(jcr, M_INFO, 0, _("End auto prune.\n\n"));
   }
   free_ua_context(ua);
   return;
}

/*
 * Prune at least one Volume in current Pool. This is called from
 *   catreq.c => next_vol.c when the Storage daemon is asking for another
 *   volume and no appendable volumes are available.
 *
 */
void prune_volumes(JCR *jcr, bool InChanger, MEDIA_DBR *mr,
        STORE *store)
{
   int count;
   int i;
   dbid_list ids;
   struct del_ctx prune_list;
   POOL_MEM query(PM_MESSAGE), changer(PM_MESSAGE);
   UAContext *ua;
   char ed1[50], ed2[100], ed3[50];

   POOL_DBR spr;

   Dmsg1(100, "Prune volumes PoolId=%d\n", jcr->jr.PoolId);
   if (!jcr->job->PruneVolumes && !jcr->pool->AutoPrune) {
      Dmsg0(100, "AutoPrune not set in Pool.\n");
      return;
   }

   memset(&prune_list, 0, sizeof(prune_list));
   prune_list.max_ids = 10000;
   prune_list.JobId = (JobId_t *)malloc(sizeof(JobId_t) * prune_list.max_ids);

   ua = new_ua_context(jcr);
   db_lock(jcr->db);

   /* Edit PoolId */
   edit_int64(mr->PoolId, ed1);
   /*
    * Get Pool record for Scratch Pool
    */
   memset(&spr, 0, sizeof(spr));
   bstrncpy(spr.Name, "Scratch", sizeof(spr.Name));
   if (db_get_pool_record(jcr, jcr->db, &spr)) {
      edit_int64(spr.PoolId, ed2);
      bstrncat(ed2, ",", sizeof(ed2));
   } else {
      ed2[0] = 0;
   }

   if (mr->ScratchPoolId) {
      edit_int64(mr->ScratchPoolId, ed3);
      bstrncat(ed2, ed3, sizeof(ed2));
      bstrncat(ed2, ",", sizeof(ed2));
   }

   Dmsg1(100, "Scratch pool(s)=%s\n", ed2);
   /*
    * ed2 ends up with scratch poolid and current poolid or
    *   just current poolid if there is no scratch pool
    */
   bstrncat(ed2, ed1, sizeof(ed2));

   /*
    * Get the List of all media ids in the current Pool or whose
    *  RecyclePoolId is the current pool or the scratch pool
    */
   const char *select = "SELECT DISTINCT MediaId,LastWritten FROM Media WHERE "
        "(PoolId=%s OR RecyclePoolId IN (%s)) AND MediaType='%s' %s"
        "ORDER BY LastWritten ASC,MediaId";

   set_storageid_in_mr(store, mr);
   if (InChanger) {
      Mmsg(changer, "AND InChanger=1 AND StorageId IN (%s) ", edit_int64(mr->StorageId, ed3));
   }

   Mmsg(query, select, ed1, ed2, mr->MediaType, changer.c_str());

   Dmsg1(100, "query=%s\n", query.c_str());
   if (!db_get_query_dbids(ua->jcr, ua->db, query, ids)) {
      Jmsg(jcr, M_ERROR, 0, "%s", db_strerror(jcr->db));
      goto bail_out;
   }

   Dmsg1(100, "Volume prune num_ids=%d\n", ids.num_ids);

   /* Visit each Volume and Prune it until we find one that is purged */
   for (i=0; i<ids.num_ids; i++) {
      MEDIA_DBR lmr;
      lmr.MediaId = ids.DBId[i];
      Dmsg1(100, "Get record MediaId=%d\n", (int)lmr.MediaId);
      if (!db_get_media_record(jcr, jcr->db, &lmr)) {
         Jmsg(jcr, M_ERROR, 0, "%s", db_strerror(jcr->db));
         continue;
      }
      Dmsg1(100, "Examine vol=%s\n", lmr.VolumeName);
      /* Don't prune archived volumes */
      if (lmr.Enabled == 2) {
         Dmsg1(100, "Vol=%s disabled\n", lmr.VolumeName);
         continue;
      }
      /* Prune only Volumes with status "Full", or "Used" */
      if (strcmp(lmr.VolStatus, "Full")   == 0 ||
          strcmp(lmr.VolStatus, "Used")   == 0) {
         Dmsg2(100, "Add prune list MediaId=%d Volume %s\n", (int)lmr.MediaId, lmr.VolumeName);
         count = get_prune_list_for_volume(ua, &lmr, &prune_list);
         Dmsg1(100, "Num pruned = %d\n", count);
         if (count != 0) {
            purge_job_list_from_catalog(ua, prune_list);
            prune_list.num_ids = 0;             /* reset count */
         }
         if (!is_volume_purged(ua, &lmr)) {
            Dmsg1(100, "Vol=%s not pruned\n", lmr.VolumeName);
            continue;
         }
         Dmsg1(100, "Vol=%s is purged\n", lmr.VolumeName);

         /*
          * Since we are also pruning the Scratch pool, continue
          *   until and check if this volume is available (InChanger + StorageId)
          * If not, just skip this volume and try the next one
          */
         if (InChanger) {
            if (!lmr.InChanger || (lmr.StorageId != mr->StorageId)) {
               Dmsg1(100, "Vol=%s not inchanger\n", lmr.VolumeName);
               continue;                  /* skip this volume, ie not loadable */
            }
         }

         if (!lmr.Recycle) {
            Dmsg1(100, "Vol=%s not recyclable\n", lmr.VolumeName);
            continue;
         }

         if (has_volume_expired(jcr, &lmr)) {
            Dmsg1(100, "Vol=%s has expired\n", lmr.VolumeName);
            continue;                     /* Volume not usable */
         }

         /*
          * If purged and not moved to another Pool,
          *   then we stop pruning and take this volume.
          */
         if (lmr.PoolId == mr->PoolId) {
            Dmsg2(100, "Got Vol=%s MediaId=%d purged.\n", lmr.VolumeName, (int)lmr.MediaId);
            mr->copy(&lmr);
            set_storageid_in_mr(store, mr);
            break;                        /* got a volume */
         }
      }
   }

bail_out:
   Dmsg0(100, "Leave prune volumes\n");
   db_unlock(jcr->db);
   free_ua_context(ua);
   if (prune_list.JobId) {
      free(prune_list.JobId);
   }
   return;
}
