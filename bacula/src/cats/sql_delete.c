/*
 * Bacula Catalog Database Delete record interface routines
 * 
 *    Kern Sibbald, December 2000
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

/* *****FIXME**** fix fixed length of select_cmd[] and insert_cmd[] */

/* The following is necessary so that we do not include
 * the dummy external definition of DB.
 */
#define __SQL_C 		      /* indicate that this is sql.c */

#include "bacula.h"
#include "cats.h"


#if    HAVE_MYSQL || HAVE_SQLITE
/* -----------------------------------------------------------------------
 *
 *   Generic Routines (or almost generic)
 *
 * -----------------------------------------------------------------------
 */

/* Imported subroutines */
extern void print_dashes(B_DB *mdb);
extern void print_result(B_DB *mdb);
extern int QueryDB(char *file, int line, B_DB *db, char *select_cmd);
extern int DeleteDB(char *file, int line, B_DB *db, char *delete_cmd);
       
/*
 * Delete Pool record, must also delete all associated
 *  Media records.
 *
 *  Returns: 0 on error
 *	     1 on success
 *	     PoolId = number of Pools deleted (should be 1)
 *	     NumVols = number of Media records deleted
 */
int
db_delete_pool_record(B_DB *mdb, POOL_DBR *pr)
{
   SQL_ROW row;

   P(mdb->mutex);
   Mmsg(&mdb->cmd, "SELECT PoolId FROM Pool WHERE Name=\"%s\"", pr->Name);
   Dmsg1(10, "selectpool: %s\n", mdb->cmd);

   pr->PoolId = pr->NumVols = 0;

   if (QUERY_DB(mdb, mdb->cmd)) {

      mdb->num_rows = sql_num_rows(mdb);
   
      if (mdb->num_rows == 0) {
         Mmsg(&mdb->errmsg, _("No pool record %s exists\n"), pr->Name);
	 sql_free_result(mdb);
	 V(mdb->mutex);
	 return 0;
      } else if (mdb->num_rows != 1) {
         Mmsg(&mdb->errmsg, _("Expecting one pool record, got %d\n"), mdb->num_rows);
	 sql_free_result(mdb);
	 V(mdb->mutex);
	 return 0;
      }
      if ((row = sql_fetch_row(mdb)) == NULL) {
	 V(mdb->mutex);
         Emsg1(M_ABORT, 0, _("Error fetching row %s\n"), sql_strerror(mdb));
      }
      pr->PoolId = atoi(row[0]);
      sql_free_result(mdb);
   }

   /* Delete Media owned by this pool */
   Mmsg(&mdb->cmd,
"DELETE FROM Media WHERE Media.PoolId = %d", pr->PoolId);

   pr->NumVols = DELETE_DB(mdb, mdb->cmd);
   Dmsg1(200, "Deleted %d Media records\n", pr->NumVols);

   /* Delete Pool */
   Mmsg(&mdb->cmd,
"DELETE FROM Pool WHERE Pool.PoolId = %d", pr->PoolId);
   pr->PoolId = DELETE_DB(mdb, mdb->cmd);
   Dmsg1(200, "Deleted %d Pool records\n", pr->PoolId);

   V(mdb->mutex);
   return 1;
}


/* Delete Media record */
int db_delete_media_record(B_DB *mdb, MEDIA_DBR *mr)
{

   P(mdb->mutex);
   if (mr->MediaId == 0) {
      Mmsg(&mdb->cmd, "DELETE FROM Media WHERE VolumeName=\"%s\"", 
	   mr->VolumeName);
   } else {
      Mmsg(&mdb->cmd, "DELETE FROM Media WHERE MediaId=%d", 
	   mr->MediaId);
   }

   mr->MediaId = DELETE_DB(mdb, mdb->cmd);

   V(mdb->mutex);
   return 1;
}

#endif /* HAVE_MYSQL || HAVE_SQLITE */
