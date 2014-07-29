/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2009-2014 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from many
   others, a complete list can be found in the file AUTHORS.

   You may use this file and others of this release according to the
   license defined in the LICENSE file, which includes the Affero General
   Public License, v3.0 ("AGPLv3") and some additional permissions and
   terms pursuant to its AGPLv3 Section 7.

   Bacula® is a registered trademark of Kern Sibbald.
*/
#ifndef __BDB_POSTGRESQL_H_
#define __BDB_POSTGRESQL_H_ 1

class B_DB_POSTGRESQL: public B_DB_PRIV {
private:
   PGconn *m_db_handle;
   PGresult *m_result;
   POOLMEM *m_buf;                        /* Buffer to manipulate queries */

public:
   B_DB_POSTGRESQL(JCR *jcr, const char *db_driver, const char *db_name,
                   const char *db_user, const char *db_password,
                   const char *db_address, int db_port, const char *db_socket,
                   bool mult_db_connections, bool disable_batch_insert);
   ~B_DB_POSTGRESQL();

   /* low level operations */
   bool db_open_database(JCR *jcr);
   void db_close_database(JCR *jcr);
   void db_thread_cleanup(void);
   void db_escape_string(JCR *jcr, char *snew, char *old, int len);
   char *db_escape_object(JCR *jcr, char *old, int len);
   void db_unescape_object(JCR *jcr, char *from, int32_t expected_len,
                           POOLMEM **dest, int32_t *len);
   void db_start_transaction(JCR *jcr);
   void db_end_transaction(JCR *jcr);
   bool db_sql_query(const char *query, DB_RESULT_HANDLER *result_handler, void *ctx);
   bool db_big_sql_query(const char *query, DB_RESULT_HANDLER *result_handler, void *ctx);
   void sql_free_result(void);
   SQL_ROW sql_fetch_row(void);
   bool sql_query(const char *query, int flags=0);
   const char *sql_strerror(void);
   int sql_num_rows(void);
   void sql_data_seek(int row);
   int sql_affected_rows(void);
   uint64_t sql_insert_autokey_record(const char *query, const char *table_name);
   void sql_field_seek(int field);
   SQL_FIELD *sql_fetch_field(void);
   int sql_num_fields(void);
   bool sql_field_is_not_null(int field_type);
   bool sql_field_is_numeric(int field_type);
   bool sql_batch_start(JCR *jcr);
   bool sql_batch_end(JCR *jcr, const char *error);
   bool sql_batch_insert(JCR *jcr, ATTR_DBR *ar);
};

#endif /* __BDB_POSTGRESQL_H_ */
