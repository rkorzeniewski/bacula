/*
 *
 *  Program to scan a Bacula Volume and compare it with
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

/* Forward referenced functions */
static void do_scan(char *fname);
static void record_cb(JCR *jcr, DEVICE *dev, DEV_BLOCK *block, DEV_RECORD *rec);
static void create_file_attributes_record(B_DB *db, char *fname, char *lname, int type,
			       char *ap, DEV_RECORD *rec);
static void create_media_record(B_DB *db, MEDIA_DBR *mr, VOLUME_LABEL *vl);
static void create_pool_record(B_DB *db, POOL_DBR *pr);
static void create_job_record(B_DB *db, JOB_DBR *mr, SESSION_LABEL *label);
static void update_job_record(B_DB *db, JOB_DBR *mr, SESSION_LABEL *elabel, 
			      DEV_RECORD *rec);
static void create_client_record(B_DB *db, CLIENT_DBR *cr);
static void create_fileset_record(B_DB *db, FILESET_DBR *fsr);
static void create_jobmedia_record(B_DB *db, JOBMEDIA_DBR *jmr, SESSION_LABEL *elabel);


/* Global variables */
static DEVICE *dev = NULL;
static B_DB *db;
static JCR *jcr;
static BSR *bsr;
static struct stat statp;
static int type;
static long record_file_index;
static POOLMEM *fname;			     /* original file name */
static POOLMEM *ofile;			     /* output name with prefix */
static POOLMEM *lname;			     /* link name */
static MEDIA_DBR mr;
static POOL_DBR pr;
static JOB_DBR jr;
static CLIENT_DBR cr;
static JOBMEDIA_DBR jmr;
static FILESET_DBR fsr;
static ATTR_DBR ar;
static SESSION_LABEL label;
static SESSION_LABEL elabel;
static uint32_t FirstIndex = 0;
static uint32_t LastIndex = 0;

static char *db_name = "bacula";
static char *db_user = "bacula";
static char *db_password = "";
static char *wd = "/tmp";
static int verbose = 0;
static int update_db = 0;

static void usage()
{
   fprintf(stderr,
"\nVersion: " VERSION " (" DATE ")\n\n"
"Usage: bscan [-d debug_level] <bacula-archive>\n"
"       -b bootstrap      specify a bootstrap file\n"
"       -dnn              set debug level to nn\n"
"       -n name           specify the database name (default bacula)\n"
"       -u user           specify database user name (default bacula)\n"
"       -p password       specify database password (default none)\n"
"       -s                synchronize or store in database\n"
"       -v                verbose\n"
"       -w dir            specify working directory (default /tmp)\n"
"       -?                print this message\n\n");
   exit(1);
}

