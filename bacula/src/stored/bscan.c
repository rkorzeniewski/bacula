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
static int  create_file_attributes_record(B_DB *db, char *fname, char *lname, int type,
			       char *ap, DEV_RECORD *rec);
static int  create_media_record(B_DB *db, MEDIA_DBR *mr, VOLUME_LABEL *vl);
static int  create_pool_record(B_DB *db, POOL_DBR *pr);
static int  create_job_record(B_DB *db, JOB_DBR *mr, SESSION_LABEL *label, DEV_RECORD *rec);
static int  update_job_record(B_DB *db, JOB_DBR *mr, SESSION_LABEL *elabel, 
			      DEV_RECORD *rec);
static int  create_client_record(B_DB *db, CLIENT_DBR *cr);
static int  create_fileset_record(B_DB *db, FILESET_DBR *fsr);
static int  create_jobmedia_record(B_DB *db, JCR *jcr);
static void create_jcr(JOB_DBR *jr, DEV_RECORD *rec, uint32_t JobId);


/* Global variables */
static DEVICE *dev = NULL;
static B_DB *db;
static JCR *jcr;		      /* jcr for bscan */
static JCR *jobjcr;		      /* jcr for simulating running job */
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
static FILESET_DBR fsr;
static ATTR_DBR ar;
static SESSION_LABEL label;
static SESSION_LABEL elabel;

static char *db_name = "bacula";
static char *db_user = "bacula";
static char *db_password = "";
static char *wd = "/tmp";
static int verbose = 0;
static int update_db = 0;

static void usage()
{
   fprintf(stderr, _(
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
"       -?                print this message\n\n"));
   exit(1);
}

