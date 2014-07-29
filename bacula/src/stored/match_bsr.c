/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2002-2014 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from many
   others, a complete list can be found in the file AUTHORS.

   You may use this file and others of this release according to the
   license defined in the LICENSE file, which includes the Affero General
   Public License, v3.0 ("AGPLv3") and some additional permissions and
   terms pursuant to its AGPLv3 Section 7.

   Bacula® is a registered trademark of Kern Sibbald.
*/
/*
 *   Match Bootstrap Records (used for restores) against
 *     Volume Records
 *
 *     Kern Sibbald, June MMII
 *
 */

/*
 * ***FIXME***
 * Also for efficiency, once a bsr is done, it really should be
 *   delinked from the bsr chain.  This will avoid the above
 *   problem and make traversal of the bsr chain more efficient.
 *
 *   To be done ...
 */

#include "bacula.h"
#include "stored.h"
#ifdef HAVE_FNMATCH
#include <fnmatch.h>
#else
#include "lib/fnmatch.h"
#endif

const int dbglevel = 500;

/* Forward references */
static int match_volume(BSR *bsr, BSR_VOLUME *volume, VOLUME_LABEL *volrec, bool done);
static int match_sesstime(BSR *bsr, BSR_SESSTIME *sesstime, DEV_RECORD *rec, bool done);
static int match_sessid(BSR *bsr, BSR_SESSID *sessid, DEV_RECORD *rec);
static int match_client(BSR *bsr, BSR_CLIENT *client, SESSION_LABEL *sessrec, bool done);
static int match_job(BSR *bsr, BSR_JOB *job, SESSION_LABEL *sessrec, bool done);
static int match_job_type(BSR *bsr, BSR_JOBTYPE *job_type, SESSION_LABEL *sessrec, bool done);
static int match_job_level(BSR *bsr, BSR_JOBLEVEL *job_level, SESSION_LABEL *sessrec, bool done);
static int match_jobid(BSR *bsr, BSR_JOBID *jobid, SESSION_LABEL *sessrec, bool done);
static int match_findex(BSR *bsr, BSR_FINDEX *findex, DEV_RECORD *rec, bool done);
static int match_volfile(BSR *bsr, BSR_VOLFILE *volfile, DEV_RECORD *rec, bool done);
static int match_voladdr(BSR *bsr, BSR_VOLADDR *voladdr, DEV_RECORD *rec, bool done);
static int match_stream(BSR *bsr, BSR_STREAM *stream, DEV_RECORD *rec, bool done);
static int match_all(BSR *bsr, DEV_RECORD *rec, VOLUME_LABEL *volrec, SESSION_LABEL *sessrec, bool done, JCR *jcr);
static int match_block_sesstime(BSR *bsr, BSR_SESSTIME *sesstime, DEV_BLOCK *block);
static int match_block_sessid(BSR *bsr, BSR_SESSID *sessid, DEV_BLOCK *block);
static BSR *find_smallest_volfile(BSR *fbsr, BSR *bsr);


/*********************************************************************
 *
 *  If possible, position the archive device (tape) to read the
 *  next block.
 */
void position_bsr_block(BSR *bsr, DEV_BLOCK *block)
{
   /* To be implemented */
}

/*********************************************************************
 *
 *  Do fast block rejection based on bootstrap records.
 *    use_fast_rejection will be set if we have VolSessionId and VolSessTime
 *    in each record. When BlockVer is >= 2, we have those in the block header
 *    so can do fast rejection.
 *
 *   returns:  1 if block may contain valid records
 *             0 if block may be skipped (i.e. it contains no records of
 *                  that can match the bsr).
 *
 */
int match_bsr_block(BSR *bsr, DEV_BLOCK *block)
{
   if (!bsr || !bsr->use_fast_rejection || (block->BlockVer < 2)) {
      return 1;                       /* cannot fast reject */
   }

   for ( ; bsr; bsr=bsr->next) {
      if (!match_block_sesstime(bsr, bsr->sesstime, block)) {
         continue;
      }
      if (!match_block_sessid(bsr, bsr->sessid, block)) {
         continue;
      }
      return 1;
   }
   return 0;
}

