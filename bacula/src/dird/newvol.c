/*
 *
 *   Bacula Director -- newvol.c -- creates new Volumes in
 *    catalog Media table from the LabelFormat specification.
 *
 *     Kern Sibbald, May MMI
 *
 *    This routine runs as a thread and must be thread reentrant.
 *
 *  Basic tasks done here:
 *	If possible create a new Media entry
 *
 *   Version $Id$
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
#include "dird.h"

/*
 * Really crude automatic Volume name creation using
 *  LabelFormat. We assume that if this routine is being
 *  called the Volume will be labeled, so we set the LabelDate.
 */
int newVolume(JCR *jcr, MEDIA_DBR *mr)
{
   POOL_DBR pr;
   char name[MAXSTRING];
   char num[20];

   memset(&pr, 0, sizeof(pr));

   /* See if we can create a new Volume */
   db_lock(jcr->db);
   pr.PoolId = jcr->PoolId;
   if (db_get_pool_record(jcr, jcr->db, &pr) && pr.LabelFormat[0] &&
       pr.LabelFormat[0] != '*') {
      if (pr.MaxVols == 0 || pr.NumVols < pr.MaxVols) {
	 set_pool_dbr_defaults_in_media_dbr(mr, &pr);
	 mr->LabelDate = time(NULL);
	 bstrncpy(mr->MediaType, jcr->store->media_type, sizeof(mr->MediaType));
	 bstrncpy(name, pr.LabelFormat, sizeof(name));
	 if (!is_volume_name_legal(NULL, name)) {
            Jmsg(jcr, M_ERROR, 0, _("Illegal character in Label Format\n"));
	    goto bail_out;
	 }
	 /* See if volume already exists */
	 mr->VolumeName[0] = 0;
	 for (int i=pr.NumVols+1; i<(int)pr.NumVols+11; i++) {
	    MEDIA_DBR tmr;
	    memset(&tmr, 0, sizeof(tmr));
            sprintf(num, "%04d", i);
	    bstrncpy(tmr.VolumeName, name, sizeof(tmr.VolumeName));
	    bstrncat(tmr.VolumeName, num, sizeof(tmr.VolumeName));
	    if (db_get_media_record(jcr, jcr->db, &tmr)) {
	       Jmsg(jcr, M_WARNING, 0, 
_("Wanted to create Volume \"%s\", but it already exists. Trying again.\n"), 
		    tmr.VolumeName);
	       continue;
	    }
	    bstrncpy(mr->VolumeName, name, sizeof(mr->VolumeName));
	    bstrncat(mr->VolumeName, num, sizeof(mr->VolumeName));
	    break;		      /* Got good name */
	 }
	 if (mr->VolumeName[0] == 0) {
            Jmsg(jcr, M_ERROR, 0, _("Too many failures. Giving up creating Volume.\n"));
	    goto bail_out;
	 }
	 pr.NumVols++;
	 if (db_create_media_record(jcr, jcr->db, mr) &&
	    db_update_pool_record(jcr, jcr->db, &pr)) {
	    db_unlock(jcr->db);
            Jmsg(jcr, M_INFO, 0, _("Created new Volume \"%s\" in catalog.\n"), mr->VolumeName);
            Dmsg1(90, "Created new Volume=%s\n", mr->VolumeName);
	    return 1;
	 } else {
            Jmsg(jcr, M_ERROR, 0, "%s", db_strerror(jcr->db));
	 }
      }
   }
bail_out:
   db_unlock(jcr->db);
   return 0;   
}
