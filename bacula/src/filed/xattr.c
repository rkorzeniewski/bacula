/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2008-2008 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from
   many others, a complete list can be found in the file AUTHORS.
   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version two of the GNU General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

   Bacula® is a registered trademark of Kern Sibbald.
   The licensor of Bacula is the Free Software Foundation Europe
   (FSFE), Fiduciary Program, Sumatrastrasse 25, 8006 Zürich,
   Switzerland, email:ftf@fsfeurope.org.
*/
/*
 * Functions to handle Extended Attributes for bacula.
 *
 * Extended Attributes are so OS specific we only restore Extended Attributes if
 * they were saved using a filed on the same platform.
 *
 * Currently we support the following OSes:
 *   - FreeBSD
 *   - Darwin
 *   - Linux
 *   - NetBSD
 *
 *   Written by Marco van Wieringen, November MMVIII
 *
 *   Version $Id: xattr.c 7879 2008-10-23 10:12:36Z kerns $
 */

#include "bacula.h"
#include "filed.h"
#include "xattr.h"

/*
 * List of supported OSs.
 */
#if !defined(HAVE_XATTR)            /* Extended Attributes support is required, of course */ \
   || !( defined(HAVE_DARWIN_OS) \
      || defined(HAVE_FREEBSD_OS) \
      || defined(HAVE_LINUX_OS) \
      || defined(HAVE_NETBSD_OS) \
        )

bool build_xattr_streams(JCR *jcr, FF_PKT *ff_pkt)
{
   Jmsg(jcr, M_FATAL, 0, _("XATTR support not configured for your machine.\n"));
   return false;
}

bool parse_xattr_stream(JCR *jcr, int stream)
{
   Jmsg(jcr, M_FATAL, 0, _("XATTR support not configured for your machine.\n"));
   return false;
}

#else

#ifdef HAVE_SYS_XATTR_H
#include <sys/xattr.h>
#endif

#if defined(HAVE_GETXATTR) && !defined(HAVE_LGETXATTR)
#define lgetxattr getxattr
#endif
#if defined(HAVE_SETXATTR) && !defined(HAVE_LSETXATTR)
#define lsetxattr setxattr
#endif
#if defined(HAVE_LISTXATTR) && !defined(HAVE_LLISTXATTR)
#define llistxattr listxattr
#endif

/*
 * Send a XATTR stream to the SD.
 */
static bool send_xattr_stream(JCR *jcr, int stream, int len)
{
   BSOCK *sd = jcr->store_bsock;
   POOLMEM *msgsave;
#ifdef FD_NO_SEND_TEST
   return true;
#endif

   /*
    * Send header
    */
   if (!sd->fsend("%ld %d 0", jcr->JobFiles, stream)) {
      Jmsg1(jcr, M_FATAL, 0, _("Network send error to SD. ERR=%s\n"),
            sd->bstrerror());

      return false;
   }

   /*
    * Send the buffer to the storage deamon
    */
   Dmsg1(400, "Backing up XATTR <%s>\n", jcr->xattr_data);
   msgsave = sd->msg;
   sd->msg = jcr->xattr_data;
   sd->msglen = len;
   if (!sd->send()) {
      sd->msg = msgsave;
      sd->msglen = 0;
      Jmsg1(jcr, M_FATAL, 0, _("Network send error to SD. ERR=%s\n"),
            sd->bstrerror());

      return false;
   }

   jcr->JobBytes += sd->msglen;
   sd->msg = msgsave;
   if (!sd->signal(BNET_EOD)) {
      Jmsg1(jcr, M_FATAL, 0, _("Network send error to SD. ERR=%s\n"),
            sd->bstrerror());

      return false;
   }

   Dmsg1(200, "XATTR of file: %s successfully backed up!\n", jcr->last_fname);

   return true;
}

static void xattr_drop_internal_table(xattr_t *xattr_value_list)
{
   xattr_t *current_xattr;

   /*
    * Walk the list of xattrs and free allocated memory on traversing.
    */
   for (current_xattr = xattr_value_list;
        current_xattr != (xattr_t *)NULL;
        current_xattr++) {
      /*
       * See if we can shortcut.
       */
      if (current_xattr->magic != XATTR_MAGIC)
         break;

      free(current_xattr->name);

      if (current_xattr->value_length > 0)
         free(current_xattr->value);
   }

   /*
    * Free the array of control structs.
    */
   free(xattr_value_list);
}


