/*
 *
 *  Dumb program to do an "ls" of a Bacula 1.0 mortal file.
 *
 *   Version $Id$
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
#include "findlib/find.h"

#if defined(HAVE_CYGWIN) || defined(HAVE_WIN32)
int win32_client = 1;
#else
int win32_client = 0;
#endif

static void do_blocks(char *infname);
static void do_jobs(char *infname);
static void do_ls(char *fname);
static void do_close(JCR *jcr);
static void get_session_record(DEVICE *dev, DEV_RECORD *rec, SESSION_LABEL *sessrec);
static int record_cb(JCR *jcr, DEVICE *dev, DEV_BLOCK *block, DEV_RECORD *rec);

static DEVICE *dev;
static int dump_label = FALSE;
static int list_blocks = FALSE;
static int list_jobs = FALSE;
static DEV_RECORD *rec;
static DEV_BLOCK *block;
static JCR *jcr;
static SESSION_LABEL sessrec;
static uint32_t num_files = 0;
static ATTR *attr;

#define CONFIG_FILE "bacula-sd.conf"
char *configfile;


static FF_PKT ff;

static BSR *bsr = NULL;

static void usage()
{
   fprintf(stderr,
"\nVersion: " VERSION " (" BDATE ")\n\n"
"Usage: bls [-d debug_level] <physical-device-name>\n"
"       -b <file>       specify a bootstrap file\n"
"       -c <file>       specify a config file\n"
"       -d <level>      specify debug level\n"
"       -e <file>       exclude list\n"
"       -i <file>       include list\n"
"       -j              list jobs\n"
"       -k              list blocks\n"
"    (no j or k option) list saved files\n"
"       -L              dump label\n"
"       -p              proceed inspite of errors\n"
"       -v              be verbose\n"
"       -V              specify Volume names (separated by |)\n"
"       -?              print this message\n\n");
   exit(1);
}


int main (int argc, char *argv[])
{
   int i, ch;
   FILE *fd;
   char line[1000];
   char *VolumeName= NULL;
   char *bsrName = NULL;
   bool ignore_label_errors = false;

   working_directory = "/tmp";
   my_name_is(argc, argv, "bls");
   init_msg(NULL, NULL);	      /* initialize message handler */

   memset(&ff, 0, sizeof(ff));
   init_include_exclude_files(&ff);

   while ((ch = getopt(argc, argv, "b:c:d:e:i:jkLpvV:?")) != -1) {
      switch (ch) {
      case 'b':
	 bsrName = optarg;
	 break;

      case 'c':                    /* specify config file */
	 if (configfile != NULL) {
	    free(configfile);
	 }
	 configfile = bstrdup(optarg);
	 break;

      case 'd':                    /* debug level */
	 debug_level = atoi(optarg);
	 if (debug_level <= 0)
	    debug_level = 1; 
	 break;

      case 'e':                    /* exclude list */
         if ((fd = fopen(optarg, "r")) == NULL) {
            Pmsg2(0, _("Could not open exclude file: %s, ERR=%s\n"),
	       optarg, strerror(errno));
	    exit(1);
	 }
	 while (fgets(line, sizeof(line), fd) != NULL) {
	    strip_trailing_junk(line);
            Dmsg1(100, "add_exclude %s\n", line);
	    add_fname_to_exclude_list(&ff, line);
	 }
	 fclose(fd);
	 break;

      case 'i':                    /* include list */
         if ((fd = fopen(optarg, "r")) == NULL) {
            Pmsg2(0, "Could not open include file: %s, ERR=%s\n",
	       optarg, strerror(errno));
	    exit(1);
	 }
	 while (fgets(line, sizeof(line), fd) != NULL) {
	    strip_trailing_junk(line);
            Dmsg1(100, "add_include %s\n", line);
	    add_fname_to_include_list(&ff, 0, line);
	 }
	 fclose(fd);
	 break;

      case 'j':
	 list_jobs = TRUE;
	 break;

      case 'k':
	 list_blocks = TRUE;
	 break;

      case 'L':
	 dump_label = TRUE;
	 break;

      case 'p':
	 ignore_label_errors = true;
	 break;

      case 'v':
	 verbose++;
	 break;

      case 'V':                    /* Volume name */
	 VolumeName = optarg;
	 break;

      case '?':
      default:
	 usage();

      } /* end switch */
   } /* end while */
   argc -= optind;
   argv += optind;

   if (!argc) {
      Pmsg0(0, _("No archive name specified\n"));
      usage();
   }

   if (configfile == NULL) {
      configfile = bstrdup(CONFIG_FILE);
   }

   parse_config(configfile);

   if (ff.included_files_list == NULL) {
      add_fname_to_include_list(&ff, 0, "/");
   }

   for (i=0; i < argc; i++) {
      if (bsrName) {
	 bsr = parse_bsr(NULL, bsrName);
      }
      jcr = setup_jcr("bls", argv[i], bsr, VolumeName);
      jcr->ignore_label_errors = ignore_label_errors;
      dev = setup_to_access_device(jcr, 1);   /* acquire for read */
      if (!dev) {
	 exit(1);
      }
      rec = new_record();
      block = new_block(dev);
      attr = new_attr();
      /*
       * Assume that we have already read the volume label.
       * If on second or subsequent volume, adjust buffer pointer 
       */
      if (dev->VolHdr.PrevVolName[0] != 0) { /* second volume */
         Pmsg1(0, "\n\
Warning, this Volume is a continuation of Volume %s\n",
		dev->VolHdr.PrevVolName);
      }

      if (list_blocks) {
	 do_blocks(argv[i]);
      } else if (list_jobs) {
	 do_jobs(argv[i]);
      } else {
	 do_ls(argv[i]);
      }
      do_close(jcr);
   }
   if (bsr) {
      free_bsr(bsr);
   }
   return 0;
}


