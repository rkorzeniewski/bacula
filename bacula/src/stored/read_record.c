/*
 *
 *  This routine provides a routine that will handle all
 *    the gory little details of reading a record from a Bacula
 *    archive. It uses a callback to pass you each record in turn,
 *    as well as a callback for mounting the next tape.  It takes
 *    care of reading blocks, applying the bsr, ...
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

static void get_session_record(DEVICE *dev, DEV_RECORD *rec, SESSION_LABEL *sessrec);
#ifdef DEBUG
static char *rec_state_to_str(DEV_RECORD *rec);
#endif

int read_records(JCR *jcr,  DEVICE *dev, 
       void record_cb(JCR *jcr, DEVICE *dev, DEV_BLOCK *block, DEV_RECORD *rec),
       int mount_cb(JCR *jcr, DEVICE *dev, DEV_BLOCK *block))
{
   DEV_BLOCK *block;
   DEV_RECORD *rec;
   uint32_t record, num_files = 0;
   int ok = TRUE;
   int done = FALSE;
   SESSION_LABEL sessrec;

   block = new_block(dev);
   rec = new_record();
   for ( ;ok && !done; ) {
      if (job_cancelled(jcr)) {
	 ok = FALSE;
	 break;
      }
      if (!read_block_from_device(dev, block)) {
         Dmsg0(20, "!read_record()\n");
	 if (dev->state & ST_EOT) {
	    DEV_RECORD *trec = new_record();

            Dmsg3(100, "EOT. stat=%s blk=%d rem=%d\n", rec_state_to_str(rec), 
		  block->BlockNumber, rec->remainder);
	    if (!mount_cb(jcr, dev, block)) {
               Dmsg3(100, "After mount next vol. stat=%s blk=%d rem=%d\n", rec_state_to_str(rec), 
		  block->BlockNumber, rec->remainder);
	       ok = FALSE;
	       /*
		* Create EOT Label so that Media record may
		*  be properly updated because this is the last
		*  tape.
		*/
	       trec->FileIndex = EOT_LABEL;
	       trec->File = dev->file;
	       trec->Block = rec->Block; /* return block last read */
	       record_cb(jcr, dev, block, trec);
	       free_record(trec);
	       break;
	    }
            Dmsg3(100, "After mount next vol. stat=%s blk=%d rem=%d\n", rec_state_to_str(rec), 
		  block->BlockNumber, rec->remainder);
	    /*
	     * We just have a new tape up, now read the label (first record)
	     *	and pass it off to the callback routine, then continue
	     *	most likely reading the previous record.
	     */
	    read_block_from_device(dev, block);
	    read_record_from_block(block, trec);
	    get_session_record(dev, trec, &sessrec);
	    record_cb(jcr, dev, block, trec);
	    free_record(trec);
	    goto next_record;
	 }
	 if (dev->state & ST_EOF) {
            Emsg2(M_INFO, 0, "Got EOF on device %s, Volume \"%s\"\n", 
		  dev_name(dev), jcr->VolumeName);
            Dmsg0(20, "read_record got eof. try again\n");
	    continue;
	 }
	 if (dev->state & ST_SHORT) {
	    Emsg0(M_INFO, 0, dev->errmsg);
	    continue;
	 }
//	 display_error_status();
	 ok = FALSE;
	 break;
      }
      if (verbose) {
         Dmsg2(10, "Block: %d blen=%d\n", block->BlockNumber, block->block_len);
      }

next_record:
      record = 0;
      for (rec->state=0; !is_block_empty(rec); ) {
	 if (!read_record_from_block(block, rec)) {
            Dmsg3(10, "!read-break. stat=%s blk=%d rem=%d\n", rec_state_to_str(rec), 
		  block->BlockNumber, rec->remainder);
	    break;
	 }
         Dmsg3(10, "read-OK. stat=%s blk=%d rem=%d\n", rec_state_to_str(rec), 
		  block->BlockNumber, rec->remainder);
	 /*
	  * At this point, we have at least a record header.
	  *  Now decide if we want this record or not, but remember
	  *  before accessing the record, we may need to read again to
	  *  get all the data.
	  */
	 record++;
	 if (verbose) {
            Dmsg6(30, "recno=%d state=%s blk=%d SI=%d ST=%d FI=%d\n", record,
	       rec_state_to_str(rec), block->BlockNumber,
	       rec->VolSessionId, rec->VolSessionTime, rec->FileIndex);
	 }
	 if (debug_level >= 30) {
            Dmsg4(30, "VolSId=%ld FI=%s Strm=%s Size=%ld\n", rec->VolSessionId,
		  FI_to_ascii(rec->FileIndex), 
		  stream_to_ascii(rec->Stream, rec->FileIndex), 
		  rec->data_len);
	 }

	 if (rec->FileIndex == EOM_LABEL) { /* end of tape? */
            Dmsg0(40, "Get EOM LABEL\n");
	    rec->remainder = 0;
	    break;			   /* yes, get out */
	 }

	 /* Some sort of label? */ 
	 if (rec->FileIndex < 0) {
	    get_session_record(dev, rec, &sessrec);
	    record_cb(jcr, dev, block, rec);
	    continue;
	 } /* end if label record */

	 /* 
	  * Apply BSR filter
	  */
	 if (jcr->bsr) {
	    int stat = match_bsr(jcr->bsr, rec, &dev->VolHdr, &sessrec);
	    if (stat == -1) { /* no more possible matches */
	       done = TRUE;   /* all items found, stop */
	       break;
	    } else if (stat == 0) {  /* no match */
	       if (verbose) {
                  Dmsg5(10, "BSR no match rec=%d block=%d SessId=%d SessTime=%d FI=%d\n",
		     record, block->BlockNumber, rec->VolSessionId, rec->VolSessionTime, 
		     rec->FileIndex);
	       }
	       rec->remainder = 0;
               continue;              /* we don't want record, read next one */
	    }
	 }
	 if (is_partial_record(rec)) {
            Dmsg6(10, "Partial, break. recno=%d state=%s blk=%d SI=%d ST=%d FI=%d\n", record,
	       rec_state_to_str(rec), block->BlockNumber,
	       rec->VolSessionId, rec->VolSessionTime, rec->FileIndex);
	    break;		      /* read second part of record */
	 }
	 record_cb(jcr, dev, block, rec);
      }
   }
   if (verbose) {
      printf("%u files found.\n", num_files);
   }
   free_record(rec);
   free_block(block);
   return ok;
}


static void get_session_record(DEVICE *dev, DEV_RECORD *rec, SESSION_LABEL *sessrec)
{
   char *rtype;
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
         rtype = "Unknown";
	 break;
   }
   Dmsg5(10, "%s Record: VolSessionId=%d VolSessionTime=%d JobId=%d DataLen=%d\n",
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
