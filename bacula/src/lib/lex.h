/*
 *   lex.h  
 *
 *    Lexical scanning of configuration files, used by parsers.
 *
 *   Kern Sibbald, MM  
 *
 *   Version $Id$
 *
 */
/*
   Copyright (C) 2000, 2001, 2002 Kern Sibbald and John Walker

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

#ifndef _LEX_H
#define _LEX_H

/* Lex get_char() return values */
#define L_EOF                         (-1)
#define L_EOL                         (-2)

/* Internal tokens */
#define T_NONE                        100                              

/* Tokens returned by get_token() */
#define T_EOF                         101
#define T_NUMBER                      102
#define T_IPADDR                      103
#define T_IDENTIFIER                  104
#define T_UNQUOTED_STRING             105
#define T_QUOTED_STRING               106
#define T_BOB                         108  /* begin block */
#define T_EOB                         109  /* end of block */
#define T_EQUALS                      110
#define T_COMMA                       111
#define T_EOL                         112
#define T_SEMI                        113
#define T_ERROR                       200
/*
 * The following will be returned only if
 * the appropriate expect flag has been set   
 */
#define T_PINT32                      114  /* positive integer */
#define T_PINT32_RANGE                115  /* positive integer range */
#define T_INT32                       116  /* integer */
#define T_INT64                       117  /* 64 bit integer */
#define T_NAME                        118  /* name max 128 chars */
#define T_STRING                      119  /* string */

#define T_ALL                           0  /* no expectations */

/* Lexical state */
enum lex_state {
   lex_none,
   lex_comment,
   lex_number,
   lex_ip_addr,
   lex_identifier,
   lex_string,
   lex_quoted_string,
   lex_include
};

/* Lex scan options */
#define LOPT_NO_IDENT            0x1  /* No Identifiers -- use string */

/* Lexical context */
typedef struct s_lex_context {
   struct s_lex_context *next;        /* pointer to next lexical context */
   int options;                       /* scan options */
   char *fname;                       /* filename */
   FILE *fd;                          /* file descriptor */
   char line[MAXSTRING];              /* input line */
   char str[MAXSTRING];               /* string being scanned */
   int str_len;                       /* length of string */
   int line_no;                       /* file line number */
   int col_no;                        /* char position on line */
   enum lex_state state;              /* lex_state variable */
   int ch;                            /* last char/L_VAL returned by get_char */
   int token;
   uint32_t pint32_val;
   uint32_t pint32_val2;
   int32_t int32_val;
   int64_t int64_val;
} LEX;

#endif /* _LEX_H */
