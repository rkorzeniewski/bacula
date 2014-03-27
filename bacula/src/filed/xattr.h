/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2004-2014 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from many
   others, a complete list can be found in the file AUTHORS.

   You may use this file and others of this release according to the
   license defined in the LICENSE file, which includes the Affero General
   Public License, v3.0 ("AGPLv3") and some additional permissions and
   terms pursuant to its AGPLv3 Section 7.

   Bacula® is a registered trademark of Kern Sibbald.
*/

#ifndef __XATTR_H
#define __XATTR_H

#if defined(HAVE_LINUX_OS)
#define BXATTR_ENOTSUP EOPNOTSUPP
#elif defined(HAVE_DARWIN_OS)
#define BXATTR_ENOTSUP ENOTSUP
#elif defined(HAVE_HURD_OS)
#define BXATTR_ENOTSUP ENOTSUP
#endif

/*
 * Magic used in the magic field of the xattr struct.
 * This way we can see we encounter a valid xattr struct.
 */
#define XATTR_MAGIC 0x5C5884

/*
 * Internal representation of an extended attribute.
 */
struct xattr_t {
   uint32_t magic;
   uint32_t name_length;
   char *name;
   uint32_t value_length;
   char *value;
};

/*
 * Internal representation of an extended attribute hardlinked file.
 */
struct xattr_link_cache_entry_t {
   uint32_t inum;
   char *target;
};

#define BXATTR_FLAG_SAVE_NATIVE    0x01
#define BXATTR_FLAG_RESTORE_NATIVE 0x02

struct xattr_build_data_t {
   uint32_t nr_errors;
   uint32_t nr_saved;
   POOLMEM *content;
   uint32_t content_length;
   alist *link_cache;
};

struct xattr_parse_data_t {
   uint32_t nr_errors;
};

/*
 * Internal tracking data.
 */
struct xattr_data_t {
   uint32_t flags;              /* See BXATTR_FLAG_* */
   uint32_t current_dev;
   union {
      struct xattr_build_data_t *build;
      struct xattr_parse_data_t *parse;
   } u;
};

/*
 * Maximum size of the XATTR stream this prevents us from blowing up the filed.
 */
#define MAX_XATTR_STREAM  (1 * 1024 * 1024) /* 1 Mb */

/*
 * Upperlimit on a xattr internal buffer
 */
#define XATTR_BUFSIZ	1024

#endif
