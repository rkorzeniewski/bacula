/*
 *
 *   record.c -- tape record handling functions
 *
 *		Kern Sibbald, April MMI
 *
 */
/*
   Copyright (C) 2000, 2001, 2002 Kern Sibbald and John Walker

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

extern int debug_level;

/*
 * Convert a FileIndex into a printable
 * ASCII string.  Not reentrant.
 */
char *FI_to_ascii(int fi)
{
   static char buf[20];
   if (fi >= 0) {
      sprintf(buf, "%d", fi);
      return buf;
   }
   switch (fi) {
   case PRE_LABEL:
      return "PRE_LABEL";
   case VOL_LABEL:
      return "VOL_LABEL";
   case EOM_LABEL:
      return "EOM_LABEL";
   case SOS_LABEL:
      return "SOS_LABEL";
   case EOS_LABEL:
      return "EOS_LABEL";
   default:
     sprintf(buf, "unknown: %d", fi);
     return buf;
   }
}


/* 
 * Convert a Stream ID into a printable
 * ASCII string.  Not reentrant.
 */
char *stream_to_ascii(int stream)
{
    static char buf[20];
    switch (stream) {
    case STREAM_UNIX_ATTRIBUTES:
       return "UATTR";
    case STREAM_FILE_DATA:
       return "DATA";
    case STREAM_MD5_SIGNATURE:
       return "MD5";
    case STREAM_GZIP_DATA:
       return "GZIP";
    default:
       sprintf(buf, "%d", stream);
       return buf;     
    }
}

/* 
 * Return a new record entity
 */
DEV_RECORD *new_record(void)
{
   DEV_RECORD *rec;

   rec = (DEV_RECORD *) get_memory(sizeof(DEV_RECORD));
   memset(rec, 0, sizeof(DEV_RECORD));
   rec->data = (char *) get_pool_memory(PM_MESSAGE);
   return rec;
}

/*
 * Free the record entity 
 *
 */
void free_record(DEV_RECORD *rec) 
{
   Dmsg0(150, "Enter free_record.\n");
   free_pool_memory(rec->data);
   Dmsg0(150, "Data buf is freed.\n");
   free_pool_memory((POOLMEM *)rec);
   Dmsg0(150, "Leave free_record.\n");
} 

/*
 * Read a record from a block
 *   if necessary, read the block from the device without locking
 *   if necessary, handle getting a new Volume
 *
 *  Returns: 0 on failure
 *	     1 on success
 */
int read_record(DEVICE *dev, DEV_BLOCK *block, DEV_RECORD *record)
{
   Dmsg2(90, "read_record() dev=%x state=%x\n", dev, dev->state);

   while (!read_record_from_block(block, record)) {
      Dmsg2(90, "!read_record_from_block data_len=%d rem=%d\n", record->data_len,
		record->remainder);
      if (!read_block_from_dev(dev, block)) {
	 /**** ****FIXME**** handle getting a new Volume */
         Dmsg0(200, "===== Got read block I/O error ======\n");
	 return 0;
      }
   }
   Dmsg4(90, "read_record FI=%s SessId=%d Strm=%s len=%d\n",
	      FI_to_ascii(record->FileIndex), record->VolSessionId, 
	      stream_to_ascii(record->Stream), record->data_len);
   return 1;
}


/*
 * Write a record to block 
 *   if necessary, write the block to the device with locking
 *   if necessary, handle getting a new Volume
 *
 *  Returns: 0 on failure
 *	     1 on success
 */
int write_record_device(JCR *jcr, DEVICE *dev, DEV_BLOCK *block, DEV_RECORD *record)
{
   Dmsg2(190, "write_device() dev=%x state=%x\n", dev, dev->state);

   while (!write_record_to_block(block, record)) {
      Dmsg2(190, "!write_record data_len=%d rem=%d\n", record->data_len,
		 record->remainder);
      if (!write_block_to_device(jcr, dev, block)) {
         Dmsg0(90, "Got write_block_to_dev error.\n");
	 return 0;
      }
   }
   Dmsg4(190, "write_record FI=%s SessId=%d Strm=%s len=%d\n",
      FI_to_ascii(record->FileIndex), record->VolSessionId, 
      stream_to_ascii(record->Stream), record->data_len);
   return 1;
}

