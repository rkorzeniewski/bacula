/*	  
      Generalized console input/output handler			   
      A maintanable replacement for readline()

	 Kern Sibbald, December MMIII

      This code is in part derived from code that I wrote in
      1981, so some of it is a bit old and could use a cleanup.
	 
*/
/*
   Copyright (C) 1981-2004 Kern Sibbald and John Walker
		 Yes, that is 1981 no error.

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


#ifdef	TEST_PROGRAM
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <ctype.h> 
#else

/* We are in Bacula */
#include "bacula.h"

#endif

#include <termios.h>
#include <termcap.h>
#include "func.h"


/* From termios library */
extern char *BC;
extern char *UP;

/* Forward referenced functions */
static void sigintcatcher(int);
static void add_smap(char *str, int func);


/* Global variables */

static char *t_up = "\n";                    /* scroll up character */
static char *t_honk = "\007";                /* sound beep */
static char *t_il;			     /* insert line */
static char *t_dl;			     /* delete line */
static char *t_cs;			     /* clear screen */
static char *t_cl;			     /* clear line */
static int t_width = 79;		     /* terminal width */
static int t_height = 24;		     /* terminal height */
static int linsdel_ok = 0;		/* set if term has line insert & delete fncs */

static char *t_cm;			     /* cursor positioning */
static char *t_ti;			     /* init sequence */
static char *t_te;			     /* end sequence */
static char *t_do;			     /* down one line */
static char *t_sf;			     /* scroll screen one line up */

/* Keypad and Function Keys */
static char *kl;			     /* left key */
static char *kr;			     /* right */
static char *ku;			     /* up */
static char *kd;			     /* down */
static char *kh;			     /* home */
static char *kb;			     /* backspace */
static char *kD;			     /* delete key */
static char *kI;			     /* insert */
static char *kN;			     /* next page */
static char *kP;			     /* previous page */
static char *kH;			     /* home */
static char *kE;			     /* end */

#ifndef EOS
#define EOS  '\0'                     /* end of string terminator */
#endif

#define TRUE  1
#define FALSE 0
/*
 * Stab entry. Input chars (str), the length, and the desired
 *  func code.
 */
typedef struct s_stab {
   struct s_stab *next;
   int len;
   int func;
   char *str;
} stab_t;

#define MAX_STAB 30

static stab_t **stab = NULL;		     /* array of stabs by length */
static int num_stab;			     /* size of stab array */

static bool old_term_params_set = false;
static struct termios old_term_params;

/* Maintain lines in a doubly linked circular pool of lines. Each line is
   preceded by a header defined by the lstr structure */


struct lstr {			      /* line pool structure */
   struct lstr *prevl;		      /* link to previous line */
   struct lstr *nextl;		      /* link to next line */
   long len;			      /* length of line+header */
   char used;			      /* set if line valid */
   char line;			      /* line is actually varying length */
};

#ifdef unix
#define POOLEN 128000		      /* bytes in line pool */
#else
#define POOLEN 500		      /* bytes in line pool */
#endif
char pool[POOLEN];		      /* line pool */
#define PHDRL ((int)sizeof(struct lstr))  /* length of line header */

static struct lstr *lptr;	      /* current line pointer */
static struct lstr *slptr;	      /* store line pointer */
static int cl, cp;
static char *getnext(), *getprev();
static int first = 1;
static int mode_insert = 0;
static int mode_wspace = 1;	      /* words separated by spaces */


