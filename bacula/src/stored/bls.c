/*
 *
 *  Dumb program to do an "ls" of a Bacula 1.0 mortal file.
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

#include "bacula.h"
#include "stored.h"
#include "findlib/find.h"

static void do_blocks(char *infname);
static void do_jobs(char *infname);
static void do_ls(char *fname);
static void print_ls_output(char *fname, char *link, int type, struct stat *statp);
static void do_setup(char *infname);
static void do_close();

static DEVICE *dev;
static int default_tape = FALSE;
static int dump_label = FALSE;
static int list_blocks = FALSE;
static int list_jobs = FALSE;
static int verbose = 0;
static char Vol[2000];
static char *VolName;
static char *p;
static DEV_RECORD *rec;
static DEV_BLOCK *block;
static int NumVolumes, CurVolume;
static JCR *jcr;


extern char BaculaId[];

static FF_PKT ff;

static void usage()
{
   fprintf(stderr,
"Usage: bls [-d debug_level] <physical-device-name>\n"
"       -b              list blocks\n"
"       -e <file>       exclude list\n"
"       -i <file>       include list\n"
"       -j              list jobs\n"
"       -L              list tape label\n"
"    (none of above)    list saved files\n"
"       -t              use default tape device\n"
"       -v              be verbose\n"
"       -?              print this message\n\n");
   exit(1);
}


int main (int argc, char *argv[])
{
   int i, ch;
   FILE *fd;
   char line[1000];

   my_name_is(argc, argv, "bls");
   init_msg(NULL, NULL);	      /* initialize message handler */

   memset(&ff, 0, sizeof(ff));
   init_include_exclude_files(&ff);

   while ((ch = getopt(argc, argv, "bd:e:i:jLtv?")) != -1) {
      switch (ch) {
         case 'b':
	    list_blocks = TRUE;
	    break;
         case 'd':                    /* debug level */
	    debug_level = atoi(optarg);
	    if (debug_level <= 0)
	       debug_level = 1; 
	    break;

         case 'e':                    /* exclude list */
            if ((fd = fopen(optarg, "r")) == NULL) {
               Pmsg2(0, "Could not open exclude file: %s, ERR=%s\n",
		  optarg, strerror(errno));
	       exit(1);
	    }
	    while (fgets(line, sizeof(line), fd) != NULL) {
	       strip_trailing_junk(line);
               Dmsg1(000, "add_exclude %s\n", line);
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
               Dmsg1(000, "add_include %s\n", line);
	       add_fname_to_include_list(&ff, 0, line);
	    }
	    fclose(fd);
	    break;

         case 'j':
	    list_jobs = TRUE;
	    break;

         case 'L':
	    dump_label = TRUE;
	    break;

         case 't':
	    default_tape = TRUE;
	    break;

         case 'v':
	    verbose++;
	    break;

         case '?':
	 default:
	    usage();

      }  
   }
   argc -= optind;
   argv += optind;

   if (!argc && !default_tape) {
      Pmsg0(0, "No archive name specified\n");
      usage();
   }

   if (ff.included_files_list == NULL) {
      add_fname_to_include_list(&ff, 0, "/");
   }

   /* Try default device */
   if (default_tape) {
      do_ls(DEFAULT_TAPE_DRIVE);
      return 0;
   }

   for (i=0; i < argc; i++) {
      do_setup(argv[i]);
      if (list_blocks) {
	 do_blocks(argv[i]);
      } else if (list_jobs) {
	 do_jobs(argv[i]);
      } else {
	 do_ls(argv[i]);
      }
      do_close();
   }
   return 0;
}

static void my_free_jcr(JCR *jcr)
{
   return;
}

/*
 * Setup device, jcr, and prepare to read
 */
