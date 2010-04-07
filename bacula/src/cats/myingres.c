/*
   Bacula® - The Network Backup Solution
   Copyright (C) 2009-2010 Free Software Foundation Europe e.V.
   The main author of Bacula is Kern Sibbald, with contributions from
   many others, a complete list can be found in the file AUTHORS.
   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version two of the GNU General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.
   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
   Bacula® is a registered trademark of Kern Sibbald.
   The licensor of Bacula is the Free Software Foundation Europe
   (FSFE), Fiduciary Program, Sumatrastrasse 25, 8006 Zürich,
   Switzerland, email:ftf@fsfeurope.org.
*/
/*
 * Bacula Catalog Database routines specific to Ingres
 *   These are Ingres specific routines
 *
 *    Stefan Reddig, June 2009 with help of Marco van Wieringen April 2010
 */
#include "bacula.h"
/* # line 37 "myingres.sc" */	
#ifdef HAVE_INGRES
#include <eqpname.h>
#include <eqdefcc.h>
#include <eqsqlca.h>
#include <eqsqlda.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "myingres.h"

#ifdef __cplusplus
extern "C" {
#endif
IISQLCA *IIsqlca();
#ifdef __cplusplus
}
#endif
#define sqlca (*(IIsqlca()))

/*
 * ---Implementations---
 */
