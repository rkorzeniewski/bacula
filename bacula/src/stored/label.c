/*
 *
 *  label.c  Bacula routines to handle labels 
 *
 *   Kern Sibbald, MM
 *			      
 *
 *   Version $Id$
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

#include "bacula.h"                   /* pull in global headers */
#include "stored.h"                   /* pull in Storage Deamon headers */

/* Forward referenced functions */
static int create_volume_label(DEVICE *dev, char *VolName);
static void create_volume_label_record(JCR *jcr, DEVICE *dev, DEV_RECORD *rec);

extern char my_name[];
extern int debug_level;

char BaculaId[] =  "Bacula 0.9 mortal\n";
unsigned int BaculaTapeVersion = 10;
unsigned int OldCompatableBaculaTapeVersion = 9;


/*
 * Read the volume label
 *
 *  If jcr->VolumeName == NULL, we accept any Bacula Volume
 *  If jcr->VolumeName[0] == 0, we accept any Bacula Volume
 *  otherwise jcr->VolumeName must match the Volume.
 *
 *  If VolName given, ensure that it matches   
 *
 *  Returns VOL_  code as defined in record.h
 *    VOL_NOT_READ
 *    VOL_OK
 *    VOL_NO_LABEL
 *    VOL_IO_ERROR
 *    VOL_NAME_ERROR
 *    VOL_CREATE_ERROR
 *    VOL_VERSION_ERROR
 *    VOL_LABEL_ERROR
 */  
int read_dev_volume_label(JCR *jcr, DEVICE *dev, DEV_BLOCK *block)
{
   char *VolName = jcr->VolumeName;
   DEV_RECORD *record;

   Dmsg2(30, "Enter read_volume_label device=%s vol=%s\n", 
      dev_name(dev), VolName);

   if (dev->state & ST_LABEL) {       /* did we already read label? */
      /* Compare Volume Names */
      if (VolName && *VolName && strcmp(dev->VolHdr.VolName, VolName) != 0) {
         Mmsg(&jcr->errmsg, _("Volume name mismatch on device %s: Wanted %s got %s\n"),
	    dev_name(dev), VolName, dev->VolHdr.VolName);
	 /*
	  * Cancel Job if too many label errors
	  *  => we are in a loop
	  */
	 if (jcr->label_errors > 100) {
	    jcr->JobStatus = JS_Cancelled;
            Jmsg(jcr, M_FATAL, 0, "%s", jcr->errmsg);
	 }
	 return jcr->label_status = VOL_NAME_ERROR;
      }
      Dmsg0(30, "Leave read_volume_label() VOL_OK\n");
      return jcr->label_status = VOL_OK;       /* label already read */
   }

   dev->state &= ~(ST_LABEL|ST_APPEND|ST_READ);  /* set no label, no append */

   if (!rewind_dev(dev)) {
      Mmsg(&jcr->errmsg, _("Couldn't rewind device %s ERR=%s\n"), dev_name(dev),
	 strerror_dev(dev));
      return jcr->label_status = VOL_IO_ERROR;
   }
   strcpy(dev->VolHdr.Id, "**error**");

   /* Read the device label block */
   record = new_record();
   Dmsg0(90, "Big if statement in read_volume_label\n");
   if (!read_block_from_dev(dev, block) || 
       !read_record_from_block(block, record) ||
       !unser_volume_label(dev, record) || 
	strcmp(dev->VolHdr.Id, BaculaId) != 0) {

      Mmsg(&jcr->errmsg, _("Volume on %s is not a Bacula labeled Volume, \
because:\n   %s"), dev_name(dev), strerror_dev(dev));
      free_record(record);
      empty_block(block);
      rewind_dev(dev);
      return jcr->label_status = VOL_NO_LABEL;
   }

   free_record(record);
   empty_block(block);
   rewind_dev(dev);

   if (dev->VolHdr.VerNum != BaculaTapeVersion && 
       dev->VolHdr.VerNum != OldCompatableBaculaTapeVersion) {
      Mmsg(&jcr->errmsg, _("Volume on %s has wrong Bacula version. Wanted %d got %d\n"),
	 dev_name(dev), BaculaTapeVersion, dev->VolHdr.VerNum);
      return jcr->label_status = VOL_VERSION_ERROR;
   }

   /* Compare Volume Names */
   Dmsg2(30, "Compare Vol names: VolName=%s hdr=%s\n", VolName?VolName:"*", dev->VolHdr.VolName);
   if (VolName && *VolName && strcmp(dev->VolHdr.VolName, VolName) != 0) {
      Mmsg(&jcr->errmsg, _("Volume name mismatch. Wanted %s got %s\n"),
	 VolName, dev->VolHdr.VolName);
      /*
       * Cancel Job if too many label errors
       *  => we are in a loop
       */
      if (jcr->label_errors > 100) {
	 jcr->JobStatus = JS_Cancelled;
         Jmsg(jcr, M_FATAL, 0, "%s", jcr->errmsg);
      }
      return jcr->label_status = VOL_NAME_ERROR;
   }
   Dmsg1(30, "Copy vol_name=%s\n", dev->VolHdr.VolName);

   /* We are looking for either an unused Bacula tape (PRE_LABEL) or
    * a Bacula volume label (VOL_LABEL)
    */
   if (dev->VolHdr.LabelType != PRE_LABEL && dev->VolHdr.LabelType != VOL_LABEL) {
      Mmsg(&jcr->errmsg, _("Volume on %s has bad Bacula label type: %x\n"), 
	  dev_name(dev), dev->VolHdr.LabelType);
      return jcr->label_status = VOL_LABEL_ERROR;
   }

   dev->state |= ST_LABEL;	      /* set has Bacula label */
   if (debug_level >= 10) {
      dump_volume_label(dev);
   }
   Dmsg0(30, "Leave read_volume_label() VOL_OK\n");
   return jcr->label_status = VOL_OK;
}

