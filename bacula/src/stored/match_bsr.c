/*     
 *   Match Bootstrap Records (used for restores) against
 *     Volume Records
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
#include <fnmatch.h>

/* Forward references */
static int match_volume(BSR *bsr, BSR_VOLUME *volume, VOLUME_LABEL *volrec, int done);
static int match_sesstime(BSR *bsr, BSR_SESSTIME *sesstime, DEV_RECORD *rec, int done);
static int match_sessid(BSR *bsr, BSR_SESSID *sessid, DEV_RECORD *rec, int done);
static int match_client(BSR *bsr, BSR_CLIENT *client, SESSION_LABEL *sessrec, int done);
static int match_job(BSR *bsr, BSR_JOB *job, SESSION_LABEL *sessrec, int done);
static int match_job_type(BSR *bsr, BSR_JOBTYPE *job_type, SESSION_LABEL *sessrec, int done);
static int match_job_level(BSR *bsr, BSR_JOBLEVEL *job_level, SESSION_LABEL *sessrec, int done);
static int match_jobid(BSR *bsr, BSR_JOBID *jobid, SESSION_LABEL *sessrec, int done);
static int match_findex(BSR *bsr, BSR_FINDEX *findex, DEV_RECORD *rec, int done);
static int match_volfile(BSR *bsr, BSR_VOLFILE *volfile, DEV_RECORD *rec, int done);
static int match_stream(BSR *bsr, BSR_STREAM *stream, DEV_RECORD *rec, int done);
static int match_all(BSR *bsr, DEV_RECORD *rec, VOLUME_LABEL *volrec, SESSION_LABEL *sessrec, int done);

/*********************************************************************
 *
 *	Match Bootstrap records
 *	  returns  1 on match
 *	  returns  0 no match
 *	 returns -1 no additional matches possible
 */
int match_bsr(BSR *bsr, DEV_RECORD *rec, VOLUME_LABEL *volrec, SESSION_LABEL *sessrec)
{
   int stat;

   if (bsr) {
      stat = match_all(bsr, rec, volrec, sessrec, 1);
   } else {
      stat = 0;
   }
// Dmsg1(000, "BSR returning %d\n", stat);
   return stat;
}

/* 
 * Match all the components of current record
 *   returns  1 on match
 *   returns  0 no match
 *   returns -1 no additional matches possible
 */
static int match_all(BSR *bsr, DEV_RECORD *rec, VOLUME_LABEL *volrec, 
		     SESSION_LABEL *sessrec, int done)
{
   if (bsr->done) {
      goto no_match;
   }
   if (bsr->count && bsr->count <= bsr->found) {
      bsr->done = 1;
      goto no_match;
   }
   if (!match_volume(bsr, bsr->volume, volrec, 1)) {
      goto no_match;
   }
   if (!match_volfile(bsr, bsr->volfile, rec, 1)) {
      goto no_match;
   }
   if (!match_sesstime(bsr, bsr->sesstime, rec, 1)) {
      goto no_match;
   }

   /* NOTE!! This test MUST come after the sesstime test */
   if (!match_sessid(bsr, bsr->sessid, rec, 1)) {
      goto no_match;
   }

   /* NOTE!! This test MUST come after sesstime and sessid tests */
   if (!match_findex(bsr, bsr->FileIndex, rec, 1)) {
      goto no_match;
   }
   if (!match_jobid(bsr, bsr->JobId, sessrec, 1)) {
      goto no_match;
   }
   if (!match_job(bsr, bsr->job, sessrec, 1)) {
      goto no_match;
   }
   if (!match_client(bsr, bsr->client, sessrec, 1)) {
      goto no_match;
   }
   if (!match_job_type(bsr, bsr->JobType, sessrec, 1)) {
      goto no_match;
   }
   if (!match_job_level(bsr, bsr->JobLevel, sessrec, 1)) {
      goto no_match;
   }
   if (!match_stream(bsr, bsr->stream, rec, 1)) {
      goto no_match;
   }
   bsr->found++;
   return 1;

no_match:
   if (bsr->next) {
      return match_all(bsr->next, rec, volrec, sessrec, bsr->done && done);
   }
   if (bsr->done && done) {
      return -1;
   }
   return 0;
}

static int match_volume(BSR *bsr, BSR_VOLUME *volume, VOLUME_LABEL *volrec, int done) 
{
   if (!volume) {
      return 0; 		      /* Volume must match */
   }
   if (strcmp(volume->VolumeName, volrec->VolName) == 0) {
      return 1;
   }
   if (volume->next) {
      return match_volume(bsr, volume->next, volrec, 1);
   }
   return 0;
}

