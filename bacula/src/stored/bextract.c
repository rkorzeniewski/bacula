/*
 *
 *  Dumb program to extract files from a Bacula backup.
 *
 *   Kern E. Sibbald
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
#include "findlib/find.h"


static void do_extract(char *fname, char *prefix);
static void print_ls_output(char *fname, char *link, int type, struct stat *statp);


static DEVICE *dev = NULL;
static int ofd = -1;

static JCR *jcr;
static FF_PKT my_ff;
static FF_PKT *ff = &my_ff;

static BSR *bsr = NULL;

static void usage()
{
   fprintf(stderr,
"Usage: bextract [-d debug_level] <bacula-archive> <directory-to-store-files>\n"
"       -b <file>       specify a bootstrap file\n"
"       -dnn            set debug level to nn\n"
"       -e <file>       exclude list\n"
"       -i <file>       include list\n"
"       -?              print this message\n\n");
   exit(1);
}

static void my_free_jcr(JCR *jcr)
{
   return;
}


int main (int argc, char *argv[])
{
   int ch;   
   FILE *fd;
   char line[1000];
   int got_inc = FALSE;

   my_name_is(argc, argv, "bextract");
   init_msg(NULL, NULL);	      /* setup message handler */

   memset(ff, 0, sizeof(FF_PKT));
   init_include_exclude_files(ff);

   while ((ch = getopt(argc, argv, "b:d:e:i:?")) != -1) {
      switch (ch) {
         case 'b':                    /* bootstrap file */
	    bsr = parse_bsr(NULL, optarg);
//	    dump_bsr(bsr);
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
               Dmsg1(900, "add_exclude %s\n", line);
	       add_fname_to_exclude_list(ff, line);
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
               Dmsg1(900, "add_include %s\n", line);
	       add_fname_to_include_list(ff, 0, line);
	    }
	    fclose(fd);
	    got_inc = TRUE;
	    break;

         case '?':
	 default:
	    usage();

      }  
   }
   argc -= optind;
   argv += optind;

   if (argc != 2) {
      Pmsg0(0, "Wrong number of arguments: \n");
      usage();
   }
   if (!got_inc) {			      /* If no include file, */
      add_fname_to_include_list(ff, 0, "/");  /*   include everything */
   }

   jcr = new_jcr(sizeof(JCR), my_free_jcr);
   jcr->VolSessionId = 1;
   jcr->VolSessionTime = (uint32_t)time(NULL);
   jcr->bsr = bsr;
   strcpy(jcr->Job, "bextract");
   jcr->dev_name = get_pool_memory(PM_FNAME);
   strcpy(jcr->dev_name, argv[0]);

   do_extract(argv[0], argv[1]);

   free_jcr(jcr);
   if (bsr) {
      free_bsr(bsr);
   }
   return 0;
}
  

