/*
 *
 *   Bacula Director -- verify.c -- responsible for running file verification
 *
 *     Kern Sibbald, October MM
 *
 *    This routine is run as a separate thread.  There may be more
 *    work to be done to make it totally reentrant!!!!
 * 
 * Current implementation is Catalog verification only (i.e. no
 *  verification versus tape).
 *
 *  Basic tasks done here:
 *     Open DB
 *     Open connection with File daemon and pass him commands
 *	 to do the verify.
 *     When the File daemon sends the attributes, compare them to
 *	 what is in the DB.
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
#include "dird.h"

/* Imported Global Variables */
extern int debug_level;

/* Commands sent to File daemon */
static char verifycmd[]   = "verify";
static char levelcmd[]    = "level = %s%s\n";

/* Responses received from File daemon */
static char OKverify[]   = "2000 OK verify\n";
static char OKlevel[]    = "2000 OK level\n";

/* Forward referenced functions */
static void verify_cleanup(JCR *jcr, int TermCode);
static void prt_fname(JCR *jcr);
static int missing_handler(void *ctx, int num_fields, char **row);

/* 
 * Do a verification of the specified files
 *    
 *  Returns:  0 on failure
 *	      1 on success
 */
int do_verify(JCR *jcr) 
{
   char *level;
   BSOCK   *fd;
   JOB_DBR jr;
   int last_full_id;

   if (!get_or_create_client_record(jcr)) {
      verify_cleanup(jcr, JS_ErrorTerminated);			  
      return 0;
   }

   Dmsg1(9, "bdird: created client %s record\n", jcr->client->hdr.name);

   /* If we are doing a verify from the catalog,
    * we must look up the time and date of the
    * last full verify.
    */
   if (jcr->level == L_VERIFY_CATALOG) {
      memcpy(&jr, &(jcr->jr), sizeof(jr));
      if (!db_find_last_full_verify(jcr->db, &jr)) {
         Jmsg(jcr, M_FATAL, 0, _("Unable to find last full verify. %s"),
	    db_strerror(jcr->db));
	 verify_cleanup(jcr, JS_ErrorTerminated);		     
	 return 0;
      }
      last_full_id = jr.JobId;
      Dmsg1(20, "Last full id=%d\n", last_full_id);
   }

   jcr->jr.JobId = jcr->JobId;
   jcr->jr.StartTime = jcr->start_time;
   jcr->jr.Level = jcr->level;
   if (!db_update_job_start_record(jcr->db, &jcr->jr)) {
      Jmsg(jcr, M_ERROR, 0, "%s", db_strerror(jcr->db));
      verify_cleanup(jcr, JS_ErrorTerminated);			  
      return 0;
   }

   if (!jcr->fname) {
      jcr->fname = (char *) get_pool_memory(PM_FNAME);
   }

   jcr->jr.JobId = last_full_id;      /* save last full id */

   /* Print Job Start message */
   Jmsg(jcr, M_INFO, 0, _("Start Verify JobId %d Job=%s\n"),
      jcr->JobId, jcr->Job);

   if (jcr->level == L_VERIFY_CATALOG) {
      memset(&jr, 0, sizeof(jr));
      jr.JobId = last_full_id;
      if (!db_get_job_record(jcr->db, &jr)) {
         Jmsg(jcr, M_ERROR, 0, _("Could not get job record. %s"), db_strerror(jcr->db));
	 verify_cleanup(jcr, JS_ErrorTerminated);		     
	 return 0;
      }
      Jmsg(jcr, M_INFO, 0, _("Verifying against Init JobId %d run %s\n"),
	 last_full_id, jr.cStartTime); 
   }

   /*
    * OK, now connect to the File daemon
    *  and ask him for the files.
    */
   jcr->sd_auth_key = bstrdup("dummy");    /* dummy Storage daemon key */
   if (!connect_to_file_daemon(jcr, 10, FDConnectTimeout, 1)) {
      verify_cleanup(jcr, JS_ErrorTerminated);			  
      return 0;
   }

   fd = jcr->file_bsock;

   Dmsg0(30, ">filed: Send include list\n");
   if (!send_include_list(jcr)) {
      verify_cleanup(jcr, JS_ErrorTerminated);			  
      return 0;
   }

   Dmsg0(30, ">filed: Send exclude list\n");
   if (!send_exclude_list(jcr)) {
      verify_cleanup(jcr, JS_ErrorTerminated);			  
      return 0;
   }

   /* 
    * Send Level command to File daemon
    *
    */
   switch (jcr->level) {
      case L_VERIFY_INIT:
         level = "init";
	 break;
      case L_VERIFY_CATALOG:
         level = "catalog";
	 break;
      case L_VERIFY_VOLUME:
         level = "volume";
	 break;
      case L_VERIFY_DATA:
         level = "data";
	 break;
      default:
         Emsg1(M_FATAL, 0, _("Unimplemented save level %d\n"), jcr->level);
	 verify_cleanup(jcr, JS_ErrorTerminated);		     
	 return 0;
   }
   Dmsg1(20, ">filed: %s", fd->msg);
   bnet_fsend(fd, levelcmd, level, " ");
   if (!response(fd, OKlevel, "Level")) {
      verify_cleanup(jcr, JS_ErrorTerminated);			  
      return 0;
   }

   /* 
    * Send verify command to File daemon
    */
   bnet_fsend(fd, verifycmd);
   if (!response(fd, OKverify, "Verify")) {
      verify_cleanup(jcr, JS_ErrorTerminated);			  
      return 0;
   }

   /*
    * Now get data back from File daemon and
    *  compare it to the catalog or store it in the
    *  catalog depending on the run type.
    */
   /* Compare to catalog */
   if (jcr->level == L_VERIFY_CATALOG) {
      Dmsg0(10, "Verify level=catalog\n");
      get_attributes_and_compare_to_catalog(jcr, last_full_id);

   /* Build catalog */
   } else if (jcr->level == L_VERIFY_INIT) {
      Dmsg0(10, "Verify level=init\n");
      get_attributes_and_put_in_catalog(jcr);

   } else {
      Emsg1(M_FATAL, 0, _("Unimplemented save level %d\n"), jcr->level);
      verify_cleanup(jcr, JS_ErrorTerminated);			  
      return 0;
   }

   verify_cleanup(jcr, JS_Terminated);
   return 1;
}

