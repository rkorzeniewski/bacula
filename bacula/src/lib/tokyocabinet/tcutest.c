/*************************************************************************************************
 * The test cases of the utility API
 *                                                      Copyright (C) 2006-2008 Mikio Hirabayashi
 * This file is part of Tokyo Cabinet.
 * Tokyo Cabinet is free software; you can redistribute it and/or modify it under the terms of
 * the GNU Lesser General Public License as published by the Free Software Foundation; either
 * version 2.1 of the License or any later version.  Tokyo Cabinet is distributed in the hope
 * that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 * You should have received a copy of the GNU Lesser General Public License along with Tokyo
 * Cabinet; if not, write to the Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307 USA.
 *************************************************************************************************/


#include <tcutil.h>
#include "myconf.h"

#define RECBUFSIZ      32                // buffer for records


/* global variables */
const char *g_progname;                  // program name


/* function prototypes */
int main(int argc, char **argv);
static void usage(void);
static void iprintf(const char *format, ...);
static int myrand(int range);
static int runxstr(int argc, char **argv);
static int runlist(int argc, char **argv);
static int runmap(int argc, char **argv);
static int runmdb(int argc, char **argv);
static int runmisc(int argc, char **argv);
static int runwicked(int argc, char **argv);
static int procxstr(int rnum);
static int proclist(int rnum, int anum);
static int procmap(int rnum, int bnum);
static int procmdb(int rnum, int bnum);
static int procmisc(int rnum);
static int procwicked(int rnum);


/* main routine */
int main(int argc, char **argv){
  g_progname = argv[0];
  srand((unsigned int)(tctime() * 1000) % UINT_MAX);
  if(argc < 2) usage();
  int rv = 0;
  if(!strcmp(argv[1], "xstr")){
    rv = runxstr(argc, argv);
  } else if(!strcmp(argv[1], "list")){
    rv = runlist(argc, argv);
  } else if(!strcmp(argv[1], "map")){
    rv = runmap(argc, argv);
  } else if(!strcmp(argv[1], "mdb")){
    rv = runmdb(argc, argv);
  } else if(!strcmp(argv[1], "misc")){
    rv = runmisc(argc, argv);
  } else if(!strcmp(argv[1], "wicked")){
    rv = runwicked(argc, argv);
  } else {
    usage();
  }
  return rv;
}


/* print the usage and exit */
static void usage(void){
  fprintf(stderr, "%s: test cases of the utility API of Tokyo Cabinet\n", g_progname);
  fprintf(stderr, "\n");
  fprintf(stderr, "usage:\n");
  fprintf(stderr, "  %s xstr rnum\n", g_progname);
  fprintf(stderr, "  %s list rnum [anum]\n", g_progname);
  fprintf(stderr, "  %s map rnum [bnum]\n", g_progname);
  fprintf(stderr, "  %s mdb rnum [bnum]\n", g_progname);
  fprintf(stderr, "  %s misc rnum\n", g_progname);
  fprintf(stderr, "  %s wicked rnum\n", g_progname);
  fprintf(stderr, "\n");
  exit(1);
}


/* print formatted information string and flush the buffer */
static void iprintf(const char *format, ...){
  va_list ap;
  va_start(ap, format);
  vprintf(format, ap);
  fflush(stdout);
  va_end(ap);
}


/* get a random number */
static int myrand(int range){
  return (int)((double)range * rand() / (RAND_MAX + 1.0));
}


/* parse arguments of xstr command */
static int runxstr(int argc, char **argv){
  char *rstr = NULL;
  for(int i = 2; i < argc; i++){
    if(!rstr && argv[i][0] == '-'){
      usage();
    } else if(!rstr){
      rstr = argv[i];
    } else {
      usage();
    }
  }
  if(!rstr) usage();
  int rnum = atoi(rstr);
  if(rnum < 1) usage();
  int rv = procxstr(rnum);
  return rv;
}


/* parse arguments of list command */
static int runlist(int argc, char **argv){
  char *rstr = NULL;
  char *astr = NULL;
  for(int i = 2; i < argc; i++){
    if(!rstr && argv[i][0] == '-'){
      usage();
    } else if(!rstr){
      rstr = argv[i];
    } else if(!astr){
      astr = argv[i];
    } else {
      usage();
    }
  }
  if(!rstr) usage();
  int rnum = atoi(rstr);
  if(rnum < 1) usage();
  int anum = astr ? atoi(astr) : -1;
  int rv = proclist(rnum, anum);
  return rv;
}


