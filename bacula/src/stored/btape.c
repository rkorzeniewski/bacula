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
   Copyright (C) 2000-2004 Kern Sibbald and John Walker

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
char VolName[MAX_NAME_LENGTH];

/*
 * If you change the format of the state file, 
 *  increment this value
 */ 
static uint32_t btape_state_level = 1;

DEVICE *dev = NULL;
DEVRES *device = NULL;

	    
/* Forward referenced subroutines */
static void do_tape_cmds();
static void helpcmd();
static void scancmd();
static void rewindcmd();
static void clearcmd();
static void wrcmd();
static void rrcmd();
static void eodcmd();
static void fillcmd();
static void qfillcmd();
static void statcmd();
static void unfillcmd();
static int flush_block(DEV_BLOCK *block, int dump);
static int record_cb(JCR *jcr, DEVICE *dev, DEV_BLOCK *block, DEV_RECORD *rec);
static int my_mount_next_read_volume(JCR *jcr, DEVICE *dev, DEV_BLOCK *block);
static void scan_blocks();
static void set_volume_name(char *VolName, int volnum);
static void rawfill_cmd();
static void bfill_cmd();
static bool open_the_device();
static char *edit_device_codes(JCR *jcr, char *omsg, char *imsg, char *cmd);
static void autochangercmd();
static void do_unfill();


/* Static variables */
#define CONFIG_FILE "bacula-sd.conf"
char *configfile;

#define MAX_CMD_ARGS 30
static POOLMEM *cmd;
static POOLMEM *args;
static char *argk[MAX_CMD_ARGS];
static char *argv[MAX_CMD_ARGS];
static int argc;

static BSR *bsr = NULL;
static int signals = TRUE;
static int ok;
static int stop;
static uint64_t vol_size;
static uint64_t VolBytes;
static time_t now;
static double kbs;
static int32_t file_index;
static int end_of_tape = 0;
static uint32_t LastBlock = 0;
static uint32_t eot_block;
static uint32_t eot_block_len;
static uint32_t eot_FileIndex;
static int dumped = 0;
static DEV_BLOCK *last_block = NULL;
static DEV_BLOCK *this_block = NULL;
static uint32_t last_file = 0;
static uint32_t last_block_num = 0;
static uint32_t BlockNumber = 0;
static int simple = FALSE;

static char *VolumeName = NULL;
static int vol_num;

static JCR *jcr = NULL;


static void usage();
static void terminate_btape(int sig);
int get_cmd(char *prompt);


/*********************************************************************
 *
 *	   Main Bacula Pool Creation Program
 *
 */
