/*
 *
 *   Bacula Tape manipulation program
 *
 *    Has various tape manipulation commands -- mostly for
 *    use in determining how tapes really work.
 *
 *     Kern Sibbald, April MM
 *
 *   Note, this program reads stored.conf, and will only
 *     talk to devices that are configured.
 *
 *   Version $Id$
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


/* External subroutines */
extern void free_config_resources();

/* Exported variables */
int quit = 0;
char buf[100000];
int bsize = TAPE_BSIZE;
char VolName[100];

DEVICE *dev = NULL;
DEVRES *device = NULL;

	    
/* Forward referenced subroutines */
static void do_tape_cmds();
static void helpcmd();
static void scancmd();
static void rewindcmd();
static void clearcmd();
static void wrcmd();
static void eodcmd();
static void fillcmd();
static void unfillcmd();
static int flush_block(DEV_BLOCK *block, int dump);
static void record_cb(JCR *jcr, DEVICE *dev, DEV_BLOCK *block, DEV_RECORD *rec);
static int my_mount_next_read_volume(JCR *jcr, DEVICE *dev, DEV_BLOCK *block);


/* Static variables */
#define CONFIG_FILE "bacula-sd.conf"
char *configfile;

static BSR *bsr = NULL;
static char cmd[1000];
static int signals = TRUE;
static int ok;
static int stop;
static uint64_t vol_size;
static uint64_t VolBytes;
static time_t now;
static double kbs;
static long file_index;
static int verbose = 0;
static int end_of_tape = 0;
static uint32_t LastBlock = 0;
static uint32_t eot_block;
static uint32_t eot_block_len;
static uint32_t eot_FileIndex;
static int dumped = 0;

static char *VolumeName = NULL;

static JCR *jcr = NULL;


static void usage();
static void terminate_btape(int sig);
int get_cmd(char *prompt);


int write_dev(DEVICE *dev, char *buf, size_t len) 
{
   Emsg0(M_ABORT, 0, "write_dev not implemented.\n");
   return 0;
}

int read_dev(DEVICE *dev, char *buf, size_t len)
{
   Emsg0(M_ABORT, 0, "read_dev not implemented.\n");
   return 0;
}


/*********************************************************************
 *
 *	   Main Bacula Pool Creation Program
 *
 */
int main(int argc, char *argv[])
{
   int ch;

   /* Sanity checks */
   if (TAPE_BSIZE % DEV_BSIZE != 0 || TAPE_BSIZE / DEV_BSIZE == 0) {
      Emsg2(M_ABORT, 0, "Tape block size (%d) not multiple of system size (%d)\n",
	 TAPE_BSIZE, DEV_BSIZE);
   }
   if (TAPE_BSIZE != (1 << (ffs(TAPE_BSIZE)-1))) {
      Emsg1(M_ABORT, 0, "Tape block size (%d) is not a power of 2\n", TAPE_BSIZE);
   }

   printf("Tape block granularity is %d bytes.\n", TAPE_BSIZE);

   working_directory = "/tmp";
   my_name_is(argc, argv, "btape");
   init_msg(NULL, NULL);

   while ((ch = getopt(argc, argv, "b:c:d:sv?")) != -1) {
      switch (ch) {
         case 'b':                    /* bootstrap file */
	    bsr = parse_bsr(NULL, optarg);
//	    dump_bsr(bsr);
	    break;

         case 'c':                    /* specify config file */
	    if (configfile != NULL) {
	       free(configfile);
	    }
	    configfile = bstrdup(optarg);
	    break;

         case 'd':                    /* set debug level */
	    debug_level = atoi(optarg);
	    if (debug_level <= 0) {
	       debug_level = 1; 
	    }
	    break;

         case 's':
	    signals = FALSE;
	    break;

         case 'v':
	    verbose++;
	    break;

         case '?':
	 default:
	    helpcmd();
	    exit(0);

      }  
   }
   argc -= optind;
   argv += optind;


   
   if (signals) {
      init_signals(terminate_btape);
   }

   if (configfile == NULL) {
      configfile = bstrdup(CONFIG_FILE);
   }

   daemon_start_time = time(NULL);

   parse_config(configfile);


   /* See if we can open a device */
   if (argc == 0) {
      Pmsg0(000, "No archive name specified.\n");
      usage();
      exit(1);
   } else if (argc != 1) {
      Pmsg0(000, "Improper number of arguments specified.\n");
      usage();
      exit(1);
   }

   jcr = setup_jcr("btape", argv[0], bsr);
   dev = setup_to_access_device(jcr, 0);     /* acquire for write */
   if (!dev) {
      exit(1);
   }

   Dmsg0(200, "Do tape commands\n");
   do_tape_cmds();
  
   terminate_btape(0);
   return 0;
}

