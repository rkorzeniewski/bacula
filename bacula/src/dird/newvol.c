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
 * Really crude automatic Volume name creation using
 *  LabelFormat
 */
int newVolume(JCR *jcr)
{
   MEDIA_DBR mr; 
   POOL_DBR pr;
   char name[MAXSTRING];

   memset(&mr, 0, sizeof(mr));
   memset(&pr, 0, sizeof(pr));

   /* See if we can create a new Volume */
   pr.PoolId = jcr->PoolId;
   if (db_get_pool_record(jcr->db, &pr) && pr.LabelFormat[0] &&
       pr.LabelFormat[0] != '*') {
      if (pr.MaxVols == 0 || pr.NumVols < pr.MaxVols) {
	 memset(&mr, 0, sizeof(mr));
	 mr.PoolId = jcr->PoolId;
	 strcpy(mr.MediaType, jcr->store->media_type);
	 strcpy(name, pr.LabelFormat);	 
         strcat(name, "%04d");
	 sprintf(mr.VolumeName, name, ++pr.NumVols);
         strcpy(mr.VolStatus, "Append");
	 mr.Recycle = pr.Recycle;
	 mr.VolRetention = pr.VolRetention;
	 if (db_create_media_record(jcr->db, &mr) &&
	    db_update_pool_record(jcr->db, &pr) == 1) {
            Dmsg1(90, "Created new Volume=%s\n", mr.VolumeName);
	    return 1;
	 } else {
            Jmsg(jcr, M_ERROR, 0, "%s", db_strerror(jcr->db));
	 }
      }
   }
   return 0;   
}
