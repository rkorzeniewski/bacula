/*
 * Bacula Catalog Database routines specific to SQLite
 *
 *    Kern Sibbald, January 2002
 *
 *    Version $Id$
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


/* The following is necessary so that we do not include
 * the dummy external definition of DB.
 */
#define __SQL_C 		      /* indicate that this is sql.c */

#include "bacula.h"
#include "cats.h"

#ifdef HAVE_SQLITE

/* -----------------------------------------------------------------------
 *
 *    SQLite dependent defines and subroutines
 *
 * -----------------------------------------------------------------------
 */

extern char *working_directory;

/* List of open databases */
static BQUEUE db_list = {&db_list, &db_list};

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

int QueryDB(char *file, int line, B_DB *db, char *select_cmd);


/*
 * Initialize database data structure. In principal this should
 * never have errors, or it is really fatal.
 */
B_DB *
db_init_database(char *db_name, char *db_user, char *db_password)
{
   B_DB *mdb;

   P(mutex);			      /* lock DB queue */
   /* Look to see if DB already open */
   for (mdb=NULL; (mdb=(B_DB *)qnext(&db_list, &mdb->bq)); ) {
      if (strcmp(mdb->db_name, db_name) == 0) {
         Dmsg2(100, "DB REopen %d %s\n", mdb->ref_count, db_name);
	 mdb->ref_count++;
	 V(mutex);
	 return mdb;		      /* already open */
      }
   }
   Dmsg0(100, "db_open first time\n");
   mdb = (B_DB *) malloc(sizeof(B_DB));
   memset(mdb, 0, sizeof(B_DB));
   mdb->db_name = bstrdup(db_name);
   mdb->have_insert_id = TRUE; 
   mdb->errmsg = get_pool_memory(PM_EMSG); /* get error message buffer */
   *mdb->errmsg = 0;
   mdb->cmd = get_pool_memory(PM_EMSG);    /* get command buffer */
   mdb->cached_path = get_pool_memory(PM_FNAME);
   mdb->cached_path_id = 0;
   mdb->ref_count = 1;
   qinsert(&db_list, &mdb->bq); 	   /* put db in list */
   V(mutex);
   return mdb;
}

/*
 * Now actually open the database.  This can generate errors,
 * which are returned in the errmsg
 */
int
db_open_database(B_DB *mdb)
{
   char *db_name;
   int len;
   struct stat statbuf;

   P(mutex);
   if (mdb->connected) {
      V(mutex);
      return 1;
   }
   mdb->connected = FALSE;

   if (rwl_init(&mdb->lock) != 0) {
      Mmsg1(&mdb->errmsg, _("Unable to initialize DB lock. ERR=%s\n"), strerror(errno));
      V(mutex);
      return 0;
   }

   /* open the database */
   len = strlen(working_directory) + strlen(mdb->db_name) + 5; 
   db_name = (char *)malloc(len);
   strcpy(db_name, working_directory);
   strcat(db_name, "/");
   strcat(db_name, mdb->db_name);
   strcat(db_name, ".db");
   if (stat(db_name, &statbuf) != 0) {
      Mmsg1(&mdb->errmsg, _("Database %s does not exist, please create it.\n"), 
	 db_name);
      free(db_name);
      V(mutex);
      return 0;
   }
   mdb->db = sqlite_open(
	db_name,		      /* database name */
	644,			      /* mode */
	&mdb->sqlite_errmsg);	      /* error message */

   Dmsg0(50, "sqlite_open\n");
  
   if (mdb->db == NULL) {
      Mmsg2(&mdb->errmsg, _("Unable to open Database=%s. ERR=%s\n"),
         db_name, mdb->sqlite_errmsg ? mdb->sqlite_errmsg : _("unknown"));
      free(db_name);
      V(mutex);
      return 0;
   }
   free(db_name);
   if (!check_tables_version(mdb)) {
      V(mutex);
      return 0;
   }

   mdb->connected = TRUE;
   V(mutex);
   return 1;
}

void
db_close_database(B_DB *mdb)
{
   P(mutex);
   mdb->ref_count--;
   if (mdb->ref_count == 0) {
      qdchain(&mdb->bq);
      if (mdb->connected && mdb->db) {
	 sqlite_close(mdb->db);
      }
      rwl_destroy(&mdb->lock);	     
      free_pool_memory(mdb->errmsg);
      free_pool_memory(mdb->cmd);
      free_pool_memory(mdb->cached_path);
      if (mdb->db_name) {
	 free(mdb->db_name);
      }
      free(mdb);
   }
   V(mutex);
}

