/*
 *
 *  This routine provides a routine that will handle all
 *    the gory little details of reading a record from a Bacula
 *    archive. It uses a callback to pass you each record in turn,
 *    as well as a callback for mounting the next tape.  It takes
 *    care of reading blocks, applying the bsr, ...
 *
 *    Kern E. Sibbald, August MMII
 *
 *   Version $Id$
 */
/*
   Copyright (C) 2000-2003 Kern Sibbald and John Walker

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

static void handle_session_record(DEVICE *dev, DEV_RECORD *rec, SESSION_LABEL *sessrec);
static BSR *position_to_first_file(JCR *jcr, DEVICE *dev);
#ifdef DEBUG
static char *rec_state_to_str(DEV_RECORD *rec);
#endif

int read_records(JCR *jcr,  DEVICE *dev, 
       int record_cb(JCR *jcr, DEVICE *dev, DEV_BLOCK *block, DEV_RECORD *rec),
       int mount_cb(JCR *jcr, DEVICE *dev, DEV_BLOCK *block))
{
   DEV_BLOCK *block;
   DEV_RECORD *rec = NULL;
   uint32_t record;
   int ok = TRUE;
   bool done = false;
   SESSION_LABEL sessrec;
   dlist *recs; 			/* linked list of rec packets open */

   block = new_block(dev);
   recs = new dlist(rec, &rec->link);
   position_to_first_file(jcr, dev);

   for ( ; ok && !done; ) {
      if (job_canceled(jcr)) {
	 ok = FALSE;
	 break;
      }
      if (!read_block_from_device(jcr, dev, block, CHECK_BLOCK_NUMBERS)) {
	 if (dev_state(dev, ST_EOT)) {
	    DEV_RECORD *trec = new_record();

            Jmsg(jcr, M_INFO, 0, "End of Volume at file %u  on device %s, Volume \"%s\"\n", 
		 dev->file, dev_name(dev), jcr->VolumeName);
	    if (!mount_cb(jcr, dev, block)) {
               Jmsg(jcr, M_INFO, 0, "End of all volumes.\n");
	       ok = FALSE;
	       /*
		* Create EOT Label so that Media record may
		*  be properly updated because this is the last
		*  tape.
		*/
	       trec->FileIndex = EOT_LABEL;
	       trec->File = dev->file;
	       ok = record_cb(jcr, dev, block, trec);
	       free_record(trec);
	       break;
	    }
	    /*
	     * We just have a new tape up, now read the label (first record)
	     *	and pass it off to the callback routine, then continue
	     *	most likely reading the previous record.
	     */
	    read_block_from_device(jcr, dev, block, NO_BLOCK_NUMBER_CHECK);
	    read_record_from_block(block, trec);
	    handle_session_record(dev, trec, &sessrec);
	    ok = record_cb(jcr, dev, block, trec);
	    free_record(trec);
	    position_to_first_file(jcr, dev);
	    /* After reading label, we must read first data block */
	    continue;

	 } else if (dev_state(dev, ST_EOF)) {
	    if (verbose) {
               Jmsg(jcr, M_INFO, 0, "Got EOF at file %u  on device %s, Volume \"%s\"\n", 
		  dev->file, dev_name(dev), jcr->VolumeName);
	    }
	    continue;
	 } else if (dev_state(dev, ST_SHORT)) {
            Jmsg(jcr, M_ERROR, 0, "%s", dev->errmsg);
	    continue;
	 } else {
	    /* I/O error or strange end of tape */
	    display_tape_error_status(jcr, dev);
	    ok = FALSE;
	    break;
	 }
      }
      Dmsg2(100, "New block at position=(file:block) %d:%d\n", dev->file, dev->block_num);
      Dmsg5(100, "Read block: devblk=%d blk=%d VI=%u VT=%u blen=%d\n", dev->block_num, block->BlockNumber, 
	 block->VolSessionId, block->VolSessionTime, block->block_len);
#ifdef FAST_BLOCK_REJECTION
      /* this does not stop when file/block are too big */
      if (!match_bsr_block(jcr->bsr, block)) {
	 continue;
      }
#endif

      /*
       * Get a new record for each Job as defined by
       *   VolSessionId and VolSessionTime 
       */
      bool found = false;
      for (rec=(DEV_RECORD *)recs->first(); rec; rec=(DEV_RECORD *)recs->next(rec)) {
	 if (rec->VolSessionId == block->VolSessionId &&
	     rec->VolSessionTime == block->VolSessionTime) {
	    found = true;
	    break;
	  }
      }
      if (!found) {
	 rec = new_record();
	 recs->prepend(rec);
         Dmsg2(100, "New record for SI=%d ST=%d\n",
	     block->VolSessionId, block->VolSessionTime);
      } else {
#ifdef xxx
	 if (rec->Block != 0 && (rec->Block+1) != block->BlockNumber) {
            Jmsg(jcr, M_ERROR, 0, _("Invalid block number. Expected %u, got %u\n"),
		 rec->Block+1, block->BlockNumber);
	 }
#endif 
      }
      Dmsg3(100, "After mount next vol. stat=%s blk=%d rem=%d\n", rec_state_to_str(rec), 
	    block->BlockNumber, rec->remainder);
      record = 0;
      for (rec->state=0; !is_block_empty(rec); ) {
	 if (!read_record_from_block(block, rec)) {
            Dmsg3(10, "!read-break. stat=%s blk=%d rem=%d\n", rec_state_to_str(rec), 
		  block->BlockNumber, rec->remainder);
	    break;
	 }
         Dmsg5(100, "read-OK. stat=%s blk=%d rem=%d file:block=%d:%d\n", 
		 rec_state_to_str(rec), block->BlockNumber, rec->remainder,
		 dev->file, dev->block_num);
	 /*
	  * At this point, we have at least a record header.
	  *  Now decide if we want this record or not, but remember
	  *  before accessing the record, we may need to read again to
	  *  get all the data.
	  */
	 record++;
         Dmsg6(100, "recno=%d state=%s blk=%d SI=%d ST=%d FI=%d\n", record,
	    rec_state_to_str(rec), block->BlockNumber,
	    rec->VolSessionId, rec->VolSessionTime, rec->FileIndex);
         Dmsg4(30, "VolSId=%ld FI=%s Strm=%s Size=%ld\n", rec->VolSessionId,
	       FI_to_ascii(rec->FileIndex), 
	       stream_to_ascii(rec->Stream, rec->FileIndex), 
	       rec->data_len);

	 if (rec->FileIndex == EOM_LABEL) { /* end of tape? */
            Dmsg0(40, "Get EOM LABEL\n");
	    break;			   /* yes, get out */
	 }

	 /* Some sort of label? */ 
	 if (rec->FileIndex < 0) {
	    handle_session_record(dev, rec, &sessrec);
	    ok = record_cb(jcr, dev, block, rec);
	    if (rec->FileIndex == EOS_LABEL) {
               Dmsg2(100, "Remove rec. SI=%d ST=%d\n", rec->VolSessionId,
		  rec->VolSessionTime);
	       recs->remove(rec);
	       free_record(rec);
	    }
	    continue;
	 } /* end if label record */

	 /* 
	  * Apply BSR filter
	  */
	 if (jcr->bsr) {
	    int stat = match_bsr(jcr->bsr, rec, &dev->VolHdr, &sessrec);
	    if (stat == -1) { /* no more possible matches */
	       done = true;   /* all items found, stop */
               Dmsg2(100, "All done=(file:block) %d:%d\n", dev->file, dev->block_num);
	       break;
	    } else if (stat == 0) {  /* no match */
	       BSR *bsr;
	       bsr = find_next_bsr(jcr->bsr, dev);
	       if (bsr == NULL && jcr->bsr->mount_next_volume) {
                  Dmsg0(100, "Would mount next volume here\n");
                  Dmsg2(100, "Current postion (file:block) %d:%d\n",
		     dev->file, dev->block_num);
		  jcr->bsr->mount_next_volume = false;
		  dev->state |= ST_EOT;
		  rec->Block = 0;
		  break;
	       }     
	       if (bsr) {
                  Dmsg4(100, "Reposition from (file:block) %d:%d to %d:%d\n",
		     dev->file, dev->block_num, bsr->volfile->sfile,
		     bsr->volblock->sblock);
		  if (verbose) {
                     Jmsg(jcr, M_INFO, 0, "Reposition from (file:block) %d:%d to %d:%d\n",
			dev->file, dev->block_num, bsr->volfile->sfile,
			bsr->volblock->sblock);
		  }
		  reposition_dev(dev, bsr->volfile->sfile, bsr->volblock->sblock);
		  rec->Block = 0;
                  Dmsg2(100, "Now at (file:block) %d:%d\n",
		     dev->file, dev->block_num);
	       }
               Dmsg5(100, "BSR no match rec=%d block=%d SessId=%d SessTime=%d FI=%d\n",
		  record, block->BlockNumber, rec->VolSessionId, rec->VolSessionTime, 
		  rec->FileIndex);
               continue;              /* we don't want record, read next one */
	    }
	 }
	 if (is_partial_record(rec)) {
            Dmsg6(10, "Partial, break. recno=%d state=%s blk=%d SI=%d ST=%d FI=%d\n", record,
	       rec_state_to_str(rec), block->BlockNumber,
	       rec->VolSessionId, rec->VolSessionTime, rec->FileIndex);
	    break;		      /* read second part of record */
	 }
	 ok = record_cb(jcr, dev, block, rec);
      } /* end for loop over records */
   } /* end for loop over blocks */
   Dmsg2(100, "Position=(file:block) %d:%d\n", dev->file, dev->block_num);

   /* Walk down list and free all remaining allocated recs */
   for (rec=(DEV_RECORD *)recs->first(); rec; ) {
      DEV_RECORD *nrec = (DEV_RECORD *)recs->next(rec);
      recs->remove(rec);
      free_record(rec);
      rec = nrec;
   }
   delete recs;
   print_block_errors(jcr, block);
   free_block(block);
   return ok;
}