/* parse arguments of map command */
static int runmap(int argc, char **argv){
  char *rstr = NULL;
  char *bstr = NULL;
  for(int i = 2; i < argc; i++){
    if(!rstr && argv[i][0] == '-'){
      usage();
    } else if(!rstr){
      rstr = argv[i];
    } else if(!bstr){
      bstr = argv[i];
    } else {
      usage();
    }
  }
  if(!rstr) usage();
  int rnum = atoi(rstr);
  if(rnum < 1) usage();
  int bnum = bstr ? atoi(bstr) : -1;
  int rv = procmap(rnum, bnum);
  return rv;
}


/* parse arguments of mdb command */
static int runmdb(int argc, char **argv){
  char *rstr = NULL;
  char *bstr = NULL;
  for(int i = 2; i < argc; i++){
    if(!rstr && argv[i][0] == '-'){
      usage();
    } else if(!rstr){
      rstr = argv[i];
    } else if(!bstr){
      bstr = argv[i];
    } else {
      usage();
    }
  }
  if(!rstr) usage();
  int rnum = atoi(rstr);
  if(rnum < 1) usage();
  int bnum = bstr ? atoi(bstr) : -1;
  int rv = procmdb(rnum, bnum);
  return rv;
}


/* parse arguments of misc command */
static int runmisc(int argc, char **argv){
  char *rstr = NULL;
  for(int i = 2; i < argc; i++){
    if(!rstr && argv[i][0] == '-'){
      usage();
    } else if(!rstr){
      rstr = argv[i];
    } else {
      usage();
    }
  }
  if(!rstr) usage();
  int rnum = atoi(rstr);
  if(rnum < 1) usage();
  int rv = procmisc(rnum);
  return rv;
}


/* parse arguments of wicked command */
static int runwicked(int argc, char **argv){
  char *rstr = NULL;
  for(int i = 2; i < argc; i++){
    if(!rstr && argv[i][0] == '-'){
      usage();
    } else if(!rstr){
      rstr = argv[i];
    } else {
      usage();
    }
  }
  if(!rstr) usage();
  int rnum = atoi(rstr);
  if(rnum < 1) usage();
  int rv = procwicked(rnum);
  return rv;
}


/* perform xstr command */
static int procxstr(int rnum){
  iprintf("<Extensible String Writing Test>\n  rnum=%d\n\n", rnum);
  double stime = tctime();
  TCXSTR *xstr = tcxstrnew();
  for(int i = 1; i <= rnum; i++){
    char buf[RECBUFSIZ];
    int len = sprintf(buf, "%08d", i);
    tcxstrcat(xstr, buf, len);
    if(rnum > 250 && i % (rnum / 250) == 0){
      putchar('.');
      fflush(stdout);
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
    }
  }
  tcxstrdel(xstr);
  iprintf("time: %.3f\n", tctime() - stime);
  iprintf("ok\n\n");
  return 0;
}


/* perform list command */
static int proclist(int rnum, int anum){
  iprintf("<List Writing Test>\n  rnum=%d  anum=%d\n\n", rnum, anum);
  double stime = tctime();
  TCLIST *list = (anum > 0) ? tclistnew2(anum) : tclistnew();
  for(int i = 1; i <= rnum; i++){
    char buf[RECBUFSIZ];
    int len = sprintf(buf, "%08d", i);
    tclistpush(list, buf, len);
    if(rnum > 250 && i % (rnum / 250) == 0){
      putchar('.');
      fflush(stdout);
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
    }
  }
  tclistdel(list);
  iprintf("time: %.3f\n", tctime() - stime);
  iprintf("ok\n\n");
  return 0;
}


/* perform map command */
static int procmap(int rnum, int bnum){
  iprintf("<Map Writing Test>\n  rnum=%d\n\n", rnum);
  double stime = tctime();
  TCMAP *map = (bnum > 0) ? tcmapnew2(bnum) : tcmapnew();
  for(int i = 1; i <= rnum; i++){
    char buf[RECBUFSIZ];
    int len = sprintf(buf, "%08d", i);
    tcmapput(map, buf, len, buf, len);
    if(rnum > 250 && i % (rnum / 250) == 0){
      putchar('.');
      fflush(stdout);
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
    }
  }
  tcmapdel(map);
  iprintf("time: %.3f\n", tctime() - stime);
  iprintf("ok\n\n");
  return 0;
}