static void do_setup(char *infname) 
{
   jcr = new_jcr(sizeof(JCR), my_free_jcr);
   VolName = Vol;
   VolName[0] = 0;
   if (strncmp(infname, "/dev/", 5) != 0) {
      /* Try stripping file part */
      p = infname + strlen(infname);
      while (p >= infname && *p != '/')
	 p--;
      if (*p == '/') {
	 strcpy(VolName, p+1);
	 *p = 0;
      }
   }
   Dmsg2(10, "Device=%s, Vol=%s.\n", infname, VolName);
   dev = init_dev(NULL, infname);
   if (!dev) {
      Emsg1(M_FATAL, 0, "Cannot open %s\n", infname);
      exit(1);
   }
   /* ***FIXME**** init capabilities */
   if (!open_device(dev)) {
      Emsg1(M_FATAL, 0, "Cannot open %s\n", infname);
      exit(1);
   }
   Dmsg0(90, "Device opened for read.\n");

   rec = new_record();
   block = new_block(dev);

   NumVolumes = 0;
   CurVolume = 1;
   for (p = VolName; p && *p; ) {
      p = strchr(p, '^');
      if (p) {
	 *p++ = 0;
      }
      NumVolumes++;
   }

   jcr->VolumeName = (char *)check_pool_memory_size(jcr->VolumeName, strlen(VolName)+1);
   strcpy(jcr->VolumeName, VolName);
   if (!acquire_device_for_read(jcr, dev, block)) {
      Emsg0(M_ERROR, 0, dev->errmsg);
      exit(1);
   }
}

static void do_close()
{
   term_dev(dev);
   free_record(rec);
   free_block(block);
   free_jcr(jcr);
}

static int mount_next_volume(char *infname)
{
   if (rec->remainder) {
      Dmsg0(20, "Not end of record. Next volume has more data for current record.\n");
   }
   Dmsg2(20, "NumVolumes=%d CurVolume=%d\n", NumVolumes, CurVolume);
   if (NumVolumes > 1 && CurVolume < NumVolumes) {
      p = VolName;
      while (*p++)  
	 { }
      CurVolume++;
      Dmsg1(20, "There is another volume %s.\n", p);
      VolName = p;
      jcr->VolumeName = check_pool_memory_size(jcr->VolumeName, 
			  strlen(VolName)+1);
      strcpy(jcr->VolumeName, VolName);

      close_dev(dev);
      dev->state &= ~ST_READ; 
      if (!acquire_device_for_read(jcr, dev, block)) {
         Emsg2(M_FATAL, 0, "Cannot open Dev=%s, Vol=%s\n", infname, VolName);
	 exit(1);
      }
      return 1; 	     /* Next volume mounted */
   }
   printf("End of Device reached.\n");
   return 0;		     /* EOT */
}

/*
 * Device got an error, attempt to analyse it
 */
static void display_error_status()
{
   uint32_t status;

   Emsg0(M_ERROR, 0, dev->errmsg);
   status_dev(dev, &status);
   Dmsg1(20, "Device status: %x\n", status);
   if (status & MT_EOD)
      Emsg0(M_ERROR_TERM, 0, "Unexpected End of Data\n");
   else if (status & MT_EOT)
      Emsg0(M_ERROR_TERM, 0, "Unexpected End of Tape\n");
   else if (status & MT_EOF)
      Emsg0(M_ERROR_TERM, 0, "Unexpected End of File\n");
   else if (status & MT_DR_OPEN)
      Emsg0(M_ERROR_TERM, 0, "Tape Door is Open\n");
   else if (!(status & MT_ONLINE))
      Emsg0(M_ERROR_TERM, 0, "Unexpected Tape is Off-line\n");
   else
      Emsg2(M_ERROR_TERM, 0, "Read error on Record Header %s: %s\n", dev_name(dev), strerror(errno));
}