int main (int argc, char *argv[])
{
   int ch;

   my_name_is(argc, argv, "bscan");
   init_msg(NULL, NULL);


   while ((ch = getopt(argc, argv, "b:d:n:p:su:vw:?")) != -1) {
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
      Pmsg0(0, _("Wrong number of arguments: \n"));
      usage();
   }

   working_directory = wd;

   jcr = setup_jcr("bscan", argv[0], bsr);

   if ((db=db_init_database(NULL, db_name, db_user, db_password)) == NULL) {
      Emsg0(M_ERROR_TERM, 0, _("Could not init Bacula database\n"));
   }
   if (!db_open_database(db)) {
      Emsg0(M_ERROR_TERM, 0, db_strerror(db));
   }
   Dmsg0(200, "Database opened\n");
   if (verbose) {
      Pmsg2(000, _("Using Database: %s, User: %s\n"), db_name, db_user);
   }

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
   memset(&pr, 0, sizeof(pr));
   memset(&jr, 0, sizeof(jr));
   memset(&cr, 0, sizeof(cr));
   memset(&fsr, 0, sizeof(fsr));

   detach_jcr_from_device(dev, jcr);

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
	    if (db_get_pool_record(db, &pr)) {
	       if (verbose) {
                  Pmsg1(000, "Pool record for %s found in DB.\n", pr.Name);
	       }
	    } else {
               Pmsg1(000, "VOL_LABEL: Pool record not found for Pool: %s\n",
		  pr.Name);
	       create_pool_record(db, &pr);
	    }
	    if (strcmp(pr.PoolType, dev->VolHdr.PoolType) != 0) {
               Pmsg2(000, "VOL_LABEL: PoolType mismatch. DB=%s Vol=%s\n",
		  pr.PoolType, dev->VolHdr.PoolType);
	       return;
	    } else if (verbose) {
               Pmsg1(000, "Pool type \"%s\" is OK.\n", pr.PoolType);
	    }

	    /* Check Media Info */
	    memset(&mr, 0, sizeof(mr));
	    strcpy(mr.VolumeName, dev->VolHdr.VolName);
	    mr.PoolId = pr.PoolId;
	    if (db_get_media_record(db, &mr)) {
	       if (verbose) {
                  Pmsg1(000, "Media record for %s found in DB.\n", mr.VolumeName);
	       }
	    } else {
               Pmsg1(000, "VOL_LABEL: Media record not found for Volume: %s\n",
		  mr.VolumeName);
	       strcpy(mr.MediaType, dev->VolHdr.MediaType);
	       create_media_record(db, &mr, &dev->VolHdr);
	    }
	    if (strcmp(mr.MediaType, dev->VolHdr.MediaType) != 0) {
               Pmsg2(000, "VOL_LABEL: MediaType mismatch. DB=%s Vol=%s\n",
		  mr.MediaType, dev->VolHdr.MediaType);
	       return;
	    } else if (verbose) {
               Pmsg1(000, "Media type \"%s\" is OK.\n", mr.MediaType);
	    }
	    /* Reset some JCR variables */
	    for (JCR *mjcr=NULL; (mjcr=next_attached_jcr(dev, mjcr)); ) {
	       mjcr->VolFirstFile = mjcr->FileIndex = 0;
	       mjcr->start_block = mjcr->end_block = 0;
	       mjcr->start_file = mjcr->end_file = 0;
	    }

            Pmsg1(000, "VOL_LABEL: OK for Volume: %s\n", mr.VolumeName);
	    break;
	 case SOS_LABEL:
	    unser_session_label(&label, rec);
	    memset(&jr, 0, sizeof(jr));
	    jr.JobId = label.JobId;
	    if (db_get_job_record(db, &jr)) {
	       /* Job record already exists in DB */
	       create_jcr(&jr, rec, jr.JobId);
	       attach_jcr_to_device(dev, jobjcr);
	       if (verbose) {
                  Pmsg1(000, _("SOS_LABEL: Found Job record for JobId: %d\n"), jr.JobId);
	       }
	    } else {
	     
	       /* Must create a Job record in DB */
               Pmsg1(000, "SOS_LABEL: Job record not found for JobId: %d\n",
		  jr.JobId);

	       /* Create Client record */
	       strcpy(cr.Name, label.ClientName);
	       create_client_record(db, &cr);
	       jr.ClientId = cr.ClientId;

	       create_job_record(db, &jr, &label, rec);
	       jr.PoolId = pr.PoolId;
	       /* Set start positions into JCR */
	       jobjcr->start_block = dev->block_num;
	       jobjcr->start_file = dev->file;
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

	    /* Create FileSet record */
	    strcpy(fsr.FileSet, label.FileSetName);
	    create_fileset_record(db, &fsr);
	    jr.FileSetId = fsr.FileSetId;

	    /* Do the final update to the Job record */
	    update_job_record(db, &jr, &elabel, rec);

	    /* Create JobMedia record */
	    jobjcr = get_jcr_by_session(rec->VolSessionId, rec->VolSessionTime);
	    if (!jobjcr) {
               Pmsg2(000, _("Could not find SessId=%d SessTime=%d for EOS record.\n"),
		   rec->VolSessionId, rec->VolSessionTime);
	       break;
	    }

	    jobjcr->end_block = dev->block_num;
	    jobjcr->end_file = dev->file;
	    create_jobmedia_record(db, jobjcr);
	    detach_jcr_from_device(dev, jobjcr);
	    free_jcr(jobjcr);

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

   /* Data stream and extracting */
   } else if (rec->Stream == STREAM_FILE_DATA) {

   } else if (rec->Stream == STREAM_GZIP_DATA) {

   } else if (rec->Stream == STREAM_MD5_SIGNATURE) {
      if (verbose > 1) {
         Pmsg0(000, _("Got MD5 record.\n"));
      }
      /* ****FIXME**** implement db_update_md5_record */
   } else {
      Pmsg2(0, _("Unknown stream type!!! stream=%d data=%s\n"), rec->Stream, rec->data);
   }
   return;
}

/*
 * Free the Job Control Record if no one is still using it.
 *  Called from main free_jcr() routine in src/lib/jcr.c so
 *  that we can do our Director specific cleanup of the jcr.
 */
static void dird_free_jcr(JCR *jcr)
{
   Dmsg0(200, "Start dird free_jcr\n");

   if (jcr->file_bsock) {
      Dmsg0(200, "Close File bsock\n");
      bnet_close(jcr->file_bsock);
   }
   if (jcr->store_bsock) {
      Dmsg0(200, "Close Store bsock\n");
      bnet_close(jcr->store_bsock);
   }
   if (jcr->RestoreBootstrap) {
      free(jcr->RestoreBootstrap);
   }
   Dmsg0(200, "End dird free_jcr\n");
}

/*
 * We got a File Attributes record on the tape.  Now, lookup the Job
 *   record, and then create the attributes record.
 */
static int create_file_attributes_record(B_DB *db, char *fname, char *lname, int type,
			       char *ap, DEV_RECORD *rec)
{
   JCR *mjcr;

   if (!update_db) {
      return 1;
   }

   mjcr = get_jcr_by_session(rec->VolSessionId, rec->VolSessionTime);
   if (!mjcr) {
      Pmsg2(000, _("Could not find Job SessId=%d SessTime=%d for Attributes record.\n"),
		   rec->VolSessionId, rec->VolSessionTime);
      return 0;
   }
   ar.fname = fname;
   ar.link = lname;
   ar.ClientId = mjcr->ClientId;
   ar.JobId = mjcr->JobId;
   ar.Stream = rec->Stream;
   ar.FileIndex = rec->FileIndex;
   ar.attr = ap;
   if (mjcr->VolFirstFile == 0) {
      mjcr->VolFirstFile = rec->FileIndex;
   }
   mjcr->FileIndex = rec->FileIndex;
   free_jcr(mjcr);		      /* done using JCR */

   if (!db_create_file_attributes_record(db, &ar)) {
      Pmsg1(0, _("Could not create File Attributes record. ERR=%s\n"), db_strerror(db));
      return 0;
   }
   if (verbose > 1) {
      Pmsg1(000, _("Created File record: %s\n"), fname);   
   }
   return 1;
}

