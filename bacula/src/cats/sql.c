/*
 * Bacula Catalog Database interface routines
 * 
 *     Almost generic set of SQL database interface routines
 *	(with a little more work)
 *
 *    Kern Sibbald, March 2000
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

/* The following is necessary so that we do not include
 * the dummy external definition of B_DB.
 */
#define __SQL_C 		      /* indicate that this is sql.c */

#include "bacula.h"
#include "cats.h"

#if    HAVE_MYSQL | HAVE_SQLITE

/* Forward referenced subroutines */
void print_dashes(B_DB *mdb);
void print_result(B_DB *mdb);


/* NOTE!!! The following routines expect that the
 *  calling subroutine sets and clears the mutex
 */

/* Utility routine for queries */
int
QueryDB(char *file, int line, B_DB *mdb, char *cmd)
{
   if (sql_query(mdb, cmd)) {
      m_msg(file, line, &mdb->errmsg, _("query %s failed:\n%s\n"), cmd, sql_strerror(mdb));
      e_msg(file, line, M_FATAL, 0, mdb->errmsg);
      return 0;
   }
   mdb->result = sql_store_result(mdb);
   
   return mdb->result != NULL;
}

/* 
 * Utility routine to do inserts   
 * Returns: 0 on failure      
 *	    1 on success
 */
int
InsertDB(char *file, int line, B_DB *mdb, char *cmd)
{
   if (sql_query(mdb, cmd)) {
      m_msg(file, line, &mdb->errmsg,  _("insert %s failed:\n%s\n"), cmd, sql_strerror(mdb));
      e_msg(file, line, M_FATAL, 0, mdb->errmsg);
      return 0;
   }
   if (mdb->have_insert_id) {
      mdb->num_rows = sql_affected_rows(mdb);
   } else {
      mdb->num_rows = 1;
   }
   if (mdb->num_rows != 1) {
      char ed1[30];
      m_msg(file, line, &mdb->errmsg, _("Insertion problem: affect_rows=%s\n"), 
	 edit_uint64(mdb->num_rows, ed1));
      e_msg(file, line, M_FATAL, 0, mdb->errmsg);  /* ***FIXME*** remove me */
      return 0;
   }
   return 1;
}

/* Utility routine for updates.
 *  Returns: 0 on failure
 *	     1 on success  
 */
int
UpdateDB(char *file, int line, B_DB *mdb, char *cmd)
{

   if (sql_query(mdb, cmd)) {
      m_msg(file, line, &mdb->errmsg, _("update %s failed:\n%s\n"), cmd, sql_strerror(mdb));
      e_msg(file, line, M_ERROR, 0, mdb->errmsg);
      e_msg(file, line, M_ERROR, 0, "%s\n", cmd);
      return 0;
   }
   mdb->num_rows = sql_affected_rows(mdb);
   if (mdb->num_rows != 1) {
      char ed1[30];
      m_msg(file, line, &mdb->errmsg, _("Update problem: affect_rows=%s\n"), 
	 edit_uint64(mdb->num_rows, ed1));
      e_msg(file, line, M_ERROR, 0, mdb->errmsg);
      e_msg(file, line, M_ERROR, 0, "%s\n", cmd);
      return 0;
   }
   return 1;
}

/* Utility routine for deletes	 
 *
 * Returns: -1 on error
 *	     n number of rows affected
 */
int
DeleteDB(char *file, int line, B_DB *mdb, char *cmd)
{

   if (sql_query(mdb, cmd)) {
      m_msg(file, line, &mdb->errmsg, _("delete %s failed:\n%s\n"), cmd, sql_strerror(mdb));
      e_msg(file, line, M_ERROR, 0, mdb->errmsg);
      return -1;
   }
   return sql_affected_rows(mdb);
}


/*
 * Get record max. Query is already in mdb->cmd
 *  No locking done
 *
 * Returns: -1 on failure
 *	    count on success
 */
int get_sql_record_max(B_DB *mdb)
{
   SQL_ROW row;
   int stat = 0;

   if (QUERY_DB(mdb, mdb->cmd)) {
      if ((row = sql_fetch_row(mdb)) == NULL) {
         Mmsg1(&mdb->errmsg, _("error fetching row: %s\n"), sql_strerror(mdb));
	 stat = -1;
      } else {
	 stat = atoi(row[0]);
      }
      sql_free_result(mdb);
   } else {
      Mmsg1(&mdb->errmsg, _("error fetching row: %s\n"), sql_strerror(mdb));
      stat = -1;
   }
   return stat;
}

char *db_strerror(B_DB *mdb)
{
   return mdb->errmsg;
}

#endif /* HAVE_MYSQL | HAVE_SQLITE */
