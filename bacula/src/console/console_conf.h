/*
 * Bacula User Agent specific configuration and defines
 *
 *     Kern Sibbald, Sep MM
 *
 *     Version $Id$
 */

/*
 * Resource codes -- they must be sequential for indexing
 */

enum {
   R_CONSOLE   = 1001,
   R_DIRECTOR,
   R_FIRST     = R_CONSOLE,
   R_LAST      = R_DIRECTOR	      /* Keep this updated */
};

/*
 * Some resource attributes
 */
enum {
   R_NAME     = 1020,
   R_ADDRESS,
   R_PASSWORD,
   R_TYPE,
   R_BACKUP
};


/* Definition of the contents of each Resource */

/* Console "globals" */
struct CONRES {
   RES	 hdr;
   char *rc_file;		      /* startup file */
   char *hist_file;		      /* command history file */
   char *password;		      /* UA server password */
#ifdef HAVE_TLS
   int tls_enable;                    /* Enable TLS on all connections */
   int tls_require;                   /* Require TLS on all connections */
   char *tls_ca_certfile;             /* TLS CA Certificate File */
   char *tls_ca_certdir;              /* TLS CA Certificate Directory */
   char *tls_certfile;                /* TLS Client Certificate File */
   char *tls_keyfile;                 /* TLS Client Key File */

   TLS_CONTEXT *tls_ctx;              /* Shared TLS Context */
#endif /* HAVE_TLS */
};

/* Director */
struct DIRRES {
   RES	 hdr;
   int	 DIRport;		      /* UA server port */
   char *address;		      /* UA server address */
   char *password;		      /* UA server password */
#ifdef HAVE_TLS
   int tls_enable;                    /* Enable TLS */
   int tls_require;                   /* Require TLS */
   char *tls_ca_certfile;             /* TLS CA Certificate File */
   char *tls_ca_certdir;              /* TLS CA Certificate Directory */
   char *tls_certfile;                /* TLS Client Certificate File */
   char *tls_keyfile;                 /* TLS Client Key File */

   TLS_CONTEXT *tls_ctx;              /* Shared TLS Context */
#endif /* HAVE_TLS */
};


/* Define the Union of all the above
 * resource structure definitions.
 */
union URES {
   DIRRES res_dir;
   CONRES res_cons;
   RES hdr;
};