/*  unser_volume_label 
 *  
 * Unserialize the Volume label into the device Volume_Label
 * structure.
 *
 * Assumes that the record is already read.
 *
 * Returns: 0 on error
 *	    1 on success
*/

int unser_volume_label(DEVICE *dev, DEV_RECORD *rec)
{
   ser_declare;

   if (rec->FileIndex != VOL_LABEL && rec->FileIndex != PRE_LABEL) {
      Mmsg3(&dev->errmsg, _("Expecting Volume Label, got FI=%s Stream=%s len=%d\n"), 
	      FI_to_ascii(rec->FileIndex), stream_to_ascii(rec->Stream), 
	      rec->data_len);
      return 0;
   }

   dev->VolHdr.LabelType = rec->FileIndex;
   dev->VolHdr.LabelSize = rec->data_len;


  /* Unserialize the record into the Volume Header */
  ser_begin(rec->data, SER_LENGTH_Volume_Label);
#define Fld(x)	(dev->VolHdr.x)
   unser_string(Fld(Id));

   unser_uint32(Fld(VerNum));

   unser_float64(Fld(label_date));
   unser_float64(Fld(label_time));
   unser_float64(Fld(write_date));
   unser_float64(Fld(write_time));

   unser_string(Fld(VolName));
   unser_string(Fld(PrevVolName));
   unser_string(Fld(PoolName));
   unser_string(Fld(PoolType));
   unser_string(Fld(MediaType));

   unser_string(Fld(HostName));
   unser_string(Fld(LabelProg));
   unser_string(Fld(ProgVersion));
   unser_string(Fld(ProgDate));

   ser_end(rec->data, SER_LENGTH_Volume_Label);
#undef Fld
   Dmsg0(90, "ser_read_vol\n");
   if (debug_level >= 90) {
      dump_volume_label(dev);	   
   }
   return 1;
}

/*
 * Put a volume label into the block
 *
 *  Returns: 0 on failure
 *	     1 on success
 */
int write_volume_label_to_block(JCR *jcr, DEVICE *dev, DEV_BLOCK *block)
{
   DEV_RECORD rec;

   Dmsg0(20, "write Label in write_volume_label_to_block()\n");
   memset(&rec, 0, sizeof(rec));
   rec.data = get_memory(SER_LENGTH_Volume_Label);

   create_volume_label_record(jcr, dev, &rec);

   empty_block(block);		      /* Volume label always at beginning */
   if (!write_record_to_block(block, &rec)) {
      free_pool_memory(rec.data);
      Jmsg1(jcr, M_FATAL, 0, _("Cannot write Volume label to block for device %s\n"),
	 dev_name(dev));
      return 0;
   } else {
      Dmsg1(90, "Wrote label of %d bytes to block\n", rec.data_len);
   }
   free_pool_memory(rec.data);
   return 1;
}

