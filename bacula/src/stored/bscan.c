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
static void do_scan(void);
static void record_cb(JCR *jcr, DEVICE *dev, DEV_BLOCK *block, DEV_RECORD *rec);
static int  create_file_attributes_record(B_DB *db, JCR *mjcr, 
			       char *fname, char *lname, int type,
			       char *ap, DEV_RECORD *rec);
static int  create_media_record(B_DB *db, MEDIA_DBR *mr, VOLUME_LABEL *vl);
static int  update_media_record(B_DB *db, MEDIA_DBR *mr);
static int  create_pool_record(B_DB *db, POOL_DBR *pr);
static JCR *create_job_record(B_DB *db, JOB_DBR *mr, SESSION_LABEL *label, DEV_RECORD *rec);
static int  update_job_record(B_DB *db, JOB_DBR *mr, SESSION_LABEL *elabel, 
			      DEV_RECORD *rec);
static int  create_client_record(B_DB *db, CLIENT_DBR *cr);
static int  create_fileset_record(B_DB *db, FILESET_DBR *fsr);
static int  create_jobmedia_record(B_DB *db, JCR *jcr);
static JCR *create_jcr(JOB_DBR *jr, DEV_RECORD *rec, uint32_t JobId);
static int update_MD5_record(B_DB *db, char *MD5buf, DEV_RECORD *rec);


/* Global variables */
static STORES *me;
static DEVICE *dev = NULL;
static B_DB *db;
static JCR *bjcr;		      /* jcr for bscan */
static BSR *bsr = NULL;
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
static FILE_DBR fr;
static SESSION_LABEL label;
static SESSION_LABEL elabel;

static time_t lasttime = 0;

static char *db_name = "bacula";
static char *db_user = "bacula";
static char *db_password = "";
static char *wd = NULL;
static int verbose = 0;
static int update_db = 0;
static int update_vol_info = 0;
static int list_records = 0;
static int ignored_msgs = 0;

#define CONFIG_FILE "bacula-sd.conf"
char *configfile;



static void usage()
{
   fprintf(stderr, _(
"\nVersion: " VERSION " (" DATE ")\n\n"
"Usage: bscan [-d debug_level] <bacula-archive>\n"
"       -b bootstrap      specify a bootstrap file\n"
"       -c <file>         specify configuration file\n"
"       -dnn              set debug level to nn\n"
"       -m                update media info in database\n"
"       -n name           specify the database name (default bacula)\n"
"       -u user           specify database user name (default bacula)\n"
"       -p password       specify database password (default none)\n"
"       -r                list records\n"
"       -s                synchronize or store in database\n"
"       -v                verbose\n"
"       -w dir            specify working directory (default from conf file)\n"
"       -?                print this message\n\n"));
   exit(1);
}