static int match_block_sesstime(BSR *bsr, BSR_SESSTIME *sesstime, DEV_BLOCK *block)
{
   if (!sesstime) {
      return 1;                       /* no specification matches all */
   }
   if (sesstime->sesstime == block->VolSessionTime) {
      return 1;
   }
   if (sesstime->next) {
      return match_block_sesstime(bsr, sesstime->next, block);
   }
   return 0;
}

static int match_block_sessid(BSR *bsr, BSR_SESSID *sessid, DEV_BLOCK *block)
{
   if (!sessid) {
      return 1;                       /* no specification matches all */
   }
   if (sessid->sessid <= block->VolSessionId && sessid->sessid2 >= block->VolSessionId) {
      return 1;
   }
   if (sessid->next) {
      return match_block_sessid(bsr, sessid->next, block);
   }
   return 0;
}

static int match_fileregex(BSR *bsr, DEV_RECORD *rec, JCR *jcr)
{
   if (bsr->fileregex_re == NULL)
      return 1;

   if (bsr->attr == NULL) {
      bsr->attr = new_attr(jcr);
   }

   /*
    * The code breaks if the first record associated with a file is
    * not of this type
    */
   if (rec->maskedStream == STREAM_UNIX_ATTRIBUTES ||
       rec->maskedStream == STREAM_UNIX_ATTRIBUTES_EX) {
      bsr->skip_file = false;
      if (unpack_attributes_record(jcr, rec->Stream, rec->data, rec->data_len, bsr->attr)) {
         if (regexec(bsr->fileregex_re, bsr->attr->fname, 0, NULL, 0) == 0) {
            Dmsg2(dbglevel, "Matched pattern, fname=%s FI=%d\n",
                  bsr->attr->fname, rec->FileIndex);
         } else {
            Dmsg2(dbglevel, "Didn't match, skipping fname=%s FI=%d\n",
                  bsr->attr->fname, rec->FileIndex);
            bsr->skip_file = true;
         }
      }
   }
   return 1;
}

/*********************************************************************
 *
 *      Match Bootstrap records
 *        returns  1 on match
 *        returns  0 no match and reposition is set if we should
 *                      reposition the tape
 *       returns -1 no additional matches possible
 */
int match_bsr(BSR *bsr, DEV_RECORD *rec, VOLUME_LABEL *volrec, SESSION_LABEL *sessrec, JCR *jcr)
{
   int stat;

   /*
    * The bsr->reposition flag is set any time a bsr is done.
    *   In this case, we can probably reposition the
    *   tape to the next available bsr position.
    */
   if (bsr) {
      bsr->reposition = false;
      stat = match_all(bsr, rec, volrec, sessrec, true, jcr);
      /*
       * Note, bsr->reposition is set by match_all when
       *  a bsr is done. We turn it off if a match was
       *  found or if we cannot use positioning
       */
      if (stat != 0 || !bsr->use_positioning) {
         bsr->reposition = false;
      }
   } else {
      stat = 1;                       /* no bsr => match all */
   }
   return stat;
}

/*
 * Find the next bsr that applies to the current tape.
 *   It is the one with the smallest VolFile position.
 */
