/*
 *
 *  Program to test batch mode
 *
 *   Eric Bollengier, March 2007
 *
 *
 *   Version $Id$
 */
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
  to create datafile

  for i in $(seq 10000 99999) ; do
     j=$((($i % 1000) + 555))
     echo "$i;/tmp/totabofds$j/fiddddle${j}$i;xxxLSTATxxxx;xxxxxxxMD5xxxxxx"
  done  > dat1

  or

  j=0
  find / | while read a; do
   j=$(($j+1))
   echo "$j;$a;xxxLSTATxxxx;xxxxxxxMD5xxxxxx"
  done > dat1
 */

#include "bacula.h"
#include "stored/stored.h"
#include "findlib/find.h"
#include "cats/cats.h"
#include "cats/sql_glue.h"

/* Forward referenced functions */
static void *do_batch(void *);


/* Local variables */
static B_DB *db;

static const char *db_name = "bacula";
static const char *db_user = "bacula";
static const char *db_password = "";
static const char *db_host = NULL;

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

static void usage()
{
   fprintf(stderr, _(
PROG_COPYRIGHT
"\nVersion: %s (%s)\n"
"Example : bbatch -w /path/to/workdir -h localhost -f dat1 -f dat -f datx\n"
" will start 3 thread and load dat1, dat and datx in your catalog\n"
"See bbatch.c to generate datafile\n\n"
"Usage: bbatch [ options ] -w working/dir -f datafile\n"
"       -b                with batch mode\n"
"       -B                without batch mode\n"
"       -d <nn>           set debug level to <nn>\n"
"       -dt               print timestamp in debug output\n"
"       -n <name>         specify the database name (default bacula)\n"
"       -u <user>         specify database user name (default bacula)\n"
"       -P <password      specify database password (default none)\n"
"       -h <host>         specify database host (default NULL)\n"
"       -w <working>      specify working directory\n"
"       -r <jobids>       call restore code with given jobids\n"
"       -v                verbose\n"
"       -f <file>         specify data file\n"
"       -?                print this message\n\n"), 2001, VERSION, BDATE);
   exit(1);
}

/* number of thread started */
int nb=0;

static int list_handler(void *ctx, int num_fields, char **row)
{
   uint64_t *a = (uint64_t*) ctx;
   (*a)++;
   return 0;
}