/*
 * For each Volume we see, we create a Medium record
 */
static int create_media_record(B_DB *db, MEDIA_DBR *mr, VOLUME_LABEL *vl)
{
   struct date_time dt;
   struct tm tm;
   if (!update_db) {
      return 1;
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
      return 0;
   }
   if (!db_update_media_record(db, mr)) {
      Pmsg1(0, _("Could not update media record. ERR=%s\n"), db_strerror(db));
      return 0;
   }
   if (verbose) {
      Pmsg2(000, _("Created Media record for Volume: %s, JobId: %d\n"), 
		   mr->VolumeName, jr.JobId);
   }
   return 1;

}

static int create_pool_record(B_DB *db, POOL_DBR *pr)
{
   if (!update_db) {
      return 1;
   }
   pr->NumVols++;
   pr->UseCatalog = 1;
   pr->VolRetention = 355 * 3600 * 24; /* 1 year */
   if (!db_create_pool_record(db, pr)) {
      Pmsg1(0, _("Could not create pool record. ERR=%s\n"), db_strerror(db));
      return 0;
   }
   if (verbose) {
      Pmsg1(000, _("Created Pool record for Pool: %s\n"), pr->Name);
   }
   return 1;

}


/*
 * Called from SOS to create a client for the current Job 
 */
static int create_client_record(B_DB *db, CLIENT_DBR *cr)
{
   if (!update_db) {
      return 1;
   }
   if (!db_create_client_record(db, cr)) {
      Pmsg1(0, _("Could not create Client record. ERR=%s\n"), db_strerror(db));
      return 0;
   }
   if (verbose) {
      Pmsg1(000, _("Created Client record for Client: %s\n"), cr->Name);
   }
   return 1;
}

static int create_fileset_record(B_DB *db, FILESET_DBR *fsr)
{
   if (!update_db) {
      return 1;
   }
   if (!db_create_fileset_record(db, fsr)) {
      Pmsg1(0, _("Could not create FileSet record. ERR=%s\n"), db_strerror(db));
      return 0;
   }
   if (verbose) {
      Pmsg1(000, _("Created FileSet record %s\n"), fsr->FileSet);
   }
   return 1;
}

/*
 * Simulate the two calls on the database to create
 *  the Job record and to update it when the Job actually
 *  begins running.
 */
static int create_job_record(B_DB *db, JOB_DBR *jr, SESSION_LABEL *label, 
			     DEV_RECORD *rec)
{
   struct date_time dt;
   struct tm tm;

   if (!update_db) {
      return 1;
   }
   Pmsg1(000, _("Creating Job record for JobId: %d\n"), jr->JobId);

   jr->JobId = label->JobId;
   jr->Type = label->JobType;
   jr->Level = label->JobLevel;
   jr->JobStatus = JS_Created;
   strcpy(jr->Name, label->JobName);
   strcpy(jr->Job, label->Job);
   dt.julian_day_number = label->write_date;
   dt.julian_day_fraction = label->write_time;
   tm_decode(&dt, &tm);
   jr->SchedTime = mktime(&tm);
   jr->StartTime = jr->SchedTime;
   jr->JobTDate = (btime_t)jr->SchedTime;
   jr->VolSessionId = rec->VolSessionId;
   jr->VolSessionTime = rec->VolSessionTime;

   /* This creates the bare essentials */
   if (!db_create_job_record(db, jr)) {
      Pmsg1(0, _("Could not create job record. ERR=%s\n"), db_strerror(db));
      return 0;
   }

   /* Now create a JCR as if starting the Job */
   create_jcr(jr, rec, label->JobId);

   /* This adds the client, StartTime, JobTDate, ... */
   if (!db_update_job_start_record(db, jr)) {
      Pmsg1(0, _("Could not update job start record. ERR=%s\n"), db_strerror(db));
      return 0;
   }
   if (verbose) {
      Pmsg1(000, _("Created Job record for JobId: %d\n"), jr->JobId);
   }
   return 1;
}

/* 
 * Simulate the database call that updates the Job 
 *  at Job termination time.
 */