BSR *find_next_bsr(BSR *root_bsr, DEVICE *dev)
{
   BSR *bsr;
   BSR *found_bsr = NULL;

   /* Do tape/disk seeking only if CAP_POSITIONBLOCKS is on */
   if (!root_bsr) {
      Dmsg0(dbglevel, "NULL root bsr pointer passed to find_next_bsr.\n");
      return NULL;
   }
   if (!root_bsr->use_positioning ||
       !root_bsr->reposition || !dev->has_cap(CAP_POSITIONBLOCKS)) {
      Dmsg2(dbglevel, "No nxt_bsr use_pos=%d repos=%d\n", root_bsr->use_positioning, root_bsr->reposition);
      return NULL;
   }
   Dmsg2(dbglevel, "use_pos=%d repos=%d\n", root_bsr->use_positioning, root_bsr->reposition);
   root_bsr->mount_next_volume = false;
   /* Walk through all bsrs to find the next one to use => smallest file,block */
   for (bsr=root_bsr; bsr; bsr=bsr->next) {
      if (bsr->done || !match_volume(bsr, bsr->volume, &dev->VolHdr, 1)) {
         continue;
      }
      if (found_bsr == NULL) {
         found_bsr = bsr;
      } else {
         found_bsr = find_smallest_volfile(found_bsr, bsr);
      }
   }
   /*
    * If we get to this point and found no bsr, it means
    *  that any additional bsr's must apply to the next
    *  tape, so set a flag.
    */
   if (found_bsr == NULL) {
      root_bsr->mount_next_volume = true;
   }
   return found_bsr;
}

/*
 * Get the smallest address from this voladdr part
 * Don't use "done" elements
 */
static bool get_smallest_voladdr(BSR_VOLADDR *va, uint64_t *ret)
{
   bool ok=false;
   uint64_t min_val=0;

   for (; va ; va = va->next) {
      if (!va->done) {
         if (ok) {
            min_val = MIN(min_val, va->saddr);
         } else {
            min_val = va->saddr;
            ok=true;
         }
      }
   }
   *ret = min_val;
   return ok;
}

/* FIXME
 * This routine needs to be fixed to only look at items that
 *   are not marked as done.  Otherwise, it can find a bsr
 *   that has already been consumed, and this will cause the
 *   bsr to be used, thus we may seek back and re-read the
 *   same records, causing an error.  This deficiency must
 *   be fixed.  For the moment, it has been kludged in
 *   read_record.c to avoid seeking back if find_next_bsr
 *   returns a bsr pointing to a smaller address (file/block).
 *
 */
static BSR *find_smallest_volfile(BSR *found_bsr, BSR *bsr)
{
   BSR *return_bsr = found_bsr;
   BSR_VOLFILE *vf;
   BSR_VOLBLOCK *vb;
   uint32_t found_bsr_sfile, bsr_sfile;
   uint32_t found_bsr_sblock, bsr_sblock;
   uint64_t found_bsr_saddr, bsr_saddr;

   /* if we have VolAddr, use it, else try with File and Block */
   if (get_smallest_voladdr(found_bsr->voladdr, &found_bsr_saddr)) {
      if (get_smallest_voladdr(bsr->voladdr, &bsr_saddr)) {
         if (found_bsr_saddr > bsr_saddr) {
            return bsr;
         } else {
            return found_bsr;
         }
      }
   }

   /* Find the smallest file in the found_bsr */
   vf = found_bsr->volfile;
   found_bsr_sfile = vf->sfile;
   while ( (vf=vf->next) ) {
      if (vf->sfile < found_bsr_sfile) {
         found_bsr_sfile = vf->sfile;
      }
   }

   /* Find the smallest file in the bsr */
   vf = bsr->volfile;
   bsr_sfile = vf->sfile;
   while ( (vf=vf->next) ) {
      if (vf->sfile < bsr_sfile) {
         bsr_sfile = vf->sfile;
      }
   }

   /* if the bsr file is less than the found_bsr file, return bsr */
   if (found_bsr_sfile > bsr_sfile) {
      return_bsr = bsr;
   } else if (found_bsr_sfile == bsr_sfile) {
      /* Files are equal */
      /* find smallest block in found_bsr */
      vb = found_bsr->volblock;
      found_bsr_sblock = vb->sblock;
      while ( (vb=vb->next) ) {
         if (vb->sblock < found_bsr_sblock) {
            found_bsr_sblock = vb->sblock;
         }
      }
      /* Find smallest block in bsr */
      vb = bsr->volblock;
      bsr_sblock = vb->sblock;
      while ( (vb=vb->next) ) {
         if (vb->sblock < bsr_sblock) {
            bsr_sblock = vb->sblock;
         }
      }
      /* Compare and return the smallest */
      if (found_bsr_sblock > bsr_sblock) {
         return_bsr = bsr;
      }
   }
   return return_bsr;
}

