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
#define R_FIRST                       1001

#define R_CONSOLE                     1001
#define R_DIRECTOR                    1002

#define R_LAST                        R_DIRECTOR

/*
 * Some resource attributes
 */
#define R_NAME                        1020
#define R_ADDRESS                     1021
#define R_PASSWORD                    1022
#define R_TYPE                        1023
#define R_BACKUP                      1024


/* Definition of the contents of each Resource */

/* Console "globals" */
struct s_res_con {
   RES   hdr;
   char *rc_file;                     /* startup file */
   char *hist_file;                   /* command history file */
   int require_ssl;                   /* Require SSL on all connections */
   char *password;                    /* UA server password */
};
typedef struct s_res_con CONRES;

/* Director */
struct s_res_dir {
   RES   hdr;
   int   DIRport;                     /* UA server port */
   char *address;                     /* UA server address */
   char *password;                    /* UA server password */
   int  enable_ssl;                   /* Use SSL */
};
typedef struct s_res_dir DIRRES;


/* Define the Union of all the above
 * resource structure definitions.
 */
union u_res {
   struct s_res_dir     res_dir;
   struct s_res_con     res_cons;
   RES hdr;
};

typedef union u_res URES;
