/*
 *
 *  Program to scan a Bacula tape and compare it with
 *    the catalog and optionally synchronize the catalog
 *    with the tape.
 *
 *   Kern E. Sibbald, December 2001
 *
 *
 *   Version $Id$
 */
/*
   Copyright (C) 2001, 2002 Kern Sibbald and John Walker

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
#include "cats/cats.h"

static void do_scan(char *fname);
static void print_ls_output(char *fname, struct stat *statp);


static DEVICE *dev = NULL;
static B_DB *db;
static JCR *jcr;

static void usage()
{
   fprintf(stderr,
"Usage: bscan [-d debug_level] <bacula-archive>\n"
"       -dnn            set debug level to nn\n"
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

   my_name_is(argc, argv, "bscan");
   init_msg(NULL, NULL);


   while ((ch = getopt(argc, argv, "d:?")) != -1) {
      switch (ch) {
         case 'd':                    /* debug level */
	    debug_level = atoi(optarg);
	    if (debug_level <= 0)
	       debug_level = 1; 
	    break;

         case '?':
	 default:
	    usage();

      }  
   }
   argc -= optind;
   argv += optind;

   if (argc != 1) {
      Pmsg0(0, "Wrong number of arguments: \n");
      usage();
   }

   jcr = new_jcr(sizeof(JCR), my_free_jcr);
   jcr->VolSessionId = 1;
   jcr->VolSessionTime = (uint32_t)time(NULL);
   jcr->NumVolumes = 1;

   /* *** FIXME **** need to put in corect db, user, and password */
   if ((db=db_init_database("bacula", "bacula", "")) == NULL) {
      Emsg0(M_ABORT, 0, "Could not init Bacula database\n");
   }
   if (!db_open_database(db)) {
      Emsg0(M_ABORT, 0, db_strerror(db));
   }
   Dmsg0(200, "Database opened\n");


   do_scan(argv[0]);

   free_jcr(jcr);
   return 0;
}
  