static void terminate_btape(int stat)
{

   sm_check(__FILE__, __LINE__, False);
   if (configfile) {
      free(configfile);
   }
   free_config_resources();

   if (dev) {
      term_dev(dev);
   }

   if (debug_level > 10)
      print_memory_pool_stats(); 

   free_jcr(jcr);
   jcr = NULL;

   if (bsr) {
      free_bsr(bsr);
   }

   term_msg();
   close_memory_pool(); 	      /* free memory in pool */

   sm_dump(False);
   exit(stat);
}

void quitcmd()
{
   quit = 1;
}

/*
 * Write a label to the tape   
 */
static void labelcmd()
{
   DEVRES *device;
   int found = 0;

   LockRes();
   for (device=NULL; (device=(DEVRES *)GetNextRes(R_DEVICE, (RES *)device)); ) {
      if (strcmp(device->device_name, dev->dev_name) == 0) {
	 jcr->device = device;	      /* Arggg a bit of duplication here */
	 device->dev = dev;
	 dev->device = device;
	 found = 1;
	 break;
      }
   } 
   UnlockRes();
   if (!found) {
      Pmsg2(0, "Could not find device %s in %s\n", dev->dev_name, configfile);
      return;
   }

   if (VolumeName) {
      strcpy(cmd, VolumeName);
   } else {
      if (!get_cmd("Enter Volume Name: ")) {
	 return;
      }
   }
	 
   if (!(dev->state & ST_OPENED)) {
      if (!open_device(dev)) {
         Pmsg1(0, "Device open failed. ERR=%s\n", strerror_dev(dev));
      }
   }
   write_volume_label_to_dev(jcr, device, cmd, "Default");
}

/*
 * Read the tape label	 
 */
static void readlabelcmd()
{
   int save_debug_level = debug_level;
   int stat;
   DEV_BLOCK *block;

   block = new_block(dev);
   stat = read_dev_volume_label(jcr, dev, block);
   switch (stat) {
      case VOL_NO_LABEL:
         Pmsg0(0, "Volume has no label.\n");
	 break;
      case VOL_OK:
         Pmsg0(0, "Volume label read correctly.\n");
	 break;
      case VOL_IO_ERROR:
         Pmsg1(0, "I/O error on device: ERR=%s", strerror_dev(dev));
	 break;
      case VOL_NAME_ERROR:
         Pmsg0(0, "Volume name error\n");
	 break;
      case VOL_CREATE_ERROR:
         Pmsg1(0, "Error creating label. ERR=%s", strerror_dev(dev));
	 break;
      case VOL_VERSION_ERROR:
         Pmsg0(0, "Volume version error.\n");
	 break;
      case VOL_LABEL_ERROR:
         Pmsg0(0, "Bad Volume label type.\n");
	 break;
      default:
         Pmsg0(0, "Unknown error.\n");
	 break;
   }

   debug_level = 20;
   dump_volume_label(dev); 
   debug_level = save_debug_level;
   free_block(block);
}


/*
 * Load the tape should have prevously been taken
 * off line, otherwise this command is not necessary.
 */
static void loadcmd()
{

   if (!load_dev(dev)) {
      Pmsg1(0, "Bad status from load. ERR=%s\n", strerror_dev(dev));
   } else
      Pmsg1(0, "Loaded %s\n", dev_name(dev));
}

/*
 * Rewind the tape.   
 */
static void rewindcmd()
{
   if (!rewind_dev(dev)) {
      Pmsg1(0, "Bad status from rewind. ERR=%s\n", strerror_dev(dev));
      clrerror_dev(dev, -1);
   } else {
      Pmsg1(0, "Rewound %s\n", dev_name(dev));
   }
}

/*
 * Clear any tape error   
 */
static void clearcmd()
{
   clrerror_dev(dev, -1);
}

/*
 * Write and end of file on the tape   
 */
static void weofcmd()
{
   int stat;

   if ((stat = weof_dev(dev, 1)) < 0) {
      Pmsg2(0, "Bad status from weof %d. ERR=%s\n", stat, strerror_dev(dev));
      return;
   } else {
      Pmsg1(0, "Wrote EOF to %s\n", dev_name(dev));
   }
}


/* Go to the end of the medium -- raw command	
 * The idea was orginally that the end of the Bacula
 * medium would be flagged differently. This is not
 * currently the case. So, this is identical to the
 * eodcmd().
 */
static void eomcmd()
{
   if (!eod_dev(dev)) {
      Pmsg1(0, _("Bad status from eod. ERR=%s\n"), strerror_dev(dev));
      return;
   } else {
      Pmsg0(0, _("Moved to end of media\n"));
   }
}

/*
 * Go to the end of the media (either hardware determined
 *  or defined by two eofs.
 */
static void eodcmd()
{
   eomcmd();
}

/*
 * Backspace file   
 */
static void bsfcmd()
{
   int stat;

   if ((stat=bsf_dev(dev, 1)) < 0) {
      Pmsg1(0, _("Bad status from bsf. ERR=%s\n"), strerror(errno));
   } else {
      Pmsg0(0, _("Back spaced one file.\n"));
   }
}

