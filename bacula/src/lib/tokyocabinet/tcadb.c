/*************************************************************************************************
 * The abstract database API of Tokyo Cabinet
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


#include "tcutil.h"
#include "tchdb.h"
#include "tcbdb.h"
#include "tcadb.h"
#include "myconf.h"



/*************************************************************************************************
 * API
 *************************************************************************************************/


/* Create an abstract database object. */
TCADB *tcadbnew(void){
  TCADB *adb;
  TCMALLOC(adb, sizeof(*adb));
  adb->name = NULL;
  adb->mdb = NULL;
  adb->hdb = NULL;
  adb->bdb = NULL;
  adb->capnum = -1;
  adb->capsiz = -1;
  adb->capcnt = 0;
  adb->cur = NULL;
  return adb;
}


/* Delete an abstract database object. */
void tcadbdel(TCADB *adb){
  assert(adb);
  if(adb->name) tcadbclose(adb);
  free(adb);
}


/* Open an abstract database. */
bool tcadbopen(TCADB *adb, const char *name){
  assert(adb && name);
  TCLIST *elems = tcstrsplit(name, "#");
  char *path = tclistshift2(elems);
  if(!path){
    tclistdel(elems);
    return false;
  }
  int64_t bnum = -1;
  int64_t capnum = -1;
  int64_t capsiz = -1;
  bool owmode = true;
  bool ocmode = true;
  bool otmode = false;
  bool onlmode = false;
  bool onbmode = false;
  int8_t apow = -1;
  int8_t fpow = -1;
  bool tlmode = false;
  bool tdmode = false;
  int32_t rcnum = -1;
  int32_t lmemb = -1;
  int32_t nmemb = -1;
  int32_t lcnum = -1;
  int32_t ncnum = -1;
  int ln = TCLISTNUM(elems);
  for(int i = 0; i < ln; i++){
    const char *elem = TCLISTVALPTR(elems, i);
    char *pv = strchr(elem, '=');
    if(!pv) continue;
    *(pv++) = '\0';
    if(!tcstricmp(elem, "bnum")){
      bnum = strtoll(pv, NULL, 10);
    } else if(!tcstricmp(elem, "capnum")){
      capnum = strtoll(pv, NULL, 10);
    } else if(!tcstricmp(elem, "capsiz")){
      capsiz = strtoll(pv, NULL, 10);
    } else if(!tcstricmp(elem, "mode")){
      owmode = strchr(pv, 'w') || strchr(pv, 'W');
      ocmode = strchr(pv, 'c') || strchr(pv, 'C');
      otmode = strchr(pv, 't') || strchr(pv, 'T');
      onlmode = strchr(pv, 'e') || strchr(pv, 'E');
      onbmode = strchr(pv, 'f') || strchr(pv, 'F');
    } else if(!tcstricmp(elem, "apow")){
      apow = strtol(pv, NULL, 10);
    } else if(!tcstricmp(elem, "fpow")){
      fpow = strtol(pv, NULL, 10);
    } else if(!tcstricmp(elem, "opts")){
      if(strchr(pv, 'l') || strchr(pv, 'L')) tlmode = true;
      if(strchr(pv, 'd') || strchr(pv, 'D')) tdmode = true;
    } else if(!tcstricmp(elem, "rcnum")){
      rcnum = strtol(pv, NULL, 10);
    } else if(!tcstricmp(elem, "lmemb")){
      lmemb = strtol(pv, NULL, 10);
    } else if(!tcstricmp(elem, "nmemb")){
      nmemb = strtol(pv, NULL, 10);
    } else if(!tcstricmp(elem, "lcnum")){
      lcnum = strtol(pv, NULL, 10);
    } else if(!tcstricmp(elem, "ncnum")){
      ncnum = strtol(pv, NULL, 10);
    }
  }
  tclistdel(elems);
  if(!tcstricmp(path, "*")){
    adb->mdb = bnum > 0 ? tcmdbnew2(bnum) : tcmdbnew();
    adb->capnum = capnum;
    adb->capsiz = capsiz;
    adb->capcnt = 0;
  } else if(tcstribwm(path, ".tch")){
    TCHDB *hdb = tchdbnew();
    tchdbsetmutex(hdb);
    int opts = 0;
    if(tlmode) opts |= HDBTLARGE;
    if(tdmode) opts |= HDBTDEFLATE;
    tchdbtune(hdb, bnum, apow, fpow, opts);
    tchdbsetcache(hdb, rcnum);
    int omode = owmode ? HDBOWRITER : HDBOREADER;
    if(ocmode) omode |= HDBOCREAT;
    if(otmode) omode |= HDBOTRUNC;
    if(onlmode) omode |= HDBONOLCK;
    if(onbmode) omode |= HDBOLCKNB;
    if(!tchdbopen(hdb, path, omode)){
      tchdbdel(hdb);
      free(path);
      return false;
    }
    adb->hdb = hdb;
  } else if(tcstribwm(path, ".tcb")){
    TCBDB *bdb = tcbdbnew();
    tcbdbsetmutex(bdb);
    int opts = 0;
    if(tlmode) opts |= BDBTLARGE;
    if(tdmode) opts |= BDBTDEFLATE;
    tcbdbtune(bdb, lmemb, nmemb, bnum, apow, fpow, opts);
    tcbdbsetcache(bdb, lcnum, ncnum);
    if(capnum > 0) tcbdbsetcapnum(bdb, capnum);
    int omode = owmode ? BDBOWRITER : BDBOREADER;
    if(ocmode) omode |= BDBOCREAT;
    if(otmode) omode |= BDBOTRUNC;
    if(onlmode) omode |= BDBONOLCK;
    if(onbmode) omode |= BDBOLCKNB;
    if(!tcbdbopen(bdb, path, omode)){
      tcbdbdel(bdb);
      free(path);
      return false;
    }
    adb->bdb = bdb;
    adb->cur = tcbdbcurnew(bdb);
  } else {
    free(path);
    return false;
  }
  free(path);
  adb->name = tcstrdup(name);
  return true;
}


