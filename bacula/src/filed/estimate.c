/*
 *  Bacula File Daemon estimate.c		    
 *   Make and estimate of the number of files and size to be saved.
 *
 *    Kern Sibbald, September MMI
 *
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
#include "filed.h"

static int tally_file(FF_PKT *ff_pkt, void *pkt);

/* 
 * Find all the requested files and count them.
 */
int make_estimate(JCR *jcr)
{
   int stat;

   jcr->JobStatus = JS_Running;

   set_find_options(jcr->ff, jcr->incremental, jcr->mtime);

   stat = find_files(jcr->ff, tally_file, (void *)jcr);

   return stat;
}	   

/* 
 * Called here by find() for each file included.
 *
 *  *****FIXME*****   add FSMs File System Modules
 *
 */
static int tally_file(FF_PKT *ff_pkt, void *ijcr)
{
   JCR *jcr = (JCR *) ijcr;

   switch (ff_pkt->type) {
   case FT_LNKSAVED:		      /* Hard linked, file already saved */
      break;
   case FT_REGE:
   case FT_REG:
   case FT_LNK:
   case FT_DIR:
   case FT_SPEC:
      break;
   case FT_NOACCESS:
   case FT_NOFOLLOW:
   case FT_NOSTAT:
   case FT_DIRNOCHG:
   case FT_NOCHG:
   case FT_ISARCH:
   case FT_NORECURSE:
   case FT_NOFSCHG:
   case FT_NOOPEN:
   default:
      return 1;
   }

   if (ff_pkt->type != FT_LNKSAVED && S_ISREG(ff_pkt->statp.st_mode) && 
	 ff_pkt->statp.st_size > 0) {
      jcr->JobBytes += ff_pkt->statp.st_size;
   }

   jcr->JobFiles++;		     /* increment number of files sent */
   return 1;
}
