/*
 *
 *   Bacula Director -- User Agent Database Query Commands
 *
 *     Kern Sibbald, December MMI
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

#include "bacula.h"
#include "dird.h"

extern DIRRES *director;

static POOLMEM *substitute_prompts(UAContext *ua,
                       POOLMEM *query, char **prompt, int nprompt);

/*
 * Read a file containing SQL queries and prompt
 *  the user to select which one.
 *
 *   File format:
 *   #  => comment
 *   :prompt for query
 *   *prompt for subst %1
 *   *prompt for subst %2
 *   ...
 *   SQL statement possibly terminated by ;
 *   :next query prompt
 */
int querycmd(UAContext *ua, const char *cmd)
{
   FILE *fd = NULL;
   POOLMEM *query = get_pool_memory(PM_MESSAGE);
   char line[1000];
   int i, item, len;
   char *prompt[9];
   int nprompt = 0;;
   char *query_file = director->query_file;

   if (!open_client_db(ua)) {
      goto bail_out;
   }
   if ((fd=fopen(query_file, "rb")) == NULL) {
      berrno be;
      ua->error_msg(_("Could not open %s: ERR=%s\n"), query_file,
         be.bstrerror());
      goto bail_out;
   }

   start_prompt(ua, _("Available queries:\n"));
   while (fgets(line, sizeof(line), fd) != NULL) {
      if (line[0] == ':') {
         strip_trailing_junk(line);
         add_prompt(ua, line+1);
      }
   }
   if ((item=do_prompt(ua, "", _("Choose a query"), NULL, 0)) < 0) {
      goto bail_out;
   }
   rewind(fd);
   i = -1;
   while (fgets(line, sizeof(line), fd) != NULL) {
      if (line[0] == ':') {
         i++;
      }
      if (i == item) {
         break;
      }
   }
   if (i != item) {
      ua->error_msg(_("Could not find query.\n"));
      goto bail_out;
   }
   query[0] = 0;
   for (i=0; i<9; i++) {
      prompt[i] = NULL;
   }
   while (fgets(line, sizeof(line), fd) != NULL) {
      if (line[0] == '#') {
         continue;
      }
      if (line[0] == ':') {
         break;
      }
      strip_trailing_junk(line);
      len = strlen(line);
      if (line[0] == '*') {            /* prompt */
         if (nprompt >= 9) {
            ua->error_msg(_("Too many prompts in query, max is 9.\n"));
         } else {
            line[len++] = ' ';
            line[len] = 0;
            prompt[nprompt++] = bstrdup(line+1);
            continue;
         }
      }
      if (*query != 0) {
         pm_strcat(query, " ");
      }
      pm_strcat(query, line);
      if (line[len-1] != ';') {
         continue;
      }
      line[len-1] = 0;             /* zap ; */
      if (query[0] != 0) {
         query = substitute_prompts(ua, query, prompt, nprompt);
         Dmsg1(100, "Query2=%s\n", query);
         if (query[0] == '!') {
            db_list_sql_query(ua->jcr, ua->db, query+1, prtit, ua, 0, VERT_LIST);
         } else if (!db_list_sql_query(ua->jcr, ua->db, query, prtit, ua, 1, HORZ_LIST)) {
            ua->send_msg("%s\n", query);
         }
         query[0] = 0;
      }
   } /* end while */

   if (query[0] != 0) {
      query = substitute_prompts(ua, query, prompt, nprompt);
      Dmsg1(100, "Query2=%s\n", query);
         if (query[0] == '!') {
            db_list_sql_query(ua->jcr, ua->db, query+1, prtit, ua, 0, VERT_LIST);
         } else if (!db_list_sql_query(ua->jcr, ua->db, query, prtit, ua, 1, HORZ_LIST)) {
            ua->error_msg("%s\n", query);
         }
   }

bail_out:
   if (fd) {
      fclose(fd);
   }
   free_pool_memory(query);
   for (i=0; i<nprompt; i++) {
      free(prompt[i]);
   }
   return 1;
}

static POOLMEM *substitute_prompts(UAContext *ua,
                       POOLMEM *query, char **prompt, int nprompt)
{
   char *p, *q, *o;
   POOLMEM *new_query;
   int i, n, len, olen;
   char *subst[9];

   if (nprompt == 0) {
      return query;
   }
   for (i=0; i<9; i++) {
      subst[i] = NULL;
   }
   new_query = get_pool_memory(PM_FNAME);
   o = new_query;
   for (q=query; (p=strchr(q, '%')); ) {
      if (p) {
        olen = o - new_query;
        new_query = check_pool_memory_size(new_query, olen + p - q + 10);
        o = new_query + olen;
         while (q < p) {              /* copy up to % */
            *o++ = *q++;
         }
         p++;
         switch (*p) {
         case '1':
         case '2':
         case '3':
         case '4':
         case '5':
         case '6':
         case '7':
         case '8':
         case '9':
            n = (int)(*p) - (int)'1';
            if (prompt[n]) {
               if (!subst[n]) {
                  if (!get_cmd(ua, prompt[n])) {
                     q += 2;
                     break;
                  }
               }
               len = strlen(ua->cmd);
               p = (char *)malloc(len * 2 + 1);
               db_escape_string(ua->jcr, ua->db, p, ua->cmd, len);
               subst[n] = p;
               olen = o - new_query;
               new_query = check_pool_memory_size(new_query, olen + strlen(p) + 10);
               o = new_query + olen;
               while (*p) {
                  *o++ = *p++;
               }
            } else {
               ua->error_msg(_("Warning prompt %d missing.\n"), n+1);
            }
            q += 2;
            break;
         case '%':
            *o++ = '%';
            q += 2;
            break;
         default:
            *o++ = '%';
            q++;
            break;
         }
      }
   }
   olen = o - new_query;
   new_query = check_pool_memory_size(new_query, olen + strlen(q) + 10);
   o = new_query + olen;
   while (*q) {
      *o++ = *q++;
   }
   *o = 0;
   for (i=0; i<9; i++) {
      if (subst[i]) {
         free(subst[i]);
      }
   }
   free_pool_memory(query);
   return new_query;
}

/*
 * Get general SQL query for Catalog
 */
int sqlquerycmd(UAContext *ua, const char *cmd)
{
   POOL_MEM query(PM_MESSAGE);
   int len;
   const char *msg;

   if (!open_new_client_db(ua)) {
      return 1;
   }
   *query.c_str() = 0;

   ua->send_msg(_("Entering SQL query mode.\n"
"Terminate each query with a semicolon.\n"
"Terminate query mode with a blank line.\n"));
   msg = _("Enter SQL query: ");
   while (get_cmd(ua, msg)) {
      len = strlen(ua->cmd);
      Dmsg2(400, "len=%d cmd=%s:\n", len, ua->cmd);
      if (len == 0) {
         break;
      }
      if (*query.c_str() != 0) {
         pm_strcat(query, " ");
      }
      pm_strcat(query, ua->cmd);
      if (ua->cmd[len-1] == ';') {
         ua->cmd[len-1] = 0;          /* zap ; */
         /* Submit query */
         db_list_sql_query(ua->jcr, ua->db, query.c_str(), prtit, ua, 1, HORZ_LIST);
         *query.c_str() = 0;         /* start new query */
         msg = _("Enter SQL query: ");
      } else {
         msg = _("Add to SQL query: ");
      }
   }
   ua->send_msg(_("End query mode.\n"));
   return 1;
}
