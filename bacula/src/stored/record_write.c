/*
   Bacula® - The Network Backup Solution

   Copyright (C) 20xx-2014 Free Software Foundation Europe e.V.

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
 *   record_write.c -- Volume (tape/disk) record write functions
 *
 *            Kern Sibbald, April MMI
 *              added BB02 format October MMII
 *              added aligned format November MMXII
 *
 */


#include "bacula.h"
#include "stored.h"

/* Imported functions */

static const int dbgep = 250;         /* debug execution path */
static const int dbgel = 250;         /* debug Enter/Leave code */

struct rechdr {
   int32_t FileIndex;
   uint32_t data_len;
   uint32_t reclen;
   int32_t Stream;
   int32_t oStream;
};

/*
 * Flush block to disk
 */
bool flush_block(DCR *dcr)
{
   bool rtn = false;

   if (!is_block_empty(dcr->block)) {
      Dmsg0(dbgep, "=== wpath 53 flush_block\n");
      Dmsg3(190, "Call flush_block BlockAddr=%lld nbytes=%d block=%x\n",
         dcr->block->BlockAddr, dcr->block->binbuf, dcr->block);
      dump_block(dcr->block, "Flush_block");
      if (dcr->jcr->is_canceled() || !dcr->write_block_to_device()) {
         Dmsg0(dbgep, "=== wpath 54 flush_block\n");
         Dmsg0(190, "Failed to write block to device, return false.\n");
         goto get_out;
      }
      empty_block(dcr->block);
   }
   rtn = true;

get_out:
   return rtn;
}

/*
 * Write a header record to the block.
 */
static bool write_header_to_block(DCR *dcr, DEV_BLOCK *block, DEV_RECORD *rec)
{
   ser_declare;

   Dmsg0(dbgep, "=== wpath 11 write_header_to_block\n");
   rec->remlen = block->buf_len - block->binbuf;
   /* Require enough room to write a full header */
   if (rec->remlen < WRITE_RECHDR_LENGTH) {
      Dmsg0(dbgep, "=== wpath 12 write_header_to_block\n");
      Dmsg4(190, "Fail remlen=%d<%d reclen buf_len=%d binbuf=%d\n",
         rec->remlen, WRITE_RECHDR_LENGTH, block->buf_len, block->binbuf);
      rec->remainder = rec->data_len + WRITE_RECHDR_LENGTH;
      return false;
   }
   ser_begin(block->bufp, WRITE_RECHDR_LENGTH);
   if (BLOCK_VER == 1) {
      Dmsg0(dbgep, "=== wpath 13 write_header_to_block\n");
      ser_uint32(rec->VolSessionId);
      ser_uint32(rec->VolSessionTime);
   } else {
      Dmsg0(dbgep, "=== wpath 14 write_header_to_block\n");
      block->VolSessionId = rec->VolSessionId;
      block->VolSessionTime = rec->VolSessionTime;
   }
   ser_int32(rec->FileIndex);
   ser_int32(rec->Stream);
   ser_uint32(rec->data_len);

   block->bufp += WRITE_RECHDR_LENGTH;
   block->binbuf += WRITE_RECHDR_LENGTH;
   rec->remlen -= WRITE_RECHDR_LENGTH;
   rec->remainder = rec->data_len;
   if (rec->FileIndex > 0) {
      Dmsg0(dbgep, "=== wpath 15 write_header_to_block\n");
      /* If data record, update what we have in this block */
      if (block->FirstIndex == 0) {
         Dmsg0(dbgep, "=== wpath 16 write_header_to_block\n");
         block->FirstIndex = rec->FileIndex;
      }
      block->LastIndex = rec->FileIndex;
   }

   //dump_block(block, "Add header");
   return true;
}

/*
 * If the prior block was not big enough to hold the
 *  whole record, write a continuation header record.
 */