static int update_job_record(B_DB *db, JOB_DBR *jr, SESSION_LABEL *elabel,
			      DEV_RECORD *rec)
{
   struct date_time dt;
   struct tm tm;
   JCR *mjcr;

   if (!update_db) {
      return 1;
   }
   mjcr = get_jcr_by_session(rec->VolSessionId, rec->VolSessionTime);
   if (!mjcr) {
      Pmsg2(000, _("Could not find SessId=%d SessTime=%d for EOS record.\n"),
		   rec->VolSessionId, rec->VolSessionTime);
      return 0;
   }
   dt.julian_day_number = elabel->write_date;
   dt.julian_day_fraction = elabel->write_time;
   tm_decode(&dt, &tm);
   jr->JobId = mjcr->JobId;
   jr->JobStatus = JS_Terminated;     /* ***FIXME*** need to add to EOS label */
   jr->EndTime = mktime(&tm);
   jr->JobFiles = elabel->JobFiles;
   jr->JobBytes = elabel->JobBytes;
   jr->VolSessionId = rec->VolSessionId;
   jr->VolSessionTime = rec->VolSessionTime;
   jr->JobTDate = (btime_t)mjcr->start_time;
   jr->ClientId = mjcr->ClientId;
   if (!db_update_job_end_record(db, jr)) {
      Pmsg1(0, _("Could not update job record. ERR=%s\n"), db_strerror(db));
      free_jcr(mjcr);
      return 0;
   }
   if (verbose) {
      Pmsg1(000, _("Updated Job termination record for JobId: %d\n"), jr->JobId);
   }
   free_jcr(mjcr);
   return 1;
}

static int create_jobmedia_record(B_DB *db, JCR *mjcr)
{
   JOBMEDIA_DBR jmr;

   if (!update_db) {
      return 1;
   }
   memset(&jmr, 0, sizeof(jmr));
   jmr.JobId = mjcr->JobId;
   jmr.MediaId = mr.MediaId;
   jmr.FirstIndex = mjcr->VolFirstFile;
   jmr.LastIndex = mjcr->FileIndex;
   jmr.StartFile = mjcr->start_file;
   jmr.EndFile = mjcr->end_file;
   jmr.StartBlock = mjcr->start_block;
   jmr.EndBlock = mjcr->end_block;

   if (!db_create_jobmedia_record(db, &jmr)) {
      Pmsg1(0, _("Could not create JobMedia record. ERR=%s\n"), db_strerror(db));
      return 0;
   }
   if (verbose) {
      Pmsg2(000, _("Created JobMedia record JobId %d, MediaId %d\n"), 
		jmr.JobId, jmr.MediaId);
   }
   return 1;
}

/* 
 * Create a JCR as if we are really starting the job
 */
static void create_jcr(JOB_DBR *jr, DEV_RECORD *rec, uint32_t JobId)
{
   /*
    * Transfer as much as possible to the Job JCR. Most important is
    *	the JobId and the ClientId.
    */
   jobjcr = new_jcr(sizeof(JCR), dird_free_jcr);
   jobjcr->JobType = jr->Type;
   jobjcr->JobLevel = jr->Level;
   jobjcr->JobStatus = jr->JobStatus;
   strcpy(jobjcr->Job, jr->Job);
   jobjcr->JobId = JobId;      /* this is JobId on tape */
   jobjcr->sched_time = jr->SchedTime;
   jobjcr->start_time = jr->StartTime;
   jobjcr->VolSessionId = rec->VolSessionId;
   jobjcr->VolSessionTime = rec->VolSessionTime;
   jobjcr->ClientId = jr->ClientId;
   attach_jcr_to_device(dev, jobjcr);
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
   /*  
    * We are at the end of reading a tape. Now, we simulate handling
    *	the end of writing a tape by wiffling through the attached
    *	jcrs creating jobmedia records.
    */
   Dmsg1(100, "Walk attached jcrs. Volume=%s\n", dev->VolCatInfo.VolCatName);
   for (JCR *mjcr=NULL; (mjcr=next_attached_jcr(dev, mjcr)); ) {
      if (verbose) {
         Pmsg1(000, "create JobMedia for Job %s\n", mjcr->Job);
      }
      mjcr->end_block = dev->block_num;
      mjcr->end_file = dev->file;
      if (!create_jobmedia_record(db, mjcr)) {
         Pmsg2(000, _("Could not create JobMedia record for Volume=%s Job=%s\n"),
	    dev->VolCatInfo.VolCatName, mjcr->Job);
      }
   }

   fprintf(stderr, "Mount Volume %s on device %s and press return when ready: ",
      jcr->VolumeName, dev_name(dev));
   getchar();	
   return 1;
}
