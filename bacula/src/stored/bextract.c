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

#ifdef HAVE_CYGWIN
int win32_client = 1;
#else
int win32_client = 0;
#endif


static void do_extract(char *fname);
static void record_cb(JCR *jcr, DEVICE *dev, DEV_BLOCK *block, DEV_RECORD *rec);

static DEVICE *dev = NULL;
static int ofd = -1;
static JCR *jcr;
static FF_PKT my_ff;
static FF_PKT *ff = &my_ff;
static BSR *bsr = NULL;
static DEV_BLOCK *block;
static int extract = FALSE;
static long record_file_index;
static long total = 0;
static POOLMEM *fname;			  /* original file name */
static POOLMEM *ofile;			  /* output name with prefix */
static POOLMEM *lname;			  /* link name */
static char *where;
static int wherelen;			  /* prefix length */
static uint32_t num_files = 0;
static struct stat statp;
static uint32_t compress_buf_size = 70000;
static POOLMEM *compress_buf;
static int type;

static void usage()
{
   fprintf(stderr,
"\nVersion: " VERSION " (" DATE ")\n\n"
"Usage: bextract [-d debug_level] <bacula-archive> <directory-to-store-files>\n"
"       -b <file>       specify a bootstrap file\n"
"       -dnn            set debug level to nn\n"
"       -e <file>       exclude list\n"
"       -i <file>       include list\n"
"       -?              print this message\n\n");
   exit(1);
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

   where = argv[1];
   do_extract(argv[0]);

   if (bsr) {
      free_bsr(bsr);
   }
   return 0;
}

static void do_extract(char *devname)
{

   jcr = setup_jcr("bextract", devname, bsr);
   dev = setup_to_read_device(jcr);
   if (!dev) {
      exit(1);
   }

   /* Make sure where directory exists and that it is a directory */
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




   compress_buf = get_memory(compress_buf_size);

   read_records(jcr, dev, record_cb, mount_next_read_volume);
   /* If output file is still open, it was the last one in the
    * archive since we just hit an end of file, so close the file. 
    */
   if (ofd >= 0) {
      close(ofd);
      set_statp(jcr, fname, ofile, lname, type, &statp);
   }
   release_device(jcr, dev);

   free_pool_memory(fname);
   free_pool_memory(ofile);
   free_pool_memory(lname);
   free_pool_memory(compress_buf);
   term_dev(dev);
   free_jcr(jcr);
   printf("%u files restored.\n", num_files);
   return;
}

/*
 * Called here for each record from read_records()
 */