/*
 * Backspace record   
 */
static void bsrcmd()
{
   int stat;

   if ((stat=bsr_dev(dev, 1)) < 0) {
      Pmsg1(0, _("Bad status from bsr. ERR=%s\n"), strerror(errno));
   } else {
      Pmsg0(0, _("Back spaced one record.\n"));
   }
}

/*
 * List device capabilities as defined in the 
 *  stored.conf file.
 */
static void capcmd()
{
   printf(_("Device capabilities:\n"));
   printf("%sEOF ", dev->capabilities & CAP_EOF ? "" : "!");
   printf("%sBSR ", dev->capabilities & CAP_BSR ? "" : "!");
   printf("%sBSF ", dev->capabilities & CAP_BSF ? "" : "!");
   printf("%sFSR ", dev->capabilities & CAP_FSR ? "" : "!");
   printf("%sFSF ", dev->capabilities & CAP_FSF ? "" : "!");
   printf("%sEOM ", dev->capabilities & CAP_EOM ? "" : "!");
   printf("%sREM ", dev->capabilities & CAP_REM ? "" : "!");
   printf("%sRACCESS ", dev->capabilities & CAP_RACCESS ? "" : "!");
   printf("%sAUTOMOUNT ", dev->capabilities & CAP_AUTOMOUNT ? "" : "!");
   printf("%sLABEL ", dev->capabilities & CAP_LABEL ? "" : "!");
   printf("%sANONVOLS ", dev->capabilities & CAP_ANONVOLS ? "" : "!");
   printf("%sALWAYSOPEN ", dev->capabilities & CAP_ALWAYSOPEN ? "" : "!");
   printf("\n");

   printf(_("Device status:\n"));
   printf("%sOPENED ", dev->state & ST_OPENED ? "" : "!");
   printf("%sTAPE ", dev->state & ST_TAPE ? "" : "!");
   printf("%sLABEL ", dev->state & ST_LABEL ? "" : "!");
   printf("%sMALLOC ", dev->state & ST_MALLOC ? "" : "!");
   printf("%sAPPEND ", dev->state & ST_APPEND ? "" : "!");
   printf("%sREAD ", dev->state & ST_READ ? "" : "!");
   printf("%sEOT ", dev->state & ST_EOT ? "" : "!");
   printf("%sWEOT ", dev->state & ST_WEOT ? "" : "!");
   printf("%sEOF ", dev->state & ST_EOF ? "" : "!");
   printf("%sNEXTVOL ", dev->state & ST_NEXTVOL ? "" : "!");
   printf("%sSHORT ", dev->state & ST_SHORT ? "" : "!");
   printf("\n");

}

/*
 * Test writting larger and larger records.  
 * This is a torture test for records.
 */
static void rectestcmd()
{
   DEV_BLOCK *block;
   DEV_RECORD *rec;
   int i, blkno = 0;

   Pmsg0(0, "Test writting larger and larger records.\n\
This is a torture test for records.\nI am going to write\n\
larger and larger records. It will stop when the record size\n\
plus the header exceeds the block size (by default about 64K\n");


   get_cmd("Do you want to continue? (y/n): ");
   if (cmd[0] != 'y') {
      Pmsg0(000, "Command aborted.\n");
      return;
   }

   sm_check(__FILE__, __LINE__, False);
   block = new_block(dev);
   rec = new_record();

   for (i=1; i<500000; i++) {
      rec->data = check_pool_memory_size(rec->data, i);
      memset(rec->data, i & 0xFF, i);
      rec->data_len = i;
      sm_check(__FILE__, __LINE__, False);
      if (write_record_to_block(block, rec)) {
	 empty_block(block);
	 blkno++;
         Pmsg2(0, "Block %d i=%d\n", blkno, i);
      } else {
	 break;
      }
      sm_check(__FILE__, __LINE__, False);
   }
   free_record(rec);
   free_block(block);
   sm_check(__FILE__, __LINE__, False);
}

/* 
 * This is a general test of Bacula's functions
 *   needed to read and write the tape.
 */
