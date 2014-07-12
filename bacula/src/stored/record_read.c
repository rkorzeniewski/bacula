/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2001-2014 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from many
   others, a complete list can be found in the file AUTHORS.

   You may use this file and others of this release according to the
   license defined in the LICENSE file, which includes the Affero General
   Public License, v3.0 ("AGPLv3") and some additional permissions and
   terms pursuant to its AGPLv3 Section 7.

   Bacula® is a registered trademark of Kern Sibbald.
*/
/*
 *
 *   record_read.c -- Volume (tape/disk) record read functions
 *
 *            Kern Sibbald, April MMI
 *              added BB02 format October MMII
 */


#include "bacula.h"
#include "stored.h"

/* Imported subroutines */


static const int read_dbglvl = 200;
static const int dbgep = 250;         /* debug execution path */

/*
 * Read the header record
 */
static bool read_header(DCR *dcr, DEV_BLOCK *block, DEV_RECORD *rec)
{
   ser_declare;
   uint32_t VolSessionId;
   uint32_t VolSessionTime;
   int32_t  FileIndex;
   int32_t  Stream;
   uint32_t rhl;
   char buf1[100], buf2[100];

   Dmsg0(dbgep, "=== rpath 1 read_header\n");
   /* Clear state flags */
   rec->state_bits = 0;
   if (block->dev->is_tape()) {
      rec->state_bits |= REC_ISTAPE;
   }
   rec->Block = ((DEVICE *)block->dev)->EndBlock;
   rec->File = ((DEVICE *)block->dev)->EndFile;

   /*
    * Get the header. There is always a full header,
    * otherwise we find it in the next block.
    */
   Dmsg3(read_dbglvl, "Block=%d Ver=%d block_len=%u\n",
         block->BlockNumber, block->BlockVer, block->block_len);
   if (block->BlockVer == 1) {
      rhl = RECHDR1_LENGTH;
   } else {
      rhl = RECHDR2_LENGTH;
   }
   if (rec->remlen >= rhl) {
      Dmsg0(dbgep, "=== rpath 2 begin unserial header\n");
      Dmsg4(read_dbglvl, "read_header: remlen=%d data_len=%d rem=%d blkver=%d\n",
            rec->remlen, rec->data_len, rec->remainder, block->BlockVer);

      unser_begin(block->bufp, WRITE_RECHDR_LENGTH);
      if (block->BlockVer == 1) {
         unser_uint32(VolSessionId);
         unser_uint32(VolSessionTime);
      } else {
         VolSessionId = block->VolSessionId;
         VolSessionTime = block->VolSessionTime;
      }
      unser_int32(FileIndex);
      unser_int32(Stream);
      unser_uint32(rec->data_bytes);

      block->bufp += rhl;
      block->binbuf -= rhl;
      rec->remlen -= rhl;

      /* If we are looking for more (remainder!=0), we reject anything
       *  where the VolSessionId and VolSessionTime don't agree
       */
      if (rec->remainder && (rec->VolSessionId != VolSessionId ||
                             rec->VolSessionTime != VolSessionTime)) {
         rec->state_bits |= REC_NO_MATCH;
         Dmsg0(read_dbglvl, "remainder and VolSession doesn't match\n");
         Dmsg0(dbgep, "=== rpath 4 VolSession no match\n");
         return false;             /* This is from some other Session */
      }

      /* if Stream is negative, it means that this is a continuation
       * of a previous partially written record.
       */
      if (Stream < 0) {               /* continuation record? */
         Dmsg0(dbgep, "=== rpath 5 negative stream\n");
         Dmsg1(read_dbglvl, "Got negative Stream => continuation. remainder=%d\n",
            rec->remainder);
         rec->state_bits |= REC_CONTINUATION;
         if (!rec->remainder) {       /* if we didn't read previously */
            Dmsg0(dbgep, "=== rpath 6 no remainder\n");
            rec->data_len = 0;        /* return data as if no continuation */
         } else if (rec->Stream != -Stream) {
            Dmsg0(dbgep, "=== rpath 7 wrong cont stream\n");
            rec->state_bits |= REC_NO_MATCH;
            return false;             /* This is from some other Session */
         }
         rec->Stream = -Stream;       /* set correct Stream */
         rec->maskedStream = rec->Stream & STREAMMASK_TYPE;
      } else {                        /* Regular record */
         Dmsg0(dbgep, "=== rpath 8 normal stream\n");
         rec->Stream = Stream;
         rec->maskedStream = rec->Stream & STREAMMASK_TYPE;
         rec->data_len = 0;           /* transfer to beginning of data */
      }
      rec->VolSessionId = VolSessionId;
      rec->VolSessionTime = VolSessionTime;
      rec->FileIndex = FileIndex;
      if (FileIndex > 0) {
         Dmsg0(dbgep, "=== rpath 9 FileIndex>0\n");
         if (block->FirstIndex == 0) {
            Dmsg0(dbgep, "=== rpath 10 FirstIndex\n");
            block->FirstIndex = FileIndex;
         }
         block->LastIndex = rec->FileIndex;
      }

      Dmsg6(read_dbglvl, "read_header: FI=%s SessId=%d Strm=%s len=%u rec->remlen=%d data_len=%d\n",
         FI_to_ascii(buf1, rec->FileIndex), rec->VolSessionId,
         stream_to_ascii(buf2, rec->Stream, rec->FileIndex), rec->data_bytes, rec->remlen,
         rec->data_len);
   } else {
      Dmsg0(dbgep, "=== rpath 11a block out of records\n");
      /*
       * No more records in this block because the number
       * of remaining bytes are less than a record header
       * length, so return empty handed, but indicate that
       * he must read again. By returning, we allow the
       * higher level routine to fetch the next block and
       * then reread.
       */
      Dmsg0(read_dbglvl, "read_header: End of block\n");
      rec->state_bits |= (REC_NO_HEADER | REC_BLOCK_EMPTY);
      empty_block(block);                      /* mark block empty */
      return false;
   }

   /* Sanity check */
   if (rec->data_bytes >= MAX_BLOCK_LENGTH) {
      Dmsg0(dbgep, "=== rpath 11b maxlen too big\n");
      /*
       * Something is wrong, force read of next block, abort
       *   continuing with this block.
       */
      rec->state_bits |= (REC_NO_HEADER | REC_BLOCK_EMPTY);
      empty_block(block);
      Jmsg2(dcr->jcr, M_WARNING, 0, _("Sanity check failed. maxlen=%d datalen=%d. Block discarded.\n"),
         MAX_BLOCK_LENGTH, rec->data_bytes);
      return false;
   }

   rec->data = check_pool_memory_size(rec->data, rec->data_len+rec->data_bytes);
   rec->rstate = st_data;
   return true;
}