static uint32_t serialize_xattr_stream(JCR *jcr, uint32_t expected_serialize_len, xattr_t *xattr_value_list)
{
   xattr_t *current_xattr;
   ser_declare;

   /*
    * Make sure the serialized stream fits in the poolmem buffer.
    * We allocate some more to be sure the stream is gonna fit.
    */
   jcr->xattr_data = check_pool_memory_size(jcr->xattr_data, expected_serialize_len + 10);
   ser_begin(jcr->xattr_data, expected_serialize_len + 10);

   /*
    * Walk the list of xattrs and serialize the data.
    */
   for (current_xattr = xattr_value_list; current_xattr != (xattr_t *)NULL; current_xattr++) {
      /*
       * See if we can shortcut.
       */
      if (current_xattr->magic != XATTR_MAGIC)
         break;

      ser_uint32(current_xattr->magic);
      ser_uint32(current_xattr->name_length);
      ser_bytes(current_xattr->name, current_xattr->name_length);

      ser_uint32(current_xattr->value_length);
      ser_bytes(current_xattr->value, current_xattr->value_length);
   }

   ser_end(jcr->xattr_data, expected_serialize_len + 10);

   return ser_length(jcr->xattr_data);
}

static bool generic_xattr_build_streams(JCR *jcr, FF_PKT *ff_pkt, int stream)
{
   int count = 0;
   int32_t xattr_list_len,
           xattr_value_len,
           expected_serialize_len = 0,
           serialize_len = 0;
   char *xattr_list, *bp;
   xattr_t *xattr_value_list, *current_xattr;

   /*
    * First get the length of the available list with extended attributes.
    */
   xattr_list_len = llistxattr(jcr->last_fname, NULL, 0);
   if (xattr_list_len < 0) {
      berrno be;
      Jmsg2(jcr, M_ERROR, 0, _("llistxattr error on file \"%s\": ERR=%s\n"),
         jcr->last_fname, be.bstrerror());
      Dmsg2(100, "llistxattr error file=%s ERR=%s\n",
         jcr->last_fname, be.bstrerror());
      return false;
   } else if (xattr_list_len == 0) {
      return true;
   }

   /*
    * Allocate room for the extented attribute list.
    */
   if ((xattr_list = (char *)malloc(xattr_list_len + 1)) == (char *)NULL) {
      Emsg1(M_ABORT, 0, _("Out of memory requesting %d bytes\n"), xattr_list_len + 1);

      return false;
   }
   memset((caddr_t)xattr_list, 0, xattr_list_len + 1);

   /*
    * Get the actual list of extended attributes names for a file.
    */
   xattr_list_len = llistxattr(jcr->last_fname, xattr_list, xattr_list_len);
   if (xattr_list_len < 0) {
      berrno be;
      Jmsg2(jcr, M_ERROR, 0, _("llistxattr error on file \"%s\": ERR=%s\n"),
         jcr->last_fname, be.bstrerror());
      Dmsg2(100, "llistxattr error file=%s ERR=%s\n",
         jcr->last_fname, be.bstrerror());

      free(xattr_list);
      return false;
   }
   xattr_list[xattr_list_len] = '\0';

   /*
    * Count the number of extended attributes on a file.
    */
   bp = xattr_list;
   while ((bp - xattr_list) + 1 < xattr_list_len) {
      count++;

      bp = strchr(bp, '\0') + 1;
   }

   /*
    * Allocate enough room to hold all extended attributes.
    * After allocating the storage make sure its empty by zeroing it.
    */
   if ((xattr_value_list = (xattr_t *)malloc(count * sizeof(xattr_t))) == (xattr_t *)NULL) {
      Emsg1(M_ABORT, 0, _("Out of memory requesting %d bytes\n"), count * sizeof(xattr_t));

      return false;
   }
   memset((caddr_t)xattr_value_list, 0, count * sizeof(xattr_t));

   /*
    * Walk the list of extended attributes names and retrieve the data.
    * We already count the bytes needed for serializing the stream later on.
    */
   current_xattr = xattr_value_list;
   bp = xattr_list;
   while ((bp - xattr_list) + 1 < xattr_list_len) {
#if defined(HAVE_LINUX_OS)
      /*
       * On Linux you also get the acls in the extented attribute list.
       * So we check if we are already backing up acls and if we do we
       * don't store the extended attribute with the same info.
       */
      if (ff_pkt->flags & FO_ACL && !strcmp(bp, "system.posix_acl_access")) {
         bp = strchr(bp, '\0') + 1;

         continue;
      }
#endif

      /*
       * Each xattr valuepair starts with a magic so we can parse it easier.
       */
      current_xattr->magic = XATTR_MAGIC;
      expected_serialize_len += sizeof(current_xattr->magic);

      /*
       * Allocate space for storing the name.
       */
      current_xattr->name_length = strlen(bp);
      if ((current_xattr->name = (char *)malloc(current_xattr->name_length)) == (char *)NULL) {
         Emsg1(M_ABORT, 0, _("Out of memory requesting %d bytes\n"), current_xattr->name_length);

         goto bail_out;
      }
      memcpy((caddr_t)current_xattr->name, (caddr_t)bp, current_xattr->name_length);

      expected_serialize_len += sizeof(current_xattr->name_length) + current_xattr->name_length;

      /*
       * First see how long the value is for the extended attribute.
       */
      xattr_value_len = lgetxattr(jcr->last_fname, bp, NULL, 0);
      if (xattr_value_len < 0) {
         berrno be;
         Jmsg2(jcr, M_ERROR, 0, _("lgetxattr error on file \"%s\": ERR=%s\n"),
            jcr->last_fname, be.bstrerror());
         Dmsg2(100, "lgetxattr error file=%s ERR=%s\n",
            jcr->last_fname, be.bstrerror());

         goto bail_out;
      }

      /*
       * Allocate space for storing the value.
       */
      if ((current_xattr->value = (char *)malloc(xattr_value_len)) == (char *)NULL) {
         Emsg1(M_ABORT, 0, _("Out of memory requesting %d bytes\n"), xattr_value_len);

         goto bail_out;
      }
      memset((caddr_t)current_xattr->value, 0, xattr_value_len);

      xattr_value_len = lgetxattr(jcr->last_fname, bp, current_xattr->value, xattr_value_len);
      if (xattr_value_len < 0) {
         berrno be;
         Jmsg2(jcr, M_ERROR, 0, _("lgetxattr error on file \"%s\": ERR=%s\n"),
            jcr->last_fname, be.bstrerror());
         Dmsg2(100, "lgetxattr error file=%s ERR=%s\n",
            jcr->last_fname, be.bstrerror());

         goto bail_out;
      }

      /*
       * Store the actual length of the value.
       */
      current_xattr->value_length = xattr_value_len;

      expected_serialize_len += sizeof(current_xattr->value_length) + current_xattr->value_length;
      
      /*
       * Next attribute.
       */
      current_xattr++;
      bp = strchr(bp, '\0') + 1;
   }

   /*
    * Serialize the datastream.
    */
   if ((serialize_len = serialize_xattr_stream(jcr, expected_serialize_len,
         xattr_value_list)) < expected_serialize_len) {
      Jmsg1(jcr, M_ERROR, 0, _("failed to serialize extended attributes on file \"%s\"\n"),
         jcr->last_fname);

      goto bail_out;
   }

   xattr_drop_internal_table(xattr_value_list);
   free(xattr_list);

   /*
    * Send the datastream to the SD.
    */
   return send_xattr_stream(jcr, stream, serialize_len);

bail_out:
   xattr_drop_internal_table(xattr_value_list);
   free(xattr_list);

   return false;
}

