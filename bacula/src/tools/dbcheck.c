/*
 *
 *  Program to check a Bacula database for consistency and to
 *   make repairs 
 *
 *   Kern E. Sibbald, August 2002
 *
 *   Version $Id$
 *
 */
/*
   Copyright (C) 2000-2003 Kern Sibbald and John Walker

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of
   the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA.

 */

#include "bacula.h"
#include "cats/cats.h"

typedef struct s_id_ctx {
   uint32_t *Id;		      /* ids to be modified */
   int num_ids; 		      /* ids stored */
   int max_ids; 		      /* size of array */
   int num_del; 		      /* number deleted */
   int tot_ids; 		      /* total to process */
} ID_LIST;

typedef struct s_name_ctx {
   char **name; 		      /* list of names */
   int num_ids; 		      /* ids stored */
   int max_ids; 		      /* size of array */
   int num_del; 		      /* number deleted */
   int tot_ids; 		      /* total to process */
} NAME_LIST;



/* Global variables */
static int fix = FALSE;
static int batch = FALSE;
static int verbose = FALSE;
static B_DB *db;
static ID_LIST id_list;
static NAME_LIST name_list;
static char buf[2000];

#define MAX_ID_LIST_LEN 1000000

/* Forward referenced functions */
static int make_id_list(char *query, ID_LIST *id_list);
static int delete_id_list(char *query, ID_LIST *id_list);
static int make_name_list(char *query, NAME_LIST *name_list);
static void print_name_list(NAME_LIST *name_list);
static void free_name_list(NAME_LIST *name_list);
static char *get_cmd(char *prompt);
static void eliminate_duplicate_filenames();
static void eliminate_duplicate_paths();
static void eliminate_orphaned_jobmedia_records();
static void eliminate_orphaned_file_records();
static void eliminate_orphaned_path_records();
static void eliminate_orphaned_filename_records();
static void eliminate_orphaned_fileset_records();
static void do_interactive_mode();
static int yes_no(char *prompt);


static void usage()
{
   fprintf(stderr,
"Usage: dbcheck [-d debug_level] <working-directory> <bacula-databse> <user> <password>\n"
"       -b              batch mode\n"
"       -dnn            set debug level to nn\n"
"       -f              fix inconsistencies\n"
"       -v              verbose\n"
"       -?              print this message\n\n");
   exit(1);
}

int main (int argc, char *argv[])
{
   int ch;
   char *user, *password, *db_name;

   my_name_is(argc, argv, "dbcheck");
   init_msg(NULL, NULL);	      /* setup message handler */

   memset(&id_list, 0, sizeof(id_list));
   memset(&name_list, 0, sizeof(name_list));


   while ((ch = getopt(argc, argv, "bd:fv?")) != -1) {
      switch (ch) {
         case 'b':                    /* batch */
	    batch = TRUE;
	    break;

         case 'd':                    /* debug level */
	    debug_level = atoi(optarg);
	    if (debug_level <= 0)
	       debug_level = 1; 
	    break;

         case 'f':                    /* fix inconsistencies */
	    fix = TRUE;
	    break;

         case 'v':
	    verbose++;
	    break;

         case '?':
	 default:
	    usage();

      }  
   }
   argc -= optind;
   argv += optind;

   if (argc > 4) {
      Pmsg0(0, _("Wrong number of arguments.\n"));
      usage();
   }

   if (argc < 1) {
      Pmsg0(0, _("Working directory not supplied.\n"));
      usage();
   }

   /* This is needed by SQLite to find the db */
   working_directory = argv[0];
   db_name = "bacula";
   user = db_name;
   password = "";

   if (argc == 2) {
      db_name = argv[1];
      user = db_name;
   } else if (argc == 3) {
      db_name = argv[1];
      user = argv[2];
   } else if (argc == 4) {
      db_name = argv[1];
      user = argv[2];
      password = argv[3];
   }

   /* Open database */
   db = db_init_database(NULL, db_name, user, password);
   if (!db_open_database(NULL, db)) {
      Emsg1(M_FATAL, 0, "%s", db_strerror(db));
   }

   if (batch) {
      eliminate_duplicate_filenames();
      eliminate_duplicate_paths();
      eliminate_orphaned_jobmedia_records();
      eliminate_orphaned_file_records();
      eliminate_orphaned_path_records();
      eliminate_orphaned_filename_records();
      eliminate_orphaned_fileset_records();
   } else {
      do_interactive_mode();
   }

   db_close_database(NULL, db);
   close_msg(NULL);
   term_msg();
   return 0;
}

