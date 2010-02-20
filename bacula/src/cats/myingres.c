
#include "bacula.h"
#include "cats.h"

#ifdef HAVE_INGRES
#include <eqdefc.h>
#include <eqsqlca.h>
    extern IISQLCA sqlca;   /* SQL Communications Area */
#include <eqsqlda.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "myingres.h"
/* ---Implementations--- */
int INGcheck()
{
    return (sqlca.sqlcode < 0) ? sqlca.sqlcode : 0;
}
short INGgetCols(const char *stmt)
{
    short number = 1;
    IISQLDA *sqlda;
    sqlda = (IISQLDA *)calloc(1,IISQDA_HEAD_SIZE + (number * IISQDA_VAR_SIZE));
    if (sqlda == (IISQLDA *)0)
        { printf("Failure allocating %d SQLDA elements\n",number); }
    sqlda->sqln = number;
/* # line 29 "myingres.sc" */	
  
  char *stmtd;
/* # line 31 "myingres.sc" */	
  
    stmtd = (char*)malloc(strlen(stmt)+1);
    strncpy(stmtd,stmt,strlen(stmt)+1);
/* # line 36 "myingres.sc" */	/* prepare */
  {
    IIsqInit(&sqlca);
    IIsqPrepare(0,(char *)"s1",(char *)0,0,stmtd);
  }
/* # line 37 "myingres.sc" */	/* host code */
    if (INGcheck() < 0 ) { free(stmtd); free(sqlda); return -1; }
/* # line 38 "myingres.sc" */	/* describe */
  {
    IIsqInit(&sqlca);
    IIsqDescribe(0,(char *)"s1",sqlda,0);
  }
/* # line 39 "myingres.sc" */	/* host code */
    if (INGcheck() < 0 ) { free(stmtd); free(sqlda); return -1; }
    number = sqlda->sqld;
    free(stmtd); free(sqlda);
    return number;
}
IISQLDA *INGgetDescriptor(short numCols, const char *stmt)
{
    IISQLDA *sqlda;
    sqlda = (IISQLDA *)calloc(1,IISQDA_HEAD_SIZE + (numCols * IISQDA_VAR_SIZE));
    if (sqlda == (IISQLDA *)0) 
        { printf("Failure allocating %d SQLDA elements\n",numCols); }
    sqlda->sqln = numCols;
/* # line 55 "myingres.sc" */	
  
  char *stmtd;
/* # line 57 "myingres.sc" */	
  
    stmtd = (char *)malloc(strlen(stmt)+1);
    strncpy(stmtd,stmt,strlen(stmt)+1);
/* # line 62 "myingres.sc" */	/* prepare */
  {
    IIsqInit(&sqlca);
    IIsqPrepare(0,(char *)"s2",sqlda,0,stmtd);
  }
/* # line 64 "myingres.sc" */	/* host code */
    free(stmtd);
    int i;
    for (i=0;i<sqlda->sqld;++i)
    {
	/* alloc space for variable like indicated in sqllen */
	/* for date types sqllen is always 0 -> allocate by type */
	switch (abs(sqlda->sqlvar[i].sqltype))
	{
	    case IISQ_TSW_TYPE:
		sqlda->sqlvar[i].sqldata =
		    (char *)malloc(IISQ_TSW_LEN);
		break;
	    case IISQ_TSWO_TYPE:
		sqlda->sqlvar[i].sqldata =
		    (char *)malloc(IISQ_TSWO_LEN);
		break;
	    case IISQ_TSTMP_TYPE:
		sqlda->sqlvar[i].sqldata =
		    (char *)malloc(IISQ_TSTMP_LEN);
		break;
	    default:
		sqlda->sqlvar[i].sqldata =
		    (char *)malloc(sqlda->sqlvar[i].sqllen);
	}
    }
    return sqlda;
}
void INGfreeDescriptor(IISQLDA *sqlda)
{
    int i;
    for ( i = 0 ; i < sqlda->sqld ; ++i )
    {
        free(sqlda->sqlvar[i].sqldata);
        free(sqlda->sqlvar[i].sqlind);
    }
    free(sqlda);
    sqlda = NULL;
}
int INGgetTypeSize(IISQLVAR *ingvar)
{
    int inglength = 0;
        switch (ingvar->sqltype)
    {
    /* TODO: add date types (at least TSTMP,TSW TSWO) */
		case IISQ_DTE_TYPE:
	    	inglength = 25;
		    break;
		case IISQ_MNY_TYPE:
		    inglength = 8;
		    break;
		default:
		    inglength = ingvar->sqllen;
    }
        return inglength;
}
INGresult *INGgetINGresult(IISQLDA *sqlda)
{
    INGresult *result = NULL;
    result = (INGresult *)calloc(1, sizeof(INGresult));
    if (result == (INGresult *)0) 
        { printf("Failure allocating INGresult\n"); }
    result->sqlda       = sqlda;
    result->num_fields  = sqlda->sqld;
    result->num_rows    = 0;
    result->first_row   = NULL;
    result->status      = ING_EMPTY_RESULT;
    result->act_row     = NULL;
    strcpy(result->numrowstring,"");
    if (result->num_fields) {
        result->fields = (INGRES_FIELD *)malloc(sizeof(INGRES_FIELD) * result->num_fields);
        memset(result->fields, 0, sizeof(INGRES_FIELD) * result->num_fields);
        int i;
        for (i=0;i<result->num_fields;++i)
        {
	    memset(result->fields[i].name, 0, 34);
	    strncpy(result->fields[i].name,
	        sqlda->sqlvar[i].sqlname.sqlnamec,
	        sqlda->sqlvar[i].sqlname.sqlnamel);
	    result->fields[i].max_length 	= INGgetTypeSize(&sqlda->sqlvar[i]);
	    result->fields[i].type 		= abs(sqlda->sqlvar[i].sqltype);
	    result->fields[i].flags		= (abs(sqlda->sqlvar[i].sqltype)<0) ? 1 : 0;
        }
    }
    return result;
}
void INGfreeINGresult(INGresult *ing_res)
{
    /* free all rows and fields, then res, not descriptor! */
    if( ing_res != NULL )
    {
        /* use of rows is a nasty workaround til I find the reason,
           why aggregates like max() don't work
         */
        int rows = ing_res->num_rows;
        ING_ROW *rowtemp;
        ing_res->act_row = ing_res->first_row;
        while (ing_res->act_row != NULL && rows > 0)
        {
            rowtemp = ing_res->act_row->next;
            INGfreeRowSpace(ing_res->act_row, ing_res->sqlda);
            ing_res->act_row = rowtemp;
            --rows;
        }
        free(ing_res->fields);
    }
    free(ing_res);
    ing_res = NULL;
}
ING_ROW *INGgetRowSpace(INGresult *ing_res)
{
    IISQLDA *sqlda = ing_res->sqlda;
    ING_ROW *row = NULL;
    IISQLVAR *vars = NULL;
    row = (ING_ROW *)calloc(1,sizeof(ING_ROW));
    if (row == (ING_ROW *)0)
        { printf("Failure allocating ING_ROW\n"); }
    vars = (IISQLVAR *)calloc(1,sizeof(IISQLVAR) * sqlda->sqld);
    if (vars == (IISQLVAR *)0)
        { printf("Failure allocating %d SQLVAR elements\n",sqlda->sqld); }
    row->sqlvar = vars;
    row->next = NULL;
    int i;
    unsigned short len; /* used for VARCHAR type length */
    for (i=0;i<sqlda->sqld;++i)
    {
	/* make strings out of the data, then the space and assign 
	    (why string? at least it seems that way, looking into the sources)
	 */
	switch (ing_res->fields[i].type)
	{
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
		switch (sqlda->sqlvar[i].sqllen)
		{
		    case 2:
			snprintf(vars[i].sqldata, 20, "%d",*(short*)sqlda->sqlvar[i].sqldata);
			break;
		    case 4:
			snprintf(vars[i].sqldata, 20, "%d",*(int*)sqlda->sqlvar[i].sqldata);
			break;
		    case 8:
			snprintf(vars[i].sqldata, 20, "%d",*(long*)sqlda->sqlvar[i].sqldata);
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
	vars[i].sqlind = (short *)malloc(sizeof(short));
	memcpy(vars[i].sqlind,sqlda->sqlvar[i].sqlind,sizeof(short));
    }
    return row;
}
void INGfreeRowSpace(ING_ROW *row, IISQLDA *sqlda)
{
    int i;
    if (row == NULL || sqlda == NULL) {	return; }
    for ( i = 0 ; i < sqlda->sqld ; ++i )
    {
        free(row->sqlvar[i].sqldata);
        free(row->sqlvar[i].sqlind);
    }
    free(row->sqlvar);
    free(row);
}
int INGfetchAll(const char *stmt, INGresult *ing_res)
{
    int linecount = 0;
    ING_ROW *row;
    IISQLDA *desc;
    int check = -1;
    desc = ing_res->sqlda;
/* # line 283 "myingres.sc" */	/* host code */
    if ((check = INGcheck()) < 0) { return check; }
/* # line 285 "myingres.sc" */	/* open */
  {
    IIsqInit(&sqlca);
    IIcsOpen((char *)"c2",31710,6267);
    IIwritio(0,(short *)0,1,32,0,(char *)"s2");
    IIcsQuery((char *)"c2",31710,6267);
  }
/* # line 286 "myingres.sc" */	/* host code */
    if ((check = INGcheck()) < 0) { return check; }
    /* for (linecount=0;sqlca.sqlcode==0;++linecount) */
    while(sqlca.sqlcode==0)
    {
/* # line 291 "myingres.sc" */	/* fetch */
  {
    IIsqInit(&sqlca);
    if (IIcsRetScroll((char *)"c2",31710,6267,-1,-1) != 0) {
      IIcsDaGet(0,desc);
      IIcsERetrieve();
    } /* IIcsRetrieve */
  }
/* # line 292 "myingres.sc" */	/* host code */
        if ((check = INGcheck()) < 0) { return check;}
	if (sqlca.sqlcode == 0)
	{
	    row = INGgetRowSpace(ing_res); /* alloc space for fetched row */
	    /* initialize list when encountered first time */
	    if (ing_res->first_row == 0) 
    	    {
		ing_res->first_row = row; /* head of the list */
		ing_res->first_row->next = NULL;
		ing_res->act_row = ing_res->first_row;
	    }	
    	    ing_res->act_row->next = row; /* append row to old act_row */
    	    ing_res->act_row = row; /* set row as act_row */
	    row->row_number = linecount;
	    ++linecount;
	}
    }
/* # line 312 "myingres.sc" */	/* close */
  {
    IIsqInit(&sqlca);
    IIcsClose((char *)"c2",31710,6267);
  }
/* # line 314 "myingres.sc" */	/* host code */
    ing_res->status = ING_COMMAND_OK;
    ing_res->num_rows = linecount;
    return linecount;
}
ING_STATUS INGresultStatus(INGresult *res)
{
    if (res == NULL) {return ING_NO_RESULT;}
    return res->status;
}
void INGrowSeek(INGresult *res, int row_number)
{
    if (res->act_row->row_number == row_number) { return; }
    /* TODO: real error handling */
    if (row_number<0 || row_number>res->num_rows) { return;}
    ING_ROW *trow = res->first_row;
    while ( trow->row_number != row_number )
    { trow = trow->next; }
    res->act_row = trow;
    /* note - can be null - if row_number not found, right? */
}
char *INGgetvalue(INGresult *res, int row_number, int column_number)
{
    if (row_number != res->act_row->row_number)
        { INGrowSeek(res, row_number); }
    return res->act_row->sqlvar[column_number].sqldata;
}
int INGgetisnull(INGresult *res, int row_number, int column_number)
{
    if (row_number != res->act_row->row_number)
        { INGrowSeek(res, row_number); }
    return (short)*res->act_row->sqlvar[column_number].sqlind;
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
    if ( (column_number > res->num_fields) || (column_number < 0) )
        { return NULL; }
    else
        { return res->fields[column_number].name; }
}
short INGftype(const INGresult *res, int column_number)
{
    return res->fields[column_number].type;
}
INGresult *INGexec(INGconn *conn, const char *query)
{
    int check;
/* # line 379 "myingres.sc" */	
  
  int rowcount;
  char *stmt;
/* # line 382 "myingres.sc" */	
  
    stmt = (char *)malloc(strlen(query)+1);
    strncpy(stmt,query,strlen(query)+1);
    rowcount = -1;
/* # line 388 "myingres.sc" */	/* execute */
  {
    IIsqInit(&sqlca);
    IIsqExImmed(stmt);
    IIsyncup((char *)0,0);
  }
/* # line 389 "myingres.sc" */	/* host code */
    free(stmt);
    if ((check = INGcheck()) < 0) { return check; }
/* # line 392 "myingres.sc" */	/* inquire_ingres */
  {
    IILQisInqSqlio((short *)0,1,30,sizeof(rowcount),&rowcount,8);
  }
/* # line 393 "myingres.sc" */	/* host code */
    if ((check = INGcheck()) < 0) { return check; }
    return rowcount;
}
INGresult *INGquery(INGconn *conn, const char *query)
{
    /* TODO: error handling */
    IISQLDA *desc = NULL;
    INGresult *res = NULL;
    int rows = -1;
    int cols = INGgetCols(query);
    desc = INGgetDescriptor(cols, query);
    res = INGgetINGresult(desc);
    rows = INGfetchAll(query, res);
    if (rows < 0)
    {
      INGfreeINGresult(res);
      return NULL;
    }
    return res;
}
void INGclear(INGresult *res)
{
    if (res == NULL) { return; }
    IISQLDA *desc = res->sqlda;
    INGfreeINGresult(res);
    INGfreeDescriptor(desc);
}
INGconn *INGconnectDB(char *dbname, char *user, char *passwd)
{
    if (dbname == NULL || strlen(dbname) == 0)
	{ return NULL; }
    INGconn *dbconn = (INGconn *)malloc(sizeof(INGconn));
    memset(dbconn, 0, sizeof(INGconn));
/* # line 434 "myingres.sc" */	
  
  char ingdbname[24];
  char ingdbuser[32];
  char ingdbpasw[32];
  char conn_name[32];
  int sess_id;
/* # line 440 "myingres.sc" */	
  
    strcpy(ingdbname,dbname);
    if ( user != NULL)
    {
        strcpy(ingdbuser,user);
	if ( passwd != NULL)
	    { strcpy(ingdbpasw,passwd); }
	else
	    { strcpy(ingdbpasw, ""); }
/* # line 451 "myingres.sc" */	/* connect */
  {
    IIsqInit(&sqlca);
    IIsqUser(ingdbuser);
    IIsqConnect(0,ingdbname,(char *)"-dbms_password",ingdbpasw,(char *)0, 
    (char *)0, (char *)0, (char *)0, (char *)0, (char *)0, (char *)0, 
    (char *)0, (char *)0, (char *)0, (char *)0);
  }
/* # line 455 "myingres.sc" */	/* host code */
    }
    else
    {
/* # line 458 "myingres.sc" */	/* connect */
  {
    IIsqInit(&sqlca);
    IIsqConnect(0,ingdbname,(char *)0, (char *)0, (char *)0, (char *)0, 
    (char *)0, (char *)0, (char *)0, (char *)0, (char *)0, (char *)0, 
    (char *)0, (char *)0, (char *)0);
  }
/* # line 459 "myingres.sc" */	/* host code */
    }   
/* # line 461 "myingres.sc" */	/* inquire_sql */
  {
    IILQisInqSqlio((short *)0,1,32,31,conn_name,13);
  }
/* # line 462 "myingres.sc" */	/* inquire_sql */
  {
    IILQisInqSqlio((short *)0,1,30,sizeof(sess_id),&sess_id,11);
  }
/* # line 464 "myingres.sc" */	/* host code */
    strcpy(dbconn->dbname,ingdbname);
    strcpy(dbconn->user,ingdbuser);
    strcpy(dbconn->password,ingdbpasw);
    strcpy(dbconn->connection_name,conn_name);
    dbconn->session_id = sess_id;
    dbconn->msg = (char*)malloc(257);
    memset(dbconn->msg,0,257);
    return dbconn;
}
void INGdisconnectDB(INGconn *dbconn)
{
    /* TODO: check for any real use of dbconn: maybe whenn multithreaded? */
/* # line 478 "myingres.sc" */	/* disconnect */
  {
    IIsqInit(&sqlca);
    IIsqDisconnect();
  }
/* # line 479 "myingres.sc" */	/* host code */
    if (dbconn != NULL) {
       free(dbconn->msg);
       free(dbconn);
    }
}
char *INGerrorMessage(const INGconn *conn)
{
/* # line 487 "myingres.sc" */	
  
  char errbuf[256];
/* # line 489 "myingres.sc" */	
  
/* # line 490 "myingres.sc" */	/* inquire_ingres */
  {
    IILQisInqSqlio((short *)0,1,32,255,errbuf,63);
  }
/* # line 491 "myingres.sc" */	/* host code */
    memcpy(conn->msg,&errbuf,256);
    return conn->msg;
}
char    *INGcmdTuples(INGresult *res)
{
    return res->numrowstring;
}
/*      TODO?
int INGputCopyEnd(INGconn *conn, const char *errormsg);
int INGputCopyData(INGconn *conn, const char *buffer, int nbytes);
*/
/* # line 505 "myingres.sc" */	
#endif