static short char_map[600]= {
   0,				       F_SOL,	 /* ^a Line start */
   F_PRVWRD, /* ^b Previous word */    F_BREAK,  /* ^C break */
   F_DELCHR, /* ^D Delete character */ F_EOL,	 /* ^e End of line */
   F_CSRRGT, /* ^f Right */	       F_TABBAK, /* ^G Back tab */
   F_CSRLFT, /* ^H Left */	       F_TAB,	 /* ^I Tab */
   F_CSRDWN, /* ^J Down */	       F_DELEOL, /* ^K kill to eol */
   F_CLRSCRN,/* ^L clear screen */     F_RETURN, /* ^M Carriage return */
   F_RETURN, /* ^N enter line  */      F_CONCAT, /* ^O Concatenate lines */
   F_CSRUP,  /* ^P cursor up */        F_TINS,	 /* ^Q Insert character mode */
   F_PAGUP,  /* ^R Page up */	       F_CENTER, /* ^S Center text */
   F_PAGDWN, /* ^T Page down */        F_DELSOL, /* ^U delete to start of line */
   F_DELWRD, /* ^V Delete word */      F_PRVWRD, /* ^W Previous word */
   F_NXTMCH, /* ^X Next match */       F_DELEOL, /* ^Y Delete to end of line */
   F_BACKGND,/* ^Z Background */       0x1B,	 /* ^[=ESC escape */
   F_TENTRY, /* ^\ Entry mode */       F_PASTECB,/* ^]=paste clipboard */
   F_HOME,   /* ^^ Home */	       F_ERSLIN, /* ^_ Erase line */

   ' ','!','"','#','$','%','&','\047',
   '(',')','*','+','\054','-','.','/',
   '0','1','2','3','4','5','6','7',
   '8','9',':',';','<','=','>','?',
   '@','A','B','C','D','E','F','G',
   'H','I','J','K','L','M','N','O',
   'P','Q','R','S','T','U','V','W',
   'X','Y','Z','[','\\',']','^','_',
   '\140','a','b','c','d','e','f','g',
   'h','i','j','k','l','m','n','o',
   'p','q','r','s','t','u','v','w',
   'x','y','z','{','|','}','\176',F_ERSCHR  /* erase character */

  };


/* Local variables */

#define CR '\r'                       /* carriage return */


/* Function Prototypes */

static int input_char(void);
static int t_gnc(void);
static void insert_space(char *curline, int line_len);
static void forward(int i, char *str, int str_len);
static void backup(int i);
static void delchr(int cnt, char *curline, int line_len);
static int iswordc(char c);
static int  next_word(char *ldb_buf);
static int  prev_word(char *ldb_buf);
static void prtcur(char *str);
static void poolinit(void);
static char * getnext(void);
static char * getprev(void);
static void putline(char *newl, int newlen);
static void t_honk_horn(void);
static void t_insert_line(void);
static void t_delete_line(void);
static void t_clrline(int pos, int width);
void t_sendl(char *msg, int len);
void t_send(char *msg);
void t_char(char c);
static void asclrs();
static void ascurs(int y, int x);

static void rawmode(FILE *input);
static void normode(void);
static int t_getch();
static void asclrl(int pos, int width);
static void asinsl();
static void asdell();

int input_line(char *string, int length);
void con_term();
void trapctlc();
int usrbrk();
void clrbrk();
    
void con_init(FILE *input)
{
   atexit(con_term); 
   rawmode(input);
   trapctlc();
}

/*
 * Zed control keys
 */
