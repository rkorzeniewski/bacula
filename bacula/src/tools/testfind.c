/*  
 * Test program for find files
 */

#include "bacula.h"
#include "findlib/find.h"
#include "jcr.h"


/* Global variables */
static int num_files = 0;
static int max_file_len = 0;
static int max_path_len = 0;
static int trunc_fname = 0;
static int trunc_path = 0;
static int attrs = 0;


static int print_file(FF_PKT *ff, void *pkt);
static void count_files(FF_PKT *ff);

static void usage()
{
   fprintf(stderr, _(
"\n"
"Usage: testfind [-d debug_level] [-] [pattern1 ...]\n"
"       -a          print extended attributes (Win32 debug)\n"
"       -dnn        set debug level to nn\n"
"       -           read pattern(s) from stdin\n"
"       -?          print this message.\n"
"\n"
"Patterns are file inclusion -- normally directories.\n"
"Debug level >= 1 prints each file found.\n"
"Debug level >= 10 prints path/file for catalog.\n"
"Errors always printed.\n"
"Files/paths truncated is number with len > 255.\n"
"Truncation is only in catalog.\n"
"\n"));

   exit(1);
}


int
main (int argc, char *const *argv)
{
   FF_PKT *ff;
   char name[1000];
   int i, ch, hard_links;

   while ((ch = getopt(argc, argv, "ad:?")) != -1) {
      switch (ch) {
         case 'a':                    /* print extended attributes *debug* */
	    attrs = 1;
	    break;

         case 'd':                    /* set debug level */
	    debug_level = atoi(optarg);
	    if (debug_level <= 0) {
	       debug_level = 1; 
	    }
	    break;

         case '?':
	 default:
	    usage();

      }  
   }
   argc -= optind;
   argv += optind;

  ff = init_find_files();
   if (argc == 0) {
     add_fname_to_include_list(ff, 0, "/"); /* default to / */
  } else {   
      for (i=0; i < argc; i++) {
        if (strcmp(argv[i], "-") == 0) {
	   while (fgets(name, sizeof(name)-1, stdin)) {
	      strip_trailing_junk(name);
	      add_fname_to_include_list(ff, 0, name); 
	   }
	   continue;
	}
	add_fname_to_include_list(ff, 0, argv[i]); 
     }
  }

  find_files(ff, print_file, NULL);
  hard_links = term_find_files(ff);
  
   printf(_("\
Total files    : %d\n\
Max file length: %d\n\
Max path length: %d\n\
Files truncated: %d\n\
Paths truncated: %d\n\
Hard links     : %d\n"),
     num_files, max_file_len, max_path_len,
     trunc_fname, trunc_path, hard_links);
  
  close_memory_pool();
  sm_dump(False);
  exit(0);
}