static void testcmd()
{
   Pmsg0(0, "Append files test.\n\n\
I'm going to write one record  in file 0,\n\
                   two records in file 1,\n\
             and three records in file 2\n\n");
   rewindcmd();
   wrcmd();
   weofcmd();	   /* end file 0 */
   wrcmd();
   wrcmd();
   weofcmd();	   /* end file 1 */
   wrcmd();
   wrcmd();
   wrcmd();
   weofcmd();	  /* end file 2 */
//   weofcmd();
   rewindcmd();
   Pmsg0(0, "Now moving to end of media.\n");
   eodcmd();
   Pmsg2(0, "End Append files test.\n\
We should be in file 3. I am at file %d. This is %s\n\n", 
      dev->file, dev->file == 3 ? "correct!" : "NOT correct!!!!");

   Pmsg0(0, "\nNow I am going to attempt to append to the tape.\n");
   wrcmd(); 
   weofcmd();
//   weofcmd();
   rewindcmd();
   scancmd();
   Pmsg0(0, "End Append to the tape test.\n\
The above scan should have four files of:\n\
One record, two records, three records, and one record respectively.\n\n");


   Pmsg0(0, "Append block test.\n\
I'm going to write a block, an EOF, rewind, go to EOM,\n\
then backspace over the EOF and attempt to append a second\n\
block in the first file.\n\n");
   rewindcmd();
   wrcmd();
   weofcmd();
//   weofcmd();
   rewindcmd();
   eodcmd();
   Pmsg2(0, "We should be at file 1. I am at EOM File=%d. This is %s\n",
      dev->file, dev->file == 1 ? "correct!" : "NOT correct!!!!");
   Pmsg0(0, "Doing backspace file.\n");
   bsfcmd();
   Pmsg0(0, "Write second block, hoping to append to first file.\n");
   wrcmd();
   weofcmd();
   rewindcmd();
   Pmsg0(0, "Done writing, scanning results\n");
   scancmd();
   Pmsg0(0, "The above should have one file of two blocks.\n");
}


static void fsfcmd()
{
   int stat;

   if ((stat=fsf_dev(dev, 1)) < 0) {
      Pmsg2(0, "Bad status from fsf %d. ERR=%s\n", stat, strerror_dev(dev));
      return;
   }
   Pmsg0(0, "Forward spaced one file.\n");
}

static void fsrcmd()
{
   int stat;

   if ((stat=fsr_dev(dev, 1)) < 0) {
      Pmsg2(0, "Bad status from fsr %d. ERR=%s\n", stat, strerror_dev(dev));
      return;
   }
   Pmsg0(0, "Forward spaced one record.\n");
}

static void rdcmd()
{
#ifdef xxxxx
   int stat;

   if (!read_dev(dev, buf, 512*126)) {
      Pmsg1(0, "Bad status from read. ERR=%s\n", strerror_dev(dev));
      return;
   }
   Pmsg1(10, "Read %d bytes\n", stat);
#else
   printf("Rdcmd no longer implemented.\n");
#endif
}


static void wrcmd()
{
   DEV_BLOCK *block;
   DEV_RECORD *rec;
   int i;

   sm_check(__FILE__, __LINE__, False);
   block = new_block(dev);
   rec = new_record();
   dump_block(block, "test");

   i = block->buf_len - 100;
   ASSERT (i > 0);
   rec->data = check_pool_memory_size(rec->data, i);
   memset(rec->data, i & 0xFF, i);
   rec->data_len = i;
   sm_check(__FILE__, __LINE__, False);
   if (!write_record_to_block(block, rec)) {
      Pmsg0(0, "Error writing record to block.\n"); 
      goto bail_out;
   }
   if (!write_block_to_dev(dev, block)) {
      Pmsg0(0, "Error writing block to device.\n"); 
      goto bail_out;
   } else {
      Pmsg1(0, "Wrote one record of %d bytes.\n",
	 ((i+TAPE_BSIZE-1)/TAPE_BSIZE) * TAPE_BSIZE);
   }
   Pmsg0(0, "Wrote block to device.\n");

bail_out:
   sm_check(__FILE__, __LINE__, False);
   free_record(rec);
   free_block(block);
   sm_check(__FILE__, __LINE__, False);
}


/*
 * Scan tape by reading block by block. Report what is
 * on the tape.
 */
static void scancmd()
{
   int stat;
   int blocks, tot_blocks, tot_files;
   int block_size;
   uint64_t bytes;


   blocks = block_size = tot_blocks = 0;
   bytes = 0;
   if (dev->state & ST_EOT) {
      Pmsg0(0, "End of tape\n");
      return; 
   }
   update_pos_dev(dev);
   tot_files = dev->file;
   for (;;) {
      if ((stat = read(dev->fd, buf, sizeof(buf))) < 0) {
	 clrerror_dev(dev, -1);
         Mmsg2(&dev->errmsg, "read error on %s. ERR=%s.\n",
	    dev->dev_name, strerror(dev->dev_errno));
         Pmsg2(0, "Bad status from read %d. ERR=%s\n", stat, strerror_dev(dev));
	 if (blocks > 0)
            printf("%d block%s of %d bytes in file %d\n",        
                    blocks, blocks>1?"s":"", block_size, dev->file);
	 return;
      }
      Dmsg1(200, "read status = %d\n", stat);
/*    sleep(1); */
      if (stat != block_size) {
	 update_pos_dev(dev);
	 if (blocks > 0) {
            printf("%d block%s of %d bytes in file %d\n", 
                 blocks, blocks>1?"s":"", block_size, dev->file);
	    blocks = 0;
	 }
	 block_size = stat;
      }
      if (stat == 0) {		      /* EOF */
	 update_pos_dev(dev);
         printf("End of File mark.\n");
	 /* Two reads of zero means end of tape */
	 if (dev->state & ST_EOF)
	    dev->state |= ST_EOT;
	 else {
	    dev->state |= ST_EOF;
	    dev->file++;
	 }
	 if (dev->state & ST_EOT) {
            printf("End of tape\n");
	    break;
	 }
      } else {			      /* Got data */
	 dev->state &= ~ST_EOF;
	 blocks++;
	 tot_blocks++;
	 bytes += stat;
      }
   }
   update_pos_dev(dev);
   tot_files = dev->file - tot_files;
   printf("Total files=%d, blocks=%d, bytes = %" lld "\n", tot_files, tot_blocks, bytes);
}