/*
 * Called after the signature record so that
 *   we can see if the current bsr has been
 *   fully processed (i.e. is done).
 *  The bsr argument is not used, but is included
 *    for consistency with the other match calls.
 *
 * Returns: true if we should reposition
 *        : false otherwise.
 */
bool is_this_bsr_done(BSR *bsr, DEV_RECORD *rec)
{
   BSR *rbsr = rec->bsr;
   Dmsg1(dbglevel, "match_set %d\n", rbsr != NULL);
   if (!rbsr) {
      return false;
   }
   rec->bsr = NULL;
   rbsr->found++;
   if (rbsr->count && rbsr->found >= rbsr->count) {
      rbsr->done = true;
      rbsr->root->reposition = true;
      Dmsg2(dbglevel, "is_end_this_bsr set reposition=1 count=%d found=%d\n",
         rbsr->count, rbsr->found);
      return true;
   }
   Dmsg2(dbglevel, "is_end_this_bsr not done count=%d found=%d\n",
        rbsr->count, rbsr->found);
   return false;
}

/*
 * Match all the components of current record
 *   returns  1 on match
 *   returns  0 no match
 *   returns -1 no additional matches possible
 */
static int match_all(BSR *bsr, DEV_RECORD *rec, VOLUME_LABEL *volrec,
                     SESSION_LABEL *sessrec, bool done, JCR *jcr)
{
   Dmsg0(dbglevel, "Enter match_all\n");
   if (bsr->done) {
//    Dmsg0(dbglevel, "bsr->done set\n");
      goto no_match;
   }
   if (!match_volume(bsr, bsr->volume, volrec, 1)) {
      Dmsg2(dbglevel, "bsr fail bsr_vol=%s != rec read_vol=%s\n", bsr->volume->VolumeName,
            volrec->VolumeName);
      goto no_match;
   }
   Dmsg2(dbglevel, "OK bsr match bsr_vol=%s read_vol=%s\n", bsr->volume->VolumeName,
            volrec->VolumeName);

   if (!match_volfile(bsr, bsr->volfile, rec, 1)) {
      if (bsr->volfile) {
         Dmsg3(dbglevel, "Fail on file=%u. bsr=%u,%u\n",
               rec->File, bsr->volfile->sfile, bsr->volfile->efile);
      }
      goto no_match;
   }

   if (!match_voladdr(bsr, bsr->voladdr, rec, 1)) {
      if (bsr->voladdr) {
         Dmsg3(dbglevel, "Fail on Addr=%llu. bsr=%llu,%llu\n",
               get_record_address(rec), bsr->voladdr->saddr, bsr->voladdr->eaddr);
      }
      goto no_match;
   }

   if (!match_sesstime(bsr, bsr->sesstime, rec, 1)) {
      Dmsg2(dbglevel, "Fail on sesstime. bsr=%u rec=%u\n",
         bsr->sesstime->sesstime, rec->VolSessionTime);
      goto no_match;
   }

   /* NOTE!! This test MUST come after the sesstime test */
   if (!match_sessid(bsr, bsr->sessid, rec)) {
      Dmsg2(dbglevel, "Fail on sessid. bsr=%u rec=%u\n",
         bsr->sessid->sessid, rec->VolSessionId);
      goto no_match;
   }

   /* NOTE!! This test MUST come after sesstime and sessid tests */
   if (!match_findex(bsr, bsr->FileIndex, rec, 1)) {
      Dmsg3(dbglevel, "Fail on findex=%d. bsr=%d,%d\n",
         rec->FileIndex, bsr->FileIndex->findex, bsr->FileIndex->findex2);
      goto no_match;
   }
   if (bsr->FileIndex) {
      Dmsg3(dbglevel, "match on findex=%d. bsr=%d,%d\n",
            rec->FileIndex, bsr->FileIndex->findex, bsr->FileIndex->findex2);
   }

   if (!match_fileregex(bsr, rec, jcr)) {
     Dmsg1(dbglevel, "Fail on fileregex='%s'\n", NPRT(bsr->fileregex));
     goto no_match;
   }

   /* This flag is set by match_fileregex (and perhaps other tests) */
   if (bsr->skip_file) {
      Dmsg1(dbglevel, "Skipping findex=%d\n", rec->FileIndex);
      goto no_match;
   }

   /*
    * If a count was specified and we have a FileIndex, assume
    *   it is a Bacula created bsr (or the equivalent). We
    *   then save the bsr where the match occurred so that
    *   after processing the record or records, we can update
    *   the found count. I.e. rec->bsr points to the bsr that
    *   satisfied the match.
    */
   if (bsr->count && bsr->FileIndex) {
      rec->bsr = bsr;
      Dmsg0(dbglevel, "Leave match_all 1\n");
      return 1;                       /* this is a complete match */
   }

   /*
    * The selections below are not used by Bacula's
    *   restore command, and don't work because of
    *   the rec->bsr = bsr optimization above.
    */
   if (!match_jobid(bsr, bsr->JobId, sessrec, 1)) {
      Dmsg0(dbglevel, "fail on JobId\n");
      goto no_match;

   }
   if (!match_job(bsr, bsr->job, sessrec, 1)) {
      Dmsg0(dbglevel, "fail on Job\n");
      goto no_match;
   }
   if (!match_client(bsr, bsr->client, sessrec, 1)) {
      Dmsg0(dbglevel, "fail on Client\n");
      goto no_match;
   }
   if (!match_job_type(bsr, bsr->JobType, sessrec, 1)) {
      Dmsg0(dbglevel, "fail on Job type\n");
      goto no_match;
   }
   if (!match_job_level(bsr, bsr->JobLevel, sessrec, 1)) {
      Dmsg0(dbglevel, "fail on Job level\n");
      goto no_match;
   }
   if (!match_stream(bsr, bsr->stream, rec, 1)) {
      Dmsg0(dbglevel, "fail on stream\n");
      goto no_match;
   }
   return 1;

no_match:
   if (bsr->next) {
      return match_all(bsr->next, rec, volrec, sessrec, bsr->done && done, jcr);
   }
   if (bsr->done && done) {
      Dmsg0(dbglevel, "Leave match all -1\n");
      return -1;
   }
   Dmsg0(dbglevel, "Leave match all 0\n");
   return 0;
}