void con_set_zed_keys(void)
{
   char_map[1]	= F_NXTWRD; /* ^A Next Word */
   char_map[2]	= F_SPLIT;  /* ^B Split line */
   char_map[3]	= F_EOI;    /* ^C Quit */
   char_map[4]	= F_DELCHR; /* ^D Delete character */
   char_map[5]	= F_EOF;    /* ^E End of file */
   char_map[6]	= F_INSCHR; /* ^F Insert character */
   char_map[7]	= F_TABBAK; /* ^G Back tab */
   char_map[8]	= F_CSRLFT; /* ^H Left */
   char_map[9]	= F_TAB;    /* ^I Tab */
   char_map[10] = F_CSRDWN; /* ^J Down */
   char_map[11] = F_CSRUP;  /* ^K Up */
   char_map[12] = F_CSRRGT; /* ^L Right */
   char_map[13] = F_RETURN; /* ^M Carriage return */
   char_map[14] = F_EOL;    /* ^N End of line */
   char_map[15] = F_CONCAT; /* ^O Concatenate lines */
   char_map[16] = F_MARK;   /* ^P Set marker */
   char_map[17] = F_TINS;   /* ^Q Insert character mode */
   char_map[18] = F_PAGUP;  /* ^R Page up */
   char_map[19] = F_CENTER; /* ^S Center text */
   char_map[20] = F_PAGDWN; /* ^T Page down */
   char_map[21] = F_SOL;    /* ^U Line start */
   char_map[22] = F_DELWRD; /* ^V Delete word */
   char_map[23] = F_PRVWRD; /* ^W Previous word */
   char_map[24] = F_NXTMCH; /* ^X Next match */
   char_map[25] = F_DELEOL; /* ^Y Delete to end of line */
   char_map[26] = F_DELLIN; /* ^Z Delete line */
   /* 27 = ESC */
   char_map[28] = F_TENTRY; /* ^\ Entry mode */
   char_map[29] = F_PASTECB;/* ^]=paste clipboard */
   char_map[30] = F_HOME;   /* ^^ Home */
   char_map[31] = F_ERSLIN; /* ^_ Erase line */

}

void con_term()
{
   normode();
}

#ifdef TEST_PROGRAM
/*
 * Guarantee that the string is properly terminated */
char *bstrncpy(char *dest, const char *src, int maxlen)
{
   strncpy(dest, src, maxlen-1);
   dest[maxlen-1] = 0;
   return dest;
}
#endif


/*
 * New style string mapping to function code
 */
static int do_smap(int c)
{
    char str[MAX_STAB];
    int len = 0; 
    stab_t *tstab;
    int i, found;

    len = 1;
    str[0] = c;
    str[1] = 0;

    if (c != 27) {
       c = char_map[c];
    }
    if (c <= 0) {
       return c;
    }
    for ( ;; ) {
       found = 0;
       for (i=len-1; i<MAX_STAB; i++) {
	  for (tstab=stab[i]; tstab; tstab=tstab->next) {
	     if (strncmp(str, tstab->str, len) == 0) {
		if (len == tstab->len) {
		   return tstab->func;
		}
		found = 1;
		break;		      /* found possibility continue searching */
	     }
	  }
       }
       if (!found) {
	  return len==1?c:0;
       }
       /* found partial match, so get next character and retry */
       str[len++] = t_gnc();
       str[len] = 0;
    }
}

#ifdef DEBUG_x
static void dump_stab()
{
    int i, j, c;
    stab_t *tstab;
    char buf[100];

    for (i=0; i<MAX_STAB; i++) {
       for (tstab=stab[i]; tstab; tstab=tstab->next) {
	  for (j=0; j<tstab->len; j++) {
	     c = tstab->str[j];
	     if (c < 0x20 || c > 0x7F) {
                sprintf(buf, " 0x%x ", c);
		t_send(buf);
	     } else {
		buf[0] = c;
		buf[1] = 0;
		t_sendl(buf, 1);
	     }
	  }
          sprintf(buf, " func=%d len=%d\n\r", tstab->func, tstab->len);
	  t_send(buf);
       }
    }
}
#endif

/*
 * New routine. Add string to string->func mapping table.
 */
static void add_smap(char *str, int func)
{
   stab_t *tstab;
   int len;
  
   if (!str) {
      return;
   }
   len = strlen(str);
   if (len == 0) {
/*    errmsg("String for func %d is zero length\n", func); */
      return;
   }
   tstab = (stab_t *)malloc(sizeof(stab_t));
   memset(tstab, 0, sizeof(stab_t));
   tstab->len = len;
   tstab->str = (char *)malloc(tstab->len + 1);
   bstrncpy(tstab->str, str, tstab->len + 1);
   tstab->func = func;
   if (tstab->len > num_stab) {
      printf("stab string too long %d. Max is %d\n", tstab->len, num_stab);
      exit(1);
   }
   tstab->next = stab[tstab->len-1];
   stab[tstab->len-1] = tstab;
/* printf("Add_smap tstab=%x len=%d func=%d tstab->next=%x\n\r", tstab, len, 
	  func, tstab->next); */

}


