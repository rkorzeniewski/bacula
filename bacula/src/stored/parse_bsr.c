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
static BSR *store_findex(LEX *lc, BSR *bsr);
static BSR *store_sessid(LEX *lc, BSR *bsr);
static BSR *store_volfile(LEX *lc, BSR *bsr);
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
   {"fileindex", store_findex},
   {"jobtype", store_jobtype},
   {"joblevel", store_joblevel},
   {"volsessionid", store_sessid},
   {"volsessiontime", store_sesstime},
   {"include", store_include},
   {"exclude", store_exclude},
   {"volfile", store_volfile},
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
   while ((token=lex_get_token(lc, T_ALL)) != T_EOF) {
      Dmsg1(150, "parse got token=%s\n", lex_tok_to_str(token));
      if (token == T_EOL) {
	 continue;
      }
      for (i=0; items[i].name; i++) {
	 if (strcasecmp(items[i].name, lc->str) == 0) {
	    token = lex_get_token(lc, T_ALL);
            Dmsg1 (150, "in T_IDENT got token=%s\n", lex_tok_to_str(token));
	    if (token != T_EQUALS) {
               scan_err1(lc, "expected an equals, got: %s", lc->str);
	    }
            Dmsg1(100, "calling handler for %s\n", items[i].name);
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
    
   token = lex_get_token(lc, T_NAME);
   if (bsr->VolumeName) {
      bsr->next = new_bsr();
      bsr = bsr->next;
   }
   bsr->VolumeName = bstrdup(lc->str);
   scan_to_eol(lc);
   return bsr;
}

static BSR *store_client(LEX *lc, BSR *bsr)
{
   int token;
   BSR_CLIENT *client;
    
   for (;;) {
      token = lex_get_token(lc, T_NAME);
      client = (BSR_CLIENT *)malloc(sizeof(BSR_CLIENT));
      memset(client, 0, sizeof(BSR_CLIENT));
      client->ClientName = bstrdup(lc->str);
      /* Add it to the end of the client chain */
      if (!bsr->client) {
	 bsr->client = client;
      } else {
	 BSR_CLIENT *bc = bsr->client;
	 for ( ;bc->next; bc=bc->next)	
	    { }
	 bc->next = client;
      }
      token = lex_get_token(lc, T_ALL);
      if (token != T_COMMA) {
	 break;
      }
   }
   return bsr;
}

static BSR *store_job(LEX *lc, BSR *bsr)
{
   int token;
   BSR_JOB *job;
    
   for (;;) {
      token = lex_get_token(lc, T_NAME);
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
      token = lex_get_token(lc, T_ALL);
      if (token != T_COMMA) {
	 break;
      }
   }
   return bsr;
}

static BSR *store_findex(LEX *lc, BSR *bsr)
{
   int token;
   BSR_FINDEX *findex;

   for (;;) {
      token = lex_get_token(lc, T_PINT32_RANGE);
      findex = (BSR_FINDEX *)malloc(sizeof(BSR_FINDEX));
      memset(findex, 0, sizeof(BSR_FINDEX));
      findex->findex = lc->pint32_val;
      findex->findex2 = lc->pint32_val2;
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
      token = lex_get_token(lc, T_ALL);
      if (token != T_COMMA) {
	 break;
      }
   }
   return bsr;
}


static BSR *store_jobid(LEX *lc, BSR *bsr)
{
   int token;
   BSR_JOBID *jobid;

   for (;;) {
      token = lex_get_token(lc, T_PINT32_RANGE);
      jobid = (BSR_JOBID *)malloc(sizeof(BSR_JOBID));
      memset(jobid, 0, sizeof(BSR_JOBID));
      jobid->JobId = lc->pint32_val;
      jobid->JobId2 = lc->pint32_val2;
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
      token = lex_get_token(lc, T_ALL);
      if (token != T_COMMA) {
	 break;
      }
   }
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




/*
 * Routine to handle Volume start/end file   
 */
static BSR *store_volfile(LEX *lc, BSR *bsr)
{
   int token;
   BSR_VOLFILE *volfile;

   for (;;) {
      token = lex_get_token(lc, T_PINT32_RANGE);
      volfile = (BSR_VOLFILE *)malloc(sizeof(BSR_VOLFILE));
      memset(volfile, 0, sizeof(BSR_VOLFILE));
      volfile->sfile = lc->pint32_val;
      volfile->efile = lc->pint32_val2;
      /* Add it to the end of the chain */
      if (!bsr->volfile) {
	 bsr->volfile = volfile;
      } else {
	 /* Add to end of chain */
	 BSR_VOLFILE *bs = bsr->volfile;
	 for ( ;bs->next; bs=bs->next)
	    {  }
	 bs->next = volfile;
      }
      token = lex_get_token(lc, T_ALL);
      if (token != T_COMMA) {
	 break;
      }
   }
   return bsr;
}



static BSR *store_sessid(LEX *lc, BSR *bsr)
{
   int token;
   BSR_SESSID *sid;

   for (;;) {
      token = lex_get_token(lc, T_PINT32_RANGE);
      sid = (BSR_SESSID *)malloc(sizeof(BSR_SESSID));
      memset(sid, 0, sizeof(BSR_SESSID));
      sid->sessid = lc->pint32_val;
      sid->sessid2 = lc->pint32_val2;
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
      token = lex_get_token(lc, T_ALL);
      if (token != T_COMMA) {
	 break;
      }
   }
   return bsr;
}

static BSR *store_sesstime(LEX *lc, BSR *bsr)
{
   int token;
   BSR_SESSTIME *stime;

   for (;;) {
      token = lex_get_token(lc, T_PINT32);
      stime = (BSR_SESSTIME *)malloc(sizeof(BSR_SESSTIME));
      memset(stime, 0, sizeof(BSR_SESSTIME));
      stime->sesstime = lc->pint32_val;
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
      token = lex_get_token(lc, T_ALL);
      if (token != T_COMMA) {
	 break;
      }
   }
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

void dump_volfile(BSR_VOLFILE *volfile)
{
   if (volfile) {
      Dmsg2(-1, "VolFile     : %u-%u\n", volfile->sfile, volfile->efile);
      dump_volfile(volfile->next);
   }
}

void dump_findex(BSR_FINDEX *FileIndex)
{
   if (FileIndex) {
      if (FileIndex->findex == FileIndex->findex2) {
         Dmsg1(-1, "FileIndex   : %u\n", FileIndex->findex);
      } else {
         Dmsg2(-1, "FileIndex   : %u-%u\n", FileIndex->findex, FileIndex->findex2);
      }
      dump_findex(FileIndex->next);
   }
}

void dump_jobid(BSR_JOBID *jobid)
{
   if (jobid) {
      if (jobid->JobId == jobid->JobId2) {
         Dmsg1(-1, "JobId       : %u\n", jobid->JobId);
      } else {
         Dmsg2(-1, "JobId       : %u-%u\n", jobid->JobId, jobid->JobId2);
      }
      dump_jobid(jobid->next);
   }
}

void dump_sessid(BSR_SESSID *sessid)
{
   if (sessid) {
      if (sessid->sessid == sessid->sessid2) {
         Dmsg1(-1, "SessId      : %u\n", sessid->sessid);
      } else {
         Dmsg2(-1, "SessId      : %u-%u\n", sessid->sessid, sessid->sessid2);
      }
      dump_sessid(sessid->next);
   }
}


void dump_client(BSR_CLIENT *client)
{
   if (client) {
      Dmsg1(-1, "Client      : %s\n", client->ClientName);
      dump_client(client->next);
   }
}

void dump_job(BSR_JOB *job)
{
   if (job) {
      Dmsg1(-1, "Job          : %s\n", job->Job);
      dump_job(job->next);
   }
}

void dump_sesstime(BSR_SESSTIME *sesstime)
{
   if (sesstime) {
      Dmsg1(-1, "SessTime    : %u\n", sesstime->sesstime);
      dump_sesstime(sesstime->next);
   }
}





void dump_bsr(BSR *bsr)
{
   if (!bsr) {
      Dmsg0(-1, "BSR is NULL\n");
      return;
   }
   Dmsg2(-1,   
"Next        : 0x%x\n"
"VolumeName  : %s\n",
		 bsr->next,
                 bsr->VolumeName ? bsr->VolumeName : "*None*");
   dump_sessid(bsr->sessid);
   dump_sesstime(bsr->sesstime);
   dump_volfile(bsr->volfile);
   dump_client(bsr->client);
   dump_jobid(bsr->JobId);
   dump_job(bsr->job);
   dump_findex(bsr->FileIndex);
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
   if (bsr) {
      free_bsr_item(bsr->next);
      free(bsr);
   }
}

void free_bsr(BSR *bsr)
{
   if (!bsr) {
      return;
   }
   free_bsr_item((BSR *)bsr->client);
   free_bsr_item((BSR *)bsr->sessid);
   free_bsr_item((BSR *)bsr->sesstime);
   free_bsr_item((BSR *)bsr->volfile);
   free_bsr_item((BSR *)bsr->JobId);
   free_bsr_item((BSR *)bsr->job);
   free_bsr_item((BSR *)bsr->FileIndex);
   free_bsr_item((BSR *)bsr->JobType);
   free_bsr_item((BSR *)bsr->JobLevel);
   if (bsr->VolumeName) {
      free(bsr->VolumeName);
   }
   free_bsr(bsr->next);
   free(bsr);
}
