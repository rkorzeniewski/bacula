/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2000-2014 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from many
   others, a complete list can be found in the file AUTHORS.

   You may use this file and others of this release according to the
   license defined in the LICENSE file, which includes the Affero General
   Public License, v3.0 ("AGPLv3") and some additional permissions and
   terms pursuant to its AGPLv3 Section 7.

   Bacula® is a registered trademark of Kern Sibbald.
*/
/*
 * Lexical scanner for Bacula configuration file
 *
 *   Kern Sibbald, 2000
 *
 */

#include "bacula.h"
#include "lex.h"

/* Debug level for this source file */
static const int dbglvl = 5000;

/*
 * Scan to "logical" end of line. I.e. end of line,
 *   or semicolon, but stop on T_EOB (same as end of
 *   line except it is not eaten).
 */
void scan_to_eol(LEX *lc)
{
   int token;
   Dmsg0(dbglvl, "start scan to eof\n");
   while ((token = lex_get_token(lc, T_ALL)) != T_EOL) {
      if (token == T_EOB) {
         lex_unget_char(lc);
         return;
      }
      if (token == T_EOF) {
         return;
      }
   }
}

/*
 * Get next token, but skip EOL
 */
int scan_to_next_not_eol(LEX * lc)
{
   int token;
   do {
      token = lex_get_token(lc, T_ALL);
   } while (token == T_EOL);
   return token;
}

/*
 * Format a scanner error message
 */
static void s_err(const char *file, int line, LEX *lc, const char *msg, ...)
{
   va_list arg_ptr;
   char buf[MAXSTRING];
   char more[MAXSTRING];

   va_start(arg_ptr, msg);
   bvsnprintf(buf, sizeof(buf), msg, arg_ptr);
   va_end(arg_ptr);

   if (lc->err_type == 0) {     /* M_ERROR_TERM by default */
      lc->err_type = M_ERROR_TERM;
   }

   if (lc->line_no > lc->begin_line_no) {
      bsnprintf(more, sizeof(more),
                _("Problem probably begins at line %d.\n"), lc->begin_line_no);
   } else {
      more[0] = 0;
   }
   if (lc->line_no > 0) {
      e_msg(file, line, lc->err_type, 0, _("Config error: %s\n"
"            : line %d, col %d of file %s\n%s\n%s"),
         buf, lc->line_no, lc->col_no, lc->fname, lc->line, more);
   } else {
      e_msg(file, line, lc->err_type, 0, _("Config error: %s\n"), buf);
   }
}

void lex_set_default_error_handler(LEX *lf)
{
   lf->scan_error = s_err;
}

/*
 * Set err_type used in error_handler
 * return the old value
 */
int lex_set_error_handler_error_type(LEX *lf, int err_type)
{
   int old = lf->err_type;
   lf->err_type = err_type;
   return old;
}

/*
 * Free the current file, and retrieve the contents
 * of the previous packet if any.
 */
LEX *lex_close_file(LEX *lf)
{
   LEX *of;

   if (lf == NULL) {
      Emsg0(M_ABORT, 0, _("Close of NULL file\n"));
   }
   Dmsg1(dbglvl, "Close lex file: %s\n", lf->fname);

   of = lf->next;
   if (lf->bpipe) {
      close_bpipe(lf->bpipe);
      lf->bpipe = NULL;
   } else if (lf->fd) {
      fclose(lf->fd);
   }
   Dmsg1(dbglvl, "Close cfg file %s\n", lf->fname);
   free(lf->fname);
   free_memory(lf->line);
   lf->line = NULL;
   free_memory(lf->str);
   lf->str = NULL;
   if (of) {
      of->options = lf->options;      /* preserve options */
      memcpy(lf, of, sizeof(LEX));
      Dmsg1(dbglvl, "Restart scan of cfg file %s\n", of->fname);
   } else {
      of = lf;
      lf = NULL;
   }
   if (of) {
      free(of);
   }
   return lf;
}

/*
 * Open a new configuration file. We push the
 * state of the current file (lf) so that we
 * can do includes.  This is a bit of a hammer.
 * Instead of passing back the pointer to the
 * new packet, I simply replace the contents
 * of the caller's packet with the new packet,
 * and link the contents of the old packet into
 * the next field.
 *
 */