static int match_client(BSR *bsr, BSR_CLIENT *client, SESSION_LABEL *sessrec, int done)
{
   if (!client) {
      return 1; 		      /* no specification matches all */
   }
   if (fnmatch(client->ClientName, sessrec->ClientName, 0) == 0) {
      return 1;
   }
   if (client->next) {
      return match_client(bsr, client->next, sessrec, 1);
   }
   return 0;
}

static int match_job(BSR *bsr, BSR_JOB *job, SESSION_LABEL *sessrec, int done)
{
   if (!job) {
      return 1; 		      /* no specification matches all */
   }
   if (fnmatch(job->Job, sessrec->Job, 0) == 0) {
      return 1;
   }
   if (job->next) {
      return match_job(bsr, job->next, sessrec, 1);
   }
   return 0;
}

static int match_job_type(BSR *bsr, BSR_JOBTYPE *job_type, SESSION_LABEL *sessrec, int done)
{
   if (!job_type) {
      return 1; 		      /* no specification matches all */
   }
   if (job_type->JobType == sessrec->JobType) {
      return 1;
   }
   if (job_type->next) {
      return match_job_type(bsr, job_type->next, sessrec, 1);
   }
   return 0;
}

static int match_job_level(BSR *bsr, BSR_JOBLEVEL *job_level, SESSION_LABEL *sessrec, int done)
{
   if (!job_level) {
      return 1; 		      /* no specification matches all */
   }
   if (job_level->JobLevel == sessrec->JobLevel) {
      return 1;
   }
   if (job_level->next) {
      return match_job_level(bsr, job_level->next, sessrec, 1);
   }
   return 0;
}

static int match_jobid(BSR *bsr, BSR_JOBID *jobid, SESSION_LABEL *sessrec, int done)
{
   if (!jobid) {
      return 1; 		      /* no specification matches all */
   }
   if (jobid->JobId <= sessrec->JobId && jobid->JobId2 >= sessrec->JobId) {
      return 1;
   }
   if (jobid->next) {
      return match_jobid(bsr, jobid->next, sessrec, 1);
   }
   return 0;
}

static int match_volfile(BSR *bsr, BSR_VOLFILE *volfile, DEV_RECORD *rec, int done)
{
   if (!volfile) {
      return 1; 		      /* no specification matches all */
   }
   if (volfile->sfile <= rec->File && volfile->efile >= rec->File) {
      return 1;
   }
   /* Once we get past last efile, we are done */
   if (rec->File > volfile->efile) {
      volfile->done = 1;	      /* set local done */
   }
   if (volfile->next) {
      return match_volfile(bsr, volfile->next, rec, volfile->done && done);
   }

   /* If we are done and all prior matches are done, this bsr is finished */
   if (volfile->done && done) {
      bsr->done = 1;
   }
   return 0;
}

static int match_stream(BSR *bsr, BSR_STREAM *stream, DEV_RECORD *rec, int done)
{
   if (!stream) {
      return 1; 		      /* no specification matches all */
   }
   if (stream->stream == rec->Stream) {
      return 1;
   }
   if (stream->next) {
      return match_stream(bsr, stream->next, rec, 1);
   }
   return 0;
}

static int match_sesstime(BSR *bsr, BSR_SESSTIME *sesstime, DEV_RECORD *rec, int done)
{
   if (!sesstime) {
      return 1; 		      /* no specification matches all */
   }
   if (sesstime->sesstime == rec->VolSessionTime) {
      return 1;
   }
   if (rec->VolSessionTime > sesstime->sesstime) {
      sesstime->done = 1;
   }
   if (sesstime->next) {
      return match_sesstime(bsr, sesstime->next, rec, sesstime->done && done);
   }
   if (sesstime->done && done) {
      bsr->done = 1;
   }
   return 0;
}

static int match_sessid(BSR *bsr, BSR_SESSID *sessid, DEV_RECORD *rec, int done)
{
   if (!sessid) {
      return 1; 		      /* no specification matches all */
   }
   if (sessid->sessid <= rec->VolSessionId && sessid->sessid2 >= rec->VolSessionId) {
      return 1;
   }
   if (rec->VolSessionId > sessid->sessid2) {
      sessid->done = 1;
   }
   if (sessid->next) {
      return match_sessid(bsr, sessid->next, rec, sessid->done && done);
   }
   if (sessid->done && done) {
      bsr->done = 1;
   }
   return 0;
}

static int match_findex(BSR *bsr, BSR_FINDEX *findex, DEV_RECORD *rec, int done)
{
   if (!findex) {
      return 1; 		      /* no specification matches all */
   }
   if (findex->findex <= rec->FileIndex && findex->findex2 >= rec->FileIndex) {
      return 1;
   }
   if (rec->FileIndex > findex->findex2) {
      findex->done = 1;
   }
   if (findex->next) {
      return match_findex(bsr, findex->next, rec, findex->done && done);
   }
   if (findex->done && done) {
      bsr->done = 1;
   }
   return 0;
}
