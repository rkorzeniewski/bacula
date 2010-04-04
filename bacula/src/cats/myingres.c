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
 *    Stefan Reddig, June 2009
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
int INGcheck()
{
   return (sqlca.sqlcode < 0) ? sqlca.sqlcode : 0;
}
short INGgetCols(INGconn *conn, const char *query)
{
/* # line 57 "myingres.sc" */	
  
  int sess_id;
  char *stmt;
/* # line 60 "myingres.sc" */	
  
   short number = 1;
   IISQLDA *sqlda;
   sqlda = (IISQLDA *)malloc(IISQDA_HEAD_SIZE + (number * IISQDA_VAR_SIZE));
   memset(sqlda, 0, (IISQDA_HEAD_SIZE + (number * IISQDA_VAR_SIZE)));
   sqlda->sqln = number;
   stmt = bstrdup(query);
   /*
    * Switch to the correct default session for this thread.
    */
   sess_id = conn->session_id;
/* # line 76 "myingres.sc" */	/* set_sql */
  {
    IILQssSetSqlio(11,(short *)0,1,30,sizeof(sess_id),&sess_id);
  }
/* # line 78 "myingres.sc" */	/* prepare */
  {
    IIsqInit(&sqlca);
    IIsqPrepare(0,(char *)"s1",(char *)0,0,stmt);
  }
/* # line 79 "myingres.sc" */	/* host code */
   if (INGcheck() < 0) {
      number = -1;
      goto bail_out;
   }
/* # line 84 "myingres.sc" */	/* describe */
  {
    IIsqInit(&sqlca);
    IIsqDescribe(0,(char *)"s1",sqlda,0);
  }
/* # line 85 "myingres.sc" */	/* host code */
   if (INGcheck() < 0) {
      number = -1;
      goto bail_out;
   }
   number = sqlda->sqld;
bail_out:
   /*
    * Switch to no default session for this thread.
    */
/* # line 96 "myingres.sc" */	/* set_sql */
  {
    IILQssSetSqlio(11,(short *)0,1,30,sizeof(-97),(void *)IILQint(-97));
  }
/* # line 97 "myingres.sc" */	/* host code */
   free(stmt);
   free(sqlda);
   return number;
}
static inline IISQLDA *INGgetDescriptor(short numCols, const char *query)
{
/* # line 104 "myingres.sc" */	
  
  char *stmt;
/* # line 106 "myingres.sc" */	
  
   int i;
   IISQLDA *sqlda;
   sqlda = (IISQLDA *)malloc(IISQDA_HEAD_SIZE + (numCols * IISQDA_VAR_SIZE));
   memset(sqlda, 0, (IISQDA_HEAD_SIZE + (numCols * IISQDA_VAR_SIZE)));
   sqlda->sqln = numCols;
   stmt = bstrdup(query);
/* # line 118 "myingres.sc" */	/* prepare */
  {
    IIsqInit(&sqlca);
    IIsqPrepare(0,(char *)"s2",sqlda,0,stmt);
  }
/* # line 120 "myingres.sc" */	/* host code */
   free(stmt);
   for (i = 0; i < sqlda->sqld; ++i) {
      /*
       * Negative type indicates nullable coulumns, so an indicator
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
   int linecount = 0;
   ING_ROW *row;
   IISQLDA *desc;
   int check = -1;
   desc = ing_res->sqlda;
/* # line 374 "myingres.sc" */	/* host code */
   if ((check = INGcheck()) < 0) {
      return check;
   }
/* # line 378 "myingres.sc" */	/* open */
  {
    IIsqInit(&sqlca);
    IIcsOpen((char *)"c2",22166,10460);
    IIwritio(0,(short *)0,1,32,0,(char *)"s2");
    IIcsQuery((char *)"c2",22166,10460);
  }
/* # line 379 "myingres.sc" */	/* host code */
   if ((check = INGcheck()) < 0) {
      return check;
   }
   /* for (linecount = 0; sqlca.sqlcode == 0; ++linecount) */
   do {
/* # line 385 "myingres.sc" */	/* fetch */
  {
    IIsqInit(&sqlca);
    if (IIcsRetScroll((char *)"c2",22166,10460,-1,-1) != 0) {
      IIcsDaGet(0,desc);
      IIcsERetrieve();
    } /* IIcsRetrieve */
  }
/* # line 387 "myingres.sc" */	/* host code */
      if ( (sqlca.sqlcode == 0) || (sqlca.sqlcode == -40202) ) {
         row = INGgetRowSpace(ing_res); /* alloc space for fetched row */
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
         row->row_number = linecount;
         ++linecount;
      }
   } while ( (sqlca.sqlcode == 0) || (sqlca.sqlcode == -40202) );
/* # line 405 "myingres.sc" */	/* close */
  {
    IIsqInit(&sqlca);
    IIcsClose((char *)"c2",22166,10460);
  }
/* # line 407 "myingres.sc" */	/* host code */
   ing_res->status = ING_COMMAND_OK;
   ing_res->num_rows = linecount;
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
int INGexec(INGconn *conn, const char *query)
{
   int check;
/* # line 488 "myingres.sc" */	
  
  int sess_id;
  int rowcount;
  char *stmt;
/* # line 492 "myingres.sc" */	
  
   stmt = bstrdup(query);
   rowcount = -1;
   /*
    * Switch to the correct default session for this thread.
    */
   sess_id = conn->session_id;
/* # line 501 "myingres.sc" */	/* set_sql */
  {
    IILQssSetSqlio(11,(short *)0,1,30,sizeof(sess_id),&sess_id);
  }
/* # line 502 "myingres.sc" */	/* execute */
  {
    IIsqInit(&sqlca);
    IIsqExImmed(stmt);
    IIsyncup((char *)0,0);
  }
/* # line 504 "myingres.sc" */	/* host code */
   free(stmt);
   if ((check = INGcheck()) < 0) {
      rowcount = check;
      goto bail_out;
   }
/* # line 511 "myingres.sc" */	/* inquire_ingres */
  {
    IILQisInqSqlio((short *)0,1,30,sizeof(rowcount),&rowcount,8);
  }
/* # line 512 "myingres.sc" */	/* host code */
   if ((check = INGcheck()) < 0) {
      rowcount = check;
      goto bail_out;
   }
bail_out:
   /*
    * Switch to no default session for this thread.
    */
/* # line 521 "myingres.sc" */	/* set_sql */
  {
    IILQssSetSqlio(11,(short *)0,1,30,sizeof(-97),(void *)IILQint(-97));
  }
/* # line 522 "myingres.sc" */	/* host code */
   return rowcount;
}
INGresult *INGquery(INGconn *conn, const char *query)
{
   /*
    * TODO: error handling
    */
   IISQLDA *desc = NULL;
   INGresult *res = NULL;
   int rows = -1;
   int cols = INGgetCols(conn, query);
/* # line 534 "myingres.sc" */	
  
  int sess_id;
/* # line 536 "myingres.sc" */	
  
   /*
    * Switch to the correct default session for this thread.
    */
   sess_id = conn->session_id;
/* # line 542 "myingres.sc" */	/* set_sql */
  {
    IILQssSetSqlio(11,(short *)0,1,30,sizeof(sess_id),&sess_id);
  }
/* # line 544 "myingres.sc" */	/* host code */
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
    * Switch to no default session for this thread.
    */
/* # line 567 "myingres.sc" */	/* set_sql */
  {
    IILQssSetSqlio(11,(short *)0,1,30,sizeof(-97),(void *)IILQint(-97));
  }
/* # line 568 "myingres.sc" */	/* host code */
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
INGconn *INGconnectDB(char *dbname, char *user, char *passwd, int session_id)
{
   INGconn *dbconn;
   if (dbname == NULL || strlen(dbname) == 0) {
      return NULL;
   }
   dbconn = (INGconn *)malloc(sizeof(INGconn));
   memset(dbconn, 0, sizeof(INGconn));
/* # line 592 "myingres.sc" */	
  
  char ingdbname[24];
  char ingdbuser[32];
  char ingdbpasswd[32];
  int sess_id;
/* # line 597 "myingres.sc" */	
  
   sess_id = session_id;
   bstrncpy(ingdbname, dbname, sizeof(ingdbname));
   if (user != NULL) {
      bstrncpy(ingdbuser, user, sizeof(ingdbuser));
      if (passwd != NULL) {
         bstrncpy(ingdbpasswd, passwd, sizeof(ingdbpasswd));
      } else {
         memset(ingdbpasswd, 0, sizeof(ingdbpasswd));
      }
/* # line 609 "myingres.sc" */	/* connect */
  {
    IIsqInit(&sqlca);
    IILQsidSessID(sess_id);
    IIsqUser(ingdbuser);
    IIsqConnect(0,ingdbname,(char *)"-dbms_password",ingdbpasswd,(char *)0, 
    (char *)0, (char *)0, (char *)0, (char *)0, (char *)0, (char *)0, 
    (char *)0, (char *)0, (char *)0, (char *)0);
  }
/* # line 614 "myingres.sc" */	/* host code */
   } else {
/* # line 615 "myingres.sc" */	/* connect */
  {
    IIsqInit(&sqlca);
    IILQsidSessID(sess_id);
    IIsqConnect(0,ingdbname,(char *)0, (char *)0, (char *)0, (char *)0, 
    (char *)0, (char *)0, (char *)0, (char *)0, (char *)0, (char *)0, 
    (char *)0, (char *)0, (char *)0);
  }
/* # line 618 "myingres.sc" */	/* host code */
   }   
   if (INGcheck() < 0) {
      return NULL;
   }
   bstrncpy(dbconn->dbname, ingdbname, sizeof(dbconn->dbname));
   bstrncpy(dbconn->user, ingdbuser, sizeof(dbconn->user));
   bstrncpy(dbconn->password, ingdbpasswd, sizeof(dbconn->password));
   dbconn->session_id = sess_id;
   dbconn->msg = (char*)malloc(257);
   memset(dbconn->msg, 0, 257);
   /*
    * Switch to no default session for this thread undo default settings from SQL CONNECT.
    */
/* # line 633 "myingres.sc" */	/* set_sql */
  {
    IILQssSetSqlio(11,(short *)0,1,30,sizeof(-97),(void *)IILQint(-97));
  }
/* # line 635 "myingres.sc" */	/* host code */
   return dbconn;
}
void INGdisconnectDB(INGconn *dbconn)
{
/* # line 640 "myingres.sc" */	
  
  int sess_id;
/* # line 642 "myingres.sc" */	
  
   sess_id = dbconn->session_id;
/* # line 645 "myingres.sc" */	/* disconnect */
  {
    IIsqInit(&sqlca);
    IILQsidSessID(sess_id);
    IIsqDisconnect();
  }
/* # line 647 "myingres.sc" */	/* host code */
   if (dbconn != NULL) {
      free(dbconn->msg);
      free(dbconn);
   }
}
char *INGerrorMessage(const INGconn *conn)
{
/* # line 655 "myingres.sc" */	
  
  char errbuf[256];
/* # line 657 "myingres.sc" */	
  
/* # line 659 "myingres.sc" */	/* inquire_ingres */
  {
    IILQisInqSqlio((short *)0,1,32,255,errbuf,63);
  }
/* # line 660 "myingres.sc" */	/* host code */
   memcpy(conn->msg, &errbuf, 256);
   return conn->msg;
}
char *INGcmdTuples(INGresult *res)
{
   return res->numrowstring;
}
/* TODO?
int INGputCopyEnd(INGconn *conn, const char *errormsg);
int INGputCopyData(INGconn *conn, const char *buffer, int nbytes);
*/
/* # line 674 "myingres.sc" */	
#endif