/* perform mdb command */
static int procmdb(int rnum, int bnum){
  iprintf("<On-memory Database Writing Test>\n  rnum=%d\n\n", rnum);
  double stime = tctime();
  TCMDB *mdb = (bnum > 0) ? tcmdbnew2(bnum) : tcmdbnew();
  for(int i = 1; i <= rnum; i++){
    char buf[RECBUFSIZ];
    int len = sprintf(buf, "%08d", i);
    tcmdbput(mdb, buf, len, buf, len);
    if(rnum > 250 && i % (rnum / 250) == 0){
      putchar('.');
      fflush(stdout);
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
    }
  }
  iprintf("record number: %llu\n", (unsigned long long)tcmdbrnum(mdb));
  iprintf("size: %llu\n", (unsigned long long)tcmdbmsiz(mdb));
  tcmdbdel(mdb);
  iprintf("time: %.3f\n", tctime() - stime);
  iprintf("ok\n\n");
  return 0;
}


/* perform misc command */
static int procmisc(int rnum){
  iprintf("<Miscellaneous Test>\n  rnum=%d\n\n", rnum);
  double stime = tctime();
  bool err = false;
  for(int i = 1; i <= rnum && !err; i++){
    const char *str = "5%2+3-1=4 \"Yes/No\" <a&b>";
    int slen = strlen(str);
    char *buf, *dec;
    int bsiz, dsiz, jl;
    time_t date, ddate;
    TCXSTR *xstr;
    TCLIST *list;
    TCMAP *map;
    buf = tcmemdup(str, slen);
    xstr = tcxstrfrommalloc(buf, slen);
    buf = tcxstrtomalloc(xstr);
    if(strcmp(buf, str)) err = true;
    free(buf);
    if(tclmax(1, 2) != 2) err = true;
    if(tclmin(1, 2) != 1) err = true;
    tclrand();
    if(tcdrand() >= 1.0) err = true;
    tcdrandnd(50, 10);
    if(tcstricmp("abc", "ABC") != 0) err = true;
    if(!tcstrfwm("abc", "ab") || !tcstrifwm("abc", "AB")) err = true;
    if(!tcstrbwm("abc", "bc") || !tcstribwm("abc", "BC")) err = true;
    if(tcstrdist("abcde", "abdfgh") != 4 || tcstrdist("abdfgh", "abcde") != 4) err = true;
    if(tcstrdistutf("abcde", "abdfgh") != 4 || tcstrdistutf("abdfgh", "abcde") != 4) err = true;
    buf = tcmemdup("abcde", 5);
    tcstrtoupper(buf);
    if(strcmp(buf, "ABCDE")) err = true;
    tcstrtolower(buf);
    if(strcmp(buf, "abcde")) err = true;
    free(buf);
    buf = tcmemdup("  ab  cd  ", 10);
    tcstrtrim(buf);
    if(strcmp(buf, "ab  cd")) err = true;
    tcstrsqzspc(buf);
    if(strcmp(buf, "ab cd")) err = true;
    tcstrsubchr(buf, "cd", "C");
    if(strcmp(buf, "ab C")) err = true;
    if(tcstrcntutf(buf) != 4) err = true;
    tcstrcututf(buf, 2);
    if(strcmp(buf, "ab")) err = true;
    free(buf);
    if(i % 10 == 1){
      int anum = myrand(30);
      uint16_t ary[anum+1];
      for(int j = 0; j < anum; j++){
        ary[j] = myrand(65535) + 1;
      }
      char ustr[anum*3+1];
      tcstrucstoutf(ary, anum, ustr);
      uint16_t dary[anum+1];
      int danum;
      tcstrutftoucs(ustr, dary, &danum);
      if(danum != anum){
        err = true;
      } else {
        for(int j = 0; j < danum; j++){
          if(dary[j] != dary[j]) err = true;
        }
      }
      list = tcstrsplit(",a,b..c,d,", ",.");
      if(tclistnum(list) != 7) err = true;
      buf = tcstrjoin(list, ':');
      if(strcmp(buf, ":a:b::c:d:")) err = true;
      free(buf);
      tclistdel(list);
      if(!tcregexmatch("ABCDEFGHI", "*(b)c[d-f]*g(h)")) err = true;
      buf = tcregexreplace("ABCDEFGHI", "*(b)c[d-f]*g(h)", "[\\1][\\2][&]");
      if(strcmp(buf, "A[B][H][BCDEFGH]I")) err = true;
      free(buf);
    }
    buf = tcmalloc(48);
    date = myrand(INT_MAX - 1000000);
    jl = 3600 * (myrand(23) - 11);
    tcdatestrwww(date, jl, buf);
    ddate = tcstrmktime(buf);
    if(ddate != date) err = true;
    tcdatestrhttp(date, jl, buf);
    ddate = tcstrmktime(buf);
    if(ddate != date) err = true;
    free(buf);
    if(i % 100){
      map = tcmapnew();
      for(int j = 0; j < 10; j++){
        char kbuf[RECBUFSIZ];
        int ksiz = sprintf(kbuf, "%d", myrand(10));
        tcmapaddint(map, kbuf, ksiz, 1);
        const char *vbuf = tcmapget2(map, kbuf);
        if(*(int *)vbuf < 1) err = true;
      }
      tcmapdel(map);
    }
    buf = tcurlencode(str, slen);
    if(strcmp(buf, "5%252%2B3-1%3D4%20%22Yes%2FNo%22%20%3Ca%26b%3E")) err = true;
    dec = tcurldecode(buf, &dsiz);
    if(dsiz != slen || strcmp(dec, str)) err = true;
    free(dec);
    free(buf);
    if(i % 10 == 1){
      map = tcurlbreak("http://mikio:oikim@estraier.net:1978/foo/bar/baz.cgi?ab=cd&ef=jkl#quux");
      const char *elem;
      if(!(elem = tcmapget2(map, "self")) ||
         strcmp(elem, "http://mikio:oikim@estraier.net:1978/foo/bar/baz.cgi?ab=cd&ef=jkl#quux"))
        err = true;
      if(!(elem = tcmapget2(map, "scheme")) || strcmp(elem, "http")) err = true;
      if(!(elem = tcmapget2(map, "host")) || strcmp(elem, "estraier.net")) err = true;
      if(!(elem = tcmapget2(map, "port")) || strcmp(elem, "1978")) err = true;
      if(!(elem = tcmapget2(map, "authority")) || strcmp(elem, "mikio:oikim")) err = true;
      if(!(elem = tcmapget2(map, "path")) || strcmp(elem, "/foo/bar/baz.cgi")) err = true;
      if(!(elem = tcmapget2(map, "file")) || strcmp(elem, "baz.cgi")) err = true;
      if(!(elem = tcmapget2(map, "query")) || strcmp(elem, "ab=cd&ef=jkl")) err = true;
      if(!(elem = tcmapget2(map, "fragment")) || strcmp(elem, "quux")) err = true;
      tcmapdel(map);
      buf = tcurlresolve("http://a:b@c.d:1/e/f/g.h?i=j#k", "http://A:B@C.D:1/E/F/G.H?I=J#K");
      if(strcmp(buf, "http://A:B@c.d:1/E/F/G.H?I=J#K")) err = true;
      free(buf);
      buf = tcurlresolve("http://a:b@c.d:1/e/f/g.h?i=j#k", "/E/F/G.H?I=J#K");
      if(strcmp(buf, "http://a:b@c.d:1/E/F/G.H?I=J#K")) err = true;
      free(buf);
      buf = tcurlresolve("http://a:b@c.d:1/e/f/g.h?i=j#k", "G.H?I=J#K");
      if(strcmp(buf, "http://a:b@c.d:1/e/f/G.H?I=J#K")) err = true;
      free(buf);
      buf = tcurlresolve("http://a:b@c.d:1/e/f/g.h?i=j#k", "?I=J#K");
      if(strcmp(buf, "http://a:b@c.d:1/e/f/g.h?I=J#K")) err = true;
      free(buf);
      buf = tcurlresolve("http://a:b@c.d:1/e/f/g.h?i=j#k", "#K");
      if(strcmp(buf, "http://a:b@c.d:1/e/f/g.h?i=j#K")) err = true;
      free(buf);
    }
    buf = tcbaseencode(str, slen);
    if(strcmp(buf, "NSUyKzMtMT00ICJZZXMvTm8iIDxhJmI+")) err = true;
    dec = tcbasedecode(buf, &dsiz);
    if(dsiz != slen || strcmp(dec, str)) err = true;
    free(dec);
    free(buf);
    buf = tcquoteencode(str, slen);
    if(strcmp(buf, "5%2+3-1=3D4 \"Yes/No\" <a&b>")) err = true;
    dec = tcquotedecode(buf, &dsiz);
    if(dsiz != slen || strcmp(dec, str)) err = true;
    free(dec);
    free(buf);
    buf = tcmimeencode(str, "UTF-8", true);
    if(strcmp(buf, "=?UTF-8?B?NSUyKzMtMT00ICJZZXMvTm8iIDxhJmI+?=")) err = true;
    char encname[32];
    dec = tcmimedecode(buf, encname);
    if(strcmp(dec, str) || strcmp(encname, "UTF-8")) err = true;
    free(dec);
    free(buf);
    buf = tcpackencode(str, slen, &bsiz);
    dec = tcpackdecode(buf, bsiz, &dsiz);
    if(dsiz != slen || strcmp(dec, str)) err = true;
    free(dec);
    free(buf);
    buf = tcbsencode(str, slen, &bsiz);
    dec = tcbsdecode(buf, bsiz, &dsiz);
    if(dsiz != slen || strcmp(dec, str)) err = true;
    free(dec);
    free(buf);
    int idx;
    buf = tcbwtencode(str, slen, &idx);
    if(memcmp(buf, "4\"o 5a23s-%+=> 1b/\"<&YNe", slen) || idx != 13) err = true;
    dec = tcbwtdecode(buf, slen, idx);
    if(memcmp(dec, str, slen)) err = true;
    free(dec);
    free(buf);
    if(_tc_deflate){
      if((buf = tcdeflate(str, slen, &bsiz)) != NULL){
        if((dec = tcinflate(buf, bsiz, &dsiz)) != NULL){
          if(slen != dsiz || memcmp(str, dec, dsiz)) err = true;
          free(dec);
        } else {
          err = true;
        }
        free(buf);
      } else {
        err = true;
      }
      if((buf = tcgzipencode(str, slen, &bsiz)) != NULL){
        if((dec = tcgzipdecode(buf, bsiz, &dsiz)) != NULL){
          if(slen != dsiz || memcmp(str, dec, dsiz)) err = true;
          free(dec);
        } else {
          err = true;
        }
        free(buf);
      } else {
        err = true;
      }
      if(tcgetcrc("hoge", 4) % 10000 != 7034) err = true;
    }
    int anum = myrand(50)+1;
    unsigned int ary[anum];
    for(int j = 0; j < anum; j++){
      ary[j] = myrand(INT_MAX);
    }
    buf = tcberencode(ary, anum, &bsiz);
    int dnum;
    unsigned int *dary = tcberdecode(buf, bsiz, &dnum);
    if(anum != dnum || memcmp(ary, dary, sizeof(*dary) * dnum)) err = true;
    free(dary);
    free(buf);
    buf = tcxmlescape(str);
    if(strcmp(buf, "5%2+3-1=4 &quot;Yes/No&quot; &lt;a&amp;b&gt;")) err = true;
    dec = tcxmlunescape(buf);
    if(strcmp(dec, str)) err = true;
    free(dec);
    free(buf);
    if(i % 10 == 1){
      list = tcxmlbreak("<abc de=\"foo&amp;\" gh='&lt;bar&gt;'>xyz<br>\na<!--<mikio>--></abc>");
      for(int j = 0; j < tclistnum(list); j++){
        const char *elem = tclistval2(list, j);
        TCMAP *attrs = tcxmlattrs(elem);
        tcmapdel(attrs);
      }
      tclistdel(list);
    }
    if(i % 10 == 1){
      for(int16_t j = 1; j <= 0x2000; j *= 2){
        for(int16_t num = j - 1; num <= j + 1; num++){
          int16_t nnum = TCHTOIS(num);
          if(num != TCITOHS(nnum)) err = true;
        }
      }
      for(int32_t j = 1; j <= 0x20000000; j *= 2){
        for(int32_t num = j - 1; num <= j + 1; num++){
          int32_t nnum = TCHTOIL(num);
          if(num != TCITOHL(nnum)) err = true;
          char buf[TCNUMBUFSIZ];
          int step, nstep;
          TCSETVNUMBUF(step, buf, num);
          TCREADVNUMBUF(buf, nnum, nstep);
          if(num != nnum || step != nstep) err = true;
        }
      }
      for(int64_t j = 1; j <= 0x2000000000000000; j *= 2){
        for(int64_t num = j - 1; num <= j + 1; num++){
          int64_t nnum = TCHTOILL(num);
          if(num != TCITOHLL(nnum)) err = true;
          char buf[TCNUMBUFSIZ];
          int step, nstep;
          TCSETVNUMBUF64(step, buf, num);
          TCREADVNUMBUF64(buf, nnum, nstep);
          if(num != nnum || step != nstep) err = true;
        }
      }
      char *bitmap = TCBITMAPNEW(100);
      for(int j = 0; j < 100; j++){
        if(j % 3 == 0) TCBITMAPON(bitmap, j);
        if(j % 5 == 0) TCBITMAPOFF(bitmap, j);
      }
      for(int j = 0; j < 100; j++){
        if(j % 5 == 0){
          if(TCBITMAPCHECK(bitmap, j)) err = true;
        } else if(j % 3 == 0){
          if(!TCBITMAPCHECK(bitmap, j)) err = true;
        }
      }
      TCBITMAPDEL(bitmap);
      buf = tcmalloc(i / 8 + 2);
      TCBITSTRM strm;
      TCBITSTRMINITW(strm, buf);
      for(int j = 0; j < i; j++){
        int sign = j % 3 == 0 || j % 7 == 0;
        TCBITSTRMCAT(strm, sign);
      }
      TCBITSTRMSETEND(strm);
      int bnum = TCBITSTRMNUM(strm);
      if(bnum != i) err = true;
      TCBITSTRMINITR(strm, buf, bsiz);
      for(int j = 0; j < i; j++){
        int sign;
        TCBITSTRMREAD(strm, sign);
        if(sign != (j % 3 == 0 || j % 7 == 0)) err = true;
      }
      free(buf);
    }
    if(rnum > 250 && i % (rnum / 250) == 0){
      putchar('.');
      fflush(stdout);
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
    }
  }
  iprintf("time: %.3f\n", tctime() - stime);
  if(err){
    iprintf("error\n\n");
    return 1;
  }
  iprintf("ok\n\n");
  return 0;
}