static int match_volume(BSR *bsr, BSR_VOLUME *volume, VOLUME_LABEL *volrec, bool done)
{
   if (!volume) {
      return 0;                       /* Volume must match */
   }
   if (strcmp(volume->VolumeName, volrec->VolumeName) == 0) {
      Dmsg1(dbglevel, "match_volume=%s\n", volrec->VolumeName);
      return 1;
   }
   if (volume->next) {
      return match_volume(bsr, volume->next, volrec, 1);
   }
   return 0;
}

static int match_client(BSR *bsr, BSR_CLIENT *client, SESSION_LABEL *sessrec, bool done)
{
   if (!client) {
      return 1;                       /* no specification matches all */
   }
   if (strcmp(client->ClientName, sessrec->ClientName) == 0) {
      return 1;
   }
   if (client->next) {
      return match_client(bsr, client->next, sessrec, 1);
   }
   return 0;
}

static int match_job(BSR *bsr, BSR_JOB *job, SESSION_LABEL *sessrec, bool done)
{
   if (!job) {
      return 1;                       /* no specification matches all */
   }
   if (strcmp(job->Job, sessrec->Job) == 0) {
      return 1;
   }
   if (job->next) {
      return match_job(bsr, job->next, sessrec, 1);
   }
   return 0;
}