/* List just block information */
static void do_blocks(char *infname)
{

   dump_volume_label(dev);

   /* Assume that we have already read the volume label.
    * If on second or subsequent volume, adjust buffer pointer 
    */
   if (dev->VolHdr.PrevVolName[0] != 0) { /* second volume */
      Pmsg1(0, "\n\
Warning, this Volume is a continuation of Volume %s\n",
		dev->VolHdr.PrevVolName);
   }
 
   for ( ;; ) {

      if (!read_block_from_device(dev, block)) {
         Dmsg0(20, "!read_record()\n");
	 if (dev->state & ST_EOT) {
	    if (!mount_next_volume(infname)) {
	       break;
	    }
	    continue;
	 }
	 if (dev->state & ST_EOF) {
            Emsg1(M_INFO, 0, "Got EOF on device %s\n", dev_name(dev));
            Dmsg0(20, "read_record got eof. try again\n");
	    continue;
	 }
	 if (dev->state & ST_SHORT) {
	    Emsg0(M_INFO, 0, dev->errmsg);
	    continue;
	 }
	 display_error_status();
	 break;
      }

      printf("Block: %d size=%d\n", block->BlockNumber, block->block_len);

   }
   return;
}

/* Do list job records */
static void do_jobs(char *infname)
{

   /* Assume that we have already read the volume label.
    * If on second or subsequent volume, adjust buffer pointer 
    */
   if (dev->VolHdr.PrevVolName[0] != 0) { /* second volume */
      Pmsg1(0, "\n\
Warning, this Volume is a continuation of Volume %s\n",
		dev->VolHdr.PrevVolName);
   }
 
   for ( ;; ) {
      if (!read_record(dev, block, rec)) {
         Dmsg0(20, "!read_record()\n");
	 if (dev->state & ST_EOT) {
	    if (!mount_next_volume(infname)) {
	       break;
	    }
	    continue;
	 }
	 if (dev->state & ST_EOF) {
            Emsg1(M_INFO, 0, "Got EOF on device %s\n", dev_name(dev));
            Dmsg0(20, "read_record got eof. try again\n");
	    continue;
	 }
	 if (dev->state & ST_SHORT) {
	    Emsg0(M_INFO, 0, dev->errmsg);
	    continue;
	 }
	 display_error_status();
	 break;
      }

      if (debug_level >= 30) {
         Dmsg4(30, "VolSId=%ld FI=%s Strm=%s Size=%ld\n", rec->VolSessionId,
	       FI_to_ascii(rec->FileIndex), stream_to_ascii(rec->Stream), 
	       rec->data_len);
      }


      /*  
       * Check for End of File record (all zeros)
       *    NOTE: this no longer exists
       */
      if (rec->VolSessionId == 0 && rec->VolSessionTime == 0) {
         Emsg0(M_ERROR_TERM, 0, "Zero VolSessionId and VolSessionTime. This shouldn't happen\n");
      }

      /* 
       * Check for Start or End of Session Record 
       *
       */
      if (rec->FileIndex < 0) {
	 dump_label_record(dev, rec, verbose);
	 continue;
      }
   }
   return;
}

