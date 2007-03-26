/*
 *
 *   Bacula Director -- Automatic Pruning
 *      Applies retention periods
 *
 *     Kern Sibbald, May MMII
 *
 *   Version $Id$
 */
/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2002-2006 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from
   many others, a complete list can be found in the file AUTHORS.
   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version two of the GNU General Public
   License as published by the Free Software Foundation plus additions
   that are listed in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

   Bacula® is a registered trademark of John Walker.
   The licensor of Bacula is the Free Software Foundation Europe
   (FSFE), Fiduciary Program, Sumatrastrasse 25, 8006 Zürich,
   Switzerland, email:ftf@fsfeurope.org.
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
   bool pruned;

   if (!jcr->client) {                /* temp -- remove me */
      return;
   }

   ua = new_ua_context(jcr);

   client = jcr->client;

   if (jcr->job->PruneJobs || jcr->client->AutoPrune) {
      Jmsg(jcr, M_INFO, 0, _("Begin pruning Jobs.\n"));
      prune_jobs(ua, client, jcr->JobType);
      pruned = true;
   } else {
      pruned = false;
   }

   if (jcr->job->PruneFiles || jcr->client->AutoPrune) {
      Jmsg(jcr, M_INFO, 0, _("Begin pruning Files.\n"));
      prune_files(ua, client);
      pruned = true;
   }
   if (pruned) {
      Jmsg(jcr, M_INFO, 0, _("End auto prune.\n\n"));
   }

   free_ua_context(ua);
   return;
}

/*
 * Prune at least on Volume in current Pool. This is called from
 *   catreq.c when the Storage daemon is asking for another
 *   volume and no appendable volumes are available.
 *
 *  Return: false if nothing pruned
 *          true if pruned, and mr is set to pruned volume
 */
bool prune_volumes(JCR *jcr, MEDIA_DBR *mr) 
{
   int count;
   int i;
   uint32_t *ids = NULL;
   int num_ids = 0;
   struct del_ctx del;
   UAContext *ua;
   bool ok = false;

   Dmsg1(050, "Prune volumes PoolId=%d\n", jcr->jr.PoolId);
   if (!jcr->job->PruneVolumes && !jcr->pool->AutoPrune) {
      Dmsg0(100, "AutoPrune not set in Pool.\n");
      return 0;
   }
   memset(&del, 0, sizeof(del));
   del.max_ids = 1000;
   del.JobId = (JobId_t *)malloc(sizeof(JobId_t) * del.max_ids);

   ua = new_ua_context(jcr);

   db_lock(jcr->db);

   /* Get the List of all media ids in the current Pool */
   if (!db_get_media_ids(jcr, jcr->db, mr, &num_ids, &ids)) {
      Jmsg(jcr, M_ERROR, 0, "%s", db_strerror(jcr->db));
      goto bail_out;
   }

   /* Visit each Volume and Prune it */
   for (i=0; i<num_ids; i++) {
      MEDIA_DBR lmr;
      memset(&lmr, 0, sizeof(lmr));
      lmr.MediaId = ids[i];
      Dmsg1(150, "Get record MediaId=%d\n", (int)lmr.MediaId);
      if (!db_get_media_record(jcr, jcr->db, &lmr)) {
         Jmsg(jcr, M_ERROR, 0, "%s", db_strerror(jcr->db));
         continue;
      }
      /* Prune only Volumes from current Pool */
      if (mr->PoolId != lmr.PoolId) {
         continue;
      }
      /* Don't prune archived volumes */
      if (lmr.Enabled == 2) {
         continue;
      }
      /* Prune only Volumes with status "Full", or "Used" */
      if (strcmp(lmr.VolStatus, "Full")   == 0 ||
          strcmp(lmr.VolStatus, "Used")   == 0) {
         Dmsg2(050, "Add prune list MediaId=%d Volume %s\n", (int)lmr.MediaId, lmr.VolumeName);
         count = get_prune_list_for_volume(ua, &lmr, &del);
         Dmsg1(050, "Num pruned = %d\n", count);
         if (count != 0) {
            purge_job_list_from_catalog(ua, del);
            del.num_ids = 0;             /* reset count */
         }
         ok = is_volume_purged(ua, &lmr);
         if (ok) {
            Dmsg2(050, "Vol=%s MediaId=%d purged.\n", lmr.VolumeName, (int)lmr.MediaId);
            mr = &lmr;             /* struct copy */
            break;
         }
      }
   }

bail_out:
   db_unlock(jcr->db);
   free_ua_context(ua);
   if (ids) {
      free(ids);
   }
   if (del.JobId) {
      free(del.JobId);
   }
   return ok;
}