static void statcmd()
{
   int stat = 0;
   int debug;
   uint32_t status;

   debug = debug_level;
   debug_level = 30;
   if (!status_dev(dev, &status)) {
      Pmsg2(0, "Bad status from status %d. ERR=%s\n", stat, strerror_dev(dev));
   }
#ifdef xxxx
   dump_volume_label(dev);
#endif
   debug_level = debug;
}


/* 
 * First we label the tape, then we fill
 *  it with data get a new tape and write a few blocks.
 */			       
static void fillcmd()
{
   DEV_RECORD rec;
   DEV_BLOCK  *block;
   char ec1[50];
   char *p;

   ok = TRUE;
   stop = FALSE;

   Pmsg0(000, "\n\
This command simulates Bacula writing to a tape.\n\
It command requires two blank tapes, which it\n\
will label and write. It will print a status approximately\n\
every 322 MB, and write an EOF every 3.2 GB.  When the first tape\n\
fills, it will ask for a second, and after writing a few \n\
blocks, it will stop.  Then it will begin re-reading the\n\
This may take a long time. I.e. hours! ...\n\n");

   get_cmd("Insert a blank tape then answer. Do you wish to continue? (y/n): ");
   if (cmd[0] != 'y') {
      Pmsg0(000, "Command aborted.\n");
      return;
   }

   VolumeName = "TestVolume1";
   labelcmd();
   VolumeName = NULL;

   
   Dmsg1(20, "Begin append device=%s\n", dev_name(dev));

   block = new_block(dev);

   /* 
    * Acquire output device for writing.  Note, after acquiring a
    *	device, we MUST release it, which is done at the end of this
    *	subroutine.
    */
   Dmsg0(100, "just before acquire_device\n");
   if (!acquire_device_for_append(jcr, dev, block)) {
      jcr->JobStatus = JS_Cancelled;
      free_block(block);
      return;
   }

   Dmsg0(100, "Just after acquire_device_for_append\n");
   /*
    * Write Begin Session Record
    */
   if (!write_session_label(jcr, block, SOS_LABEL)) {
      jcr->JobStatus = JS_Cancelled;
      Jmsg1(jcr, M_FATAL, 0, _("Write session label failed. ERR=%s\n"),
	 strerror_dev(dev));
      ok = FALSE;
   }

   memset(&rec, 0, sizeof(rec));
   rec.data = get_memory(100000);     /* max record size */
   /* 
    * Fill write buffer with random data
    */
#define REC_SIZE 32768
   p = rec.data;
   for (int i=0; i < REC_SIZE; ) {
      makeSessionKey(p, NULL, 0);
      p += 16;
      i += 16;
   }
   rec.data_len = REC_SIZE;

   /* 
    * Get Data from File daemon, write to device   
    */
   jcr->VolFirstFile = 0;
   time(&jcr->run_time);	      /* start counting time for rates */
   for (file_index = 0; ok && !job_cancelled(jcr); ) {
      uint64_t *lp;
      rec.VolSessionId = jcr->VolSessionId;
      rec.VolSessionTime = jcr->VolSessionTime;
      rec.FileIndex = ++file_index;
      rec.Stream = STREAM_FILE_DATA;
      /* Write file_index at beginning of buffer */
      lp = (uint64_t *)rec.data;
      *lp = (uint64_t)file_index;

      Dmsg4(250, "before writ_rec FI=%d SessId=%d Strm=%s len=%d\n",
	 rec.FileIndex, rec.VolSessionId, stream_to_ascii(rec.Stream, rec.FileIndex), 
	 rec.data_len);
       
      if (!write_record_to_block(block, &rec)) {
         Dmsg2(150, "!write_record_to_block data_len=%d rem=%d\n", rec.data_len,
		    rec.remainder);

	 /* Write block to tape */
	 if (!flush_block(block, 1)) {
	    return;
	 }

	 /* Every 5000 blocks (approx 322MB) report where we are.
	  */
	 if ((block->BlockNumber % 5000) == 0) {
	    now = time(NULL);
	    now -= jcr->run_time;
	    if (now <= 0) {
	       now = 1;
	    }
	    kbs = (double)dev->VolCatInfo.VolCatBytes / (1000 * now);
            Pmsg3(000, "Wrote block=%u, VolBytes=%s rate=%.1f KB/s\n", block->BlockNumber,
	       edit_uint64_with_commas(dev->VolCatInfo.VolCatBytes, ec1), (float)kbs);
	 }
	 /* Every 50000 blocks (approx 3.2MB) write an eof.
	  */
	 if ((block->BlockNumber % 50000) == 0) {
            Pmsg0(000, "Flush block, write EOF\n");
	    flush_block(block, 0);
	    weof_dev(dev, 1);
	    /* The weof resets the block number */
	 }

	 if (block->BlockNumber > 10 && stop) {      /* get out */
	    break;
	 }
      }
      if (!ok) {
         Pmsg0(000, "Not OK\n");
	 break;
      }
      jcr->JobBytes += rec.data_len;   /* increment bytes this job */
      Dmsg4(190, "write_record FI=%s SessId=%d Strm=%s len=%d\n",
	 FI_to_ascii(rec.FileIndex), rec.VolSessionId, 
	 stream_to_ascii(rec.Stream, rec.FileIndex), rec.data_len);
   }
   Dmsg0(000, "Write_end_session_label()\n");
   /* Create Job status for end of session label */
   if (!job_cancelled(jcr) && ok) {
      jcr->JobStatus = JS_Terminated;
   } else if (!ok) {
      jcr->JobStatus = JS_ErrorTerminated;
   }
   if (!write_session_label(jcr, block, EOS_LABEL)) {
      Pmsg1(000, _("Error writting end session label. ERR=%s\n"), strerror_dev(dev));
      ok = FALSE;
   }
   /* Write out final block of this session */
   if (!write_block_to_device(jcr, dev, block)) {
      Pmsg0(000, "Set ok=FALSE after write_block_to_device.\n");
      ok = FALSE;
   }

   /* Release the device */
   if (!release_device(jcr, dev)) {
      Pmsg0(000, "Error in release_device\n");
      ok = FALSE;
   }

   free_block(block);
   free_memory(rec.data);
   Pmsg0(000, "Done with fill command. Now beginning re-read of tapes...\n");

   unfillcmd();
}