static int print_file(FF_PKT *ff, void *pkt)
{

   switch (ff->type) {
   case FT_LNKSAVED:
      if (debug_level == 1) {
         printf("%s\n", ff->fname);
      } else if (debug_level > 1) {
         printf("Lnka: %s -> %s\n", ff->fname, ff->link);
      }
      break;
   case FT_REGE:
      if (debug_level == 1) {
         printf("%s\n", ff->fname);
      } else if (debug_level > 1) {
         printf("Empty: %s\n", ff->fname);
      }
      count_files(ff);
      break; 
   case FT_REG:
      if (debug_level == 1) {
         printf("%s\n", ff->fname);
      } else if (debug_level > 1) {
         printf("Reg: %s\n", ff->fname);
      }
      count_files(ff);
      break;
   case FT_LNK:
      if (debug_level == 1) {
         printf("%s\n", ff->fname);
      } else if (debug_level > 1) {
         printf("Lnk: %s -> %s\n", ff->fname, ff->link);
      }
      count_files(ff);
      break;
   case FT_DIR:
      if (debug_level == 1) {
         printf("%s\n", ff->fname);
      } else if (debug_level > 1) {
         printf("Dir: %s\n", ff->fname);
      }
      count_files(ff);
      break;
   case FT_SPEC:
      if (debug_level == 1) {
         printf("%s\n", ff->fname);
      } else if (debug_level > 1) {
         printf("Spec: %s\n", ff->fname);
      }
      count_files(ff);
      break;
   case FT_NOACCESS:
      printf(_("Err: Could not access %s: %s\n"), ff->fname, strerror(errno));
      break;
   case FT_NOFOLLOW:
      printf(_("Err: Could not follow ff->link %s: %s\n"), ff->fname, strerror(errno));
      break;
   case FT_NOSTAT:
      printf(_("Err: Could not stat %s: %s\n"), ff->fname, strerror(errno));
      break;
   case FT_NOCHG:
      printf(_("Skip: File not saved. No change. %s\n"), ff->fname);
      break;
   case FT_ISARCH:
      printf(_("Err: Attempt to backup archive. Not saved. %s\n"), ff->fname);
      break;
   case FT_NORECURSE:
      printf(_("Recursion turned off. Directory not entered. %s\n"), ff->fname);
      break;
   case FT_NOFSCHG:
      printf(_("Skip: File system change prohibited. Directory not entered. %s\n"), ff->fname);
      break;
   case FT_NOOPEN:
      printf(_("Err: Could not open directory %s: %s\n"), ff->fname, strerror(errno));
      break;
   default:
      printf(_("Err: Unknown file ff->type %d: %s\n"), ff->type, ff->fname);
      break;
   }
   if (attrs) {
      char attr[200];
      encode_attribsEx(NULL, attr, ff);
      if (*attr != 0) {
         printf("AttrEx=%s\n", attr);
      }
//    set_attribsEx(NULL, ff->fname, NULL, NULL, ff->type, attr);
   }
   return 1;
}

static void count_files(FF_PKT *ar) 
{
   int fnl, pnl;
   char *l, *p;
   char file[MAXSTRING];
   char spath[MAXSTRING];

   num_files++;

   /* Find path without the filename.  
    * I.e. everything after the last / is a "filename".
    * OK, maybe it is a directory name, but we treat it like
    * a filename. If we don't find a / then the whole name
    * must be a path name (e.g. c:).
    */
   for (p=l=ar->fname; *p; p++) {
      if (*p == '/') {
	 l = p; 		      /* set pos of last slash */
      }
   }
   if (*l == '/') {                   /* did we find a slash? */
      l++;			      /* yes, point to filename */
   } else {			      /* no, whole thing must be path name */
      l = p;
   }

   /* If filename doesn't exist (i.e. root directory), we
    * simply create a blank name consisting of a single 
    * space. This makes handling zero length filenames
    * easier.
    */
   fnl = p - l;
   if (fnl > max_file_len) {
      max_file_len = fnl;
   }
   if (fnl > 255) {
      printf(_("===== Filename truncated to 255 chars: %s\n"), l);
      fnl = 255;
      trunc_fname++;
   }
   if (fnl > 0) {
      strncpy(file, l, fnl);	      /* copy filename */
      file[fnl] = 0;
   } else {
      file[0] = ' ';                  /* blank filename */
      file[1] = 0;
   }

   pnl = l - ar->fname;    
   if (pnl > max_path_len) {
      max_path_len = pnl;
   }
   if (pnl > 255) {
      printf(_("========== Path name truncated to 255 chars: %s\n"), ar->fname);
      pnl = 255;
      trunc_path++;
   }
   strncpy(spath, ar->fname, pnl);
   spath[pnl] = 0;
   if (pnl == 0) {
      spath[0] = ' ';
      spath[1] = 0;
      printf(_("========== Path length is zero. File=%s\n"), ar->fname);
   }
   if (debug_level >= 10) {
      printf("Path: %s\n", spath);
      printf("File: %s\n", file);
   }

}