/* Get the next character from the terminal - performs table lookup on
   the character to do the desired translation */
static int
input_char()
{
    int c;

    if ((c=t_gnc()) <= 599) {	      /* IBM generates codes up to 260 */
	  c = do_smap(c);
    } else if (c > 1000) {	      /* stuffed function */
       c -= 1000;		      /* convert back to function code */
    }
    if (c <= 0) {
	t_honk_horn();
    }
    /* if we got a screen size escape sequence, read height, width */
    if (c == F_SCRSIZ) {
       int y, x;
       y = t_gnc() - 0x20;	  /* y */
       x = t_gnc() - 0x20;	  /* x */
       c = input_char();
    }
    return c;
}


/* Get a complete input line */

int
input_line(char *string, int length)
{
   char curline[2000];		      /* edit buffer */
   int noline;
   int c;

    if (first) {
       poolinit();		     /* build line pool */
       first = 0;
    }
    noline = 1; 		      /* no line fetched yet */
    for (cl=cp=0; cl<length && cl<(int)sizeof(curline); ) {
       if (usrbrk()) {
	  clrbrk();
	  break;
       }
       switch (c=(int)input_char()) {
       case F_RETURN:		     /* CR */
           t_sendl("\r\n", 2);       /* yes, print it and */
	   goto done;		     /* get out */
       case F_CLRSCRN:		     /* clear screen */
	  asclrs();
	  t_sendl(curline, cl);
	  ascurs(0, cp);
	  break;
       case F_CSRUP:
	   if (noline) {	     /* no line fetched yet */
	       getnext();	     /* getnext so getprev gets current */
	       noline = 0;	     /* we now have line */
	   }
	   bstrncpy(curline, getprev(), sizeof(curline));
	   prtcur(curline);
	   break;
       case F_CSRDWN:
	   noline = 0;		     /* mark line fetched */
	   bstrncpy(curline, getnext(), sizeof(curline));
	   prtcur(curline);
	   break;
       case F_INSCHR:
	   insert_space(curline, sizeof(curline));
	   break;
       case F_DELCHR:
	   delchr(1, curline, sizeof(curline));       /* delete one character */
	   break;
       case F_CSRLFT:		     /* Backspace */
	   backup(1);
	   break;
       case F_CSRRGT:
	   forward(1,curline, sizeof(curline));
	   break;
       case F_ERSCHR:		     /* Rubout */
	   backup(1);
	   delchr(1, curline, sizeof(curline));
	   break;
       case F_DELEOL:
	   t_clrline(0, t_width);
	   if (cl > cp)
	       cl = cp;
	   break;
       case F_NXTWRD:
	   forward(next_word(curline),curline, sizeof(curline));
	   break;
       case F_PRVWRD:
	   backup(prev_word(curline));
	   break;
       case F_DELWRD:
	   delchr(next_word(curline), curline, sizeof(curline)); /* delete word */
	   break;
       case F_NXTMCH:		     /* Ctl-X */
	   if (cl==0) {
	       *string = EOS;	     /* terminate string */
	       return(c);	     /* give it to him */
	   }
	   /* Note fall through */
       case F_DELLIN:
       case F_ERSLIN:
	   backup(cp);		     /* backup to beginning of line */
	   t_clrline(0,t_width);     /* erase line */
	   cp = 0;
	   cl = 0;		     /* reset cursor counter */
	   break;
       case F_SOL:
	   backup(cp);
	   break;
       case F_EOL:
	   while (cp < cl) {
	       forward(1,curline, sizeof(curline));
	   }
	   while (cp > cl) {
	       backup(1);
	   }
	   break;
       case F_TINS:		     /* toggle insert mode */
	   mode_insert = !mode_insert;	/* flip bit */
	   break;
       default:
	   if (c > 255) {	     /* function key hit */
	       if (cl==0) {	     /* if first character then */
		  *string = EOS;     /* terminate string */
		  return c;	     /* return it */
	       }
	       t_honk_horn();	     /* complain */
	   } else {
	       if (mode_insert) {
		  insert_space(curline, sizeof(curline));
	       }
	       curline[cp++] = c;    /* store character in line being built */
	       t_char((char)c);      /* echo character to terminal */
	       if (cp > cl) {
		  cl = cp;	     /* keep current length */
	       }
	   }
	   break;
       }			     /* end switch */
    }
/* If we fall through here rather than goto done, the line is too long
   simply return what we have now. */
done:
   curline[cl++] = EOS; 	     /* terminate */
   bstrncpy(string,curline,length);	      /* return line to caller */
   /* Note, put line zaps curline */
   putline(curline,cl); 	     /* save line for posterity */
   return 0;			     /* give it to him/her */
}