/* 
 *  create_volume_label_record
 *   Serialize label (from dev->VolHdr structure) into device record.
 *   Assumes that the dev->VolHdr structure is properly 
 *   initialized.
*/
static void create_volume_label_record(JCR *jcr, DEVICE *dev, DEV_RECORD *rec)
{
   ser_declare;
   struct date_time dt;

   /* Serialize the label into the device record. */

   ser_begin(rec->data, SER_LENGTH_Volume_Label);
#define Fld(x)	(dev->VolHdr.x)
   ser_string(Fld(Id));

   ser_uint32(Fld(VerNum));

   ser_float64(Fld(label_date));
   ser_float64(Fld(label_time));

   get_current_time(&dt);
   Fld(write_date) = dt.julian_day_number;
   Fld(write_time) = dt.julian_day_fraction;

   ser_float64(Fld(write_date));
   ser_float64(Fld(write_time));

   ser_string(Fld(VolName));
   ser_string(Fld(PrevVolName));
   ser_string(Fld(PoolName));
   ser_string(Fld(PoolType));
   ser_string(Fld(MediaType));

   ser_string(Fld(HostName));
   ser_string(Fld(LabelProg));
   ser_string(Fld(ProgVersion));
   ser_string(Fld(ProgDate));

   ser_end(rec->data, SER_LENGTH_Volume_Label);
   rec->data_len = ser_length(rec->data);
   rec->FileIndex = Fld(LabelType);
   rec->VolSessionId = jcr->VolSessionId;
   rec->VolSessionTime = jcr->VolSessionTime;
   rec->Stream = jcr->NumVolumes;
   Dmsg2(100, "Created Vol label rec: FI=%s len=%d\n", FI_to_ascii(rec->FileIndex),
      rec->data_len);
#undef Fld
}     


/*
 * Create a volume label in memory
 *  Returns: 0 on error
 *	     1 on success
 */
static int create_volume_label(DEVICE *dev, char *VolName)
{
   struct date_time dt;
   DEVRES *device = (DEVRES *)dev->device;

   Dmsg0(90, "Start create_volume_label()\n");

   ASSERT(dev != NULL);

   memset(&dev->VolHdr, 0, sizeof(dev->VolHdr));

   /* ***FIXME*** we really need to get the volume name,    
    * pool name, and pool type from the database.
    * We also need to pickup the MediaType.
    */
   strcpy(dev->VolHdr.Id, BaculaId);
   dev->VolHdr.VerNum = BaculaTapeVersion;
   dev->VolHdr.LabelType = PRE_LABEL;  /* Mark tape as unused */
   strcpy(dev->VolHdr.VolName, VolName);
   strcpy(dev->VolHdr.PoolName, "Default");
   strcpy(dev->VolHdr.MediaType, device->media_type);
   strcpy(dev->VolHdr.PoolType, "Backup");

   /* Put label time/date in header */
   get_current_time(&dt);
   dev->VolHdr.label_date = dt.julian_day_number;
   dev->VolHdr.label_time = dt.julian_day_fraction;

   strcpy(dev->VolHdr.LabelProg, my_name);
   sprintf(dev->VolHdr.ProgVersion, "Ver. %s %s", VERSION, DATE);
   sprintf(dev->VolHdr.ProgDate, "Build %s %s", __DATE__, __TIME__);
   dev->state |= ST_LABEL;	      /* set has Bacula label */
   if (debug_level >= 90) {
      dump_volume_label(dev);
   }
   return 1;
}

/*
 * Write a Volume Label
 *  !!! Note, this is ONLY used for writing
 *	      a fresh volume label.  Any data
 *	      after the label will be destroyed,
 *	      in fact, we write the label 5 times !!!!
 * 
 *  This routine expects that open_device() was previously called.
 *
 *  This routine should be used only when labeling a blank tape.
 */
