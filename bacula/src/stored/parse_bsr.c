/*     
 *   Parse a Bootstrap Records (used for restores) 
 *  
 *     Kern Sibbald, June MMII
 *
 *   Version $Id$
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


#include "bacula.h"
#include "stored.h"

typedef BSR * (ITEM_HANDLER)(LEX *lc, BSR *bsr);

static BSR *store_vol(LEX *lc, BSR *bsr);
static BSR *store_client(LEX *lc, BSR *bsr);
static BSR *store_job(LEX *lc, BSR *bsr);
static BSR *store_jobid(LEX *lc, BSR *bsr);
static BSR *store_jobtype(LEX *lc, BSR *bsr);
static BSR *store_joblevel(LEX *lc, BSR *bsr);
static BSR *store_file_index(LEX *lc, BSR *bsr);
static BSR *store_sessid(LEX *lc, BSR *bsr);
static BSR *store_sesstime(LEX *lc, BSR *bsr);
static BSR *store_include(LEX *lc, BSR *bsr);
static BSR *store_exclude(LEX *lc, BSR *bsr);

struct kw_items {
   char *name;
   ITEM_HANDLER *handler;
};

struct kw_items items[] = {
   {"volume", store_vol},
   {"client", store_client},
   {"job", store_job},
   {"jobid", store_jobid},
   {"fileindex", store_file_index},
   {"jobtype", store_jobtype},
   {"joblevel", store_joblevel},
   {"volsessionid", store_sessid},
   {"volsessiontime", store_sesstime},
   {"include", store_include},
   {"exclude", store_exclude},
   {NULL, NULL}

};

static BSR *new_bsr() 
{
   BSR *bsr = (BSR *)malloc(sizeof(BSR));
   memset(bsr, 0, sizeof(BSR));
   return bsr;
}

/*********************************************************************
 *
 *	Parse Bootstrap file
 *
 */
BSR *parse_bsr(char *cf)
{
   LEX *lc = NULL;
   int token, i;
   BSR *root_bsr = new_bsr();
   BSR *bsr = root_bsr;

   Dmsg0(200, "Enter parse_bsf()\n");
   lc = lex_open_file(lc, cf);
   while ((token=lex_get_token(lc)) != T_EOF) {
      Dmsg1(150, "parse got token=%s\n", lex_tok_to_str(token));
      if (token == T_EOL) {
	 continue;
      }
      if (token != T_IDENTIFIER) {
         scan_err1(lc, "Expected a keyword identifier, got: %s", lc->str);
      }
      for (i=0; items[i].name; i++) {
	 if (strcasecmp(items[i].name, lc->str) == 0) {
	    token = lex_get_token(lc);
            Dmsg1 (150, "in T_IDENT got token=%s\n", lex_tok_to_str(token));
	    if (token != T_EQUALS) {
               scan_err1(lc, "expected an equals, got: %s", lc->str);
	    }
            Dmsg1(150, "calling handler for %s\n", items[i].name);
	    /* Call item handler */
	    bsr = items[i].handler(lc, bsr);
	    i = -1;
	    break;
	 }
      }
      if (i >= 0) {
         Dmsg1(150, "Keyword = %s\n", lc->str);
         scan_err1(lc, "Keyword %s not found", lc->str);
      }

   }
   lc = lex_close_file(lc);
   Dmsg0(200, "Leave parse_bsf()\n");
   return root_bsr;
}

static BSR *store_vol(LEX *lc, BSR *bsr)
{
   int token;
    
   token = lex_get_token(lc);
   if (token != T_IDENTIFIER && token != T_STRING && token != T_QUOTED_STRING) {
      scan_err1(lc, "expected an identifier or string, got: %s", lc->str);
   } else if (lc->str_len > MAX_RES_NAME_LENGTH) {
      scan_err3(lc, "name %s length %d too long, max is %d\n", lc->str, 
	 lc->str_len, MAX_RES_NAME_LENGTH);
   } else {
      if (bsr->VolumeName) {
	 bsr->next = new_bsr();
	 bsr = bsr->next;
      }
      bsr->VolumeName = bstrdup(lc->str);
   }
   scan_to_eol(lc);
   return bsr;
}