/* Insert a space at the current cursor position */
static void
insert_space(char *curline, int curline_len)
{
   int i;

   if (cp > cl || cl+1 > curline_len) return;
   /* Note! source and destination overlap */
   memmove(&curline[cp+1],&curline[cp],i=cl-cp);
   cl++;
   i++;
   curline[cp] = ' ';
   forward(i,curline, curline_len);
   backup(i);
}


/* Move cursor forward keeping characters under it */
static void
forward(int i, char *str, int str_len)
{
   while (i--) {
      if (cp > str_len) {
	 return;
      }
      if (cp>=cl) {
          t_char(' ');
          str[cp+1] = ' ';
      } else {
	  t_char(str[cp]);
      }
      cp++;
   }
}

/* Backup cursor keeping characters under it */
static void
backup(int i)
{
    for ( ; i && cp; i--,cp--) {
       t_char('\010');
    }
}

/* Delete the character under the cursor */
static void
delchr(int cnt, char *curline, int line_len) 
{
   register int i;

   if (cp > cl)
      return;
   if ((i=cl-cp-cnt+1) > 0) {
      memcpy(&curline[cp], &curline[cp+cnt],i);
   }
   curline[cl -= cnt] = EOS;
   t_clrline(0,t_width);
   if (cl > cp) {
      forward(i=cl-cp,curline, line_len);
      backup(i);
   }
}

/* Determine if character is part of a word */
static int
iswordc(char c)
{
   if (mode_wspace)
      return !isspace(c);
   if (c >= '0' && c <= '9')
      return TRUE;
   if (c == '$' || c == '%')
      return TRUE;
   return isalpha(c);
}

/* Return number of characters to get to next word */
static int
next_word(char *ldb_buf)
{
   int ncp;

   if (cp > cl)
      return 0;
   ncp = cp;
   for ( ; ncp<cl && iswordc(*(ldb_buf+ncp)); ncp++) ;
   for ( ; ncp<cl && !iswordc(*(ldb_buf+ncp)); ncp++) ;
   return ncp-cp;
}

/* Return number of characters to get to previous word */
static int
prev_word(char *ldb_buf)
{
    int ncp, i;

    if (cp == 0)		      /* if at begin of line stop now */
	return 0;
    if (cp > cl)		      /* if past eol start at eol */
	ncp=cl+1;
    else
	ncp = cp;
    /* backup to end of previous word - i.e. skip special chars */
    for (i=ncp-1; i && !iswordc(*(ldb_buf+i)); i--) ;
    if (i == 0) {		      /* at beginning of line? */
	return cp;		      /* backup to beginning */
    }
    /* now move back through word to beginning of word */
    for ( ; i && iswordc(*(ldb_buf+i)); i--) ;
    ncp = i+1;			      /* position to first char of word */
    if (i==0 && iswordc(*ldb_buf))    /* check for beginning of line */
	ncp = 0;
    return cp-ncp;		      /* return count */
}