/* Close an abstract database object. */
bool tcadbclose(TCADB *adb){
  assert(adb);
  int err = false;
  if(!adb->name) return false;
  if(adb->mdb){
    tcmdbdel(adb->mdb);
    adb->mdb = NULL;
  } else if(adb->hdb){
    if(!tchdbclose(adb->hdb)) err = true;
    tchdbdel(adb->hdb);
    adb->hdb = NULL;
  } else if(adb->bdb){
    tcbdbcurdel(adb->cur);
    if(!tcbdbclose(adb->bdb)) err = true;
    tcbdbdel(adb->bdb);
    adb->bdb = NULL;
  }
  free(adb->name);
  adb->name = NULL;
  return !err;
}


/* Store a record into an abstract database object. */
bool tcadbput(TCADB *adb, const void *kbuf, int ksiz, const void *vbuf, int vsiz){
  assert(adb && kbuf && ksiz >= 0 && vbuf && vsiz >= 0);
  bool err = false;
  if(adb->mdb){
    tcmdbput(adb->mdb, kbuf, ksiz, vbuf, vsiz);
    if(adb->capnum > 0 || adb->capsiz > 0){
      adb->capcnt++;
      if((adb->capcnt & 0xff) == 0){
        if(adb->capnum > 0 && tcmdbrnum(adb->mdb) > adb->capnum) tcmdbcutfront(adb->mdb, 0x100);
        if(adb->capsiz > 0 && tcmdbmsiz(adb->mdb) > adb->capsiz) tcmdbcutfront(adb->mdb, 0x200);
      }
    }
  } else if(adb->hdb){
    if(!tchdbput(adb->hdb, kbuf, ksiz, vbuf, vsiz)) err = true;
  } else if(adb->bdb){
    if(!tcbdbput(adb->bdb, kbuf, ksiz, vbuf, vsiz)) err = true;
  } else {
    err = true;
  }
  return !err;
}


/* Store a string record into an abstract object. */
bool tcadbput2(TCADB *adb, const char *kstr, const char *vstr){
  assert(adb && kstr && vstr);
  return tcadbput(adb, kstr, strlen(kstr), vstr, strlen(vstr));
}


/* Store a new record into an abstract database object. */
bool tcadbputkeep(TCADB *adb, const void *kbuf, int ksiz, const void *vbuf, int vsiz){
  assert(adb && kbuf && ksiz >= 0 && vbuf && vsiz >= 0);
  bool err = false;
  if(adb->mdb){
    if(tcmdbputkeep(adb->mdb, kbuf, ksiz, vbuf, vsiz)){
      if(adb->capnum > 0 || adb->capsiz > 0){
        adb->capcnt++;
        if((adb->capcnt & 0xff) == 0){
          if(adb->capnum > 0 && tcmdbrnum(adb->mdb) > adb->capnum) tcmdbcutfront(adb->mdb, 0x100);
          if(adb->capsiz > 0 && tcmdbmsiz(adb->mdb) > adb->capsiz) tcmdbcutfront(adb->mdb, 0x200);
        }
      }
    } else {
      err = true;
    }
  } else if(adb->hdb){
    if(!tchdbputkeep(adb->hdb, kbuf, ksiz, vbuf, vsiz)) err = true;
  } else if(adb->bdb){
    if(!tcbdbputkeep(adb->bdb, kbuf, ksiz, vbuf, vsiz)) err = true;
  } else {
    err = true;
  }
  return !err;
}


