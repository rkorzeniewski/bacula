/*
 * Bacula Catalog Database routines specific to MySQL
 *   These are MySQL specific routines -- hopefully all
 *    other files are generic.
 *
 *    Kern Sibbald, March 2000
 *
 *    Version $Id$
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
 * the dummy external definition of DB.
 */
#define __SQL_C 		      /* indicate that this is sql.c */

#include "bacula.h"
#include "cats.h"

#ifdef HAVE_MYSQL

/* -----------------------------------------------------------------------
 *
 *   MySQL dependent defines and subroutines
 *
 * -----------------------------------------------------------------------
 */

/* List of open databases */
static BQUEUE db_list = {&db_list, &db_list};

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

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
   mdb->db_user = bstrdup(db_user);
   mdb->db_password = bstrdup(db_password);
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
   P(mutex);
   if (mdb->connected) {
      V(mutex);
      return 1;
   }
   mdb->connected = FALSE;

   if (rwl_init(&mdb->lock) != 0) {
      Mmsg1(&mdb->errmsg, "Unable to initialize DB lock. ERR=%s\n", strerror(errno));
      V(mutex);
      return 0;
   }

   /* connect to the database */
#ifdef HAVE_EMBEDDED_MYSQL
   mysql_server_init(0, NULL, NULL);
#endif
   mysql_init(&(mdb->mysql));
   Dmsg0(50, "mysql_init done\n");
   mdb->db = mysql_real_connect(
	&(mdb->mysql),		      /* db */
	NULL,			      /* default = localhost */
	mdb->db_user,		      /*  login name */
	mdb->db_password,	      /*  password */
	mdb->db_name,		      /* database name */
	0,			      /* default port */
	NULL,			      /* default = socket */
	0);			      /* flags = none */

   /* If no connect, try once more incase it is a timing problem */
   if (mdb->db == NULL) {
      mdb->db = mysql_real_connect(
	   &(mdb->mysql),		 /* db */
	   NULL,			 /* default = localhost */
	   mdb->db_user,		 /*  login name */
	   mdb->db_password,		 /*  password */
	   mdb->db_name,		 /* database name */
	   0,				 /* default port */
	   NULL,			 /* default = socket */
	   0);				 /* flags = none */
   }
    
   Dmsg0(50, "mysql_real_connect done\n");
   Dmsg3(50, "db_user=%s db_name=%s db_password=%s\n", mdb->db_user, mdb->db_name, 
            mdb->db_password==NULL?"(NULL)":mdb->db_password);
  
   if (mdb->db == NULL) {
      Mmsg2(&mdb->errmsg, _("Unable to connect to MySQL server. \n\
Database=%s User=%s\n\
It is probably not running or your password is incorrect.\n"), 
	 mdb->db_name, mdb->db_user);
      V(mutex);
      return 0;
   }

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
	 sql_close(mdb);
#ifdef HAVE_EMBEDDED_MYSQL
	 mysql_server_end();
#endif
      }
/*    pthread_mutex_destroy(&mdb->mutex); */
      rwl_destroy(&mdb->lock);	     
      free_pool_memory(mdb->errmsg);
      free_pool_memory(mdb->cmd);
      free_pool_memory(mdb->cached_path);
      if (mdb->db_name) {
	 free(mdb->db_name);
      }
      if (mdb->db_user) {
	 free(mdb->db_user);
      }
      if (mdb->db_password) {
	 free(mdb->db_password);
      }
      free(mdb);
   }
   V(mutex);
}

/*
 * Return the next unique index (auto-increment) for
 * the given table.  Return NULL on error.
 *  
 * For MySQL, NULL causes the auto-increment value
 *  to be updated.
 */
int db_next_index(B_DB *mdb, char *table, char *index)
{
   strcpy(index, "NULL");
   return 1;
}   



void
db_escape_string(char *snew, char *old, int len)
{
   mysql_escape_string(snew, old, len);
}

/*
 * Submit a general SQL command (cmd), and for each row returned,
 *  the sqlite_handler is called with the ctx.
 */
int db_sql_query(B_DB *mdb, char *query, DB_RESULT_HANDLER *result_handler, void *ctx)
{
   SQL_ROW row;
  
   db_lock(mdb);
   if (sql_query(mdb, query) != 0) {
      Mmsg(&mdb->errmsg, _("Query failed: %s: ERR=%s\n"), query, sql_strerror(mdb));
      db_unlock(mdb);
      return 0;
   }
   if (result_handler != NULL) {
      if ((mdb->result = sql_store_result(mdb)) != NULL) {
	 int num_fields = sql_num_fields(mdb);

	 while ((row = sql_fetch_row(mdb)) != NULL) {
	    if (result_handler(ctx, num_fields, row))
	       break;
	 }

	 sql_free_result(mdb);
      }
   }
   db_unlock(mdb);
   return 1;

}


static void
list_dashes(B_DB *mdb, DB_LIST_HANDLER *send, void *ctx)
{
   SQL_FIELD  *field;
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
   SQL_ROW    row;
   unsigned int i, col_len;
   char buf[2000], ewc[30];

   if (mdb->result == NULL) {
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
      sprintf(buf, " %-*s |", (int)field->max_length, field->name);
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
            sprintf(buf, " %-*s |", (int)field->max_length, "NULL");
	 } else if (IS_NUM(field->type)) {
            sprintf(buf, " %*s |", (int)field->max_length,       
	       add_commas(row[i], ewc));
	 } else {
            sprintf(buf, " %-*s |", (int)field->max_length, row[i]);
	 }
	 send(ctx, buf);
      }
      send(ctx, "\n");
   }
   list_dashes(mdb, send, ctx);
}


#endif /* HAVE_MYSQL */