short INGgetCols(INGconn *dbconn, const char *query, bool transaction)
{
/* # line 52 "myingres.sc" */	
  
  int sess_id;
  char *stmt;
/* # line 55 "myingres.sc" */	
  
   short number = -1;
   IISQLDA *sqlda;
   sqlda = (IISQLDA *)malloc(IISQDA_HEAD_SIZE + IISQDA_VAR_SIZE);
   memset(sqlda, 0, (IISQDA_HEAD_SIZE + IISQDA_VAR_SIZE));
   sqlda->sqln = number;
   stmt = bstrdup(query);
   /*
    * Switch to the correct default session for this thread.
    */
   sess_id = dbconn->session_id;
/* # line 71 "myingres.sc" */	/* set_sql */
  {
    IILQssSetSqlio(11,(short *)0,1,30,sizeof(sess_id),&sess_id);
  }
/* # line 75 "myingres.sc" */	/* prepare */
  {
    IIsqInit(&sqlca);
    IIsqPrepare(0,(char *)"s1",(char *)0,0,stmt);
    if (sqlca.sqlcode < 0) 
      goto bail_out;
  }
/* # line 76 "myingres.sc" */	/* describe */
  {
    IIsqInit(&sqlca);
    IIsqDescribe(0,(char *)"s1",sqlda,0);
    if (sqlca.sqlcode < 0) 
      goto bail_out;
  }
/* # line 80 "myingres.sc" */	/* host code */
   number = sqlda->sqld;
   /*
    * If we are not in a transaction we commit our work now.
    */
   if (!transaction) {
/* # line 86 "myingres.sc" */	/* commit */
  {
    IIsqInit(&sqlca);
    IIxact(3);
  }
/* # line 87 "myingres.sc" */	/* host code */
   }
bail_out:
   /*
    * Switch to no default session for this thread.
    */
/* # line 93 "myingres.sc" */	/* set_sql */
  {
    IILQssSetSqlio(11,(short *)0,1,30,sizeof(-97),(void *)IILQint(-97));
  }
/* # line 94 "myingres.sc" */	/* host code */
   free(stmt);
   free(sqlda);
   return number;
}
static inline IISQLDA *INGgetDescriptor(short numCols, const char *query)
{
/* # line 101 "myingres.sc" */	
  
  char *stmt;
/* # line 103 "myingres.sc" */	
  
   int i;
   IISQLDA *sqlda;
   sqlda = (IISQLDA *)malloc(IISQDA_HEAD_SIZE + (numCols * IISQDA_VAR_SIZE));
   memset(sqlda, 0, (IISQDA_HEAD_SIZE + (numCols * IISQDA_VAR_SIZE)));
   sqlda->sqln = numCols;
   stmt = bstrdup(query);
/* # line 115 "myingres.sc" */	/* prepare */
  {
    IIsqInit(&sqlca);
    IIsqPrepare(0,(char *)"s2",sqlda,0,stmt);
  }
/* # line 117 "myingres.sc" */	/* host code */
   for (i = 0; i < sqlda->sqld; ++i) {
      /*
       * Negative type indicates nullable columns, so an indicator
       * is allocated, otherwise it's null
       */
      if (sqlda->sqlvar[i].sqltype > 0) {
         sqlda->sqlvar[i].sqlind = NULL;
      } else {
         sqlda->sqlvar[i].sqlind = (short *)malloc(sizeof(short));
      }
      /*
       * Alloc space for variable like indicated in sqllen
       * for date types sqllen is always 0 -> allocate by type
       */
      switch (abs(sqlda->sqlvar[i].sqltype)) {
      case IISQ_TSW_TYPE:
         sqlda->sqlvar[i].sqldata = (char *)malloc(IISQ_TSW_LEN);
         break;
      case IISQ_TSWO_TYPE:
         sqlda->sqlvar[i].sqldata = (char *)malloc(IISQ_TSWO_LEN);
         break;
      case IISQ_TSTMP_TYPE:
         sqlda->sqlvar[i].sqldata = (char *)malloc(IISQ_TSTMP_LEN);
         break;
      default:
         /*
          * plus one to avoid zero mem allocs
          */
         sqlda->sqlvar[i].sqldata = (char *)malloc(sqlda->sqlvar[i].sqllen+1);
         break;
      }
   }
   free(stmt);
   return sqlda;
}
static void INGfreeDescriptor(IISQLDA *sqlda)
{
   int i;
   if (!sqlda) {
      return;
   }
   for (i = 0; i < sqlda->sqld; ++i) {
      if (sqlda->sqlvar[i].sqldata) {
         free(sqlda->sqlvar[i].sqldata);
      }
      if (sqlda->sqlvar[i].sqlind) {
         free(sqlda->sqlvar[i].sqlind);
      }
   }
   free(sqlda);
   sqlda = NULL;
}
static inline int INGgetTypeSize(IISQLVAR *ingvar)
{
   int inglength = 0;
   /*
    * TODO: add date types (at least TSTMP,TSW TSWO)
    */
   switch (ingvar->sqltype) {
   case IISQ_DTE_TYPE:
      inglength = 25;
      break;
   case IISQ_MNY_TYPE:
      inglength = 8;
      break;
   default:
      inglength = ingvar->sqllen;
      break;
   }
   return inglength;
}
static inline INGresult *INGgetINGresult(IISQLDA *sqlda)
{
   int i;
   INGresult *result = NULL;
   if (!sqlda) {
      return NULL;
   }
   result = (INGresult *)malloc(sizeof(INGresult));
   memset(result, 0, sizeof(INGresult));
   result->sqlda = sqlda;
   result->num_fields = sqlda->sqld;
   result->num_rows = 0;
   result->first_row = NULL;
   result->status = ING_EMPTY_RESULT;
   result->act_row = NULL;
   memset(result->numrowstring, 0, sizeof(result->numrowstring));
   if (result->num_fields) {
      result->fields = (INGRES_FIELD *)malloc(sizeof(INGRES_FIELD) * result->num_fields);
      memset(result->fields, 0, sizeof(INGRES_FIELD) * result->num_fields);
      for (i = 0; i < result->num_fields; ++i) {
         memset(result->fields[i].name, 0, 34);
         bstrncpy(result->fields[i].name, sqlda->sqlvar[i].sqlname.sqlnamec, sqlda->sqlvar[i].sqlname.sqlnamel);
         result->fields[i].max_length = INGgetTypeSize(&sqlda->sqlvar[i]);
         result->fields[i].type = abs(sqlda->sqlvar[i].sqltype);
         result->fields[i].flags = (sqlda->sqlvar[i].sqltype < 0) ? 1 : 0;
      }
   }
   return result;
}
static inline void INGfreeRowSpace(ING_ROW *row, IISQLDA *sqlda)
{
   int i;
   if (row == NULL || sqlda == NULL) {
      return;
   }
   for (i = 0; i < sqlda->sqld; ++i) {
      if (row->sqlvar[i].sqldata) {
         free(row->sqlvar[i].sqldata);
      }
      if (row->sqlvar[i].sqlind) {
         free(row->sqlvar[i].sqlind);
      }
   }
   free(row->sqlvar);
   free(row);
}
static void INGfreeINGresult(INGresult *ing_res)
{
   int rows;
   ING_ROW *rowtemp;
   if (!ing_res) {
      return;
   }
   /*
    * Free all rows and fields, then res, not descriptor!
    *
    * Use of rows is a nasty workaround til I find the reason,
    * why aggregates like max() don't work
    */
   rows = ing_res->num_rows;
   ing_res->act_row = ing_res->first_row;
   while (ing_res->act_row != NULL && rows > 0) {
      rowtemp = ing_res->act_row->next;
      INGfreeRowSpace(ing_res->act_row, ing_res->sqlda);
      ing_res->act_row = rowtemp;
      --rows;
   }
   if (ing_res->fields) {
      free(ing_res->fields);
   }
   free(ing_res);
}
static inline ING_ROW *INGgetRowSpace(INGresult *ing_res)
{
   int i;
   unsigned short len; /* used for VARCHAR type length */
   IISQLDA *sqlda = ing_res->sqlda;
   ING_ROW *row = NULL;
   IISQLVAR *vars = NULL;
   row = (ING_ROW *)malloc(sizeof(ING_ROW));
   memset(row, 0, sizeof(ING_ROW));
   vars = (IISQLVAR *)malloc(sizeof(IISQLVAR) * sqlda->sqld);
   memset(vars, 0, sizeof(IISQLVAR) * sqlda->sqld);
   row->sqlvar = vars;
   row->next = NULL;
   for (i = 0; i < sqlda->sqld; ++i) {
      /*
       * Make strings out of the data, then the space and assign 
       * (why string? at least it seems that way, looking into the sources)
       */
      vars[i].sqlind = (short *)malloc(sizeof(short));
      if (sqlda->sqlvar[i].sqlind) {
         memcpy(vars[i].sqlind,sqlda->sqlvar[i].sqlind,sizeof(short));
      } else {
         *vars[i].sqlind = NULL;
      }
      /*
       * if sqlind pointer exists AND points to -1 -> column is 'null'
       */
      if ( *vars[i].sqlind && (*vars[i].sqlind == -1)) {
         vars[i].sqldata = NULL;
      } else {
         switch (ing_res->fields[i].type) {
         case IISQ_VCH_TYPE:
            len = ((ING_VARCHAR *)sqlda->sqlvar[i].sqldata)->len;
            vars[i].sqldata = (char *)malloc(len+1);
            memcpy(vars[i].sqldata,sqlda->sqlvar[i].sqldata+2,len);
            vars[i].sqldata[len] = '\0';
            break;
         case IISQ_CHA_TYPE:
            vars[i].sqldata = (char *)malloc(ing_res->fields[i].max_length+1);
            memcpy(vars[i].sqldata,sqlda->sqlvar[i].sqldata,sqlda->sqlvar[i].sqllen);
            vars[i].sqldata[ing_res->fields[i].max_length] = '\0';
            break;
         case IISQ_INT_TYPE:
            vars[i].sqldata = (char *)malloc(20);
            memset(vars[i].sqldata, 0, 20);
            switch (sqlda->sqlvar[i].sqllen) {
            case 2:
               bsnprintf(vars[i].sqldata, 20, "%d",*(short*)sqlda->sqlvar[i].sqldata);
               break;
            case 4:
               bsnprintf(vars[i].sqldata, 20, "%ld",*(int*)sqlda->sqlvar[i].sqldata);
               break;
            case 8:
               bsnprintf(vars[i].sqldata, 20, "%lld",*(long*)sqlda->sqlvar[i].sqldata);
               break;
            }
            break;
         case IISQ_TSTMP_TYPE:
            vars[i].sqldata = (char *)malloc(IISQ_TSTMP_LEN+1);
            vars[i].sqldata[IISQ_TSTMP_LEN] = '\0';
            break;
         case IISQ_TSWO_TYPE:
            vars[i].sqldata = (char *)malloc(IISQ_TSWO_LEN+1);
            vars[i].sqldata[IISQ_TSWO_LEN] = '\0';
            break;
         case IISQ_TSW_TYPE:
            vars[i].sqldata = (char *)malloc(IISQ_TSW_LEN+1);
            vars[i].sqldata[IISQ_TSW_LEN] = '\0';
            break;
         }
      }
   }
   return row;
}
static inline int INGfetchAll(const char *query, INGresult *ing_res)
{
   ING_ROW *row;
   IISQLDA *desc;
   int linecount = -1;
   desc = ing_res->sqlda;
/* # line 371 "myingres.sc" */	/* open */
  {
    IIsqInit(&sqlca);
    IIcsOpen((char *)"c2",11858,14584);
    IIwritio(0,(short *)0,1,32,0,(char *)"s2");
    IIcsQuery((char *)"c2",11858,14584);
    if (sqlca.sqlcode < 0) 
      goto bail_out;
  }
/* # line 375 "myingres.sc" */	/* host code */
   linecount = 0;
   do {
/* # line 377 "myingres.sc" */	/* fetch */
  {
    IIsqInit(&sqlca);
    if (IIcsRetScroll((char *)"c2",11858,14584,-1,-1) != 0) {
      IIcsDaGet(0,desc);
      IIcsERetrieve();
    } /* IIcsRetrieve */
  }
/* # line 379 "myingres.sc" */	/* host code */
      if (sqlca.sqlcode == 0 || sqlca.sqlcode == -40202) {
         /*
          * Allocate space for fetched row
          */
         row = INGgetRowSpace(ing_res);
         /*
          * Initialize list when encountered first time
          */
         if (ing_res->first_row == 0) {
            ing_res->first_row = row; /* head of the list */
            ing_res->first_row->next = NULL;
            ing_res->act_row = ing_res->first_row;
         }      
         ing_res->act_row->next = row; /* append row to old act_row */
         ing_res->act_row = row; /* set row as act_row */
         row->row_number = linecount++;
      }
   } while ( (sqlca.sqlcode == 0) || (sqlca.sqlcode == -40202) );
/* # line 400 "myingres.sc" */	/* close */
  {
    IIsqInit(&sqlca);
    IIcsClose((char *)"c2",11858,14584);
  }
/* # line 402 "myingres.sc" */	/* host code */
   ing_res->status = ING_COMMAND_OK;
   ing_res->num_rows = linecount;
bail_out:
   return linecount;
}
static inline ING_STATUS INGresultStatus(INGresult *res)
{
   if (res == NULL) {
      return ING_NO_RESULT;
   } else {
      return res->status;
   }
}
static void INGrowSeek(INGresult *res, int row_number)
{
   ING_ROW *trow = NULL;
   if (res->act_row->row_number == row_number) {
      return;
   }
   /*
    * TODO: real error handling
    */
   if (row_number<0 || row_number>res->num_rows) {
      return;
   }
   for (trow = res->first_row; trow->row_number != row_number; trow = trow->next) ;
   res->act_row = trow;
   /*
    * Note - can be null - if row_number not found, right?
    */
}
char *INGgetvalue(INGresult *res, int row_number, int column_number)
{
   if (row_number != res->act_row->row_number) {
      INGrowSeek(res, row_number);
   }
   return res->act_row->sqlvar[column_number].sqldata;
}
bool INGgetisnull(INGresult *res, int row_number, int column_number)
{
   if (row_number != res->act_row->row_number) {
      INGrowSeek(res, row_number);
   }
   return (*res->act_row->sqlvar[column_number].sqlind == -1) ? true : false;
}
int INGntuples(const INGresult *res)
{
   return res->num_rows;
}
int INGnfields(const INGresult *res)
{
   return res->num_fields;
}
char *INGfname(const INGresult *res, int column_number)
{
   if ((column_number > res->num_fields) || (column_number < 0)) {
      return NULL;
   } else {
      return res->fields[column_number].name;
   }
}
short INGftype(const INGresult *res, int column_number)
{
   return res->fields[column_number].type;
}
int INGexec(INGconn *dbconn, const char *query, bool transaction)
{
   int check;
/* # line 485 "myingres.sc" */	
  
  int sess_id;
  int rowcount;
  char *stmt;
/* # line 489 "myingres.sc" */	
  
   rowcount = -1;
   stmt = bstrdup(query);
   /*
    * Switch to the correct default session for this thread.
    */
   sess_id = dbconn->session_id;
/* # line 498 "myingres.sc" */	/* set_sql */
  {
    IILQssSetSqlio(11,(short *)0,1,30,sizeof(sess_id),&sess_id);
  }
/* # line 502 "myingres.sc" */	/* execute */
  {
    IIsqInit(&sqlca);
    IIsqExImmed(stmt);
    IIsyncup((char *)0,0);
    if (sqlca.sqlcode < 0) 
      goto bail_out;
  }
/* # line 503 "myingres.sc" */	/* inquire_ingres */
  {
    IILQisInqSqlio((short *)0,1,30,sizeof(rowcount),&rowcount,8);
  }
/* # line 507 "myingres.sc" */	/* host code */
   /*
    * If we are not in a transaction we commit our work now.
    */
   if (!transaction) {
/* # line 511 "myingres.sc" */	/* commit */
  {
    IIsqInit(&sqlca);
    IIxact(3);
  }
/* # line 512 "myingres.sc" */	/* host code */
   }
bail_out:
   /*
    * Switch to no default session for this thread.
    */
/* # line 518 "myingres.sc" */	/* set_sql */
  {
    IILQssSetSqlio(11,(short *)0,1,30,sizeof(-97),(void *)IILQint(-97));
  }
/* # line 519 "myingres.sc" */	/* host code */
   free(stmt);
   return rowcount;
}
INGresult *INGquery(INGconn *dbconn, const char *query, bool transaction)
{
   /*
    * TODO: error handling
    */
   IISQLDA *desc = NULL;
   INGresult *res = NULL;
   int rows = -1;
   int cols = INGgetCols(dbconn, query, transaction);
/* # line 532 "myingres.sc" */	
  
  int sess_id;
/* # line 534 "myingres.sc" */	
  
   /*
    * Switch to the correct default session for this thread.
    */
   sess_id = dbconn->session_id;
/* # line 540 "myingres.sc" */	/* set_sql */
  {
    IILQssSetSqlio(11,(short *)0,1,30,sizeof(sess_id),&sess_id);
  }
/* # line 542 "myingres.sc" */	/* host code */
   desc = INGgetDescriptor(cols, query);
   if (!desc) {
      goto bail_out;
   }
   res = INGgetINGresult(desc);
   if (!res) {
      goto bail_out;
   }
   rows = INGfetchAll(query, res);
   if (rows < 0) {
      INGfreeDescriptor(desc);
      INGfreeINGresult(res);
      res = NULL;
      goto bail_out;
   }
bail_out:
   /*
    * If we are not in a transaction we commit our work now.
    */
   if (!transaction) {
/* # line 566 "myingres.sc" */	/* commit */
  {
    IIsqInit(&sqlca);
    IIxact(3);
  }
/* # line 567 "myingres.sc" */	/* host code */
   }
   /*
    * Switch to no default session for this thread.
    */
/* # line 571 "myingres.sc" */	/* set_sql */
  {
    IILQssSetSqlio(11,(short *)0,1,30,sizeof(-97),(void *)IILQint(-97));
  }
/* # line 572 "myingres.sc" */	/* host code */
   return res;
}
void INGclear(INGresult *res)
{
   if (res == NULL) {
      return;
   }
   INGfreeDescriptor(res->sqlda);
   INGfreeINGresult(res);
}
void INGcommit(const INGconn *dbconn)
{
/* # line 587 "myingres.sc" */	
  
  int sess_id;
/* # line 589 "myingres.sc" */	
  
   if (dbconn != NULL) {
      sess_id = dbconn->session_id;
/* # line 593 "myingres.sc" */	/* disconnect */
  {
    IIsqInit(&sqlca);
    IILQsidSessID(sess_id);
    IIsqDisconnect();
  }
/* # line 595 "myingres.sc" */	/* host code */
      /*
       * Commit our work.
       */
/* # line 598 "myingres.sc" */	/* commit */
  {
    IIsqInit(&sqlca);
    IIxact(3);
  }
/* # line 600 "myingres.sc" */	/* host code */
      /*
       * Switch to no default session for this thread.
       */
/* # line 603 "myingres.sc" */	/* set_sql */
  {
    IILQssSetSqlio(11,(short *)0,1,30,sizeof(-97),(void *)IILQint(-97));
  }
/* # line 604 "myingres.sc" */	/* host code */
   }
}
INGconn *INGconnectDB(char *dbname, char *user, char *passwd, int session_id)
{
/* # line 609 "myingres.sc" */	
  
  char ingdbname[24];
  char ingdbuser[32];
  char ingdbpasswd[32];
  int sess_id;
/* # line 614 "myingres.sc" */	
  
   INGconn *dbconn;
   if (dbname == NULL || strlen(dbname) == 0) {
      return NULL;
   }
   sess_id = session_id;
   bstrncpy(ingdbname, dbname, sizeof(ingdbname));
/* # line 626 "myingres.sc" */	/* host code */
   if (user != NULL) {
      bstrncpy(ingdbuser, user, sizeof(ingdbuser));
      if (passwd != NULL) {
         bstrncpy(ingdbpasswd, passwd, sizeof(ingdbpasswd));
      } else {
         memset(ingdbpasswd, 0, sizeof(ingdbpasswd));
      }
/* # line 633 "myingres.sc" */	/* connect */
  {
    IIsqInit(&sqlca);
    IILQsidSessID(sess_id);
    IIsqUser(ingdbuser);
    IIsqConnect(0,ingdbname,(char *)"-dbms_password",ingdbpasswd,(char *)0, 
    (char *)0, (char *)0, (char *)0, (char *)0, (char *)0, (char *)0, 
    (char *)0, (char *)0, (char *)0, (char *)0);
    if (sqlca.sqlcode < 0) 
      goto bail_out;
  }
/* # line 638 "myingres.sc" */	/* host code */
   } else {
/* # line 639 "myingres.sc" */	/* connect */
  {
    IIsqInit(&sqlca);
    IILQsidSessID(sess_id);
    IIsqConnect(0,ingdbname,(char *)0, (char *)0, (char *)0, (char *)0, 
    (char *)0, (char *)0, (char *)0, (char *)0, (char *)0, (char *)0, 
    (char *)0, (char *)0, (char *)0);
    if (sqlca.sqlcode < 0) 
      goto bail_out;
  }
/* # line 642 "myingres.sc" */	/* host code */
   }   
/* # line 646 "myingres.sc" */	/* host code */
   dbconn = (INGconn *)malloc(sizeof(INGconn));
   memset(dbconn, 0, sizeof(INGconn));
   bstrncpy(dbconn->dbname, ingdbname, sizeof(dbconn->dbname));
   bstrncpy(dbconn->user, ingdbuser, sizeof(dbconn->user));
   bstrncpy(dbconn->password, ingdbpasswd, sizeof(dbconn->password));
   dbconn->session_id = sess_id;
   dbconn->msg = (char *)malloc(257);
   memset(dbconn->msg, 0, 257);
   /*
    * Switch to no default session for this thread undo default settings from SQL CONNECT.
    */
/* # line 659 "myingres.sc" */	/* set_sql */
  {
    IILQssSetSqlio(11,(short *)0,1,30,sizeof(-97),(void *)IILQint(-97));
  }
/* # line 661 "myingres.sc" */	/* host code */
bail_out:
   return dbconn;
}
void INGsetDefaultLockingMode(INGconn *dbconn)
{
   /*
    * Set the default Ingres session locking mode:
    *
    * SET LOCKMODE provides four different parameters to govern
    * the nature of locking in an INGRES session:
    *
    * Level: This refers to the level of granularity desired when
    * the table is accessed. You can specify any of the following
    * locking levels:
    *
    * page    Specifies locking at the level of the data page (subject to
    *         escalation criteria; see below)
    * table   Specifies table-level locking in the database
    * session Specifies the current default for your INGRES session
    * system  Specifies that INGRES will start with page-level locking,
    *         unless it estimates that more than Maxlocks pages will be
    *         referenced, in which case table-level locking will be used.
    *
    * Readlock: This refers to locking in situations where table access
    *           is required for reading data only (as opposed to updating
    *           data). You can specify any of the following Readlock modes:
    *
    *    nolock     Specifies no locking when reading data
    *    shared     Specifies the default mode of locking when reading data
    *    exclusive  Specifies exclusive locking when reading data (useful in
    *               "select-for-update" processing within a multi-statement
    *               transaction)
    *    system     Specifies the general Readlock default for the INGRES system
    *
    * Maxlocks: This refers to an escalation factor, or number of locks on
    *           data pages, at which locking escalates from page-level
    *           to table-level. The number of locks available to you is
    *           dependent upon your system configuration. You can specify the
    *           following Maxlocks escalation factors:
    *
    *    n       A specific (integer) number of page locks to allow before
    *            escalating to table-level locking. The default "n" is 10,
    *            and "n" must be greater than 0.
    *    session Specifies the current Maxlocks default for your INGRES
    *            session
    *    system  Specifies the general Maxlocks default for the INGRES system
    *
    * Note: If you specify page-level locking, and the number of locks granted
    * during a query exceeds the system-wide lock limit, or if the operating
    * system's locking resources are depleted, locking escalates to table-level.
    * This escalation occurs automatically and is independent of the user.
    *
    * Timeout: This refers to a time limit, expressed in seconds, for which
    * a lock request should remain pending. If INGRES cannot grant the lock
    * request within the specified time, then the query that requested the
    * lock aborts. You can specify the following timeout characteristics:
    *
    *    n       A specific (integer) number of seconds to wait for a lock
    *            (setting "n" to 0 requires INGRES to wait indefinitely for
    *            the lock)
    *    session Specifies the current timeout default for your INGRES
    *            session (which is also the INGRES default)
    *    system  Specifies the general timeout default for the INGRES system
    *
    */
/* # line 727 "myingres.sc" */	
  
  int sess_id;
/* # line 729 "myingres.sc" */	
  
   if (dbconn != NULL) {
      sess_id = dbconn->session_id;
      /*
       * Switch to the correct default session for this thread.
       */
/* # line 736 "myingres.sc" */	/* set_sql */
  {
    IILQssSetSqlio(11,(short *)0,1,30,sizeof(sess_id),&sess_id);
  }
/* # line 738 "myingres.sc" */	/* set */
  {
    IIsqInit(&sqlca);
    IIwritio(0,(short *)0,1,32,0,(char *)
"set LOCKMODE session where level=system, readlock=nolock");
    IIsyncup((char *)0,0);
  }
/* # line 740 "myingres.sc" */	/* host code */
      /*
       * Switch to no default session for this thread.
       */
/* # line 743 "myingres.sc" */	/* set_sql */
  {
    IILQssSetSqlio(11,(short *)0,1,30,sizeof(-97),(void *)IILQint(-97));
  }
/* # line 744 "myingres.sc" */	/* host code */
   }
}
void INGdisconnectDB(INGconn *dbconn)
{
/* # line 749 "myingres.sc" */	
  
  int sess_id;
/* # line 751 "myingres.sc" */	
  
   if (dbconn != NULL) {
      sess_id = dbconn->session_id;
/* # line 755 "myingres.sc" */	/* disconnect */
  {
    IIsqInit(&sqlca);
    IILQsidSessID(sess_id);
    IIsqDisconnect();
  }
/* # line 757 "myingres.sc" */	/* host code */
      free(dbconn->msg);
      free(dbconn);
   }
}
char *INGerrorMessage(const INGconn *dbconn)
{
/* # line 764 "myingres.sc" */	
  
  char errbuf[256];
/* # line 766 "myingres.sc" */	
  
/* # line 768 "myingres.sc" */	/* inquire_ingres */
  {
    IILQisInqSqlio((short *)0,1,32,255,errbuf,63);
  }
/* # line 769 "myingres.sc" */	/* host code */
   strncpy(dbconn->msg, errbuf, sizeof(dbconn->msg));
   return dbconn->msg;
}
char *INGcmdTuples(INGresult *res)
{
   return res->numrowstring;
}
/* TODO?
int INGputCopyEnd(INGconn *dbconn, const char *errormsg);
int INGputCopyData(INGconn *dbconn, const char *buffer, int nbytes);
*/
/* # line 783 "myingres.sc" */	
#endif
