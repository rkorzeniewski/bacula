/*
 * Properties we use for getting and setting ACLs.
 */

#ifndef _BACULA_ACL_
#define _BACULA_ACL_

/* If you want shorter ACL strings when possible, uncomment this */
#define BACL_WANT_SHORT_ACLS

/* If you want numeric user/group ids when possible, uncomment this */
/* #define BACL_WANT_NUMERIC_IDS */

/* We support the following types of ACLs */
#define BACL_TYPE_NONE	      0x000
#define BACL_TYPE_ACCESS      0x001
#define BACL_TYPE_DEFAULT     0x002

#define BACL_CAP_NONE	      0x000    /* No special capabilities */
#define BACL_CAP_DEFAULTS     0x001    /* Has default ACLs for directories */
#define BACL_CAP_DEFAULTS_DIR 0x002    /* Default ACLs must be read separately */

/* Set capabilities for various OS */
#if defined(HAVE_SUN_OS)
#define BACL_CAP	      BACL_CAP_DEFAULTS
#elif defined(HAVE_FREEBSD_OS) \
   || defined(HAVE_IRIX_OS) \
   || defined(HAVE_OSF1_OS) \
   || defined(HAVE_LINUX_OS)
#define BACL_CAP	      (BACL_CAP_DEFAULTS|BACL_CAP_DEFAULTS_DIR)
#else
#define BACL_CAP	      BACL_CAP_NONE
#endif

#endif
/*
 * Properties we use for getting and setting ACLs.
 */

#ifndef _BACULA_ACL_
#define _BACULA_ACL_

/* If you want shorter ACL strings when possible, uncomment this */
#define BACL_WANT_SHORT_ACLS

/* If you want numeric user/group ids when possible, uncomment this */
/* #define BACL_WANT_NUMERIC_IDS */

/* We support the following types of ACLs */
#define BACL_TYPE_NONE	      0x000
#define BACL_TYPE_ACCESS      0x001
#define BACL_TYPE_DEFAULT     0x002

#define BACL_CAP_NONE	      0x000    /* No special capabilities */
#define BACL_CAP_DEFAULTS     0x001    /* Has default ACLs for directories */
#define BACL_CAP_DEFAULTS_DIR 0x002    /* Default ACLs must be read separately */

/* Set capabilities for various OS */
#if defined(HAVE_SUN_OS)
#define BACL_CAP	      BACL_CAP_DEFAULTS
#elif defined(HAVE_FREEBSD_OS) \
   || defined(HAVE_IRIX_OS) \
   || defined(HAVE_OSF1_OS) \
   || defined(HAVE_LINUX_OS)
#define BACL_CAP	      (BACL_CAP_DEFAULTS|BACL_CAP_DEFAULTS_DIR)
#else
#define BACL_CAP	      BACL_CAP_NONE
#endif

#endif
