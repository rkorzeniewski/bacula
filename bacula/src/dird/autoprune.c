/*
 *
 *   Bacula Director -- Automatic Pruning 
 *	Applies retention periods
 *
 *     Kern Sibbald, May MMII
 *
 *   Version $Id$
 */

/*
   Copyright (C) 2002 Kern Sibbald and John Walker

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
#include "ua.h"

/* Forward referenced functions */

void create_ua_context(JCR *jcr, UAContext *ua)
{
   memset(ua, 0, sizeof(UAContext));
   ua->jcr = jcr;
   ua->db = jcr->db;
   ua->cmd = get_pool_memory(PM_FNAME);
   ua->args = get_pool_memory(PM_FNAME);
   ua->verbose = 1;
}

void free_ua_context(UAContext *ua)
{
   if (ua->cmd) {
      free_pool_memory(ua->cmd);
   }
   if (ua->args) {
      free_pool_memory(ua->args);
   }
}

/*
 * Auto Prune Jobs and Files
 *   Volumes are done separately
 */
int do_autoprune(JCR *jcr)
{
   UAContext ua;
   CLIENT *client;
   int pruned;

   if (!jcr->client) {		      /* temp -- remove me */
      return 1;
   }

   create_ua_context(jcr, &ua);

   client = jcr->client;

   if (jcr->job->PruneJobs || jcr->client->AutoPrune) {
      Jmsg(jcr, M_INFO, 0, _("Begin pruning Jobs.\n"));
      prune_jobs(&ua, client);
      pruned = TRUE;
   } else {
      pruned = FALSE;
   }
  
   if (jcr->job->PruneFiles || jcr->client->AutoPrune) {
      Jmsg(jcr, M_INFO, 0, _("Begin pruning Files.\n"));
      prune_files(&ua, client);
      pruned = TRUE;
   }
   if (pruned) {
      Jmsg(jcr, M_INFO, 0, _("End auto prune.\n\n"));
   }

   free_ua_context(&ua);
   return 1;	
}

/*
 * Prune all volumes in current Pool.
 *
 *  Return 0: on error
 *	   number of Volumes Purged
 */
int prune_volumes(JCR *jcr)
{
   int stat = 0;
   int i;
   uint32_t *ids = NULL;
   int num_ids = 0;
   MEDIA_DBR mr;
   POOL_DBR pr;
   UAContext ua;

   if (!jcr->job->PruneVolumes && !jcr->pool->AutoPrune) {
      Dmsg0(200, "AutoPrune not set in Pool.\n");
      return stat;
   }
   memset(&mr, 0, sizeof(mr));
   memset(&pr, 0, sizeof(pr));
   create_ua_context(jcr, &ua);

   db_lock(jcr->db);

   pr.PoolId = jcr->PoolId;
   if (!db_get_pool_record(jcr->db, &pr) || !db_get_media_ids(jcr->db, &num_ids, &ids)) {
      Jmsg(jcr, M_ERROR, 0, "%s", db_strerror(jcr->db));
      goto rtn;
   }


   for (i=0; i<num_ids; i++) {
      mr.MediaId = ids[i];
      if (!db_get_media_record(jcr->db, &mr)) {
         Jmsg(jcr, M_ERROR, 0, "%s", db_strerror(jcr->db));
	 continue;
      }
      /* Prune only Volumes from current Pool */
      if (pr.PoolId != mr.PoolId) {
	 continue;
      }
      /* Prune only Volumes with status "Full" */
      if (strcmp(mr.VolStatus, "Full") != 0) {
	 continue;
      }
      Dmsg1(200, "Prune Volume %s\n", mr.VolumeName);
      stat += prune_volume(&ua, &pr, &mr); 
      Dmsg1(200, "Num pruned = %d\n", stat);
   }   
rtn:
   db_unlock(jcr->db);
   free_ua_context(&ua);
   if (ids) {
      free(ids);
   }
   return stat;
}