/* Do an ls type listing of an archive */
static void do_ls(char *infname)
{
   char fname[2000];
   struct stat statp;
   int type;
   long record_file_index;

   if (dump_label) {
      dump_volume_label(dev);
      return;
   }

   /* Assume that we have already read the volume label.
    * If on second or subsequent volume, adjust buffer pointer 
    */
   if (dev->VolHdr.PrevVolName[0] != 0) { /* second volume */
      Pmsg1(0, "\n\
Warning, this Volume is a continuation of Volume %s\n",
		dev->VolHdr.PrevVolName);
   }
 
   for ( ;; ) {
      if (!read_record(dev, block, rec)) {
         Dmsg0(20, "!read_record()\n");
	 if (dev->state & ST_EOT) {
	    if (!mount_next_volume(infname)) {
	       break;
	    }
	    continue;
	 }
	 if (dev->state & ST_EOF) {
            Emsg1(M_INFO, 0, "Got EOF on device %s\n", dev_name(dev));
            Dmsg0(20, "read_record got eof. try again\n");
	    continue;
	 }
	 if (dev->state & ST_SHORT) {
	    Emsg0(M_INFO, 0, dev->errmsg);
	    continue;
	 }
	 display_error_status();
	 break;
      }

      if (debug_level >= 30) {
         Dmsg4(30, "VolSId=%ld FI=%s Strm=%s Size=%ld\n", rec->VolSessionId,
	       FI_to_ascii(rec->FileIndex), stream_to_ascii(rec->Stream), 
	       rec->data_len);
      }


      /*  
       * Check for End of File record (all zeros)
       *    NOTE: this no longer exists
       */
      if (rec->VolSessionId == 0 && rec->VolSessionTime == 0) {
         Emsg0(M_ERROR_TERM, 0, "Zero VolSessionId and VolSessionTime. This shouldn't happen\n");
      }

      /* 
       * Check for Start or End of Session Record 
       *
       */
      if (rec->FileIndex < 0) {
	 dump_label_record(dev, rec, 0);
	 continue;
      }

      /* File Attributes stream */
      if (rec->Stream == STREAM_UNIX_ATTRIBUTES) {
	 char *ap;
         sscanf(rec->data, "%ld %d %s", &record_file_index, &type, fname);
	 if (record_file_index != rec->FileIndex) {
            Emsg2(M_ERROR_TERM, 0, "Record header file index %ld not equal record index %ld\n",
	       rec->FileIndex, record_file_index);
	 }
	 ap = rec->data;
	 /* Skip to attributes */
	 while (*ap++ != 0)
	    ;
	 decode_stat(ap, &statp);
	 /* Skip to link name */  
	 while (*ap++ != 0)
	    ;
	 print_ls_output(fname, ap, type, &statp);
      }
   }
   return;
}

extern char *getuser(uid_t uid);
extern char *getgroup(gid_t gid);

static void print_ls_output(char *fname, char *link, int type, struct stat *statp)
{
   char buf[1000]; 
   char ec1[30];
   char *p, *f;
   int n;

   if (!file_is_included(&ff, fname) || file_is_excluded(&ff, fname)) {
      return;
   }
   p = encode_mode(statp->st_mode, buf);
   n = sprintf(p, "  %2d ", (uint32_t)statp->st_nlink);
   p += n;
   n = sprintf(p, "%-8.8s %-8.8s", getuser(statp->st_uid), getgroup(statp->st_gid));
   p += n;
   n = sprintf(p, "%8.8s ", edit_uint64(statp->st_size, ec1));
   p += n;
   p = encode_time(statp->st_ctime, p);
   *p++ = ' ';
   *p++ = ' ';
   /* Copy file name */
   for (f=fname; *f && (p-buf) < (int)sizeof(buf); )
      *p++ = *f++;
   if (type == FT_LNK) {
      *p++ = ' ';
      *p++ = '-';
      *p++ = '>';
      *p++ = ' ';
      /* Copy link name */
      for (f=link; *f && (p-buf) < (int)sizeof(buf); )
	 *p++ = *f++;
   }
   *p++ = '\n';
   *p = 0;
   fputs(buf, stdout);
}

/* Dummies to replace askdir.c */
int	dir_get_volume_info(JCR *jcr) { return 1;}
int	dir_find_next_appendable_volume(JCR *jcr) { return 1;}
int	dir_update_volume_info(JCR *jcr, VOLUME_CAT_INFO *vol, int relabel) { return 1; }
int	dir_ask_sysop_to_mount_next_volume(JCR *jcr, DEVICE *dev) { return 1; }
int	dir_update_file_attributes(JCR *jcr, DEV_RECORD *rec) { return 1;}
int	dir_send_job_status(JCR *jcr) {return 1;}


int dir_ask_sysop_to_mount_volume(JCR *jcr, DEVICE *dev)
{
   fprintf(stderr, "Mount Volume %s on device %s and press return when ready: ",
      jcr->VolumeName, dev_name(dev));
   getchar();	
   return 1;
}
