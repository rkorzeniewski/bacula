/*
 *  Bacula File Daemon  restore.c Restorefiles.
 *
 *    Kern Sibbald, November MM
 *
 *   Version $Id$
 *
 */
/*
   Copyright (C) 2000-2006 Kern Sibbald

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   version 2 as amended with additional clauses defined in the
   file LICENSE in the main source directory.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the 
   the file LICENSE for additional details.

 */

#include "bacula.h"
#include "filed.h"

#ifdef HAVE_DARWIN_OS
#include <sys/attr.h>
#endif

#if defined(HAVE_CRYPTO)
const bool have_crypto = true;
#else
const bool have_crypto = false;
#endif

/* Data received from Storage Daemon */
static char rec_header[] = "rechdr %ld %ld %ld %ld %ld";

/* Forward referenced functions */
#if   defined(HAVE_LIBZ)
static const char *zlib_strerror(int stat);
const bool have_libz = true;
#else
const bool have_libz = false;
#endif

int verify_signature(JCR *jcr, SIGNATURE *sig);
int32_t extract_data(JCR *jcr, BFILE *bfd, POOLMEM *buf, int32_t buflen,
      uint64_t *addr, int flags, CIPHER_CONTEXT *cipher, uint32_t cipher_block_size);
bool flush_cipher(JCR *jcr, BFILE *bfd, int flags, CIPHER_CONTEXT *cipher, uint32_t cipher_block_size);

#define RETRY 10                      /* retry wait time */

/*
 * Close a bfd check that we are at the expected file offset.
 * Makes some code in set_attributes().
 */
int bclose_chksize(JCR *jcr, BFILE *bfd, off_t osize)
{
   char ec1[50], ec2[50];
   off_t fsize;

   fsize = blseek(bfd, 0, SEEK_CUR);
   bclose(bfd);                              /* first close file */
   if (fsize > 0 && fsize != osize) {
      Qmsg3(jcr, M_ERROR, 0, _("Size of data or stream of %s not correct. Original %s, restored %s.\n"),
            jcr->last_fname, edit_uint64(osize, ec1),
            edit_uint64(fsize, ec2));
      return -1;
   }
   return 0;
}

/*
 * Restore the requested files.
 *
 */
