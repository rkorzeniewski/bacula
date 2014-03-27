/*
 * Program for determining file system type
 *
 *   Written by Preben 'Peppe' Guldberg, December MMIV
 *
 *   Version $Id$
 *
 */
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

#include "bacula.h"
#include "findlib/find.h"
#include "lib/mntent_cache.h"

/* Dummy functions */
int generate_daemon_event(JCR *jcr, const char *event)
   { return 1; }

static void usage()
{
   fprintf(stderr, _(
"\n"
"Usage: fstype [-v] path ...\n"
"\n"
"       Print the file system type a given file/directory is on.\n"
"       The following options are supported:\n"
"\n"
"       -v     print both path and file system type.\n"
"       -?     print this message.\n"
"\n"));

   exit(1);
}


int
main (int argc, char *const *argv)
{
   char fs[1000];
   int verbose = 0;
   int status = 0;
   int ch, i;

   setlocale(LC_ALL, "");
   bindtextdomain("bacula", LOCALEDIR);
   textdomain("bacula");

   while ((ch = getopt(argc, argv, "v?")) != -1) {
      switch (ch) {
         case 'v':
            verbose = 1;
            break;
         case '?':
         default:
            usage();

      }
   }
   argc -= optind;
   argv += optind;

   if (argc < 1) {
      usage();
   }

   OSDependentInit();

   for (i = 0; i < argc; --argc, ++argv) {
      if (fstype(*argv, fs, sizeof(fs))) {
         if (verbose) {
            printf("%s: %s\n", *argv, fs);
         } else {
            puts(fs);
         }
      } else {
         fprintf(stderr, _("%s: unknown\n"), *argv);
         status = 1;
      }
   }

   flush_mntent_cache();

   exit(status);
}
