/*
 *
 *  Program to copy a Bacula from one volume to another.
 *
 *   Kern E. Sibbald, October 2002
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

/* Forward referenced functions */
static void do_scan(void);
static void record_cb(JCR *jcr, DEVICE *dev, DEV_BLOCK *block, DEV_RECORD *rec);


/* Global variables */
static DEVICE *dev = NULL;
static B_DB *db;
static JCR *bjcr;		      /* jcr for bscan */
static BSR *bsr = NULL;
static char *wd = "/tmp";
static int verbose = 0;

#define CONFIG_FILE "bacula-sd.conf"
char *configfile;


static void usage()
{
   fprintf(stderr, _(
"\nVersion: " VERSION " (" DATE ")\n\n"
"Usage: bcopy [-d debug_level] <input-archive> <output-archive>\n"
"       -b bootstrap      specify a bootstrap file\n"
"       -c <file>         specify configuration file\n"
"       -dnn              set debug level to nn\n"
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

   fprintf(stderr, "\n\nPlease don't use this program.\n\
It is currently under development and does not work.\n\n");

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

   if (argc != 2) {
      Pmsg0(0, _("Wrong number of arguments: \n"));
      usage();
   }

   working_directory = wd;

   if (configfile == NULL) {
      configfile = bstrdup(CONFIG_FILE);
   }

   parse_config(configfile);

   bjcr = setup_jcr("bcopy", argv[0], bsr);
   dev = setup_to_access_device(bjcr, 0);   /* read device */
   if (!dev) { 
      exit(1);
   }

   do_copy();

   free_jcr(bjcr);
   return 0;
}
  

static void do_copy()		  
{
   detach_jcr_from_device(dev, bjcr);

   read_records(bjcr, dev, record_cb, mount_next_read_volume);
   release_device(bjcr, dev);

   term_dev(dev);
}

static void record_cb(JCR *bjcr, DEVICE *dev, DEV_BLOCK *block, DEV_RECORD *rec)
{
   JCR *mjcr;
   char ec1[30];

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

      if (verbose > 1) {
	 dump_label_record(dev, rec, 1);
      }
      switch (rec->FileIndex) {
	 case PRE_LABEL:
            Pmsg0(000, "Volume is prelabeled. This volume cannot be copied.\n");
	    return;
	    break;
	 case VOL_LABEL:
            Pmsg1(000, "VOL_LABEL: OK for Volume: %s\n", mr.VolumeName);
	    break;
	 case SOS_LABEL:
	    break;
	 case EOS_LABEL:
	    break;
	 case EOM_LABEL:
	    break;
	 case EOT_LABEL:	      /* end of all tapes */
	    break;
	 default:
	    break;
      }
      return;
   }

   /*  Write record */

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
         Pmsg1(000, "create JobMedia for Job %s\n", mjcr->Job);
      }
      if (dev->state & ST_TAPE) {
	 mjcr->EndBlock = dev->block_num;
	 mjcr->EndFile = dev->file;
      } else {
	 mjcr->EndBlock = (uint32_t)dev->file_addr;
	 mjcr->StartBlock = (uint32_t)(dev->file_addr >> 32);
      }
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