LEX *lex_open_file(LEX *lf, const char *filename, LEX_ERROR_HANDLER *scan_error)

{
   LEX *nf;
   FILE *fd;
   BPIPE *bpipe = NULL;
   char *fname = bstrdup(filename);

   if (fname[0] == '|') {
      if ((bpipe = open_bpipe(fname+1, 0, "rb")) == NULL) {
         free(fname);
         return NULL;
      }
      fd = bpipe->rfd;
   } else if ((fd = fopen(fname, "rb")) == NULL) {
      free(fname);
      return NULL;
   }
   Dmsg1(400, "Open config file: %s\n", fname);
   nf = (LEX *)malloc(sizeof(LEX));
   if (lf) {
      memcpy(nf, lf, sizeof(LEX));
      memset(lf, 0, sizeof(LEX));
      lf->next = nf;                  /* if have lf, push it behind new one */
      lf->options = nf->options;      /* preserve user options */
      /*
       * preserve err_type to prevent bacula exiting on 'reload'
       * if config is invalid. Fixes bug #877
       */
      lf->err_type = nf->err_type;
   } else {
      lf = nf;                        /* start new packet */
      memset(lf, 0, sizeof(LEX));
      lex_set_error_handler_error_type(lf, M_ERROR_TERM);
   }
   if (scan_error) {
      lf->scan_error = scan_error;
   } else {
      lex_set_default_error_handler(lf);
   }
   lf->fd = fd;
   lf->bpipe = bpipe;
   lf->fname = fname;
   lf->line = get_memory(5000);
   lf->state = lex_none;
   lf->ch = L_EOL;
   lf->str = get_memory(5000);
   Dmsg1(dbglvl, "Return lex=%x\n", lf);
   return lf;
}

/*
 * Get the next character from the input.
 *  Returns the character or
 *    L_EOF if end of file
 *    L_EOL if end of line
 */
int lex_get_char(LEX *lf)
{
   if (lf->ch == L_EOF) {
      Emsg0(M_ABORT, 0, _("get_char: called after EOF."
         " You may have a open double quote without the closing double quote.\n"));
   }
   if (lf->ch == L_EOL) {
      if (bfgets(lf->line, lf->fd) == NULL) {
         lf->ch = L_EOF;
         if (lf->next) {
            lex_close_file(lf);
         }
         return lf->ch;
      }
      lf->line_no++;
      lf->col_no = 0;
      Dmsg2(1000, "fget line=%d %s", lf->line_no, lf->line);
   }
   lf->ch = (uint8_t)lf->line[lf->col_no];
   if (lf->ch == 0) {
      lf->ch = L_EOL;
   } else {
      lf->col_no++;
   }
   Dmsg2(dbglvl, "lex_get_char: %c %d\n", lf->ch, lf->ch);
   return lf->ch;
}

void lex_unget_char(LEX *lf)
{
   if (lf->ch == L_EOL) {
      lf->ch = 0;                     /* End of line, force read of next one */
   } else {
      lf->col_no--;                   /* Backup to re-read char */
   }
}


/*
 * Add a character to the current string
 */
static void add_str(LEX *lf, int ch)
{
   if (lf->str_len >= sizeof_pool_memory(lf->str)) {
      Emsg3(M_ERROR_TERM, 0, _(
           _("Config token too long, file: %s, line %d, begins at line %d\n")),
             lf->fname, lf->line_no, lf->begin_line_no);
   }
   lf->str[lf->str_len++] = ch;
   lf->str[lf->str_len] = 0;
}

/*
 * Begin the string
 */
static void begin_str(LEX *lf, int ch)
{
   lf->str_len = 0;
   lf->str[0] = 0;
   if (ch != 0) {
      add_str(lf, ch);
   }
   lf->begin_line_no = lf->line_no;   /* save start string line no */
}