/*
 * Write a Record to the block
 *
 *  Returns: 0 on failure (none or partially written)
 *	     1 on success (all bytes written)
 *
 *  and remainder returned in packet.
 *
 *  We require enough room for the header, and we deal with
 *  two special cases. 1. Only part of the record may have
 *  been transferred the last time (when remainder is
 *  non-zero), and 2. The remaining bytes to write may not
 *  all fit into the block.
 */
int write_record_to_block(DEV_BLOCK *block, DEV_RECORD *rec)
{
   ser_declare;
   uint32_t remlen;

   sm_check(__FILE__, __LINE__, False);
   remlen = block->buf_len - block->binbuf;

   ASSERT(block->binbuf == (uint32_t) (block->bufp - block->buf));
   ASSERT(remlen >= 0);

   Dmsg6(190, "write_record_to_block() FI=%s SessId=%d Strm=%s len=%d\n\
rem=%d remainder=%d\n",
      FI_to_ascii(rec->FileIndex), rec->VolSessionId, 
      stream_to_ascii(rec->Stream), rec->data_len,
      remlen, rec->remainder);

   if (rec->remainder == 0) {
      /* Require enough room to write a full header */
      if (remlen >= RECHDR_LENGTH) {
	 ser_begin(block->bufp, RECHDR_LENGTH);
	 ser_uint32(rec->VolSessionId);
	 ser_uint32(rec->VolSessionTime);
	 ser_int32(rec->FileIndex);
	 ser_int32(rec->Stream);
	 ser_uint32(rec->data_len);
	 ASSERT(ser_length(block->bufp) == RECHDR_LENGTH);

	 block->bufp += RECHDR_LENGTH;
	 block->binbuf += RECHDR_LENGTH;
	 remlen -= RECHDR_LENGTH;
	 rec->remainder = rec->data_len;
      } else {
	 rec->remainder = rec->data_len + RECHDR_LENGTH;
	 sm_check(__FILE__, __LINE__, False);
	 return 0;
      }
   } else {
      /* 
       * We are here to write unwritten bytes from a previous
       * time. Presumably we have a new buffer (possibly 
       * containing a volume label), so the new header 
       * should be able to fit in the block -- otherwise we have
       * an error.  Note, we may have to continue splitting the
       * data record though.
       * 
       *  First, write the header, then write as much as 
       * possible of the data record.
       */
      ser_begin(block->bufp, RECHDR_LENGTH);
      ser_uint32(rec->VolSessionId);
      ser_uint32(rec->VolSessionTime);
      ser_int32(rec->FileIndex);
      if (rec->remainder > rec->data_len) {
	 ser_int32(rec->Stream);      /* normal full header */
	 ser_uint32(rec->data_len);
	 rec->remainder = rec->data_len; /* must still do data record */
      } else {
	 ser_int32(-rec->Stream);     /* mark this as a continuation record */
	 ser_uint32(rec->remainder);  /* bytes to do */
      }
      ASSERT(ser_length(block->bufp) == RECHDR_LENGTH);

      /* Require enough room to write a full header */
      ASSERT(remlen >= RECHDR_LENGTH);

      block->bufp += RECHDR_LENGTH;
      block->binbuf += RECHDR_LENGTH;
      remlen -= RECHDR_LENGTH;
   }
   if (remlen == 0) {
      sm_check(__FILE__, __LINE__, False);
      return 0; 		      /* partial transfer */
   }

   /* Now deal with data record.
    * Part of it may have already been transferred, and we 
    * may not have enough room to transfer the whole this time.
    */
   if (rec->remainder > 0) {
      /* Write as much of data as possible */
      if (remlen >= rec->remainder) {
	 memcpy(block->bufp, rec->data+rec->data_len-rec->remainder,
		rec->remainder);
	 block->bufp += rec->remainder;
	 block->binbuf += rec->remainder;
      } else {
	 memcpy(block->bufp, rec->data+rec->data_len-rec->remainder, 
		remlen);
	 if (!sm_check_rtn(__FILE__, __LINE__, False)) {
	    /* We damaged a buffer */
            Dmsg6(0, "Damaged block FI=%s SessId=%d Strm=%s len=%d\n\
rem=%d remainder=%d\n",
	       FI_to_ascii(rec->FileIndex), rec->VolSessionId, 
	       stream_to_ascii(rec->Stream), rec->data_len,
	       remlen, rec->remainder);
            Dmsg5(0, "Damaged block: bufp=%x binbuf=%d buf_len=%d rem=%d moved=%d\n",
	       block->bufp, block->binbuf, block->buf_len, block->buf_len-block->binbuf,
	       remlen);
            Dmsg2(0, "Damaged block: buf=%x binbuffrombuf=%d \n",
	       block->buf, block->bufp-block->buf);

               Emsg0(M_ABORT, 0, "Damaged buffer\n");
	 }

	 block->bufp += remlen;
	 block->binbuf += remlen;
	 rec->remainder -= remlen;
	 return 0;		      /* did partial transfer */
      }
   }
   rec->remainder = 0;		      /* did whole transfer */
   sm_check(__FILE__, __LINE__, False);
   return 1;
}