int main (int argc, char *argv[])
{
   int ch;
   bool disable_batch = false;
   char *restore_list=NULL;
   setlocale(LC_ALL, "");
   bindtextdomain("bacula", LOCALEDIR);
   textdomain("bacula");
   init_stack_dump();
   lmgr_init_thread();

   char **files = (char **) malloc (10 * sizeof(char *));
   int i;
   my_name_is(argc, argv, "bbatch");
   init_msg(NULL, NULL);

   OSDependentInit();

   while ((ch = getopt(argc, argv, "bBh:c:d:n:P:Su:vf:w:r:?")) != -1) {
      switch (ch) {
      case 'r':
         restore_list=bstrdup(optarg);
         break;
      case 'B':
         disable_batch = true;
         break;
      case 'b':
         disable_batch = false;
         break;
      case 'd':                    /* debug level */
         if (*optarg == 't') {
            dbg_timestamp = true;
         } else {
            debug_level = atoi(optarg);
            if (debug_level <= 0) {
               debug_level = 1;
            }
         }
         break;

      case 'h':
         db_host = optarg;
         break;

      case 'n':
         db_name = optarg;
         break;

      case 'w':
         working_directory = optarg;
         break;

      case 'u':
         db_user = optarg;
         break;

      case 'P':
         db_password = optarg;
         break;

      case 'v':
         verbose++;
         break;

      case 'f':
         if (nb < 10 ) {
            files[nb++] = optarg;
         }
         break;

      case '?':
      default:
         usage();

      }
   }
   argc -= optind;
   argv += optind;

   if (argc != 0) {
      Pmsg0(0, _("Wrong number of arguments: \n"));
      usage();
   }

   if (restore_list) {
      uint64_t nb_file=0;
      btime_t start, end;
      /* To use the -r option, the catalog should already contains records */

      if ((db = db_init_database(NULL, NULL, db_name, db_user, db_password,
                                 db_host, 0, NULL, false, disable_batch)) == NULL) {
         Emsg0(M_ERROR_TERM, 0, _("Could not init Bacula database\n"));
      }
      if (!db_open_database(NULL, db)) {
         Emsg0(M_ERROR_TERM, 0, db_strerror(db));
      }

      start = get_current_btime();
      db_get_file_list(NULL, db, restore_list, false, false, list_handler, &nb_file);
      end = get_current_btime();

      Pmsg3(0, _("Computing file list for jobid=%s files=%lld secs=%d\n"),
            restore_list, nb_file, (uint32_t)btime_to_unix(end-start));

      free(restore_list);
      return 0;
   }

   if (disable_batch) {
      printf("Without new Batch mode\n");
   } else {
      printf("With new Batch mode\n");
   }

   i = nb;
   while (--i >= 0) {
      pthread_t thid;
      JCR *bjcr = new_jcr(sizeof(JCR), NULL);
      bjcr->bsr = NULL;
      bjcr->VolSessionId = 1;
      bjcr->VolSessionTime = (uint32_t)time(NULL);
      bjcr->NumReadVolumes = 0;
      bjcr->NumWriteVolumes = 0;
      bjcr->JobId = getpid();
      bjcr->setJobType(JT_CONSOLE);
      bjcr->setJobLevel(L_FULL);
      bjcr->JobStatus = JS_Running;
      bjcr->where = bstrdup(files[i]);
      bjcr->job_name = get_pool_memory(PM_FNAME);
      pm_strcpy(bjcr->job_name, "Dummy.Job.Name");
      bjcr->client_name = get_pool_memory(PM_FNAME);
      pm_strcpy(bjcr->client_name, "Dummy.Client.Name");
      bstrncpy(bjcr->Job, "bbatch", sizeof(bjcr->Job));
      bjcr->fileset_name = get_pool_memory(PM_FNAME);
      pm_strcpy(bjcr->fileset_name, "Dummy.fileset.name");
      bjcr->fileset_md5 = get_pool_memory(PM_FNAME);
      pm_strcpy(bjcr->fileset_md5, "Dummy.fileset.md5");

      if ((db = db_init_database(NULL, NULL, db_name, db_user, db_password,
                                 db_host, 0, NULL, false, false)) == NULL) {
         Emsg0(M_ERROR_TERM, 0, _("Could not init Bacula database\n"));
      }
      if (!db_open_database(NULL, db)) {
         Emsg0(M_ERROR_TERM, 0, db_strerror(db));
      }
      Dmsg0(200, "Database opened\n");
      if (verbose) {
         Pmsg2(000, _("Using Database: %s, User: %s\n"), db_name, db_user);
      }

      bjcr->db = db;

      pthread_create(&thid, NULL, do_batch, bjcr);
   }

   while (nb > 0) {
      bmicrosleep(1,0);
   }

   return 0;
}

static void fill_attr(ATTR_DBR *ar, char *data)
{
   char *p;
   char *b;
   int index=0;
   ar->Stream = STREAM_UNIX_ATTRIBUTES;
   ar->JobId = getpid();

   for(p = b = data; *p; p++) {
      if (*p == ';') {
         *p = '\0';
         switch (index) {
         case 0:
            ar->FileIndex = str_to_int64(b);
            break;
         case 1:
            ar->fname = b;
            break;
         case 2:
            ar->attr = b;
            break;
         case 3:
            ar->Digest = b;
            break;
         }
         index++;
         b = ++p;
      }
   }
}

static void *do_batch(void *jcr)
{
   JCR *bjcr = (JCR *)jcr;
   char data[1024];
   int lineno = 0;
   struct ATTR_DBR ar;
   memset(&ar, 0, sizeof(ar));
   btime_t begin = get_current_btime();
   char *datafile = bjcr->where;

   FILE *fd = fopen(datafile, "r");
   if (!fd) {
      Emsg1(M_ERROR_TERM, 0, _("Error opening datafile %s\n"), datafile);
   }
   while (fgets(data, sizeof(data)-1, fd)) {
      strip_trailing_newline(data);
      lineno++;
      if (verbose && ((lineno % 5000) == 1)) {
         printf("\r%i", lineno);
      }
      fill_attr(&ar, data);
      if (!db_create_attributes_record(bjcr, bjcr->db, &ar)) {
         Emsg0(M_ERROR_TERM, 0, _("Error while inserting file\n"));
      }
   }
   fclose(fd);
   db_write_batch_file_records(bjcr);
   btime_t end = get_current_btime();

   P(mutex);
   char ed1[200], ed2[200];
   printf("\rbegin = %s, end = %s\n", edit_int64(begin, ed1),edit_int64(end, ed2));
   printf("Insert time = %sms\n", edit_int64((end - begin) / 10000, ed1));
   printf("Create %u files at %.2f/s\n", lineno,
          (lineno / ((float)((end - begin) / 1000000))));
   nb--;
   V(mutex);
   pthread_exit(NULL);
   return NULL;
}