static void do_interactive_mode()
{
   int quit = FALSE;
   char *cmd;

   printf("Hello, this is the database check/correct program.\n\
Modify database is %s. Verbose is %s.\n\
Please select the fuction you want to perform.\n",
          fix?"On":"Off", verbose?"On":"Off");

   while (!quit) {
      if (fix) {
         printf(_("\n\
     1) Toggle modify database flag\n\
     2) Toggle verbose flag\n\
     3) Eliminate duplicate Filename records\n\
     4) Eliminate duplicate Path records\n\
     5) Eliminate orphaned Jobmedia records\n\
     6) Eliminate orphaned File records\n\
     7) Eliminate orphaned Path records\n\
     8) Eliminate orphaned Filename records\n\
     9) Eliminate orphaned FileSet records\n\
    10) All (3-9)\n\
    11) Quit\n"));
       } else {
         printf(_("\n\
     1) Toggle modify database flag\n\
     2) Toggle verbose flag\n\
     3) Check for duplicate Filename records\n\
     4) Check for duplicate Path records\n\
     5) Check for orphaned Jobmedia records\n\
     6) Check for orphaned File records\n\
     7) Check for orphaned Path records\n\
     8) Check for orphaned Filename records\n\
     9) Check for orphaned FileSet records\n\
    10) All (3-9)\n\
    11) Quit\n"));
       }

      cmd = get_cmd(_("Select function number: "));
      if (cmd) {
	 int item = atoi(cmd);
	 switch (item) {
	 case 1:
	    fix = !fix;
            printf(_("Database will %sbe modified.\n"), fix?"":_("NOT "));
	    break;
	 case 2:
	    verbose = verbose?0:1;
            printf(_("Verbose is %s\n"), verbose?_("On"):_("Off"));
	    break;
	 case 3:
	    eliminate_duplicate_filenames();
	    break;
	 case 4:
	    eliminate_duplicate_paths();
	    break;
	 case 5:
	    eliminate_orphaned_jobmedia_records();
	    break;
	 case 6:
	    eliminate_orphaned_file_records();
	    break;
	 case 7:
	    eliminate_orphaned_path_records();
	    break;
	 case 8:
	    eliminate_orphaned_filename_records();
	    break;
	 case 9:
	    eliminate_orphaned_fileset_records();
	    break;
	 case 10:
	    eliminate_duplicate_filenames();
	    eliminate_duplicate_paths();
	    eliminate_orphaned_jobmedia_records();
	    eliminate_orphaned_file_records();
	    eliminate_orphaned_path_records();
	    eliminate_orphaned_filename_records();
	    eliminate_orphaned_fileset_records();
	    break;
	 case 11:
	    quit = 1;
	    break;
	 }
      }
   }
}

static int print_name_handler(void *ctx, int num_fields, char **row)
{
   if (row[0]) {
      printf("%s\n", row[0]);
   }
   return 0;
}

static int print_jobmedia_handler(void *ctx, int num_fields, char **row)
{
   printf(_("Orphaned JobMediaId=%s JobId=%s Volume=\"%s\"\n"), 
	      NPRT(row[0]), NPRT(row[1]), NPRT(row[2]));
   return 0;
}

static int print_file_handler(void *ctx, int num_fields, char **row)
{
   printf(_("Orphaned FileId=%s JobId=%s Volume=\"%s\"\n"), 
	      NPRT(row[0]), NPRT(row[1]), NPRT(row[2]));
   return 0;
}

static int print_fileset_handler(void *ctx, int num_fields, char **row)
{
   printf(_("Orphaned FileSetId=%s FileSet=\"%s\" MD5=%s\n"), 
	      NPRT(row[0]), NPRT(row[1]), NPRT(row[2]));
   return 0;
}




  
/*
 * Called here with each id to be added to the list
 */