/*
 * Read two tapes written by the "fill" command and ensure
 *  that the data is valid.
 */
static void unfillcmd()
{
   DEV_BLOCK *block;

   dumped = 0;
   VolBytes = 0;
   LastBlock = 0;
   block = new_block(dev);

   dev->capabilities |= CAP_ANONVOLS; /* allow reading any volume */
   dev->capabilities &= ~CAP_LABEL;   /* don't label anything here */

   end_of_tape = 0;
   get_cmd("Mount first of two tapes. Press enter when ready: "); 
   
   pm_strcpy(&jcr->VolumeName, "TestVolume1");
   close_dev(dev);
   dev->state &= ~ST_READ;
   if (!acquire_device_for_read(jcr, dev, block)) {
      Pmsg0(000, dev->errmsg);
      return;
   }

   time(&jcr->run_time);	      /* start counting time for rates */
   stop = 0;
   file_index = 0;
   read_records(jcr, dev, record_cb, my_mount_next_read_volume);
   free_block(block);

   Pmsg0(000, "Done with unfillcmd.\n");
}


/* 
 * We are called here from "unfill" for each record on the
 *   tape.
 */
static void record_cb(JCR *jcr, DEVICE *dev, DEV_BLOCK *block, DEV_RECORD *rec)
{

   SESSION_LABEL label;
   if (stop > 1) {		      /* on second tape */
      Pmsg4(000, "Blk: FileIndex=%d: block=%u size=%d vol=%s\n", 
	   rec->FileIndex, block->BlockNumber, block->block_len, dev->VolHdr.VolName);
      Pmsg6(000, "   Rec: VId=%d VT=%d FI=%s Strm=%s len=%d state=%x\n",
	   rec->VolSessionId, rec->VolSessionTime, 
	   FI_to_ascii(rec->FileIndex), stream_to_ascii(rec->Stream, rec->FileIndex),
	   rec->data_len, rec->state);

      if (!dumped) {
	 dumped = 1;
         dump_block(block, "Block not written to previous tape");
      }
   }
   if (rec->FileIndex < 0) {
      if (verbose > 1) {
	 dump_label_record(dev, rec, 1);
      }
      switch (rec->FileIndex) {
	 case PRE_LABEL:
            Pmsg0(000, "Volume is prelabeled. This tape cannot be scanned.\n");
	    return;
	 case VOL_LABEL:
	    unser_volume_label(dev, rec);
            Pmsg3(000, "VOL_LABEL: block=%u size=%d vol=%s\n", block->BlockNumber, 
	       block->block_len, dev->VolHdr.VolName);
	    stop++;
	    break;
	 case SOS_LABEL:
	    unser_session_label(&label, rec);
            Pmsg1(000, "SOS_LABEL: JobId=%u\n", label.JobId);
	    break;
	 case EOS_LABEL:
	    unser_session_label(&label, rec);
            Pmsg2(000, "EOS_LABEL: block=%u JobId=%u\n", block->BlockNumber, 
	       label.JobId);
	    break;
	 case EOM_LABEL:
            Pmsg0(000, "EOM_LABEL:\n");
	    break;
	 case EOT_LABEL:	      /* end of all tapes */
	    char ec1[50];

	    if (LastBlock != block->BlockNumber) {
	       VolBytes += block->block_len;
	    }
	    LastBlock = block->BlockNumber;
	    now = time(NULL);
	    now -= jcr->run_time;
	    if (now <= 0) {
	       now = 1;
	    }
	    kbs = (double)VolBytes / (1000 * now);
            Pmsg3(000, "Read block=%u, VolBytes=%s rate=%.1f KB/s\n", block->BlockNumber,
		     edit_uint64_with_commas(VolBytes, ec1), (float)kbs);

            Pmsg0(000, "End of all tapes.\n");

	    break;
	 default:
	    break;
      }
      return;
   }
   if (LastBlock != block->BlockNumber) {
      VolBytes += block->block_len;
   }
   if ((block->BlockNumber != LastBlock) && (block->BlockNumber % 50000) == 0) {
      char ec1[50];
      now = time(NULL);
      now -= jcr->run_time;
      if (now <= 0) {
	 now = 1;
      }
      kbs = (double)VolBytes / (1000 * now);
      Pmsg3(000, "Read block=%u, VolBytes=%s rate=%.1f KB/s\n", block->BlockNumber,
	       edit_uint64_with_commas(VolBytes, ec1), (float)kbs);
   }
   LastBlock = block->BlockNumber;
   if (end_of_tape) {
      Pmsg1(000, "End of all blocks. Block=%u\n", block->BlockNumber);
   }
}



