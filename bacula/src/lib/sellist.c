/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2011-2014 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from many
   others, a complete list can be found in the file AUTHORS.

   You may use this file and others of this release according to the
   license defined in the LICENSE file, which includes the Affero General
   Public License, v3.0 ("AGPLv3") and some additional permissions and
   terms pursuant to its AGPLv3 Section 7.

   Bacula® is a registered trademark of Kern Sibbald.
*/
/*
 *  Kern Sibbald, January  MMXII
 *
 *  Selection list. A string of integers separated by commas
 *   representing items selected. Ranges of the form nn-mm
 *   are also permitted.
 */

#include "bacula.h"

/*
 * Returns next item
 *   error if returns -1 and errmsg set
 *   end of items if returns -1 and errmsg NULL
 */
int64_t sellist::next()
{
   errmsg = NULL;
   if (beg <= end) {
      return beg++;
   }
   if (e == NULL) {
      goto bail_out;
   }
   /*
    * As we walk the list, we set EOF in
    *   the end of the next item to ease scanning,
    *   but save and then restore the character.
    */
   for (p=e; p && *p; p=e) {
      /* Check for list */
      e = strchr(p, ',');
      if (e) {                       /* have list */
         esave = *e;
         *e++ = 0;
      } else {
         esave = 0;
      }
      /* Check for range */
      h = strchr(p, '-');             /* range? */
      if (h == p) {
         errmsg = _("Negative numbers not permitted.\n");
         goto bail_out;
      }
      if (h) {                        /* have range */
         hsave = *h;
         *h++ = 0;
         if (!is_an_integer(h)) {
            errmsg = _("Range end is not integer.\n");
            goto bail_out;
         }
         skip_spaces(&p);
         if (!is_an_integer(p)) {
            errmsg = _("Range start is not an integer.\n");
            goto bail_out;
         }
         beg = str_to_int64(p);
         end = str_to_int64(h);
         if (end < beg) {
            errmsg = _("Range end not bigger than start.\n");
            goto bail_out;
         }
      } else {                           /* not list, not range */
         hsave = 0;
         skip_spaces(&p);
         /* Check for abort (.) */
         if (*p == '.') {
            errmsg = _("User cancel requested.\n");
            goto bail_out;
         }
         /* Check for all keyword */
         if (strncasecmp(p, "all", 3) == 0) {
            all = true;
            errmsg = NULL;
            return 0;
         }
         if (!is_an_integer(p)) {
            errmsg = _("Input value is not an integer.\n");
            goto bail_out;
         }
         beg = end = str_to_int64(p);
      }
      if (esave) {
         *(e-1) = esave;
      }
      if (hsave) {
         *(h-1) = hsave;
      }
      if (beg <= 0 || end <= 0) {
         errmsg = _("Selection items must be be greater than zero.\n");
         goto bail_out;
      }
      if (beg <= end) {
         return beg++;
      }
   }
   /* End of items */
   errmsg = NULL;      /* No error */
   return -1;

bail_out:
   return -1;          /* Error, errmsg set */
}


/*
 * Set selection string and optionally scan it
 *   returns false on error in string
 *   returns true if OK
 */
bool sellist::set_string(char *string, bool scan=true)
{
   /*
    * Copy string, because we write into it,
    *  then scan through it once to find any
    *  errors.
    */
   if (expanded) {
      free(expanded);
      expanded = NULL;
   }
   if (str) {
      free(str);
   }
   e = str = bstrdup(string);
   end = 0;
   beg = 1;
   num_items = 0;
   if (scan) {
      while (next() >= 0) {
         num_items++;
      }
      if (get_errmsg()) {
         return false;
      }
      e = str;
      end = 0;
      beg = 1;
   }
   return true;
}

/* Get the expanded list of all ids, very useful for SQL queries */
char *sellist::get_expanded_list()
{
   int32_t expandedsize = 512;
   int32_t len;
   int64_t val;
   char    *p, *tmp;
   char    ed1[50];

   if (!expanded) {
      p = expanded = (char *)malloc(expandedsize * sizeof(char));
      *p = 0;

      while ((val = next()) >= 0) {
         edit_int64(val, ed1);
         len = strlen(ed1);

         /* Alloc more space if needed */
         if ((p + len + 1) > (expanded + expandedsize)) {
            expandedsize = expandedsize * 2;

            tmp = (char *) realloc(expanded, expandedsize);

            /* Compute new addresses for p and expanded */
            p = tmp + (p - expanded);
            expanded = tmp;
         }

         /* If not at the begining of the string, add a "," */
         if (p != expanded) {
            strcpy(p, ",");
            p++;
         }

         strcpy(p, ed1);
         p += len;
      }
   }
   return expanded;
}

#ifdef TEST_PROGRAM
int main(int argc, char **argv, char **env)
{
   const char *msg;
   sellist sl;
   int i;

   if (!argv[1]) {
      msg = _("No input string given.\n");
      goto bail_out;
   }
   Dmsg1(000, "argv[1]=%s\n", argv[1]);

   strip_trailing_junk(argv[1]);
   sl.set_string(argv[1]);

   //Dmsg1(000, "get_list=%s\n", sl.get_list());

   /* If the list is very long, Dmsg will truncate it */
   Dmsg1(000, "get_expanded_list=%s\n", NPRT(sl.get_expanded_list()));

   if ((msg = sl.get_errmsg())) {
      goto bail_out;
   }
   while ((i=sl.next()) > 0) {
      Dmsg1(000, "rtn=%d\n", i);
   }
   if ((msg = sl.get_errmsg())) {
      goto bail_out;
   }
   printf("\nPass 2 argv[1]=%s\n", argv[1]);
   sl.set_string(argv[1]);
   while ((i=sl.next()) > 0) {
      Dmsg1(000, "rtn=%d\n", i);
   }
   msg = sl.get_errmsg();
   Dmsg2(000, "rtn=%d msg=%s\n", i, NPRT(msg));
   if (msg) {
      goto bail_out;
   }
   return 0;

bail_out:
   Dmsg1(000, "Error: %s\n", NPRT(msg));
   return 1;

}
#endif /* TEST_PROGRAM */