/*
 * Release resources allocated during backup.
 *
 */
static void verify_cleanup(JCR *jcr, int TermCode)
{
   char sdt[50], edt[50];
   char ec1[30];
   char term_code[100];
   char *term_msg;
   int msg_type;
   int last_full_id;

   Dmsg0(100, "Enter verify_cleanup()\n");

   last_full_id = jcr->jr.JobId;
   jcr->JobStatus = TermCode;

   update_job_end_record(jcr);

   msg_type = M_INFO;		      /* by default INFO message */
   switch (TermCode) {
      case JS_Terminated:
         term_msg = _("Verify OK");
	 break;
      case JS_ErrorTerminated:
         term_msg = _("*** Verify Error ***"); 
	 msg_type = M_ERROR;	      /* Generate error message */
	 break;
      case JS_Cancelled:
         term_msg = _("Verify Cancelled");
	 break;
      case JS_Differences:
         term_msg = _("Verify Differences");
	 break;
      default:
	 term_msg = term_code;
         sprintf(term_code, _("Inappropriate term code: %c\n"), TermCode);
	 break;
   }
   bstrftime(sdt, sizeof(sdt), jcr->jr.StartTime);
   bstrftime(edt, sizeof(edt), jcr->jr.EndTime);

   Jmsg(jcr, msg_type, 0, _("%s\n\
JobId:                  %d\n\
Job:                    %s\n\
FileSet:                %s\n\
Verify Level:           %s\n\
Client:                 %s\n\
Start time:             %s\n\
End time:               %s\n\
Files Examined:         %s\n\
Termination:            %s\n\n"),
	edt,
	jcr->jr.JobId,
	jcr->jr.Job,
	jcr->fileset->hdr.name,
	level_to_str(jcr->level),
	jcr->client->hdr.name,
	sdt,
	edt,
	edit_uint64_with_commas(jcr->jr.JobFiles, ec1),
	term_msg);

   Dmsg0(100, "Leave verify_cleanup()\n");
   if (jcr->fname) {
      free_memory(jcr->fname);
      jcr->fname = NULL;
   }
}

/*
 * This routine is called only during a Verify
 */