/*
 * Write current block to tape regardless of whether or
 *   not it is full. If the tape fills, attempt to
 *   acquire another tape.
 */
static int flush_block(DEV_BLOCK *block, int dump)
{
   char ec1[50];
   lock_device(dev);
   if (!write_block_to_dev(dev, block)) {
      Pmsg0(000, strerror_dev(dev));		
      Pmsg3(000, "Block not written: FileIndex=%u Block=%u Size=%u\n", 
	 (unsigned)file_index, block->BlockNumber, block->block_len);
      if (dump) {
         dump_block(block, "Block not written");
      }
      if (!stop) {
	 eot_block = block->BlockNumber;
	 eot_block_len = block->block_len;
	 eot_FileIndex = file_index;
      }
      now = time(NULL);
      now -= jcr->run_time;
      if (now <= 0) {
	 now = 1;
      }
      kbs = (double)dev->VolCatInfo.VolCatBytes / (1000 * now);
      vol_size = dev->VolCatInfo.VolCatBytes;
      Pmsg2(000, "End of tape. VolumeCapacity=%s. Write rate = %.1f KB/s\n", 
	 edit_uint64_with_commas(dev->VolCatInfo.VolCatBytes, ec1), kbs);
      if (!fixup_device_block_write_error(jcr, dev, block)) {
         Pmsg1(000, _("Cannot fixup device error. %s\n"), strerror_dev(dev));
	 ok = FALSE;
	 unlock_device(dev);
	 return 0;
      }
      stop = 1; 						    
      unlock_device(dev);
      return 1;     /* write one more block to next tape then stop */
   }
   unlock_device(dev);
   return 1;
}


struct cmdstruct { char *key; void (*func)(); char *help; }; 
static struct cmdstruct commands[] = {
 {"bsf",        bsfcmd,       "backspace file"},
 {"bsr",        bsrcmd,       "backspace record"},
 {"cap",        capcmd,       "list device capabilities"},
 {"clear",      clearcmd,     "clear tape errors"},
 {"eod",        eodcmd,       "go to end of Bacula data for append"},
 {"test",       testcmd,      "General test Bacula tape functions"},
 {"eom",        eomcmd,       "go to the physical end of medium"},
 {"fill",       fillcmd,      "fill tape, write onto second volume"},
 {"unfill",     unfillcmd,    "read filled tape"},
 {"fsf",        fsfcmd,       "forward space a file"},
 {"fsr",        fsrcmd,       "forward space a record"},
 {"help",       helpcmd,      "print this command"},
 {"label",      labelcmd,     "write a Bacula label to the tape"},
 {"load",       loadcmd,      "load a tape"},
 {"quit",       quitcmd,      "quit btape"},   
 {"rd",         rdcmd,        "read tape"},
 {"readlabel",  readlabelcmd, "read and print the Bacula tape label"},
 {"rectest",    rectestcmd,   "test record handling functions"},
 {"rewind",     rewindcmd,    "rewind the tape"},
 {"scan",       scancmd,      "read tape block by block to EOT and report"}, 
 {"status",     statcmd,      "print tape status"},
 {"weof",       weofcmd,      "write an EOF on the tape"},
 {"wr",         wrcmd,        "write a single record of 2048 bytes"}, 
	     };
