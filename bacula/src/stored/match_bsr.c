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

/* Forward references */
static int match_sesstime(BSR_SESSTIME *sesstime, DEV_RECORD *rec);
static int match_sessid(BSR_SESSID *sessid, DEV_RECORD *rec);
static int match_client(BSR_CLIENT *client, SESSION_LABEL *sesrec);
static int match_job(BSR_JOB *job, SESSION_LABEL *sesrec);
static int match_job_type(BSR_JOBTYPE *job_type, SESSION_LABEL *sesrec);
static int match_job_level(BSR_JOBLEVEL *job_level, SESSION_LABEL *sesrec);
static int match_jobid(BSR_JOBID *jobid, SESSION_LABEL *sesrec);
static int match_file_index(BSR_FINDEX *findex, DEV_RECORD *rec);
static int match_one_bsr(BSR *bsr, DEV_RECORD *rec, VOLUME_LABEL *volrec, SESSION_LABEL *sesrec);

/*********************************************************************
 *
 *	Match Bootstrap records
 *
 */
int match_bsr(BSR *bsr, DEV_RECORD *rec, VOLUME_LABEL *volrec, SESSION_LABEL *sesrec)
{
   if (!bsr) {
      return 0;
   }
   if (match_one_bsr(bsr, rec, volrec, sesrec)) {
      return 1;
   }
   return match_bsr(bsr->next, rec, volrec, sesrec);
}

static int match_one_bsr(BSR *bsr, DEV_RECORD *rec, VOLUME_LABEL *volrec, SESSION_LABEL *sesrec)
{
   if (strcmp(bsr->VolumeName, volrec->VolName) != 0) {
      return 0;
   }
   if (!match_client(bsr->client, sesrec)) {
      return 0;
   }
   if (!match_sessid(bsr->sessid, rec)) {
      return 0;
   }
   if (!match_sesstime(bsr->sesstime, rec)) {
      return 0;
   }
   if (!match_job(bsr->job, sesrec)) {
      return 0;
   }
   if (!match_file_index(bsr->FileIndex, rec)) {
      return 0;
   }
   if (!match_job_type(bsr->JobType, sesrec)) {
      return 0;
   }
   if (!match_job_level(bsr->JobLevel, sesrec)) {
      return 0;
   }
   if (!match_jobid(bsr->JobId, sesrec)) {
      return 0;
   }
   return 1;
}

static int match_client(BSR_CLIENT *client, SESSION_LABEL *sesrec)
{
   if (!client) {
      return 1; 		      /* no specification matches all */
   }
   if (strcmp(client->ClientName, sesrec->ClientName) == 0) {
      return 1;
   }
   if (client->next) {
      return match_client(client->next, sesrec);
   }
   return 0;
}

static int match_job(BSR_JOB *job, SESSION_LABEL *sesrec)
{
   if (!job) {
      return 1; 		      /* no specification matches all */
   }
   if (strcmp(job->Job, sesrec->Job) == 0) {
      job->found++;
      return 1;
   }
   if (job->next) {
      return match_job(job->next, sesrec);
   }
   return 0;
}


static int match_job_type(BSR_JOBTYPE *job_type, SESSION_LABEL *sesrec)
{
   if (!job_type) {
      return 1; 		      /* no specification matches all */
   }
   if (job_type->JobType == sesrec->JobType) {
      return 1;
   }
   if (job_type->next) {
      return match_job_type(job_type->next, sesrec);
   }
   return 0;
}

static int match_job_level(BSR_JOBLEVEL *job_level, SESSION_LABEL *sesrec)
{
   if (!job_level) {
      return 1; 		      /* no specification matches all */
   }
   if (job_level->JobLevel == sesrec->JobLevel) {
      return 1;
   }
   if (job_level->next) {
      return match_job_level(job_level->next, sesrec);
   }
   return 0;
}

static int match_jobid(BSR_JOBID *jobid, SESSION_LABEL *sesrec)
{
   if (!jobid) {
      return 1; 		      /* no specification matches all */
   }
   if (jobid->JobId == sesrec->JobId) {
      jobid->found++;
      return 1;
   }
   if (jobid->next) {
      return match_jobid(jobid->next, sesrec);
   }
   return 0;
}


static int match_file_index(BSR_FINDEX *findex, DEV_RECORD *rec)
{
   if (!findex) {
      return 1; 		      /* no specification matches all */
   }
   if (findex->FileIndex == rec->FileIndex) {
      findex->found++;
      return 1;
   }
   if (findex->next) {
      return match_file_index(findex->next, rec);
   }
   return 0;
}


static int match_sessid(BSR_SESSID *sessid, DEV_RECORD *rec)
{
   if (!sessid) {
      return 1; 		      /* no specification matches all */
   }
   if (sessid->sessid1 == rec->VolSessionId) {
      sessid->found++;
      return 1;
   }
   if (sessid->next) {
      return match_sessid(sessid->next, rec);
   }
   return 0;
}

static int match_sesstime(BSR_SESSTIME *sesstime, DEV_RECORD *rec)
{
   if (!sesstime) {
      return 1; 		      /* no specification matches all */
   }
   if (sesstime->sesstime == rec->VolSessionTime) {
      sesstime->found++;
      return 1;
   }
   if (sesstime->next) {
      return match_sesstime(sesstime->next, rec);
   }
   return 0;
}
