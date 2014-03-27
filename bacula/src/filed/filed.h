/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2001-2014 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from many
   others, a complete list can be found in the file AUTHORS.

   You may use this file and others of this release according to the
   license defined in the LICENSE file, which includes the Affero General
   Public License, v3.0 ("AGPLv3") and some additional permissions and
   terms pursuant to its AGPLv3 Section 7.

   Bacula® is a registered trademark of Kern Sibbald.
*/
/*
 * Bacula File Daemon specific configuration and defines
 *
 *     Kern Sibbald, Jan MMI
 *
 */

/*
 * Number of acl errors to report per job.
 */
#define ACL_REPORT_ERR_MAX_PER_JOB      25

/*
 * Number of xattr errors to report per job.
 */
#define XATTR_REPORT_ERR_MAX_PER_JOB    25

/*
 * Return codes from acl subroutines.
 */
typedef enum {
   bacl_exit_fatal = -1,
   bacl_exit_error = 0,
   bacl_exit_ok = 1
} bacl_exit_code;

/*
 * Return codes from xattr subroutines.
 */
typedef enum {
   bxattr_exit_fatal = -1,
   bxattr_exit_error = 0,
   bxattr_exit_ok = 1
} bxattr_exit_code;

#define FILE_DAEMON 1
#include "lib/htable.h"
#include "filed_conf.h"
#include "fd_plugins.h"
#include "findlib/find.h"
#include "acl.h"
#include "xattr.h"
#include "jcr.h"
#include "protos.h"                   /* file daemon prototypes */
#include "lib/runscript.h"
#include "lib/breg.h"
#ifdef HAVE_LIBZ
#include <zlib.h>                     /* compression headers */
#else
#define uLongf uint32_t
#endif
#ifdef HAVE_LZO
#include <lzo/lzoconf.h>
#include <lzo/lzo1x.h>
#endif

extern CLIENT *me;                    /* "Global" Client resource */
extern bool win32decomp;              /* Use decomposition of BackupRead data */
extern bool no_win32_write_errors;    /* Ignore certain errors */

void terminate_filed(int sig);

struct s_cmds {
   const char *cmd;
   int (*func)(JCR *);
   int monitoraccess; /* specify if monitors have access to this function */
};

void allow_os_suspensions();
void prevent_os_suspensions();