/* perform wicked command */
static int procwicked(int rnum){
  iprintf("<Wicked Writing Test>\n  rnum=%d\n\n", rnum);
  double stime = tctime();
  TCMPOOL *mpool = tcmpoolglobal();
  TCXSTR *xstr = myrand(2) > 0 ? tcxstrnew() : tcxstrnew2("hello world");
  tcmpoolputxstr(mpool, xstr);
  TCLIST *list = myrand(2) > 0 ? tclistnew() : tclistnew2(myrand(rnum) + rnum / 2);
  tcmpoolputlist(mpool, list);
  TCMAP *map = myrand(2) > 0 ? tcmapnew() : tcmapnew2(myrand(rnum) + rnum / 2);
  tcmpoolputmap(mpool, map);
  TCMDB *mdb = myrand(2) > 0 ? tcmdbnew() : tcmdbnew2(myrand(rnum) + rnum / 2);
  tcmpoolput(mpool, mdb, (void (*)(void*))tcmdbdel);
  for(int i = 1; i <= rnum; i++){
    char kbuf[RECBUFSIZ];
    int ksiz = sprintf(kbuf, "%d", myrand(i));
    char vbuf[RECBUFSIZ];
    int vsiz = sprintf(vbuf, "%d", myrand(i));
    char *tmp;
    switch(myrand(69)){
    case 0:
      putchar('0');
      tcxstrcat(xstr, kbuf, ksiz);
      break;
    case 1:
      putchar('1');
      tcxstrcat2(xstr, kbuf);
      break;
    case 2:
      putchar('2');
      if(myrand(rnum / 100 + 1) == 0) tcxstrclear(xstr);
      break;
    case 3:
      putchar('3');
      tcxstrprintf(xstr, "[%s:%d]", kbuf, i);
      break;
    case 4:
      putchar('4');
      tclistpush(list, kbuf, ksiz);
      break;
    case 5:
      putchar('5');
      tclistpush2(list, kbuf);
      break;
    case 6:
      putchar('6');
      tmp = tcmemdup(kbuf, ksiz);
      tclistpushmalloc(list, tmp, strlen(tmp));
      break;
    case 7:
      putchar('7');
      if(myrand(10) == 0) free(tclistpop(list, &ksiz));
      break;
    case 8:
      putchar('8');
      if(myrand(10) == 0) free(tclistpop2(list));
      break;
    case 9:
      putchar('9');
      tclistunshift(list, kbuf, ksiz);
      break;
    case 10:
      putchar('A');
      tclistunshift2(list, kbuf);
      break;
    case 11:
      putchar('B');
      if(myrand(10) == 0) free(tclistshift(list, &ksiz));
      break;
    case 12:
      putchar('C');
      if(myrand(10) == 0) free(tclistshift2(list));
      break;
    case 13:
      putchar('D');
      tclistinsert(list, i / 10, kbuf, ksiz);
      break;
    case 14:
      putchar('E');
      tclistinsert2(list, i / 10, kbuf);
      break;
    case 15:
      putchar('F');
      if(myrand(10) == 0) free(tclistremove(list, i / 10, &ksiz));
      break;
    case 16:
      putchar('G');
      if(myrand(10) == 0) free(tclistremove2(list, i / 10));
      break;
    case 17:
      putchar('H');
      tclistover(list, i / 10, kbuf, ksiz);
      break;
    case 18:
      putchar('I');
      tclistover2(list, i / 10, kbuf);
      break;
    case 19:
      putchar('J');
      if(myrand(rnum / 1000 + 1) == 0) tclistsort(list);
      break;
    case 20:
      putchar('K');
      if(myrand(rnum / 1000 + 1) == 0) tclistsortci(list);
      break;
    case 21:
      putchar('L');
      if(myrand(rnum / 1000 + 1) == 0) tclistlsearch(list, kbuf, ksiz);
      break;
    case 22:
      putchar('M');
      if(myrand(rnum / 1000 + 1) == 0) tclistbsearch(list, kbuf, ksiz);
      break;
    case 23:
      putchar('N');
      if(myrand(rnum / 100 + 1) == 0) tclistclear(list);
      break;
    case 24:
      putchar('O');
      if(myrand(rnum / 100 + 1) == 0){
        int dsiz;
        char *dbuf = tclistdump(list, &dsiz);
        tclistdel(tclistload(dbuf, dsiz));
        free(dbuf);
      }
      break;
    case 25:
      putchar('P');
      if(myrand(100) == 0){
        if(myrand(2) == 0){
          for(int j = 0; j < tclistnum(list); j++){
            int rsiz;
            tclistval(list, j, &rsiz);
          }
        } else {
          for(int j = 0; j < tclistnum(list); j++){
            tclistval2(list, j);
          }
        }
      }
      break;
    case 26:
      putchar('Q');
      tcmapput(map, kbuf, ksiz, vbuf, vsiz);
      break;
    case 27:
      putchar('R');
      tcmapput2(map, kbuf, vbuf);
      break;
    case 28:
      putchar('S');
      tcmapput3(map, kbuf, ksiz, vbuf, vsiz, vbuf, vsiz);
      break;
    case 29:
      putchar('T');
      tcmapputkeep(map, kbuf, ksiz, vbuf, vsiz);
      break;
    case 30:
      putchar('U');
      tcmapputkeep2(map, kbuf, vbuf);
      break;
    case 31:
      putchar('V');
      tcmapputcat(map, kbuf, ksiz, vbuf, vsiz);
      break;
    case 32:
      putchar('W');
      tcmapputcat2(map, kbuf, vbuf);
      break;
    case 33:
      putchar('X');
      if(myrand(10) == 0) tcmapout(map, kbuf, ksiz);
      break;
    case 34:
      putchar('Y');
      if(myrand(10) == 0) tcmapout2(map, kbuf);
      break;
    case 35:
      putchar('Z');
      tcmapget3(map, kbuf, ksiz, &vsiz);
      break;
    case 36:
      putchar('a');
      tcmapmove(map, kbuf, ksiz, true);
      break;
    case 37:
      putchar('b');
      tcmapmove(map, kbuf, ksiz, false);
      break;
    case 38:
      putchar('c');
      tcmapmove2(map, kbuf, true);
      break;
    case 39:
      putchar('d');
      if(myrand(100) == 0) tcmapiterinit(map);
      break;
    case 40:
      putchar('e');
      tcmapiternext(map, &vsiz);
      break;
    case 41:
      putchar('f');
      tcmapiternext2(map);
      break;
    case 42:
      putchar('g');
      if(myrand(100) == 0){
        if(myrand(2) == 0){
          tclistdel(tcmapkeys(map));
        } else {
          tclistdel(tcmapvals(map));
        }
      }
      break;
    case 43:
      putchar('h');
      if(myrand(rnum / 100 + 1) == 0) tcmapclear(map);
      break;
    case 44:
      putchar('i');
      if(myrand(20) == 0) tcmapcutfront(map, myrand(10));
      break;
    case 45:
      putchar('j');
      if(myrand(rnum / 100 + 1) == 0){
        int dsiz;
        char *dbuf = tcmapdump(map, &dsiz);
        free(tcmaploadone(dbuf, dsiz, kbuf, ksiz, &vsiz));
        tcmapdel(tcmapload(dbuf, dsiz));
        free(dbuf);
      }
      break;
    case 46:
      putchar('k');
      tcmdbput(mdb, kbuf, ksiz, vbuf, vsiz);
      break;
    case 47:
      putchar('l');
      tcmdbput2(mdb, kbuf, vbuf);
      break;
    case 48:
      putchar('m');
      tcmdbputkeep(mdb, kbuf, ksiz, vbuf, vsiz);
      break;
    case 49:
      putchar('n');
      tcmdbputkeep2(mdb, kbuf, vbuf);
      break;
    case 50:
      putchar('o');
      tcmdbputcat(mdb, kbuf, ksiz, vbuf, vsiz);
      break;
    case 51:
      putchar('p');
      tcmdbputcat2(mdb, kbuf, vbuf);
      break;
    case 52:
      putchar('q');
      if(myrand(10) == 0) tcmdbout(mdb, kbuf, ksiz);
      break;
    case 53:
      putchar('r');
      if(myrand(10) == 0) tcmdbout2(mdb, kbuf);
      break;
    case 54:
      putchar('s');
      free(tcmdbget(mdb, kbuf, ksiz, &vsiz));
      break;
    case 55:
      putchar('t');
      free(tcmdbget3(mdb, kbuf, ksiz, &vsiz));
      break;
    case 56:
      putchar('u');
      if(myrand(100) == 0) tcmdbiterinit(mdb);
      break;
    case 57:
      putchar('v');
      free(tcmdbiternext(mdb, &vsiz));
      break;
    case 58:
      putchar('w');
      tmp = tcmdbiternext2(mdb);
      free(tmp);
      break;
    case 59:
      putchar('x');
      if(myrand(rnum / 100 + 1) == 0) tcmdbvanish(mdb);
      break;
    case 60:
      putchar('y');
      if(myrand(200) == 0) tcmdbcutfront(mdb, myrand(100));
      break;
    case 61:
      putchar('+');
      if(myrand(100) == 0) tcmpoolmalloc(mpool, 1);
      break;
    case 62:
      putchar('+');
      if(myrand(100) == 0) tcmpoolxstrnew(mpool);
      break;
    case 63:
      putchar('+');
      if(myrand(100) == 0) tcmpoollistnew(mpool);
      break;
    case 64:
      putchar('+');
      if(myrand(100) == 0) tcmpoolmapnew(mpool);
      break;
    default:
      putchar('@');
      if(myrand(10000) == 0) srand((unsigned int)(tctime() * 1000) % UINT_MAX);
      break;
    }
    if(i % 50 == 0) iprintf(" (%08d)\n", i);
  }
  if(rnum % 50 > 0) iprintf(" (%08d)\n", rnum);
  iprintf("time: %.3f\n", tctime() - stime);
  iprintf("ok\n\n");
  return 0;
}



// END OF FILE