static int id_list_handler(void *ctx, int num_fields, char **row)
{
   ID_LIST *lst = (ID_LIST *)ctx;

   if (lst->num_ids == MAX_ID_LIST_LEN) {  
      return 1;
   }
   if (lst->num_ids == lst->max_ids) {
      if (lst->max_ids == 0) {
	 lst->max_ids = 1000;
	 lst->Id = (uint32_t *)bmalloc(sizeof(uint32_t) * lst->max_ids);
      } else {
	 lst->max_ids = (lst->max_ids * 3) / 2;
	 lst->Id = (uint32_t *)brealloc(lst->Id, sizeof(uint32_t) * lst->max_ids);
      }
   }
   lst->Id[lst->num_ids++] = (uint32_t)strtod(row[0], NULL);
   return 0;
}

/*
 * Construct record id list
 */
static int make_id_list(char *query, ID_LIST *id_list)
{
   id_list->num_ids = 0;
   id_list->num_del = 0;
   id_list->tot_ids = 0;

   if (!db_sql_query(db, query, id_list_handler, (void *)id_list)) {
      printf("%s", db_strerror(db));
      return 0;
   }
   return 1;
}

/*
 * Delete all entries in the list 
 */
static int delete_id_list(char *query, ID_LIST *id_list)
{ 
   int i;

   for (i=0; i < id_list->num_ids; i++) {
      sprintf(buf, query, id_list->Id[i]);
      if (verbose) {
         printf("Deleting: %s\n", buf);
      }
      db_sql_query(db, buf, NULL, NULL);
   }
   return 1;
}

/*
 * Called here with each name to be added to the list
 */
static int name_list_handler(void *ctx, int num_fields, char **row)
{
   NAME_LIST *name = (NAME_LIST *)ctx;

   if (name->num_ids == MAX_ID_LIST_LEN) {  
      return 1;
   }
   if (name->num_ids == name->max_ids) {
      if (name->max_ids == 0) {
	 name->max_ids = 1000;
	 name->name = (char **)bmalloc(sizeof(char *) * name->max_ids);
      } else {
	 name->max_ids = (name->max_ids * 3) / 2;
	 name->name = (char **)brealloc(name->name, sizeof(char *) * name->max_ids);
      }
   }
   name->name[name->num_ids++] = bstrdup(row[0]);
   return 0;
}


/*
 * Construct name list
 */
static int make_name_list(char *query, NAME_LIST *name_list)
{
   name_list->num_ids = 0;
   name_list->num_del = 0;
   name_list->tot_ids = 0;

   if (!db_sql_query(db, query, name_list_handler, (void *)name_list)) {
      printf("%s", db_strerror(db));
      return 0;
   }
   return 1;
}

/*
 * Print names in the list
 */
static void print_name_list(NAME_LIST *name_list)
{ 
   int i;

   for (i=0; i < name_list->num_ids; i++) {
      printf("%s\n", name_list->name[i]);
   }
}


/*
 * Free names in the list
 */
static void free_name_list(NAME_LIST *name_list)
{ 
   int i;

   for (i=0; i < name_list->num_ids; i++) {
      free(name_list->name[i]);
   }
   name_list->num_ids = 0;
}

static void eliminate_duplicate_filenames()
{
   char *query;
   char esc_name[5000];

   printf("Checking for duplicate Filename entries.\n");
   
   /* Make list of duplicated names */
   query = "SELECT Name,count(Name) as Count FROM Filename GROUP BY Name "
           "HAVING Count > 1";

   if (!make_name_list(query, &name_list)) {
      exit(1);
   }
   printf("Found %d duplicate Filename records.\n", name_list.num_ids);
   if (verbose && yes_no("Print the list? (yes/no): ")) {
      print_name_list(&name_list);
   }
   if (fix) {
      /* Loop through list of duplicate names */
      for (int i=0; i<name_list.num_ids; i++) {
	 /* Get all the Ids of each name */
	 db_escape_string(esc_name, name_list.name[i], strlen(name_list.name[i]));
         sprintf(buf, "SELECT FilenameId FROM Filename WHERE Name='%s'", esc_name);
	 if (!make_id_list(buf, &id_list)) {
	    exit(1);
	 }
	 if (verbose) {
            printf("Found %d for: %s\n", id_list.num_ids, name_list.name[i]);
	 }
	 /* Force all records to use the first id then delete the other ids */
	 for (int j=1; j<id_list.num_ids; j++) {
            sprintf(buf, "UPDATE File SET FilenameId=%u WHERE FilenameId=%u", 
	       id_list.Id[0], id_list.Id[j]);
	    db_sql_query(db, buf, NULL, NULL);
            sprintf(buf, "DELETE FROM Filename WHERE FilenameId=%u", 
	       id_list.Id[j]);
	    db_sql_query(db, buf, NULL, NULL);
	 }
      }
   }
   free_name_list(&name_list);
}

