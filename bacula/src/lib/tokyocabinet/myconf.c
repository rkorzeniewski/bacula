/*************************************************************************************************
 * System-dependent configurations of Tokyo Cabinet
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


#include "myconf.h"



/*************************************************************************************************
 * common settings
 *************************************************************************************************/


int _tc_dummyfunc(void){
  return 0;
}


int _tc_dummyfuncv(int a, ...){
  return 0;
}



/*************************************************************************************************
 * for ZLIB
 *************************************************************************************************/


#if TCUSEZLIB


#include <zlib.h>

#define ZLIBBUFSIZ     8192


static char *_tc_deflate_impl(const char *ptr, int size, int *sp, int mode);
static char *_tc_inflate_impl(const char *ptr, int size, int *sp, int mode);
static unsigned int _tc_getcrc_impl(const char *ptr, int size);


char *(*_tc_deflate)(const char *, int, int *, int) = _tc_deflate_impl;
char *(*_tc_inflate)(const char *, int, int *, int) = _tc_inflate_impl;
unsigned int (*_tc_getcrc)(const char *, int) = _tc_getcrc_impl;


static char *_tc_deflate_impl(const char *ptr, int size, int *sp, int mode){
  assert(ptr && size >= 0 && sp);
  z_stream zs;
  char *buf, *swap;
  unsigned char obuf[ZLIBBUFSIZ];
  int rv, asiz, bsiz, osiz;
  zs.zalloc = Z_NULL;
  zs.zfree = Z_NULL;
  zs.opaque = Z_NULL;
  switch(mode){
  case _TCZMRAW:
    if(deflateInit2(&zs, 5, Z_DEFLATED, -15, 7, Z_DEFAULT_STRATEGY) != Z_OK)
      return NULL;
    break;
  case _TCZMGZIP:
    if(deflateInit2(&zs, 6, Z_DEFLATED, 15 + 16, 9, Z_DEFAULT_STRATEGY) != Z_OK)
      return NULL;
    break;
  default:
    if(deflateInit2(&zs, 6, Z_DEFLATED, 15, 8, Z_DEFAULT_STRATEGY) != Z_OK)
      return NULL;
    break;
  }
  asiz = size + 16;
  if(asiz < ZLIBBUFSIZ) asiz = ZLIBBUFSIZ;
  if(!(buf = malloc(asiz))){
    deflateEnd(&zs);
    return NULL;
  }
  bsiz = 0;
  zs.next_in = (unsigned char *)ptr;
  zs.avail_in = size;
  zs.next_out = obuf;
  zs.avail_out = ZLIBBUFSIZ;
  while((rv = deflate(&zs, Z_FINISH)) == Z_OK){
    osiz = ZLIBBUFSIZ - zs.avail_out;
    if(bsiz + osiz > asiz){
      asiz = asiz * 2 + osiz;
      if(!(swap = realloc(buf, asiz))){
        free(buf);
        deflateEnd(&zs);
        return NULL;
      }
      buf = swap;
    }
    memcpy(buf + bsiz, obuf, osiz);
    bsiz += osiz;
    zs.next_out = obuf;
    zs.avail_out = ZLIBBUFSIZ;
  }
  if(rv != Z_STREAM_END){
    free(buf);
    deflateEnd(&zs);
    return NULL;
  }
  osiz = ZLIBBUFSIZ - zs.avail_out;
  if(bsiz + osiz + 1 > asiz){
    asiz = asiz * 2 + osiz;
    if(!(swap = realloc(buf, asiz))){
      free(buf);
      deflateEnd(&zs);
      return NULL;
    }
    buf = swap;
  }
  memcpy(buf + bsiz, obuf, osiz);
  bsiz += osiz;
  buf[bsiz] = '\0';
  if(mode == _TCZMRAW) bsiz++;
  *sp = bsiz;
  deflateEnd(&zs);
  return buf;
}


static char *_tc_inflate_impl(const char *ptr, int size, int *sp, int mode){
  assert(ptr && size >= 0 && sp);
  z_stream zs;
  char *buf, *swap;
  unsigned char obuf[ZLIBBUFSIZ];
  int rv, asiz, bsiz, osiz;
  zs.zalloc = Z_NULL;
  zs.zfree = Z_NULL;
  zs.opaque = Z_NULL;
  switch(mode){
  case _TCZMRAW:
    if(inflateInit2(&zs, -15) != Z_OK) return NULL;
    break;
  case _TCZMGZIP:
    if(inflateInit2(&zs, 15 + 16) != Z_OK) return NULL;
    break;
  default:
    if(inflateInit2(&zs, 15) != Z_OK) return NULL;
    break;
  }
  asiz = size * 2 + 16;
  if(asiz < ZLIBBUFSIZ) asiz = ZLIBBUFSIZ;
  if(!(buf = malloc(asiz))){
    inflateEnd(&zs);
    return NULL;
  }
  bsiz = 0;
  zs.next_in = (unsigned char *)ptr;
  zs.avail_in = size;
  zs.next_out = obuf;
  zs.avail_out = ZLIBBUFSIZ;
  while((rv = inflate(&zs, Z_NO_FLUSH)) == Z_OK){
    osiz = ZLIBBUFSIZ - zs.avail_out;
    if(bsiz + osiz >= asiz){
      asiz = asiz * 2 + osiz;
      if(!(swap = realloc(buf, asiz))){
        free(buf);
        inflateEnd(&zs);
        return NULL;
      }
      buf = swap;
    }
    memcpy(buf + bsiz, obuf, osiz);
    bsiz += osiz;
    zs.next_out = obuf;
    zs.avail_out = ZLIBBUFSIZ;
  }
  if(rv != Z_STREAM_END){
    free(buf);
    inflateEnd(&zs);
    return NULL;
  }
  osiz = ZLIBBUFSIZ - zs.avail_out;
  if(bsiz + osiz >= asiz){
    asiz = asiz * 2 + osiz;
    if(!(swap = realloc(buf, asiz))){
      free(buf);
      inflateEnd(&zs);
      return NULL;
    }
    buf = swap;
  }
  memcpy(buf + bsiz, obuf, osiz);
  bsiz += osiz;
  buf[bsiz] = '\0';
  *sp = bsiz;
  inflateEnd(&zs);
  return buf;
}


static unsigned int _tc_getcrc_impl(const char *ptr, int size){
  assert(ptr && size >= 0);
  int crc = crc32(0, Z_NULL, 0);
  return crc32(crc, (unsigned char *)ptr, size);
}


#else


char *(*_tc_deflate)(const char *, int, int *, int) = NULL;
char *(*_tc_inflate)(const char *, int, int *, int) = NULL;
unsigned int (*_tc_getcrc)(const char *, int) = NULL;


#endif



// END OF FILE
