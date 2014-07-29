/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2011-2014 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from many
   others, a complete list can be found in the file AUTHORS.

   You may use this file and others of this release according to the
   license defined in the LICENSE file, which includes the Affero General
   Public License, v3.0 ("AGPLv3") and some additional permissions and
   terms pursuant to its AGPLv3 Section 7.

   Bacula® is a registered trademark of Kern Sibbald.
*/
/*
 * Generic catalog class methods.
 *
 * Written by Marco van Wieringen, January 2011
 */

#include "bacula.h"

#if HAVE_SQLITE3 || HAVE_MYSQL || HAVE_POSTGRESQL

#include "cats.h"
#include "bdb_priv.h"
#include "sql_glue.h"

bool B_DB::db_match_database(const char *db_driver, const char *db_name,
                             const char *db_address, int db_port)
{
   bool match;

   if (db_driver) {
      match = strcasecmp(m_db_driver, db_driver) == 0 &&
              bstrcmp(m_db_name, db_name) &&
              bstrcmp(m_db_address, db_address) &&
              m_db_port == db_port &&
              m_dedicated == false;
   } else {
      match = bstrcmp(m_db_name, db_name) &&
              bstrcmp(m_db_address, db_address) &&
              m_db_port == db_port &&
              m_dedicated == false;
   }
   return match;
}

B_DB *B_DB::db_clone_database_connection(JCR *jcr, bool mult_db_connections)
{
   /*
    * See if its a simple clone e.g. with mult_db_connections set to false
    * then we just return the calling class pointer.
    */
   if (!mult_db_connections) {
      m_ref_count++;
      return this;
   }

   /*
    * A bit more to do here just open a new session to the database.
    */
   return db_init_database(jcr, m_db_driver, m_db_name, m_db_user, m_db_password,
                           m_db_address, m_db_port, m_db_socket, true, m_disabled_batch_insert);
}

const char *B_DB::db_get_type(void)
{
   switch (m_db_interface_type) {
   case SQL_INTERFACE_TYPE_MYSQL:
      return "MySQL";
   case SQL_INTERFACE_TYPE_POSTGRESQL:
      return "PostgreSQL";
   case SQL_INTERFACE_TYPE_SQLITE3:
      return "SQLite3";
   default:
      return "Unknown";
   }
}

/*
 * Lock database, this can be called multiple times by the same
 * thread without blocking, but must be unlocked the number of
 * times it was locked using db_unlock().
 */
void B_DB::_db_lock(const char *file, int line)
{
   int errstat;

   if ((errstat = rwl_writelock_p(&m_lock, file, line)) != 0) {
      berrno be;
      e_msg(file, line, M_FATAL, 0, "rwl_writelock failure. stat=%d: ERR=%s\n",
            errstat, be.bstrerror(errstat));
   }
}

/*
 * Unlock the database. This can be called multiple times by the
 * same thread up to the number of times that thread called
 * db_lock()/
 */
void B_DB::_db_unlock(const char *file, int line)
{
   int errstat;

   if ((errstat = rwl_writeunlock(&m_lock)) != 0) {
      berrno be;
      e_msg(file, line, M_FATAL, 0, "rwl_writeunlock failure. stat=%d: ERR=%s\n",
            errstat, be.bstrerror(errstat));
   }
}

bool B_DB::db_sql_query(const char *query, int flags)
{
   bool retval;

   db_lock(this);
   retval = ((B_DB_PRIV *)this)->sql_query(query, flags);
   if (!retval) {
      Mmsg(errmsg, _("Query failed: %s: ERR=%s\n"), query, ((B_DB_PRIV *)this)->sql_strerror());
   }
   db_unlock(this);
   return retval;
}

void B_DB::print_lock_info(FILE *fp)
{
   if (m_lock.valid == RWLOCK_VALID) {
      fprintf(fp, "\tRWLOCK=%p w_active=%i w_wait=%i\n", &m_lock, m_lock.w_active, m_lock.w_wait);
   }
}

#endif /* HAVE_SQLITE3 || HAVE_MYSQL || HAVE_POSTGRESQL */