static void eliminate_duplicate_paths()
{
   char *query;
   char esc_name[5000];

   printf(_("Checking for duplicate Path entries.\n"));
   
   /* Make list of duplicated names */

   query = "SELECT Path,count(Path) as Count FROM Path "
           "GROUP BY Path HAVING Count > 1";

   if (!make_name_list(query, &name_list)) {
      exit(1);
   }
   printf("Found %d duplicate Path records.\n", name_list.num_ids);
   if (name_list.num_ids && verbose && yes_no("Print them? (yes/no): ")) {
      print_name_list(&name_list);
   }
   if (fix) {
      /* Loop through list of duplicate names */
      for (int i=0; i<name_list.num_ids; i++) {
	 /* Get all the Ids of each name */
	 db_escape_string(esc_name, name_list.name[i], strlen(name_list.name[i]));
         sprintf(buf, "SELECT PathId FROM Path WHERE Path='%s'", esc_name);
	 id_list.num_ids = 0;
	 if (!make_id_list(buf, &id_list)) {
	    exit(1);
	 }
	 if (verbose) {
            printf("Found %d for: %s\n", id_list.num_ids, name_list.name[i]);
	 }
	 /* Force all records to use the first id then delete the other ids */
	 for (int j=1; j<id_list.num_ids; j++) {
            sprintf(buf, "UPDATE File SET PathId=%u WHERE PathId=%u", 
	       id_list.Id[0], id_list.Id[j]);
	    db_sql_query(db, buf, NULL, NULL);
            sprintf(buf, "DELETE FROM Path WHERE PathId=%u", 
	       id_list.Id[j]);
	    db_sql_query(db, buf, NULL, NULL);
	 }
      }
   }
   free_name_list(&name_list);
}

static void eliminate_orphaned_jobmedia_records()
{
   char *query;

   printf("Checking for orphaned JobMedia entries.\n");
   query = "SELECT JobMedia.JobMediaId,Job.JobId FROM JobMedia "
           "LEFT OUTER JOIN Job ON (JobMedia.JobId=Job.JobId) "
           "WHERE Job.JobId IS NULL";
   if (!make_id_list(query, &id_list)) {
      exit(1);
   }
   printf("Found %d orphaned JobMedia records.\n", id_list.num_ids);
   if (id_list.num_ids && verbose && yes_no("Print them? (yes/no): ")) {
      int i;
      for (i=0; i < id_list.num_ids; i++) {
	 sprintf(buf, 
"SELECT JobMedia.JobMediaId,JobMedia.JobId,Media.VolumeName FROM JobMedia,Media "
"WHERE JobMedia.JobMediaId=%u AND Media.MediaId=JobMedia.MediaId", id_list.Id[i]);
	 if (!db_sql_query(db, buf, print_jobmedia_handler, NULL)) {
            printf("%s\n", db_strerror(db));
	 }
      }
   }
   
   if (fix && id_list.num_ids > 0) {
      printf("Deleting %d orphaned JobMedia records.\n", id_list.num_ids);
      delete_id_list("DELETE FROM JobMedia WHERE JobMediaId=%u", &id_list);
   }
}

static void eliminate_orphaned_file_records()
{
   char *query;

   printf("Checking for orphaned File entries. This may take some time!\n");
   query = "SELECT File.FileId,Job.JobId FROM File "
           "LEFT OUTER JOIN Job ON (File.JobId=Job.JobId) "
           "WHERE Job.JobId IS NULL";
   if (!make_id_list(query, &id_list)) {
      exit(1);
   }
   printf("Found %d orphaned File records.\n", id_list.num_ids);
   if (name_list.num_ids && verbose && yes_no("Print them? (yes/no): ")) {
      int i;
      for (i=0; i < id_list.num_ids; i++) {
	 sprintf(buf, 
"SELECT File.FileId,File.JobId,Filename.Name FROM File,Filename "
"WHERE File.FileId=%u AND File.FilenameId=Filename.FilenameId", id_list.Id[i]);
	 if (!db_sql_query(db, buf, print_file_handler, NULL)) {
            printf("%s\n", db_strerror(db));
	 }
      }
   }
      
   if (fix && id_list.num_ids > 0) {
      printf("Deleting %d orphaned File records.\n", id_list.num_ids);
      delete_id_list("DELETE FROM File WHERE FileId=%u", &id_list);
   }
}

