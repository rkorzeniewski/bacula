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
/*
 * Properties we use for getting and setting ACLs.
 */

#ifndef __ACL_H
#define __ACL_H

/* For shorter ACL strings when possible, define BACL_WANT_SHORT_ACLS */
/* #define BACL_WANT_SHORT_ACLS */

/* For numeric user/group ids when possible, define BACL_WANT_NUMERIC_IDS */
/* #define BACL_WANT_NUMERIC_IDS */

/*
 * We support the following types of ACLs
 */
typedef enum {
   BACL_TYPE_NONE = 0,
   BACL_TYPE_ACCESS = 1,
   BACL_TYPE_DEFAULT = 2,
   BACL_TYPE_DEFAULT_DIR = 3,
   BACL_TYPE_EXTENDED = 4,
   BACL_TYPE_NFS4 = 5
} bacl_type;

/*
 * This value is used as ostype when we encounter an invalid acl type.
 * The way the code is build this should never happen.
 */
#if !defined(ACL_TYPE_NONE)
#define ACL_TYPE_NONE 0x0
#endif

#if defined(HAVE_FREEBSD_OS) || \
    defined(HAVE_DARWIN_OS) || \
    defined(HAVE_HPUX_OS) || \
    defined(HAVE_LINUX_OS)
#define BACL_ENOTSUP EOPNOTSUPP
#elif defined(HAVE_IRIX_OS)
#define BACL_ENOTSUP ENOSYS
#endif

#define BACL_FLAG_SAVE_NATIVE    0x01
#define BACL_FLAG_SAVE_AFS       0x02
#define BACL_FLAG_RESTORE_NATIVE 0x04
#define BACL_FLAG_RESTORE_AFS    0x08

struct acl_build_data_t {
   uint32_t nr_errors;
   uint32_t content_length;
   POOLMEM *content;
};

struct acl_parse_data_t {
   uint32_t nr_errors;
};

/*
 * Internal tracking data.
 */
struct acl_data_t {
   uint32_t flags;		/* See BACL_FLAG_* */
   uint32_t current_dev;
   union {
      struct acl_build_data_t *build;
      struct acl_parse_data_t *parse;
   } u;
};

#endif
