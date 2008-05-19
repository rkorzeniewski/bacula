/*************************************************************************************************
 * The test cases of the abstract database API
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
#include <tcadb.h>
#include "myconf.h"

#define RECBUFSIZ      32                // buffer for records


/* global variables */
const char *g_progname;                  // program name


/* function prototypes */
int main(int argc, char **argv);
static void usage(void);
static void iprintf(const char *format, ...);
static void eprint(TCADB *adb, const char *func);
static int myrand(int range);
static int runwrite(int argc, char **argv);
static int runread(int argc, char **argv);
static int runremove(int argc, char **argv);
static int runrcat(int argc, char **argv);
static int runmisc(int argc, char **argv);
static int runwicked(int argc, char **argv);
static int procwrite(const char *name, int rnum);
static int procread(const char *name);
static int procremove(const char *name);
static int procrcat(const char *name, int rnum);
static int procmisc(const char *name, int rnum);
static int procwicked(const char *name, int rnum);


/* main routine */
int main(int argc, char **argv){
  g_progname = argv[0];
  srand((unsigned int)(tctime() * 1000) % UINT_MAX);
  if(argc < 2) usage();
  int rv = 0;
  if(!strcmp(argv[1], "write")){
    rv = runwrite(argc, argv);
  } else if(!strcmp(argv[1], "read")){
    rv = runread(argc, argv);
  } else if(!strcmp(argv[1], "remove")){
    rv = runremove(argc, argv);
  } else if(!strcmp(argv[1], "rcat")){
    rv = runrcat(argc, argv);
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
  fprintf(stderr, "%s: test cases of the abstract database API of Tokyo Cabinet\n", g_progname);
  fprintf(stderr, "\n");
  fprintf(stderr, "usage:\n");
  fprintf(stderr, "  %s write name rnum\n", g_progname);
  fprintf(stderr, "  %s read name\n", g_progname);
  fprintf(stderr, "  %s remove name\n", g_progname);
  fprintf(stderr, "  %s rcat name rnum\n", g_progname);
  fprintf(stderr, "  %s misc name rnum\n", g_progname);
  fprintf(stderr, "  %s wicked name rnum\n", g_progname);
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


/* print error message of abstract database */
static void eprint(TCADB *adb, const char *func){
  fprintf(stderr, "%s: %s: error\n", g_progname, func);
}


/* get a random number */
static int myrand(int range){
  return (int)((double)range * rand() / (RAND_MAX + 1.0));
}


/* parse arguments of write command */
static int runwrite(int argc, char **argv){
  char *name = NULL;
  char *rstr = NULL;
  for(int i = 2; i < argc; i++){
    if(!name && argv[i][0] == '-'){
      usage();
    } else if(!name){
      name = argv[i];
    } else if(!rstr){
      rstr = argv[i];
    } else {
      usage();
    }
  }
  if(!name || !rstr) usage();
  int rnum = atoi(rstr);
  if(rnum < 1) usage();
  int rv = procwrite(name, rnum);
  return rv;
}


/* parse arguments of read command */
static int runread(int argc, char **argv){
  char *name = NULL;
  for(int i = 2; i < argc; i++){
    if(!name && argv[i][0] == '-'){
      usage();
    } else if(!name){
      name = argv[i];
    } else {
      usage();
    }
  }
  if(!name) usage();
  int rv = procread(name);
  return rv;
}


/* parse arguments of remove command */
static int runremove(int argc, char **argv){
  char *name = NULL;
  for(int i = 2; i < argc; i++){
    if(!name && argv[i][0] == '-'){
      usage();
    } else if(!name){
      name = argv[i];
    } else {
      usage();
    }
  }
  if(!name) usage();
  int rv = procremove(name);
  return rv;
}


/* parse arguments of rcat command */
static int runrcat(int argc, char **argv){
  char *name = NULL;
  char *rstr = NULL;
  for(int i = 2; i < argc; i++){
    if(!name && argv[i][0] == '-'){
      usage();
    } else if(!name){
      name = argv[i];
    } else if(!rstr){
      rstr = argv[i];
    } else {
      usage();
    }
  }
  if(!name || !rstr) usage();
  int rnum = atoi(rstr);
  if(rnum < 1) usage();
  int rv = procrcat(name, rnum);
  return rv;
}


/* parse arguments of misc command */
static int runmisc(int argc, char **argv){
  char *name = NULL;
  char *rstr = NULL;
  for(int i = 2; i < argc; i++){
    if(!name && argv[i][0] == '-'){
      usage();
    } else if(!name){
      name = argv[i];
    } else if(!rstr){
      rstr = argv[i];
    } else {
      usage();
    }
  }
  if(!name || !rstr) usage();
  int rnum = atoi(rstr);
  if(rnum < 1) usage();
  int rv = procmisc(name, rnum);
  return rv;
}


/* parse arguments of wicked command */
static int runwicked(int argc, char **argv){
  char *name = NULL;
  char *rstr = NULL;
  for(int i = 2; i < argc; i++){
    if(!name && argv[i][0] == '-'){
      usage();
    } else if(!name){
      name = argv[i];
    } else if(!rstr){
      rstr = argv[i];
    } else {
      usage();
    }
  }
  if(!name || !rstr) usage();
  int rnum = atoi(rstr);
  if(rnum < 1) usage();
  int rv = procwicked(name, rnum);
  return rv;
}


/* perform write command */
static int procwrite(const char *name, int rnum){
  iprintf("<Writing Test>\n  name=%s  rnum=%d\n\n", name, rnum);
  bool err = false;
  double stime = tctime();
  TCADB *adb = tcadbnew();
  if(!tcadbopen(adb, name)){
    eprint(adb, "tcadbopen");
    err = true;
  }
  for(int i = 1; i <= rnum; i++){
    char buf[RECBUFSIZ];
    int len = sprintf(buf, "%08d", i);
    if(!tcadbput(adb, buf, len, buf, len)){
      eprint(adb, "tcadbput");
      err = true;
      break;
    }
    if(rnum > 250 && i % (rnum / 250) == 0){
      putchar('.');
      fflush(stdout);
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
    }
  }
  iprintf("record number: %llu\n", (unsigned long long)tcadbrnum(adb));
  iprintf("size: %llu\n", (unsigned long long)tcadbsize(adb));
  if(!tcadbclose(adb)){
    eprint(adb, "tcadbclose");
    err = true;
  }
  tcadbdel(adb);
  iprintf("time: %.3f\n", tctime() - stime);
  iprintf("%s\n\n", err ? "error" : "ok");
  return err ? 1 : 0;
}


/* perform read command */
static int procread(const char *name){
  iprintf("<Reading Test>\n  name=%s\n\n", name);
  bool err = false;
  double stime = tctime();
  TCADB *adb = tcadbnew();
  if(!tcadbopen(adb, name)){
    eprint(adb, "tcadbopen");
    err = true;
  }
  int rnum = tcadbrnum(adb);
  for(int i = 1; i <= rnum; i++){
    char kbuf[RECBUFSIZ];
    int ksiz = sprintf(kbuf, "%08d", i);
    int vsiz;
    char *vbuf = tcadbget(adb, kbuf, ksiz, &vsiz);
    if(!vbuf){
      eprint(adb, "tcadbget");
      err = true;
      break;
    }
    free(vbuf);
    if(rnum > 250 && i % (rnum / 250) == 0){
      putchar('.');
      fflush(stdout);
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
    }
  }
  iprintf("record number: %llu\n", (unsigned long long)tcadbrnum(adb));
  iprintf("size: %llu\n", (unsigned long long)tcadbsize(adb));
  if(!tcadbclose(adb)){
    eprint(adb, "tcadbclose");
    err = true;
  }
  tcadbdel(adb);
  iprintf("time: %.3f\n", tctime() - stime);
  iprintf("%s\n\n", err ? "error" : "ok");
  return err ? 1 : 0;
}


/* perform remove command */
static int procremove(const char *name){
  iprintf("<Removing Test>\n  name=%s\n\n", name);
  bool err = false;
  double stime = tctime();
  TCADB *adb = tcadbnew();
  if(!tcadbopen(adb, name)){
    eprint(adb, "tcadbopen");
    err = true;
  }
  int rnum = tcadbrnum(adb);
  for(int i = 1; i <= rnum; i++){
    char kbuf[RECBUFSIZ];
    int ksiz = sprintf(kbuf, "%08d", i);
    if(!tcadbout(adb, kbuf, ksiz)){
      eprint(adb, "tcadbout");
      err = true;
      break;
    }
    if(rnum > 250 && i % (rnum / 250) == 0){
      putchar('.');
      fflush(stdout);
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
    }
  }
  iprintf("record number: %llu\n", (unsigned long long)tcadbrnum(adb));
  iprintf("size: %llu\n", (unsigned long long)tcadbsize(adb));
  if(!tcadbclose(adb)){
    eprint(adb, "tcadbclose");
    err = true;
  }
  tcadbdel(adb);
  iprintf("time: %.3f\n", tctime() - stime);
  iprintf("%s\n\n", err ? "error" : "ok");
  return err ? 1 : 0;
}


/* perform rcat command */
static int procrcat(const char *name, int rnum){
  iprintf("<Random Concatenating Test>\n  name=%s  rnum=%d\n\n", name, rnum);
  int pnum = rnum / 5 + 1;
  bool err = false;
  double stime = tctime();
  TCADB *adb = tcadbnew();
  if(!tcadbopen(adb, name)){
    eprint(adb, "tcadbopen");
    err = true;
  }
  for(int i = 1; i <= rnum; i++){
    char kbuf[RECBUFSIZ];
    int ksiz = sprintf(kbuf, "%d", myrand(pnum));
    if(!tcadbputcat(adb, kbuf, ksiz, kbuf, ksiz)){
      eprint(adb, "tcadbputcat");
      err = true;
      break;
    }
    if(rnum > 250 && i % (rnum / 250) == 0){
      putchar('.');
      fflush(stdout);
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
    }
  }
  iprintf("record number: %llu\n", (unsigned long long)tcadbrnum(adb));
  iprintf("size: %llu\n", (unsigned long long)tcadbsize(adb));
  if(!tcadbclose(adb)){
    eprint(adb, "tcadbclose");
    err = true;
  }
  tcadbdel(adb);
  iprintf("time: %.3f\n", tctime() - stime);
  iprintf("%s\n\n", err ? "error" : "ok");
  return err ? 1 : 0;
}


/* perform misc command */
static int procmisc(const char *name, int rnum){
  iprintf("<Random Concatenating Test>\n  name=%s  rnum=%d\n\n", name, rnum);
  bool err = false;
  double stime = tctime();
  TCADB *adb = tcadbnew();
  if(!tcadbopen(adb, name)){
    eprint(adb, "tcadbopen");
    err = true;
  }
  iprintf("writing:\n");
  for(int i = 1; i <= rnum; i++){
    char buf[RECBUFSIZ];
    int len = sprintf(buf, "%08d", i);
    if(!tcadbputkeep(adb, buf, len, buf, len)){
      eprint(adb, "tcadbputkeep");
      err = true;
      break;
    }
    if(rnum > 250 && i % (rnum / 250) == 0){
      putchar('.');
      fflush(stdout);
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
    }
  }
  iprintf("reading:\n");
  for(int i = 1; i <= rnum; i++){
    char kbuf[RECBUFSIZ];
    int ksiz = sprintf(kbuf, "%08d", i);
    int vsiz;
    char *vbuf = tcadbget(adb, kbuf, ksiz, &vsiz);
    if(!vbuf){
      eprint(adb, "tcadbget");
      err = true;
      break;
    } else if(vsiz != ksiz || memcmp(vbuf, kbuf, vsiz)){
      eprint(adb, "(validation)");
      err = true;
      free(vbuf);
      break;
    }
    free(vbuf);
    if(rnum > 250 && i % (rnum / 250) == 0){
      putchar('.');
      fflush(stdout);
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
    }
  }
  if(tcadbrnum(adb) != rnum){
    eprint(adb, "(validation)");
    err = true;
  }
  iprintf("random writing:\n");
  for(int i = 1; i <= rnum; i++){
    char kbuf[RECBUFSIZ];
    int ksiz = sprintf(kbuf, "%d", myrand(rnum));
    char vbuf[RECBUFSIZ];
    int vsiz = myrand(RECBUFSIZ);
    memset(vbuf, '*', vsiz);
    if(!tcadbput(adb, kbuf, ksiz, vbuf, vsiz)){
      eprint(adb, "tcadbput");
      err = true;
      break;
    }
    int rsiz;
    char *rbuf = tcadbget(adb, kbuf, ksiz, &rsiz);
    if(!rbuf){
      eprint(adb, "tcadbget");
      err = true;
      break;
    }
    if(rsiz != vsiz || memcmp(rbuf, vbuf, rsiz)){
      eprint(adb, "(validation)");
      err = true;
      free(rbuf);
      break;
    }
    if(rnum > 250 && i % (rnum / 250) == 0){
      putchar('.');
      fflush(stdout);
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
    }
    free(rbuf);
  }
  iprintf("word writing:\n");
  const char *words[] = {
    "a", "A", "bb", "BB", "ccc", "CCC", "dddd", "DDDD", "eeeee", "EEEEEE",
    "mikio", "hirabayashi", "tokyo", "cabinet", "hyper", "estraier", "19780211", "birth day",
    "one", "first", "two", "second", "three", "third", "four", "fourth", "five", "fifth",
    "_[1]_", "uno", "_[2]_", "dos", "_[3]_", "tres", "_[4]_", "cuatro", "_[5]_", "cinco",
    "[\xe5\xb9\xb3\xe6\x9e\x97\xe5\xb9\xb9\xe9\x9b\x84]", "[\xe9\xa6\xac\xe9\xb9\xbf]", NULL
  };
  for(int i = 0; words[i] != NULL; i += 2){
    const char *kbuf = words[i];
    int ksiz = strlen(kbuf);
    const char *vbuf = words[i+1];
    int vsiz = strlen(vbuf);
    if(!tcadbputkeep(adb, kbuf, ksiz, vbuf, vsiz)){
      eprint(adb, "tcadbputkeep");
      err = true;
      break;
    }
    if(rnum > 250) putchar('.');
  }
  if(rnum > 250) iprintf(" (%08d)\n", sizeof(words) / sizeof(*words));
  iprintf("random erasing:\n");
  for(int i = 1; i <= rnum; i++){
    char kbuf[RECBUFSIZ];
    int ksiz = sprintf(kbuf, "%d", myrand(rnum));
    tcadbout(adb, kbuf, ksiz);
    if(rnum > 250 && i % (rnum / 250) == 0){
      putchar('.');
      fflush(stdout);
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
    }
  }
  iprintf("writing:\n");
  for(int i = 1; i <= rnum; i++){
    char kbuf[RECBUFSIZ];
    int ksiz = sprintf(kbuf, "[%d]", i);
    char vbuf[RECBUFSIZ];
    int vsiz = i % RECBUFSIZ;
    memset(vbuf, '*', vsiz);
    if(!tcadbputkeep(adb, kbuf, ksiz, vbuf, vsiz)){
      eprint(adb, "tcadbputkeep");
      err = true;
      break;
    }
    if(vsiz < 1){
      char tbuf[PATH_MAX];
      for(int j = 0; j < PATH_MAX; j++){
        tbuf[j] = myrand(0x100);
      }
      if(!tcadbput(adb, kbuf, ksiz, tbuf, PATH_MAX)){
        eprint(adb, "tcadbput");
        err = true;
        break;
      }
    }
    if(rnum > 250 && i % (rnum / 250) == 0){
      putchar('.');
      fflush(stdout);
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
    }
  }
  iprintf("erasing:\n");
  for(int i = 1; i <= rnum; i++){
    if(i % 2 == 1){
      char kbuf[RECBUFSIZ];
      int ksiz = sprintf(kbuf, "[%d]", i);
      if(!tcadbout(adb, kbuf, ksiz)){
        eprint(adb, "tcadbout");
        err = true;
        break;
      }
      tcadbout(adb, kbuf, ksiz);
    }
    if(rnum > 250 && i % (rnum / 250) == 0){
      putchar('.');
      fflush(stdout);
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
    }
  }
  iprintf("iterator checking:\n");
  if(!tcadbiterinit(adb)){
    eprint(adb, "tcadbiterinit");
    err = true;
  }
  char *kbuf;
  int ksiz;
  int inum = 0;
  for(int i = 1; (kbuf = tcadbiternext(adb, &ksiz)) != NULL; i++, inum++){
    int vsiz;
    char *vbuf = tcadbget(adb, kbuf, ksiz, &vsiz);
    if(!vbuf){
      eprint(adb, "tcadbget");
      err = true;
      free(kbuf);
      break;
    }
    free(vbuf);
    free(kbuf);
    if(rnum > 250 && i % (rnum / 250) == 0){
      putchar('.');
      fflush(stdout);
      if(i == rnum || i % (rnum / 10) == 0) iprintf(" (%08d)\n", i);
    }
  }
  if(rnum > 250) iprintf(" (%08d)\n", inum);
  if(inum != tcadbrnum(adb)){
    eprint(adb, "(validation)");
    err = true;
  }
  if(*name != '*' && !tcadbsync(adb)){
    eprint(adb, "tcadbsync");
    err = true;
  }
  if(!tcadbvanish(adb)){
    eprint(adb, "tcadbsync");
    err = true;
  }
  iprintf("record number: %llu\n", (unsigned long long)tcadbrnum(adb));
  iprintf("size: %llu\n", (unsigned long long)tcadbsize(adb));
  if(!tcadbclose(adb)){
    eprint(adb, "tcadbclose");
    err = true;
  }
  tcadbdel(adb);
  iprintf("time: %.3f\n", tctime() - stime);
  iprintf("%s\n\n", err ? "error" : "ok");
  return err ? 1 : 0;
}


/* perform wicked command */
static int procwicked(const char *name, int rnum){
  iprintf("<Wicked Writing Test>\n  name=%s  rnum=%d\n\n", name, rnum);
  bool err = false;
  double stime = tctime();
  TCADB *adb = tcadbnew();
  if(!tcadbopen(adb, name)){
    eprint(adb, "tcadbopen");
    err = true;
  }
  TCMAP *map = tcmapnew2(rnum / 5);
  for(int i = 1; i <= rnum && !err; i++){
    char kbuf[RECBUFSIZ];
    int ksiz = sprintf(kbuf, "%d", myrand(rnum));
    char vbuf[RECBUFSIZ];
    int vsiz = myrand(RECBUFSIZ);
    memset(vbuf, '*', vsiz);
    vbuf[vsiz] = '\0';
    char *rbuf;
    switch(myrand(16)){
    case 0:
      putchar('0');
      if(!tcadbput(adb, kbuf, ksiz, vbuf, vsiz)){
        eprint(adb, "tcadbput");
        err = true;
      }
      tcmapput(map, kbuf, ksiz, vbuf, vsiz);
      break;
    case 1:
      putchar('1');
      if(!tcadbput2(adb, kbuf, vbuf)){
        eprint(adb, "tcadbput2");
        err = true;
      }
      tcmapput2(map, kbuf, vbuf);
      break;
    case 2:
      putchar('2');
      tcadbputkeep(adb, kbuf, ksiz, vbuf, vsiz);
      tcmapputkeep(map, kbuf, ksiz, vbuf, vsiz);
      break;
    case 3:
      putchar('3');
      tcadbputkeep2(adb, kbuf, vbuf);
      tcmapputkeep2(map, kbuf, vbuf);
      break;
    case 4:
      putchar('4');
      if(!tcadbputcat(adb, kbuf, ksiz, vbuf, vsiz)){
        eprint(adb, "tcadbputcat");
        err = true;
      }
      tcmapputcat(map, kbuf, ksiz, vbuf, vsiz);
      break;
    case 5:
      putchar('5');
      if(!tcadbputcat2(adb, kbuf, vbuf)){
        eprint(adb, "tcadbputcat2");
        err = true;
      }
      tcmapputcat2(map, kbuf, vbuf);
      break;
    case 6:
      putchar('6');
      if(myrand(10) == 0){
        tcadbout(adb, kbuf, ksiz);
        tcmapout(map, kbuf, ksiz);
      }
      break;
    case 7:
      putchar('7');
      if(myrand(10) == 0){
        tcadbout2(adb, kbuf);
        tcmapout2(map, kbuf);
      }
      break;
    case 8:
      putchar('8');
      if((rbuf = tcadbget(adb, kbuf, ksiz, &vsiz)) != NULL) free(rbuf);
      break;
    case 9:
      putchar('9');
      if((rbuf = tcadbget2(adb, kbuf)) != NULL) free(rbuf);
      break;
    case 10:
      putchar('A');
      tcadbvsiz(adb, kbuf, ksiz);
      break;
    case 11:
      putchar('B');
      tcadbvsiz2(adb, kbuf);
      break;
    case 12:
      putchar('E');
      if(myrand(rnum / 50) == 0){
        if(!tcadbiterinit(adb)){
          eprint(adb, "tcadbiterinit");
          err = true;
        }
      }
      for(int j = myrand(rnum) / 1000 + 1; j >= 0; j--){
        int iksiz;
        char *ikbuf = tcadbiternext(adb, &iksiz);
        if(ikbuf) free(ikbuf);
      }
      break;
    default:
      putchar('@');
      if(myrand(10000) == 0) srand((unsigned int)(tctime() * 1000) % UINT_MAX);
      break;
    }
    if(i % 50 == 0) iprintf(" (%08d)\n", i);
  }
  if(rnum % 50 > 0) iprintf(" (%08d)\n", rnum);
  tcadbsync(adb);
  if(tcadbrnum(adb) != tcmaprnum(map)){
    eprint(adb, "(validation)");
    err = true;
  }
  for(int i = 1; i <= rnum && !err; i++){
    char kbuf[RECBUFSIZ];
    int ksiz = sprintf(kbuf, "%d", i - 1);
    int vsiz;
    const char *vbuf = tcmapget(map, kbuf, ksiz, &vsiz);
    int rsiz;
    char *rbuf = tcadbget(adb, kbuf, ksiz, &rsiz);
    if(vbuf){
      putchar('.');
      if(!rbuf){
        eprint(adb, "tcadbget");
        err = true;
      } else if(rsiz != vsiz || memcmp(rbuf, vbuf, rsiz)){
        eprint(adb, "(validation)");
        err = true;
      }
    } else {
      putchar('*');
      if(rbuf){
        eprint(adb, "(validation)");
        err = true;
      }
    }
    free(rbuf);
    if(i % 50 == 0) iprintf(" (%08d)\n", i);
  }
  if(rnum % 50 > 0) iprintf(" (%08d)\n", rnum);
  tcmapiterinit(map);
  int ksiz;
  const char *kbuf;
  for(int i = 1; (kbuf = tcmapiternext(map, &ksiz)) != NULL; i++){
    putchar('+');
    int vsiz;
    const char *vbuf = tcmapiterval(kbuf, &vsiz);
    int rsiz;
    char *rbuf = tcadbget(adb, kbuf, ksiz, &rsiz);
    if(!rbuf){
      eprint(adb, "tcadbget");
      err = true;
    } else if(rsiz != vsiz || memcmp(rbuf, vbuf, rsiz)){
      eprint(adb, "(validation)");
      err = true;
    }
    free(rbuf);
    if(!tcadbout(adb, kbuf, ksiz)){
      eprint(adb, "tcadbout");
      err = true;
    }
    if(i % 50 == 0) iprintf(" (%08d)\n", i);
  }
  int mrnum = tcmaprnum(map);
  if(mrnum % 50 > 0) iprintf(" (%08d)\n", mrnum);
  if(tcadbrnum(adb) != 0){
    eprint(adb, "(validation)");
    err = true;
  }
  iprintf("record number: %llu\n", (unsigned long long)tcadbrnum(adb));
  iprintf("size: %llu\n", (unsigned long long)tcadbsize(adb));
  tcmapdel(map);
  if(!tcadbclose(adb)){
    eprint(adb, "tcadbclose");
    err = true;
  }
  tcadbdel(adb);
  iprintf("time: %.3f\n", tctime() - stime);
  iprintf("%s\n\n", err ? "error" : "ok");
  return err ? 1 : 0;
}



// END OF FILE
