/*
 * Functions to handle ACL for bacula.
 *
 * We handle two different typers of ACLs: access and default ACLS.
 * Default ACLs only apply to directories.
 *
 * On some systems (eg. linux and FreeBSD) we must obtain the two ACLs
 * independently, while others (eg. Solaris) provide both in one call.
 *
 * As for the streams to use, we have two choices:
 *
 * 1. Build a generic framework.
 *    With two different types of ACLs, supported differently, we
 *    probably end up encoding and decoding everything ourselves.
 *
 * 2. Take the easy way out.
 *    Just handle each platform individually, assuming that backups
 *    and restores are done on the same (kind of) client.
 *
 * Currently we take the easy way out. We use two kinds of streams, one
 * for access ACLs and one for default ACLs. If an OS mixes the two, we
 * send the mix in the access ACL stream.
 *
 * Looking at more man pages, supporting a framework seems really hard
 * if we want to support HP-UX. Deity knows what AIX is up to.
 *
 *   Written by Preben 'Peppe' Guldberg, December MMIV
 *
 *   Version $Id$
 */

#ifdef xxxxxxx

Remove fprintf() and actuallyfree and fix POOLMEM coding

Pass errmsg buffer and a length or a POOLMEM buffer
into subroutine rather than malloc in
subroutine and free() in higher level routine, which causes
memory leaks if you forget to free. 

#ifndef TEST_PROGRAM

#include "bacula.h"
#include "filed.h"


#else
/*
 * Test program setup 
 *
 * Compile and set up with eg. with eg.
 *
 *    $ cc -DTEST_PROGRAM -DHAVE_SUN_OS -lsec -o acl acl.c
 *    $ ln -s acl aclcp
 *
 * You can then list ACLs with acl and copy them with aclcp.
 *
 * For a list of compiler flags, see the list preceding the big #if below.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "acl.h"

#define POOLMEM 	   char
#define bstrdup 	   strdup
#define actuallyfree(x)    free(x)
int aclls(char *fname);
int aclcp(char *src, char *dst);

#endif

/*
 * List of supported OSs.
 * Not sure if all the HAVE_XYZ_OS are correct for autoconf.
 * The ones that says man page, are coded according to man pages only.
 */
#if !defined(HAVE_ACL)              /* ACL support is required, of course */ \
   || !( defined(HAVE_AIX_OS)       /* man page -- may need flags         */ \
      || defined(HAVE_FREEBSD_OS)   /* tested   -- compile wihtout flags  */ \
      || defined(HAVE_IRIX_OS)      /* man page -- compile without flags  */ \
      || defined(HAVE_OSF1_OS)      /* man page -- may need -lpacl        */ \
      || defined(HAVE_LINUX_OS)     /* tested   -- compile with -lacl     */ \
      || defined(HAVE_HPUX_OS)      /* man page -- may need flags         */ \
      || defined(HAVE_SUN_OS)       /* tested   -- compile with -lsec     */ \
       )

POOLMEM *bacl_get(char *fname, int acltype)
{
   return NULL;
}

int bacl_set(char *fname, int acltype, char *acltext)
{
   return -1;
}

#elif defined(HAVE_AIX_OS)

#include <sys/access.h>

POOLMEM *bacl_get(char *fname, int acltype)
{
   char *tmp;
   POOLMEM *acltext = NULL;

   if ((tmp = acl_get(fname)) != NULL) {
      acltext = bstrdup(tmp);
      actuallyfree(tmp);
   }
   return acltext;
}

int bacl_set(char *fname, int acltype, char *acltext)
{
   if (acl_put(fname, acltext, 0) != 0) {
      return -1;
   }
   return 0;
}

#elif defined(HAVE_FREEBSD_OS) \
   || defined(HAVE_IRIX_OS) \
   || defined(HAVE_OSF1_OS) \
   || defined(HAVE_LINUX_OS)

#include <sys/types.h>
#include <sys/acl.h>

/* On IRIX we can get shortened ACLs */
#if defined(HAVE_IRIX_OS) && defined(BACL_WANT_SHORT_ACLS)
#define acl_to_text(acl,len)	 acl_to_short_text((acl), (len))
#endif

/* In Linux we can get numeric and/or shorted ACLs */
#if defined(HAVE_LINUX_OS)
#if defined(BACL_WANT_SHORT_ACLS) && defined(BACL_WANT_NUMERIC_IDS)
#define BACL_ALTERNATE_TEXT	       (TEXT_ABBREVIATE|TEXT_NUMERIC_IDS)
#elif defined(BACL_WANT_SHORT_ACLS)
#define BACL_ALTERNATE_TEXT	       TEXT_ABBREVIATE
#elif defined(BACL_WANT_NUMERIC_IDS)
#define BACL_ALTERNATE_TEXT	       TEXT_NUMERIC_IDS
#endif
#ifdef BACL_ALTERNATE_TEXT
#include <acl/libacl.h>
#define acl_to_text(acl,len)     ((len), acl_to_any_text((acl), NULL, ',', BACL_ALTERNATE_TEXT))
#endif
#endif

