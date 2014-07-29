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
 * Tray Monitor specific configuration and defines
 *
 *   Adapted from dird_conf.c
 *
 *     Nicolas Boichat, August MMIV
 *
 */

/* NOTE:  #includes at the end of this file */

/*
 * Resource codes -- they must be sequential for indexing
 */
enum rescode {
   R_MONITOR = 1001,
   R_DIRECTOR,
   R_CLIENT,
   R_STORAGE,
   R_CONSOLE_FONT,
   R_FIRST = R_MONITOR,
   R_LAST  = R_CONSOLE_FONT                /* keep this updated */
};


/*
 * Some resource attributes
 */
enum {
   R_NAME = 1020,
   R_ADDRESS,
   R_PASSWORD,
   R_TYPE,
   R_BACKUP
};

/* Director */
struct DIRRES {
   RES   hdr;
   uint32_t DIRport;                  /* UA server port */
   char *address;                     /* UA server address */
   bool enable_ssl;                   /* Use SSL */
};

/*
 *   Tray Monitor Resource
 *
 */
struct MONITOR {
   RES   hdr;
   bool require_ssl;                  /* Require SSL for all connections */
   MSGS *messages;                    /* Daemon message handler */
   char *password;                    /* UA server password */
   utime_t RefreshInterval;           /* Status refresh interval */
   utime_t FDConnectTimeout;          /* timeout for connect in seconds */
   utime_t SDConnectTimeout;          /* timeout in seconds */
   utime_t DIRConnectTimeout;         /* timeout in seconds */
};


/*
 *   Client Resource
 *
 */
struct CLIENT {
   RES   hdr;

   uint32_t FDport;                   /* Where File daemon listens */
   char *address;
   char *password;
   bool enable_ssl;                   /* Use SSL */
};

/*
 *   Store Resource
 *
 */
struct STORE {
   RES   hdr;

   uint32_t SDport;                   /* port where Directors connect */
   char *address;
   char *password;
   bool enable_ssl;                   /* Use SSL */
};

struct CONFONTRES {
   RES   hdr;
   char *fontface;                    /* Console Font specification */
};

/* Define the Union of all the above
 * resource structure definitions.
 */
union URES {
   MONITOR    res_monitor;
   DIRRES     res_dir;
   CLIENT     res_client;
   STORE      res_store;
   CONFONTRES con_font;
   RES        hdr;
};
