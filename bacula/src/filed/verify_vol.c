/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2002-2014 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from many
   others, a complete list can be found in the file AUTHORS.

   You may use this file and others of this release according to the
   license defined in the LICENSE file, which includes the Affero General
   Public License, v3.0 ("AGPLv3") and some additional permissions and
   terms pursuant to its AGPLv3 Section 7.

   Bacula® is a registered trademark of Kern Sibbald.
*/
/*
 *  Bacula File Daemon  verify-vol.c Verify files on a Volume
 *    versus attributes in Catalog
 *
 *    Kern Sibbald, July MMII
 *
 */

#include "bacula.h"
#include "filed.h"

/* Data received from Storage Daemon */
static char rec_header[] = "rechdr %ld %ld %ld %ld %ld";

/* Forward referenced functions */


/*
 * Verify attributes of the requested files on the Volume
 *
 */
void do_verify_volume(JCR *jcr)
{
   BSOCK *sd, *dir;
   POOLMEM *fname;                    /* original file name */
   POOLMEM *lname;                    /* link name */
   int32_t stream;
   uint32_t size;
   uint32_t VolSessionId, VolSessionTime, file_index;
   uint32_t record_file_index;
   char digest[BASE64_SIZE(CRYPTO_DIGEST_MAX_SIZE)];
   int type, stat;

   sd = jcr->store_bsock;
   if (!sd) {
      Jmsg(jcr, M_FATAL, 0, _("Storage command not issued before Verify.\n"));
      jcr->setJobStatus(JS_FatalError);
      return;
   }
   dir = jcr->dir_bsock;
   jcr->setJobStatus(JS_Running);

   LockRes();
   CLIENT *client = (CLIENT *)GetNextRes(R_CLIENT, NULL);
   UnlockRes();
   uint32_t buf_size;
   if (client) {
      buf_size = client->max_network_buffer_size;
   } else {
      buf_size = 0;                   /* use default */
   }
   if (!sd->set_buffer_size(buf_size, BNET_SETBUF_WRITE)) {
      jcr->setJobStatus(JS_FatalError);
      return;
   }
   jcr->buf_size = sd->msglen;

   fname = get_pool_memory(PM_FNAME);
   lname = get_pool_memory(PM_FNAME);

   /*
    * Get a record from the Storage daemon
    */
   while (bget_msg(sd) >= 0 && !job_canceled(jcr)) {
      /*
       * First we expect a Stream Record Header
       */
      if (sscanf(sd->msg, rec_header, &VolSessionId, &VolSessionTime, &file_index,
          &stream, &size) != 5) {
         Jmsg1(jcr, M_FATAL, 0, _("Record header scan error: %s\n"), sd->msg);
         goto bail_out;
      }
      Dmsg3(30, "Got hdr: FilInx=%d Stream=%d size=%d.\n", file_index, stream, size);

      /*
       * Now we expect the Stream Data
       */
      if (bget_msg(sd) < 0) {
         Jmsg1(jcr, M_FATAL, 0, _("Data record error. ERR=%s\n"), sd->bstrerror());
         goto bail_out;
      }
      if (size != ((uint32_t)sd->msglen)) {
         Jmsg2(jcr, M_FATAL, 0, _("Actual data size %d not same as header %d\n"), sd->msglen, size);
         goto bail_out;
      }
      Dmsg2(30, "Got stream data %s, len=%d\n", stream_to_ascii(stream), sd->msglen);

      /* File Attributes stream */
      switch (stream) {
      case STREAM_UNIX_ATTRIBUTES:
      case STREAM_UNIX_ATTRIBUTES_EX:
         char *ap, *lp, *fp;

         Dmsg0(400, "Stream=Unix Attributes.\n");

         if ((int)sizeof_pool_memory(fname) < sd->msglen) {
            fname = realloc_pool_memory(fname, sd->msglen + 1);
         }

         if ((int)sizeof_pool_memory(lname) < sd->msglen) {
            lname = realloc_pool_memory(lname, sd->msglen + 1);
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
          *    Extended Attributes (if Win32)
          */
         if (sscanf(sd->msg, "%d %d", &record_file_index, &type) != 2) {
            Jmsg(jcr, M_FATAL, 0, _("Error scanning record header: %s\n"), sd->msg);
            Dmsg0(0, "\nError scanning header\n");
            goto bail_out;
         }
         Dmsg2(30, "Got Attr: FilInx=%d type=%d\n", record_file_index, type);
         ap = sd->msg;
         while (*ap++ != ' ')         /* skip record file index */
            ;
         while (*ap++ != ' ')         /* skip type */
            ;
         /* Save filename and position to attributes */
         fp = fname;
         while (*ap != 0) {
            *fp++  = *ap++;           /* copy filename to fname */
         }
         *fp = *ap++;                 /* terminate filename & point to attribs */

         Dmsg2(100, "File=%s Attr=%s\n", fname, ap);
         /* Skip to Link name */
         if (type == FT_LNK || type == FT_LNKSAVED) {
            lp = ap;
            while (*lp++ != 0) {
               ;
            }
            pm_strcat(lname, lp);        /* "save" link name */
         } else {
            *lname = 0;
         }
         jcr->lock();
         jcr->JobFiles++;
         jcr->num_files_examined++;
         pm_strcpy(jcr->last_fname, fname); /* last file examined */
         jcr->unlock();

         /*
          * Send file attributes to Director
          *   File_index
          *   Stream
          *   Verify Options
          *   Filename (full path)
          *   Encoded attributes
          *   Link name (if type==FT_LNK)
          * For a directory, link is the same as fname, but with trailing
          * slash. For a linked file, link is the link.
          */
         /* Send file attributes to Director */
         Dmsg2(200, "send ATTR inx=%d fname=%s\n", jcr->JobFiles, fname);
         if (type == FT_LNK || type == FT_LNKSAVED) {
            stat = dir->fsend("%d %d %s %s%c%s%c%s%c", jcr->JobFiles,
                          STREAM_UNIX_ATTRIBUTES, "pinsug5", fname,
                          0, ap, 0, lname, 0);
         /* for a deleted record, we set fileindex=0 */
         } else if (type == FT_DELETED)  {
            stat = dir->fsend("%d %d %s %s%c%s%c%c", 0,
                          STREAM_UNIX_ATTRIBUTES, "pinsug5", fname,
                          0, ap, 0, 0);
         } else {
            stat = dir->fsend("%d %d %s %s%c%s%c%c", jcr->JobFiles,
                          STREAM_UNIX_ATTRIBUTES, "pinsug5", fname,
                          0, ap, 0, 0);
         }
         Dmsg2(200, "bfiled>bdird: attribs len=%d: msg=%s\n", dir->msglen, dir->msg);
         if (!stat) {
            Jmsg(jcr, M_FATAL, 0, _("Network error in send to Director: ERR=%s\n"), dir->bstrerror());
            goto bail_out;
         }
         break;

      case STREAM_MD5_DIGEST:
         bin_to_base64(digest, sizeof(digest), (char *)sd->msg, CRYPTO_DIGEST_MD5_SIZE, true);
         Dmsg2(400, "send inx=%d MD5=%s\n", jcr->JobFiles, digest);
         dir->fsend("%d %d %s *MD5-%d*", jcr->JobFiles, STREAM_MD5_DIGEST, digest,
                    jcr->JobFiles);
         Dmsg2(20, "bfiled>bdird: MD5 len=%d: msg=%s\n", dir->msglen, dir->msg);
         break;

      case STREAM_SHA1_DIGEST:
         bin_to_base64(digest, sizeof(digest), (char *)sd->msg, CRYPTO_DIGEST_SHA1_SIZE, true);
         Dmsg2(400, "send inx=%d SHA1=%s\n", jcr->JobFiles, digest);
         dir->fsend("%d %d %s *SHA1-%d*", jcr->JobFiles, STREAM_SHA1_DIGEST,
                    digest, jcr->JobFiles);
         Dmsg2(20, "bfiled>bdird: SHA1 len=%d: msg=%s\n", dir->msglen, dir->msg);
         break;

      case STREAM_SHA256_DIGEST:
         bin_to_base64(digest, sizeof(digest), (char *)sd->msg, CRYPTO_DIGEST_SHA256_SIZE, true);
         Dmsg2(400, "send inx=%d SHA256=%s\n", jcr->JobFiles, digest);
         dir->fsend("%d %d %s *SHA256-%d*", jcr->JobFiles, STREAM_SHA256_DIGEST,
                    digest, jcr->JobFiles);
         Dmsg2(20, "bfiled>bdird: SHA256 len=%d: msg=%s\n", dir->msglen, dir->msg);
         break;

      case STREAM_SHA512_DIGEST:
         bin_to_base64(digest, sizeof(digest), (char *)sd->msg, CRYPTO_DIGEST_SHA512_SIZE, true);
         Dmsg2(400, "send inx=%d SHA512=%s\n", jcr->JobFiles, digest);
         dir->fsend("%d %d %s *SHA512-%d*", jcr->JobFiles, STREAM_SHA512_DIGEST,
                    digest, jcr->JobFiles);
         Dmsg2(20, "bfiled>bdird: SHA512 len=%d: msg=%s\n", dir->msglen, dir->msg);
         break;

      /*
       * Restore stream object is counted, but not restored here
       */
      case STREAM_RESTORE_OBJECT:
         jcr->lock();
         jcr->JobFiles++;
         jcr->num_files_examined++;
         jcr->unlock();
         break;

      /* Ignore everything else */
      default:
         break;

      } /* end switch */
   } /* end while bnet_get */
   jcr->setJobStatus(JS_Terminated);
   goto ok_out;

bail_out:
   jcr->setJobStatus(JS_ErrorTerminated);

ok_out:
   if (jcr->compress_buf) {
      free(jcr->compress_buf);
      jcr->compress_buf = NULL;
   }
   free_pool_memory(fname);
   free_pool_memory(lname);
   Dmsg2(050, "End Verify-Vol. Files=%d Bytes=%" lld "\n", jcr->JobFiles,
      jcr->JobBytes);
}