static bool generic_xattr_parse_streams(JCR *jcr)
{
   unser_declare;
   xattr_t current_xattr;
   bool retval = true;

   /*
    * Parse the stream and perform the setxattr calls on the file.
    *
    * Start unserializing the data. We keep on looping while we have not
    * unserialized all bytes in the stream.
    */
   unser_begin(jcr->xattr_data, jcr->xattr_data_len);
   while (unser_length(jcr->xattr_data) < jcr->xattr_data_len) {
      /*
       * First make sure the magic is present. This way we can easily catch corruption.
       * Any missing MAGIC is fatal we do NOT try to continue.
       */
      unser_uint32(current_xattr.magic);
      if (current_xattr.magic != XATTR_MAGIC) {
         Jmsg1(jcr, M_ERROR, 0, _("Illegal xattr stream, no XATTR_MAGIC on file \"%s\"\n"),
            jcr->last_fname);

         return false;
      }

      /*
       * Decode the valuepair. First decode the length of the name.
       */
      unser_uint32(current_xattr.name_length);
      
      /*
       * Allocate room for the name and decode its content.
       */
      if ((current_xattr.name = (char *)malloc(current_xattr.name_length + 1)) == (char *)NULL) {
         Emsg1(M_ABORT, 0, _("Out of memory requesting %d bytes\n"), current_xattr.name_length + 1);

         return false;
      }
      unser_bytes(current_xattr.name, current_xattr.name_length);

      /*
       * The xattr_name needs to be null terminated for lsetxattr.
       */
      current_xattr.name[current_xattr.name_length] = '\0';

      /*
       * Decode the value length.
       */
      unser_uint32(current_xattr.value_length);

      /*
       * Allocate room for the value and decode its content.
       */
      if ((current_xattr.value = (char *)malloc(current_xattr.value_length)) == (char *)NULL) {
         Emsg1(M_ABORT, 0, _("Out of memory requesting %d bytes\n"), current_xattr.value_length);

         return false;
      }
      unser_bytes(current_xattr.value, current_xattr.value_length);

      /*
       * Try to set the extended attribute on the file.
       * If we fail to set this attribute we flag the error but its not fatal,
       * we try to restore the other extended attributes too.
       */
      if (lsetxattr(jcr->last_fname, current_xattr.name, current_xattr.value,
         current_xattr.value_length, 0) != 0) {
         berrno be;
         Jmsg2(jcr, M_ERROR, 0, _("lsetxattr error on file \"%s\": ERR=%s\n"),
            jcr->last_fname, be.bstrerror());
         Dmsg2(100, "lsetxattr error file=%s ERR=%s\n",
            jcr->last_fname, be.bstrerror());

         /*
          * Reset the return flag to false to indicate one or more extended attributes
          * could not be restored.
          */
         retval = false;
      }

      /*
       * Free the temporary buffers.
       */
      free(current_xattr.name);
      free(current_xattr.value);
   }

   unser_end(jcr->xattr_data, jcr->xattr_data_len);
   return retval;
}

