/*
 * Bacula Catalog Database interface routines
 * 
 *     Almost generic set of SQL database interface routines
 *	(with a little more work)
 *
 *    Kern Sibbald, March 2000
 *
 *    Version $Id$
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

/*
 * Called here to retrieve an integer from the database
 */
static int int_handler(void *ctx, int num_fields, char **row)
{
   uint32_t *val = (uint32_t *)ctx;

   if (row[0]) {
      *val = atoi(row[0]);
   } else {
      *val = 0;
   }
   return 0;
}
       


/* NOTE!!! The following routines expect that the
 *  calling subroutine sets and clears the mutex
 */

/* Check that the tables conrrespond to the version we want */
int check_tables_version(B_DB *mdb)
{
   uint32_t version;
   char *query = "SELECT VersionId FROM Version";
  
   version = 0;
   db_sql_query(mdb, query, int_handler, (void *)&version);
   if (version != BDB_VERSION) {
      Mmsg(&mdb->errmsg, "Version error for database \"%s\". Wanted %d, got %d\n",
	 mdb->db_name, BDB_VERSION, version);
      Jmsg(mdb->jcr, M_FATAL, 0, mdb->errmsg);
      return 0;
   }
   return 1;
}

/* Utility routine for queries */
int
QueryDB(char *file, int line, B_DB *mdb, char *cmd)
{
   if (sql_query(mdb, cmd)) {
      m_msg(file, line, &mdb->errmsg, _("query %s failed:\n%s\n"), cmd, sql_strerror(mdb));
      j_msg(file, line, mdb->jcr, M_FATAL, 0, mdb->errmsg);
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
      j_msg(file, line, mdb->jcr, M_FATAL, 0, mdb->errmsg);
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
      return 0;
   }
   mdb->changes++;
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
      j_msg(file, line, mdb->jcr, M_ERROR, 0, mdb->errmsg);
      j_msg(file, line, mdb->jcr, M_ERROR, 0, "%s\n", cmd);
      return 0;
   }
   mdb->num_rows = sql_affected_rows(mdb);
   if (mdb->num_rows != 1) {
      char ed1[30];
      m_msg(file, line, &mdb->errmsg, _("Update problem: affect_rows=%s\n"), 
	 edit_uint64(mdb->num_rows, ed1));
      return 0;
   }
   mdb->changes++;
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
      j_msg(file, line, mdb->jcr, M_ERROR, 0, mdb->errmsg);
      return -1;
   }
   mdb->changes++;
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

void _db_lock(char *file, int line, B_DB *mdb)
{
   int errstat;
   if ((errstat=rwl_writelock(&mdb->lock)) != 0) {
      j_msg(file, line, mdb->jcr, M_ABORT, 0, "rwl_writelock failure. ERR=%s\n",
	   strerror(errstat));
   }
}    

void _db_unlock(char *file, int line, B_DB *mdb)
{
   int errstat;
   if ((errstat=rwl_writeunlock(&mdb->lock)) != 0) {
      j_msg(file, line, mdb->jcr, M_ABORT, 0, "rwl_writeunlock failure. ERR=%s\n",
	   strerror(errstat));
   }
}    

/*
 * Start a transaction. This groups inserts and makes things
 *  much more efficient. Usually started when inserting 
 *  file attributes.
 */
void db_start_transaction(B_DB *mdb)
{
#ifdef xAVE_SQLITE
   db_lock(mdb);
   /* Allow only 10,000 changes per transaction */
   if (mdb->transaction && mdb->changes > 10000) {
      db_end_transaction(mdb);
   }
   if (!mdb->transaction) {   
      my_sqlite_query(mdb, "BEGIN");  /* begin transaction */
      Dmsg0(400, "Start SQLite transaction\n");
      mdb->transaction = 1;
   }
   db_unlock(mdb);
#endif

}

void db_end_transaction(B_DB *mdb)
{
#ifdef xAVE_SQLITE
   db_lock(mdb);
   if (mdb->transaction) {
      my_sqlite_query(mdb, "COMMIT"); /* end transaction */
      mdb->transaction = 0;
      Dmsg1(400, "End SQLite transaction changes=%d\n", mdb->changes);
   }
   mdb->changes = 0;
   db_unlock(mdb);
#endif
}

void split_path_and_filename(B_DB *mdb, char *fname)
{
   char *p, *f;

   /* Find path without the filename.  
    * I.e. everything after the last / is a "filename".
    * OK, maybe it is a directory name, but we treat it like
    * a filename. If we don't find a / then the whole name
    * must be a path name (e.g. c:).
    */
   for (p=f=fname; *p; p++) {
      if (*p == '/') {
	 f = p; 		      /* set pos of last slash */
      }
   }
   if (*f == '/') {                   /* did we find a slash? */
      f++;			      /* yes, point to filename */
   } else {			      /* no, whole thing must be path name */
      f = p;
   }

   /* If filename doesn't exist (i.e. root directory), we
    * simply create a blank name consisting of a single 
    * space. This makes handling zero length filenames
    * easier.
    */
   mdb->fnl = p - f;
   if (mdb->fnl > 0) {
      mdb->fname = check_pool_memory_size(mdb->fname, mdb->fnl+1);
      strncpy(mdb->fname, f, mdb->fnl);    /* copy filename */
      mdb->fname[mdb->fnl] = 0;
   } else {
      mdb->fname[0] = ' ';            /* blank filename */
      mdb->fname[1] = 0;
      mdb->fnl = 1;
   }

   mdb->pnl = f - fname;    
   if (mdb->pnl > 0) {
      mdb->path = check_pool_memory_size(mdb->path, mdb->pnl+1);
      strncpy(mdb->path, fname, mdb->pnl);
      mdb->path[mdb->pnl] = 0;
   } else {
      Mmsg1(&mdb->errmsg, _("Path length is zero. File=%s\n"), fname);
      Jmsg(mdb->jcr, M_ERROR, 0, "%s", mdb->errmsg);
      mdb->path[0] = ' ';
      mdb->path[1] = 0;
      mdb->pnl = 1;
   }

   Dmsg2(100, "sllit path=%s file=%s\n", mdb->path, mdb->fname);
}

#endif /* HAVE_MYSQL | HAVE_SQLITE */
