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
#ifndef __BDB_PRIV_H_
#define __BDB_PRIV_H_ 1

#ifndef _BDB_PRIV_INTERFACE_
#error "Illegal inclusion of catalog private interface"
#endif

/*
 * Generic definition of a sql_row.
 */
typedef char **SQL_ROW;

/*
 * Generic definition of a a sql_field.
 */
typedef struct sql_field {
   char *name;                        /* name of column */
   int max_length;                    /* max length */
   uint32_t type;                     /* type */
   uint32_t flags;                    /* flags */
} SQL_FIELD;

class B_DB_PRIV: public B_DB {
protected:
   int m_status;                      /* status */
   int m_num_rows;                    /* number of rows returned by last query */
   int m_num_fields;                  /* number of fields returned by last query */
   int m_rows_size;                   /* size of malloced rows */
   int m_fields_size;                 /* size of malloced fields */
   int m_row_number;                  /* row number from xx_data_seek */
   int m_field_number;                /* field number from sql_field_seek */
   SQL_ROW m_rows;                    /* defined rows */
   SQL_FIELD *m_fields;               /* defined fields */
   bool m_allow_transactions;         /* transactions allowed */
   bool m_transaction;                /* transaction started */

public:
   /* methods */
   B_DB_PRIV() {};
   virtual ~B_DB_PRIV() {};

   int sql_num_rows(void) { return m_num_rows; };
   void sql_field_seek(int field) { m_field_number = field; };
   int sql_num_fields(void) { return m_num_fields; };
   virtual void sql_free_result(void) = 0;
   virtual SQL_ROW sql_fetch_row(void) = 0;
   virtual bool sql_query(const char *query, int flags=0) = 0;
   virtual const char *sql_strerror(void) = 0;
   virtual void sql_data_seek(int row) = 0;
   virtual int sql_affected_rows(void) = 0;
   virtual uint64_t sql_insert_autokey_record(const char *query, const char *table_name) = 0;
   virtual SQL_FIELD *sql_fetch_field(void) = 0;
   virtual bool sql_field_is_not_null(int field_type) = 0;
   virtual bool sql_field_is_numeric(int field_type) = 0;
   virtual bool sql_batch_start(JCR *jcr) = 0;
   virtual bool sql_batch_end(JCR *jcr, const char *error) = 0;
   virtual bool sql_batch_insert(JCR *jcr, ATTR_DBR *ar) = 0;
};

#endif /* __BDB_PRIV_H_ */