#define comsize (sizeof(commands)/sizeof(struct cmdstruct))

static void
do_tape_cmds()
{
   unsigned int i;
   int found;

   while (get_cmd("*")) {
      sm_check(__FILE__, __LINE__, False);
      found = 0;
      for (i=0; i<comsize; i++)       /* search for command */
	 if (fstrsch(cmd,  commands[i].key)) {
	    (*commands[i].func)();    /* go execute command */
	    found = 1;
	    break;
	 }
      if (!found)
         Pmsg1(0, _("%s is an illegal command\n"), cmd);
      if (quit)
	 break;
   }
}

static void helpcmd()
{
   unsigned int i;
   usage();
   printf(_("  Command    Description\n  =======    ===========\n"));
   for (i=0; i<comsize; i++)
      printf("  %-10s %s\n", commands[i].key, commands[i].help);
   printf("\n");
}

static void usage()
{
   fprintf(stderr, _(
"\nVersion: " VERSION " (" DATE ")\n\n"
"Usage: btape [-c config_file] [-d debug_level] [device_name]\n"
"       -c <file>   set configuration file to file\n"
"       -dnn        set debug level to nn\n"
"       -s          turn off signals\n"
"       -t          open the default tape device\n"
"       -?          print this message.\n"  
"\n"));

}

/*	
 * Get next input command from terminal.  This
 * routine is REALLY primitive, and should be enhanced
 * to have correct backspacing, etc.
 */
int 
get_cmd(char *prompt)
{
   int i = 0;
   int ch;
   fprintf(stdout, prompt);

   /* We really should turn off echoing and pretty this
    * up a bit.
    */
   cmd[i] = 0;
   while ((ch = fgetc(stdin)) != EOF) { 
      if (ch == '\n') {
	 strip_trailing_junk(cmd);
	 return 1;
      } else if (ch == 4 || ch == 0xd3 || ch == 0x8) {
	 if (i > 0)
	    cmd[--i] = 0;
	 continue;
      } 
	 
      cmd[i++] = ch;
      cmd[i] = 0;
   }
   quit = 1;
   return 0;
}

/* Dummies to replace askdir.c */
int	dir_get_volume_info(JCR *jcr, int writing) { return 1;}
int	dir_find_next_appendable_volume(JCR *jcr) { return 1;}
int	dir_update_volume_info(JCR *jcr, VOLUME_CAT_INFO *vol, int relabel) { return 1; }
int	dir_create_jobmedia_record(JCR *jcr) { return 1; }
int	dir_update_file_attributes(JCR *jcr, DEV_RECORD *rec) { return 1;}
int	dir_send_job_status(JCR *jcr) {return 1;}


int dir_ask_sysop_to_mount_volume(JCR *jcr, DEVICE *dev)
{
   Pmsg0(000, dev->errmsg);	      /* print reason */
   fprintf(stderr, "Mount Volume \"%s\" on device %s and press return when ready: ",
      jcr->VolumeName, dev_name(dev));
   getchar();	
   return 1;
}

int dir_ask_sysop_to_mount_next_volume(JCR *jcr, DEVICE *dev)
{
   fprintf(stderr, "Mount next Volume on device %s and press return when ready: ",
      dev_name(dev));
   getchar();	
   VolumeName = "TestVolume2";
   labelcmd();
   VolumeName = NULL;
   return 1;
}

static int my_mount_next_read_volume(JCR *jcr, DEVICE *dev, DEV_BLOCK *block)
{
   char ec1[50];

   Pmsg1(000, "End of Volume \"%s\"\n", jcr->VolumeName);

   if (LastBlock != block->BlockNumber) {
      VolBytes += block->block_len;
   }
   LastBlock = block->BlockNumber;
   now = time(NULL);
   now -= jcr->run_time;
   if (now <= 0) {
      now = 1;
   }
   kbs = (double)VolBytes / (1000 * now);
   Pmsg3(000, "Read block=%u, VolBytes=%s rate=%.1f KB/s\n", block->BlockNumber,
	    edit_uint64_with_commas(VolBytes, ec1), (float)kbs);

   if (strcmp(jcr->VolumeName, "TestVolume2") == 0) {
      end_of_tape = 1;
      return 0;
   }
   pm_strcpy(&jcr->VolumeName, "TestVolume2");
   close_dev(dev);
   dev->state &= ~ST_READ; 
   if (!acquire_device_for_read(jcr, dev, block)) {
      Pmsg2(0, "Cannot open Dev=%s, Vol=%s\n", dev_name(dev), jcr->VolumeName);
      return 0;
   }
   return 1;			   /* next volume mounted */
}