/*
 * Return the next unique index (auto-increment) for
 * the given table.  Return 0 on error.
 */
int db_next_index(B_DB *mdb, char *table, char *index)
{
   SQL_ROW row;

   db_lock(mdb);

   Mmsg(&mdb->cmd,
"SELECT id FROM NextId WHERE TableName=\"%s\"", table);
   if (!QUERY_DB(mdb, mdb->cmd)) {
      Mmsg(&mdb->errmsg, _("next_index query error: ERR=%s\n"), sql_strerror(mdb));
      db_unlock(mdb);
      return 0;
   }
   if ((row = sql_fetch_row(mdb)) == NULL) {
      Mmsg(&mdb->errmsg, _("Error fetching index: ERR=%s\n"), sql_strerror(mdb));
      db_unlock(mdb);
      return 0;
   }
   strncpy(index, row[0], 28);
   index[28] = 0;
   sql_free_result(mdb);

   Mmsg(&mdb->cmd,
"UPDATE NextId SET id=id+1 WHERE TableName=\"%s\"", table);
   if (!QUERY_DB(mdb, mdb->cmd)) {
      Mmsg(&mdb->errmsg, _("next_index update error: ERR=%s\n"), sql_strerror(mdb));
      db_unlock(mdb);
      return 0;
   }
   sql_free_result(mdb);

   db_unlock(mdb);
   return 1;
}   



void
db_escape_string(char *snew, char *old, int len)
{
   char *n, *o;

   n = snew;
   o = old;
   while (len--) {
      switch (*o) {
      case '\'':
         *n++ = '\\';
         *n++ = '\'';
	 o++;
	 break;
      case '"':
         *n++ = '\\';
         *n++ = '"';
	 o++;
	 break;
      case 0:
         *n++ = '\\';
	 *n++ = 0;
	 o++;
	 break;
      default:
	 *n++ = *o++;
	 break;
      }
   }
   *n = 0;
}

struct rh_data {
   DB_RESULT_HANDLER *result_handler;
   void *ctx;
};

/*  
 * Convert SQLite's callback into Bacula DB callback  
 */
static int sqlite_result(void *arh_data, int num_fields, char **rows, char **col_names)
{
   struct rh_data *rh_data = (struct rh_data *)arh_data;   

   if (rh_data->result_handler) {
      (*(rh_data->result_handler))(rh_data->ctx, num_fields, rows);
   }
   return 0;
}

/*
 * Submit a general SQL command (cmd), and for each row returned,
 *  the sqlite_handler is called with the ctx.
 */
int db_sql_query(B_DB *mdb, char *query, DB_RESULT_HANDLER *result_handler, void *ctx)
{
   struct rh_data rh_data;
   int stat;

   db_lock(mdb);
   if (mdb->sqlite_errmsg) {
      actuallyfree(mdb->sqlite_errmsg);
      mdb->sqlite_errmsg = NULL;
   }
   rh_data.result_handler = result_handler;
   rh_data.ctx = ctx;
   stat = sqlite_exec(mdb->db, query, sqlite_result, (void *)&rh_data, &mdb->sqlite_errmsg);
   if (stat != 0) {
      Mmsg(&mdb->errmsg, _("Query failed: %s: ERR=%s\n"), query, sql_strerror(mdb));
      db_unlock(mdb);
      return 0;
   }
   db_unlock(mdb);
   return 1;
}

/*
 * Submit a sqlite query and retrieve all the data
 */
int my_sqlite_query(B_DB *mdb, char *cmd) 
{
   int stat;

   if (mdb->sqlite_errmsg) {
      actuallyfree(mdb->sqlite_errmsg);
      mdb->sqlite_errmsg = NULL;
   }
   stat = sqlite_get_table(mdb->db, cmd, &mdb->result, &mdb->nrow, &mdb->ncolumn,
	    &mdb->sqlite_errmsg);
   mdb->row = 0;		      /* row fetched */
   return stat;
}

/* Fetch one row at a time */
SQL_ROW my_sqlite_fetch_row(B_DB *mdb)
{
   if (mdb->row >= mdb->nrow) {
      return NULL;
   }
   mdb->row++;
   return &mdb->result[mdb->ncolumn * mdb->row];
}

void my_sqlite_free_table(B_DB *mdb)
{
   unsigned int i;

   if (mdb->fields_defined) {
      for (i=0; i < sql_num_fields(mdb); i++) {
	 free(mdb->fields[i]);
      }
      free(mdb->fields);
      mdb->fields_defined = FALSE;
   }
   sqlite_free_table(mdb->result);
   mdb->nrow = mdb->ncolumn = 0; 
}