static int match_job_type(BSR *bsr, BSR_JOBTYPE *job_type, SESSION_LABEL *sessrec, bool done)
{
   if (!job_type) {
      return 1;                       /* no specification matches all */
   }
   if (job_type->JobType == sessrec->JobType) {
      return 1;
   }
   if (job_type->next) {
      return match_job_type(bsr, job_type->next, sessrec, 1);
   }
   return 0;
}

static int match_job_level(BSR *bsr, BSR_JOBLEVEL *job_level, SESSION_LABEL *sessrec, bool done)
{
   if (!job_level) {
      return 1;                       /* no specification matches all */
   }
   if (job_level->JobLevel == sessrec->JobLevel) {
      return 1;
   }
   if (job_level->next) {
      return match_job_level(bsr, job_level->next, sessrec, 1);
   }
   return 0;
}

static int match_jobid(BSR *bsr, BSR_JOBID *jobid, SESSION_LABEL *sessrec, bool done)
{
   if (!jobid) {
      return 1;                       /* no specification matches all */
   }
   if (jobid->JobId <= sessrec->JobId && jobid->JobId2 >= sessrec->JobId) {
      return 1;
   }
   if (jobid->next) {
      return match_jobid(bsr, jobid->next, sessrec, 1);
   }
   return 0;
}

static int match_volfile(BSR *bsr, BSR_VOLFILE *volfile, DEV_RECORD *rec, bool done)
{
   if (!volfile) {
      return 1;                       /* no specification matches all */
   }
/*
 * The following code is turned off because this should now work
 *   with disk files too, though since a "volfile" is 4GB, it does
 *   not improve performance much.
 */
#ifdef xxx
   /* For the moment, these tests work only with tapes. */
   if (!(rec->state & REC_ISTAPE)) {
      return 1;                       /* All File records OK for this match */
   }
   Dmsg3(dbglevel, "match_volfile: sfile=%u efile=%u recfile=%u\n",
             volfile->sfile, volfile->efile, rec->File);
#endif
   if (volfile->sfile <= rec->File && volfile->efile >= rec->File) {
      return 1;
   }
   /* Once we get past last efile, we are done */
   if (rec->File > volfile->efile) {
      volfile->done = true;              /* set local done */
   }
   if (volfile->next) {
      return match_volfile(bsr, volfile->next, rec, volfile->done && done);
   }

   /* If we are done and all prior matches are done, this bsr is finished */
   if (volfile->done && done) {
      bsr->done = true;
      bsr->root->reposition = true;
      Dmsg2(dbglevel, "bsr done from volfile rec=%u volefile=%u\n",
         rec->File, volfile->efile);
   }
   return 0;
}

static int match_voladdr(BSR *bsr, BSR_VOLADDR *voladdr, DEV_RECORD *rec, bool done)
{
   if (!voladdr) {
      return 1;                       /* no specification matches all */
   }

#ifdef xxx

   /* For the moment, these tests work only with disk. */
   if (rec->state & REC_ISTAPE) {
      uint32_t sFile = (voladdr->saddr)>>32;
      uint32_t eFile = (voladdr->eaddr)>>32;
      if (sFile <= rec->File && eFile >= rec->File) {
         return 1;
      }
   }

#endif

   uint64_t addr = get_record_address(rec);
   Dmsg6(dbglevel, "match_voladdr: saddr=%llu eaddr=%llu recaddr=%llu sfile=%u efile=%u recfile=%u\n",
         voladdr->saddr, voladdr->eaddr, addr, voladdr->saddr>>32, voladdr->eaddr>>32, addr>>32);

   if (voladdr->saddr <= addr && voladdr->eaddr >= addr) {
      return 1;
   }
   /* Once we get past last eblock, we are done */
   if (addr > voladdr->eaddr) {
      voladdr->done = true;              /* set local done */
   }
   if (voladdr->next) {
      return match_voladdr(bsr, voladdr->next, rec, voladdr->done && done);
   }

   /* If we are done and all prior matches are done, this bsr is finished */
   if (voladdr->done && done) {
      bsr->done = true;
      bsr->root->reposition = true;
      Dmsg2(dbglevel, "bsr done from voladdr rec=%llu voleaddr=%llu\n",
            addr, voladdr->eaddr);
   }
   return 0;
}