static BSR *store_client(LEX *lc, BSR *bsr)
{
   int token;
   BSR_CLIENT *client;
    
   token = lex_get_token(lc);
   if (token != T_IDENTIFIER && token != T_STRING && token != T_QUOTED_STRING) {
      scan_err1(lc, "expected an identifier or string, got: %s", lc->str);
   } else if (lc->str_len > MAX_RES_NAME_LENGTH) {
      scan_err3(lc, "name %s length %d too long, max is %d\n", lc->str, 
	 lc->str_len, MAX_RES_NAME_LENGTH);
   } else {
      client = (BSR_CLIENT *)malloc(sizeof(BSR_CLIENT));
      memset(client, 0, sizeof(BSR_CLIENT));
      client->ClientName = bstrdup(lc->str);
      /* Add it to the end of the client chain */
      if (!bsr->client) {
	 bsr->client = client;
      } else {
	 BSR_CLIENT *bc = bsr->client;
	 for ( ;; ) {
	    if (bc->next) {
	       bc = bc->next;
	    } else {
	       bc->next = client;
	       break;
	    }
	 }
      }
   }
   scan_to_eol(lc);
   return bsr;
}

static BSR *store_job(LEX *lc, BSR *bsr)
{
   int token;
   BSR_JOB *job;
    
   token = lex_get_token(lc);
   if (token != T_IDENTIFIER && token != T_STRING && token != T_QUOTED_STRING) {
      scan_err1(lc, "expected an identifier or string, got: %s", lc->str);
   } else if (lc->str_len > MAX_RES_NAME_LENGTH) {
      scan_err3(lc, "name %s length %d too long, max is %d\n", lc->str, 
	 lc->str_len, MAX_RES_NAME_LENGTH);
   } else {
      job = (BSR_JOB *)malloc(sizeof(BSR_JOB));
      memset(job, 0, sizeof(BSR_JOB));
      job->Job = bstrdup(lc->str);
      /* Add it to the end of the client chain */
      if (!bsr->job) {
	 bsr->job = job;
      } else {
	 /* Add to end of chain */
	 BSR_JOB *bc = bsr->job;
	 for ( ;bc->next; bc=bc->next)
	    { }
	 bc->next = job;
      }
   }
   scan_to_eol(lc);
   return bsr;
}

static BSR *store_file_index(LEX *lc, BSR *bsr)
{
   int token;
   int32_t FileIndex;
   BSR_FINDEX *findex;


   token = lex_get_token(lc);
   if (token != T_NUMBER || !is_a_number(lc->str)) {
      scan_err1(lc, "expected a positive integer number, got: %s", lc->str);
   } else {
      errno = 0;
      FileIndex = strtoul(lc->str, NULL, 10);
      if (errno != 0) {
         scan_err1(lc, "expected a integer number, got: %s", lc->str);
      }
      findex = (BSR_FINDEX *)malloc(sizeof(BSR_FINDEX));
      memset(findex, 0, sizeof(BSR_FINDEX));
      findex->FileIndex = FileIndex;
      /* Add it to the end of the chain */
      if (!bsr->FileIndex) {
	 bsr->FileIndex = findex;
      } else {
	 /* Add to end of chain */
	 BSR_FINDEX *bs = bsr->FileIndex;
	 for ( ;bs->next; bs=bs->next)
	   {  }
	 bs->next = findex;
      }
   }
   scan_to_eol(lc);
   return bsr;
}


static BSR *store_jobid(LEX *lc, BSR *bsr)
{
   int token;
   uint32_t JobId;    
   BSR_JOBID *jobid;


   token = lex_get_token(lc);
   if (token != T_NUMBER || !is_a_number(lc->str)) {
      scan_err1(lc, "expected a positive integer number, got: %s", lc->str);
   } else {
      errno = 0;
      JobId = strtoul(lc->str, NULL, 10);
      if (errno != 0) {
         scan_err1(lc, "expected a integer number, got: %s", lc->str);
      }
      jobid = (BSR_JOBID *)malloc(sizeof(BSR_JOBID));
      memset(jobid, 0, sizeof(BSR_JOBID));
      jobid->JobId = JobId;
      /* Add it to the end of the chain */
      if (!bsr->JobId) {
	 bsr->JobId = jobid;
      } else {
	 /* Add to end of chain */
	 BSR_JOBID *bs = bsr->JobId;
	 for ( ;bs->next; bs=bs->next)
	   {  }
	 bs->next = jobid;
      }
   }
   scan_to_eol(lc);
   return bsr;
}

static BSR *store_jobtype(LEX *lc, BSR *bsr)
{
   /* *****FIXME****** */
   Dmsg0(-1, "JobType not yet implemented\n");
   return bsr;
}