int write_volume_label_to_dev(JCR *jcr, DEVRES *device, char *VolName, char *PoolName)
{
   DEVICE *dev = device->dev;
   DEV_RECORD rec;   
   DEV_BLOCK *block;
   int stat = 1;


   Dmsg0(99, "write_volume_label()\n");
   if (!create_volume_label(dev, VolName)) {
      return 0;
   }
   strcpy(dev->VolHdr.MediaType, device->media_type);
   strcpy(dev->VolHdr.VolName, VolName);
   strcpy(dev->VolHdr.PoolName, PoolName);

   if (!rewind_dev(dev)) {
      Dmsg2(30, "Bad status on %s from rewind. ERR=%s\n", dev_name(dev), strerror_dev(dev));
      return 0;
   }

   block = new_block(dev);
   memset(&rec, 0, sizeof(rec));
   rec.data = (char *) get_memory(SER_LENGTH_Volume_Label);
   create_volume_label_record(jcr, dev, &rec);
   rec.Stream = 0;

   if (!write_record_to_block(block, &rec)) {
      Dmsg2(30, "Bad Label write on %s. ERR=%s\n", dev_name(dev), strerror_dev(dev));
      free_block(block);
      free_pool_memory(rec.data);
      return 0;
   } else {
      Dmsg2(30, "Wrote label of %d bytes to %s\n", rec.data_len, dev_name(dev));
   }
   free_pool_memory(rec.data);
      
   Dmsg0(99, "Call write_block_to_device()\n");
   if (!write_block_to_dev(dev, block)) {
      Dmsg2(30, "Bad Label write on %s. ERR=%s\n", dev_name(dev), strerror_dev(dev));
      stat = 9;
   }
   Dmsg0(99, " Wrote block to device\n");
     
   flush_dev(dev);
   weof_dev(dev, 1);
   dev->state |= ST_LABEL;

   if (debug_level >= 20)  {
      dump_volume_label(dev);
   }
   free_block(block);
   return stat;
}     


/*
 * Create session label
 *  The pool memory must be released by the calling program
 */
void create_session_label(JCR *jcr, DEV_RECORD *rec, int label)
{
   ser_declare;
   struct date_time dt;

   rec->sync	       = 1;	    /* wait for completion */
   rec->VolSessionId   = jcr->VolSessionId;
   rec->VolSessionTime = jcr->VolSessionTime;
   rec->Stream	       = jcr->JobId;

   ser_begin(rec->data, SER_LENGTH_Session_Label);
   ser_string(BaculaId);
   ser_uint32(BaculaTapeVersion);

   ser_uint32(jcr->JobId);

   get_current_time(&dt);
   ser_float64(dt.julian_day_number);
   ser_float64(dt.julian_day_fraction);

   ser_string(jcr->pool_name);
   ser_string(jcr->pool_type);
   ser_string(jcr->job_name);	      /* base Job name */
   ser_string(jcr->client_name);
   /* Added in VerNum 10 */
   ser_string(jcr->Job);	      /* Unique name of this Job */
   ser_string(jcr->fileset_name);
   ser_uint32(jcr->JobType);
   ser_uint32(jcr->JobLevel);

   if (label == EOS_LABEL) {
      ser_uint32(jcr->JobFiles);
      ser_uint64(jcr->JobBytes);
      ser_uint32(jcr->start_block);
      ser_uint32(jcr->end_block);
      ser_uint32(jcr->start_file);
      ser_uint32(jcr->end_file);
      ser_uint32(jcr->JobErrors);
   }
   ser_end(rec->data, SER_LENGTH_Session_Label);
   rec->data_len = ser_length(rec->data);
}

/* Write session label
 *  Returns: 0 on failure
 *	     1 on success 
 */