int main (int argc, char *argv[])
{
   int ch;
   struct stat stat_buf;

   my_name_is(argc, argv, "bscan");
   init_msg(NULL, NULL);


   while ((ch = getopt(argc, argv, "b:c:d:mn:p:rsu:vw:?")) != -1) {
      switch (ch) {
         case 'b':
	    bsr = parse_bsr(NULL, optarg);
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

         case 'm':
	    update_vol_info = 1;
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

         case 'r':
	    list_records = 1;
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

   if (configfile == NULL) {
      configfile = bstrdup(CONFIG_FILE);
   }

   parse_config(configfile);
   LockRes();
   me = (STORES *)GetNextRes(R_STORAGE, NULL);
   if (!me) {
      UnlockRes();
      Emsg1(M_ERROR_TERM, 0, _("No Storage resource defined in %s. Cannot continue.\n"), 
	 configfile);
   }
   UnlockRes();
   /* Check if -w option given, otherwise use resource for working directory */
   if (wd) { 
      working_directory = wd;
   } else if (!me->working_directory) {
      Emsg1(M_ERROR_TERM, 0, _("No Working Directory defined in %s. Cannot continue.\n"),
	 configfile);
   } else {
      working_directory = me->working_directory;
   }

   /* Check that working directory is good */
   if (stat(working_directory, &stat_buf) != 0) {
      Emsg1(M_ERROR_TERM, 0, _("Working Directory: %s not found. Cannot continue.\n"),
	 working_directory);
   }
   if (!S_ISDIR(stat_buf.st_mode)) {
      Emsg1(M_ERROR_TERM, 0, _("Working Directory: %s is not a directory. Cannot continue.\n"),
	 working_directory);
   }

   bjcr = setup_jcr("bscan", argv[0], bsr);
   dev = setup_to_access_device(bjcr, 1);   /* read device */
   if (!dev) { 
      exit(1);
   }

   if ((db=db_init_database(NULL, db_name, db_user, db_password)) == NULL) {
      Emsg0(M_ERROR_TERM, 0, _("Could not init Bacula database\n"));
   }
   if (!db_open_database(NULL, db)) {
      Emsg0(M_ERROR_TERM, 0, db_strerror(db));
   }
   Dmsg0(200, "Database opened\n");
   if (verbose) {
      Pmsg2(000, _("Using Database: %s, User: %s\n"), db_name, db_user);
   }

   do_scan();

   free_jcr(bjcr);
   return 0;
}
  

static void do_scan()		  
{
   fname = get_pool_memory(PM_FNAME);
   ofile = get_pool_memory(PM_FNAME);
   lname = get_pool_memory(PM_FNAME);

   memset(&ar, 0, sizeof(ar));
   memset(&pr, 0, sizeof(pr));
   memset(&jr, 0, sizeof(jr));
   memset(&cr, 0, sizeof(cr));
   memset(&fsr, 0, sizeof(fsr));
   memset(&fr, 0, sizeof(fr));

   detach_jcr_from_device(dev, bjcr);

   read_records(bjcr, dev, record_cb, mount_next_read_volume);
   release_device(bjcr, dev);

   free_pool_memory(fname);
   free_pool_memory(ofile);
   free_pool_memory(lname);
   term_dev(dev);
}

static void record_cb(JCR *bjcr, DEVICE *dev, DEV_BLOCK *block, DEV_RECORD *rec)
{
   JCR *mjcr;
   char ec1[30];

   if (rec->data_len > 0) {
      mr.VolBytes += rec->data_len + WRITE_RECHDR_LENGTH; /* Accumulate Volume bytes */
   }
   if (list_records) {
      Pmsg5(000, _("Record: SessId=%u SessTim=%u FileIndex=%d Stream=%d len=%u\n"),
	    rec->VolSessionId, rec->VolSessionTime, rec->FileIndex, 
	    rec->Stream, rec->data_len);
   }
   /* 
    * Check for Start or End of Session Record 
    *
    */
   if (rec->FileIndex < 0) {
      int save_update_db = update_db;

      if (verbose > 1) {
	 dump_label_record(dev, rec, 1);
      }
      switch (rec->FileIndex) {
	 case PRE_LABEL:
            Pmsg0(000, _("Volume is prelabeled. This tape cannot be scanned.\n"));
	    return;
	    break;
	 case VOL_LABEL:
	    unser_volume_label(dev, rec);
	    /* Check Pool info */
	    strcpy(pr.Name, dev->VolHdr.PoolName);
	    strcpy(pr.PoolType, dev->VolHdr.PoolType);
	    if (db_get_pool_record(bjcr, db, &pr)) {
	       if (verbose) {
                  Pmsg1(000, _("Pool record for %s found in DB.\n"), pr.Name);
	       }
	    } else {
	       if (!update_db) {
                  Pmsg1(000, _("VOL_LABEL: Pool record not found for Pool: %s\n"),
		     pr.Name);
	       }
	       create_pool_record(db, &pr);
	    }
	    if (strcmp(pr.PoolType, dev->VolHdr.PoolType) != 0) {
               Pmsg2(000, _("VOL_LABEL: PoolType mismatch. DB=%s Vol=%s\n"),
		  pr.PoolType, dev->VolHdr.PoolType);
	       return;
	    } else if (verbose) {
               Pmsg1(000, _("Pool type \"%s\" is OK.\n"), pr.PoolType);
	    }

	    /* Check Media Info */
	    memset(&mr, 0, sizeof(mr));
	    strcpy(mr.VolumeName, dev->VolHdr.VolName);
	    mr.PoolId = pr.PoolId;
	    if (db_get_media_record(bjcr, db, &mr)) {
	       if (verbose) {
                  Pmsg1(000, _("Media record for %s found in DB.\n"), mr.VolumeName);
	       }
	       /* Clear out some volume statistics that will be updated */
	       mr.VolJobs = mr.VolFiles = mr.VolBlocks = 0;
	       mr.VolBytes = rec->data_len + 20;
	    } else {
	       if (!update_db) {
                  Pmsg1(000, _("VOL_LABEL: Media record not found for Volume: %s\n"),
		     mr.VolumeName);
	       }
	       strcpy(mr.MediaType, dev->VolHdr.MediaType);
	       create_media_record(db, &mr, &dev->VolHdr);
	    }
	    if (strcmp(mr.MediaType, dev->VolHdr.MediaType) != 0) {
               Pmsg2(000, _("VOL_LABEL: MediaType mismatch. DB=%s Vol=%s\n"),
		  mr.MediaType, dev->VolHdr.MediaType);
	       return;
	    } else if (verbose) {
               Pmsg1(000, _("Media type \"%s\" is OK.\n"), mr.MediaType);
	    }
	    /* Reset some JCR variables */
	    for (mjcr=NULL; (mjcr=next_attached_jcr(dev, mjcr)); ) {
	       mjcr->VolFirstFile = mjcr->FileIndex = 0;
	       mjcr->StartBlock = mjcr->EndBlock = 0;
	       mjcr->StartFile = mjcr->EndFile = 0;
	    }

            Pmsg1(000, _("VOL_LABEL: OK for Volume: %s\n"), mr.VolumeName);
	    break;
	 case SOS_LABEL:
	    mr.VolJobs++;
	    if (ignored_msgs > 0) {
               Pmsg1(000, _("%d \"errors\" ignored before first Start of Session record.\n"), 
		     ignored_msgs);
	       ignored_msgs = 0;
	    }
	    unser_session_label(&label, rec);
	    memset(&jr, 0, sizeof(jr));
	    jr.JobId = label.JobId;
	    if (db_get_job_record(bjcr, db, &jr)) {
	       /* Job record already exists in DB */
               update_db = 0;  /* don't change db in create_job_record */
	       if (verbose) {
                  Pmsg1(000, _("SOS_LABEL: Found Job record for JobId: %d\n"), jr.JobId);
	       }
	    } else {
	       /* Must create a Job record in DB */
	       if (!update_db) {
                  Pmsg1(000, _("SOS_LABEL: Job record not found for JobId: %d\n"),
		     jr.JobId);
	       }
	    }
	    /* Create Client record if not already there */
	       strcpy(cr.Name, label.ClientName);
	       create_client_record(db, &cr);
	       jr.ClientId = cr.ClientId;

            /* process label, if Job record exists don't update db */
	    mjcr = create_job_record(db, &jr, &label, rec);
	    update_db = save_update_db;

	    jr.PoolId = pr.PoolId;
	    /* Set start positions into JCR */
	    if (dev->state & ST_TAPE) {
	       mjcr->StartBlock = dev->block_num;
	       mjcr->StartFile = dev->file;
	    } else {
	       mjcr->StartBlock = (uint32_t)dev->file_addr;
	       mjcr->StartFile = (uint32_t)(dev->file_addr >> 32);
	    }
	    mjcr->start_time = jr.StartTime;
	    mjcr->JobLevel = jr.Level;

	    mjcr->client_name = get_pool_memory(PM_FNAME);
	    pm_strcpy(&mjcr->client_name, label.ClientName);
	    mjcr->pool_type = get_pool_memory(PM_FNAME);
	    pm_strcpy(&mjcr->pool_type, label.PoolType);
	    mjcr->fileset_name = get_pool_memory(PM_FNAME);
	    pm_strcpy(&mjcr->fileset_name, label.FileSetName);
	    mjcr->pool_name = get_pool_memory(PM_FNAME);
	    pm_strcpy(&mjcr->pool_name, label.PoolName);

	    if (rec->VolSessionId != jr.VolSessionId) {
               Pmsg3(000, _("SOS_LABEL: VolSessId mismatch for JobId=%u. DB=%d Vol=%d\n"),
		  jr.JobId,
		  jr.VolSessionId, rec->VolSessionId);
	       return;
	    }
	    if (rec->VolSessionTime != jr.VolSessionTime) {
               Pmsg3(000, _("SOS_LABEL: VolSessTime mismatch for JobId=%u. DB=%d Vol=%d\n"),
		  jr.JobId,
		  jr.VolSessionTime, rec->VolSessionTime);
	       return;
	    }
	    if (jr.PoolId != pr.PoolId) {
               Pmsg3(000, _("SOS_LABEL: PoolId mismatch for JobId=%u. DB=%d Vol=%d\n"),
		  jr.JobId,
		  jr.PoolId, pr.PoolId);
	       return;
	    }
	    break;
	 case EOS_LABEL:
	    unser_session_label(&elabel, rec);

	    /* Create FileSet record */
	    strcpy(fsr.FileSet, label.FileSetName);
	    strcpy(fsr.MD5, label.FileSetMD5);
	    create_fileset_record(db, &fsr);
	    jr.FileSetId = fsr.FileSetId;

	    mjcr = get_jcr_by_session(rec->VolSessionId, rec->VolSessionTime);
	    if (!mjcr) {
               Pmsg2(000, _("Could not find SessId=%d SessTime=%d for EOS record.\n"),
		     rec->VolSessionId, rec->VolSessionTime);
	       break;
	    }

	    /* Do the final update to the Job record */
	    update_job_record(db, &jr, &elabel, rec);

	    mjcr->end_time = jr.EndTime;
	    mjcr->JobStatus = JS_Terminated;

	    /* Create JobMedia record */
	    create_jobmedia_record(db, mjcr);
	    detach_jcr_from_device(dev, mjcr);
	    free_jcr(mjcr);

	    break;
	 case EOM_LABEL:
	    break;
	 case EOT_LABEL:	      /* end of all tapes */
	    /* 
	     * Wiffle through all jobs still open and close
	     *	 them.
	     */
	    if (update_db) {
	       for (mjcr=NULL; (mjcr=next_attached_jcr(dev, mjcr)); ) {
		  jr.JobId = mjcr->JobId;
		  jr.JobStatus = JS_ErrorTerminated;
		  jr.JobFiles = mjcr->JobFiles;
		  jr.JobBytes = mjcr->JobBytes;
		  jr.VolSessionId = mjcr->VolSessionId;
		  jr.VolSessionTime = mjcr->VolSessionTime;
		  jr.JobTDate = (utime_t)mjcr->start_time;
		  jr.ClientId = mjcr->ClientId;
		  free_jcr(mjcr);
		  if (!db_update_job_end_record(bjcr, db, &jr)) {
                     Pmsg1(0, _("Could not update job record. ERR=%s\n"), db_strerror(db));
		  }
	       }
	    }
	    mr.VolFiles = rec->File;
	    mr.VolBlocks = rec->Block;
	    mr.VolBytes += mr.VolBlocks * WRITE_BLKHDR_LENGTH; /* approx. */
	    mr.VolMounts++;
	    update_media_record(db, &mr);
            Pmsg3(0, _("End of Volume. VolFiles=%u VolBlocks=%u VolBytes=%s\n"), mr.VolFiles,
		       mr.VolBlocks, edit_uint64_with_commas(mr.VolBytes, ec1));
	    break;
	 default:
	    break;
      }
      return;
   }


   /* File Attributes stream */
   if (rec->Stream == STREAM_UNIX_ATTRIBUTES || rec->Stream == STREAM_WIN32_ATTRIBUTES) {
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
      mjcr = get_jcr_by_session(rec->VolSessionId, rec->VolSessionTime);
      if (!mjcr) {
	 if (mr.VolJobs > 0) {
            Pmsg2(000, _("Could not find Job SessId=%d SessTime=%d for Attributes record.\n"),
			 rec->VolSessionId, rec->VolSessionTime);
	 } else {
	    ignored_msgs++;
	 }
	 return;
      }
      fr.JobId = mjcr->JobId;
      fr.FileId = 0;
      if (db_get_file_attributes_record(bjcr, db, fname, &fr)) {
	 if (verbose > 1) {
            Pmsg1(000, _("File record already exists for: %s\n"), fname);
	 }
      } else {
	 create_file_attributes_record(db, mjcr, fname, lname, type, ap, rec);
      }
      free_jcr(mjcr);

   /* Data stream */
   } else if (rec->Stream == STREAM_FILE_DATA) {
      mjcr = get_jcr_by_session(rec->VolSessionId, rec->VolSessionTime);
      if (!mjcr) {
	 if (mr.VolJobs > 0) {
            Pmsg2(000, _("Could not find Job SessId=%d SessTime=%d for File Data record.\n"),
			 rec->VolSessionId, rec->VolSessionTime);
	 } else {
	    ignored_msgs++;
	 }
	 return;
      }
      mjcr->JobBytes += rec->data_len;
      free_jcr(mjcr);		      /* done using JCR */

   } else if (rec->Stream == STREAM_SPARSE_DATA) {
      mjcr = get_jcr_by_session(rec->VolSessionId, rec->VolSessionTime);
      if (!mjcr) {
	 if (mr.VolJobs > 0) {
            Pmsg2(000, _("Could not find Job SessId=%d SessTime=%d for Sparse Data record.\n"),
			 rec->VolSessionId, rec->VolSessionTime);
	 } else {
	    ignored_msgs++;
	 }
	 return;
      }
      mjcr->JobBytes += rec->data_len - sizeof(uint64_t);
      free_jcr(mjcr);		      /* done using JCR */

   } else if (rec->Stream == STREAM_GZIP_DATA) {
      mjcr = get_jcr_by_session(rec->VolSessionId, rec->VolSessionTime);
      if (!mjcr) {
	 if (mr.VolJobs > 0) {
            Pmsg2(000, _("Could not find Job SessId=%d SessTime=%d for GZIP Data record.\n"),
			 rec->VolSessionId, rec->VolSessionTime);
	 } else {
	    ignored_msgs++;
	 }
	 return;
      }
      mjcr->JobBytes += rec->data_len; /* No correct, we should expand it */
      free_jcr(mjcr);		      /* done using JCR */

   } else if (rec->Stream == STREAM_SPARSE_GZIP_DATA) {
      mjcr = get_jcr_by_session(rec->VolSessionId, rec->VolSessionTime);
      if (!mjcr) {
	 if (mr.VolJobs > 0) {
            Pmsg2(000, _("Could not find Job SessId=%d SessTime=%d for Sparse GZIP Data record.\n"),
			 rec->VolSessionId, rec->VolSessionTime);
	 } else {
	    ignored_msgs++;
	 }
	 return;
      }
      mjcr->JobBytes += rec->data_len - sizeof(uint64_t); /* No correct, we should expand it */
      free_jcr(mjcr);		      /* done using JCR */


   } else if (rec->Stream == STREAM_MD5_SIGNATURE) {
      char MD5buf[30];
      bin_to_base64(MD5buf, (char *)rec->data, 16); /* encode 16 bytes */
      if (verbose > 1) {
         Pmsg1(000, _("Got MD5 record: %s\n"), MD5buf);
      }
      update_MD5_record(db, MD5buf, rec);

   } else if (rec->Stream == STREAM_PROGRAM_NAMES) {
      if (verbose) {
         Pmsg1(000, _("Got Prog Names Stream: %s\n"), rec->data);
      }
   } else if (rec->Stream == STREAM_PROGRAM_DATA) {
      if (verbose > 1) {
         Pmsg0(000, _("Got Prog Data Stream record.\n"));
      }
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
static int create_file_attributes_record(B_DB *db, JCR *mjcr,
			       char *fname, char *lname, int type,
			       char *ap, DEV_RECORD *rec)
{

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
   mjcr->JobFiles++;

   if (!update_db) {
      return 1;
   }

   if (!db_create_file_attributes_record(bjcr, db, &ar)) {
      Pmsg1(0, _("Could not create File Attributes record. ERR=%s\n"), db_strerror(db));
      return 0;
   }
   mjcr->FileId = ar.FileId;

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

   strcpy(mr->VolStatus, "Full");
   mr->VolRetention = 365 * 3600 * 24; /* 1 year */
   if (vl->VerNum >= 11) {
      mr->FirstWritten = btime_to_utime(vl->write_btime);
      mr->LabelDate    = btime_to_utime(vl->label_btime);
   } else {
      /* DEPRECATED DO NOT USE */
      dt.julian_day_number = vl->write_date;
      dt.julian_day_fraction = vl->write_time;
      tm_decode(&dt, &tm);
      mr->FirstWritten = mktime(&tm);
      dt.julian_day_number = vl->label_date;
      dt.julian_day_fraction = vl->label_time;
      tm_decode(&dt, &tm);
      mr->LabelDate = mktime(&tm);
   }
   lasttime = mr->LabelDate;

   if (!update_db) {
      return 1;
   }

   if (!db_create_media_record(bjcr, db, mr)) {
      Pmsg1(0, _("Could not create media record. ERR=%s\n"), db_strerror(db));
      return 0;
   }
   if (!db_update_media_record(bjcr, db, mr)) {
      Pmsg1(0, _("Could not update media record. ERR=%s\n"), db_strerror(db));
      return 0;
   }
   if (verbose) {
      Pmsg1(000, _("Created Media record for Volume: %s\n"), mr->VolumeName);
   }
   return 1;

}

/*
 * Called at end of media to update it
 */
static int update_media_record(B_DB *db, MEDIA_DBR *mr)
{
   if (!update_db && !update_vol_info) {
      return 1;
   }

   mr->LastWritten = lasttime;
   if (!db_update_media_record(bjcr, db, mr)) {
      Pmsg1(0, _("Could not update media record. ERR=%s\n"), db_strerror(db));
      return 0;
   }
   if (verbose) {
      Pmsg1(000, _("Updated Media record at end of Volume: %s\n"), mr->VolumeName);
   }
   return 1;

}


static int create_pool_record(B_DB *db, POOL_DBR *pr)
{
   pr->NumVols++;
   pr->UseCatalog = 1;
   pr->VolRetention = 355 * 3600 * 24; /* 1 year */

   if (!update_db) {
      return 1;
   }
   if (!db_create_pool_record(bjcr, db, pr)) {
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
   if (!db_create_client_record(bjcr, db, cr)) {
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
   fsr->FileSetId = 0;
   if (fsr->MD5[0] == 0) {
      fsr->MD5[0] = ' ';              /* Equivalent to nothing */
      fsr->MD5[1] = 0;
   }
   if (db_get_fileset_record(bjcr, db, fsr)) {
      if (verbose) {
         Pmsg1(000, _("Fileset \"%s\" already exists.\n"), fsr->FileSet);
      }
   } else {
      if (!db_create_fileset_record(bjcr, db, fsr)) {
         Pmsg2(0, _("Could not create FileSet record \"%s\". ERR=%s\n"), 
	    fsr->FileSet, db_strerror(db));
	 return 0;
      }
      if (verbose) {
         Pmsg1(000, _("Created FileSet record \"%s\"\n"), fsr->FileSet);
      }
   }
   return 1;
}

/*
 * Simulate the two calls on the database to create
 *  the Job record and to update it when the Job actually
 *  begins running.
 */
static JCR *create_job_record(B_DB *db, JOB_DBR *jr, SESSION_LABEL *label, 
			     DEV_RECORD *rec)
{
   JCR *mjcr;
   struct date_time dt;
   struct tm tm;

   jr->JobId = label->JobId;
   jr->Type = label->JobType;
   jr->Level = label->JobLevel;
   jr->JobStatus = JS_Created;
   strcpy(jr->Name, label->JobName);
   strcpy(jr->Job, label->Job);
   if (label->VerNum >= 11) {
      jr->SchedTime = btime_to_unix(label->write_btime);
   } else {
      dt.julian_day_number = label->write_date;
      dt.julian_day_fraction = label->write_time;
      tm_decode(&dt, &tm);
      jr->SchedTime = mktime(&tm);
   }

   jr->StartTime = jr->SchedTime;
   jr->JobTDate = (utime_t)jr->SchedTime;
   jr->VolSessionId = rec->VolSessionId;
   jr->VolSessionTime = rec->VolSessionTime;

   /* Now create a JCR as if starting the Job */
   mjcr = create_jcr(jr, rec, label->JobId);

   if (!update_db) {
      return mjcr;
   }

   /* This creates the bare essentials */
   if (!db_create_job_record(bjcr, db, jr)) {
      Pmsg1(0, _("Could not create JobId record. ERR=%s\n"), db_strerror(db));
      return mjcr;
   }

   /* This adds the client, StartTime, JobTDate, ... */
   if (!db_update_job_start_record(bjcr, db, jr)) {
      Pmsg1(0, _("Could not update job start record. ERR=%s\n"), db_strerror(db));
      return mjcr;
   }
   Pmsg2(000, _("Created new JobId=%u record for original JobId=%u\n"), jr->JobId, 
	 label->JobId);
   mjcr->JobId = jr->JobId;	      /* set new JobId */
   return mjcr;
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

   mjcr = get_jcr_by_session(rec->VolSessionId, rec->VolSessionTime);
   if (!mjcr) {
      Pmsg2(000, _("Could not find SessId=%d SessTime=%d for EOS record.\n"),
		   rec->VolSessionId, rec->VolSessionTime);
      return 0;
   }
   if (elabel->VerNum >= 11) {
      jr->EndTime = btime_to_unix(elabel->write_btime);
   } else {
      dt.julian_day_number = elabel->write_date;
      dt.julian_day_fraction = elabel->write_time;
      tm_decode(&dt, &tm);
      jr->EndTime = mktime(&tm);
   }
   lasttime = jr->EndTime;
   mjcr->end_time = jr->EndTime;

   jr->JobId = mjcr->JobId;
   jr->JobStatus = elabel->JobStatus;
   mjcr->JobStatus = elabel->JobStatus;
   jr->JobFiles = elabel->JobFiles;
   jr->JobBytes = elabel->JobBytes;
   jr->VolSessionId = rec->VolSessionId;
   jr->VolSessionTime = rec->VolSessionTime;
   jr->JobTDate = (utime_t)mjcr->start_time;
   jr->ClientId = mjcr->ClientId;

   if (!update_db) {
      free_jcr(mjcr);
      return 1;
   }
   
   if (!db_update_job_end_record(bjcr, db, jr)) {
      Pmsg2(0, _("Could not update JobId=%u record. ERR=%s\n"), jr->JobId,  db_strerror(db));
      free_jcr(mjcr);
      return 0;
   }
   if (verbose) {
      Pmsg1(000, _("Updated Job termination record for new JobId=%u\n"), jr->JobId);
   }
   if (verbose > 1) {
      char *term_msg;
      static char term_code[70];
      char sdt[50], edt[50];
      char ec1[30], ec2[30], ec3[30];

      switch (mjcr->JobStatus) {
      case JS_Terminated:
         term_msg = _("Backup OK");
	 break;
      case JS_FatalError:
      case JS_ErrorTerminated:
         term_msg = _("*** Backup Error ***");
	 break;
      case JS_Cancelled:
         term_msg = _("Backup Cancelled");
	 break;
      default:
	 term_msg = term_code;
         sprintf(term_code, _("Job Termination code: %d"), mjcr->JobStatus);
	 break;
      }
      bstrftime(sdt, sizeof(sdt), mjcr->start_time);
      bstrftime(edt, sizeof(edt), mjcr->end_time);
      Pmsg14(000,  _("%s\n\
JobId:                  %d\n\
Job:                    %s\n\
FileSet:                %s\n\
Backup Level:           %s\n\
Client:                 %s\n\
Start time:             %s\n\
End time:               %s\n\
Files Written:          %s\n\
Bytes Written:          %s\n\
Volume Session Id:      %d\n\
Volume Session Time:    %d\n\
Last Volume Bytes:      %s\n\
Termination:            %s\n\n"),
	edt,
	mjcr->JobId,
	mjcr->Job,
	mjcr->fileset_name,
	job_level_to_str(mjcr->JobLevel),
	mjcr->client_name,
	sdt,
	edt,
	edit_uint64_with_commas(mjcr->JobFiles, ec1),
	edit_uint64_with_commas(mjcr->JobBytes, ec2),
	mjcr->VolSessionId,
	mjcr->VolSessionTime,
	edit_uint64_with_commas(mr.VolBytes, ec3),
	term_msg);
   }
   free_jcr(mjcr);
   return 1;
}

static int create_jobmedia_record(B_DB *db, JCR *mjcr)
{
   JOBMEDIA_DBR jmr;

   if (dev->state & ST_TAPE) {
      mjcr->EndBlock = dev->EndBlock;
      mjcr->EndFile  = dev->EndFile;
   } else {
      mjcr->EndBlock = (uint32_t)dev->file_addr;
      mjcr->EndFile = (uint32_t)(dev->file_addr >> 32);
   }

   memset(&jmr, 0, sizeof(jmr));
   jmr.JobId = mjcr->JobId;
   jmr.MediaId = mr.MediaId;
   jmr.FirstIndex = mjcr->VolFirstFile;
   jmr.LastIndex = mjcr->FileIndex;
   jmr.StartFile = mjcr->StartFile;
   jmr.EndFile = mjcr->EndFile;
   jmr.StartBlock = mjcr->StartBlock;
   jmr.EndBlock = mjcr->EndBlock;


   if (!update_db) {
      return 1;
   }

   if (!db_create_jobmedia_record(bjcr, db, &jmr)) {
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
 * Simulate the database call that updates the MD5 record
 */
static int update_MD5_record(B_DB *db, char *MD5buf, DEV_RECORD *rec)
{
   JCR *mjcr;

   mjcr = get_jcr_by_session(rec->VolSessionId, rec->VolSessionTime);
   if (!mjcr) {
      if (mr.VolJobs > 0) {
         Pmsg2(000, _("Could not find SessId=%d SessTime=%d for MD5 record.\n"),
		      rec->VolSessionId, rec->VolSessionTime);
      } else {
	 ignored_msgs++;
      }
      return 0;
   }

   if (!update_db) {
      free_jcr(mjcr);
      return 1;
   }
   
   if (!db_add_MD5_to_file_record(bjcr, db, mjcr->FileId, MD5buf)) {
      Pmsg1(0, _("Could not add MD5 to File record. ERR=%s\n"), db_strerror(db));
      free_jcr(mjcr);
      return 0;
   }
   if (verbose > 1) {
      Pmsg0(000, _("Updated MD5 record\n"));
   }
   free_jcr(mjcr);
   return 1;
}


/* 
 * Create a JCR as if we are really starting the job
 */
static JCR *create_jcr(JOB_DBR *jr, DEV_RECORD *rec, uint32_t JobId)
{
   JCR *jobjcr;
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
   return jobjcr;
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
         Pmsg1(000, _("Create JobMedia for Job %s\n"), mjcr->Job);
      }
      if (dev->state & ST_TAPE) {
	 mjcr->EndBlock = dev->EndBlock;
	 mjcr->EndFile = dev->EndFile;
      } else {
	 mjcr->EndBlock = (uint32_t)dev->file_addr;
	 mjcr->StartBlock = (uint32_t)(dev->file_addr >> 32);
      }
      if (!create_jobmedia_record(db, mjcr)) {
         Pmsg2(000, _("Could not create JobMedia record for Volume=%s Job=%s\n"),
	    dev->VolCatInfo.VolCatName, mjcr->Job);
      }
   }

   fprintf(stderr, _("Mount Volume %s on device %s and press return when ready: "),
      jcr->VolumeName, dev_name(dev));
   getchar();	
   return 1;
}