static BSR *store_joblevel(LEX *lc, BSR *bsr)
{
   /* *****FIXME****** */
   Dmsg0(-1, "JobLevel not yet implemented\n");
   return bsr;
}



static BSR *store_sessid(LEX *lc, BSR *bsr)
{
   int token;
   uint32_t sessid1;
   BSR_SESSID *sid;


   token = lex_get_token(lc);
   if (token != T_NUMBER || !is_a_number(lc->str)) {
      scan_err1(lc, "expected a positive integer number, got: %s", lc->str);
   } else {
      errno = 0;
      sessid1 = strtoul(lc->str, NULL, 10);
      if (errno != 0) {
         scan_err1(lc, "expected a integer number, got: %s", lc->str);
      }
      sid = (BSR_SESSID *)malloc(sizeof(BSR_SESSID));
      memset(sid, 0, sizeof(BSR_SESSID));
      sid->sessid1 = sessid1;
      /* Add it to the end of the chain */
      if (!bsr->sessid) {
	 bsr->sessid = sid;
      } else {
	 /* Add to end of chain */
	 BSR_SESSID *bs = bsr->sessid;
	 for ( ;bs->next; bs=bs->next)
	   {  }
	 bs->next = sid;
      }
   }
   scan_to_eol(lc);
   return bsr;
}

static BSR *store_sesstime(LEX *lc, BSR *bsr)
{
   int token;
   uint32_t sesstime;
   BSR_SESSTIME *stime;


   token = lex_get_token(lc);
   if (token != T_NUMBER || !is_a_number(lc->str)) {
      scan_err1(lc, "expected a positive integer number, got: %s", lc->str);
   } else {
      errno = 0;
      sesstime = strtoul(lc->str, NULL, 10);
      if (errno != 0) {
         scan_err1(lc, "expected a integer number, got: %s", lc->str);
      }
      stime = (BSR_SESSTIME *)malloc(sizeof(BSR_SESSTIME));
      memset(stime, 0, sizeof(BSR_SESSTIME));
      stime->sesstime = sesstime;
      /* Add it to the end of the chain */
      if (!bsr->sesstime) {
	 bsr->sesstime = stime;
      } else {
	 /* Add to end of chain */
	 BSR_SESSTIME *bs = bsr->sesstime;
	 for ( ;bs->next; bs=bs->next)
	    { }
	 bs->next = stime;
      }
   }
   scan_to_eol(lc);
   return bsr;
}

static BSR *store_include(LEX *lc, BSR *bsr)
{
   scan_to_eol(lc);
   return bsr;
}

static BSR *store_exclude(LEX *lc, BSR *bsr)
{
   scan_to_eol(lc);
   return bsr;
}

void dump_bsr(BSR *bsr)
{
   if (!bsr) {
      Dmsg0(-1, "BSR is NULL\n");
      return;
   }
   Dmsg8(-1,   
"Next        : 0x%x\n"
"VolumeName  : %s\n"
"Client      : %s\n"
"Job         : %s\n"
"JobId       : %u\n"
"SessId      : %u\n"
"SessTime    : %u\n"
"FileIndex   : %d\n",
		 bsr->next,
                 bsr->VolumeName ? bsr->VolumeName : "*None*",
                 bsr->client ? bsr->client->ClientName : "*None*",
                 bsr->job ? bsr->job->Job : "*None*",
		 bsr->JobId ? bsr->JobId->JobId : 0,
		 bsr->sessid ? bsr->sessid->sessid1 : 0,
		 bsr->sesstime ? bsr->sesstime->sesstime : 0,
		 bsr->FileIndex ? bsr->FileIndex->FileIndex : 0);
   if (bsr->next) {
      Dmsg0(-1, "\n");
      dump_bsr(bsr->next);
   }
}


/*********************************************************************
 *
 *	Free bsr resources
 */

static void free_bsr_item(BSR *bsr)
{
   if (!bsr) {
      return;
   }
   free_bsr_item(bsr->next);
   free(bsr);
}

void free_bsr(BSR *bsr)
{
   if (!bsr) {
      return;
   }
   free_bsr_item((BSR *)bsr->client);
   free_bsr_item((BSR *)bsr->sessid);
   free_bsr_item((BSR *)bsr->sesstime);
   if (bsr->VolumeName) {
      free(bsr->VolumeName);
   }
   free_bsr(bsr->next);
   free(bsr);
}