/* Display new current line */
static void
prtcur(char *str)
{
    backup(cp);
    t_clrline(0,t_width);
    cp = cl = strlen(str);
    t_sendl(str,cl);
}


/* Initialize line pool. Split pool into two pieces. */
static void
poolinit()
{
   slptr = lptr = (struct lstr *)pool;
   lptr->nextl = lptr;
   lptr->prevl = lptr;
   lptr->used = 1;
   lptr->line = 0;
   lptr->len = POOLEN;
}


/* Return pointer to next line in the pool and advance current line pointer */
static char *
getnext()
{
   do { 			     /* find next used line */
      lptr = lptr->nextl;
   } while (!lptr->used);
   return (char *)&lptr->line;
}

/* Return pointer to previous line in the pool */
static char *
getprev()
{
   do { 			     /* find previous used line */
      lptr = lptr->prevl;
   } while (!lptr->used);
   return (char *)&lptr->line;
}

static void
putline(char *newl, int newlen)
{
   struct lstr *nptr;		     /* points to next line */
   char *p;

   lptr = slptr;		     /* get ptr to last line stored */
   lptr = lptr->nextl;		     /* advance pointer */
   if ((char *)lptr-pool+newlen+PHDRL > POOLEN) { /* not enough room */
       lptr->used = 0;		     /* delete line */
       lptr = (struct lstr *)pool;   /* start at beginning of buffer */
   }
   while (lptr->len < newlen+PHDRL) { /* concatenate buffers */
       nptr = lptr->nextl;	     /* point to next line */
       lptr->nextl = nptr->nextl;    /* unlink it from list */
       nptr->nextl->prevl = lptr;
       lptr->len += nptr->len;
   }
   if (lptr->len > newlen + 2 * PHDRL) { /* split buffer */
       nptr = (struct lstr *)((char *)lptr + newlen + PHDRL);
       /* Appropriate byte alignment - normally 2 byte, but on
	  sparc we need 4 byte alignment, so we always do 4 */
       if (((long unsigned)nptr & 3) != 0) { /* test four byte alignment */
	   p = (char *)nptr;
	   nptr = (struct lstr *)((((long unsigned) p) & ~3) + 4);
       }
       nptr->len = lptr->len - ((char *)nptr - (char *)lptr);
       lptr->len -= nptr->len;
       nptr->nextl = lptr->nextl;    /* link in new buffer */
       lptr->nextl->prevl = nptr;
       lptr->nextl = nptr;
       nptr->prevl = lptr;
       nptr->used = 0;
   }
   memcpy(&lptr->line,newl,newlen);
   lptr->used = 1;		     /* mark line used */
   slptr = lptr;		     /* save as stored line */
}

#ifdef	DEBUGOUT
static void
dump(struct lstr *ptr, char *msg)
{
    printf("%s buf=%x nextl=%x prevl=%x len=%d used=%d\n",
	msg,ptr,ptr->nextl,ptr->prevl,ptr->len,ptr->used);
    if (ptr->used)
        printf("line=%s\n",&ptr->line);
}
#endif	/* DEBUGOUT */


/* Honk horn on terminal */
static void
t_honk_horn()
{
   t_send(t_honk);
}

/* Insert line on terminal */
static void
t_insert_line()
{
   asinsl();
}

/* Delete line from terminal */
static void
t_delete_line()
{
   asdell();
}

/* clear line from pos to width */
static void
t_clrline(int pos, int width)
{
    asclrl(pos, width); 	  /* clear to end of line */
}

/* Helper function to add string preceded by 
 *  ESC to smap table */