static BSR *position_to_first_file(JCR *jcr, DEVICE *dev)
{
   BSR *bsr = NULL;
   /*
    * Now find and position to first file and block 
    *	on this tape.
    */
   if (jcr->bsr) {
      jcr->bsr->reposition = true;    /* force repositioning */
      bsr = find_next_bsr(jcr->bsr, dev);
      if (bsr) {
         Jmsg(jcr, M_INFO, 0, _("Forward spacing to file:block %u:%u.\n"), 
	    bsr->volfile->sfile, bsr->volblock->sblock);
         Dmsg4(100, "Reposition new from (file:block) %d:%d to %d:%d\n",
	       dev->file, dev->block_num, bsr->volfile->sfile,
	       bsr->volblock->sblock);
	 reposition_dev(dev, bsr->volfile->sfile, bsr->volblock->sblock);
         Dmsg2(100, "Now at (file:block) %d:%d\n",
	       dev->file, dev->block_num);
      }
   }
   return bsr;
}


static void handle_session_record(DEVICE *dev, DEV_RECORD *rec, SESSION_LABEL *sessrec)
{
   char *rtype;
   char buf[100];
   memset(sessrec, 0, sizeof(sessrec));
   switch (rec->FileIndex) {
   case PRE_LABEL:
      rtype = "Fresh Volume Label";   
      break;
   case VOL_LABEL:
      rtype = "Volume Label";
      unser_volume_label(dev, rec);
      break;
   case SOS_LABEL:
      rtype = "Begin Session";
      unser_session_label(sessrec, rec);
      break;
   case EOS_LABEL:
      rtype = "End Session";
      break;
   case EOM_LABEL:
      rtype = "End of Media";
      break;
   default:
      bsnprintf(buf, sizeof(buf), "Unknown code %d\n", rec->FileIndex);
      rtype = buf;
      break;
   }
   Dmsg5(100, "%s Record: VolSessionId=%d VolSessionTime=%d JobId=%d DataLen=%d\n",
	 rtype, rec->VolSessionId, rec->VolSessionTime, rec->Stream, rec->data_len);
}

#ifdef DEBUG
static char *rec_state_to_str(DEV_RECORD *rec)
{
   static char buf[200]; 
   buf[0] = 0;
   if (rec->state & REC_NO_HEADER) {
      strcat(buf, "Nohdr,");
   }
   if (is_partial_record(rec)) {
      strcat(buf, "partial,");
   }
   if (rec->state & REC_BLOCK_EMPTY) {
      strcat(buf, "empty,");
   }
   if (rec->state & REC_NO_MATCH) {
      strcat(buf, "Nomatch,");
   }
   if (rec->state & REC_CONTINUATION) {
      strcat(buf, "cont,");
   }
   if (buf[0]) {
      buf[strlen(buf)-1] = 0;
   }
   return buf;
}
#endif