int write_session_label(JCR *jcr, DEV_BLOCK *block, int label)
{
   DEVICE *dev = jcr->device->dev;
   DEV_RECORD *rec;

   rec = new_record();
   Dmsg1(90, "session_label record=%x\n", rec);
   switch (label) {
      case SOS_LABEL:
	 jcr->start_block = dev->block_num;
	 jcr->start_file  = dev->file;
	 break;
      case EOS_LABEL:
	 jcr->end_block = dev->block_num;
	 jcr->end_file = dev->file;
	 break;
      default:
         Jmsg1(jcr, M_ABORT, 0, _("Bad session label = %d\n"), label);
	 break;
   }
   create_session_label(jcr, rec, label);
   rec->FileIndex = label;

   while (!write_record_to_block(block, rec)) {
      Dmsg2(190, "!write_record data_len=%d rem=%d\n", rec->data_len,
		 rec->remainder);
      if (!write_block_to_device(jcr, dev, block)) {
         Dmsg0(90, "Got session label write_block_to_dev error.\n");
         Jmsg(jcr, M_FATAL, 0, _("Error writing Session label to %s: %s\n"), 
			   dev_vol_name(dev), strerror(errno));
	 free_record(rec);
	 return 0;
      }
   }

   Dmsg6(20, "Write sesson_label record JobId=%d FI=%s SessId=%d Strm=%s len=%d\n\
remainder=%d\n", jcr->JobId,
      FI_to_ascii(rec->FileIndex), rec->VolSessionId, 
      stream_to_ascii(rec->Stream), rec->data_len,
      rec->remainder);

   free_record(rec);
   Dmsg2(20, "Leave write_session_label Block=%d File=%d\n", 
      dev->block_num, dev->file);
   return 1;
}

void dump_volume_label(DEVICE *dev)
{
   int dbl;
   uint32_t File;
   char *LabelType, buf[30];
   struct tm tm;
   struct date_time dt;

   File = dev->file;
   switch (dev->VolHdr.LabelType) {
      case PRE_LABEL:
         LabelType = "PRE_LABEL";
	 break;
      case VOL_LABEL:
         LabelType = "VOL_LABEL";
	 break;
      case EOM_LABEL:
         LabelType = "EOM_LABEL";
	 break;
      case SOS_LABEL:
         LabelType = "SOS_LABEL";
	 break;
      case EOS_LABEL:
         LabelType = "EOS_LABEL";
	 break;
      default:
	 LabelType = buf;
         sprintf(buf, "Unknown %d", dev->VolHdr.LabelType);
	 break;
   }
	      
   
   dbl = debug_level;
   debug_level = 1;
   Pmsg11(-1, "\nVolume Label:\n\
Id                : %s\
VerNo             : %d\n\
VolName           : %s\n\
PrevVolName       : %s\n\
VolFile           : %d\n\
LabelType         : %s\n\
LabelSize         : %d\n\
PoolName          : %s\n\
MediaType         : %s\n\
PoolType          : %s\n\
HostName          : %s\n\
",
	     dev->VolHdr.Id, dev->VolHdr.VerNum,
	     dev->VolHdr.VolName, dev->VolHdr.PrevVolName,
	     File, LabelType, dev->VolHdr.LabelSize, 
	     dev->VolHdr.PoolName, dev->VolHdr.MediaType, 
	     dev->VolHdr.PoolType, dev->VolHdr.HostName);

   dt.julian_day_number   = dev->VolHdr.label_date;
   dt.julian_day_fraction = dev->VolHdr.label_time;
   tm_decode(&dt, &tm);
   Pmsg5(-1, "\
Date label written: %04d-%02d-%02d at %02d:%02d\n", 
      tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday, tm.tm_hour, tm.tm_min);
   debug_level = dbl;
}

int unser_session_label(SESSION_LABEL *label, DEV_RECORD *rec) 
{
   ser_declare;

   unser_begin(rec->data, SER_LENGTH_Session_Label);
   unser_string(label->Id);
   unser_uint32(label->VerNum);
   unser_uint32(label->JobId);
   unser_float64(label->write_date);
   unser_float64(label->write_time);
   unser_string(label->PoolName);
   unser_string(label->PoolType);
   unser_string(label->JobName);
   unser_string(label->ClientName);
   if (label->VerNum > 9) {
      unser_string(label->Job); 	 /* Unique name of this Job */
      unser_string(label->FileSetName);
      unser_uint32(label->JobType);
      unser_uint32(label->JobLevel);
   }
   if (rec->FileIndex == EOS_LABEL) {
      unser_uint32(label->JobFiles);
      unser_uint64(label->JobBytes);
      unser_uint32(label->start_block);
      unser_uint32(label->end_block);
      unser_uint32(label->start_file);
      unser_uint32(label->end_file);
      unser_uint32(label->JobErrors);
   }	  
   return 1;
}