int main(int margc, char *margv[])
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
   my_name_is(margc, margv, "btape");
   init_msg(NULL, NULL);

   while ((ch = getopt(margc, margv, "b:c:d:sv?")) != -1) {
      switch (ch) {
      case 'b':                    /* bootstrap file */
	 bsr = parse_bsr(NULL, optarg);
//	 dump_bsr(bsr, true);
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
   margc -= optind;
   margv += optind;

   cmd = get_pool_memory(PM_FNAME);
   args = get_pool_memory(PM_FNAME);
   
   if (signals) {
      init_signals(terminate_btape);
   }

   if (configfile == NULL) {
      configfile = bstrdup(CONFIG_FILE);
   }

   daemon_start_time = time(NULL);

   parse_config(configfile);


   /* See if we can open a device */
   if (margc == 0) {
      Pmsg0(000, "No archive name specified.\n");
      usage();
      exit(1);
   } else if (margc != 1) {
      Pmsg0(000, "Improper number of arguments specified.\n");
      usage();
      exit(1);
   }

   jcr = setup_jcr("btape", margv[0], bsr, NULL);
   dev = setup_to_access_device(jcr, 0);     /* acquire for write */
   if (!dev) {
      exit(1);
   }
   dev->max_volume_size = 0;	     
   if (!open_the_device()) {
      goto terminate;
   }

   Dmsg0(200, "Do tape commands\n");
   do_tape_cmds();
  
terminate:
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
   if (args) {
      free_pool_memory(args);
      args = NULL;
   }
   if (cmd) {
      free_pool_memory(cmd);
      cmd = NULL;
   }

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

   if (last_block) {
      free_block(last_block);
   }
   if (this_block) {
      free_block(this_block);
   }

   term_msg();
   close_memory_pool(); 	      /* free memory in pool */

   sm_dump(False);
   exit(stat);
}

static bool open_the_device()
{
   DEV_BLOCK *block;
   
   block = new_block(dev);
   lock_device(dev);
   if (!(dev->state & ST_OPENED)) {
      Dmsg1(200, "Opening device %s\n", jcr->VolumeName);
      if (open_dev(dev, jcr->VolumeName, READ_WRITE) < 0) {
         Emsg1(M_FATAL, 0, _("dev open failed: %s\n"), dev->errmsg);
	 unlock_device(dev);
	 free_block(block);
	 return false;
      }
   }
   Dmsg1(000, "open_dev %s OK\n", dev_name(dev));
   unlock_device(dev);
   free_block(block);
   return true;
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
   if (VolumeName) {
      pm_strcpy(&cmd, VolumeName);
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
   write_volume_label_to_dev(jcr, jcr->device, cmd, "Default");
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
   int num = 1;
   if (argc > 1) {
      num = atoi(argk[1]);
   }
   if (num <= 0) {
      num = 1;
   }

   if ((stat = weof_dev(dev, num)) < 0) {
      Pmsg2(0, "Bad status from weof %d. ERR=%s\n", stat, strerror_dev(dev));
      return;
   } else {
      Pmsg3(0, "Wrote %d EOF%s to %s\n", num, num==1?"":"s", dev_name(dev));
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
      Pmsg1(0, "%s", strerror_dev(dev));
      return;
   } else {
      Pmsg0(0, _("Moved to end of medium.\n"));
   }
}

/*
 * Go to the end of the medium (either hardware determined
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
   int num = 1;
   if (argc > 1) {
      num = atoi(argk[1]);
   }
   if (num <= 0) {
      num = 1;
   }

   if (!bsf_dev(dev, num)) {
      Pmsg1(0, _("Bad status from bsf. ERR=%s\n"), strerror_dev(dev));
   } else {
      Pmsg2(0, _("Backspaced %d file%s.\n"), num, num==1?"":"s");
   }
}

/*
 * Backspace record   
 */
static void bsrcmd()
{
   int num = 1;
   if (argc > 1) {
      num = atoi(argk[1]);
   }
   if (num <= 0) {
      num = 1;
   }
   if (!bsr_dev(dev, num)) {
      Pmsg1(0, _("Bad status from bsr. ERR=%s\n"), strerror_dev(dev));
   } else {
      Pmsg2(0, _("Backspaced %d record%s.\n"), num, num==1?"":"s");
   }
}

/*
 * List device capabilities as defined in the 
 *  stored.conf file.
 */
static void capcmd()
{
   printf(_("Configured device capabilities:\n"));
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

   printf(_("Device parameters:\n"));
   printf("Device name: %s\n", dev->dev_name);
   printf("File=%u block=%u\n", dev->file, dev->block_num);
   printf("Min block=%u Max block=%u\n", dev->min_block_size, dev->max_block_size);

   printf("Status:\n");
   statcmd();

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

   Pmsg0(0, "Test writting larger and larger records.\n"
"This is a torture test for records.\nI am going to write\n"
"larger and larger records. It will stop when the record size\n"
"plus the header exceeds the block size (by default about 64K)\n");


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
 * This test attempts to re-read a block written by Bacula
 *   normally at the end of the tape. Bacula will then back up
 *   over the two eof marks, backup over the record and reread
 *   it to make sure it is valid.  Bacula can skip this validation
 *   if you set "Backward space record = no"
 */
static int re_read_block_test()
{
   DEV_BLOCK *block;
   DEV_RECORD *rec;
   int stat = 0;
   int len;

   if (!(dev->capabilities & CAP_BSR)) {
      Pmsg0(-1, _("Skipping read backwards test because BSR turned off.\n"));
      return 0;
   }

   Pmsg0(-1, _("\n=== Write, backup, and re-read test ===\n\n"
      "I'm going to write three records and an eof\n"
      "then backup over the eof and re-read the last record.\n"     
      "Bacula does this after writing the last block on the\n"
      "tape to verify that the block was written correctly.\n"
      "It is not an *essential* feature ...\n\n")); 
   rewindcmd();
   block = new_block(dev);
   rec = new_record();
   rec->data = check_pool_memory_size(rec->data, block->buf_len);
   len = rec->data_len = block->buf_len-100;
   memset(rec->data, 1, rec->data_len);
   if (!write_record_to_block(block, rec)) {
      Pmsg0(0, _("Error writing record to block.\n")); 
      goto bail_out;
   }
   if (!write_block_to_dev(jcr, dev, block)) {
      Pmsg0(0, _("Error writing block to device.\n")); 
      goto bail_out;
   } else {
      Pmsg1(0, _("Wrote first record of %d bytes.\n"), rec->data_len);
   }
   memset(rec->data, 2, rec->data_len);
   if (!write_record_to_block(block, rec)) {
      Pmsg0(0, _("Error writing record to block.\n")); 
      goto bail_out;
   }
   if (!write_block_to_dev(jcr, dev, block)) {
      Pmsg0(0, _("Error writing block to device.\n")); 
      goto bail_out;
   } else {
      Pmsg1(0, _("Wrote second record of %d bytes.\n"), rec->data_len);
   }
   memset(rec->data, 3, rec->data_len);
   if (!write_record_to_block(block, rec)) {
      Pmsg0(0, _("Error writing record to block.\n")); 
      goto bail_out;
   }
   if (!write_block_to_dev(jcr, dev, block)) {
      Pmsg0(0, _("Error writing block to device.\n")); 
      goto bail_out;
   } else {
      Pmsg1(0, _("Wrote third record of %d bytes.\n"), rec->data_len);
   }
   weofcmd();
   if (dev_cap(dev, CAP_TWOEOF)) {
      weofcmd();
   }
   if (!bsf_dev(dev, 1)) {
      Pmsg1(0, _("Backspace file failed! ERR=%s\n"), strerror_dev(dev));
      goto bail_out;
   }
   if (dev_cap(dev, CAP_TWOEOF)) {
      if (!bsf_dev(dev, 1)) {
         Pmsg1(0, _("Backspace file failed! ERR=%s\n"), strerror_dev(dev));
	 goto bail_out;
      }
   }
   Pmsg0(0, "Backspaced over EOF OK.\n");
   if (!bsr_dev(dev, 1)) {
      Pmsg1(0, _("Backspace record failed! ERR=%s\n"), strerror_dev(dev));
      goto bail_out;
   }
   Pmsg0(0, "Backspace record OK.\n");
   if (!read_block_from_dev(jcr, dev, block, NO_BLOCK_NUMBER_CHECK)) {
      Pmsg1(0, _("Read block failed! ERR=%s\n"), strerror(dev->dev_errno));
      goto bail_out;
   }
   memset(rec->data, 0, rec->data_len);
   if (!read_record_from_block(block, rec)) {
      Pmsg1(0, _("Read block failed! ERR=%s\n"), strerror(dev->dev_errno));
      goto bail_out;
   }
   for (int i=0; i<len; i++) {
      if (rec->data[i] != 3) {
         Pmsg0(0, _("Bad data in record. Test failed!\n"));
	 goto bail_out;
      }
   }
   Pmsg0(0, _("\nBlock re-read correct. Test succeeded!\n"));
   Pmsg0(-1, _("=== End Write, backup, and re-read test ===\n\n"));

   stat = 1;

bail_out:
   free_block(block);
   free_record(rec);
   if (stat == 0) {
      Pmsg0(0, _("This is not terribly serious since Bacula only uses\n"
                 "this function to verify the last block written to the\n"
                 "tape. Bacula will skip the last block verification\n"
                 "if you add:\n\n"
                  "Backward Space Record = No\n\n"
                  "to your Storage daemon's Device resource definition.\n"));
   }   
   return stat;
}

/*
 * This test writes some records, then writes an end of file,
 *   rewinds the tape, moves to the end of the data and attepts
 *   to append to the tape.  This function is essential for
 *   Bacula to be able to write multiple jobs to the tape.
 */
static int append_test()
{
   Pmsg0(-1, _("\n\n=== Append files test ===\n\n"
               "This test is essential to Bacula.\n\n"
"I'm going to write one record  in file 0,\n"
"                   two records in file 1,\n"
"             and three records in file 2\n\n"));
   argc = 1;
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
   if (dev_cap(dev, CAP_TWOEOF)) {
      weofcmd();
   }
   rewindcmd();
   Pmsg0(0, _("Now moving to end of medium.\n"));
   eodcmd();
   Pmsg2(-1, _("We should be in file 3. I am at file %d. This is %s\n"), 
      dev->file, dev->file == 3 ? "correct!" : "NOT correct!!!!");

   if (dev->file != 3) {
      return -1;
   }

   Pmsg0(-1, _("\nNow the important part, I am going to attempt to append to the tape.\n\n"));
   wrcmd(); 
   weofcmd();
   if (dev_cap(dev, CAP_TWOEOF)) {
      weofcmd();
   }
   rewindcmd();
   Pmsg0(-1, _("Done appending, there should be no I/O errors\n\n"));
   Pmsg0(-1, "Doing Bacula scan of blocks:\n");
   scan_blocks();
   Pmsg0(-1, _("End scanning the tape.\n"));
   Pmsg2(-1, _("We should be in file 4. I am at file %d. This is %s\n"), 
      dev->file, dev->file == 4 ? "correct!" : "NOT correct!!!!");

   if (dev->file != 4) {
      return -2;
   }

   return 1;
}


/*
 * This test exercises the autochanger
 */
static int autochanger_test()
{
   POOLMEM *results, *changer;
   int slot, status, loaded;
   int timeout = 120;
   int sleep_time = 0;

   if (!dev_cap(dev, CAP_AUTOCHANGER)) {
      return 1;
   }
   if (!(jcr->device && jcr->device->changer_name && jcr->device->changer_command)) {
      Pmsg0(-1, "\nAutochanger enabled, but no name or no command device specified.\n");
      return 1;
   }

   Pmsg0(-1, "\nTo test the autochanger you must have a blank tape in Slot 1.\n"
             "I'm going to write on it.\n");
   if (!get_cmd("\nDo you wish to continue with the Autochanger test? (y/n): ")) {
      return 0;
   }
   if (cmd[0] != 'y' && cmd[0] != 'Y') {
      return 0;
   }

   Pmsg0(-1, _("\n\n=== Autochanger test ===\n\n"));

   results = get_pool_memory(PM_MESSAGE);
   changer = get_pool_memory(PM_FNAME);

try_again:
   slot = 1;
   jcr->VolCatInfo.Slot = slot;
   /* Find out what is loaded, zero means device is unloaded */
   Pmsg0(-1, _("3301 Issuing autochanger \"loaded\" command.\n"));
   changer = edit_device_codes(jcr, changer, jcr->device->changer_command, 
                "loaded");
   status = run_program(changer, timeout, results);
   Dmsg3(100, "run_prog: %s stat=%d result=%s\n", changer, status, results);
   if (status == 0) {
      loaded = atoi(results);
   } else {
      Pmsg1(-1, _("3991 Bad autochanger \"load slot\" status=%d.\n"), status);
      loaded = -1;		/* force unload */
      goto bail_out;
   }
   if (loaded) {
      Pmsg1(-1, "Slot %d loaded. I am going to unload it.\n", loaded);
   } else {
      Pmsg0(-1, "Nothing loaded into the drive. OK.\n");
   }
   Dmsg1(100, "Results from loaded query=%s\n", results);
   if (loaded) {
      offline_or_rewind_dev(dev);
      /* We are going to load a new tape, so close the device */
      force_close_dev(dev);
      Pmsg0(-1, _("3302 Issuing autochanger \"unload\" command.\n"));
      changer = edit_device_codes(jcr, changer, 
                     jcr->device->changer_command, "unload");
      status = run_program(changer, timeout, NULL);
      Pmsg2(-1, "unload status=%s %d\n", status==0?"OK":"Bad", status);
   }

   /*
    * Load the Slot 1
    */
   Pmsg1(-1, _("3303 Issuing autochanger \"load slot %d\" command.\n"), slot);
   changer = edit_device_codes(jcr, changer, jcr->device->changer_command, "load");
   Dmsg1(200, "Changer=%s\n", changer);
   status = run_program(changer, timeout, NULL);
   if (status == 0) {
      Pmsg1(-1,  _("3304 Autochanger \"load slot %d\" status is OK.\n"), slot);
   } else {
      Pmsg1(-1,  _("3992 Bad autochanger \"load slot\" status=%d.\n"), status);
      goto bail_out;
   }

   if (!open_the_device()) {
      goto bail_out;
   }
   bmicrosleep(sleep_time, 0);
   if (!rewind_dev(dev)) {
      Pmsg1(0, "Bad status from rewind. ERR=%s\n", strerror_dev(dev));
      clrerror_dev(dev, -1);
      Pmsg0(-1, "\nThe test failed, probably because you need to put\n"
                "a longer sleep time in the mtx-script in the load) case.\n" 
                "Adding a 30 second sleep and trying again ...\n");
      sleep_time += 30;
      goto try_again;
   } else {
      Pmsg1(0, "Rewound %s\n", dev_name(dev));
   }
      
   if ((status = weof_dev(dev, 1)) < 0) {
      Pmsg2(0, "Bad status from weof %d. ERR=%s\n", status, strerror_dev(dev));
      goto bail_out;
   } else {
      Pmsg1(0, "Wrote EOF to %s\n", dev_name(dev));
   }

   if (sleep_time) {
      Pmsg1(-1, "\nThe test worked this time. Please add:\n\n"
                "   sleep %d\n\n"
                "to your mtx-changer script in the load) case.\n\n",
		sleep_time);
   } else {
      Pmsg0(-1, "\nThe test autochanger worked!!\n\n");
   }

   free_pool_memory(changer);
   free_pool_memory(results);
   return 1;


bail_out:
   free_pool_memory(changer);
   free_pool_memory(results);
   Pmsg0(-1, "You must correct this error or the Autochanger will not work.\n");
   return -2;
}

static void autochangercmd()
{
   autochanger_test();
}


/*
 * This test assumes that the append test has been done,
 *   then it tests the fsf function.
 */
static int fsf_test()
{
   bool set_off = false;
   
   Pmsg0(-1, _("\n\n=== Forward space files test ===\n\n"
               "This test is essential to Bacula.\n\n"
               "I'm going to write five files then test forward spacing\n\n"));
   argc = 1;
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
   wrcmd();
   wrcmd();
   weofcmd();	  /* end file 3 */
   wrcmd();
   weofcmd();	  /* end file 4 */
   if (dev_cap(dev, CAP_TWOEOF)) {
      weofcmd();
   }

test_again:
   rewindcmd();
   Pmsg0(0, _("Now forward spacing 1 file.\n"));
   if (!fsf_dev(dev, 1)) {
      Pmsg1(0, "Bad status from fsr. ERR=%s\n", strerror_dev(dev));
      goto bail_out;
   }
   Pmsg2(-1, _("We should be in file 1. I am at file %d. This is %s\n"), 
      dev->file, dev->file == 1 ? "correct!" : "NOT correct!!!!");

   if (dev->file != 1) {
      goto bail_out;
   }

   Pmsg0(0, _("Now forward spacing 2 files.\n"));
   if (!fsf_dev(dev, 2)) {
      Pmsg1(0, "Bad status from fsr. ERR=%s\n", strerror_dev(dev));
      goto bail_out;
   }
   Pmsg2(-1, _("We should be in file 3. I am at file %d. This is %s\n"), 
      dev->file, dev->file == 3 ? "correct!" : "NOT correct!!!!");

   if (dev->file != 3) {
      goto bail_out;
   }

   rewindcmd();
   Pmsg0(0, _("Now forward spacing 4 files.\n"));
   if (!fsf_dev(dev, 4)) {
      Pmsg1(0, "Bad status from fsr. ERR=%s\n", strerror_dev(dev));
      goto bail_out;
   }
   Pmsg2(-1, _("We should be in file 4. I am at file %d. This is %s\n"), 
      dev->file, dev->file == 4 ? "correct!" : "NOT correct!!!!");

   if (dev->file != 4) {
      goto bail_out;
   }
   if (set_off) {
      Pmsg0(-1, "The test worked this time. Please add:\n\n"
                "   Fast Forward Space File = no\n\n"
                "to your Device resource for this drive.\n");
   }

   Pmsg0(-1, "\n");
   Pmsg0(0, _("Now forward spacing 4 more files.\n"));
   if (!fsf_dev(dev, 4)) {
      Pmsg1(0, "Bad status from fsr. ERR=%s\n", strerror_dev(dev));
      goto bail_out;
   }
   Pmsg2(-1, _("We should be in file 5. I am at file %d. This is %s\n"), 
      dev->file, dev->file == 5 ? "correct!" : "NOT correct!!!!");
   if (dev->file != 5) {
      goto bail_out;
   }

   return 1;

bail_out:
   Pmsg0(-1, _("\nThe forward space file test failed.\n"));
   if (dev_cap(dev, CAP_FASTFSF)) {
      Pmsg0(-1, "You have Fast Forward Space File enabled.\n"
              "I am turning it off then retrying the test.\n");
      dev->capabilities &= ~CAP_FASTFSF;
      set_off = true;
      goto test_again;
   }
   Pmsg0(-1, "You must correct this error or Bacula will not work.\n");
   return -2;
}





/* 
 * This is a general test of Bacula's functions
 *   needed to read and write the tape.
 */
static void testcmd()
{
   int stat;

   stat = append_test();
   if (stat == 1) {		      /* OK get out */
      goto all_done;
   }
   if (stat == -1) {		      /* first test failed */
      if (dev_cap(dev, CAP_EOM) || dev_cap(dev, CAP_FASTFSF)) {
         Pmsg0(-1, "\nAppend test failed. Attempting again.\n"
                   "Setting \"Hardware End of Medium = no\n"
                   "    and \"Fast Forward Space File = no\n"
                   "and retrying append test.\n\n");
	 dev->capabilities &= ~CAP_EOM; /* turn off eom */
	 dev->capabilities &= ~CAP_FASTFSF; /* turn off fast fsf */
	 stat = append_test();
	 if (stat == 1) {
            Pmsg0(-1, "\n\nIt looks like the test worked this time, please add:\n\n"
                     "    Hardware End of Medium = No\n\n"
                     "    Fast Forward Space File = No\n"
                     "to your Device resource in the Storage conf file.\n");
	    goto all_done;
	 }
	 if (stat == -1) {
            Pmsg0(-1, "\n\nThat appears not to have corrected the problem.\n");
	    goto all_done;
	 }
	 /* Wrong count after append */
	 if (stat == -2) {
            Pmsg0(-1, "\n\nIt looks like the append failed. Attempting again.\n"
                     "Setting \"BSF at EOM = yes\" and retrying append test.\n");
	    dev->capabilities |= CAP_BSFATEOM; /* backspace on eom */
	    stat = append_test();
	    if (stat == 1) {
               Pmsg0(-1, "\n\nIt looks like the test worked this time, please add:\n\n"
                     "    Hardware End of Medium = No\n"
                     "    Fast Forward Space File = No\n"
                     "    BSF at EOM = yes\n\n"
                     "to your Device resource in the Storage conf file.\n");
	       goto all_done;
	    }
	 }

      }
      Pmsg0(-1, "\nAppend test failed.\n\n");
      Pmsg0(-1, "\n!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n"
            "Unable to correct the problem. You MUST fix this\n"
             "problem before Bacula can use your tape drive correctly\n");
      Pmsg0(-1, "\nPerhaps running Bacula in fixed block mode will work.\n"
            "Do so by setting:\n\n"
            "Minimum Block Size = nnn\n"
            "Maximum Block Size = nnn\n\n"
            "in your Storage daemon's Device definition.\n"
            "nnn must match your tape driver's block size.\n"
            "This, however, is not really an ideal solution.\n");
   }

all_done:
   Pmsg0(-1, _("\nThe above Bacula scan should have output identical to what follows.\n"
        "Please double check it ...\n"
        "=== Sample correct output ===\n"
        "1 block of 64448 bytes in file 1\n"
        "End of File mark.\n"
        "2 blocks of 64448 bytes in file 2\n"
        "End of File mark.\n"
        "3 blocks of 64448 bytes in file 3\n"
        "End of File mark.\n"
        "1 block of 64448 bytes in file 4\n" 
        "End of File mark.\n"
        "Total files=4, blocks=7, bytes = 451,136\n"
        "=== End sample correct output ===\n\n"));

   Pmsg0(-1, _("If the above scan output is not identical to the\n"
               "sample output, you MUST correct the problem\n"
               "or Bacula will not be able to write multiple Jobs to \n"
               "the tape.\n\n"));

   if (stat == 1) {
      re_read_block_test();
   }

   fsf_test();			      /* do fast forward space file test */

   autochanger_test();		      /* do autochanger test */

   Pmsg0(-1, _("\n=== End Append files test ===\n"));
   
}

/* Forward space a file */
static void fsfcmd()
{
   int num = 1;
   if (argc > 1) {
      num = atoi(argk[1]);
   }
   if (num <= 0) {
      num = 1;
   }
   if (!fsf_dev(dev, num)) {
      Pmsg1(0, "Bad status from fsf. ERR=%s\n", strerror_dev(dev));
      return;
   }
   Pmsg2(0, "Forward spaced %d file%s.\n", num, num==1?"":"s");
}

/* Forward space a record */
static void fsrcmd()
{
   int num = 1;
   if (argc > 1) {
      num = atoi(argk[1]);
   }
   if (num <= 0) {
      num = 1;
   }
   if (!fsr_dev(dev, num)) {
      Pmsg1(0, "Bad status from fsr. ERR=%s\n", strerror_dev(dev));
      return;
   }
   Pmsg2(0, "Forward spaced %d record%s.\n", num, num==1?"":"s");
}


/*
 * Write a Bacula block to the tape
 */
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
      Pmsg0(0, _("Error writing record to block.\n")); 
      goto bail_out;
   }
   if (!write_block_to_dev(jcr, dev, block)) {
      Pmsg0(0, _("Error writing block to device.\n")); 
      goto bail_out;
   } else {
      Pmsg1(0, _("Wrote one record of %d bytes.\n"), i);
   }
   Pmsg0(0, _("Wrote block to device.\n"));

bail_out:
   sm_check(__FILE__, __LINE__, False);
   free_record(rec);
   free_block(block);
   sm_check(__FILE__, __LINE__, False);
}

/* 
 * Read a record from the tape
 */
static void rrcmd()
{
   char *buf;
   int stat, len;

   if (!get_cmd("Enter length to read: ")) {
      return;
   }
   len = atoi(cmd);
   if (len < 0 || len > 1000000) {
      Pmsg0(0, _("Bad length entered, using default of 1024 bytes.\n"));
      len = 1024;
   }
   buf = (char *)malloc(len);
   stat = read(dev->fd, buf, len);
   if (stat > 0 && stat <= len) {
      errno = 0;
   }
   Pmsg3(0, _("Read of %d bytes gives stat=%d. ERR=%s\n"),
      len, stat, strerror(errno));
   free(buf);
}


/*
 * Scan tape by reading block by block. Report what is
 * on the tape.  Note, this command does raw reads, and as such
 * will not work with fixed block size devices.
 */
static void scancmd()
{
   int stat;
   int blocks, tot_blocks, tot_files;
   int block_size;
   uint64_t bytes;
   char ec1[50];


   blocks = block_size = tot_blocks = 0;
   bytes = 0;
   if (dev->state & ST_EOT) {
      Pmsg0(0, "End of tape\n");
      return; 
   }
   update_pos_dev(dev);
   tot_files = dev->file;
   Pmsg1(0, _("Starting scan at file %u\n"), dev->file);
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
   printf("Total files=%d, blocks=%d, bytes = %s\n", tot_files, tot_blocks, 
      edit_uint64_with_commas(bytes, ec1));
}


/*
 * Scan tape by reading Bacula block by block. Report what is
 * on the tape.  This function reads Bacula blocks, so if your
 * Device resource is correctly defined, it should work with
 * either variable or fixed block sizes.
 */
static void scan_blocks()
{
   int blocks, tot_blocks, tot_files;
   uint32_t block_size;
   uint64_t bytes;
   DEV_BLOCK *block;
   char ec1[50];

   block = new_block(dev);
   blocks = block_size = tot_blocks = 0;
   bytes = 0;

   update_pos_dev(dev);
   tot_files = dev->file;
   for (;;) {
      if (!read_block_from_device(jcr, dev, block, NO_BLOCK_NUMBER_CHECK)) {
         Dmsg1(100, "!read_block(): ERR=%s\n", strerror_dev(dev));
	 if (dev->state & ST_EOT) {
	    if (blocks > 0) {
               printf("%d block%s of %d bytes in file %d\n", 
                    blocks, blocks>1?"s":"", block_size, dev->file);
	       blocks = 0;
	    }
	    goto bail_out;
	 }
	 if (dev->state & ST_EOF) {
	    if (blocks > 0) {
               printf("%d block%s of %d bytes in file %d\n",        
                       blocks, blocks>1?"s":"", block_size, dev->file);
	       blocks = 0;
	    }
            printf(_("End of File mark.\n"));
	    continue;
	 }
	 if (dev->state & ST_SHORT) {
	    if (blocks > 0) {
               printf("%d block%s of %d bytes in file %d\n",        
                       blocks, blocks>1?"s":"", block_size, dev->file);
	       blocks = 0;
	    }
            printf(_("Short block read.\n"));
	    continue;
	 }
         printf(_("Error reading block. ERR=%s\n"), strerror_dev(dev));
	 goto bail_out;
      }
      if (block->block_len != block_size) {
	 if (blocks > 0) {
            printf("%d block%s of %d bytes in file %d\n",        
                    blocks, blocks>1?"s":"", block_size, dev->file);
	    blocks = 0;
	 }
	 block_size = block->block_len;
      }
      blocks++;
      tot_blocks++;
      bytes += block->block_len;
      Dmsg6(100, "Blk_blk=%u dev_blk=%u blen=%u bVer=%d SessId=%u SessTim=%u\n",
	 block->BlockNumber, dev->block_num, block->block_len, block->BlockVer,
	 block->VolSessionId, block->VolSessionTime);
      if (verbose == 1) {
	 DEV_RECORD *rec = new_record();
	 read_record_from_block(block, rec);
         Pmsg8(-1, "Blk_block: %u dev_blk=%u blen=%u First rec FI=%s SessId=%u SessTim=%u Strm=%s rlen=%d\n",
	      block->BlockNumber, dev->block_num, block->block_len,
	      FI_to_ascii(rec->FileIndex), rec->VolSessionId, rec->VolSessionTime,
	      stream_to_ascii(rec->Stream, rec->FileIndex), rec->data_len);
	 rec->remainder = 0;
	 free_record(rec);
      } else if (verbose > 1) {
         dump_block(block, "");
      }

   }
bail_out:
   free_block(block);
   tot_files = dev->file - tot_files;
   printf("Total files=%d, blocks=%d, bytes = %s\n", tot_files, tot_blocks, 
      edit_uint64_with_commas(bytes, ec1));
}


static void statcmd()
{
   int debug = debug_level;
   debug_level = 30;
   Pmsg2(0, "Device status: %u. ERR=%s\n", status_dev(dev), strerror_dev(dev));
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
   int fd;
   uint32_t i;
   uint32_t min_block_size;

   ok = TRUE;
   stop = 0;
   vol_num = 0;
   last_file = 0;
   last_block_num = 0;
   BlockNumber = 0;

   Pmsg0(-1, "\n\
This command simulates Bacula writing to a tape.\n\
It requires either one or two blank tapes, which it\n\
will label and write. It will print a status approximately\n\
every 322 MB, and write an EOF every 3.2 GB.  If you have\n\
selected the simple test option, after writing the first tape\n\
it will rewind it and re-read the last block written.\n\
If you have selected the multiple tape test, when the first tape\n\
fills, it will ask for a second, and after writing a few more \n\
blocks, it will stop.  Then it will begin re-reading the\n\
two tapes.\n\n\
This may take a long time -- hours! ...\n\n");

   get_cmd("Insert a blank tape then indicate if you want\n"
           "to run the simplified test (s) with one tape or\n"
           "the complete multiple tape (m) test: (s/m) ");
   if (cmd[0] == 's') {
      Pmsg0(-1, "Simple test (single tape) selected.\n");
      simple = TRUE;
   } else if (cmd[0] == 'm') {
      Pmsg0(-1, "Complete multiple tape test selected.\n"); 
      simple = FALSE;
   } else {
      Pmsg0(000, "Command aborted.\n");
      return;
   }

   set_volume_name("TestVolume1", 1);
   labelcmd();
   VolumeName = NULL;

   
   Dmsg1(20, "Begin append device=%s\n", dev_name(dev));


   /* Use fixed block size to simplify read back */
   min_block_size = dev->min_block_size;
   dev->min_block_size = dev->max_block_size;
   block = new_block(dev);

   /* 
    * Acquire output device for writing.  Note, after acquiring a
    *	device, we MUST release it, which is done at the end of this
    *	subroutine.
    */
   Dmsg0(100, "just before acquire_device\n");
   if (!(dev=acquire_device_for_append(jcr, dev, block))) {
      set_jcr_job_status(jcr, JS_ErrorTerminated);
      free_block(block);
      return;
   }

   Dmsg0(100, "Just after acquire_device_for_append\n");
   /*
    * Write Begin Session Record
    */
   if (!write_session_label(jcr, block, SOS_LABEL)) {
      set_jcr_job_status(jcr, JS_ErrorTerminated);
      Jmsg1(jcr, M_FATAL, 0, _("Write session label failed. ERR=%s\n"),
	 strerror_dev(dev));
      ok = FALSE;
   }
   Pmsg0(-1, "Wrote Start Of Session label.\n");

   memset(&rec, 0, sizeof(rec));
   rec.data = get_memory(100000);     /* max record size */

#define REC_SIZE 32768
   rec.data_len = REC_SIZE;

   /* 
    * Put some random data in the record
    */
   fd = open("/dev/urandom", O_RDONLY);
   if (fd) {
      read(fd, rec.data, rec.data_len);
      close(fd);
   } else {
      uint32_t *p = (uint32_t *)rec.data;
      srandom(time(NULL));
      for (i=0; i<rec.data_len/sizeof(uint32_t); i++) {
	 p[i] = random();
      }
   }

   /* 
    * Generate data as if from File daemon, write to device   
    */
   jcr->VolFirstIndex = 0;
   time(&jcr->run_time);	      /* start counting time for rates */
   if (simple) {
      Pmsg0(-1, "Begin writing Bacula records to tape ...\n");
   } else {
      Pmsg0(-1, "Begin writing Bacula records to first tape ...\n");
   }
   for (file_index = 0; ok && !job_canceled(jcr); ) {
      rec.VolSessionId = jcr->VolSessionId;
      rec.VolSessionTime = jcr->VolSessionTime;
      rec.FileIndex = ++file_index;
      rec.Stream = STREAM_FILE_DATA;

      /* Mix up the data just a bit */
      uint32_t *lp = (uint32_t *)rec.data;
      lp[0] += lp[13];
      for (i=1; i < (rec.data_len-sizeof(uint32_t))/sizeof(uint32_t)-1; i++) {
	 lp[i] += lp[i-1];
      }

      Dmsg4(250, "before write_rec FI=%d SessId=%d Strm=%s len=%d\n",
	 rec.FileIndex, rec.VolSessionId, stream_to_ascii(rec.Stream, rec.FileIndex), 
	 rec.data_len);
       
      while (!write_record_to_block(block, &rec)) {
	 /*
	  * When we get here we have just filled a block
	  */
         Dmsg2(150, "!write_record_to_block data_len=%d rem=%d\n", rec.data_len,
		    rec.remainder);

	 /* Write block to tape */
	 if (!flush_block(block, 1)) {
	    break;
	 }

	 /* Every 5000 blocks (approx 322MB) report where we are.
	  */
	 if ((block->BlockNumber % 5000) == 0) {
	    now = time(NULL);
	    now -= jcr->run_time;
	    if (now <= 0) {
	       now = 1; 	 /* prevent divide error */
	    }
	    kbs = (double)dev->VolCatInfo.VolCatBytes / (1000.0 * (double)now);
            Pmsg4(-1, "Wrote blk_block=%u, dev_blk_num=%u VolBytes=%s rate=%.1f KB/s\n", 
	       block->BlockNumber, dev->block_num,
	       edit_uint64_with_commas(dev->VolCatInfo.VolCatBytes, ec1), (float)kbs);
	 }
	 /* Every 15000 blocks (approx 1GB) write an EOF.
	  */
	 if ((block->BlockNumber % 15000) == 0) {
            Pmsg0(-1, "Flush block, write EOF\n");
	    flush_block(block, 0);
	    weof_dev(dev, 1);
	 }

	 /* Get out after writing 10 blocks to the second tape */
	 if (++BlockNumber > 10 && stop != 0) {      /* get out */
	    break;    
	 }
      }
      if (!ok) {
         Pmsg0(000, _("Not OK\n"));
	 break;
      }
      jcr->JobBytes += rec.data_len;   /* increment bytes this job */
      Dmsg4(190, "write_record FI=%s SessId=%d Strm=%s len=%d\n",
	 FI_to_ascii(rec.FileIndex), rec.VolSessionId, 
	 stream_to_ascii(rec.Stream, rec.FileIndex), rec.data_len);

      /* Get out after writing 10 blocks to the second tape */
      if (BlockNumber > 10 && stop != 0) {	/* get out */
         Pmsg0(-1, "Done writing ...\n");
	 break;    
      }
   }
   if (stop > 0) {
      Dmsg0(100, "Write_end_session_label()\n");
      /* Create Job status for end of session label */
      if (!job_canceled(jcr) && ok) {
	 set_jcr_job_status(jcr, JS_Terminated);
      } else if (!ok) {
	 set_jcr_job_status(jcr, JS_ErrorTerminated);
      }
      if (!write_session_label(jcr, block, EOS_LABEL)) {
         Pmsg1(000, _("Error writting end session label. ERR=%s\n"), strerror_dev(dev));
	 ok = FALSE;
      }
      /* Write out final block of this session */
      if (!write_block_to_device(jcr, dev, block)) {
         Pmsg0(-1, _("Set ok=FALSE after write_block_to_device.\n"));
	 ok = FALSE;
      }
      Pmsg0(-1, _("Wrote End Of Session label.\n"));
   }

   sprintf(buf, "%s/btape.state", working_directory);
   fd = open(buf, O_CREAT|O_TRUNC|O_WRONLY, 0640);
   if (fd >= 0) {
      write(fd, &btape_state_level, sizeof(btape_state_level));
      write(fd, &last_block_num, sizeof(last_block_num));
      write(fd, &last_file, sizeof(last_file));
      write(fd, last_block->buf, last_block->buf_len);
      close(fd);
      Pmsg0(-1, "Wrote state file.\n");
   } else {
      Pmsg2(-1, _("Could not create state file: %s ERR=%s\n"), buf,
		 strerror(errno));
   }

   /* Release the device */
   if (!release_device(jcr, dev)) {
      Pmsg0(-1, _("Error in release_device\n"));
      ok = FALSE;
   }

   if (verbose) {
      Pmsg0(-1, "\n");
      dump_block(last_block, _("Last block written to tape.\n"));
   }

   Pmsg0(-1, _("\n\nDone filling tape. Now beginning re-read of tape ...\n"));

   if (simple) {
      do_unfill();
    } else {
       /* Multiple Volume tape */
      dumped = 0;
      VolBytes = 0;
      LastBlock = 0;
      block = new_block(dev);

      dev->capabilities |= CAP_ANONVOLS; /* allow reading any volume */
      dev->capabilities &= ~CAP_LABEL;   /* don't label anything here */

      end_of_tape = 0;


      time(&jcr->run_time);		 /* start counting time for rates */
      stop = 0;
      file_index = 0;
      /* Close device so user can use autochanger if desired */
      if (dev_cap(dev, CAP_OFFLINEUNMOUNT)) {
	 offline_dev(dev);
      }
      force_close_dev(dev);
      get_cmd(_("Mount first tape. Press enter when ready: ")); 
   
      free_vol_list(jcr);
      set_volume_name("TestVolume1", 1);
      jcr->bsr = NULL;
      create_vol_list(jcr);
      close_dev(dev);
      dev->state &= ~ST_READ;
      if (!acquire_device_for_read(jcr, dev, block)) {
         Pmsg1(-1, "%s", dev->errmsg);
	 goto bail_out;
      }
      /* Read all records and then second tape */
      read_records(jcr, dev, record_cb, my_mount_next_read_volume);
   }
bail_out:
   dev->min_block_size = min_block_size;
   free_block(block);
   free_memory(rec.data);
}

/*
 * Read two tapes written by the "fill" command and ensure
 *  that the data is valid.  If stop==1 we simulate full read back
 *  of two tapes.  If stop==-1 we simply read the last block and
 *  verify that it is correct.
 */
static void unfillcmd()
{
   int fd;

   if (!last_block) {
      last_block = new_block(dev);
   }
   sprintf(buf, "%s/btape.state", working_directory);
   fd = open(buf, O_RDONLY);
   if (fd >= 0) {
      uint32_t state_level;		 
      read(fd, &state_level, sizeof(btape_state_level));
      read(fd, &last_block_num, sizeof(last_block_num));
      read(fd, &last_file, sizeof(last_file));
      read(fd, last_block->buf, last_block->buf_len);
      close(fd);
      if (state_level != btape_state_level) {
          Pmsg0(-1, "\nThe state file level has changed. You must redo\n"
                  "the fill command.\n");
	  return;
       }
   } else {
      Pmsg2(-1, "\nCould not find the state file: %s ERR=%s\n"
             "You must redo the fill command.\n", buf, strerror(errno));
      return;
   }
   
   do_unfill();
}

static void do_unfill()
{
   DEV_BLOCK *block;

   dumped = 0;
   VolBytes = 0;
   LastBlock = 0;
   block = new_block(dev);

   dev->capabilities |= CAP_ANONVOLS; /* allow reading any volume */
   dev->capabilities &= ~CAP_LABEL;   /* don't label anything here */

   end_of_tape = 0;


   time(&jcr->run_time);	      /* start counting time for rates */
   stop = 0;
   file_index = 0;

   /*
    * Note, re-reading last block may have caused us to 
    *	lose track of where we are (block number unknown).
    */
   rewind_dev(dev);		      /* get to a known place on tape */
   Pmsg4(-1, _("Reposition from %u:%u to %u:%u\n"), dev->file, dev->block_num,
	 last_file, last_block_num);
   if (!reposition_dev(dev, last_file, last_block_num)) {
      Pmsg1(-1, "Reposition error. ERR=%s\n", strerror_dev(dev));
   }
   Pmsg1(-1, _("Reading block %u.\n"), last_block_num);
   if (!read_block_from_device(jcr, dev, block, NO_BLOCK_NUMBER_CHECK)) {
      Pmsg1(-1, _("Error reading block: ERR=%s\n"), strerror_dev(dev));
      goto bail_out;
   }
   if (last_block) {
      char *p, *q;
      uint32_t CheckSum, block_len;
      ser_declare;
      p = last_block->buf;	
      q = block->buf;
      unser_begin(q, BLKHDR2_LENGTH);
      unser_uint32(CheckSum);
      unser_uint32(block_len);
      while (q < (block->buf+block_len)) {
	 if (*p == *q) {
	    p++;
	    q++;
	    continue;
	 }
         Pmsg0(-1, "\n");
         dump_block(last_block, _("Last block written"));
         Pmsg0(-1, "\n");
         dump_block(block, _("Block read back"));
         Pmsg1(-1, "\n\nThe blocks differ at byte %u\n", p - last_block->buf);
         Pmsg0(-1, "\n\n!!!! The last block written and the block\n"
                   "that was read back differ. The test FAILED !!!!\n"
                   "This must be corrected before you use Bacula\n"
                   "to write multi-tape Volumes.!!!!\n");
	 goto bail_out;
      }
      Pmsg0(-1, _("\nThe blocks are identical. Test succeeded.\n\n"));
      if (verbose) {
         dump_block(last_block, _("Last block written"));
         dump_block(block, _("Block read back"));
      }
   }

bail_out:
   free_block(block);
}

/* 
 * We are called here from "unfill" for each record on the tape.
 */
static int record_cb(JCR *jcr, DEVICE *dev, DEV_BLOCK *block, DEV_RECORD *rec)
{
   SESSION_LABEL label;

   if (stop > 1 && !dumped) {	      /* on second tape */
      dumped = 1;
      if (verbose) {
         dump_block(block, "First block on second tape");
      }
      Pmsg4(-1, "Blk: FileIndex=%d: block=%u size=%d vol=%s\n", 
	   rec->FileIndex, block->BlockNumber, block->block_len, dev->VolHdr.VolName);
      Pmsg6(-1, "   Rec: VId=%d VT=%d FI=%s Strm=%s len=%d state=%x\n",
	   rec->VolSessionId, rec->VolSessionTime, 
	   FI_to_ascii(rec->FileIndex), stream_to_ascii(rec->Stream, rec->FileIndex),
	   rec->data_len, rec->state);
   }
   if (rec->FileIndex < 0) {
      if (verbose > 1) {
	 dump_label_record(dev, rec, 1);
      }
      switch (rec->FileIndex) {
      case PRE_LABEL:
         Pmsg0(-1, "Volume is prelabeled. This tape cannot be scanned.\n");
	 return 1;;
      case VOL_LABEL:
	 unser_volume_label(dev, rec);
         Pmsg3(-1, "VOL_LABEL: block=%u size=%d vol=%s\n", block->BlockNumber, 
	    block->block_len, dev->VolHdr.VolName);
	 stop++;
	 break;
      case SOS_LABEL:
	 unser_session_label(&label, rec);
         Pmsg1(-1, "SOS_LABEL: JobId=%u\n", label.JobId);
	 break;
      case EOS_LABEL:
	 unser_session_label(&label, rec);
         Pmsg2(-1, "EOS_LABEL: block=%u JobId=%u\n", block->BlockNumber, 
	    label.JobId);
	 break;
      case EOM_LABEL:
         Pmsg0(-1, "EOM_LABEL:\n");
	 break;
      case EOT_LABEL:		   /* end of all tapes */
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
      return 1;
   }
   if (++file_index != rec->FileIndex) {
      Pmsg3(000, "Incorrect FileIndex in Block %u. Got %d, expected %d.\n", 
	 block->BlockNumber, rec->FileIndex, file_index);
   }
   /*
    * Now check that the right data is in the record.
    */
   uint64_t *lp = (uint64_t *)rec->data;
   uint64_t val = ~file_index;
   for (uint32_t i=0; i < (REC_SIZE-sizeof(uint64_t))/sizeof(uint64_t); i++) {
      if (*lp++ != val) {
         Pmsg2(000, "Record %d contains bad data in Block %u.\n",
	    file_index, block->BlockNumber);
	 break;
      }
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
   return 1;
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
   DEV_BLOCK *tblock;
   uint32_t this_file, this_block_num;

   if (!this_block) {
      this_block = new_block(dev);
   }
   /* Copy block */
   free_memory(this_block->buf);    
   memcpy(this_block, block, sizeof(DEV_BLOCK));
   this_block->buf = get_memory(block->buf_len);
   this_file = dev->file;
   this_block_num = dev->block_num;
   if (!write_block_to_dev(jcr, dev, block)) {
      Pmsg3(000, "Last block at: %u:%u this_dev_block_num=%d\n", 
		  last_file, last_block_num, this_block_num);
      if (verbose) {
         Pmsg3(000, "Block not written: FileIndex=%u blk_block=%u Size=%u\n", 
	    (unsigned)file_index, block->BlockNumber, block->block_len);
	 if (dump) {
            dump_block(block, "Block not written");
	 }
      }
      if (stop == 0) {
	 eot_block = block->BlockNumber;
	 eot_block_len = block->block_len;
	 eot_FileIndex = file_index;
      }
      now = time(NULL);
      now -= jcr->run_time;
      if (now <= 0) {
         now = 1;                     /* don't divide by zero */
      }
      kbs = (double)dev->VolCatInfo.VolCatBytes / (1000 * now);
      vol_size = dev->VolCatInfo.VolCatBytes;
      Pmsg2(000, "End of tape. VolumeCapacity=%s. Write rate = %.1f KB/s\n", 
	 edit_uint64_with_commas(dev->VolCatInfo.VolCatBytes, ec1), kbs);

      if (simple) {
	 stop = -1;		      /* stop, but do simplified test */
      } else {
	 /* Full test in progress */
	 if (!fixup_device_block_write_error(jcr, dev, block)) {
            Pmsg1(000, _("Cannot fixup device error. %s\n"), strerror_dev(dev));
	    ok = FALSE;
	    unlock_device(dev);
	    return 0;
	 }
	 stop = 1;						       
	 BlockNumber = 0;	      /* start counting for second tape */
      }
      unlock_device(dev);
      return 1; 		      /* end of tape reached */
   }
   /* Save contents after write so that the header is serialized */
   memcpy(this_block->buf, block->buf, this_block->buf_len);

   /*
    * Toggle between two allocated blocks for efficiency.
    * Switch blocks so that the block just successfully written is
    *  always in last_block. 
    */
   tblock = last_block;
   last_block = this_block; 
   this_block = tblock;
   last_file = this_file;
   last_block_num = this_block_num;

   unlock_device(dev);
   return 1;
}


/* 
 * First we label the tape, then we fill
 *  it with data get a new tape and write a few blocks.
 */			       
static void qfillcmd()
{
   DEV_BLOCK *block;
   DEV_RECORD *rec;
   int i, count;

   Pmsg0(0, "Test writing blocks of 64512 bytes to tape.\n");

   get_cmd("How many blocks do you want to write? (1000): ");

   count = atoi(cmd);
   if (count <= 0) {
      count = 1000;
   }

   sm_check(__FILE__, __LINE__, False);
   block = new_block(dev);
   rec = new_record();

   i = block->buf_len - 100;
   ASSERT (i > 0);
   rec->data = check_pool_memory_size(rec->data, i);
   memset(rec->data, i & 0xFF, i);
   rec->data_len = i;
   rewindcmd();
   Pmsg1(0, "Begin writing %d Bacula blocks to tape ...\n", count);
   for (i=0; i < count; i++) {
      if (i % 100 == 0) {
         printf("+");
	 fflush(stdout);
      }
      if (!write_record_to_block(block, rec)) {
         Pmsg0(0, _("Error writing record to block.\n")); 
	 goto bail_out;
      }
      if (!write_block_to_dev(jcr, dev, block)) {
         Pmsg0(0, _("Error writing block to device.\n")); 
	 goto bail_out;
      }
   }
   printf("\n");
   weofcmd();
   if (dev_cap(dev, CAP_TWOEOF)) {
      weofcmd();
   }
   rewindcmd();
   scan_blocks();

bail_out:
   sm_check(__FILE__, __LINE__, False);
   free_record(rec);
   free_block(block);
   sm_check(__FILE__, __LINE__, False);

}

/*
 * Fill a tape using raw write() command
 */
static void rawfill_cmd()
{
   DEV_BLOCK *block;
   int stat;
   int fd;
   uint32_t block_num = 0;
   uint32_t *p;
   int my_errno;
   uint32_t i;

   block = new_block(dev);
   fd = open("/dev/urandom", O_RDONLY);
   if (fd) {
      read(fd, block->buf, block->buf_len);
      close(fd);
   } else {
      uint32_t *p = (uint32_t *)block->buf;
      srandom(time(NULL));
      for (i=0; i<block->buf_len/sizeof(uint32_t); i++) {
	 p[i] = random();
      }
   }
   p = (uint32_t *)block->buf;
   Pmsg1(0, "Begin writing raw blocks of %u bytes.\n", block->buf_len);
   for ( ;; ) {
      *p = block_num;
      stat = write(dev->fd, block->buf, block->buf_len);
      if (stat == (int)block->buf_len) {
	 if ((block_num++ % 100) == 0) {
            printf("+");
	    fflush(stdout);
	 }
	 p[0] += p[13];
	 for (i=1; i<(block->buf_len-sizeof(uint32_t))/sizeof(uint32_t)-1; i++) {
	    p[i] += p[i-1];
	 }
	 continue;
      }
      break;
   }
   my_errno = errno;
   printf("\n");
   printf("Write failed at block %u. stat=%d ERR=%s\n", block_num, stat,
      strerror(my_errno));
   weofcmd();
   free_block(block);
}


/*
 * Fill a tape using raw write() command
 */
static void bfill_cmd()
{
   DEV_BLOCK *block;
   uint32_t block_num = 0;
   uint32_t *p;
   int my_errno;
   int fd;   
   uint32_t i;

   block = new_block(dev);
   fd = open("/dev/urandom", O_RDONLY);
   if (fd) {
      read(fd, block->buf, block->buf_len);
      close(fd);
   } else {
      uint32_t *p = (uint32_t *)block->buf;
      srandom(time(NULL));
      for (i=0; i<block->buf_len/sizeof(uint32_t); i++) {
	 p[i] = random();
      }
   }
   p = (uint32_t *)block->buf;
   Pmsg1(0, "Begin writing Bacula blocks of %u bytes.\n", block->buf_len);
   for ( ;; ) {
      *p = block_num;
      block->binbuf = block->buf_len;
      block->bufp = block->buf + block->binbuf;
      if (!write_block_to_dev(jcr, dev, block)) {
	 break;
      }
      if ((block_num++ % 100) == 0) {
         printf("+");
	 fflush(stdout);
      }
      p[0] += p[13];
      for (i=1; i<(block->buf_len/sizeof(uint32_t)-1); i++) {
	 p[i] += p[i-1];
      }
   }
   my_errno = errno;
   printf("\n");
   printf("Write failed at block %u.\n", block_num);     
   weofcmd();
   free_block(block);
}


struct cmdstruct { char *key; void (*func)(); char *help; }; 
static struct cmdstruct commands[] = {
 {"autochanger", autochangercmd, "test autochanger"},
 {"bsf",        bsfcmd,       "backspace file"},
 {"bsr",        bsrcmd,       "backspace record"},
 {"bfill",      bfill_cmd,    "fill tape using Bacula writes"},
 {"cap",        capcmd,       "list device capabilities"},
 {"clear",      clearcmd,     "clear tape errors"},
 {"eod",        eodcmd,       "go to end of Bacula data for append"},
 {"eom",        eomcmd,       "go to the physical end of medium"},
 {"fill",       fillcmd,      "fill tape, write onto second volume"},
 {"unfill",     unfillcmd,    "read filled tape"},
 {"fsf",        fsfcmd,       "forward space a file"},
 {"fsr",        fsrcmd,       "forward space a record"},
 {"help",       helpcmd,      "print this command"},
 {"label",      labelcmd,     "write a Bacula label to the tape"},
 {"load",       loadcmd,      "load a tape"},
 {"quit",       quitcmd,      "quit btape"},   
 {"rawfill",    rawfill_cmd,  "use write() to fill tape"},
 {"readlabel",  readlabelcmd, "read and print the Bacula tape label"},
 {"rectest",    rectestcmd,   "test record handling functions"},
 {"rewind",     rewindcmd,    "rewind the tape"},
 {"scan",       scancmd,      "read() tape block by block to EOT and report"}, 
 {"scanblocks", scan_blocks,  "Bacula read block by block to EOT and report"},
 {"status",     statcmd,      "print tape status"},
 {"test",       testcmd,      "General test Bacula tape functions"},
 {"weof",       weofcmd,      "write an EOF on the tape"},
 {"wr",         wrcmd,        "write a single Bacula block"}, 
 {"rr",         rrcmd,        "read a single record"},
 {"qfill",      qfillcmd,     "quick fill command"},
	     };
#define comsize (sizeof(commands)/sizeof(struct cmdstruct))

static void
do_tape_cmds()
{
   unsigned int i;
   bool found;

   while (get_cmd("*")) {
      sm_check(__FILE__, __LINE__, False);
      found = false;
      parse_args(cmd, &args, &argc, argk, argv, MAX_CMD_ARGS);
      for (i=0; i<comsize; i++)       /* search for command */
	 if (argc > 0 && fstrsch(argk[0],  commands[i].key)) {
	    (*commands[i].func)();    /* go execute command */
	    found = true;
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
   printf(_("Interactive commands:\n"));
   printf(_("  Command    Description\n  =======    ===========\n"));
   for (i=0; i<comsize; i++)
      printf("  %-10s %s\n", commands[i].key, commands[i].help);
   printf("\n");
}

static void usage()
{
   fprintf(stderr, _(
"\nVersion: " VERSION " (" BDATE ")\n\n"
"Usage: btape <options> <device_name>\n"
"       -c <file>   set configuration file to file\n"
"       -d <nn>     set debug level to nn\n"
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
int	dir_get_volume_info(JCR *jcr, enum get_vol_info_rw  writing) { return 1;}
int	dir_update_volume_info(JCR *jcr, DEVICE *dev, int relabel) { return 1; }
int	dir_create_jobmedia_record(JCR *jcr) { return 1; }
int	dir_update_file_attributes(JCR *jcr, DEV_RECORD *rec) { return 1;}
int	dir_send_job_status(JCR *jcr) {return 1;}



int	dir_find_next_appendable_volume(JCR *jcr) 
{ 
   return 1; 
}

int dir_ask_sysop_to_mount_volume(JCR *jcr, DEVICE *dev)
{
   /* Close device so user can use autochanger if desired */
   if (dev_cap(dev, CAP_OFFLINEUNMOUNT)) {
      offline_dev(dev);
   }
   force_close_dev(dev);
   Pmsg1(-1, "%s", dev->errmsg);           /* print reason */
   fprintf(stderr, "Mount Volume \"%s\" on device %s and press return when ready: ",
      jcr->VolumeName, dev_name(dev));
   getchar();	
   return 1;
}

int dir_ask_sysop_to_create_appendable_volume(JCR *jcr, DEVICE *dev)
{
   /* Close device so user can use autochanger if desired */
   if (dev_cap(dev, CAP_OFFLINEUNMOUNT)) {
      offline_dev(dev);
   }
   force_close_dev(dev);
   fprintf(stderr, "Mount next Volume on device %s and press return when ready: ",
      dev_name(dev));
   getchar();	
   set_volume_name("TestVolume2", 2);
   labelcmd();
   VolumeName = NULL;
   BlockNumber = 0;
   stop = 1;
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
   kbs = (double)VolBytes / (1000.0 * (double)now);
   Pmsg3(-1, "Read block=%u, VolBytes=%s rate=%.1f KB/s\n", block->BlockNumber,
	    edit_uint64_with_commas(VolBytes, ec1), (float)kbs);

   if (strcmp(jcr->VolumeName, "TestVolume2") == 0) {
      end_of_tape = 1;
      return 0;
   }

   free_vol_list(jcr);
   set_volume_name("TestVolume2", 2);
   jcr->bsr = NULL;
   create_vol_list(jcr);
   close_dev(dev);
   dev->state &= ~ST_READ; 
   if (!acquire_device_for_read(jcr, dev, block)) {
      Pmsg2(0, "Cannot open Dev=%s, Vol=%s\n", dev_name(dev), jcr->VolumeName);
      return 0;
   }
   return 1;			   /* next volume mounted */
}

static void set_volume_name(char *VolName, int volnum) 
{
   VolumeName = VolName;
   vol_num = volnum;
   pm_strcpy(&jcr->VolumeName, VolName);
   bstrncpy(dev->VolCatInfo.VolCatName, VolName, sizeof(dev->VolCatInfo.VolCatName));
}

/*
 * Edit codes into ChangerCommand
 *  %% = %
 *  %a = archive device name
 *  %c = changer device name
 *  %f = Client's name
 *  %j = Job name
 *  %o = command
 *  %s = Slot base 0
 *  %S = Slot base 1
 *  %v = Volume name
 *
 *
 *  omsg = edited output message
 *  imsg = input string containing edit codes (%x)
 *  cmd = command string (load, unload, ...) 
 *
 */
static char *edit_device_codes(JCR *jcr, char *omsg, char *imsg, char *cmd) 
{
   char *p;
   const char *str;
   char add[20];

   *omsg = 0;
   Dmsg1(400, "edit_device_codes: %s\n", imsg);
   for (p=imsg; *p; p++) {
      if (*p == '%') {
	 switch (*++p) {
         case '%':
            str = "%";
	    break;
         case 'a':
	    str = dev_name(jcr->device->dev);
	    break;
         case 'c':
	    str = NPRT(jcr->device->changer_name);
	    break;
         case 'o':
	    str = NPRT(cmd);
	    break;
         case 's':
            sprintf(add, "%d", jcr->VolCatInfo.Slot - 1);
	    str = add;
	    break;
         case 'S':
            sprintf(add, "%d", jcr->VolCatInfo.Slot);
	    str = add;
	    break;
         case 'j':                    /* Job name */
	    str = jcr->Job;
	    break;
         case 'v':
	    str = NPRT(jcr->VolumeName);
	    break;
         case 'f':
	    str = NPRT(jcr->client_name);
	    break;

	 default:
            add[0] = '%';
	    add[1] = *p;
	    add[2] = 0;
	    str = add;
	    break;
	 }
      } else {
	 add[0] = *p;
	 add[1] = 0;
	 str = add;
      }
      Dmsg1(400, "add_str %s\n", str);
      pm_strcat(&omsg, (char *)str);
      Dmsg1(400, "omsg=%s\n", omsg);
   }
   return omsg;
}