static void eliminate_orphaned_path_records()
{
   char *query;

   printf("Checking for orphaned Path entries. This may take some time!\n");
   query = "SELECT Path.PathId,File.PathId FROM Path "
           "LEFT OUTER JOIN File ON (Path.PathId=File.PathId) "
           "HAVING File.PathId IS NULL";
   if (!make_id_list(query, &id_list)) {
      exit(1);
   }
   printf("Found %d orphaned Path records.\n", id_list.num_ids);
   if (id_list.num_ids && verbose && yes_no("Print them? (yes/no): ")) {
      int i;
      for (i=0; i < id_list.num_ids; i++) {
         sprintf(buf, "SELECT Path FROM Path WHERE PathId=%u", id_list.Id[i]);
	 db_sql_query(db, buf, print_name_handler, NULL);
      }
   }
   
   if (fix && id_list.num_ids > 0) {
      printf("Deleting %d orphaned Path records.\n", id_list.num_ids);
      delete_id_list("DELETE FROM Path WHERE PathId=%u", &id_list);
   }
}

static void eliminate_orphaned_filename_records()
{
   char *query;

   printf("Checking for orphaned Filename entries. This may take some time!\n");
   query = "SELECT Filename.FilenameId,File.FilenameId FROM Filename "
           "LEFT OUTER JOIN File ON (Filename.FilenameId=File.FilenameId) "
           "WHERE File.FilenameId IS NULL";

   if (!make_id_list(query, &id_list)) {
      exit(1);
   }
   printf("Found %d orphaned Filename records.\n", id_list.num_ids);
   if (id_list.num_ids && verbose && yes_no("Print them? (yes/no): ")) {
      int i;
      for (i=0; i < id_list.num_ids; i++) {
         sprintf(buf, "SELECT Name FROM Filename WHERE FilenameId=%u", id_list.Id[i]);
	 db_sql_query(db, buf, print_name_handler, NULL);
      }
   }
   
   if (fix && id_list.num_ids > 0) {
      printf("Deleting %d orphaned Filename records.\n", id_list.num_ids);
      delete_id_list("DELETE FROM Filename WHERE FilenameId=%u", &id_list);
   }
}

static void eliminate_orphaned_fileset_records()
{
   char *query;

   printf("Checking for orphaned FileSet entries. This takes some time!\n");
   query = "SELECT FileSet.FileSetId,Job.FileSetId FROM FileSet "
           "LEFT OUTER JOIN Job ON (FileSet.FileSetId=Job.FileSetId) "
           "WHERE Job.FileSetId IS NULL";
   if (!make_id_list(query, &id_list)) {
      exit(1);
   }
   printf("Found %d orphaned FileSet records.\n", id_list.num_ids);
   if (id_list.num_ids && verbose && yes_no("Print them? (yes/no): ")) {
      int i;
      for (i=0; i < id_list.num_ids; i++) {
         sprintf(buf, "SELECT FileSetId,FileSet,MD5 FROM FileSet "
                      "WHERE FileSetId=%u", id_list.Id[i]);
	 if (!db_sql_query(db, buf, print_fileset_handler, NULL)) {
            printf("%s\n", db_strerror(db));
	 }
      }
   }
   
   if (fix && id_list.num_ids > 0) {
      printf("Deleting %d orphaned FileSet records.\n", id_list.num_ids);
      delete_id_list("DELETE FROM FileSet WHERE FileSetId=%u", &id_list);
   }
}


/*
 * Gen next input command from the terminal
 */
static char *get_cmd(char *prompt)
{
   static char cmd[1000];

   printf("%s", prompt);
   if (fgets(cmd, sizeof(cmd), stdin) == NULL)
      return NULL;
   printf("\n");
   strip_trailing_junk(cmd);
   return cmd;
}

static int yes_no(char *prompt)
{
   char *cmd;  
   cmd = get_cmd(prompt);
   return strcasecmp(cmd, "yes") == 0;
}