void do_restore(JCR *jcr)
{
   BSOCK *sd;
   int32_t stream = 0;
   int32_t prev_stream;
   uint32_t VolSessionId, VolSessionTime;
   bool extract = false;
   int32_t file_index;
   char ec1[50];                      /* Buffer printing huge values */

   BFILE bfd;                         /* File content */
   uint64_t fileAddr = 0;             /* file write address */
   uint32_t size;                     /* Size of file */
   BFILE altbfd;                      /* Alternative data stream */
   uint64_t alt_addr = 0;             /* Write address for alternative stream */
   intmax_t alt_size = 0;             /* Size of alternate stream */
   SIGNATURE *sig = NULL;             /* Cryptographic signature (if any) for file */
   CRYPTO_SESSION *cs = NULL;         /* Cryptographic session data (if any) for file */
   CIPHER_CONTEXT *cipher_ctx = NULL; /* Cryptographic cipher context (if any) for file */
   uint32_t cipher_block_size = 0;    /* Cryptographic algorithm block size for file */
   int flags = 0;                     /* Options for extract_data() */
   int stat;
   ATTR *attr;

   /* The following variables keep track of "known unknowns" */
   int non_support_data = 0;
   int non_support_attr = 0;
   int non_support_rsrc = 0;
   int non_support_finfo = 0;
   int non_support_acl = 0;
   int non_support_progname = 0;

   /* Finally, set up for special configurations */
#ifdef HAVE_DARWIN_OS
   intmax_t rsrc_len = 0;             /* Original length of resource fork */
   struct attrlist attrList;

   memset(&attrList, 0, sizeof(attrList));
   attrList.bitmapcount = ATTR_BIT_MAP_COUNT;
   attrList.commonattr = ATTR_CMN_FNDRINFO;
#endif

   sd = jcr->store_bsock;
   set_jcr_job_status(jcr, JS_Running);

   LockRes();
   CLIENT *client = (CLIENT *)GetNextRes(R_CLIENT, NULL);
   UnlockRes();
   uint32_t buf_size;
   if (client) {
      buf_size = client->max_network_buffer_size;
   } else {
      buf_size = 0;                   /* use default */
   }
   if (!bnet_set_buffer_size(sd, buf_size, BNET_SETBUF_WRITE)) {
      set_jcr_job_status(jcr, JS_ErrorTerminated);
      return;
   }
   jcr->buf_size = sd->msglen;

#ifdef stbernard_implemented
/  #if defined(HAVE_WIN32)
   bool        bResumeOfmOnExit = FALSE;
   if (isOpenFileManagerRunning()) {
       if ( pauseOpenFileManager() ) {
          Jmsg(jcr, M_INFO, 0, _("Open File Manager paused\n") );
          bResumeOfmOnExit = TRUE;
       }
       else {
          Jmsg(jcr, M_ERROR, 0, _("FAILED to pause Open File Manager\n") );
       }
   }
   {
       char username[UNLEN+1];
       DWORD usize = sizeof(username);
       int privs = enable_backup_privileges(NULL, 1);
       if (GetUserName(username, &usize)) {
          Jmsg2(jcr, M_INFO, 0, _("Running as '%s'. Privmask=%#08x\n"), username,
       } else {
          Jmsg(jcr, M_WARNING, 0, _("Failed to retrieve current UserName\n"));
       }
   }
#endif

   if (have_libz) {
      uint32_t compress_buf_size = jcr->buf_size + 12 + ((jcr->buf_size+999) / 1000) + 100;
      jcr->compress_buf = (char *)bmalloc(compress_buf_size);
      jcr->compress_buf_size = compress_buf_size;
   }

   if (have_crypto) {
      jcr->crypto_buf = get_memory(CRYPTO_CIPHER_MAX_BLOCK_SIZE);
   }
   
   /*
    * Get a record from the Storage daemon. We are guaranteed to
    *   receive records in the following order:
    *   1. Stream record header
    *   2. Stream data
    *        a. Attributes (Unix or Win32)
    *        b. Possibly stream encryption session data (e.g., symmetric session key)
    *    or  c. File data for the file
    *    or  d. Alternate data stream (e.g. Resource Fork)
    *    or  e. Finder info
    *    or  f. ACLs
    *    or  g. Possibly a cryptographic signature
    *    or  h. Possibly MD5 or SHA1 record
    *   3. Repeat step 1
    *
    * NOTE: We keep track of two bacula file descriptors:
    *   1. bfd for file data.
    *      This fd is opened for non empty files when an attribute stream is
    *      encountered and closed when we find the next attribute stream.
    *   2. alt_bfd for alternate data streams
    *      This fd is opened every time we encounter a new alternate data
    *      stream for the current file. When we find any other stream, we
    *      close it again.
    *      The expected size of the stream, alt_len, should be set when
    *      opening the fd.
    */
   binit(&bfd);
   binit(&altbfd);
   attr = new_attr();
   jcr->acl_text = get_pool_memory(PM_MESSAGE);

   while (bget_msg(sd) >= 0 && !job_canceled(jcr)) {
      /* Remember previous stream type */
      prev_stream = stream;

      /* First we expect a Stream Record Header */
      if (sscanf(sd->msg, rec_header, &VolSessionId, &VolSessionTime, &file_index,
          &stream, &size) != 5) {
         Jmsg1(jcr, M_FATAL, 0, _("Record header scan error: %s\n"), sd->msg);
         goto bail_out;
      }
      Dmsg2(30, "Got hdr: FilInx=%d Stream=%d.\n", file_index, stream);

      /* * Now we expect the Stream Data */
      if (bget_msg(sd) < 0) {
         Jmsg1(jcr, M_FATAL, 0, _("Data record error. ERR=%s\n"), bnet_strerror(sd));
         goto bail_out;
      }
      if (size != (uint32_t)sd->msglen) {
         Jmsg2(jcr, M_FATAL, 0, _("Actual data size %d not same as header %d\n"), sd->msglen, size);
         goto bail_out;
      }
      Dmsg1(30, "Got stream data, len=%d\n", sd->msglen);

      /* If we change streams, close and reset alternate data streams */
      if (prev_stream != stream) {
         if (is_bopen(&altbfd)) {
            bclose_chksize(jcr, &altbfd, alt_size);
         }
         alt_size = -1; /* Use an impossible value and set a proper one below */
         alt_addr = 0;
      }

      /* File Attributes stream */
      switch (stream) {
      case STREAM_UNIX_ATTRIBUTES:
      case STREAM_UNIX_ATTRIBUTES_EX:
         Dmsg1(30, "Stream=Unix Attributes. extract=%d\n", extract);
         /*
          * If extracting, it was from previous stream, so
          * close the output file and validate the signature.
          */
         if (extract) {
            if (size > 0 && !is_bopen(&bfd)) {
               Jmsg0(jcr, M_ERROR, 0, _("Logic error: output file should be open\n"));
            }
            /* Flush and deallocate previous stream's cipher context */
            if (cipher_ctx && prev_stream != STREAM_ENCRYPTED_SESSION_DATA) {
               flush_cipher(jcr, &bfd, flags, cipher_ctx, cipher_block_size);
               crypto_cipher_free(cipher_ctx);
               cipher_ctx = NULL;
            }
            set_attributes(jcr, attr, &bfd);
            extract = false;

            /* Verify the cryptographic signature, if any */
            if (jcr->pki_sign) {
               if (sig) {
                  // Failure is reported in verify_signature() ...
                  verify_signature(jcr, sig);
               } else {
                  Jmsg1(jcr, M_ERROR, 0, _("Missing cryptographic signature for %s\n"), jcr->last_fname);
               }
            }
            /* Free Signature */
            if (sig) {
               crypto_sign_free(sig);
               sig = NULL;
            }
            if (cs) {
               crypto_session_free(cs);
               cs = NULL;
            }
            Dmsg0(30, "Stop extracting.\n");
         } else if (is_bopen(&bfd)) {
            Jmsg0(jcr, M_ERROR, 0, _("Logic error: output file should not be open\n"));
            bclose(&bfd);
         }

         /*
          * Unpack and do sanity check fo attributes.
          */
         if (!unpack_attributes_record(jcr, stream, sd->msg, attr)) {
            goto bail_out;
         }
         if (file_index != attr->file_index) {
            Jmsg(jcr, M_FATAL, 0, _("Record header file index %ld not equal record index %ld\n"),
                 file_index, attr->file_index);
            Dmsg0(100, "File index error\n");
            goto bail_out;
         }

         Dmsg3(200, "File %s\nattrib=%s\nattribsEx=%s\n", attr->fname,
               attr->attr, attr->attrEx);

         attr->data_stream = decode_stat(attr->attr, &attr->statp, &attr->LinkFI);

         if (!is_restore_stream_supported(attr->data_stream)) {
            if (!non_support_data++) {
               Jmsg(jcr, M_ERROR, 0, _("%s stream not supported on this Client.\n"),
                  stream_to_ascii(attr->data_stream));
            }
            continue;
         }

         build_attr_output_fnames(jcr, attr);

         /*
          * Now determine if we are extracting or not.
          */
         jcr->num_files_examined++;
         Dmsg1(30, "Outfile=%s\n", attr->ofname);
         extract = false;
         stat = create_file(jcr, attr, &bfd, jcr->replace);
         switch (stat) {
         case CF_ERROR:
         case CF_SKIP:
            break;
         case CF_EXTRACT:        /* File created and we expect file data */
            extract = true;
            /* FALLTHROUGH */
         case CF_CREATED:        /* File created, but there is no content */
            jcr->lock();  
            pm_strcpy(jcr->last_fname, attr->ofname);
            jcr->last_type = attr->type;
            jcr->JobFiles++;
            jcr->unlock();
            fileAddr = 0;
            print_ls_output(jcr, attr);
#ifdef HAVE_DARWIN_OS
            /* Only restore the resource fork for regular files */
            from_base64(&rsrc_len, attr->attrEx);
            if (attr->type == FT_REG && rsrc_len > 0) {
               extract = true;
            }
#endif
            if (!extract) {
               /* set attributes now because file will not be extracted */
               set_attributes(jcr, attr, &bfd);
            }
            break;
         }
         break;

      /* Data stream */
      case STREAM_ENCRYPTED_SESSION_DATA:
         crypto_error_t cryptoerr;

         Dmsg1(30, "Stream=Encrypted Session Data, size: %d\n", sd->msglen);

         /* Decode and save session keys. */
         cryptoerr = crypto_session_decode((uint8_t *)sd->msg, (uint32_t)sd->msglen, jcr->pki_recipients, &cs);
         switch(cryptoerr) {
         case CRYPTO_ERROR_NONE:
            /* Success */
            break;
         case CRYPTO_ERROR_NORECIPIENT:
            Jmsg(jcr, M_ERROR, 0, _("Missing private key required to decrypt encrypted backup data."));
            break;
         case CRYPTO_ERROR_DECRYPTION:
            Jmsg(jcr, M_ERROR, 0, _("Decrypt of the session key failed."));
            break;
         default:
            /* Shouldn't happen */
            Jmsg1(jcr, M_ERROR, 0, _("An error occured while decoding encrypted session data stream: %s"), crypto_strerror(cryptoerr));
            break;
         }

         if (cryptoerr != CRYPTO_ERROR_NONE) {
            extract = false;
            bclose(&bfd);
            continue;
         }

         /* Set up a decryption context */
         if ((cipher_ctx = crypto_cipher_new(cs, false, &cipher_block_size)) == NULL) {
            Jmsg1(jcr, M_ERROR, 0, _("Failed to initialize decryption context for %s\n"), jcr->last_fname);
            crypto_session_free(cs);
            cs = NULL;
            extract = false;
            bclose(&bfd);
            continue;
         }
         break;

      case STREAM_FILE_DATA:
      case STREAM_SPARSE_DATA:
      case STREAM_WIN32_DATA:
      case STREAM_GZIP_DATA:
      case STREAM_SPARSE_GZIP_DATA:
      case STREAM_WIN32_GZIP_DATA:
      case STREAM_ENCRYPTED_FILE_DATA:
      case STREAM_ENCRYPTED_WIN32_DATA:
      case STREAM_ENCRYPTED_FILE_GZIP_DATA:
      case STREAM_ENCRYPTED_WIN32_GZIP_DATA:
         /* Force an expected, consistent stream type here */
         if (extract && (prev_stream == stream || prev_stream == STREAM_UNIX_ATTRIBUTES
                  || prev_stream == STREAM_UNIX_ATTRIBUTES_EX
                  || prev_stream == STREAM_ENCRYPTED_SESSION_DATA)) {
            flags = 0;

            if (stream == STREAM_SPARSE_DATA || stream == STREAM_SPARSE_GZIP_DATA) {
               flags |= FO_SPARSE;
            }

            if (stream == STREAM_GZIP_DATA || stream == STREAM_SPARSE_GZIP_DATA
                  || stream == STREAM_WIN32_GZIP_DATA || stream == STREAM_ENCRYPTED_FILE_GZIP_DATA
                  || stream == STREAM_ENCRYPTED_WIN32_GZIP_DATA) {
               flags |= FO_GZIP;
            }

            if (stream == STREAM_ENCRYPTED_FILE_DATA
                  || stream == STREAM_ENCRYPTED_FILE_GZIP_DATA
                  || stream == STREAM_ENCRYPTED_WIN32_DATA
                  || stream == STREAM_ENCRYPTED_WIN32_GZIP_DATA) {
               flags |= FO_ENCRYPT;
            }

            if (is_win32_stream(stream) && !have_win32_api()) {
               set_portable_backup(&bfd);
               flags |= FO_WIN32DECOMP;    /* "decompose" BackupWrite data */
            }

            if (extract_data(jcr, &bfd, sd->msg, sd->msglen, &fileAddr, flags, cipher_ctx, cipher_block_size) < 0) {
               extract = false;
               bclose(&bfd);
               continue;
            }
         }
         break;

      /* Resource fork stream - only recorded after a file to be restored */
      /* Silently ignore if we cannot write - we already reported that */
      case STREAM_ENCRYPTED_MACOS_FORK_DATA:
         flags |= FO_ENCRYPT;
      case STREAM_MACOS_FORK_DATA:
#ifdef HAVE_DARWIN_OS
         if (extract) {
            if (prev_stream != stream) {
               if (bopen_rsrc(&altbfd, jcr->last_fname, O_WRONLY | O_TRUNC | O_BINARY, 0) < 0) {
                  Jmsg(jcr, M_ERROR, 0, _("     Cannot open resource fork for %s.\n"), jcr->last_fname);
                  extract = false;
                  continue;
               }
               alt_size = rsrc_len;
               Dmsg0(30, "Restoring resource fork\n");
            }
            flags = 0;
            if (extract_data(jcr, &altbfd, sd->msg, sd->msglen, &alt_addr, flags, cipher_ctx, cipher_block_size) < 0) {
               extract = false;
               bclose(&altbfd);
               continue;
            }
         }
#else
         non_support_rsrc++;
#endif
         break;

      case STREAM_HFSPLUS_ATTRIBUTES:
#ifdef HAVE_DARWIN_OS
         Dmsg0(30, "Restoring Finder Info\n");
         if (sd->msglen != 32) {
            Jmsg(jcr, M_ERROR, 0, _("     Invalid length of Finder Info (got %d, not 32)\n"), sd->msglen);
            continue;
         }
         if (setattrlist(jcr->last_fname, &attrList, sd->msg, sd->msglen, 0) != 0) {
            Jmsg(jcr, M_ERROR, 0, _("     Could not set Finder Info on %s\n"), jcr->last_fname);
            continue;
         }
#else
         non_support_finfo++;
#endif

      case STREAM_UNIX_ATTRIBUTES_ACCESS_ACL:
#ifdef HAVE_ACL
         pm_strcpy(jcr->acl_text, sd->msg);
         Dmsg2(400, "Restoring ACL type 0x%2x <%s>\n", BACL_TYPE_ACCESS, jcr->acl_text);
         if (bacl_set(jcr, BACL_TYPE_ACCESS) != 0) {
               Qmsg1(jcr, M_WARNING, 0, _("Can't restore ACL of %s\n"), jcr->last_fname);
         }
#else 
         non_support_acl++;
#endif
         break;

      case STREAM_UNIX_ATTRIBUTES_DEFAULT_ACL:
#ifdef HAVE_ACL
         pm_strcpy(jcr->acl_text, sd->msg);
         Dmsg2(400, "Restoring ACL type 0x%2x <%s>\n", BACL_TYPE_DEFAULT, jcr->acl_text);
         if (bacl_set(jcr, BACL_TYPE_DEFAULT) != 0) {
               Qmsg1(jcr, M_WARNING, 0, _("Can't restore default ACL of %s\n"), jcr->last_fname);
         }
#else 
         non_support_acl++;
#endif
         break;

      case STREAM_SIGNED_DIGEST:
         /* Save signature. */
         if ((sig = crypto_sign_decode((uint8_t *)sd->msg, (uint32_t)sd->msglen)) == NULL) {
            Jmsg1(jcr, M_ERROR, 0, _("Failed to decode message signature for %s\n"), jcr->last_fname);
         }
         break;

      case STREAM_MD5_DIGEST:
      case STREAM_SHA1_DIGEST:
      case STREAM_SHA256_DIGEST:
      case STREAM_SHA512_DIGEST:
         break;

      case STREAM_PROGRAM_NAMES:
      case STREAM_PROGRAM_DATA:
         if (!non_support_progname) {
            Pmsg0(000, "Got Program Name or Data Stream. Ignored.\n");
            non_support_progname++;
         }
         break;

      default:
         /* If extracting, wierd stream (not 1 or 2), close output file anyway */
         if (extract) {
            Dmsg1(30, "Found wierd stream %d\n", stream);
            if (size > 0 && !is_bopen(&bfd)) {
               Jmsg0(jcr, M_ERROR, 0, _("Logic error: output file should be open\n"));
            }
            /* Flush and deallocate cipher context */
            if (cipher_ctx) {
               flush_cipher(jcr, &bfd, flags, cipher_ctx, cipher_block_size);
               crypto_cipher_free(cipher_ctx);
               cipher_ctx = NULL;
            }
            set_attributes(jcr, attr, &bfd);

            /* Verify the cryptographic signature if any */
            if (jcr->pki_sign) {
               if (sig) {
                  // Failure is reported in verify_signature() ...
                  verify_signature(jcr, sig);
               } else {
                  Jmsg1(jcr, M_ERROR, 0, _("Missing cryptographic signature for %s\n"), jcr->last_fname);
               }
            }

            extract = false;
         } else if (is_bopen(&bfd)) {
            Jmsg0(jcr, M_ERROR, 0, _("Logic error: output file should not be open\n"));
            bclose(&bfd);
         }
         Jmsg(jcr, M_ERROR, 0, _("Unknown stream=%d ignored. This shouldn't happen!\n"), stream);
         Dmsg2(0, "None of above!!! stream=%d data=%s\n", stream,sd->msg);
         break;
      } /* end switch(stream) */

   } /* end while get_msg() */

   /* If output file is still open, it was the last one in the
    * archive since we just hit an end of file, so close the file.
    */
   if (is_bopen(&altbfd)) {
      bclose_chksize(jcr, &altbfd, alt_size);
   }
   if (extract) {
      /* Flush and deallocate cipher context */
      if (cipher_ctx) {
         flush_cipher(jcr, &bfd, flags, cipher_ctx, cipher_block_size);
         crypto_cipher_free(cipher_ctx);
         cipher_ctx = NULL;
      }
      set_attributes(jcr, attr, &bfd);

      /* Verify the cryptographic signature on the last file, if any */
      if (jcr->pki_sign) {
         if (sig) {
            // Failure is reported in verify_signature() ...
            verify_signature(jcr, sig);
         } else {
            Jmsg1(jcr, M_ERROR, 0, _("Missing cryptographic signature for %s\n"), jcr->last_fname);
         }
      }
   }

   if (is_bopen(&bfd)) {
      bclose(&bfd);
   }

   set_jcr_job_status(jcr, JS_Terminated);
   goto ok_out;

bail_out:
   set_jcr_job_status(jcr, JS_ErrorTerminated);
ok_out:

   /* Free Signature & Crypto Data */
   if (sig) {
      crypto_sign_free(sig);
      sig = NULL;
   }
   if (cs) {
      crypto_session_free(cs);
      cs = NULL;
   }
   if (cipher_ctx) {
      crypto_cipher_free(cipher_ctx);
      cipher_ctx = NULL;
   }
   if (jcr->compress_buf) {
      free(jcr->compress_buf);
      jcr->compress_buf = NULL;
      jcr->compress_buf_size = 0;
   }
   if (jcr->crypto_buf) {
      free_pool_memory(jcr->crypto_buf);
      jcr->crypto_buf = NULL;
   }
   bclose(&altbfd);
   bclose(&bfd);
   free_attr(attr);
   free_pool_memory(jcr->acl_text);
   Dmsg2(10, "End Do Restore. Files=%d Bytes=%s\n", jcr->JobFiles,
      edit_uint64(jcr->JobBytes, ec1));
   if (non_support_data > 1 || non_support_attr > 1) {
      Jmsg(jcr, M_ERROR, 0, _("%d non-supported data streams and %d non-supported attrib streams ignored.\n"),
         non_support_data, non_support_attr);
   }
   if (non_support_rsrc) {
      Jmsg(jcr, M_INFO, 0, _("%d non-supported resource fork streams ignored.\n"), non_support_rsrc);
   }
   if (non_support_finfo) {
      Jmsg(jcr, M_INFO, 0, _("%d non-supported Finder Info streams ignored.\n"), non_support_rsrc);
   }
   if (non_support_acl) {
      Jmsg(jcr, M_INFO, 0, _("%d non-supported acl streams ignored.\n"), non_support_acl);
   }

}

/*
 * Convert ZLIB error code into an ASCII message
 */
static const char *zlib_strerror(int stat)
{
   if (stat >= 0) {
      return _("None");
   }
   switch (stat) {
   case Z_ERRNO:
      return _("Zlib errno");
   case Z_STREAM_ERROR:
      return _("Zlib stream error");
   case Z_DATA_ERROR:
      return _("Zlib data error");
   case Z_MEM_ERROR:
      return _("Zlib memory error");
   case Z_BUF_ERROR:
      return _("Zlib buffer error");
   case Z_VERSION_ERROR:
      return _("Zlib version error");
   default:
      return _("*none*");
   }
}

static int do_file_digest(FF_PKT *ff_pkt, void *pkt, bool top_level) 
{
   JCR *jcr = (JCR *)pkt;
   return (digest_file(jcr, ff_pkt, jcr->digest));
}

/*
 * Verify the signature for the last restored file
 * Return value is either true (signature correct)
 * or false (signature could not be verified).
 * TODO landonf: Better signature failure handling.
 */
int verify_signature(JCR *jcr, SIGNATURE *sig)
{
   X509_KEYPAIR *keypair;
   DIGEST *digest = NULL;
   crypto_error_t err;

   /* Iterate through the trusted signers */
   foreach_alist(keypair, jcr->pki_signers) {
      err = crypto_sign_get_digest(sig, jcr->pki_keypair, &digest);

      switch (err) {
      case CRYPTO_ERROR_NONE:
         /* Signature found, digest allocated */
         jcr->digest = digest;

         /* Checksum the entire file */
         if (find_one_file(jcr, jcr->ff, do_file_digest, jcr, jcr->last_fname, (dev_t)-1, 1) != 0) {
            Qmsg(jcr, M_ERROR, 0, _("Signature validation failed for %s: \n"), jcr->last_fname);
            return false;
         }

         /* Verify the signature */
         if ((err = crypto_sign_verify(sig, keypair, digest)) != CRYPTO_ERROR_NONE) {
            Dmsg1(100, "Bad signature on %s\n", jcr->last_fname);
            Qmsg2(jcr, M_ERROR, 0, _("Signature validation failed for %s: %s\n"), jcr->last_fname, crypto_strerror(err));
            crypto_digest_free(digest);
            return false;
         }

         /* Valid signature */
         Dmsg1(100, "Signature good on %s\n", jcr->last_fname);
         crypto_digest_free(digest);
         return true;

      case CRYPTO_ERROR_NOSIGNER:
         /* Signature not found, try again */
         continue;
      default:
         /* Something strange happened (that shouldn't happen!)... */
         Qmsg2(jcr, M_ERROR, 0, _("Signature validation failed for %s: %s\n"), jcr->last_fname, crypto_strerror(err));
         if (digest) {
            crypto_digest_free(digest);
         }
         return false;
      }
   }

   /* No signer */
   Dmsg1(100, "Could not find a valid public key for signature on %s\n", jcr->last_fname);
   crypto_digest_free(digest);
   return false;
}

/*
 * In the context of jcr, write data to bfd.
 * We write buflen bytes in buf at addr. addr is updated in place.
 * The flags specify whether to use sparse files or compression.
 * Return value is the number of bytes written, or -1 on errors.
 */
int32_t extract_data(JCR *jcr, BFILE *bfd, POOLMEM *buf, int32_t buflen,
      uint64_t *addr, int flags, CIPHER_CONTEXT *cipher, uint32_t cipher_block_size)
{
   int stat;
   char *wbuf;                        /* write buffer */
   uint32_t wsize;                    /* write size */
   uint32_t rsize;                    /* read size */
   char ec1[50];                      /* Buffer printing huge values */
   const uint8_t *cipher_input;       /* Decryption input */
   uint32_t cipher_input_len;         /* Decryption input length */
   uint32_t decrypted_len = 0;        /* Decryption output length */

   if (flags & FO_SPARSE) {
      ser_declare;
      uint64_t faddr;
      char ec1[50];
      wbuf = buf + SPARSE_FADDR_SIZE;
      rsize = buflen - SPARSE_FADDR_SIZE;
      ser_begin(buf, SPARSE_FADDR_SIZE);
      unser_uint64(faddr);
      if (*addr != faddr) {
         *addr = faddr;
         if (blseek(bfd, (off_t)*addr, SEEK_SET) < 0) {
            berrno be;
            Jmsg3(jcr, M_ERROR, 0, _("Seek to %s error on %s: ERR=%s\n"),
                  edit_uint64(*addr, ec1), jcr->last_fname, 
                  be.strerror(bfd->berrno));
            return -1;
         }
      }
   } else {
      wbuf = buf;
      rsize = buflen;
   }
   wsize = rsize;
   cipher_input = (uint8_t *)wbuf;
   cipher_input_len = (uint32_t)wsize;

   if (flags & FO_GZIP) {
#ifdef HAVE_LIBZ
      uLong compress_len;
      /* 
       * NOTE! We only use uLong and Byte because they are
       *  needed by the zlib routines, they should not otherwise
       *  be used in Bacula.
       */
      compress_len = jcr->compress_buf_size;
      Dmsg2(100, "Comp_len=%d msglen=%d\n", compress_len, wsize);
      if ((stat=uncompress((Byte *)jcr->compress_buf, &compress_len,
                  (const Byte *)wbuf, (uLong)rsize)) != Z_OK) {
         Qmsg(jcr, M_ERROR, 0, _("Uncompression error on file %s. ERR=%s\n"),
               jcr->last_fname, zlib_strerror(stat));
         return -1;
      }
      wbuf = jcr->compress_buf;
      wsize = compress_len;
      cipher_input = (uint8_t *)jcr->compress_buf; /* decrypt decompressed data */
      cipher_input_len = compress_len;
      Dmsg2(100, "Write uncompressed %d bytes, total before write=%s\n", compress_len, edit_uint64(jcr->JobBytes, ec1));
#else
      Qmsg(jcr, M_ERROR, 0, _("GZIP data stream found, but GZIP not configured!\n"));
      return -1;
#endif
   } else {
      Dmsg2(30, "Write %u bytes, total before write=%s\n", wsize, edit_uint64(jcr->JobBytes, ec1));
   }

   if (flags & FO_ENCRYPT) {
      ASSERT(cipher);

      /*
       * Grow the crypto buffer, if necessary.
       * crypto_cipher_update() will process only whole blocks,
       * buffering the remaining input.
       */
      jcr->crypto_buf = check_pool_memory_size(jcr->crypto_buf, cipher_input_len + cipher_block_size);


      /* Encrypt the input block */
      if (!crypto_cipher_update(cipher, cipher_input, cipher_input_len, (uint8_t *)jcr->crypto_buf, &decrypted_len)) {
         /* Decryption failed. Shouldn't happen. */
         Jmsg(jcr, M_FATAL, 0, _("Decryption error\n"));
         return -1;
      }

      if (decrypted_len == 0) {
         /* No full block of data available, write more data */
         goto ok;
      }

      Dmsg2(400, "decrypted len=%d undecrypted len=%d\n",
         decrypted_len, cipher_input_len);
      wsize = decrypted_len;
      wbuf = jcr->crypto_buf; /* Decrypted, possibly decompressed output here. */
   }


   if (flags & FO_WIN32DECOMP) {
      if (!processWin32BackupAPIBlock(bfd, wbuf, wsize)) {
         berrno be;
         Jmsg2(jcr, M_ERROR, 0, _("Write error in Win32 Block Decomposition on %s: %s\n"), 
               jcr->last_fname, be.strerror(bfd->berrno));
         return -1;
      }
   } else if (bwrite(bfd, wbuf, wsize) != (ssize_t)wsize) {
      berrno be;
      Jmsg2(jcr, M_ERROR, 0, _("Write error on %s: %s\n"), 
            jcr->last_fname, be.strerror(bfd->berrno));
      return -1;
   }

   if (decrypted_len && decrypted_len > wsize) {
      /* If more than wsize is output, it was previously buffered
       * and reported, and should not be reported again */
      wsize = wsize - decrypted_len;
   }

ok:
   jcr->JobBytes += wsize;
   jcr->ReadBytes += rsize;
   *addr += wsize;

   return wsize;
}

/*
 * In the context of jcr, flush any remaining data from the cipher context,
 * writing it to bfd.
 * Return value is true on success, false on failure.
 */
bool flush_cipher(JCR *jcr, BFILE *bfd, int flags, CIPHER_CONTEXT *cipher, uint32_t cipher_block_size)
{
   uint32_t decrypted_len;

   /* Write out the remaining block and free the cipher context */
   jcr->crypto_buf = check_pool_memory_size(jcr->crypto_buf, cipher_block_size);

   if (!crypto_cipher_finalize(cipher, (uint8_t *)jcr->crypto_buf, &decrypted_len)) {
      /* Writing out the final, buffered block failed. Shouldn't happen. */
      Jmsg1(jcr, M_FATAL, 0, _("Decryption error for %s\n"), jcr->last_fname);
   }

   if (flags & FO_WIN32DECOMP) {
      if (!processWin32BackupAPIBlock(bfd, jcr->crypto_buf, decrypted_len)) {
         berrno be;
         Jmsg2(jcr, M_ERROR, 0, _("Write error in Win32 Block Decomposition on %s: %s\n"), 
               jcr->last_fname, be.strerror(bfd->berrno));
         return false;
      }
   } else if (bwrite(bfd, jcr->crypto_buf, decrypted_len) != (ssize_t)decrypted_len) {
      berrno be;
      Jmsg2(jcr, M_ERROR, 0, _("Write error on %s: %s\n"), 
            jcr->last_fname, be.strerror(bfd->berrno));
      return false;
   }

   return true;
}