static void write_continue_header_to_block(DCR *dcr, DEV_BLOCK *block, DEV_RECORD *rec)
{
   ser_declare;

   Dmsg0(dbgep, "=== wpath 17 write_cont_hdr_to_block\n");
   rec->remlen = block->buf_len - block->binbuf;
   /*
    * We have unwritten bytes from a previous
    * time. Presumably we have a new buffer (possibly
    * containing a volume label), so the new header
    * should be able to fit in the block -- otherwise we have
    * an error.  Note, we have to continue splitting the
    * data record if it is longer than the block.
    *
    * First, write the header.
    *
    * Every time we write a header and it is a continuation
    * of a previous partially written record, we store the
    * Stream as -Stream in the record header.
    */
   ser_begin(block->bufp, WRITE_RECHDR_LENGTH);
   if (BLOCK_VER == 1) {
      Dmsg0(dbgep, "=== wpath 18 write_cont_hdr_to_block\n");
      ser_uint32(rec->VolSessionId);
      ser_uint32(rec->VolSessionTime);
   } else {
      Dmsg0(dbgep, "=== wpath 19 write_cont_hdr_to_block\n");
      block->VolSessionId = rec->VolSessionId;
      block->VolSessionTime = rec->VolSessionTime;
   }
   ser_int32(rec->FileIndex);
   if (rec->remainder > rec->data_len) {
      Dmsg0(dbgep, "=== wpath 20 write_cont_hdr_to_block\n");
      ser_int32(rec->Stream);      /* normal full header */
      ser_uint32(rec->data_len);
      rec->remainder = rec->data_len; /* must still do data record */
   } else {
      Dmsg0(dbgep, "=== wpath 21 write_cont_hdr_to_block\n");
      ser_int32(-rec->Stream);     /* mark this as a continuation record */
      ser_uint32(rec->remainder);  /* bytes to do */
   }

   /* Require enough room to write a full header */
   ASSERT(rec->remlen >= WRITE_RECHDR_LENGTH);

   block->bufp += WRITE_RECHDR_LENGTH;
   block->binbuf += WRITE_RECHDR_LENGTH;
   rec->remlen -= WRITE_RECHDR_LENGTH;
   if (rec->FileIndex > 0) {
      Dmsg0(dbgep, "=== wpath 22 write_cont_hdr_to_block\n");
      /* If data record, update what we have in this block */
      if (block->FirstIndex == 0) {
         Dmsg0(dbgep, "=== wpath 23 write_cont_hdr_to_block\n");
         block->FirstIndex = rec->FileIndex;
      }
      block->LastIndex = rec->FileIndex;
   }
   //dump_block(block, "Add cont header");
}

/*
 */
static bool write_data_to_block(DCR *dcr, DEV_BLOCK *block, DEV_RECORD *rec)
{
   Dmsg0(dbgep, "=== wpath 24 write_data_to_block\n");
   rec->remlen = block->buf_len - block->binbuf;
   /* Write as much of data as possible */
   if (rec->remlen >= rec->remainder) {
      Dmsg0(dbgep, "=== wpath 25 write_data_to_block\n");
      memcpy(block->bufp, rec->data+rec->data_len-rec->remainder,
             rec->remainder);
      block->bufp += rec->remainder;
      block->binbuf += rec->remainder;
      rec->remainder = 0;
   } else {
      Dmsg0(dbgep, "=== wpath 26 write_data_to_block\n");
      memcpy(block->bufp, rec->data+rec->data_len-rec->remainder,
             rec->remlen);
      block->bufp += rec->remlen;
      block->binbuf += rec->remlen;
      rec->remainder -= rec->remlen;
      return false;                /* did partial transfer */
   }
   return true;
}

/*
 * Write a record to the block -- handles writing out full
 *   blocks by writing them to the device.
 *
 *  Returns: false means the block could not be written to tape/disk.
 *           true  on success (all bytes written to the block).
 */
bool DCR::write_record(DEV_RECORD *rec)
{
   Enter(dbgel);
   Dmsg0(dbgep, "=== wpath 33 write_record\n");
   while (!write_record_to_block(this, rec)) {
      Dmsg2(850, "!write_record_to_block data_len=%d rem=%d\n", rec->data_len,
                 rec->remainder);
      if (jcr->is_canceled()) {
         Leave(dbgel);
         return false;
      }
      if (!write_block_to_device()) {
         Dmsg0(dbgep, "=== wpath 34 write_record\n");
         Pmsg2(000, "Got write_block_to_dev error on device %s. %s\n",
            dev->print_name(), dev->bstrerror());
         Leave(dbgel);
         return false;
      }
      Dmsg2(850, "!write_record_to_block data_len=%d rem=%d\n", rec->data_len,
                 rec->remainder);
   }
   Leave(dbgel);
   return true;
}