/*
 * Read a Record from the block
 *  Returns: 0 on failure
 *	     1 on success
 */
int read_record_from_block(DEV_BLOCK *block, DEV_RECORD *rec)
{
   ser_declare;
   uint32_t data_bytes;
   int32_t Stream;
   uint32_t remlen;

   remlen = block->binbuf;

   /* 
    * Get the header. There is always a full header,
    * otherwise we find it in the next block.
    */
   if (remlen >= RECHDR_LENGTH) {
      Dmsg3(90, "read_record_block: remlen=%d data_len=%d rem=%d\n", 
	    remlen, rec->data_len, rec->remainder);
/*    memcpy(rec->ser_buf, block->bufp, RECHDR_LENGTH); */

      unser_begin(block->bufp, RECHDR_LENGTH);
      unser_uint32(rec->VolSessionId);
      unser_uint32(rec->VolSessionTime);
      unser_int32(rec->FileIndex);
      unser_int32(Stream);
      unser_uint32(data_bytes);

      ASSERT(unser_length(block->bufp) == RECHDR_LENGTH);
      block->bufp += RECHDR_LENGTH;
      block->binbuf -= RECHDR_LENGTH;
      remlen -= RECHDR_LENGTH;

      if (Stream < 0) { 	      /* continuation record? */
         Dmsg1(500, "Got negative Stream => continuation. remainder=%d\n", 
	    rec->remainder);
	 /* ****FIXME**** add code to verify that this
	  *  is the correct continuation of the previous
	  *  record.
	  */
         if (!rec->remainder) {       /* if we didn't read previously */
	    rec->data_len = 0;	      /* simply return data */
	 }
	 rec->Stream = -Stream;       /* set correct Stream */
      } else {			      /* Regular record */
	 rec->Stream = Stream;
	 rec->data_len = 0;	      /* transfer to beginning of data */
      }
      Dmsg6(90, "rd_rec_blk() got FI=%s SessId=%d Strm=%s len=%d\n\
remlen=%d data_len=%d\n",
	 FI_to_ascii(rec->FileIndex), rec->VolSessionId, 
	 stream_to_ascii(rec->Stream), data_bytes, remlen, rec->data_len);
   } else {
      Dmsg0(90, "read_record_block: nothing\n");
      if (!rec->remainder) {
	 rec->remainder = 1;	      /* set to expect continuation */
	 rec->data_len = 0;	      /* no data transferred */
      }
      return 0;
   }

   ASSERT(data_bytes < MAX_BLOCK_LENGTH);	/* temp sanity check */

   rec->data = (char *) check_pool_memory_size(rec->data, rec->data_len+data_bytes);
   
   /*
    * At this point, we have read the header, now we
    * must transfer as much of the data record as 
    * possible taking into account: 1. A partial
    * data record may have previously been transferred,
    * 2. The current block may not contain the whole data
    * record.
    */
   if (remlen >= data_bytes) {
      /* Got whole record */
      memcpy(rec->data+rec->data_len, block->bufp, data_bytes);
      block->bufp += data_bytes;
      block->binbuf -= data_bytes;
      rec->data_len += data_bytes;
   } else {
      /* Partial record */
      memcpy(rec->data+rec->data_len, block->bufp, remlen);
      block->bufp += remlen;
      block->binbuf -= remlen;
      rec->data_len += remlen;
      rec->remainder = 1;	      /* partial record transferred */
      Dmsg1(90, "read_record_block: partial xfered=%d\n", rec->data_len);
      return 0;
   }
   rec->remainder = 0;
   Dmsg4(90, "Rtn full rd_rec_blk FI=%s SessId=%d Strm=%s len=%d\n",
      FI_to_ascii(rec->FileIndex), rec->VolSessionId, 
      stream_to_ascii(rec->Stream), rec->data_len);
   return 1;			      /* transferred full record */
}