void my_sqlite_field_seek(B_DB *mdb, int field)
{
   unsigned int i, j;
   if (mdb->result == NULL) {
      return;
   }
   /* On first call, set up the fields */
   if (!mdb->fields_defined && sql_num_fields(mdb) > 0) {
      mdb->fields = (SQL_FIELD **)malloc(sizeof(SQL_FIELD) * mdb->ncolumn);
      for (i=0; i < sql_num_fields(mdb); i++) {
	 mdb->fields[i] = (SQL_FIELD *)malloc(sizeof(SQL_FIELD));
	 mdb->fields[i]->name = mdb->result[i];
	 mdb->fields[i]->length = strlen(mdb->fields[i]->name);
	 mdb->fields[i]->max_length = mdb->fields[i]->length;
	 for (j=1; j <= (unsigned)mdb->nrow; j++) {
	    uint32_t len;
	    if (mdb->result[i + mdb->ncolumn *j]) {
	       len = (uint32_t)strlen(mdb->result[i + mdb->ncolumn * j]);
	    } else {
	       len = 0;
	    }
	    if (len > mdb->fields[i]->max_length) {
	       mdb->fields[i]->max_length = len;
	    }
	 }
	 mdb->fields[i]->type = 0;
	 mdb->fields[i]->flags = 1;	   /* not null */
      }
      mdb->fields_defined = TRUE;
   }
   if (field > (int)sql_num_fields(mdb)) {
      field = (int)sql_num_fields(mdb);
    }
    mdb->field = field;

}

SQL_FIELD *my_sqlite_fetch_field(B_DB *mdb)
{
   return mdb->fields[mdb->field++];
}

static void
list_dashes(B_DB *mdb, DB_LIST_HANDLER *send, void *ctx)
{
   SQL_FIELD *field;
   unsigned int i, j;

   sql_field_seek(mdb, 0);
   send(ctx, "+");
   for (i = 0; i < sql_num_fields(mdb); i++) {
      field = sql_fetch_field(mdb);
      for (j = 0; j < field->max_length + 2; j++)
              send(ctx, "-");
      send(ctx, "+");
   }
   send(ctx, "\n");
}

void
list_result(B_DB *mdb, DB_LIST_HANDLER *send, void *ctx)
{
   SQL_FIELD *field;
   SQL_ROW row;
   unsigned int i, col_len;
   char buf[2000], ewc[30];

   if (mdb->result == NULL || mdb->nrow == 0) {
      send(ctx, _("No results to list.\n"));
      return;
   }
   /* determine column display widths */
   sql_field_seek(mdb, 0);
   for (i = 0; i < sql_num_fields(mdb); i++) {
      field = sql_fetch_field(mdb);
      if (IS_NUM(field->type) && field->max_length > 0) { /* fixup for commas */
	 field->max_length += (field->max_length - 1) / 3;
      }
      col_len = strlen(field->name);
      if (col_len < field->max_length)
	      col_len = field->max_length;
      if (col_len < 4 && !IS_NOT_NULL(field->flags))
              col_len = 4;    /* 4 = length of the word "NULL" */
      field->max_length = col_len;    /* reset column info */
   }

   list_dashes(mdb, send, ctx);
   send(ctx, "|");
   sql_field_seek(mdb, 0);
   for (i = 0; i < sql_num_fields(mdb); i++) {
      field = sql_fetch_field(mdb);
      sprintf(buf, " %-*s |", field->max_length, field->name);
      send(ctx, buf);
   }
   send(ctx, "\n");
   list_dashes(mdb, send, ctx);

   while ((row = sql_fetch_row(mdb)) != NULL) {
      sql_field_seek(mdb, 0);
      send(ctx, "|");
      for (i = 0; i < sql_num_fields(mdb); i++) {
	 field = sql_fetch_field(mdb);
	 if (row[i] == NULL) {
            sprintf(buf, " %-*s |", field->max_length, "NULL");
	 } else if (IS_NUM(field->type)) {
            sprintf(buf, " %*s |", field->max_length,       
	       add_commas(row[i], ewc));
	 } else {
            sprintf(buf, " %-*s |", field->max_length, row[i]);
	 }
	 send(ctx, buf);
      }
      send(ctx, "\n");
   }
   list_dashes(mdb, send, ctx);
}


#endif /* HAVE_SQLITE */