/* Store a new string record into an abstract database object. */
bool tcadbputkeep2(TCADB *adb, const char *kstr, const char *vstr){
  assert(adb && kstr && vstr);
  return tcadbputkeep(adb, kstr, strlen(kstr), vstr, strlen(vstr));
}


/* Concatenate a value at the end of the existing record in an abstract database object. */
bool tcadbputcat(TCADB *adb, const void *kbuf, int ksiz, const void *vbuf, int vsiz){
  assert(adb && kbuf && ksiz >= 0 && vbuf && vsiz >= 0);
  bool err = false;
  if(adb->mdb){
    tcmdbputcat(adb->mdb, kbuf, ksiz, vbuf, vsiz);
    if(adb->capnum > 0 || adb->capsiz > 0){
      adb->capcnt++;
      if((adb->capcnt & 0xff) == 0){
        if(adb->capnum > 0 && tcmdbrnum(adb->mdb) > adb->capnum) tcmdbcutfront(adb->mdb, 0x100);
        if(adb->capsiz > 0 && tcmdbmsiz(adb->mdb) > adb->capsiz) tcmdbcutfront(adb->mdb, 0x200);
      }
    }
  } else if(adb->hdb){
    if(!tchdbputcat(adb->hdb, kbuf, ksiz, vbuf, vsiz)) err = true;
  } else if(adb->bdb){
    if(!tcbdbputcat(adb->bdb, kbuf, ksiz, vbuf, vsiz)) err = true;
  } else {
    err = true;
  }
  return !err;
}


/* Concatenate a string value at the end of the existing record in an abstract database object. */
bool tcadbputcat2(TCADB *adb, const char *kstr, const char *vstr){
  assert(adb && kstr && vstr);
  return tcadbputcat(adb, kstr, strlen(kstr), vstr, strlen(vstr));
}


/* Remove a record of an abstract database object. */
bool tcadbout(TCADB *adb, const void *kbuf, int ksiz){
  assert(adb && kbuf && ksiz >= 0);
  bool err = false;
  if(adb->mdb){
    if(!tcmdbout(adb->mdb, kbuf, ksiz)) err = true;
  } else if(adb->hdb){
    if(!tchdbout(adb->hdb, kbuf, ksiz)) err = true;
  } else if(adb->bdb){
    if(!tcbdbout(adb->bdb, kbuf, ksiz)) err = true;
  } else {
    err = true;
  }
  return !err;
}


/* Remove a string record of an abstract database object. */
bool tcadbout2(TCADB *adb, const char *kstr){
  assert(adb && kstr);
  return tcadbout(adb, kstr, strlen(kstr));
}


/* Retrieve a record in an abstract database object. */
void *tcadbget(TCADB *adb, const void *kbuf, int ksiz, int *sp){
  assert(adb && kbuf && ksiz >= 0 && sp);
  char *rv;
  if(adb->mdb){
    rv = tcmdbget(adb->mdb, kbuf, ksiz, sp);
  } else if(adb->hdb){
    rv = tchdbget(adb->hdb, kbuf, ksiz, sp);
  } else if(adb->bdb){
    rv = tcbdbget(adb->bdb, kbuf, ksiz, sp);
  } else {
    rv = NULL;
  }
  return rv;
}


/* Retrieve a string record in an abstract database object. */
char *tcadbget2(TCADB *adb, const char *kstr){
  assert(adb && kstr);
  int vsiz;
  return tcadbget(adb, kstr, strlen(kstr), &vsiz);
}


/* Get the size of the value of a record in an abstract database object. */
int tcadbvsiz(TCADB *adb, const void *kbuf, int ksiz){
  assert(adb && kbuf && ksiz >= 0);
  int rv;
  if(adb->mdb){
    rv = tcmdbvsiz(adb->mdb, kbuf, ksiz);
  } else if(adb->hdb){
    rv = tchdbvsiz(adb->hdb, kbuf, ksiz);
  } else if(adb->bdb){
    rv = tcbdbvsiz(adb->bdb, kbuf, ksiz);
  } else {
    rv = -1;
  }
  return rv;
}


/* Get the size of the value of a string record in an abstract database object. */
int tcadbvsiz2(TCADB *adb, const char *kstr){
  assert(adb && kstr);
  return tcadbvsiz(adb, kstr, strlen(kstr));
}