#ifdef DEBUG
static const char *lex_state_to_str(int state)
{
   switch (state) {
   case lex_none:          return _("none");
   case lex_comment:       return _("comment");
   case lex_number:        return _("number");
   case lex_ip_addr:       return _("ip_addr");
   case lex_identifier:    return _("identifier");
   case lex_string:        return _("string");
   case lex_quoted_string: return _("quoted_string");
   case lex_include:       return _("include");
   case lex_include_quoted_string: return _("include_quoted_string");
   case lex_utf8_bom:      return _("UTF-8 Byte Order Mark");
   case lex_utf16_le_bom:  return _("UTF-16le Byte Order Mark");
   default:                return "??????";
   }
}
#endif

/*
 * Convert a lex token to a string
 * used for debug/error printing.
 */
const char *lex_tok_to_str(int token)
{
   switch(token) {
   case L_EOF:             return "L_EOF";
   case L_EOL:             return "L_EOL";
   case T_NONE:            return "T_NONE";
   case T_NUMBER:          return "T_NUMBER";
   case T_IPADDR:          return "T_IPADDR";
   case T_IDENTIFIER:      return "T_IDENTIFIER";
   case T_UNQUOTED_STRING: return "T_UNQUOTED_STRING";
   case T_QUOTED_STRING:   return "T_QUOTED_STRING";
   case T_BOB:             return "T_BOB";
   case T_EOB:             return "T_EOB";
   case T_EQUALS:          return "T_EQUALS";
   case T_ERROR:           return "T_ERROR";
   case T_EOF:             return "T_EOF";
   case T_COMMA:           return "T_COMMA";
   case T_EOL:             return "T_EOL";
   case T_UTF8_BOM:        return "T_UTF8_BOM";
   case T_UTF16_BOM:       return "T_UTF16_BOM";
   default:                return "??????";
   }
}

static uint32_t scan_pint(LEX *lf, char *str)
{
   int64_t val = 0;
   if (!is_a_number(str)) {
      scan_err1(lf, _("expected a positive integer number, got: %s"), str);
      /* NOT REACHED */
   } else {
      errno = 0;
      val = str_to_int64(str);
      if (errno != 0 || val < 0) {
         scan_err1(lf, _("expected a positive integer number, got: %s"), str);
         /* NOT REACHED */
      }
   }
   return (uint32_t)val;
}

static uint64_t scan_pint64(LEX *lf, char *str)
{
   uint64_t val = 0;
   if (!is_a_number(str)) {
      scan_err1(lf, _("expected a positive integer number, got: %s"), str);
      /* NOT REACHED */
   } else {
      errno = 0;
      val = str_to_uint64(str);
      if (errno != 0) {
         scan_err1(lf, _("expected a positive integer number, got: %s"), str);
         /* NOT REACHED */
      }
   }
   return val;
}

/*
 *
 * Get the next token from the input
 *
 */