POOLMEM *bacl_get(char *fname, int acltype)
{
   acl_t acl;
   int ostype;
   char *tmp;
   POOLMEM *acltext = NULL;

   ostype = (acltype & BACL_TYPE_DEFAULT) ? ACL_TYPE_DEFAULT : ACL_TYPE_ACCESS;

   acl = acl_get_file(fname, ostype);
   if (acl) {
      if ((tmp = acl_to_text(acl, NULL)) != NULL) {
	 acltext = get_pool_memory(PM_MESSAGE);
	 pm_strcpy(acltext, tmp);
	 acl_free(tmp);
      }
      acl_free(acl);
   }
   /***** Do we really want to silently ignore errors from acl_get_file
     and acl_to_text?  *****/
   return acltext;
}

int bacl_set(char *fname, int acltype, char *acltext)
{
   acl_t acl;
   int ostype, stat;

   ostype = (acltype & BACL_TYPE_DEFAULT) ? ACL_TYPE_DEFAULT : ACL_TYPE_ACCESS;

   acl = acl_from_text(acltext);
   if (acl == NULL) {
      return -1;
   }

   /*
    * FreeBSD always fails acl_valid() - at least on valid input...
    * As it does the right thing, given valid input, just ignore acl_valid().
    */
#ifndef HAVE_FREEBSD_OS
   if (acl_valid(acl) != 0) {
      acl_free(acl);
      return -1;
   }
#endif

   stat = acl_set_file(fname, ostype, acl);
   acl_free(acl);
   return stat;
}

#elif defined(HAVE_HPUX_OS)
#include <sys/acl.h>
#include <acllib.h>

POOLMEM *bacl_get(char *fname, int acltype)
{
   int n;
   struct acl_entry acls[NACLENTRIES];
   char *tmp;
   POOLMEM *acltext = NULL;

   if ((n = getacl(fname, 0, acls)) <= 0) {
      return NULL;
   }
   if ((n = getacl(fname, n, acls)) > 0) {
      if ((tmp = acltostr(n, acls, FORM_SHORT)) != NULL) {
	 acltext = bstrdup(tmp);
	 actuallyfree(tmp);
      }
   }
   return acltext;
}

int bacl_set(char *fname, int acltype, char *acltext)
{
   int n, stat;
   struct acl_entry acls[NACLENTRIES];

   n = strtoacl(acltext, 0, NACLENTRIES, acls, ACL_FILEOWNER, ACL_FILEGROUP);
   if (n <= 0) {
      return -1;
   }
   if (strtoacl(acltext, n, NACLENTRIES, acls, ACL_FILEOWNER, ACL_FILEGROUP) != n) {
      return -1;
   }
   if (setacl(fname, n, acls) != 0) {
      return -1;
   }
   return 0;
}

#elif defined(HAVE_SUN_OS)
#include <sys/acl.h>

POOLMEM *bacl_get(char *fname, int acltype)
{
   int n;
   aclent_t *acls;
   char *tmp;
   POOLMEM *acltext = NULL;

   n = acl(fname, GETACLCNT, 0, NULL);
   if (n <= 0) {
      return NULL;
   }
   if ((acls = (aclent_t *)malloc(n * sizeof(aclent_t))) == NULL) {
      return NULL;
   }
   if (acl(fname, GETACL, n, acls) == n) {
      if ((tmp = acltotext(acls, n)) != NULL) {
	 acltext = bstrdup(tmp);
	 actuallyfree(tmp);
      }
   }
   actuallyfree(acls);
   return acltext;
}

int bacl_set(char *fname, int acltype, char *acltext)
{
   int n, stat;
   aclent_t *acls;

   acls = aclfromtext(acltext, &n);
   if (!acls) {
      return -1;
   }
   stat = acl(fname, SETACL, n, acls);
   actuallyfree(acls);
   return stat;
}

#endif