static void add_esc_smap(char *str, int func)
{
   char buf[1000];
   buf[0] = 0x1B;		      /* esc */
   bstrncpy(buf+1, str, sizeof(buf)-1);
   add_smap(buf, func);
}

/* Set raw mode on terminal file.  Basically, get the terminal into a
   mode in which all characters can be read as they are entered.  CBREAK
   mode is not sufficient.
 */
static void rawmode(FILE *input)
{
   struct termios t;
   static char term_buf[2048];
   static char *term_buffer = term_buf;
   char *termtype = (char *)getenv("TERM");

   /* Make sure we are dealing with a terminal */
   if (!isatty(fileno(input))) {
      return;
   }
   if (tcgetattr(0, &old_term_params) != 0) {
      printf("conio: Cannot tcgetattr()\n");
      exit(1);
   }
   old_term_params_set = true;
   t = old_term_params; 			
   t.c_cc[VMIN] = 1; /* satisfy read after 1 char */
   t.c_cc[VTIME] = 0;
   t.c_iflag &= ~(BRKINT | IGNPAR | PARMRK | INPCK | 
		  ISTRIP | ICRNL | IXON | IXOFF | INLCR | IGNCR);     
   t.c_iflag |= IGNBRK;
   t.c_oflag |= ONLCR;
   t.c_lflag &= ~(ECHO | ECHOE | ECHOK | ECHONL | ICANON |
		  NOFLSH | TOSTOP);
   tcflush(0, TCIFLUSH);
   if (tcsetattr(0, TCSANOW, &t) == -1) {
      printf("Cannot tcsetattr()\n");
   }

   /* Defaults, the main program can override these */
   signal(SIGQUIT, SIG_IGN);
   signal(SIGHUP, SIG_IGN);
// signal(SIGSTOP, SIG_IGN);
   signal(SIGINT, sigintcatcher);
   signal(SIGWINCH, SIG_IGN);	
   signal(SIGQUIT, SIG_IGN);
   signal(SIGCHLD, SIG_IGN);
// signal(SIGTSTP, SIG_IGN);

   if (!termtype) {
      printf("Cannot get terminal type.\n");
      normode();
      exit(1);
   }
   if (tgetent(term_buffer, termtype) < 0) {
      printf("Cannot get terminal termcap entry.\n");
      normode();
      exit(1);
   }
   t_width = t_height = -1;
   t_width = tgetnum("co") - 1;
   t_height = tgetnum("li");
   BC = NULL;
   UP = NULL;
   t_cm = (char *)tgetstr("cm", &term_buffer);
   t_cs = (char *)tgetstr("cl", &term_buffer); /* clear screen */
   t_cl = (char *)tgetstr("ce", &term_buffer); /* clear line */
   t_dl = (char *)tgetstr("dl", &term_buffer); /* delete line */
   t_il = (char *)tgetstr("al", &term_buffer); /* insert line */
   t_honk = (char *)tgetstr("bl", &term_buffer); /* beep */
   t_ti = (char *)tgetstr("ti", &term_buffer);
   t_te = (char *)tgetstr("te", &term_buffer);
   t_up = (char *)tgetstr("up", &term_buffer);
   t_do = (char *)tgetstr("do", &term_buffer);
   t_sf = (char *)tgetstr("sf", &term_buffer);

   num_stab = MAX_STAB; 		 /* get default stab size */
   stab = (stab_t **)malloc(sizeof(stab_t *) * num_stab);
   memset(stab, 0, sizeof(stab_t *) * num_stab);

   /* Key bindings */
   kl = (char *)tgetstr("kl", &term_buffer);
   kr = (char *)tgetstr("kr", &term_buffer);
   ku = (char *)tgetstr("ku", &term_buffer);
   kd = (char *)tgetstr("kd", &term_buffer);
   kh = (char *)tgetstr("kh", &term_buffer);
   kb = (char *)tgetstr("kb", &term_buffer);
   kD = (char *)tgetstr("kD", &term_buffer);
   kI = (char *)tgetstr("kI", &term_buffer);
   kN = (char *)tgetstr("kN", &term_buffer);
   kP = (char *)tgetstr("kP", &term_buffer);
   kH = (char *)tgetstr("kH", &term_buffer);
   kE = (char *)tgetstr("kE", &term_buffer);

   add_smap(kl, F_CSRLFT);
   add_smap(kr, F_CSRRGT);
   add_smap(ku, F_CSRUP);
   add_smap(kd, F_CSRDWN);
   add_smap(kI, F_TINS);
   add_smap(kN, F_PAGDWN);
   add_smap(kP, F_PAGUP);
   add_smap(kH, F_HOME);
   add_smap(kE, F_EOF);


   add_esc_smap("[A",   F_CSRUP);
   add_esc_smap("[B",   F_CSRDWN);
   add_esc_smap("[C",   F_CSRRGT);
   add_esc_smap("[D",   F_CSRLFT);
   add_esc_smap("[1~",  F_HOME);
   add_esc_smap("[2~",  F_TINS);
   add_esc_smap("[3~",  F_DELCHR);
   add_esc_smap("[4~",  F_EOF);
   add_esc_smap("f",    F_NXTWRD);
   add_esc_smap("b",    F_PRVWRD);
}