int
lex_get_token(LEX *lf, int expect)
{
   int ch;
   int token = T_NONE;
   bool esc_next = false;
   /* Unicode files, especially on Win32, may begin with a "Byte Order Mark"
      to indicate which transmission format the file is in. The codepoint for
      this mark is U+FEFF and is represented as the octets EF-BB-BF in UTF-8
      and as FF-FE in UTF-16le(little endian) and  FE-FF in UTF-16(big endian).
      We use a distinct state for UTF-8 and UTF-16le, and use bom_bytes_seen
      to tell which byte we are expecting. */
   int bom_bytes_seen = 0;

   Dmsg0(dbglvl, "enter lex_get_token\n");
   while (token == T_NONE) {
      ch = lex_get_char(lf);
      switch (lf->state) {
      case lex_none:
         Dmsg2(dbglvl, "Lex state lex_none ch=%d,%x\n", ch, ch);
         if (B_ISSPACE(ch))
            break;
         if (B_ISALPHA(ch)) {
            if (lf->options & LOPT_NO_IDENT || lf->options & LOPT_STRING) {
               lf->state = lex_string;
            } else {
               lf->state = lex_identifier;
            }
            begin_str(lf, ch);
            break;
         }
         if (B_ISDIGIT(ch)) {
            if (lf->options & LOPT_STRING) {
               lf->state = lex_string;
            } else {
               lf->state = lex_number;
            }
            begin_str(lf, ch);
            break;
         }
         Dmsg0(dbglvl, "Enter lex_none switch\n");
         switch (ch) {
         case L_EOF:
            token = T_EOF;
            Dmsg0(dbglvl, "got L_EOF set token=T_EOF\n");
            break;
         case '#':
            lf->state = lex_comment;
            break;
         case '{':
            token = T_BOB;
            begin_str(lf, ch);
            break;
         case '}':
            token = T_EOB;
            begin_str(lf, ch);
            break;
         case '"':
            lf->state = lex_quoted_string;
            begin_str(lf, 0);
            break;
         case '=':
            token = T_EQUALS;
            begin_str(lf, ch);
            break;
         case ',':
            token = T_COMMA;
            begin_str(lf, ch);
            break;
         case ';':
            if (expect != T_SKIP_EOL) {
               token = T_EOL;      /* treat ; like EOL */
            }
            break;
         case L_EOL:
            Dmsg0(dbglvl, "got L_EOL set token=T_EOL\n");
            if (expect != T_SKIP_EOL) {
               token = T_EOL;
            }
            break;
         case '@':
            /* In NO_EXTERN mode, @ is part of a string */
            if (lf->options & LOPT_NO_EXTERN) {
               lf->state = lex_string;
               begin_str(lf, ch);
            } else {
               lf->state = lex_include;
               begin_str(lf, 0);
            }
            break;
         case 0xEF: /* probably a UTF-8 BOM */
         case 0xFF: /* probably a UTF-16le BOM */
         case 0xFE: /* probably a UTF-16be BOM (error)*/
            if (lf->line_no != 1 || lf->col_no != 1)
            {
               lf->state = lex_string;
               begin_str(lf, ch);
            } else {
               bom_bytes_seen = 1;
               if (ch == 0xEF) {
                  lf->state = lex_utf8_bom;
               } else if (ch == 0xFF) {
                  lf->state = lex_utf16_le_bom;
               } else {
                  scan_err0(lf, _("This config file appears to be in an "
                     "unsupported Unicode format (UTF-16be). Please resave as UTF-8\n"));
                  return T_ERROR;
               }
            }
            break;
         default:
            lf->state = lex_string;
            begin_str(lf, ch);
            break;
         }
         break;
      case lex_comment:
         Dmsg1(dbglvl, "Lex state lex_comment ch=%x\n", ch);
         if (ch == L_EOL) {
            lf->state = lex_none;
            if (expect != T_SKIP_EOL) {
               token = T_EOL;
            }
         } else if (ch == L_EOF) {
            token = T_ERROR;
         }
         break;
      case lex_number:
         Dmsg2(dbglvl, "Lex state lex_number ch=%x %c\n", ch, ch);
         if (ch == L_EOF) {
            token = T_ERROR;
            break;
         }
         /* Might want to allow trailing specifications here */
         if (B_ISDIGIT(ch)) {
            add_str(lf, ch);
            break;
         }

         /* A valid number can be terminated by the following */
         if (B_ISSPACE(ch) || ch == L_EOL || ch == ',' || ch == ';') {
            token = T_NUMBER;
            lf->state = lex_none;
         } else {
            lf->state = lex_string;
         }
         lex_unget_char(lf);
         break;
      case lex_ip_addr:
         if (ch == L_EOF) {
            token = T_ERROR;
            break;
         }
         Dmsg1(dbglvl, "Lex state lex_ip_addr ch=%x\n", ch);
         break;
      case lex_string:
         Dmsg1(dbglvl, "Lex state lex_string ch=%x\n", ch);
         if (ch == L_EOF) {
            token = T_ERROR;
            break;
         }
         if (ch == '\n' || ch == L_EOL || ch == '=' || ch == '}' || ch == '{' ||
             ch == '\r' || ch == ';' || ch == ',' || ch == '#' || (B_ISSPACE(ch)) ) {
            lex_unget_char(lf);
            token = T_UNQUOTED_STRING;
            lf->state = lex_none;
            break;
         }
         add_str(lf, ch);
         break;
      case lex_identifier:
         Dmsg2(dbglvl, "Lex state lex_identifier ch=%x %c\n", ch, ch);
         if (B_ISALPHA(ch)) {
            add_str(lf, ch);
            break;
         } else if (B_ISSPACE(ch)) {
            break;
         } else if (ch == '\n' || ch == L_EOL || ch == '=' || ch == '}' || ch == '{' ||
                    ch == '\r' || ch == ';' || ch == ','   || ch == '"' || ch == '#') {
            lex_unget_char(lf);
            token = T_IDENTIFIER;
            lf->state = lex_none;
            break;
         } else if (ch == L_EOF) {
            token = T_ERROR;
            lf->state = lex_none;
            begin_str(lf, ch);
            break;
         }
         /* Some non-alpha character => string */
         lf->state = lex_string;
         add_str(lf, ch);
         break;
      case lex_quoted_string:
         Dmsg2(dbglvl, "Lex state lex_quoted_string ch=%x %c\n", ch, ch);
         if (ch == L_EOF) {
            token = T_ERROR;
            break;
         }
         if (ch == L_EOL) {
            esc_next = false;
            break;
         }
         if (esc_next) {
            add_str(lf, ch);
            esc_next = false;
            break;
         }
         if (ch == '\\') {
            esc_next = true;
            break;
         }
         if (ch == '"') {
            token = T_QUOTED_STRING;
            /*
             * Since we may be scanning a quoted list of names,
             *  we get the next character (a comma indicates another
             *  one), then we put it back for rescanning.
             */
            lex_get_char(lf);
            lex_unget_char(lf);
            lf->state = lex_none;
            break;
         }
         add_str(lf, ch);
         break;
      case lex_include_quoted_string:
         if (ch == L_EOF) {
            token = T_ERROR;
            break;
         }
         if (esc_next) {
            add_str(lf, ch);
            esc_next = false;
            break;
         }
         if (ch == '\\') {
            esc_next = true;
            break;
         }
         if (ch == '"') {
            /* Keep the original LEX so we can print an error if the included file can't be opened. */
            LEX* lfori = lf;
            /* Skip the double quote when restarting parsing */
            lex_get_char(lf);

            lf->state = lex_none;
            lf = lex_open_file(lf, lf->str, lf->scan_error);
            if (lf == NULL) {
               berrno be;
               scan_err2(lfori, _("Cannot open included config file %s: %s\n"),
                  lfori->str, be.bstrerror());
               return T_ERROR;
            }
            break;
         }
         add_str(lf, ch);
         break;
      case lex_include:            /* scanning a filename */
         if (ch == L_EOF) {
            token = T_ERROR;
            break;
         }
         if (ch == '"') {
            lf->state = lex_include_quoted_string;
            break;
         }


         if (B_ISSPACE(ch) || ch == '\n' || ch == L_EOL || ch == '}' || ch == '{' ||
             ch == ';' || ch == ','   || ch == '"' || ch == '#') {
            /* Keep the original LEX so we can print an error if the included file can't be opened. */
            LEX* lfori = lf;

            lf->state = lex_none;
            lf = lex_open_file(lf, lf->str, lf->scan_error);
            if (lf == NULL) {
               berrno be;
               scan_err2(lfori, _("Cannot open included config file %s: %s\n"),
                  lfori->str, be.bstrerror());
               return T_ERROR;
            }
            break;
         }
         add_str(lf, ch);
         break;
      case lex_utf8_bom:
         /* we only end up in this state if we have read an 0xEF
            as the first byte of the file, indicating we are probably
            reading a UTF-8 file */
         if (ch == 0xBB && bom_bytes_seen == 1) {
            bom_bytes_seen++;
         } else if (ch == 0xBF && bom_bytes_seen == 2) {
            token = T_UTF8_BOM;
            lf->state = lex_none;
         } else {
            token = T_ERROR;
         }
         break;
      case lex_utf16_le_bom:
         /* we only end up in this state if we have read an 0xFF
            as the first byte of the file -- indicating that we are
            probably dealing with an Intel based (little endian) UTF-16 file*/
         if (ch == 0xFE) {
            token = T_UTF16_BOM;
            lf->state = lex_none;
         } else {
            token = T_ERROR;
         }
         break;
      }
      Dmsg4(dbglvl, "ch=%d state=%s token=%s %c\n", ch, lex_state_to_str(lf->state),
        lex_tok_to_str(token), ch);
   }
   Dmsg2(dbglvl, "lex returning: line %d token: %s\n", lf->line_no, lex_tok_to_str(token));
   lf->token = token;

   /*
    * Here is where we check to see if the user has set certain
    *  expectations (e.g. 32 bit integer). If so, we do type checking
    *  and possible additional scanning (e.g. for range).
    */
   switch (expect) {
   case T_PINT32:
      lf->pint32_val = scan_pint(lf, lf->str);
      lf->pint32_val2 = lf->pint32_val;
      token = T_PINT32;
      break;

   case T_PINT32_RANGE:
      if (token == T_NUMBER) {
         lf->pint32_val = scan_pint(lf, lf->str);
         lf->pint32_val2 = lf->pint32_val;
         token = T_PINT32;
      } else {
         char *p = strchr(lf->str, '-');
         if (!p) {
            scan_err2(lf, _("expected an integer or a range, got %s: %s"),
               lex_tok_to_str(token), lf->str);
            token = T_ERROR;
            break;
         }
         *p++ = 0;                       /* terminate first half of range */
         lf->pint32_val  = scan_pint(lf, lf->str);
         lf->pint32_val2 = scan_pint(lf, p);
         token = T_PINT32_RANGE;
      }
      break;

   case T_INT32:
      if (token != T_NUMBER || !is_a_number(lf->str)) {
         scan_err2(lf, _("expected an integer number, got %s: %s"),
               lex_tok_to_str(token), lf->str);
         token = T_ERROR;
         break;
      }
      errno = 0;
      lf->int32_val = (int32_t)str_to_int64(lf->str);
      if (errno != 0) {
         scan_err2(lf, _("expected an integer number, got %s: %s"),
               lex_tok_to_str(token), lf->str);
         token = T_ERROR;
      } else {
         token = T_INT32;
      }
      break;

   case T_INT64:
      Dmsg2(dbglvl, "int64=:%s: %f\n", lf->str, strtod(lf->str, NULL));
      if (token != T_NUMBER || !is_a_number(lf->str)) {
         scan_err2(lf, _("expected an integer number, got %s: %s"),
               lex_tok_to_str(token), lf->str);
         token = T_ERROR;
         break;
      }
      errno = 0;
      lf->int64_val = str_to_int64(lf->str);
      if (errno != 0) {
         scan_err2(lf, _("expected an integer number, got %s: %s"),
               lex_tok_to_str(token), lf->str);
         token = T_ERROR;
      } else {
         token = T_INT64;
      }
      break;

   case T_PINT64_RANGE:
      if (token == T_NUMBER) {
         lf->pint64_val = scan_pint64(lf, lf->str);
         lf->pint64_val2 = lf->pint64_val;
         token = T_PINT64;
      } else {
         char *p = strchr(lf->str, '-');
         if (!p) {
            scan_err2(lf, _("expected an integer or a range, got %s: %s"),
               lex_tok_to_str(token), lf->str);
            token = T_ERROR;
            break;
         }
         *p++ = 0;                       /* terminate first half of range */
         lf->pint64_val  = scan_pint64(lf, lf->str);
         lf->pint64_val2 = scan_pint64(lf, p);
         token = T_PINT64_RANGE;
      }
      break;

   case T_NAME:
      if (token != T_IDENTIFIER && token != T_UNQUOTED_STRING && token != T_QUOTED_STRING) {
         scan_err2(lf, _("expected a name, got %s: %s"),
               lex_tok_to_str(token), lf->str);
         token = T_ERROR;
      } else if (lf->str_len > MAX_RES_NAME_LENGTH) {
         scan_err3(lf, _("name %s length %d too long, max is %d\n"), lf->str,
            lf->str_len, MAX_RES_NAME_LENGTH);
         token = T_ERROR;
      }
      break;

   case T_STRING:
      if (token != T_IDENTIFIER && token != T_UNQUOTED_STRING && token != T_QUOTED_STRING) {
         scan_err2(lf, _("expected a string, got %s: %s"),
               lex_tok_to_str(token), lf->str);
         token = T_ERROR;
      } else {
         token = T_STRING;
      }
      break;


   default:
      break;                          /* no expectation given */
   }
   lf->token = token;                 /* set possible new token */
   return token;
}