static void do_scan(char *devname)	       
{
   int n;     
   char VolName[100];
   char *p;
   struct stat statp;
   int type;
   long record_file_index;
   DEV_RECORD rec;
   DEV_BLOCK *block;
   char *fname; 		      /* original file name */
   char *ofile; 		      /* output name with prefix */
   char *lname; 		      /* link name */
   MEDIA_DBR mr;
   POOL_DBR pr;
   JOB_DBR jr;

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

   dev = init_dev(NULL, devname);
   if (!dev || !open_device(dev)) {
      Emsg1(M_ABORT, 0, "Cannot open %s\n", devname);
   }
   Dmsg0(90, "Device opened for read.\n");

   fname = (char *)get_pool_memory(PM_FNAME);
   ofile = (char *)get_pool_memory(PM_FNAME);
   lname = (char *)get_pool_memory(PM_FNAME);

   block = new_block(dev);

   strcpy(jcr->VolumeName, VolName);

   if (!acquire_device_for_read(jcr, dev, block)) {
      Emsg1(M_ABORT, 0, "Cannot open %s\n", devname);
   }

   memset(&rec, 0, sizeof(rec));
   rec.data = (char *)get_memory(70000);

   memset(&mr, 0, sizeof(mr));
   memset(&pr, 0, sizeof(pr));

   for ( ;; ) {
      if (!read_record(dev, block, &rec)) {
	 uint32_t status;
	 if (dev->state & ST_EOT) {
	    break;
	 }
	 if (dev->state & ST_EOF) {
	    continue;		      /* try again */
	 }
         Pmsg0(0, "Read Record got a bad record\n");
	 status_dev(dev, &status);
         Dmsg1(20, "Device status: %x\n", status);
	 if (status & MT_EOD)
            Emsg0(M_ABORT, 0, "Unexpected End of Data\n");
	 else if (status & MT_EOT)
            Emsg0(M_ABORT, 0, "Unexpected End of Tape\n");
	 else if (status & MT_EOF)
            Emsg0(M_ABORT, 0, "Unexpected End of File\n");
	 else if (status & MT_DR_OPEN)
            Emsg0(M_ABORT, 0, "Tape Door is Open\n");
	 else if (!(status & MT_ONLINE))
            Emsg0(M_ABORT, 0, "Unexpected Tape is Off-line\n");
	 else
            Emsg3(M_ABORT, 0, "Read error %d on Record Header %s: %s\n", n, dev_name(dev), strerror(errno));
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
	 SESSION_LABEL label, elabel;

	 if (debug_level > 1) {
	    dump_label_record(dev, &rec, 1);
	 }
	 switch (rec.FileIndex) {
	    case PRE_LABEL:
               Pmsg0(000, "Volume is prelabeled. This tape cannot be scanned.\n");
	       return;
	       break;
	    case VOL_LABEL:
	       unser_volume_label(dev, &rec);
	       strcpy(mr.VolumeName, dev->VolHdr.VolName);
	       if (!db_get_media_record(db, &mr)) {
                  Pmsg1(000, "VOL_LABEL: Media record not found for Volume: %s\n",
		     mr.VolumeName);
		  continue;
	       }
	       if (strcmp(mr.MediaType, dev->VolHdr.MediaType) != 0) {
                  Pmsg2(000, "VOL_LABEL: MediaType mismatch. DB=%s Vol=%s\n",
		     mr.MediaType, dev->VolHdr.MediaType);
		  continue;
	       }
	       strcpy(pr.Name, dev->VolHdr.PoolName);
	       if (!db_get_pool_record(db, &pr)) {
                  Pmsg1(000, "VOL_LABEL: Pool record not found for Pool: %s\n",
		     pr.Name);
		  continue;
	       }
	       if (strcmp(pr.PoolType, dev->VolHdr.PoolType) != 0) {
                  Pmsg2(000, "VOL_LABEL: PoolType mismatch. DB=%s Vol=%s\n",
		     pr.PoolType, dev->VolHdr.PoolType);
		  continue;
	       }
               Pmsg1(000, "VOL_LABEL: OK for Volume: %s\n", mr.VolumeName);
	       break;
	    case SOS_LABEL:
	       unser_session_label(&label, &rec);
	       memset(&jr, 0, sizeof(jr));
	       jr.JobId = label.JobId;
	       if (!db_get_job_record(db, &jr)) {
                  Pmsg1(000, "SOS_LABEL: Job record not found for JobId: %d\n",
		     jr.JobId);
		  continue;
	       }
	       if (rec.VolSessionId != jr.VolSessionId) {
                  Pmsg2(000, "SOS_LABEL: VolSessId mismatch. DB=%d Vol=%d\n",
		     jr.VolSessionId, rec.VolSessionId);
		  continue;
	       }
	       if (rec.VolSessionTime != jr.VolSessionTime) {
                  Pmsg2(000, "SOS_LABEL: VolSessTime mismatch. DB=%d Vol=%d\n",
		     jr.VolSessionTime, rec.VolSessionTime);
		  continue;
	       }
	       if (jr.PoolId != pr.PoolId) {
                  Pmsg2(000, "SOS_LABEL: PoolId mismatch. DB=%d Vol=%d\n",
		     jr.PoolId, pr.PoolId);
		  continue;
	       }
	       break;
	    case EOS_LABEL:
	       unser_session_label(&elabel, &rec);
	       if (elabel.JobId != label.JobId) {
                  Pmsg2(000, "EOS_LABEL: Start/End JobId mismatch. Start=%d End=%d\n",
		     label.JobId, elabel.JobId);
		  continue;
	       }
	       if (elabel.JobFiles != jr.JobFiles) {
                  Pmsg2(000, "EOS_LABEL: JobFiles mismatch. DB=%d EOS=%d\n",
		     jr.JobFiles, elabel.JobFiles);
		  continue;
	       }				 
	       if (elabel.JobBytes != jr.JobBytes) {
                  Pmsg2(000, "EOS_LABEL: JobBytes mismatch. DB=%d EOS=%d\n",
		     jr.JobBytes, elabel.JobBytes);
		  continue;
	       }				 
               Pmsg1(000, "EOS_LABEL: OK for JobId=%d\n", elabel.JobId);
	       break;
	    case EOM_LABEL:
	       break;
	    default:
	       break;
	 }
	 continue;
      }

      /* File Attributes stream */
      if (rec.Stream == STREAM_UNIX_ATTRIBUTES) {
	 char *ap, *lp;

	 if (sizeof_pool_memory(fname) < rec.data_len) {
	    fname = (char *)realloc_pool_memory(fname, rec.data_len + 1);
	 }
	 if (sizeof_pool_memory(lname) < rec.data_len) {
	    ofile = (char *)realloc_pool_memory(ofile, rec.data_len + 1);
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
         sscanf(rec.data, "%ld %d %s", &record_file_index, &type, fname);
	 if (record_file_index != rec.FileIndex)
            Emsg2(M_ABORT, 0, "Record header file index %ld not equal record index %ld\n",
	       rec.FileIndex, record_file_index);
	 ap = rec.data;
	 /* Skip to attributes */
	 while (*ap++ != 0)
	    ;
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


	 decode_stat(ap, &statp);
/*       Dmsg1(000, "Restoring: %s\n", ofile); */

	 if (debug_level > 1) {
	    print_ls_output(fname, &statp);   
	 }

      /* Data stream and extracting */
      } else if (rec.Stream == STREAM_FILE_DATA) {

      } else if (rec.Stream != STREAM_MD5_SIGNATURE) {
         Pmsg2(0, "None of above!!! stream=%d data=%s\n", rec.Stream, rec.data);
      }
   }

   release_device(jcr, dev, block);

   free_pool_memory(fname);
   free_pool_memory(ofile);
   free_pool_memory(lname);
   term_dev(dev);
   free_block(block);
   return;
}

extern char *getuser(uid_t uid);
extern char *getgroup(gid_t gid);

static void print_ls_output(char *fname, struct stat *statp)
{
   char buf[1000]; 
   char *p, *f;
   int n;

   p = encode_mode(statp->st_mode, buf);
   n = sprintf(p, "  %2d ", (uint32_t)statp->st_nlink);
   p += n;
   n = sprintf(p, "%-8.8s %-8.8s", getuser(statp->st_uid), getgroup(statp->st_gid));
   p += n;
   n = sprintf(p, "%8lld  ", (uint64_t)statp->st_size);
   p += n;
   p = encode_time(statp->st_ctime, p);
   *p++ = ' ';
   *p++ = ' ';
   for (f=fname; *f; )
      *p++ = *f++;
   *p++ = '\n';
   *p = 0;
   fputs(buf, stdout);
}