#if defined(HAVE_DARWIN_OS)
static bool darwin_build_xattr_streams(JCR *jcr, FF_PKT *ff_pkt)
{
   return generic_xattr_build_streams(jcr, ff_pkt, STREAM_XATTR_DARWIN);
}

static bool darwin_parse_xattr_stream(JCR *jcr, int stream)
{
   switch (stream) {
   case STREAM_XATTR_DARWIN:
      return generic_xattr_parse_streams(jcr);
   }
}
#elif defined(HAVE_FREEBSD_OS)
static bool freebsd_build_xattr_streams(JCR *jcr, FF_PKT *ff_pkt)
{
   return generic_xattr_build_streams(jcr, ff_pkt, STREAM_XATTR_FREEBSD);
}

static bool freebsd_parse_xattr_stream(JCR *jcr, int stream)
{
   switch (stream) {
   case STREAM_XATTR_FREEBSD:
      return generic_xattr_parse_streams(jcr);
   }
}
#elif defined(HAVE_LINUX_OS)
static bool linux_build_xattr_streams(JCR *jcr, FF_PKT *ff_pkt)
{
   return generic_xattr_build_streams(jcr, ff_pkt, STREAM_XATTR_LINUX);
}

static bool linux_parse_xattr_stream(JCR *jcr, int stream)
{
   switch (stream) {
   case STREAM_XATTR_LINUX:
      return generic_xattr_parse_streams(jcr);
   }
   return false;
}
#elif defined(HAVE_NETBSD_OS)
static bool netbsd_build_xattr_streams(JCR *jcr, FF_PKT *ff_pkt)
{
   return generic_xattr_build_streams(jcr, ff_pkt, STREAM_XATTR_NETBSD);
}

static bool netbsd_parse_xattr_stream(JCR *jcr, int stream)
{
   switch (stream) {
   case STREAM_XATTR_NETBSD:
      return generic_xattr_parse_streams(jcr);
   }
   return false;
}
#endif

bool build_xattr_streams(JCR *jcr, FF_PKT *ff_pkt)
{
#if defined(HAVE_DARWIN_OS)
   return darwin_build_xattr_streams(jcr, ff_pkt);
#elif defined(HAVE_FREEBSD_OS)
   return freebsd_build_xattr_streams(jcr, ff_pkt);
#elif defined(HAVE_LINUX_OS)
   return linux_build_xattr_streams(jcr, ff_pkt);
#elif defined(HAVE_NETBSD_OS)
   return netbsd_build_xattr_streams(jcr, ff_pkt);
#endif
}

bool parse_xattr_stream(JCR *jcr, int stream)
{
   /*
    * Based on the stream being passed in dispatch to the right function
    * for parsing and restoring a specific xattr. The platform determines
    * which streams are recognized and parsed and which are handled by
    * the default case and ignored. As only one of the platform defines
    * is true per compile we never end up with duplicate switch values.
    */
   switch (stream) {
#if defined(HAVE_DARWIN_OS)
   case STREAM_XATTR_DARWIN:
      return darwin_parse_xattr_stream(jcr, stream);
#elif defined(HAVE_FREEBSD_OS)
   case STREAM_XATTR_FREEBSD:
      return freebsd_parse_xattr_stream(jcr, stream);
#elif defined(HAVE_LINUX_OS)
   case STREAM_XATTR_LINUX:
      return linux_parse_xattr_stream(jcr, stream);
#elif defined(HAVE_NETBSD_OS)
   case STREAM_XATTR_NETBSD:
      return netbsd_parse_xattr_stream(jcr, stream);
#endif
   default:
      /*
       * Issue a warning and discard the message. But pretend the restore was ok.
       */
      Qmsg2(jcr, M_WARNING, 0,
         _("Can't restore Extended Attributes of %s - incompatible xattr stream encountered - %d\n"),
         jcr->last_fname, stream);
      return true;
   } /* end switch (stream) */
}

#endif