/*
 * Write a record to the block
 *
 *  Returns: false on failure (none or partially written)
 *           true  on success (all bytes written)
 *
 *  and remainder returned in packet.
 *
 *  We require enough room for the header, and we deal with
 *  two special cases. 1. Only part of the record may have
 *  been transferred the last time (when remainder is
 *  non-zero), and 2. The remaining bytes to write may not
 *  all fit into the block.
 *
 */
bool write_record_to_block(DCR *dcr, DEV_RECORD *rec)
{
   char buf1[100], buf2[100];
   bool rtn;

   Enter(dbgel);
   Dmsg0(dbgep, "=== wpath 35 enter write_record_to_block\n");
   Dmsg7(200, "write_record_to_block() state=%d FI=%s SessId=%d"
         " Strm=%s len=%d rem=%d remainder=%d\n", rec->wstate,
         FI_to_ascii(buf1, rec->FileIndex), rec->VolSessionId,
         stream_to_ascii(buf2, rec->Stream, rec->FileIndex), rec->data_len,
         rec->remlen, rec->remainder);
   Dmsg4(160, "write_rec Strm=%s len=%d rem=%d remainder=%d\n",
         stream_to_ascii(buf2, rec->Stream, rec->FileIndex), rec->data_len,
         rec->remlen, rec->remainder);

   for ( ;; ) {
      Dmsg0(dbgep, "=== wpath 37 top of for loop\n");
      ASSERT(dcr->block->binbuf == (uint32_t) (dcr->block->bufp - dcr->block->buf));
      ASSERT(dcr->block->buf_len >= dcr->block->binbuf);

      switch (rec->wstate) {
      case st_none:
         Dmsg0(dbgep, "=== wpath 38 st_none\n");
         /* Figure out what to do */
         rec->wstate = st_header;
         if (rec->FileIndex < 0) {
            /* Label record  */
            rec->wstate = st_header;
            continue;
         }
         continue;              /* go to next state */

      case st_header:
         /*
          * Write header
          *
          * If rec->remainder is non-zero, we have been called a
          *  second (or subsequent) time to finish writing a record
          *  that did not previously fit into the block.
          */
         Dmsg0(dbgep, "=== wpath 42 st_header\n");
         if (!write_header_to_block(dcr, dcr->block, rec)) {
            Dmsg0(dbgep, "=== wpath 43 st_header\n");
            rec->wstate = st_cont_header;
            goto fail_out;
         }
         Dmsg0(dbgep, "=== wpath 44 st_header\n");
         rec->wstate = st_data;
         continue;

      case st_cont_header:
         Dmsg0(dbgep, "=== wpath 45 st_cont_header\n");
         write_continue_header_to_block(dcr, dcr->block, rec);
         rec->wstate = st_data;
         if (rec->remlen == 0) {
            Dmsg0(dbgep, "=== wpath 46 st_cont_header\n");
            goto fail_out;
         }
         continue;

      /*
       * We come here only once for each record
       */
      case st_data:
         /*
          * Write data
          *
          * Part of it may have already been transferred, and we
          * may not have enough room to transfer the whole this time.
          */
         Dmsg0(dbgep, "=== wpath 47 st_data\n");
         if (rec->remainder > 0) {
            Dmsg0(dbgep, "=== wpath 48 st_data\n");
            if (!write_data_to_block(dcr, dcr->block, rec)) {
               Dmsg0(dbgep, "=== wpath 49 st_data\n");
               rec->wstate = st_cont_header;
               goto fail_out;
            }
         }
         rec->remainder = 0;                /* did whole transfer */
         rec->wstate = st_none;
         goto get_out;

      default:
         Dmsg0(dbgep, "=== wpath 67!!!! default\n");
         Dmsg0(50, "Something went wrong. Default state.\n");
         rec->wstate = st_none;
         goto get_out;
      }
   }
get_out:
   rtn = true;
   goto out;
fail_out:
   rtn = false;
out:
   Leave(dbgel);
   return rtn;
}