static void do_close(JCR *jcr)
{
   release_device(jcr, dev);
   free_attr(attr);
   term_dev(dev);
   free_record(rec);
   free_block(block);
   free_jcr(jcr);
}


/* List just block information */
static void do_blocks(char *infname)
{
   if (verbose) {
      dump_volume_label(dev);
      rec = new_record();
   }
   for ( ;; ) {
      if (!read_block_from_device(jcr, dev, block, NO_BLOCK_NUMBER_CHECK)) {
         Dmsg1(100, "!read_block(): ERR=%s\n", strerror_dev(dev));
	 if (dev->state & ST_EOT) {
	    if (!mount_next_read_volume(jcr, dev, block)) {
               Jmsg(jcr, M_INFO, 0, _("Got EOM at file %u on device %s, Volume \"%s\"\n"), 
		  dev->file, dev_name(dev), jcr->VolumeName);
	       break;
	    }
	    /* Read and discard Volume label */
	    DEV_RECORD *record;
	    record = new_record();
	    read_block_from_device(jcr, dev, block, NO_BLOCK_NUMBER_CHECK);
	    read_record_from_block(block, record);
	    get_session_record(dev, record, &sessrec);
	    free_record(record);
            Jmsg(jcr, M_INFO, 0, _("Mounted Volume \"%s\".\n"), jcr->VolumeName);
	    
	 } else if (dev->state & ST_EOF) {
            Jmsg(jcr, M_INFO, 0, _("Got EOF at file %u on device %s, Volume \"%s\"\n"), 
	       dev->file, dev_name(dev), jcr->VolumeName);
            Dmsg0(20, "read_record got eof. try again\n");
	    continue;
	 } else if (dev->state & ST_SHORT) {
            Jmsg(jcr, M_INFO, 0, "%s", dev->errmsg);
	    continue;
	 } else {
	    /* I/O error */
	    display_tape_error_status(jcr, dev);
	    break;
	 }
      }
      if (!match_bsr_block(bsr, block)) {
         Dmsg5(100, "reject Blk=%u blen=%u bVer=%d SessId=%u SessTim=%u\n",
	    block->BlockNumber, block->block_len, block->BlockVer,
	    block->VolSessionId, block->VolSessionTime);
	 continue;
      }
      Dmsg5(100, "Blk=%u blen=%u bVer=%d SessId=%u SessTim=%u\n",
	block->BlockNumber, block->block_len, block->BlockVer,
	block->VolSessionId, block->VolSessionTime);
      if (verbose == 1) {
	 read_record_from_block(block, rec);
         Pmsg7(-1, "Block: %u blen=%u First rec FI=%s SessId=%u SessTim=%u Strm=%s rlen=%d\n",
	      block->BlockNumber, block->block_len,
	      FI_to_ascii(rec->FileIndex), rec->VolSessionId, rec->VolSessionTime,
	      stream_to_ascii(rec->Stream, rec->FileIndex), rec->data_len);
	 rec->remainder = 0;
      } else if (verbose > 1) {
         dump_block(block, "");
      } else {
         printf("Block: %d size=%d\n", block->BlockNumber, block->block_len);
      }

   }
   return;
}