/* Restore tty mode */
static void normode()
{
   if (old_term_params_set) {
      tcsetattr(0, TCSANOW, &old_term_params);
      old_term_params_set = false;
   }
}

/* Get next character from terminal/script file/unget buffer */
static int
t_gnc()
{
    return t_getch();
}


/* Get next character from OS */
static int t_getch(void)
{
   char c;

   if (read(0, &c, 1) != 1) {
      c = 0;
   }
   return (int)c;   
}
    
/* Send message to terminal - primitive routine */
void
t_sendl(char *msg, int len)
{
   write(1, msg, len);
}

void
t_send(char *msg)
{
   if (msg == NULL) {
      return;
   }
   t_sendl(msg, strlen(msg));	 /* faster than one char at time */
}

/* Send single character to terminal - primitive routine - */
void
t_char(char c)
{
   write(1, &c, 1);
}


static int brkflg = 0;		    /* set on user break */

/* Routine to return true if user types break */
int usrbrk()
{
   return brkflg;
}

/* Clear break flag */
void clrbrk()
{
   brkflg = 0;

}

/* Interrupt caught here */
static void sigintcatcher(int sig)
{
   brkflg++;
   if (brkflg > 3) {
      normode();
      exit(1);
   }
   signal(SIGINT, sigintcatcher);
}


/* Trap Ctl-C */
void trapctlc()
{
   signal(SIGINT, sigintcatcher);
}


/* ASCLRL() -- Clear to end of line from current position */
static void asclrl(int pos, int width) 
{
   int i;

   if (t_cl) {
       t_send(t_cl);		     /* use clear to eol function */
       return;
   }
   if (pos==1 && linsdel_ok) {
       t_delete_line(); 	     /* delete line */
       t_insert_line(); 	     /* reinsert it */
       return;
   }
   for (i=1; i<=width-pos+1; i++)
       t_char(' ');                  /* last resort, blank it out */
   for (i=1; i<=width-pos+1; i++)    /* backspace to original position */
       t_char(0x8);
   return;
  
}


/* ASCURS -- Set cursor position */
static void ascurs(int y, int x)
{
   t_send((char *)tgoto(t_cm, x, y));
}
											

/* ASCLRS -- Clear whole screen */
static void asclrs() 
{
   ascurs(0,0);
   t_send(t_cs);
}



/* ASINSL -- insert new line after cursor */
static void asinsl()
{
   t_clrline(0, t_width);
   t_send(t_il);		      /* insert before */
}

/* ASDELL -- Delete line at cursor */
static void asdell()
{
   t_send(t_dl);
}