static void dump_session_label(DEV_RECORD *rec, char *type)
{
   int dbl;
   struct date_time dt;
   struct tm tm;
   SESSION_LABEL label;
   char ec1[30], ec2[30], ec3[30], ec4[30], ec5[30], ec6[30], ec7[30];


   unser_session_label(&label, rec);
   dbl = debug_level;
   debug_level = 1;
   Pmsg6(-1, "\n%s Record:\n\
JobId             : %d\n\
PoolName          : %s\n\
PoolType          : %s\n\
JobName           : %s\n\
ClientName        : %s\n\
",    type, 
      label.JobId, label.PoolName, label.PoolType,
      label.JobName, label.ClientName);

   if (label.VerNum >= 10) {
      Pmsg4(-1, "\
Job (unique name) : %s\n\
FileSet           : %s\n\
JobType           : %c\n\
JobLevel          : %c\n\
", label.Job, label.FileSetName, label.JobType, label.JobLevel);
   }

   if (rec->FileIndex == EOS_LABEL) {
      Pmsg7(-1, "\
JobFiles          : %s\n\
JobBytes          : %s\n\
StartBlock        : %s\n\
EndBlock          : %s\n\
StartFile         : %s\n\
EndFile           : %s\n\
JobErrors         : %s\n\
",
	 edit_uint64_with_commas(label.JobFiles, ec1),
	 edit_uint64_with_commas(label.JobBytes, ec2),
	 edit_uint64_with_commas(label.start_block, ec3),
	 edit_uint64_with_commas(label.end_block, ec4),
	 edit_uint64_with_commas(label.start_file, ec5),
	 edit_uint64_with_commas(label.end_file, ec6),
	 edit_uint64_with_commas(label.JobErrors, ec7));
   }
   dt.julian_day_number   = label.write_date;
   dt.julian_day_fraction = label.write_time;
   tm_decode(&dt, &tm);
   Pmsg5(-1, "\
Date written      : %04d-%02d-%02d at %02d:%02d\n", 
      tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday, tm.tm_hour, tm.tm_min);

   debug_level = dbl;
}

void dump_label_record(DEVICE *dev, DEV_RECORD *rec, int verbose)
{
   char *type;
   int dbl;

   dbl = debug_level;
   debug_level = 1;
   switch (rec->FileIndex) {
      case PRE_LABEL:
         type = "Fresh Volume";   
	 break;
      case VOL_LABEL:
         type = "Volume";
	 break;
      case SOS_LABEL:
         type = "Begin Session";
	 break;
      case EOS_LABEL:
         type = "End Session";
	 break;
      case EOM_LABEL:
         type = "End of Media";
	 break;
      default:
         type = "Unknown";
	 break;
   }
   if (verbose) {
      switch (rec->FileIndex) {
	 case PRE_LABEL:
	 case VOL_LABEL:
	    unser_volume_label(dev, rec);
	    dump_volume_label(dev);
	    break;
	 case SOS_LABEL:
	    dump_session_label(rec, type);
	    break;
	 case EOS_LABEL:
	    dump_session_label(rec, type);
	    break;
	 case EOM_LABEL:
            Pmsg5(-1, "%s Record: VSessId=%d VSessTime=%d JobId=%d DataLen=%d\n",
	       type, rec->VolSessionId, rec->VolSessionTime, rec->Stream, rec->data_len);
	    break;
	 default:
            Pmsg5(-1, "%s Record: VSessId=%d VSessTime=%d JobId=%d DataLen=%d\n",
	       type, rec->VolSessionId, rec->VolSessionTime, rec->Stream, rec->data_len);
	    break;
      }
   } else {
      Pmsg5(-1, "%s Record: VSessId=%d VSessTime=%d JobId=%d DataLen=%d\n",
	 type, rec->VolSessionId, rec->VolSessionTime, rec->Stream, rec->data_len);
   }
   debug_level = dbl;
}