int get_attributes_and_compare_to_catalog(JCR *jcr, int last_full_id)
{
   BSOCK   *fd;
   int n, len;
   FILE_DBR fdbr;
   struct stat statf;		      /* file stat */
   struct stat statc;		      /* catalog stat */
   int stat = JS_Terminated;
   char buf[MAXSTRING];
   char *fname = (char *)get_pool_memory(PM_MESSAGE);
   int do_MD5 = FALSE;

   memset(&fdbr, 0, sizeof(FILE_DBR));
   fd = jcr->file_bsock;
   fdbr.JobId = last_full_id;
   
   Dmsg0(20, "bdird: waiting to receive file attributes\n");
   /*
    * Get Attributes and MD5 Signature from File daemon
    */
   while ((n=bget_msg(fd, 0)) > 0) {
      long file_index, attr_file_index;
      int stream;
      char *attr, *p;
      char Opts_MD5[MAXSTRING];        /* Verify Opts or MD5 signature */

      fname = (char *)check_pool_memory_size(fname, fd->msglen);
      jcr->fname = (char *)check_pool_memory_size(jcr->fname, fd->msglen);
      Dmsg1(50, "Atts+MD5=%s\n", fd->msg);
      if ((len = sscanf(fd->msg, "%ld %d %100s %s", &file_index, &stream, 
	    Opts_MD5, fname)) != 4) {
         Jmsg3(jcr, M_FATAL, 0, _("bird<filed: bad attributes, expected 4 fields got %d\n\
 mslen=%d msg=%s\n"), len, fd->msglen, fd->msg);
	 jcr->JobStatus = JS_ErrorTerminated;
	 return 0;
      }
      /*
       * Got attributes stream, decode it
       */
      if (stream == STREAM_UNIX_ATTRIBUTES) {
	 attr_file_index = file_index;	  /* remember attribute file_index */
	 len = strlen(fd->msg);
	 attr = &fd->msg[len+1];
	 decode_stat(attr, &statf);  /* decode file stat packet */
	 do_MD5 = FALSE;
	 jcr->fn_printed = FALSE;
	 strip_trailing_junk(fname);
	 strcpy(jcr->fname, fname);  /* move filename into JCR */

         Dmsg2(040, "dird<filed: stream=%d %s\n", stream, jcr->fname);
         Dmsg1(20, "dird<filed: attr=%s\n", attr);

	 /* 
	  * Find equivalent record in the database 
	  */
	 fdbr.FileId = 0;
	 if (!db_get_file_attributes_record(jcr->db, jcr->fname, &fdbr)) {
            Jmsg(jcr, M_INFO, 0, _("New file: %s\n"), jcr->fname);
            Dmsg1(020, _("File not in catalog: %s\n"), jcr->fname);
	    stat = JS_Differences;
	    continue;
	 } else {
	    /* 
	     * mark file record as visited by stuffing the
	     * current JobId, which is unique, into the FileIndex
	     */
	    db_mark_file_record(jcr->db, fdbr.FileId, jcr->JobId);
	 }

         Dmsg3(100, "Found %s in catalog. inx=%d Opts=%s\n", jcr->fname, 
	    file_index, Opts_MD5);
	 decode_stat(fdbr.LStat, &statc); /* decode catalog stat */
	 /*
	  * Loop over options supplied by user and verify the
	  * fields he requests.
	  */
	 for (p=Opts_MD5; *p; p++) {
	    switch (*p) {
            case 'i':                /* compare INODEs */
	       if (statc.st_ino != statf.st_ino) {
		  prt_fname(jcr);
                  Jmsg(jcr, M_INFO, 0, _("      st_ino   differ. Cat: %x File: %x\n"), 
		     statc.st_ino, statf.st_ino);
		  stat = JS_Differences;
	       }
	       break;
            case 'p':                /* permissions bits */
	       if (statc.st_mode != statf.st_mode) {
		  prt_fname(jcr);
                  Jmsg(jcr, M_INFO, 0, _("      st_mode  differ. Cat: %x File: %x\n"), 
		     statc.st_mode, statf.st_mode);
		  stat = JS_Differences;
	       }
	       break;
            case 'n':                /* number of links */
	       if (statc.st_nlink != statf.st_nlink) {
		  prt_fname(jcr);
                  Jmsg(jcr, M_INFO, 0, _("      st_nlink differ. Cat: %d File: %d\n"), 
		     statc.st_nlink, statf.st_nlink);
		  stat = JS_Differences;
	       }
	       break;
            case 'u':                /* user id */
	       if (statc.st_uid != statf.st_uid) {
		  prt_fname(jcr);
                  Jmsg(jcr, M_INFO, 0, _("      st_uid   differ. Cat: %d File: %d\n"), 
		     statc.st_uid, statf.st_uid);
		  stat = JS_Differences;
	       }
	       break;
            case 'g':                /* group id */
	       if (statc.st_gid != statf.st_gid) {
		  prt_fname(jcr);
                  Jmsg(jcr, M_INFO, 0, _("      st_gid   differ. Cat: %d File: %d\n"), 
		     statc.st_gid, statf.st_gid);
		  stat = JS_Differences;
	       }
	       break;
            case 's':                /* size */
	       if (statc.st_size != statf.st_size) {
		  prt_fname(jcr);
                  Jmsg(jcr, M_INFO, 0, _("      st_size  differ. Cat: %d File: %d\n"), 
		     statc.st_size, statf.st_size);
		  stat = JS_Differences;
	       }
	       break;
            case 'a':                /* access time */
	       if (statc.st_atime != statf.st_atime) {
		  prt_fname(jcr);
                  Jmsg(jcr, M_INFO, 0, _("      st_atime differs\n"));
		  stat = JS_Differences;
	       }
	       break;
            case 'm':
	       if (statc.st_mtime != statf.st_mtime) {
		  prt_fname(jcr);
                  Jmsg(jcr, M_INFO, 0, _("      st_mtime differs\n"));
		  stat = JS_Differences;
	       }
	       break;
            case 'c':                /* ctime */
	       if (statc.st_ctime != statf.st_ctime) {
		  prt_fname(jcr);
                  Jmsg(jcr, M_INFO, 0, _("      st_ctime differs\n"));
		  stat = JS_Differences;
	       }
	       break;
            case 'd':                /* file size decrease */
	       if (statc.st_size > statf.st_size) {
		  prt_fname(jcr);
                  Jmsg(jcr, M_INFO, 0, _("      st_size  decrease. Cat: %d File: %d\n"), 
		     statc.st_size, statf.st_size);
		  stat = JS_Differences;
	       }
	       break;
            case '5':                /* compare MD5 */
               Dmsg1(500, "set Do_MD5 for %s\n", jcr->fname);
	       do_MD5 = TRUE;
	       break;
            case ':':
            case 'V':
	    default:
	       break;
	    }
	 }
      /*
       * Got MD5 Signature from Storage daemon
       *  It came across in the Opts_MD5 field.
       */
      } else if (stream == STREAM_MD5_SIGNATURE) {
         Dmsg2(100, "stream=MD5 inx=%d fname=%s\n", file_index, jcr->fname);
	 /* 
	  * When ever we get an MD5 signature is MUST have been
	  * preceded by an attributes record, which sets attr_file_index
	  */
	 if (attr_file_index != file_index) {
            Jmsg2(jcr, M_FATAL, 0, _("MD5 index %d not same as attributes %d\n"),
	       file_index, attr_file_index);
	    jcr->JobStatus = JS_ErrorTerminated;
	    return 0;
	 } 
	 if (do_MD5) {
	    db_escape_string(buf, Opts_MD5, strlen(Opts_MD5));
	    if (strcmp(buf, fdbr.MD5) != 0) {
	       prt_fname(jcr);
	       if (debug_level >= 10) {
                  Jmsg(jcr, M_INFO, 0, _("      MD5 not same. File=%s Cat=%s\n"), buf, fdbr.MD5);
	       } else {
                  Jmsg(jcr, M_INFO, 0, _("      MD5 differs.\n"));
	       }
	       stat = JS_Differences;
	    }
	    do_MD5 = FALSE;
	 }
      }
      jcr->jr.JobFiles = file_index;
   } 
   if (n < 0) {
      Jmsg2(jcr, M_FATAL, 0, _("bdird<filed: bad attributes from filed n=%d : %s\n"),
			n, strerror(errno));
      jcr->JobStatus = JS_ErrorTerminated;
      return 0;
   }

   /* Now find all the files that are missing -- i.e. all files in
    *  the database where the FileIndex != current JobId
    */
   jcr->fn_printed = FALSE;
   sprintf(buf, 
"SELECT Path.Path,Filename.Name FROM File,Path,Filename "
"WHERE File.JobId=%d "
"AND File.FileIndex!=%d AND File.PathId=Path.PathId "
"AND File.FilenameId=Filename.FilenameId", 
      last_full_id, jcr->JobId);
   /* missing_handler is called for each file found */
   db_sql_query(jcr->db, buf, missing_handler, (void *)jcr);
   if (jcr->fn_printed) {
      stat = JS_Differences;
   }
   jcr->JobStatus = stat;
   return 1;
}

/*
 * We are called here for each record that matches the above
 *  SQL query -- that is for each file contained in the Catalog
 *  that was not marked earlier. This means that the file in
 *  question is a missing file (in the Catalog but on on Disk).
 */
static int missing_handler(void *ctx, int num_fields, char **row)
{
   JCR *jcr = (JCR *)ctx;

   if (!jcr->fn_printed) {
      Jmsg(jcr, M_INFO, 0, "\n");
      Jmsg(jcr, M_INFO, 0, _("The following files are missing:\n"));
      jcr->fn_printed = TRUE;
   }
   Jmsg(jcr, M_INFO, 0, "      %s%s\n", row[0]?row[0]:"", row[1]?row[1]:"");
   return 0;
}


/* 
 * Print filename for verify
 */
static void prt_fname(JCR *jcr)
{
   if (!jcr->fn_printed) {
      Jmsg(jcr, M_INFO, 0, _("File: %s\n"), jcr->fname);
      jcr->fn_printed = TRUE;
   }
}