/*
 * We are only looking for labels or in particula Job Session records
 */
static int jobs_cb(JCR *jcr, DEVICE *dev, DEV_BLOCK *block, DEV_RECORD *rec)
{
   if (rec->FileIndex < 0) {
      dump_label_record(dev, rec, verbose);
   }
   rec->remainder = 0;
   return 1;
}

/* Do list job records */
static void do_jobs(char *infname)
{
   read_records(jcr, dev, jobs_cb, mount_next_read_volume);
}

/* Do an ls type listing of an archive */
static void do_ls(char *infname)
{
   if (dump_label) {
      dump_volume_label(dev);
      return;
   }
   read_records(jcr, dev, record_cb, mount_next_read_volume);
   printf("%u files found.\n", num_files);
}

/*
 * Called here for each record from read_records()
 */
static int record_cb(JCR *jcr, DEVICE *dev, DEV_BLOCK *block, DEV_RECORD *rec)
{
   if (rec->FileIndex < 0) {
      get_session_record(dev, rec, &sessrec);
      return 1;
   }
   /* File Attributes stream */
   if (rec->Stream == STREAM_UNIX_ATTRIBUTES || 
       rec->Stream == STREAM_UNIX_ATTRIBUTES_EX) {

      if (!unpack_attributes_record(jcr, rec->Stream, rec->data, attr)) {
         Emsg0(M_ERROR_TERM, 0, _("Cannot continue.\n"));
      }

      if (attr->file_index != rec->FileIndex) {
         Emsg2(M_ERROR_TERM, 0, _("Record header file index %ld not equal record index %ld\n"),
	    rec->FileIndex, attr->file_index);
      }

      attr->data_stream = decode_stat(attr->attr, &attr->statp, &attr->LinkFI);
      build_attr_output_fnames(jcr, attr);

      if (file_is_included(&ff, attr->fname) && !file_is_excluded(&ff, attr->fname)) {
	 print_ls_output(jcr, attr);
	 num_files++;
      }
   }
   return 1;
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
      rtype = "End of Medium";
      break;
   default:
      rtype = "Unknown";
      break;
   }
   Dmsg5(10, "%s Record: VolSessionId=%d VolSessionTime=%d JobId=%d DataLen=%d\n",
	 rtype, rec->VolSessionId, rec->VolSessionTime, rec->Stream, rec->data_len);
}


/* Dummies to replace askdir.c */
int	dir_get_volume_info(JCR *jcr, enum get_vol_info_rw  writing) { return 1;}
int	dir_find_next_appendable_volume(JCR *jcr) { return 1;}
int	dir_update_volume_info(JCR *jcr, DEVICE *dev, int relabel) { return 1; }
int	dir_create_jobmedia_record(JCR *jcr) { return 1; }
int	dir_ask_sysop_to_create_appendable_volume(JCR *jcr, DEVICE *dev) { return 1; }
int	dir_update_file_attributes(JCR *jcr, DEV_RECORD *rec) { return 1;}
int	dir_send_job_status(JCR *jcr) {return 1;}


int dir_ask_sysop_to_mount_volume(JCR *jcr, DEVICE *dev)
{
   fprintf(stderr, "Mount Volume \"%s\" on device %s and press return when ready: ",
      jcr->VolumeName, dev_name(dev));
   getchar();	
   return 1;
}