/* Initialize the iterator of an abstract database object. */
bool tcadbiterinit(TCADB *adb){
  assert(adb);
  bool err = false;
  if(adb->mdb){
    tcmdbiterinit(adb->mdb);
  } else if(adb->hdb){
    if(!tchdbiterinit(adb->hdb)) err = true;
  } else if(adb->bdb){
    if(!tcbdbcurfirst(adb->cur)){
      int ecode = tcbdbecode(adb->bdb);
      if(ecode != TCESUCCESS && ecode != TCEINVALID && ecode != TCEKEEP && ecode != TCENOREC)
        err = true;
    }
  } else {
    err = true;
  }
  return !err;
}


/* Get the next key of the iterator of an abstract database object. */
void *tcadbiternext(TCADB *adb, int *sp){
  assert(adb && sp);
  char *rv;
  if(adb->mdb){
    rv = tcmdbiternext(adb->mdb, sp);
  } else if(adb->hdb){
    rv = tchdbiternext(adb->hdb, sp);
  } else if(adb->bdb){
    rv = tcbdbcurkey(adb->cur, sp);
    tcbdbcurnext(adb->cur);
  } else {
    rv = NULL;
  }
  return rv;
}


/* Get the next key string of the iterator of an abstract database object. */
char *tcadbiternext2(TCADB *adb){
  assert(adb);
  int vsiz;
  return tcadbiternext(adb, &vsiz);
}


/* Get forward matching keys in an abstract database object. */
TCLIST *tcadbfwmkeys(TCADB *adb, const void *pbuf, int psiz, int max){
  assert(adb && pbuf && psiz >= 0);
  TCLIST *rv;
  if(adb->mdb){
    rv = tcmdbfwmkeys(adb->mdb, pbuf, psiz, max);
  } else if(adb->hdb){
    rv = tchdbfwmkeys(adb->hdb, pbuf, psiz, max);
  } else if(adb->bdb){
    rv = tcbdbfwmkeys(adb->bdb, pbuf, psiz, max);
  } else {
    rv = tclistnew();
  }
  return rv;
}


/* Get forward matching string keys in an abstract database object. */
TCLIST *tcadbfwmkeys2(TCADB *adb, const char *pstr, int max){
  assert(adb && pstr);
  return tcadbfwmkeys(adb, pstr, strlen(pstr), max);
}


/* Synchronize updated contents of an abstract database object with the file and the device. */
bool tcadbsync(TCADB *adb){
  assert(adb);
  bool err = false;
  if(adb->mdb){
    err = true;
  } else if(adb->hdb){
    if(!tchdbsync(adb->hdb)) err = true;
  } else if(adb->bdb){
    if(!tcbdbsync(adb->bdb)) err = true;
  } else {
    err = true;
  }
  return !err;
}


/* Remove all records of an abstract database object. */
bool tcadbvanish(TCADB *adb){
  assert(adb);
  bool err = false;
  if(adb->mdb){
    tcmdbvanish(adb->mdb);
  } else if(adb->hdb){
    if(!tchdbvanish(adb->hdb)) err = true;
  } else if(adb->bdb){
    if(!tcbdbvanish(adb->bdb)) err = true;
  } else {
    err = true;
  }
  return !err;
}


/* Copy the database file of an abstract database object. */
bool tcadbcopy(TCADB *adb, const char *path){
  assert(adb && path);
  bool err = false;
  if(adb->mdb){
    err = true;
  } else if(adb->hdb){
    if(!tchdbcopy(adb->hdb, path)) err = true;
  } else if(adb->bdb){
    if(!tcbdbcopy(adb->bdb, path)) err = true;
  } else {
    err = true;
  }
  return !err;
}


/* Get the number of records of an abstract database object. */
uint64_t tcadbrnum(TCADB *adb){
  assert(adb);
  uint64_t rv;
  if(adb->mdb){
    rv = tcmdbrnum(adb->mdb);
  } else if(adb->hdb){
    rv = tchdbrnum(adb->hdb);
  } else if(adb->bdb){
    rv = tcbdbrnum(adb->bdb);
  } else {
    rv = 0;
  }
  return rv;
}


/* Get the size of the database of an abstract database object. */
uint64_t tcadbsize(TCADB *adb){
  assert(adb);
  uint64_t rv;
  if(adb->mdb){
    rv = tcmdbmsiz(adb->mdb);
  } else if(adb->hdb){
    rv = tchdbfsiz(adb->hdb);
  } else if(adb->bdb){
    rv = tcbdbfsiz(adb->bdb);
  } else {
    rv = 0;
  }
  return rv;
}



// END OF FILE