static int match_stream(BSR *bsr, BSR_STREAM *stream, DEV_RECORD *rec, bool done)
{
   if (!stream) {
      return 1;                       /* no specification matches all */
   }
   if (stream->stream == rec->Stream) {
      return 1;
   }
   if (stream->next) {
      return match_stream(bsr, stream->next, rec, 1);
   }
   return 0;
}

static int match_sesstime(BSR *bsr, BSR_SESSTIME *sesstime, DEV_RECORD *rec, bool done)
{
   if (!sesstime) {
      return 1;                       /* no specification matches all */
   }
   if (sesstime->sesstime == rec->VolSessionTime) {
      return 1;
   }
   if (rec->VolSessionTime > sesstime->sesstime) {
      sesstime->done = true;
   }
   if (sesstime->next) {
      return match_sesstime(bsr, sesstime->next, rec, sesstime->done && done);
   }
   if (sesstime->done && done) {
      bsr->done = true;
      bsr->root->reposition = true;
      Dmsg0(dbglevel, "bsr done from sesstime\n");
   }
   return 0;
}

/*
 * Note, we cannot mark bsr done based on session id because we may
 *  have interleaved records, and there may be more of what we want
 *  later.
 */
static int match_sessid(BSR *bsr, BSR_SESSID *sessid, DEV_RECORD *rec)
{
   if (!sessid) {
      return 1;                       /* no specification matches all */
   }
   if (sessid->sessid <= rec->VolSessionId && sessid->sessid2 >= rec->VolSessionId) {
      return 1;
   }
   if (sessid->next) {
      return match_sessid(bsr, sessid->next, rec);
   }
   return 0;
}

/*
 * When reading the Volume, the Volume Findex (rec->FileIndex) always
 *   are found in sequential order. Thus we can make optimizations.
 *
 *  ***FIXME*** optimizations
 * We could optimize by removing the recursion.
 */
static int match_findex(BSR *bsr, BSR_FINDEX *findex, DEV_RECORD *rec, bool done)
{
   if (!findex) {
      return 1;                       /* no specification matches all */
   }
   if (!findex->done) {
      if (findex->findex <= rec->FileIndex && findex->findex2 >= rec->FileIndex) {
         Dmsg3(dbglevel, "Match on findex=%d. bsrFIs=%d,%d\n",
               rec->FileIndex, findex->findex, findex->findex2);
         return 1;
      }
      if (rec->FileIndex > findex->findex2) {
         findex->done = true;
      }
   }
   if (findex->next) {
      return match_findex(bsr, findex->next, rec, findex->done && done);
   }
   if (findex->done && done) {
      bsr->done = true;
      bsr->root->reposition = true;
      Dmsg1(dbglevel, "bsr done from findex %d\n", rec->FileIndex);
   }
   return 0;
}

uint64_t get_bsr_start_addr(BSR *bsr, uint32_t *file, uint32_t *block)
{
   uint64_t bsr_addr = 0;
   uint32_t sfile = 0, sblock = 0;

   if (bsr) {
      if (bsr->voladdr) {
         bsr_addr = bsr->voladdr->saddr;
         sfile = bsr_addr>>32;
         sblock = (uint32_t)bsr_addr;

      } else if (bsr->volfile && bsr->volblock) {
         bsr_addr = (((uint64_t)bsr->volfile->sfile)<<32)|bsr->volblock->sblock;
         sfile = bsr->volfile->sfile;
         sblock = bsr->volblock->sblock;
      }
   }

   if (file && block) {
      *file = sfile;
      *block = sblock;
   }

   return bsr_addr;
}