static void record_cb(JCR *jcr, DEVICE *dev, DEV_BLOCK *block, DEV_RECORD *rec)
{
   if (rec->FileIndex < 0) {
      return;                         /* we don't want labels */
   }

   /* File Attributes stream */
   if (rec->Stream == STREAM_UNIX_ATTRIBUTES) {
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

      if (sizeof_pool_memory(fname) < rec->data_len) {
	 fname = realloc_pool_memory(fname, rec->data_len + 1);
      }
      if (sizeof_pool_memory(ofile) < rec->data_len + wherelen + 1) {
	 ofile = realloc_pool_memory(ofile, rec->data_len + wherelen + 1);
      }
      if (sizeof_pool_memory(lname) < rec->data_len) {
	 lname = realloc_pool_memory(lname, rec->data_len + wherelen + 1);
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
      sscanf(rec->data, "%ld %d", &record_file_index, &type);
      if (record_file_index != rec->FileIndex)
         Emsg2(M_ERROR_TERM, 0, "Record header file index %ld not equal record index %ld\n",
	    rec->FileIndex, record_file_index);
      ap = rec->data;
      while (*ap++ != ' ')         /* skip record file index */
	 ;
      while (*ap++ != ' ')         /* skip type */
	 ;
      /* Save filename and position to attributes */
      fp = fname;
      while (*ap != 0) {
	 *fp++	= *ap++;
      }
      *fp = *ap++;		   /* terminate filename & point to attribs */

      /* Skip to Link name */
      if (type == FT_LNK || type == FT_LNKSAVED) {
	 lp = ap;
	 while (*lp++ != 0) {
	    ;
	 }
      } else {
         lp = "";
      }

	 
      if (file_is_included(ff, fname) && !file_is_excluded(ff, fname)) {

	 decode_stat(ap, &statp);
	 /*
	  * Prepend the where directory so that the
	  * files are put where the user wants.
	  *
	  * We do a little jig here to handle Win32 files with
	  * a drive letter.  
	  *   If where is null and we are running on a win32 client,
	  *	 change nothing.
	  *   Otherwise, if the second character of the filename is a
	  *   colon (:), change it into a slash (/) -- this creates
	  *   a reasonable pathname on most systems.
	  */
	 if (where[0] == 0 && win32_client) {
	    strcpy(ofile, fname);
	    strcpy(lname, lp);
	 } else {
	    strcpy(ofile, where);
            if (fname[1] == ':') {
               fname[1] = '/';
	       strcat(ofile, fname);
               fname[1] = ':';
	    } else {
	       strcat(ofile, fname);
	    }
	    /* Fixup link name */
	    if (type == FT_LNK || type == FT_LNKSAVED) {
               if (lp[0] == '/') {      /* if absolute path */
		  strcpy(lname, where);
	       }       
               /* ***FIXME**** we shouldn't have links on Windoz */
               if (lp[1] == ':') {
                  lp[1] = '/';
		  strcat(lname, lp);
                  lp[1] = ':';
	       } else {
		  strcat(lname, lp);
	       }
	    }
	 }

           /*          Pmsg1(000, "Restoring: %s\n", ofile); */

	 extract = create_file(jcr, fname, ofile, lname, type, &statp, &ofd);
	 num_files++;

	 if (extract) {
	     print_ls_output(ofile, lname, type, &statp);   
	 }
      }

   /* Data stream and extracting */
   } else if (rec->Stream == STREAM_FILE_DATA) {
      if (extract) {
	 total += rec->data_len;
         Dmsg2(8, "Write %ld bytes, total=%ld\n", rec->data_len, total);
	 if ((uint32_t)write(ofd, rec->data, rec->data_len) != rec->data_len) {
            Emsg1(M_ERROR_TERM, 0, "Write error: %s\n", strerror(errno));
	 }
      }

   } else if (rec->Stream == STREAM_GZIP_DATA) {
#ifdef HAVE_LIBZ
      if (extract) {
	 uLongf compress_len;

	 compress_len = compress_buf_size;
	 if (uncompress((Bytef *)compress_buf, &compress_len, 
	       (const Bytef *)rec->data, (uLong)rec->data_len) != Z_OK) {
            Emsg0(M_ERROR_TERM, 0, _("Uncompression error.\n"));
	 }

         Dmsg2(100, "Write uncompressed %d bytes, total before write=%d\n", compress_len, total);
	 if ((uLongf)write(ofd, compress_buf, (size_t)compress_len) != compress_len) {
            Pmsg0(0, "===Write error===\n");
            Emsg2(M_ERROR_TERM, 0, "Write error on %s: %s\n", ofile, strerror(errno));
	 }
	 total += compress_len;
         Dmsg2(100, "Compress len=%d uncompressed=%d\n", rec->data_len,
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
   } else if (rec->Stream != STREAM_MD5_SIGNATURE) {
      Pmsg2(0, "None of above!!! stream=%d data=%s\n", rec->Stream, rec->data);
   }
}




/* Dummies to replace askdir.c */
int	dir_get_volume_info(JCR *jcr) { return 1;}
int	dir_find_next_appendable_volume(JCR *jcr) { return 1;}
int	dir_update_volume_info(JCR *jcr, VOLUME_CAT_INFO *vol, int relabel) { return 1; }
int	dir_create_jobmedia_record(JCR *jcr) { return 1; }
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