int main (int argc, char *argv[])
{
   int ch;

   my_name_is(argc, argv, "bscan");
   init_msg(NULL, NULL);


   while ((ch = getopt(argc, argv, "b:d:n:p:u:vw:?")) != -1) {
      switch (ch) {
         case 'b':
	    bsr = parse_bsr(NULL, optarg);
	    break;
         case 'd':                    /* debug level */
	    debug_level = atoi(optarg);
	    if (debug_level <= 0)
	       debug_level = 1; 
	    break;

         case 'n':
	    db_name = optarg;
	    break;

         case 'u':
	    db_user = optarg;
	    break;

         case 'p':
	    db_password = optarg;
	    break;

         case 's':
	    update_db = 1;
	    break;

         case 'v':
	    verbose++;
	    break;

         case 'w':
	    wd = optarg;
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

   working_directory = wd;

   jcr = setup_jcr("bscan", argv[0], bsr);

   if ((db=db_init_database(NULL, db_name, db_user, db_password)) == NULL) {
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

   dev = setup_to_read_device(jcr);
   if (!dev) { 
      exit(1);
   }

   fname = get_pool_memory(PM_FNAME);
   ofile = get_pool_memory(PM_FNAME);
   lname = get_pool_memory(PM_FNAME);

   memset(&ar, 0, sizeof(ar));
   memset(&mr, 0, sizeof(mr));
   memset(&pr, 0, sizeof(pr));
   memset(&jr, 0, sizeof(jr));
   memset(&cr, 0, sizeof(cr));
   memset(&jmr, 0, sizeof(jmr));
   memset(&fsr, 0, sizeof(fsr));

   read_records(jcr, dev, record_cb, mount_next_read_volume);
   release_device(jcr, dev);

   free_pool_memory(fname);
   free_pool_memory(ofile);
   free_pool_memory(lname);
   term_dev(dev);
}

static void record_cb(JCR *jcr, DEVICE *dev, DEV_BLOCK *block, DEV_RECORD *rec)
{
   /* 
    * Check for Start or End of Session Record 
    *
    */
   if (rec->FileIndex < 0) {

      if (verbose > 1) {
	 dump_label_record(dev, rec, 1);
      }
      switch (rec->FileIndex) {
	 case PRE_LABEL:
            Pmsg0(000, "Volume is prelabeled. This tape cannot be scanned.\n");
	    return;
	    break;
	 case VOL_LABEL:
	    unser_volume_label(dev, rec);
	    /* Check Pool info */
	    strcpy(pr.Name, dev->VolHdr.PoolName);
	    strcpy(pr.PoolType, dev->VolHdr.PoolType);
	    if (!db_get_pool_record(db, &pr)) {
               Pmsg1(000, "VOL_LABEL: Pool record not found for Pool: %s\n",
		  pr.Name);
	       create_pool_record(db, &pr);
	       return;
	    } else if (verbose) {
               Pmsg1(000, "Pool record for %s found in DB.\n", pr.Name);
	    }
	    if (strcmp(pr.PoolType, dev->VolHdr.PoolType) != 0) {
               Pmsg2(000, "VOL_LABEL: PoolType mismatch. DB=%s Vol=%s\n",
		  pr.PoolType, dev->VolHdr.PoolType);
	       return;
	    } else if (verbose) {
               Pmsg1(000, "Pool type \"%s\" is OK.\n", pr.PoolType);
	    }

	    /* Check Media Info */
	    strcpy(mr.VolumeName, dev->VolHdr.VolName);
	    mr.PoolId = pr.PoolId;
	    if (!db_get_media_record(db, &mr)) {
               Pmsg1(000, "VOL_LABEL: Media record not found for Volume: %s\n",
		  mr.VolumeName);
	       strcpy(mr.MediaType, dev->VolHdr.MediaType);
	       create_media_record(db, &mr, &dev->VolHdr);
	       return;
	    } else if (verbose) {
               Pmsg1(000, "Media record for %s found in DB.\n", mr.VolumeName);
	    }
	    if (strcmp(mr.MediaType, dev->VolHdr.MediaType) != 0) {
               Pmsg2(000, "VOL_LABEL: MediaType mismatch. DB=%s Vol=%s\n",
		  mr.MediaType, dev->VolHdr.MediaType);
	       return;
	    } else if (verbose) {
               Pmsg1(000, "Media type \"%s\" is OK.\n", mr.MediaType);
	    }
            Pmsg1(000, "VOL_LABEL: OK for Volume: %s\n", mr.VolumeName);
	    break;
	 case SOS_LABEL:
	    unser_session_label(&label, rec);
	    FirstIndex = LastIndex = 0;
	    memset(&jr, 0, sizeof(jr));
	    jr.JobId = label.JobId;
	    if (!db_get_job_record(db, &jr)) {
               Pmsg1(000, "SOS_LABEL: Job record not found for JobId: %d\n",
		  jr.JobId);
	       create_job_record(db, &jr, &label);
	       jr.PoolId = pr.PoolId;
	       jr.VolSessionId = rec->VolSessionId;
	       jr.VolSessionTime = rec->VolSessionTime;
	       return;
	    }
	    if (rec->VolSessionId != jr.VolSessionId) {
               Pmsg3(000, "SOS_LABEL: VolSessId mismatch for JobId=%u. DB=%d Vol=%d\n",
		  jr.JobId,
		  jr.VolSessionId, rec->VolSessionId);
	       return;
	    }
	    if (rec->VolSessionTime != jr.VolSessionTime) {
               Pmsg3(000, "SOS_LABEL: VolSessTime mismatch for JobId=%u. DB=%d Vol=%d\n",
		  jr.JobId,
		  jr.VolSessionTime, rec->VolSessionTime);
	       return;
	    }
	    if (jr.PoolId != pr.PoolId) {
               Pmsg3(000, "SOS_LABEL: PoolId mismatch for JobId=%u. DB=%d Vol=%d\n",
		  jr.JobId,
		  jr.PoolId, pr.PoolId);
	       return;
	    }
	    break;
	 case EOS_LABEL:
	    unser_session_label(&elabel, rec);

	    /* Create Client record */
	    strcpy(cr.Name, label.ClientName);
	    create_client_record(db, &cr);
	    jr.ClientId = cr.ClientId;

	    /* Create FileSet record */
	    strcpy(fsr.FileSet, label.FileSetName);
	    create_fileset_record(db, &fsr);
	    jr.FileSetId = fsr.FileSetId;

	    /* Update Job record */
	    update_job_record(db, &jr, &elabel, rec);

	    /* Create JobMedia record */
	    jmr.JobId = jr.JobId;
	    jmr.MediaId = mr.MediaId;
	    create_jobmedia_record(db, &jmr, &elabel);
	    if (elabel.JobId != label.JobId) {
               Pmsg2(000, "EOS_LABEL: Start/End JobId mismatch. Start=%d End=%d\n",
		  label.JobId, elabel.JobId);
	       return;
	    }
	    if (elabel.JobFiles != jr.JobFiles) {
               Pmsg3(000, "EOS_LABEL: JobFiles mismatch for JobId=%u. DB=%d EOS=%d\n",
		  elabel.JobId,
		  jr.JobFiles, elabel.JobFiles);
	       return;
	    }				      
	    if (elabel.JobBytes != jr.JobBytes) {
               Pmsg3(000, "EOS_LABEL: JobBytes mismatch for JobId=%u. DB=%d EOS=%d\n",
		  elabel.JobId,
		  jr.JobBytes, elabel.JobBytes);
	       return;
	    }				      
            Pmsg1(000, "EOS_LABEL: OK for JobId=%d\n", elabel.JobId);
	    break;
	 case EOM_LABEL:
	    break;
	 default:
	    break;
      }
      return;
   }


   /* File Attributes stream */
   if (rec->Stream == STREAM_UNIX_ATTRIBUTES) {
      char *ap, *lp, *fp;

      if (sizeof_pool_memory(fname) < rec->data_len) {
	 fname = realloc_pool_memory(fname, rec->data_len + 1);
      }
      if (sizeof_pool_memory(lname) < rec->data_len) {
	 lname = realloc_pool_memory(lname, rec->data_len + 1);
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

      /* Skip through attributes to link name */
      lp = ap;
      while (*lp++ != 0) {
	 ;
      }
      strcat(lname, lp);        /* "save" link name */


      if (verbose > 1) {
	 decode_stat(ap, &statp);
	 print_ls_output(fname, lname, type, &statp);	
      }
      create_file_attributes_record(db, fname, lname, type, ap, rec);
      if (FirstIndex == 0) {
	 FirstIndex = rec->FileIndex;
      }
      LastIndex = rec->FileIndex;

   /* Data stream and extracting */
   } else if (rec->Stream == STREAM_FILE_DATA) {

   } else if (rec->Stream != STREAM_MD5_SIGNATURE) {
      Pmsg2(0, "None of above!!! stream=%d data=%s\n", rec->Stream, rec->data);
   }
   
   return;
}

static void create_file_attributes_record(B_DB *db, char *fname, char *lname, int type,
			       char *ap, DEV_RECORD *rec)
{
   if (!update_db) {
      return;
   }
   ar.fname = fname;
   ar.link = lname;
   ar.ClientId = cr.ClientId;
   ar.JobId = jr.JobId;
   ar.Stream = rec->Stream;
   ar.FileIndex = rec->FileIndex;
   ar.attr = ap;
   if (!db_create_file_attributes_record(db, &ar)) {
      Pmsg1(0, _("Could not create File Attributes record. ERR=%s\n"), db_strerror(db));
   }

}

static void create_media_record(B_DB *db, MEDIA_DBR *mr, VOLUME_LABEL *vl)
{
   struct date_time dt;
   struct tm tm;
   if (!update_db) {
      return;
   }
   strcpy(mr->VolStatus, "Full");
   mr->VolRetention = 355 * 3600 * 24; /* 1 year */
   dt.julian_day_number = vl->write_date;
   dt.julian_day_fraction = vl->write_time;
   tm_decode(&dt, &tm);
   mr->FirstWritten = mktime(&tm);
   dt.julian_day_number = vl->label_date;
   dt.julian_day_fraction = vl->label_time;
   tm_decode(&dt, &tm);
   mr->LabelDate = mktime(&tm);
   if (!db_create_media_record(db, mr)) {
      Pmsg1(0, _("Could not create media record. ERR=%s\n"), db_strerror(db));
   }
   if (!db_update_media_record(db, mr)) {
      Pmsg1(0, _("Could not update media record. ERR=%s\n"), db_strerror(db));
   }

}

static void create_pool_record(B_DB *db, POOL_DBR *pr)
{
   if (!update_db) {
      return;
   }
   pr->NumVols++;
   pr->UseCatalog = 1;
   pr->VolRetention = 355 * 3600 * 24; /* 1 year */
   if (!db_create_pool_record(db, pr)) {
      Pmsg1(0, _("Could not create pool record. ERR=%s\n"), db_strerror(db));
   }

}


static void create_client_record(B_DB *db, CLIENT_DBR *cr)
{
   if (!update_db) {
      return;
   }
   if (!db_create_client_record(db, cr)) {
      Pmsg1(0, _("Could not create Client record. ERR=%s\n"), db_strerror(db));
   }
}

static void create_fileset_record(B_DB *db, FILESET_DBR *fsr)
{
   if (!update_db) {
      return;
   }
   if (!db_create_fileset_record(db, fsr)) {
      Pmsg1(0, _("Could not create FileSet record. ERR=%s\n"), db_strerror(db));
   }
}

static void create_job_record(B_DB *db, JOB_DBR *jr, SESSION_LABEL *label)
{
   struct date_time dt;
   struct tm tm;

   if (!update_db) {
      return;
   }
   jr->JobId = label->JobId;
   jr->Type = label->JobType;
   jr->Level = label->JobLevel;
   strcpy(jr->Name, label->JobName);
   strcpy(jr->Job, label->Job);
   dt.julian_day_number = label->write_date;
   dt.julian_day_fraction = label->write_time;
   tm_decode(&dt, &tm);
   jr->SchedTime = mktime(&tm);
   jr->StartTime = jr->SchedTime;
   jr->JobTDate = (btime_t)jr->SchedTime;
   if (!db_create_job_record(db, jr)) {
      Pmsg1(0, _("Could not create job record. ERR=%s\n"), db_strerror(db));
   }

 
}

static void update_job_record(B_DB *db, JOB_DBR *jr, SESSION_LABEL *elabel,
			      DEV_RECORD *rec)
{
   struct date_time dt;
   struct tm tm;

   if (!update_db) {
      return;
   }
   dt.julian_day_number = elabel->write_date;
   dt.julian_day_fraction = elabel->write_time;
   tm_decode(&dt, &tm);
   jr->JobStatus = JS_Terminated;
   jr->EndTime = mktime(&tm);
   jr->JobFiles = elabel->JobFiles;
   jr->JobBytes = elabel->JobBytes;
   jr->VolSessionId = rec->VolSessionId;
   jr->VolSessionTime = rec->VolSessionTime;
   if (!db_create_job_record(db, jr)) {
      Pmsg1(0, _("Could not create job record. ERR=%s\n"), db_strerror(db));
   }

}

static void create_jobmedia_record(B_DB *db, JOBMEDIA_DBR *jmr, SESSION_LABEL *elabel)
{
   if (!update_db) {
      return;
   }
   jmr->FirstIndex = FirstIndex;
   jmr->LastIndex = LastIndex;
   jmr->StartFile = elabel->start_file;
   jmr->EndFile = elabel->end_file;
   jmr->StartBlock = elabel->start_block;
   jmr->EndBlock = elabel->end_block;
   if (!db_create_jobmedia_record(db, jmr)) {
      Pmsg1(0, _("Could not create JobMedia record. ERR=%s\n"), db_strerror(db));
   }

}


/* Dummies to replace askdir.c */
int	dir_get_volume_info(JCR *jcr, int writing) { return 1;}
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