static void do_extract(char *devname, char *where)
{
   char VolName[100];
   char *p;
   struct stat statp;
   int extract = FALSE;
   int type;
   long record_file_index;
   long total = 0;
   DEV_RECORD rec;
   DEV_BLOCK *block;
   POOLMEM *fname;		      /* original file name */
   POOLMEM *ofile;		      /* output name with prefix */
   POOLMEM *lname;		      /* link name */
   int wherelen;		      /* prefix length */
   SESSION_LABEL sessrec;

   if (strncmp(devname, "/dev/", 5) != 0) {
      /* Try stripping file part */
      p = devname + strlen(devname);
      while (p >= devname && *p != '/') {
	 p--;
      }
      if (*p == '/') {
	 strcpy(VolName, p+1);
	 *p = 0;
      }
   }
   strcpy(jcr->VolumeName, VolName);

   dev = init_dev(NULL, devname);
   if (!dev || !open_device(dev)) {
      Emsg1(M_ERROR_TERM, 0, "Cannot open %s\n", devname);
   }
   Dmsg0(90, "Device opened for read.\n");

   if (stat(where, &statp) < 0) {
      Emsg2(M_ERROR_TERM, 0, "Cannot stat %s. It must exist. ERR=%s\n",
	 where, strerror(errno));
   }
   if (!S_ISDIR(statp.st_mode)) {
      Emsg1(M_ERROR_TERM, 0, "%s must be a directory.\n", where);
   }

   wherelen = strlen(where);
   fname = get_pool_memory(PM_FNAME);
   ofile = get_pool_memory(PM_FNAME);
   lname = get_pool_memory(PM_FNAME);

   block = new_block(dev);

   create_vol_list(jcr);

   Dmsg1(20, "Found %d volumes names to restore.\n", jcr->NumVolumes);

   /* 
    * Ready device for reading, and read records
    */
   if (!acquire_device_for_read(jcr, dev, block)) {
      free_block(block);
      free_vol_list(jcr);
      return;
   }

   memset(&rec, 0, sizeof(rec));
   rec.data = get_memory(70000);

   uint32_t compress_buf_size = 70000;
   POOLMEM *compress_buf = get_memory(compress_buf_size);

   for ( ;; ) {
      int ok;
      DEV_RECORD *record;	      /* for reading label of multi-volumes */

      if (!read_record(dev, block, &rec)) {
	 uint32_t status;
         Dmsg1(500, "Main read record failed. rem=%d\n", rec.remainder);
	 if (dev->state & ST_EOT) {
	    if (rec.remainder) {
               Dmsg0(500, "Not end of record.\n");
	    }
            Dmsg2(90, "NumVolumes=%d CurVolume=%d\n", jcr->NumVolumes, jcr->CurVolume);
	    /*
	     * End Of Tape -- mount next Volume (if another specified)
	     */
	    if (jcr->NumVolumes > 1 && jcr->CurVolume < jcr->NumVolumes) {
	       VOL_LIST *vol = jcr->VolList;
	       /* Find next Volume */
	       jcr->CurVolume++;
	       for (int i=1; i<jcr->CurVolume; i++) {
		  vol = vol->next;
	       }
	       strcpy(jcr->VolumeName, vol->VolumeName);
               Dmsg1(400, "There is another volume %s.\n", jcr->VolumeName);

	       close_dev(dev);
	       dev->state &= ~ST_READ; 
	       if (!acquire_device_for_read(jcr, dev, block)) {
                  Emsg2(M_FATAL, 0, "Cannot open Dev=%s, Vol=%s\n", dev_name(dev),
			jcr->VolumeName);
		  ok = FALSE;
		  break;
	       }
	       record = new_record();
               Dmsg1(500, "read record after new tape. rem=%d\n", record->remainder);
	       read_record(dev, block, record); /* read vol label */
	       dump_label_record(dev, record, 0);
	       free_record(record);
	       continue;
	    }
            Dmsg0(90, "End of Device reached.\n");
	    break;		      /* End of Tape */
	 }
	 if (dev->state & ST_EOF) {
	    continue;		      /* try again */
	 }
         Pmsg0(0, "Read Record got a bad record\n");
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
            Emsg3(M_ERROR_TERM, 0, "Read error %d on Record Header %s: %s\n", 
	       status, dev_name(dev), strerror(errno));
      }


      /* This is no longer used */
      if (rec.VolSessionId == 0 && rec.VolSessionTime == 0) {
         Emsg0(M_ERROR, 0, "Zero header record. This shouldn't happen.\n");
	 break; 		      /* END OF FILE */
      }

      /* 
       * Check for Start or End of Session Record 
       *
       */
      if (rec.FileIndex < 0) {
	 char *rtype;
	 memset(&sessrec, 0, sizeof(sessrec));
	 switch (rec.FileIndex) {
	    case PRE_LABEL:
               rtype = "Fresh Volume Label";   
	       break;
	    case VOL_LABEL:
               rtype = "Volume Label";
	       unser_volume_label(dev, &rec);
	       break;
	    case SOS_LABEL:
               rtype = "Begin Session";
	       unser_session_label(&sessrec, &rec);
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
	 if (debug_level > 0) {
            printf("%s Record: VolSessionId=%d VolSessionTime=%d JobId=%d DataLen=%d\n",
	       rtype, rec.VolSessionId, rec.VolSessionTime, rec.Stream, rec.data_len);
	 }

         Dmsg1(40, "Got label = %d\n", rec.FileIndex);
	 if (rec.FileIndex == EOM_LABEL) { /* end of tape? */
            Dmsg0(40, "Get EOM LABEL\n");
	    break;			   /* yes, get out */
	 }
	 continue;			   /* ignore other labels */
      } /* end if label record */

      /* Is this the file we want? */
      if (bsr && !match_bsr(bsr, &rec, &dev->VolHdr, &sessrec)) {
	 continue;
      }

      /* File Attributes stream */
      if (rec.Stream == STREAM_UNIX_ATTRIBUTES) {
	 char *ap, *lp, *fp;

	 /* If extracting, it was from previous stream, so
	  * close the output file.
	  */
	 if (extract) {
	    if (ofd < 0) {
               Emsg0(M_ERROR_TERM, 0, "Logic error output file should be open\n");
	    }
	    close(ofd);
	    ofd = -1;
	    extract = FALSE;
	    set_statp(jcr, fname, ofile, lname, type, &statp);
	 }

	 if (sizeof_pool_memory(fname) < rec.data_len) {
	    fname = realloc_pool_memory(fname, rec.data_len + 1);
	 }
	 if (sizeof_pool_memory(ofile) < sizeof_pool_memory(fname) + wherelen + 1) {
	    ofile = realloc_pool_memory(ofile, sizeof_pool_memory(fname) + wherelen + 1);
	 }
	 if (sizeof_pool_memory(lname) < rec.data_len) {
	    ofile = realloc_pool_memory(ofile, rec.data_len + 1);
	 }
	 *fname = 0;
	 *lname = 0;

	 /*		 
	  * An Attributes record consists of:
	  *    File_index
	  *    Type   (FT_types)
	  *    Filename
	  *    Attributes
	  *    Link name (if file linked i.e. FT_LNK)
	  *
	  */
         sscanf(rec.data, "%ld %d", &record_file_index, &type);
	 if (record_file_index != rec.FileIndex)
            Emsg2(M_ERROR_TERM, 0, "Record header file index %ld not equal record index %ld\n",
	       rec.FileIndex, record_file_index);
	 ap = rec.data;
         while (*ap++ != ' ')         /* skip record file index */
	    ;
         while (*ap++ != ' ')         /* skip type */
	    ;
	 /* Save filename and position to attributes */
	 fp = fname;
	 while (*ap != 0) {
	    *fp++  = *ap++;
	 }
	 *fp = *ap++;		      /* terminate filename & point to attribs */

	 /* Skip to Link name */
	 if (type == FT_LNK) {
	    lp = ap;
	    while (*lp++ != 0) {
	       ;
	    }
            strcat(lname, lp);        /* "save" link name */
	 } else {
	    *lname = 0;
	 }

	    
	 if (file_is_included(ff, fname) && !file_is_excluded(ff, fname)) {

	    decode_stat(ap, &statp);
	    /*
	     * Prepend the where directory so that the
	     * files are put where the user wants.
	     *
	     * We do a little jig here to handle Win32 files with
	     * a drive letter.	
	     *	 If where is null and we are running on a win32 client,
	     *	    change nothing.
	     *	 Otherwise, if the second character of the filename is a
	     *	 colon (:), change it into a slash (/) -- this creates
	     *	 a reasonable pathname on most systems.
	     */
	    strcpy(ofile, where);
            if (fname[1] == ':') {
               fname[1] = '/';
	       strcat(ofile, fname);
               fname[1] = ':';
	    } else {
	       strcat(ofile, fname);
	    }
/*          Pmsg1(000, "Restoring: %s\n", ofile); */

	    extract = create_file(jcr, fname, ofile, lname, type, &statp, &ofd);

	    if (extract) {
		print_ls_output(ofile, lname, type, &statp);   
	    }
	 }

      /* Data stream and extracting */
      } else if (rec.Stream == STREAM_FILE_DATA) {
	 if (extract) {
	    total += rec.data_len;
            Dmsg2(8, "Write %ld bytes, total=%ld\n", rec.data_len, total);
	    if ((uint32_t)write(ofd, rec.data, rec.data_len) != rec.data_len) {
               Emsg1(M_ERROR_TERM, 0, "Write error: %s\n", strerror(errno));
	    }
	 }
 
      } else if (rec.Stream == STREAM_GZIP_DATA) {
#ifdef HAVE_LIBZ
	 if (extract) {
	    uLongf compress_len;

	    compress_len = compress_buf_size;
	    if (uncompress((Bytef *)compress_buf, &compress_len, 
		  (const Bytef *)rec.data, (uLong)rec.data_len) != Z_OK) {
               Emsg0(M_ERROR_TERM, 0, _("Uncompression error.\n"));
	    }

            Dmsg2(100, "Write uncompressed %d bytes, total before write=%d\n", compress_len, total);
	    if ((uLongf)write(ofd, compress_buf, (size_t)compress_len) != compress_len) {
               Pmsg0(0, "===Write error===\n");
               Emsg2(M_ERROR_TERM, 0, "Write error on %s: %s\n", ofile, strerror(errno));
	    }
	    total += compress_len;
            Dmsg2(100, "Compress len=%d uncompressed=%d\n", rec.data_len,
	       compress_len);
	 }
#else
	 if (extract) {
            Emsg0(M_ERROR_TERM, 0, "GZIP data stream found, but GZIP not configured!\n");
	 }
#endif


      /* If extracting, wierd stream (not 1 or 2), close output file anyway */
      } else if (extract) {
	 if (ofd < 0) {
            Emsg0(M_ERROR_TERM, 0, "Logic error output file should be open\n");
	 }
	 close(ofd);
	 ofd = -1;
	 extract = FALSE;
	 set_statp(jcr, fname, ofile, lname, type, &statp);
      } else if (rec.Stream != STREAM_MD5_SIGNATURE) {
         Pmsg2(0, "None of above!!! stream=%d data=%s\n", rec.Stream, rec.data);
      }
   }

   /* If output file is still open, it was the last one in the
    * archive since we just hit an end of file, so close the file. 
    */
   if (ofd >= 0) {
      close(ofd);
      set_statp(jcr, fname, ofile, lname, type, &statp);
   }
   release_device(jcr, dev, block);

   free_pool_memory(fname);
   free_pool_memory(ofile);
   free_pool_memory(lname);
   free_pool_memory(compress_buf);
   term_dev(dev);
   free_block(block);
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
int	dir_create_job_media_record(JCR *jcr) { return 1; }
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