#ifdef TEST_PROGRAM
int main(int argc, char **argv)
{
   char *prgname;
   int status = 0;

   if (argc < 1) {
      fprintf(stderr, "Cannot determine my own name\n");
      return EXIT_FAILURE;
   }

   prgname = strrchr(argv[0], '/');
   if (prgname == NULL || *++prgname == '\0') {
      prgname = argv[0];
   }
   --argc;
   ++argv;

   /* aclcp "copies" ACLs - set ACLs on destination equal to ACLs on source */
   if (strcmp(prgname, "aclcp") == 0) {
      int verbose = 0;
      if (strcmp(*argv, "-v") == 0) {
	 ++verbose;
	 --argc;
	 ++argv;
      }
      if (argc != 2) {
         fprintf(stderr, "%s: wrong number of arguments\n"
               "usage:\t%s [-v] source destination\n"
               "\tCopies ACLs from source to destination.\n"
               "\tSpecify -v to show ACLs after copy for verification.\n",
	       prgname, prgname);
	 return EXIT_FAILURE;
      }
      if (strcmp(argv[0], argv[1]) == 0) {
         fprintf(stderr, "%s: identical source and destination.\n"
               "usage:\t%s [-v] source destination\n"
               "\tCopies ACLs from source to destination.\n"
               "\tSpecify -v to show ACLs after copy for verification.\n",
	       prgname, prgname);
	 return EXIT_FAILURE;
      }
      if (verbose) {
	 aclls(argv[0]);
      }
      status = aclcp(argv[0], argv[1]);
      if (verbose && status == 0) {
	 aclls(argv[1]);
      }
      return status;
   }

   /* Default: just list ACLs */
   if (argc < 1) {
      fprintf(stderr, "%s: missing arguments\n"
            "usage:\t%s file ...\n"
            "\tLists ACLs of specified files or directories.\n",
	    prgname, prgname);
      return EXIT_FAILURE;
   }
   while (argc--) {
      if (!aclls(*argv++)) {
	 status = EXIT_FAILURE;
      }
   }

   return status;
}

/**** Test program *****/
int aclcp(char *src, char *dst)
{
   struct stat st;
   POOLMEM *acltext;

   if (lstat(dst, &st) != 0) {
      fprintf(stderr, "aclcp: destination does not exist\n");
      return EXIT_FAILURE;
   }
   if (S_ISLNK(st.st_mode)) {
      fprintf(stderr, "aclcp: cannot set ACL on symlinks\n");
      return EXIT_FAILURE;
   }
   if (lstat(src, &st) != 0) {
      fprintf(stderr, "aclcp: source does not exist\n");
      return EXIT_FAILURE;
   }
   if (S_ISLNK(st.st_mode)) {
      fprintf(stderr, "aclcp: will not read ACL from symlinks\n");
      return EXIT_FAILURE;
   }

   acltext = bacl_get(src, BACL_TYPE_ACCESS);
   if (!acltext) {
      fprintf(stderr, "aclcp: could not read ACLs for %s\n", src);
      return EXIT_FAILURE;
   } else {
      if (bacl_set(dst, BACL_TYPE_ACCESS, acltext) != 0) {
         fprintf(stderr, "aclcp: could not set ACLs on %s\n", dst);
	 actuallyfree(acltext);
	 return EXIT_FAILURE;
      }
      actuallyfree(acltext);
   }

   if (S_ISDIR(st.st_mode) && (BACL_CAP & BACL_CAP_DEFAULTS_DIR)) {
      acltext = bacl_get(src, BACL_TYPE_DEFAULT);
      if (acltext) {
	 if (bacl_set(dst, BACL_TYPE_DEFAULT, acltext) != 0) {
            fprintf(stderr, "aclcp: could not set default ACLs on %s\n", dst);
	    actuallyfree(acltext);
	    return EXIT_FAILURE;
	 }
	 actuallyfree(acltext);
      }
   }

   return 0;
}

/**** Test program *****/
int aclls(char *fname)
{
   struct stat st;
   POOLMEM *acltext;

   if (lstat(fname, &st) != 0) {
      fprintf(stderr, "acl: source does not exist\n");
      return EXIT_FAILURE;
   }
   if (S_ISLNK(st.st_mode)) {
      fprintf(stderr, "acl: will not read ACL from symlinks\n");
      return EXIT_FAILURE;
   }

   acltext = bacl_get(fname, BACL_TYPE_ACCESS);
   if (!acltext) {
      fprintf(stderr, "acl: could not read ACLs for %s\n", fname);
      return EXIT_FAILURE;
   }
   printf("#file: %s\n%s\n", fname, acltext);
   actuallyfree(acltext);

   if (S_ISDIR(st.st_mode) && (BACL_CAP & BACL_CAP_DEFAULTS_DIR)) {
      acltext = bacl_get(fname, BACL_TYPE_DEFAULT);
      if (!acltext) {
         fprintf(stderr, "acl: could not read default ACLs for %s\n", fname);
	 return EXIT_FAILURE;
      }
      printf("#file: %s [default]\n%s\n", fname, acltext);
      actuallyfree(acltext);
   }

   return 0;
}
#endif
#endif