/*
 * We have just read a header, now read the data into the record.
 *  Note, if we do not read the full record, we will return to
 *  read the next header, which will then come back here later
 *  to finish reading the full record.
 */
static void read_data(DEV_BLOCK *block, DEV_RECORD *rec)
{
   char buf1[100], buf2[100];

   Dmsg0(dbgep, "=== rpath 22 read_data\n");
   /*
    * At this point, we have read the header, now we
    * must transfer as much of the data record as
    * possible taking into account: 1. A partial
    * data record may have previously been transferred,
    * 2. The current block may not contain the whole data
    * record.
    */
   if (rec->remlen >= rec->data_bytes) {
      Dmsg0(dbgep, "=== rpath 23 full record\n");
      /* Got whole record */
      memcpy(rec->data+rec->data_len, block->bufp, rec->data_bytes);
      block->bufp += rec->data_bytes;
      block->binbuf -= rec->data_bytes;
      rec->data_len += rec->data_bytes;
      rec->remainder = 0;
      Dmsg5(190, "Rdata full FI=%s SessId=%d Strm=%s len=%d block=%p\n",
         FI_to_ascii(buf1, rec->FileIndex), rec->VolSessionId,
         stream_to_ascii(buf2, rec->Stream, rec->FileIndex), rec->data_len,
         block);
   } else {
      Dmsg0(dbgep, "=== rpath 24 partial record\n");
      /* Partial record */
      memcpy(rec->data+rec->data_len, block->bufp, rec->remlen);
      block->bufp += rec->remlen;
      block->binbuf -= rec->remlen;
      rec->data_len += rec->remlen;
      rec->remainder = 1;             /* partial record transferred */
      Dmsg1(read_dbglvl, "read_data: partial xfered=%d\n", rec->data_len);
      rec->state_bits |= (REC_PARTIAL_RECORD | REC_BLOCK_EMPTY);
   }
}


/*
 * Read a Record from the block
 *  Returns: false if nothing read or if the continuation record does not match.
 *                 In both of these cases, a block read must be done.
 *           true  if at least the record header was read, this
 *                 routine may have to be called again with a new
 *                 block if the entire record was not read.
 */
bool read_record_from_block(DCR *dcr,  DEV_RECORD *rec)
{
   bool rtn;

   Dmsg0(dbgep, "=== rpath 1 Enter read_record_from block\n");
   for ( ;; ) {
      switch (rec->rstate) {
      case st_none:
         dump_block(dcr->block, "st_none");
      case st_header:
         Dmsg0(dbgep, "=== rpath 33 st_header\n");
         rec->remlen = dcr->block->binbuf;
         /* Note read_header sets rec->rstate on return true */
         if (!read_header(dcr, dcr->block, rec)) {  /* sets state */
            Dmsg0(dbgep, "=== rpath 34 failed read header\n");
            Dmsg0(read_dbglvl, "read_header returned EOF.\n");
            goto fail_out;
         }
         continue;

      case st_data:
         Dmsg0(dbgep, "=== rpath 37 st_data\n");
         read_data(dcr->block, rec);
         rec->rstate = st_header;         /* next pass look for a header */
         goto get_out;

      default:
         Dmsg0(dbgep, "=== rpath 50 default\n");
         Dmsg0(0, "======= In default !!!!!\n");
         Pmsg1(190, "Read: unknown state=%d\n", rec->rstate);
         goto fail_out;
      }
   }
get_out:
   char buf1[100], buf2[100];
   Dmsg5(read_dbglvl, "read_rec return: FI=%s Strm=%s len=%d rem=%d remainder=%d\n",
         FI_to_ascii(buf1, rec->FileIndex),
         stream_to_ascii(buf2, rec->Stream, rec->FileIndex), rec->data_len,
         rec->remlen, rec->remainder);
   rtn = true;
   goto out;
fail_out:
   rec->rstate = st_none;
   rtn = false;
out:
   return rtn;
}
