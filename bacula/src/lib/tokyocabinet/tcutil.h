/*************************************************************************************************
 * The utility API of Tokyo Cabinet
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


#ifndef _TCUTIL_H                        /* duplication check */
#define _TCUTIL_H

#if defined(__cplusplus)
#define __TCUTIL_CLINKAGEBEGIN extern "C" {
#define __TCUTIL_CLINKAGEEND }
#else
#define __TCUTIL_CLINKAGEBEGIN
#define __TCUTIL_CLINKAGEEND
#endif
__TCUTIL_CLINKAGEBEGIN


#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>



/*************************************************************************************************
 * basic utilities
 *************************************************************************************************/


/* String containing the version information. */
extern const char *tcversion;


/* Pointer to the call back function for handling a fatal error.
   The argument specifies the error message.
   The initial value of this variable is `NULL'.  If the value is `NULL', the default function is
   called when a fatal error occurs.  A fatal error occurs when memory allocation is failed. */
extern void (*tcfatalfunc)(const char *);


/* Allocate a region on memory.
   `size' specifies the size of the region.
   The return value is the pointer to the allocated region.
   This function handles failure of memory allocation implicitly.  Because the region of the
   return value is allocated with the `malloc' call, it should be released with the `free' call
   when it is no longer in use. */
void *tcmalloc(size_t size);


/* Allocate a nullified region on memory.
   `nmemb' specifies the number of elements.
   `size' specifies the size of each element.
   The return value is the pointer to the allocated nullified region.
   This function handles failure of memory allocation implicitly.  Because the region of the
   return value is allocated with the `calloc' call, it should be released with the `free' call
   when it is no longer in use. */
void *tccalloc(size_t nmemb, size_t size);


/* Re-allocate a region on memory.
   `ptr' specifies the pointer to the region.
   `size' specifies the size of the region.
   The return value is the pointer to the re-allocated region.
   This function handles failure of memory allocation implicitly.  Because the region of the
   return value is allocated with the `realloc' call, it should be released with the `free' call
   when it is no longer in use. */
void *tcrealloc(void *ptr, size_t size);


/* Duplicate a region on memory.
   `ptr' specifies the pointer to the region.
   `size' specifies the size of the region.
   The return value is the pointer to the allocated region of the duplicate.
   Because an additional zero code is appended at the end of the region of the return value,
   the return value can be treated as a character string.  Because the region of the return
   value is allocated with the `malloc' call, it should be released with the `free' call when
   it is no longer in use. */
void *tcmemdup(const void *ptr, size_t size);


/* Duplicate a string on memory.
   `str' specifies the string.
   The return value is the allocated string equivalent to the specified string.
   Because the region of the return value is allocated with the `malloc' call, it should be
   released with the `free' call when it is no longer in use. */
char *tcstrdup(const void *str);


/* Free a region on memory.
   `ptr' specifies the pointer to the region.  If it is `NULL', this function has no effect.
   Although this function is just a wrapper of `free' call, this is useful in applications using
   another package of the `malloc' series. */
void tcfree(void *ptr);



/*************************************************************************************************
 * extensible string
 *************************************************************************************************/


typedef struct {                         /* type of structure for an extensible string object */
  char *ptr;                             /* pointer to the region */
  int size;                              /* size of the region */
  int asize;                             /* size of the allocated region */
} TCXSTR;


/* Create an extensible string object.
   The return value is the new extensible string object. */
TCXSTR *tcxstrnew(void);


/* Create an extensible string object from a character string.
   `str' specifies the string of the initial content.
   The return value is the new extensible string object containing the specified string. */
TCXSTR *tcxstrnew2(const char *str);


/* Create an extensible string object with the initial allocation size.
   `asiz' specifies the initial allocation size.
   The return value is the new extensible string object. */
TCXSTR *tcxstrnew3(int asiz);


/* Copy an extensible string object.
   `xstr' specifies the extensible string object.
   The return value is the new extensible string object equivalent to the specified object. */
TCXSTR *tcxstrdup(const TCXSTR *xstr);


/* Delete an extensible string object.
   `xstr' specifies the extensible string object.
   Note that the deleted object and its derivatives can not be used anymore. */
void tcxstrdel(TCXSTR *xstr);


/* Concatenate a region to the end of an extensible string object.
   `xstr' specifies the extensible string object.
   `ptr' specifies the pointer to the region to be appended.
   `size' specifies the size of the region. */
void tcxstrcat(TCXSTR *xstr, const void *ptr, int size);


/* Concatenate a character string to the end of an extensible string object.
   `xstr' specifies the extensible string object.
   `str' specifies the string to be appended. */
void tcxstrcat2(TCXSTR *xstr, const char *str);


/* Get the pointer of the region of an extensible string object.
   `xstr' specifies the extensible string object.
   The return value is the pointer of the region of the object.
   Because an additional zero code is appended at the end of the region of the return value,
   the return value can be treated as a character string. */
const void *tcxstrptr(const TCXSTR *xstr);


/* Get the size of the region of an extensible string object.
   `xstr' specifies the extensible string object.
   The return value is the size of the region of the object. */
int tcxstrsize(const TCXSTR *xstr);


/* Clear an extensible string object.
   `xstr' specifies the extensible string object.
   The internal buffer of the object is cleared and the size is set zero. */
void tcxstrclear(TCXSTR *xstr);


/* Convert an extensible string object into a usual allocated region.
   `xstr' specifies the extensible string object.
   The return value is the pointer to the allocated region of the object.
   Because an additional zero code is appended at the end of the region of the return value,
   the return value can be treated as a character string.  Because the region of the return
   value is allocated with the `malloc' call, it should be released with the `free' call when it
   is no longer in use.  Because the region of the original object is deleted, it should not be
   deleted again. */
void *tcxstrtomalloc(TCXSTR *xstr);


/* Create an extensible string object from an allocated region.
   `ptr' specifies the pointer to the region allocated with `malloc' call.
   `size' specifies the size of the region.
   The return value is the new extensible string object wrapping the specified region.
   Note that the specified region is released when the object is deleted. */
TCXSTR *tcxstrfrommalloc(void *ptr, int size);


/* Perform formatted output into an extensible string object.
   `xstr' specifies the extensible string object.
   `format' specifies the printf-like format string.
   The conversion character `%' can be used with such flag characters as `s', `d', `o', `u',
   `x', `X', `c', `e', `E', `f', `g', `G', `@', `?', `%'.  `@' works as with `s' but escapes meta
   characters of XML.  `?' works as with `s' but escapes meta characters of URL.  The other
   conversion character work as with each original.
   The other arguments are used according to the format string. */
void tcxstrprintf(TCXSTR *xstr, const char *format, ...);


/* Allocate a formatted string on memory.
   `format' specifies the printf-like format string.
   The conversion character `%' can be used with such flag characters as `s', `d', `o', `u',
   `x', `X', `c', `e', `E', `f', `g', `G', `@', `?', `%'.  `@' works as with `s' but escapes meta
   characters of XML.  `?' works as with `s' but escapes meta characters of URL.  The other
   conversion character work as with each original.
   The other arguments are used according to the format string.
   Because the region of the return value is allocated with the `malloc' call, it should be
   released with the `free' call when it is no longer in use. */
char *tcsprintf(const char *format, ...);



/*************************************************************************************************
 * array list
 *************************************************************************************************/


typedef struct {                         /* type of structure for an element of a list */
  char *ptr;                             /* pointer to the region */
  int size;                              /* size of the effective region */
} TCLISTDATUM;

typedef struct {                         /* type of structure for an array list */
  TCLISTDATUM *array;                    /* array of data */
  int anum;                              /* number of the elements of the array */
  int start;                             /* start index of used elements */
  int num;                               /* number of used elements */
} TCLIST;


/* Create a list object.
   The return value is the new list object. */
TCLIST *tclistnew(void);


/* Create a list object with expecting the number of elements.
   `anum' specifies the number of elements expected to be stored in the list.
   The return value is the new list object. */
TCLIST *tclistnew2(int anum);


/* Copy a list object.
   `list' specifies the list object.
   The return value is the new list object equivalent to the specified object. */
TCLIST *tclistdup(const TCLIST *list);


/* Delete a list object.
   `list' specifies the list object.
   Note that the deleted object and its derivatives can not be used anymore. */
void tclistdel(TCLIST *list);


/* Get the number of elements of a list object.
   `list' specifies the list object.
   The return value is the number of elements of the list. */
int tclistnum(const TCLIST *list);


/* Get the pointer to the region of an element of a list object.
   `list' specifies the list object.
   `index' specifies the index of the element.
   `sp' specifies the pointer to the variable into which the size of the region of the return
   value is assigned.
   The return value is the pointer to the region of the value.
   Because an additional zero code is appended at the end of the region of the return value,
   the return value can be treated as a character string.  If `index' is equal to or more than
   the number of elements, the return value is `NULL'. */
const void *tclistval(const TCLIST *list, int index, int *sp);


/* Get the string of an element of a list object.
   `list' specifies the list object.
   `index' specifies the index of the element.
   The return value is the string of the value.
   If `index' is equal to or more than the number of elements, the return value is `NULL'. */
const char *tclistval2(const TCLIST *list, int index);


/* Add an element at the end of a list object.
   `list' specifies the list object.
   `ptr' specifies the pointer to the region of the new element.
   `size' specifies the size of the region. */
void tclistpush(TCLIST *list, const void *ptr, int size);


/* Add a string element at the end of a list object.
   `list' specifies the list object.
   `str' specifies the string of the new element. */
void tclistpush2(TCLIST *list, const char *str);


/* Add an allocated element at the end of a list object.
   `list' specifies the list object.
   `ptr' specifies the pointer to the region allocated with `malloc' call.
   `size' specifies the size of the region.
   Note that the specified region is released when the object is deleted. */
void tclistpushmalloc(TCLIST *list, void *ptr, int size);


/* Remove an element of the end of a list object.
   `list' specifies the list object.
   `sp' specifies the pointer to the variable into which the size of the region of the return
   value is assigned.
   The return value is the pointer to the region of the removed element.
   Because an additional zero code is appended at the end of the region of the return value,
   the return value can be treated as a character string.  Because the region of the return
   value is allocated with the `malloc' call, it should be released with the `free' call when it
   is no longer in use.  If the list is empty, the return value is `NULL'. */
void *tclistpop(TCLIST *list, int *sp);


/* Remove a string element of the end of a list object.
   `list' specifies the list object.
   The return value is the string of the removed element.
   Because the region of the return value is allocated with the `malloc' call, it should be
   released with the `free' call when it is no longer in use.  If the list is empty, the return
   value is `NULL'. */
char *tclistpop2(TCLIST *list);


/* Add an element at the top of a list object.
   `list' specifies the list object.
   `ptr' specifies the pointer to the region of the new element.
   `size' specifies the size of the region. */
void tclistunshift(TCLIST *list, const void *ptr, int size);


/* Add a string element at the top of a list object.
   `list' specifies the list object.
   `str' specifies the string of the new element. */
void tclistunshift2(TCLIST *list, const char *str);


/* Remove an element of the top of a list object.
   `list' specifies the list object.
   `sp' specifies the pointer to the variable into which the size of the region of the return
   value is assigned.
   The return value is the pointer to the region of the removed element.
   Because an additional zero code is appended at the end of the region of the return value,
   the return value can be treated as a character string.  Because the region of the return
   value is allocated with the `malloc' call, it should be released with the `free' call when it
   is no longer in use.  If the list is empty, the return value is `NULL'. */
void *tclistshift(TCLIST *list, int *sp);


/* Remove a string element of the top of a list object.
   `list' specifies the list object.
   The return value is the string of the removed element.
   Because the region of the return value is allocated with the `malloc' call, it should be
   released with the `free' call when it is no longer in use.  If the list is empty, the return
   value is `NULL'. */
char *tclistshift2(TCLIST *list);


/* Add an element at the specified location of a list object.
   `list' specifies the list object.
   `index' specifies the index of the new element.
   `ptr' specifies the pointer to the region of the new element.
   `size' specifies the size of the region.
   If `index' is equal to or more than the number of elements, this function has no effect. */
void tclistinsert(TCLIST *list, int index, const void *ptr, int size);


/* Add a string element at the specified location of a list object.
   `list' specifies the list object.
   `index' specifies the index of the new element.
   `str' specifies the string of the new element.
   If `index' is equal to or more than the number of elements, this function has no effect. */
void tclistinsert2(TCLIST *list, int index, const char *str);


/* Remove an element at the specified location of a list object.
   `list' specifies the list object.
   `index' specifies the index of the element to be removed.
   `sp' specifies the pointer to the variable into which the size of the region of the return
   value is assigned.
   The return value is the pointer to the region of the removed element.
   Because an additional zero code is appended at the end of the region of the return value,
   the return value can be treated as a character string.  Because the region of the return
   value is allocated with the `malloc' call, it should be released with the `free' call when it
   is no longer in use.  If `index' is equal to or more than the number of elements, no element
   is removed and the return value is `NULL'. */
void *tclistremove(TCLIST *list, int index, int *sp);


/* Remove a string element at the specified location of a list object.
   `list' specifies the list object.
   `index' specifies the index of the element to be removed.
   The return value is the string of the removed element.
   Because the region of the return value is allocated with the `malloc' call, it should be
   released with the `free' call when it is no longer in use.  If `index' is equal to or more
   than the number of elements, no element is removed and the return value is `NULL'. */
char *tclistremove2(TCLIST *list, int index);


/* Overwrite an element at the specified location of a list object.
   `list' specifies the list object.
   `index' specifies the index of the element to be overwritten.
   `ptr' specifies the pointer to the region of the new content.
   `size' specifies the size of the new content.
   If `index' is equal to or more than the number of elements, this function has no effect. */
void tclistover(TCLIST *list, int index, const void *ptr, int size);


/* Overwrite a string element at the specified location of a list object.
   `list' specifies the list object.
   `index' specifies the index of the element to be overwritten.
   `str' specifies the string of the new content.
   If `index' is equal to or more than the number of elements, this function has no effect. */
void tclistover2(TCLIST *list, int index, const char *str);


/* Sort elements of a list object in lexical order.
   `list' specifies the list object. */
void tclistsort(TCLIST *list);


/* Sort elements of a list object in case-insensitive lexical order.
   `list' specifies the list object. */
void tclistsortci(TCLIST *list);


/* Sort elements of a list object by an arbitrary comparison function.
   `list' specifies the list object.
   `cmp' specifies the pointer to the comparison function.  The structure TCLISTDATUM has the
   member "ptr" which is the pointer to the region of the element, and the member "size" which is
   the size of the region. */
void tclistsortex(TCLIST *list, int (*cmp)(const TCLISTDATUM *, const TCLISTDATUM *));


/* Search a list object for an element using liner search.
   `list' specifies the list object.
   `ptr' specifies the pointer to the region of the key.
   `size' specifies the size of the region.
   The return value is the index of a corresponding element or -1 if there is no corresponding
   element.
   If two or more elements correspond, the former returns. */
int tclistlsearch(const TCLIST *list, const void *ptr, int size);


/* Search a list object for an element using binary search.
   `list' specifies the list object.  It should be sorted in lexical order.
   `ptr' specifies the pointer to the region of the key.
   `size' specifies the size of the region.
   The return value is the index of a corresponding element or -1 if there is no corresponding
   element.
   If two or more elements correspond, which returns is not defined. */
int tclistbsearch(const TCLIST *list, const void *ptr, int size);


/* Clear a list object.
   `list' specifies the list object.
   All elements are removed. */
void tclistclear(TCLIST *list);


/* Serialize a list object into a byte array.
   `list' specifies the list object.
   `sp' specifies the pointer to the variable into which the size of the region of the return
   value is assigned.
   The return value is the pointer to the region of the result serial region.
   Because the region of the return value is allocated with the `malloc' call, it should be
   released with the `free' call when it is no longer in use. */
void *tclistdump(const TCLIST *list, int *sp);


/* Create a list object from a serialized byte array.
   `ptr' specifies the pointer to the region of serialized byte array.
   `size' specifies the size of the region.
   The return value is a new list object.
   Because the object of the return value is created with the function `tclistnew', it should
   be deleted with the function `tclistdel' when it is no longer in use. */
TCLIST *tclistload(const void *ptr, int size);



/*************************************************************************************************
 * hash map
 *************************************************************************************************/


typedef struct _TCMAPREC {               /* type of structure for an element of a map */
  int ksiz;                              /* size of the region of the key */
  int vsiz;                              /* size of the region of the value */
  unsigned int hash;                     /* second hash value */
  struct _TCMAPREC *left;                /* pointer to the left child */
  struct _TCMAPREC *right;               /* pointer to the right child */
  struct _TCMAPREC *prev;                /* pointer to the previous element */
  struct _TCMAPREC *next;                /* pointer to the next element */
} TCMAPREC;

typedef struct {                         /* type of structure for a map */
  TCMAPREC **buckets;                    /* bucket array */
  TCMAPREC *first;                       /* pointer to the first element */
  TCMAPREC *last;                        /* pointer to the last element */
  TCMAPREC *cur;                         /* pointer to the current element */
  uint32_t bnum;                         /* number of buckets */
  uint64_t rnum;                         /* number of records */
  uint64_t msiz;                         /* total size of records */
} TCMAP;


/* Create a map object.
   The return value is the new map object. */
TCMAP *tcmapnew(void);


/* Create a map object with specifying the number of the buckets.
   `bnum' specifies the number of the buckets.
   The return value is the new map object. */
TCMAP *tcmapnew2(uint32_t bnum);


/* Copy a map object.
   `map' specifies the map object.
   The return value is the new map object equivalent to the specified object. */
TCMAP *tcmapdup(const TCMAP *map);


/* Delete a map object.
   `map' specifies the map object.
   Note that the deleted object and its derivatives can not be used anymore. */
void tcmapdel(TCMAP *map);


/* Store a record into a map object.
   `map' specifies the map object.
   `kbuf' specifies the pointer to the region of the key.
   `ksiz' specifies the size of the region of the key.
   `vbuf' specifies the pointer to the region of the value.
   `vsiz' specifies the size of the region of the value.
   If a record with the same key exists in the map, it is overwritten. */
void tcmapput(TCMAP *map, const void *kbuf, int ksiz, const void *vbuf, int vsiz);


/* Store a string record into a map object.
   `map' specifies the map object.
   `kstr' specifies the string of the key.
   `vstr' specifies the string of the value.
   If a record with the same key exists in the map, it is overwritten. */
void tcmapput2(TCMAP *map, const char *kstr, const char *vstr);


/* Store a record of the value of two regions into a map object.
   `map' specifies the map object.
   `kbuf' specifies the pointer to the region of the key.
   `ksiz' specifies the size of the region of the key.
   `fvbuf' specifies the pointer to the former region of the value.
   `fvsiz' specifies the size of the former region of the value.
   `lvbuf' specifies the pointer to the latter region of the value.
   `lvsiz' specifies the size of the latter region of the value.
   If a record with the same key exists in the map, it is overwritten. */
void tcmapput3(TCMAP *map, const char *kbuf, int ksiz,
               const void *fvbuf, int fvsiz, const char *lvbuf, int lvsiz);


/* Store a new record into a map object.
   `map' specifies the map object.
   `kbuf' specifies the pointer to the region of the key.
   `ksiz' specifies the size of the region of the key.
   `vbuf' specifies the pointer to the region of the value.
   `vsiz' specifies the size of the region of the value.
   If successful, the return value is true, else, it is false.
   If a record with the same key exists in the map, this function has no effect. */
bool tcmapputkeep(TCMAP *map, const void *kbuf, int ksiz, const void *vbuf, int vsiz);


/* Store a new string record into a map object.
   `map' specifies the map object.
   `kstr' specifies the string of the key.
   `vstr' specifies the string of the value.
   If successful, the return value is true, else, it is false.
   If a record with the same key exists in the map, this function has no effect. */
bool tcmapputkeep2(TCMAP *map, const char *kstr, const char *vstr);


/* Concatenate a value at the end of the value of the existing record in a map object.
   `map' specifies the map object.
   `kbuf' specifies the pointer to the region of the key.
   `ksiz' specifies the size of the region of the key.
   `vbuf' specifies the pointer to the region of the value.
   `vsiz' specifies the size of the region of the value.
   If there is no corresponding record, a new record is created. */
void tcmapputcat(TCMAP *map, const void *kbuf, int ksiz, const void *vbuf, int vsiz);


/* Concatenate a string value at the end of the value of the existing record in a map object.
   `map' specifies the map object.
   `kstr' specifies the string of the key.
   `vstr' specifies the string of the value.
   If there is no corresponding record, a new record is created. */
void tcmapputcat2(TCMAP *map, const char *kstr, const char *vstr);


/* Remove a record of a map object.
   `map' specifies the map object.
   `kbuf' specifies the pointer to the region of the key.
   `ksiz' specifies the size of the region of the key.
   If successful, the return value is true.  False is returned when no record corresponds to
   the specified key. */
bool tcmapout(TCMAP *map, const void *kbuf, int ksiz);


/* Remove a string record of a map object.
   `map' specifies the map object.
   `kstr' specifies the string of the key.
   If successful, the return value is true.  False is returned when no record corresponds to
   the specified key. */
bool tcmapout2(TCMAP *map, const char *kstr);


/* Retrieve a record in a map object.
   `map' specifies the map object.
   `kbuf' specifies the pointer to the region of the key.
   `ksiz' specifies the size of the region of the key.
   `sp' specifies the pointer to the variable into which the size of the region of the return
   value is assigned.
   If successful, the return value is the pointer to the region of the value of the
   corresponding record.  `NULL' is returned when no record corresponds.
   Because an additional zero code is appended at the end of the region of the return value,
   the return value can be treated as a character string. */
const void *tcmapget(const TCMAP *map, const void *kbuf, int ksiz, int *sp);


/* Retrieve a string record in a map object.
   `map' specifies the map object.
   `kstr' specifies the string of the key.
   If successful, the return value is the string of the value of the corresponding record.
   `NULL' is returned when no record corresponds. */
const char *tcmapget2(const TCMAP *map, const char *kstr);


/* Retrieve a semivolatile record in a map object.
   `map' specifies the map object.
   `kbuf' specifies the pointer to the region of the key.
   `ksiz' specifies the size of the region of the key.
   `sp' specifies the pointer to the variable into which the size of the region of the return
   value is assigned.
   If successful, the return value is the pointer to the region of the value of the
   corresponding record.  `NULL' is returned when no record corresponds.
   Because an additional zero code is appended at the end of the region of the return value,
   the return value can be treated as a character string.  The internal region of the returned
   record is moved to the tail so that the record will survive for a time under LRU cache
   algorithm removing records from the head. */
const void *tcmapget3(TCMAP *map, const void *kbuf, int ksiz, int *sp);


/* Move a record to the edge of a map object.
   `map' specifies the map object.
   `kbuf' specifies the pointer to the region of a key.
   `ksiz' specifies the size of the region of the key.
   `head' specifies the destination which is head if it is true or tail if else.
   If successful, the return value is true.  False is returned when no record corresponds to
   the specified key. */
bool tcmapmove(TCMAP *map, const void *kbuf, int ksiz, bool head);


/* Move a string record to the edge of a map object.
   `map' specifies the map object.
   `kstr' specifies the string of a key.
   `head' specifies the destination which is head if it is true or tail if else.
   If successful, the return value is true.  False is returned when no record corresponds to
   the specified key. */
bool tcmapmove2(TCMAP *map, const char *kstr, bool head);


/* Initialize the iterator of a map object.
   `map' specifies the map object.
   The iterator is used in order to access the key of every record stored in the map object. */
void tcmapiterinit(TCMAP *map);


/* Get the next key of the iterator of a map object.
   `map' specifies the map object.
   `sp' specifies the pointer to the variable into which the size of the region of the return
   value is assigned.
   If successful, the return value is the pointer to the region of the next key, else, it is
   `NULL'.  `NULL' is returned when no record can be fetched from the iterator.
   Because an additional zero code is appended at the end of the region of the return value,
   the return value can be treated as a character string.
   The order of iteration is assured to be the same as the stored order. */
const void *tcmapiternext(TCMAP *map, int *sp);


/* Get the next key string of the iterator of a map object.
   `map' specifies the map object.
   If successful, the return value is the pointer to the region of the next key, else, it is
   `NULL'.  `NULL' is returned when no record can be fetched from the iterator.
   The order of iteration is assured to be the same as the stored order. */
const char *tcmapiternext2(TCMAP *map);


/* Get the value bound to the key fetched from the iterator of a map object.
   `kbuf' specifies the pointer to the region of the iteration key.
   `sp' specifies the pointer to the variable into which the size of the region of the return
   value is assigned.
   The return value is the pointer to the region of the value of the corresponding record.
   Because an additional zero code is appended at the end of the region of the return value,
   the return value can be treated as a character string. */
const void *tcmapiterval(const void *kbuf, int *sp);


/* Get the value string bound to the key fetched from the iterator of a map object.
   `kstr' specifies the string of the iteration key.
   The return value is the pointer to the region of the value of the corresponding record. */
const char *tcmapiterval2(const char *kstr);


/* Get the number of records stored in a map object.
   `map' specifies the map object.
   The return value is the number of the records stored in the map object. */
uint64_t tcmaprnum(const TCMAP *map);


/* Get the total size of memory used in a map object.
   `map' specifies the map object.
   The return value is the total size of memory used in a map object. */
uint64_t tcmapmsiz(const TCMAP *map);


/* Create a list object containing all keys in a map object.
   `map' specifies the map object.
   The return value is the new list object containing all keys in the map object.
   Because the object of the return value is created with the function `tclistnew', it should
   be deleted with the function `tclistdel' when it is no longer in use. */
TCLIST *tcmapkeys(const TCMAP *map);


/* Create a list object containing all values in a map object.
   `map' specifies the map object.
   The return value is the new list object containing all values in the map object.
   Because the object of the return value is created with the function `tclistnew', it should
   be deleted with the function `tclistdel' when it is no longer in use. */
TCLIST *tcmapvals(const TCMAP *map);


/* Add an integer to a record in a map object.
   `map' specifies the map object.
   `kbuf' specifies the pointer to the region of the key.
   `ksiz' specifies the size of the region of the key.
   `num' specifies the additional value.
   If the corresponding record exists, the value is treated as a integer and is added to.  If no
   record corresponds, a new record of the additional value is stored. */
void tcmapaddint(TCMAP *map, const char *kbuf, int ksiz, int num);


/* Clear a map object.
   `map' specifies the map object.
   All records are removed. */
void tcmapclear(TCMAP *map);


/* Remove front records of a map object.
   `map' specifies the map object.
   `num' specifies the number of records to be removed.
   If successful, the return value is true.  False is returned when no record corresponds to
   the specified key. */
void tcmapcutfront(TCMAP *map, int num);


/* Serialize a map object into a byte array.
   `map' specifies the map object.
   `sp' specifies the pointer to the variable into which the size of the region of the return
   value is assigned.
   The return value is the pointer to the region of the result serial region.
   Because the region of the return value is allocated with the `malloc' call, it should be
   released with the `free' call when it is no longer in use. */
void *tcmapdump(const TCMAP *map, int *sp);


/* Create a map object from a serialized byte array.
   `ptr' specifies the pointer to the region of serialized byte array.
   `size' specifies the size of the region.
   The return value is a new map object.
   Because the object of the return value is created with the function `tcmapnew', it should be
   deleted with the function `tcmapdel' when it is no longer in use. */
TCMAP *tcmapload(const void *ptr, int size);


/* Extract a map record from a serialized byte array.
   `ptr' specifies the pointer to the region of serialized byte array.
   `size' specifies the size of the region.
   `kbuf' specifies the pointer to the region of the key.
   `ksiz' specifies the size of the region of the key.
   `sp' specifies the pointer to the variable into which the size of the region of the return
   value is assigned.
   If successful, the return value is the pointer to the region of the value of the
   corresponding record.  `NULL' is returned when no record corresponds.
   Because an additional zero code is appended at the end of the region of the return value,
   the return value can be treated as a character string. */
void *tcmaploadone(const void *ptr, int size, const void *kbuf, int ksiz, int *sp);



/*************************************************************************************************
 * on-memory database
 *************************************************************************************************/


typedef struct {                         /* type of structure for a on-memory database */
  void **mmtxs;                          /* mutexes for method */
  void *imtx;                            /* mutex for iterator */
  TCMAP **maps;                          /* internal map object */
  int iter;                              /* index of maps for the iterator */
} TCMDB;


/* Create an on-memory database object.
   The return value is the new on-memory database object.
   The object can be shared by plural threads because of the internal mutex. */
TCMDB *tcmdbnew(void);


/* Create an on-memory database object with specifying the number of the buckets.
   `bnum' specifies the number of the buckets.
   The return value is the new on-memory database object.
   The object can be shared by plural threads because of the internal mutex. */
TCMDB *tcmdbnew2(uint32_t bnum);


/* Delete an on-memory database object.
   `mdb' specifies the on-memory database object. */
void tcmdbdel(TCMDB *mdb);


/* Store a record into an on-memory database object.
   `mdb' specifies the on-memory database object.
   `kbuf' specifies the pointer to the region of the key.
   `ksiz' specifies the size of the region of the key.
   `vbuf' specifies the pointer to the region of the value.
   `vsiz' specifies the size of the region of the value.
   If a record with the same key exists in the database, it is overwritten. */
void tcmdbput(TCMDB *mdb, const void *kbuf, int ksiz, const void *vbuf, int vsiz);


/* Store a string record into an on-memory database object.
   `mdb' specifies the on-memory database object.
   `kstr' specifies the string of the key.
   `vstr' specifies the string of the value.
   If a record with the same key exists in the database, it is overwritten. */
void tcmdbput2(TCMDB *mdb, const char *kstr, const char *vstr);


/* Store a record of the value of two regions into an on-memory database object.
   `mdb' specifies the on-memory database object.
   `kbuf' specifies the pointer to the region of the key.
   `ksiz' specifies the size of the region of the key.
   `fvbuf' specifies the pointer to the former region of the value.
   `fvsiz' specifies the size of the former region of the value.
   `lvbuf' specifies the pointer to the latter region of the value.
   `lvsiz' specifies the size of the latter region of the value.
   If a record with the same key exists in the database, it is overwritten. */
void tcmdbput3(TCMDB *mdb, const char *kbuf, int ksiz,
               const void *fvbuf, int fvsiz, const char *lvbuf, int lvsiz);


/* Store a new record into an on-memory database object.
   `mdb' specifies the on-memory database object.
   `kbuf' specifies the pointer to the region of the key.
   `ksiz' specifies the size of the region of the key.
   `vbuf' specifies the pointer to the region of the value.
   `vsiz' specifies the size of the region of the value.
   If successful, the return value is true, else, it is false.
   If a record with the same key exists in the database, this function has no effect. */
bool tcmdbputkeep(TCMDB *mdb, const void *kbuf, int ksiz, const void *vbuf, int vsiz);


/* Store a new string record into an on-memory database object.
   `mdb' specifies the on-memory database object.
   `kstr' specifies the string of the key.
   `vstr' specifies the string of the value.
   If successful, the return value is true, else, it is false.
   If a record with the same key exists in the database, this function has no effect. */
bool tcmdbputkeep2(TCMDB *mdb, const char *kstr, const char *vstr);


/* Concatenate a value at the end of the value of the existing record in an on-memory database.
   `mdb' specifies the on-memory database object.
   `kbuf' specifies the pointer to the region of the key.
   `ksiz' specifies the size of the region of the key.
   `vbuf' specifies the pointer to the region of the value.
   `vsiz' specifies the size of the region of the value.
   If there is no corresponding record, a new record is created. */
void tcmdbputcat(TCMDB *mdb, const void *kbuf, int ksiz, const void *vbuf, int vsiz);


/* Concatenate a string at the end of the value of the existing record in an on-memory database.
   `mdb' specifies the on-memory database object.
   `kstr' specifies the string of the key.
   `vstr' specifies the string of the value.
   If there is no corresponding record, a new record is created. */
void tcmdbputcat2(TCMDB *mdb, const char *kstr, const char *vstr);


/* Remove a record of an on-memory database object.
   `mdb' specifies the on-memory database object.
   `kbuf' specifies the pointer to the region of the key.
   `ksiz' specifies the size of the region of the key.
   If successful, the return value is true.  False is returned when no record corresponds to
   the specified key. */
bool tcmdbout(TCMDB *mdb, const void *kbuf, int ksiz);


/* Remove a string record of an on-memory database object.
   `mdb' specifies the on-memory database object.
   `kstr' specifies the string of the key.
   If successful, the return value is true.  False is returned when no record corresponds to
   the specified key. */
bool tcmdbout2(TCMDB *mdb, const char *kstr);


/* Retrieve a record in an on-memory database object.
   `mdb' specifies the on-memory database object.
   `kbuf' specifies the pointer to the region of the key.
   `ksiz' specifies the size of the region of the key.
   `sp' specifies the pointer to the variable into which the size of the region of the return
   value is assigned.
   If successful, the return value is the pointer to the region of the value of the
   corresponding record.  `NULL' is returned when no record corresponds.
   Because an additional zero code is appended at the end of the region of the return value,
   the return value can be treated as a character string.  Because the region of the return
   value is allocated with the `malloc' call, it should be released with the `free' call when
   it is no longer in use. */
void *tcmdbget(TCMDB *mdb, const void *kbuf, int ksiz, int *sp);


/* Retrieve a string record in an on-memory database object.
   `mdb' specifies the on-memory database object.
   `kstr' specifies the string of the key.
   If successful, the return value is the string of the value of the corresponding record.
   `NULL' is returned when no record corresponds.
   Because the region of the return value is allocated with the `malloc' call, it should be
   released with the `free' call when it is no longer in use. */
char *tcmdbget2(TCMDB *mdb, const char *kstr);


/* Retrieve a record and move it astern in an on-memory database object.
   `mdb' specifies the on-memory database object.
   `kbuf' specifies the pointer to the region of the key.
   `ksiz' specifies the size of the region of the key.
   `sp' specifies the pointer to the variable into which the size of the region of the return
   value is assigned.
   If successful, the return value is the pointer to the region of the value of the
   corresponding record.  `NULL' is returned when no record corresponds.
   Because an additional zero code is appended at the end of the region of the return value,
   the return value can be treated as a character string.  Because the region of the return value
   is allocated with the `malloc' call, it should be released with the `free' call when it is no
   longer in use.  The internal region of the returned record is moved to the tail so that the
   record will survive for a time under LRU cache algorithm removing records from the head. */
void *tcmdbget3(TCMDB *mdb, const void *kbuf, int ksiz, int *sp);


/* Get the size of the value of a record in an on-memory database object.
   `mdb' specifies the on-memory database object.
   `kbuf' specifies the pointer to the region of the key.
   `ksiz' specifies the size of the region of the key.
   If successful, the return value is the size of the value of the corresponding record, else,
   it is -1. */
int tcmdbvsiz(TCMDB *mdb, const void *kbuf, int ksiz);


/* Get the size of the value of a string record in an on-memory database object.
   `mdb' specifies the on-memory database object.
   `kstr' specifies the string of the key.
   If successful, the return value is the size of the value of the corresponding record, else,
   it is -1. */
int tcmdbvsiz2(TCMDB *mdb, const char *kstr);


/* Initialize the iterator of an on-memory database object.
   `mdb' specifies the on-memory database object.
   The iterator is used in order to access the key of every record stored in the on-memory
   database. */
void tcmdbiterinit(TCMDB *mdb);


/* Get the next key of the iterator of an on-memory database object.
   `mdb' specifies the on-memory database object.
   `sp' specifies the pointer to the variable into which the size of the region of the return
   value is assigned.
   If successful, the return value is the pointer to the region of the next key, else, it is
   `NULL'.  `NULL' is returned when no record can be fetched from the iterator.
   Because an additional zero code is appended at the end of the region of the return value,
   the return value can be treated as a character string.  Because the region of the return
   value is allocated with the `malloc' call, it should be released with the `free' call when
   it is no longer in use.  The order of iteration is assured to be the same as the stored
   order. */
void *tcmdbiternext(TCMDB *mdb, int *sp);


/* Get the next key string of the iterator of an on-memory database object.
   `mdb' specifies the on-memory database object.
   If successful, the return value is the pointer to the region of the next key, else, it is
   `NULL'.  `NULL' is returned when no record can be fetched from the iterator.
   Because the region of the return value is allocated with the `malloc' call, it should be
   released with the `free' call when it is no longer in use.  The order of iteration is assured
   to be the same as the stored order. */
char *tcmdbiternext2(TCMDB *mdb);


/* Get forward matching keys in an on-memory database object.
   `mdb' specifies the on-memory database object.
   `pbuf' specifies the pointer to the region of the prefix.
   `psiz' specifies the size of the region of the prefix.
   `max' specifies the maximum number of keys to be fetched.  If it is negative, no limit is
   specified.
   The return value is a list object of the corresponding keys.  This function does never fail
   and return an empty list even if no key corresponds.
   Because the object of the return value is created with the function `tclistnew', it should be
   deleted with the function `tclistdel' when it is no longer in use.  Note that this function
   may be very slow because every key in the database is scanned. */
TCLIST *tcmdbfwmkeys(TCMDB *mdb, const void *pbuf, int psiz, int max);


/* Get forward matching string keys in an on-memory database object.
   `mdb' specifies the on-memory database object.
   `pstr' specifies the string of the prefix.
   `max' specifies the maximum number of keys to be fetched.  If it is negative, no limit is
   specified.
   The return value is a list object of the corresponding keys.  This function does never fail
   and return an empty list even if no key corresponds.
   Because the object of the return value is created with the function `tclistnew', it should be
   deleted with the function `tclistdel' when it is no longer in use.  Note that this function
   may be very slow because every key in the database is scanned. */
TCLIST *tcmdbfwmkeys2(TCMDB *mdb, const char *pstr, int max);


/* Get the number of records stored in an on-memory database object.
   `mdb' specifies the on-memory database object.
   The return value is the number of the records stored in the database. */
uint64_t tcmdbrnum(TCMDB *mdb);


/* Get the total size of memory used in an on-memory database object.
   `mdb' specifies the on-memory database object.
   The return value is the total size of memory used in the database. */
uint64_t tcmdbmsiz(TCMDB *mdb);


/* Clear an on-memory database object.
   `mdb' specifies the on-memory database object.
   All records are removed. */
void tcmdbvanish(TCMDB *mdb);


/* Remove front records of an on-memory database object.
   `mdb' specifies the on-memory database object.
   `num' specifies the number of records to be removed.
   If successful, the return value is true.  False is returned when no record corresponds to
   the specified key. */
void tcmdbcutfront(TCMDB *mdb, int num);



/*************************************************************************************************
 * memory pool
 *************************************************************************************************/


typedef struct {                         /* type of an element of memory pool */
  void *ptr;                             /* pointer */
  void (*del)(void *);                   /* deleting function */
} TCMPELEM;

typedef struct {                         /* type of structure for a memory pool object */
  void *mutex;                           /* mutex for operations */
  TCMPELEM *elems;                       /* array of elements */
  int anum;                              /* number of the elements of the array */
  int num;                               /* number of used elements */
} TCMPOOL;


/* Create a memory pool object.
   The return value is the new memory pool object. */
TCMPOOL *tcmpoolnew(void);


/* Delete a memory pool object.
   `mpool' specifies the memory pool object.
   Note that the deleted object and its derivatives can not be used anymore. */
void tcmpooldel(TCMPOOL *mpool);


/* Relegate an arbitrary object to a memory pool object.
   `mpool' specifies the memory pool object.
   `ptr' specifies the pointer to the object to be relegated.
   `del' specifies the pointer to the function to delete the object.
   This function assures that the specified object is deleted when the memory pool object is
   deleted. */
void tcmpoolput(TCMPOOL *mpool, void *ptr, void (*del)(void *));


/* Relegate an allocated region to a memory pool object.
   `ptr' specifies the pointer to the region to be relegated.
   This function assures that the specified region is released when the memory pool object is
   deleted. */
void tcmpoolputptr(TCMPOOL *mpool, void *ptr);


/* Relegate an extensible string object to a memory pool object.
   `mpool' specifies the memory pool object.
   `xstr' specifies the extensible string object.
   This function assures that the specified object is deleted when the memory pool object is
   deleted. */
void tcmpoolputxstr(TCMPOOL *mpool, TCXSTR *xstr);


/* Relegate a list object to a memory pool object.
   `mpool' specifies the memory pool object.
   `list' specifies the list object.
   This function assures that the specified object is deleted when the memory pool object is
   deleted. */
void tcmpoolputlist(TCMPOOL *mpool, TCLIST *list);


/* Relegate a map object to a memory pool object.
   `mpool' specifies the memory pool object.
   `map' specifies the map object.
   This function assures that the specified object is deleted when the memory pool object is
   deleted. */
void tcmpoolputmap(TCMPOOL *mpool, TCMAP *map);


/* Allocate a region relegated to a memory pool object.
   `mpool' specifies the memory pool object.
   The return value is the pointer to the allocated region under the memory pool. */
void *tcmpoolmalloc(TCMPOOL *mpool, size_t size);


/* Create an extensible string object relegated to a memory pool object.
   The return value is the new extensible string object under the memory pool. */
TCXSTR *tcmpoolxstrnew(TCMPOOL *mpool);


/* Create a list object relegated to a memory pool object.
   The return value is the new list object under the memory pool. */
TCLIST *tcmpoollistnew(TCMPOOL *mpool);


/* Create a map object relegated to a memory pool object.
   The return value is the new map object under the memory pool. */
TCMAP *tcmpoolmapnew(TCMPOOL *mpool);


/* Get the global memory pool object.
   The return value is the global memory pool object.
   The global memory pool object is a singleton and assured to be deleted when the porcess is
   terminating normally. */
TCMPOOL *tcmpoolglobal(void);



/*************************************************************************************************
 * miscellaneous utilities
 *************************************************************************************************/


/* Get the larger value of two integers.
   `a' specifies an integer.
   `b' specifies the other integer.
   The return value is the larger value of the two. */
long tclmax(long a, long b);


/* Get the lesser value of two integers.
   `a' specifies an integer.
   `b' specifies the other integer.
   The return value is the lesser value of the two. */
long tclmin(long a, long b);


/* Get a random number as long integer based on uniform distribution.
   The return value is the random number between 0 and `ULONG_MAX'.
   This function uses the random number source device and generates a real random number if
   possible. */
unsigned long tclrand(void);


/* Get a random number as double decimal based on uniform distribution.
   The return value is the random number equal to or greater than 0, and less than 1.0.
   This function uses the random number source device and generates a real random number if
   possible. */
double tcdrand(void);


/* Get a random number as double decimal based on normal distribution.
   `avg' specifies the average.
   `sd' specifies the standard deviation.
   The return value is the random number.
   This function uses the random number source device and generates a real random number if
   possible. */
double tcdrandnd(double avg, double sd);


/* Compare two strings with case insensitive evaluation.
   `astr' specifies a string.
   `bstr' specifies of the other string.
   The return value is positive if the former is big, negative if the latter is big, 0 if both
   are equivalent. */
int tcstricmp(const char *astr, const char *bstr);


/* Check whether a string begins with a key.
   `str' specifies the target string.
   `key' specifies the forward matching key string.
   The return value is true if the target string begins with the key, else, it is false. */
bool tcstrfwm(const char *str, const char *key);


/* Check whether a string begins with a key with case insensitive evaluation.
   `str' specifies the target string.
   `key' specifies the forward matching key string.
   The return value is true if the target string begins with the key, else, it is false. */
bool tcstrifwm(const char *str, const char *key);


/* Check whether a string ends with a key.
   `str' specifies the target string.
   `key' specifies the backward matching key string.
   The return value is true if the target string ends with the key, else, it is false. */
bool tcstrbwm(const char *str, const char *key);


/* Check whether a string ends with a key with case insensitive evaluation.
   `str' specifies the target string.
   `key' specifies the backward matching key string.
   The return value is true if the target string ends with the key, else, it is false. */
bool tcstribwm(const char *str, const char *key);


/* Calculate the edit distance of two strings.
   `astr' specifies a string.
   `bstr' specifies of the other string.
   The return value is the edit distance which is known as the Levenshtein distance.  The cost is
   calculated by byte. */
int tcstrdist(const char *astr, const char *bstr);


/* Calculate the edit distance of two UTF-8 strings.
   `astr' specifies a string.
   `bstr' specifies of the other string.
   The return value is the edit distance which is known as the Levenshtein distance.  The cost is
   calculated by Unicode character. */
int tcstrdistutf(const char *astr, const char *bstr);


/* Convert the letters of a string into upper case.
   `str' specifies the string to be converted.
   The return value is the string itself. */
char *tcstrtoupper(char *str);


/* Convert the letters of a string into lower case.
   `str' specifies the string to be converted.
   The return value is the string itself. */
char *tcstrtolower(char *str);


/* Cut space characters at head or tail of a string.
   `str' specifies the string to be converted.
   The return value is the string itself. */
char *tcstrtrim(char *str);


/* Squeeze space characters in a string and trim it.
   `str' specifies the string to be converted.
   The return value is the string itself. */
char *tcstrsqzspc(char *str);


/* Substitute characters in a string.
   `str' specifies the string to be converted.
   `rstr' specifies the string containing characters to be replaced.
   `sstr' specifies the string containing characters to be substituted.
   If the substitute string is shorter then the replacement string, corresponding characters are
   removed. */
char *tcstrsubchr(char *str, const char *rstr, const char *sstr);


/* Count the number of characters in a string of UTF-8.
   `str' specifies the string of UTF-8.
   The return value is the number of characters in the string. */
int tcstrcntutf(const char *str);


/* Cut a string of UTF-8 at the specified number of characters.
   `str' specifies the string of UTF-8.
   `num' specifies the number of characters to be kept.
   The return value is the string itself. */
char *tcstrcututf(char *str, int num);


/* Convert a UTF-8 string into a UCS-2 array.
   `str' specifies the UTF-8 string.
   `ary' specifies the pointer to the region into which the result UCS-2 codes are written.  The
   size of the buffer should be sufficient.
   `np' specifies the pointer to a variable into which the number of elements of the result array
   is assigned. */
void tcstrutftoucs(const char *str, uint16_t *ary, int *np);


/* Convert a UCS-2 array into a UTF-8 string.
   `ary' specifies the array of UCS-2 code codes.
   `num' specifies the number of the array.
   `str' specifies the pointer to the region into which the result UTF-8 string is written.  The
   size of the buffer should be sufficient. */
void tcstrucstoutf(const uint16_t *ary, int num, char *str);


/* Create a list object by splitting a string.
   `str' specifies the source string.
   `delim' specifies a string containing delimiting characters.
   The return value is a list object of the split elements.
   If two delimiters are successive, it is assumed that an empty element is between the two.
   Because the object of the return value is created with the function `tclistnew', it should
   be deleted with the function `tclistdel' when it is no longer in use. */
TCLIST *tcstrsplit(const char *str, const char *delims);


/* Create a string by joining all elements of a list object.
   `list' specifies a list object.
   `delim' specifies a delimiting character.
   The return value is the result string.
   Because the region of the return value is allocated with the `malloc' call, it should be
   released with the `free' call when it is no longer in use. */
char *tcstrjoin(TCLIST *list, char delim);


/* Check whether a string matches a regular expression.
   `str' specifies the target string.
   `regex' specifies the regular expression string.  If it begins with `*', the trailing
   substring is used as a case-insensitive regular expression.
   The return value is true if matching is success, else, it is false. */
bool tcregexmatch(const char *str, const char *regex);


/* Replace each substring matching a regular expression string.
   `str' specifies the target string.
   `regex' specifies the regular expression string for substrings.  If it begins with `*', the
   trailing substring is used as a case-insensitive regular expression.
   `alt' specifies the alternative string with which each substrings is replaced.  Each `&' in
   the string is replaced with the matched substring.  Each `\' in the string escapes the
   following character.  Special escapes "\1" through "\9" referring to the corresponding
   matching sub-expressions in the regular expression string are supported.
   The return value is a new converted string.  Even if the regular expression is invalid, a copy
   of the original string is returned.
   Because the region of the return value is allocated with the `malloc' call, it should be
   released with the `free' call when it is no longer in use. */
char *tcregexreplace(const char *str, const char *regex, const char *alt);


/* Get the time of day in seconds.
   The return value is the time of day in seconds.  The accuracy is in microseconds. */
double tctime(void);


/* Get the Gregorian calendar of a time.
   `t' specifies the source time in seconds from the epoch.  If it is `INT64_MAX', the current
   time is specified.
   `jl' specifies the jet lag of a location in seconds.  If it is `INT_MAX', the local jet lag is
   specified.
   `yearp' specifies the pointer to a variable to which the year is assigned.  If it is `NULL',
   it is not used.
   `monp' specifies the pointer to a variable to which the month is assigned.  If it is `NULL',
   it is not used.  1 means January and 12 means December.
   `dayp' specifies the pointer to a variable to which the day of the month is assigned.  If it
   is `NULL', it is not used.
   `hourp' specifies the pointer to a variable to which the hours is assigned.  If it is `NULL',
   it is not used.
   `minp' specifies the pointer to a variable to which the minutes is assigned.  If it is `NULL',
   it is not used.
   `secp' specifies the pointer to a variable to which the seconds is assigned.  If it is `NULL',
   it is not used. */
void tccalendar(int64_t t, int jl, int *yearp, int *monp, int *dayp,
                int *hourp, int *minp, int *secp);


/* Format a date as a string in W3CDTF.
   `t' specifies the source time in seconds from the epoch.  If it is `INT64_MAX', the current
   time is specified.
   `jl' specifies the jet lag of a location in seconds.  If it is `INT_MAX', the local jet lag is
   specified.
   `buf' specifies the pointer to the region into which the result string is written.  The size
   of the buffer should be equal to or more than 48 bytes.
   W3CDTF represents a date as "YYYY-MM-DDThh:mm:ddTZD". */
void tcdatestrwww(int64_t t, int jl, char *buf);


/* Format a date as a string in RFC 1123 format.
   `t' specifies the source time in seconds from the epoch.  If it is `INT64_MAX', the current
   time is specified.
   `jl' specifies the jet lag of a location in seconds.  If it is `INT_MAX', the local jet lag is
   specified.
   `buf' specifies the pointer to the region into which the result string is written.  The size
   of the buffer should be equal to or more than 48 bytes.
   RFC 1123 format represents a date as "Wdy, DD-Mon-YYYY hh:mm:dd TZD". */
void tcdatestrhttp(int64_t t, int jl, char *buf);


/* Get the time value of a date string.
   `str' specifies the date string in decimal, hexadecimal, W3CDTF, or RFC 822 (1123).  Decimal
   can be trailed by "s" for in seconds, "m" for in minutes, "h" for in hours, and "d" for in
   days.
   The return value is the time value of the date or `INT64_MAX' if the format is invalid. */
int64_t tcstrmktime(const char *str);


/* Get the day of week of a date.
   `year' specifies the year of a date.
   `mon' specifies the month of the date.
   `day' specifies the day of the date.
   The return value is the day of week of the date.  0 means Sunday and 6 means Saturday. */
int tcdayofweek(int year, int mon, int day);



/*************************************************************************************************
 * filesystem utilities
 *************************************************************************************************/


/* Get the canonicalized absolute path of a file.
   `path' specifies the path of the file.
   The return value is the canonicalized absolute path of a file, or `NULL' if the path is
   invalid.
   Because the region of the return value is allocated with the `malloc' call, it should be
   released with the `free' call when it is no longer in use. */
char *tcrealpath(const char *path);


/* Read whole data of a file.
   `path' specifies the path of the file.  If it is `NULL', the standard input is specified.
   `limit' specifies the limiting size of reading data.  If it is not more than 0, the limitation
   is not specified.
   `sp' specifies the pointer to the variable into which the size of the region of the return
   value is assigned.  If it is `NULL', it is not used.
   The return value is the pointer to the allocated region of the read data, or `NULL' if the
   file could not be opened.
   Because an additional zero code is appended at the end of the region of the return value, the
   return value can be treated as a character string.  Because the region of the return value is
   allocated with the `malloc' call, it should be released with the `free' call when when is no
   longer in use.  */
void *tcreadfile(const char *path, int limit, int *sp);


/* Read every line of a file.
   `path' specifies the path of the file.  If it is `NULL', the standard input is specified.
   The return value is a list object of every lines if successful, else it is `NULL'.
   Line separators are cut out.  Because the object of the return value is created with the
   function `tclistnew', it should be deleted with the function `tclistdel' when it is no longer
   in use. */
TCLIST *tcreadfilelines(const char *path);


/* Write data into a file.
   `path' specifies the path of the file.  If it is `NULL', the standard output is specified.
   `ptr' specifies the pointer to the data region.
   `size' specifies the size of the region.
   If successful, the return value is true, else, it is false. */
bool tcwritefile(const char *path, const void *ptr, int size);


/* Copy a file.
   `src' specifies the path of the source file.
   `dest' specifies the path of the destination file.
   The return value is true if successful, else, it is false.
   If the destination file exists, it is overwritten. */
bool tccopyfile(const char *src, const char *dest);


/* Read names of files in a directory.
   `path' specifies the path of the directory.
   The return value is a list object of names if successful, else it is `NULL'.
   Links to the directory itself and to the parent directory are ignored.
   Because the object of the return value is created with the function `tclistnew', it should
   be deleted with the function `tclistdel' when it is no longer in use. */
TCLIST *tcreaddir(const char *path);


/* Expand a pattern into a list of matched paths.
   `pattern' specifies the matching pattern.
   The return value is a list object of matched paths.  If no path is matched, an empty list is
   returned.
   Because the object of the return value is created with the function `tclistnew', it should
   be deleted with the function `tclistdel' when it is no longer in use. */
TCLIST *tcglobpat(const char *pattern);


/* Remove a file or a directory and its sub ones recursively.
   `path' specifies the path of the link.
   If successful, the return value is true, else, it is false.  False is returned when the link
   does not exist or the permission is denied. */
bool tcremovelink(const char *path);


/* Write data into a file.
   `fd' specifies the file descriptor.
   `buf' specifies the buffer to be written.
   `size' specifies the size of the buffer.
   The return value is true if successful, else, it is false. */
bool tcwrite(int fd, const void *buf, size_t size);


/* Read data from a file.
   `fd' specifies the file descriptor.
   `buf' specifies the buffer to store into.
   `size' specifies the size of the buffer.
   The return value is true if successful, else, it is false. */
bool tcread(int fd, void *buf, size_t size);


/* Lock a file.
   `fd' specifies the file descriptor.
   `ex' specifies whether an exclusive lock or a shared lock is performed.
   `nb' specifies whether to request with non-blocking.
   The return value is true if successful, else, it is false. */
bool tclock(int fd, bool ex, bool nb);



/*************************************************************************************************
 * encoding utilities
 *************************************************************************************************/


/* Encode a serial object with URL encoding.
   `ptr' specifies the pointer to the region.
   `size' specifies the size of the region.
   The return value is the result string.
   Because the region of the return value is allocated with the `malloc' call, it should be
   released with the `free' call if when is no longer in use. */
char *tcurlencode(const char *ptr, int size);


/* Decode a string encoded with URL encoding.
   `str' specifies the encoded string.
   `sp' specifies the pointer to a variable into which the size of the region of the return
   value is assigned.
   The return value is the pointer to the region of the result.
   Because an additional zero code is appended at the end of the region of the return value,
   the return value can be treated as a character string.  Because the region of the return
   value is allocated with the `malloc' call, it should be released with the `free' call when
   it is no longer in use. */
char *tcurldecode(const char *str, int *sp);


/* Break up a URL into elements.
   `str' specifies the URL string.
   The return value is the map handle whose keys are the name of elements.  The key "self"
   specifies the URL itself.  The key "scheme" specifies the scheme.  The key "host" specifies
   the host of the server.  The key "port" specifies the port number of the server.  The key
   "authority" specifies the authority information.  The key "path" specifies the path of the
   resource.  The key "file" specifies the file name without the directory section.  The key
   "query" specifies the query string.  The key "fragment" specifies the fragment string.
   Supported schema are HTTP, HTTPS, FTP, and FILE.  Absolute URL and relative URL are supported.
   Because the object of the return value is created with the function `tcmapnew', it should be
   deleted with the function `tcmapdel' when it is no longer in use. */
TCMAP *tcurlbreak(const char *str);


/* Resolve a relative URL with an absolute URL.
   `base' specifies the absolute URL of the base location.
   `target' specifies the URL to be resolved.
   The return value is the resolved URL.  If the target URL is relative, a new URL of relative
   location from the base location is returned.  Else, a copy of the target URL is returned.
   Because the region of the return value is allocated with the `malloc' call, it should be
   released with the `free' call when it is no longer in use. */
char *tcurlresolve(const char *base, const char *target);


/* Encode a serial object with Base64 encoding.
   `ptr' specifies the pointer to the region.
   `size' specifies the size of the region.
   The return value is the result string.
   Because the region of the return value is allocated with the `malloc' call, it should be
   released with the `free' call if when is no longer in use. */
char *tcbaseencode(const char *ptr, int size);


/* Decode a string encoded with Base64 encoding.
   `str' specifies the encoded string.
   `sp' specifies the pointer to a variable into which the size of the region of the return
   value is assigned.
   The return value is the pointer to the region of the result.
   Because an additional zero code is appended at the end of the region of the return value,
   the return value can be treated as a character string.  Because the region of the return
   value is allocated with the `malloc' call, it should be released with the `free' call when
   it is no longer in use. */
char *tcbasedecode(const char *str, int *sp);


/* Encode a serial object with Quoted-printable encoding.
   `ptr' specifies the pointer to the region.
   `size' specifies the size of the region.
   The return value is the result string.
   Because the region of the return value is allocated with the `malloc' call, it should be
   released with the `free' call if when is no longer in use. */
char *tcquoteencode(const char *ptr, int size);


/* Decode a string encoded with Quoted-printable encoding.
   `str' specifies the encoded string.
   `sp' specifies the pointer to a variable into which the size of the region of the return
   value is assigned.
   The return value is the pointer to the region of the result.
   Because an additional zero code is appended at the end of the region of the return value,
   the return value can be treated as a character string.  Because the region of the return
   value is allocated with the `malloc' call, it should be released with the `free' call when
   it is no longer in use. */
char *tcquotedecode(const char *str, int *sp);


/* Encode a string with MIME encoding.
   `str' specifies the string.
   `encname' specifies the string of the name of the character encoding.
   `base' specifies whether to use Base64 encoding.  If it is false, Quoted-printable is used.
   The return value is the result string.
   Because the region of the return value is allocated with the `malloc' call, it should be
   released with the `free' call when it is no longer in use. */
char *tcmimeencode(const char *str, const char *encname, bool base);


/* Decode a string encoded with MIME encoding.
   `str' specifies the encoded string.
   `enp' specifies the pointer to the region into which the name of encoding is written.  If it
   is `NULL', it is not used.  The size of the buffer should be equal to or more than 32 bytes.
   The return value is the result string.
   Because the region of the return value is allocated with the `malloc' call, it should be
   released with the `free' call when it is no longer in use. */
char *tcmimedecode(const char *str, char *enp);


/* Compress a serial object with Packbits encoding.
   `ptr' specifies the pointer to the region.
   `size' specifies the size of the region.
   `sp' specifies the pointer to the variable into which the size of the region of the return
   value is assigned.
   If successful, the return value is the pointer to the result object, else, it is `NULL'.
   Because the region of the return value is allocated with the `malloc' call, it should be
   released with the `free' call when it is no longer in use. */
char *tcpackencode(const char *ptr, int size, int *sp);


/* Decompress a serial object compressed with Packbits encoding.
   `ptr' specifies the pointer to the region.
   `size' specifies the size of the region.
   `sp' specifies the pointer to a variable into which the size of the region of the return
   value is assigned.
   If successful, the return value is the pointer to the result object, else, it is `NULL'.
   Because an additional zero code is appended at the end of the region of the return value,
   the return value can be treated as a character string.  Because the region of the return
   value is allocated with the `malloc' call, it should be released with the `free' call when it
   is no longer in use. */
char *tcpackdecode(const char *ptr, int size, int *sp);


/* Compress a serial object with TCBS encoding.
   `ptr' specifies the pointer to the region.
   `size' specifies the size of the region.
   `sp' specifies the pointer to the variable into which the size of the region of the return
   value is assigned.
   If successful, the return value is the pointer to the result object, else, it is `NULL'.
   Because the region of the return value is allocated with the `malloc' call, it should be
   released with the `free' call when it is no longer in use. */
char *tcbsencode(const char *ptr, int size, int *sp);


/* Decompress a serial object compressed with TCBS encoding.
   `ptr' specifies the pointer to the region.
   `size' specifies the size of the region.
   `sp' specifies the pointer to a variable into which the size of the region of the return
   value is assigned.
   If successful, the return value is the pointer to the result object, else, it is `NULL'.
   Because an additional zero code is appended at the end of the region of the return value,
   the return value can be treated as a character string.  Because the region of the return
   value is allocated with the `malloc' call, it should be released with the `free' call when it
   is no longer in use. */
char *tcbsdecode(const char *ptr, int size, int *sp);


/* Compress a serial object with Deflate encoding.
   `ptr' specifies the pointer to the region.
   `size' specifies the size of the region.
   `sp' specifies the pointer to the variable into which the size of the region of the return
   value is assigned.
   If successful, the return value is the pointer to the result object, else, it is `NULL'.
   Because the region of the return value is allocated with the `malloc' call, it should be
   released with the `free' call when it is no longer in use. */
char *tcdeflate(const char *ptr, int size, int *sp);


/* Decompress a serial object compressed with Deflate encoding.
   `ptr' specifies the pointer to the region.
   `size' specifies the size of the region.
   `sp' specifies the pointer to a variable into which the size of the region of the return
   value is assigned.
   If successful, the return value is the pointer to the result object, else, it is `NULL'.
   Because an additional zero code is appended at the end of the region of the return value,
   the return value can be treated as a character string.  Because the region of the return
   value is allocated with the `malloc' call, it should be released with the `free' call when it
   is no longer in use. */
char *tcinflate(const char *ptr, int size, int *sp);


/* Compress a serial object with GZIP encoding.
   `ptr' specifies the pointer to the region.
   `size' specifies the size of the region.
   `sp' specifies the pointer to the variable into which the size of the region of the return
   value is assigned.
   If successful, the return value is the pointer to the result object, else, it is `NULL'.
   Because the region of the return value is allocated with the `malloc' call, it should be
   released with the `free' call when it is no longer in use. */
char *tcgzipencode(const char *ptr, int size, int *sp);


/* Decompress a serial object compressed with GZIP encoding.
   `ptr' specifies the pointer to the region.
   `size' specifies the size of the region.
   `sp' specifies the pointer to a variable into which the size of the region of the return
   value is assigned.
   If successful, the return value is the pointer to the result object, else, it is `NULL'.
   Because an additional zero code is appended at the end of the region of the return value,
   the return value can be treated as a character string.  Because the region of the return
   value is allocated with the `malloc' call, it should be released with the `free' call when it
   is no longer in use. */
char *tcgzipdecode(const char *ptr, int size, int *sp);


/* Get the CRC32 checksum of a serial object.
   `ptr' specifies the pointer to the region.
   `size' specifies the size of the region.
   The return value is the CRC32 checksum of the object. */
unsigned int tcgetcrc(const char *ptr, int size);


/* Encode an array of nonnegative integers with BER encoding.
   `ary' specifies the pointer to the array of nonnegative integers.
   `anum' specifies the size of the array.
   `sp' specifies the pointer to a variable into which the size of the region of the return
   value is assigned.
   The return value is the pointer to the region of the result.
   Because the region of the return value is allocated with the `malloc' call, it should be
   released with the `free' call if when is no longer in use. */
char *tcberencode(const unsigned int *ary, int anum, int *sp);


/* Decode a serial object encoded with BER encoding.
   `ptr' specifies the pointer to the region.
   `size' specifies the size of the region.
   `np' specifies the pointer to a variable into which the number of elements of the return value
   is assigned.
   The return value is the pointer to the array of the result.
   Because the region of the return value is allocated with the `malloc' call, it should be
   released with the `free' call if when is no longer in use. */
unsigned int *tcberdecode(const char *ptr, int size, int *np);


/* Escape meta characters in a string with the entity references of XML.
   `str' specifies the string.
   The return value is the pointer to the escaped string.
   This function escapes only `&', `<', `>', and `"'.  Because the region of the return value
   is allocated with the `malloc' call, it should be released with the `free' call when it is no
   longer in use. */
char *tcxmlescape(const char *str);


/* Unescape entity references in a string of XML.
   `str' specifies the string.
   The return value is the unescaped string.
   This function restores only `&amp;', `&lt;', `&gt;', and `&quot;'.  Because the region of the
   return value is allocated with the `malloc' call, it should be released with the `free' call
   when it is no longer in use. */
char *tcxmlunescape(const char *str);


/* Split an XML string into tags and text sections.
   `str' specifies the XML string.
   The return value is the list object whose elements are strings of tags or text sections.
   Because the object of the return value is created with the function `tclistnew', it should
   be deleted with the function `tclistdel' when it is no longer in use.  Because this function
   does not check validation, it can handle also HTML and SGML. */
TCLIST *tcxmlbreak(const char *str);


/* Get the map of attributes of an XML tag.
   `str' specifies the pointer to the region of a tag string.
   The return value is the map object containing attribute names and their values which are
   unescaped.  You can get the name of the tag with the key of an empty string.
   Because the object of the return value is created with the function `tcmapnew', it should
   be deleted with the function `tcmapdel' when it is no longer in use. */
TCMAP *tcxmlattrs(const char *str);



/*************************************************************************************************
 * bit operation utilities
 *************************************************************************************************/


typedef struct {                         /* type of structure for a bit stream object */
  uint8_t *sp;                           /* start pointer */
  uint8_t *cp;                           /* current pointer */
  int idx;                               /* bit index */
  int size;                              /* size of used region */
} TCBITSTRM;


/* Create a bitmap object. */
#define TCBITMAPNEW(TC_num) \
  tccalloc(((TC_num) >> 3) + 1, 1);


/* Delete a bitmap object */
#define TCBITMAPDEL(TC_bitmap) \
  do { \
    free((TC_bitmap)); \
  } while(false);


/* Turn on a field of a bitmap object. */
#define TCBITMAPON(TC_bitmap, TC_idx) \
  do { \
    (TC_bitmap)[(TC_idx)>>3] |= 0x1 << ((TC_idx) & 0x7); \
  } while(false);


/* Turn off a field of a bitmap object. */
#define TCBITMAPOFF(TC_bitmap, TC_idx) \
  do { \
    (TC_bitmap)[(TC_idx)>>3] &= ~(0x1 << ((TC_idx) & 0x7)); \
  } while(false);


/* Check a field of a bitmap object. */
#define TCBITMAPCHECK(TC_bitmap, TC_idx) \
  ((TC_bitmap)[(TC_idx)>>3] & 0x1 << ((TC_idx) & 0x7))


/* Initialize a bit stream object as writer. */
#define TCBITSTRMINITW(TC_bitstrm, TC_ptr) \
  do { \
    (TC_bitstrm).sp = (uint8_t *)(TC_ptr); \
    (TC_bitstrm).cp = (TC_bitstrm).sp; \
    *(TC_bitstrm).cp = 0; \
    (TC_bitstrm).idx = 3; \
    (TC_bitstrm).size = 1; \
  } while(false);


/* Concatenate a bit to a bit stream object. */
#define TCBITSTRMCAT(TC_bitstrm, sign) \
  do { \
    if((TC_bitstrm).idx >= 8){ \
      *(++(TC_bitstrm).cp) = 0; \
      (TC_bitstrm).idx = 0; \
      (TC_bitstrm).size++; \
    } \
    *(TC_bitstrm).cp |= (sign << (TC_bitstrm).idx); \
    (TC_bitstrm).idx++; \
  } while(false);


/* Set the end mark to a bit stream object. */
#define TCBITSTRMSETEND(TC_bitstrm) \
  do { \
    if((TC_bitstrm).idx >= 8){ \
      *(++(TC_bitstrm).cp) = 0; \
      (TC_bitstrm).idx = 0; \
      (TC_bitstrm).size++; \
    } \
    *(TC_bitstrm).sp |= (TC_bitstrm).idx & 7; \
  } while(false);


/* Get the size of the used region of a bit stream object. */
#define TCBITSTRMSIZE(TC_bitstrm) \
  ((TC_bitstrm).size)


/* Initialize a bit stream object as reader. */
#define TCBITSTRMINITR(TC_bitstrm, TC_ptr, TC_size) \
  do { \
    (TC_bitstrm).sp = (uint8_t *)(TC_ptr); \
    (TC_bitstrm).cp = (TC_bitstrm).sp; \
    (TC_bitstrm).idx = 3; \
    (TC_bitstrm).size = (TC_size); \
  } while(false);


/* Read a bit from a bit stream object. */
#define TCBITSTRMREAD(TC_bitstrm, TC_sign) \
  do { \
    if((TC_bitstrm).idx >= 8){ \
      (TC_bitstrm).cp++; \
      (TC_bitstrm).idx = 0; \
    } \
    (TC_sign) = (*((TC_bitstrm).cp) & (1 << (TC_bitstrm).idx)) > 0; \
    (TC_bitstrm).idx++; \
  } while(false);


/* Get the number of bits of a bit stream object. */
#define TCBITSTRMNUM(TC_bitstrm) \
  ((((TC_bitstrm).size - 1) << 3) + (*(TC_bitstrm).sp & 7) - 3)



/*************************************************************************************************
 * features for experts
 *************************************************************************************************/


#include <stdio.h>

#define _TC_VERSION    "1.2.5"
#define _TC_LIBVER     306
#define _TC_FORMATVER  "1.0"


/* Show error message on the standard error output and exit.
   `message' specifies an error message.
   This function does not return. */
void *tcmyfatal(const char *message);


/* Lock the global mutex object.
   If successful, the return value is true, else, it is false. */
bool tcglobalmutexlock(void);


/* Lock the global mutex object by shared locking.
   If successful, the return value is true, else, it is false. */
bool tcglobalmutexlockshared(void);


/* Unlock the global mutex object.
   If successful, the return value is true, else, it is false. */
bool tcglobalmutexunlock(void);


/* Encode a serial object with BWT encoding.
   `ptr' specifies the pointer to the region.
   `size' specifies the size of the region.
   `idxp' specifies the pointer to the variable into which the index of the original string in
   the rotation array is assigned.
   The return value is the pointer to the result object.
   Because an additional zero code is appended at the end of the region of the return value,
   the return value can be treated as a character string.  Because the region of the return
   value is allocated with the `malloc' call, it should be released with the `free' call when it
   is no longer in use. */
char *tcbwtencode(const char *ptr, int size, int *idxp);


/* Decode a serial object encoded with BWT encoding.
   `ptr' specifies the pointer to the region.
   `size' specifies the size of the region.
   `idx' specifies the index of the original string in the rotation array is assigned.
   The return value is the pointer to the result object.
   Because an additional zero code is appended at the end of the region of the return value,
   the return value can be treated as a character string.  Because the region of the return
   value is allocated with the `malloc' call, it should be released with the `free' call when it
   is no longer in use. */
char *tcbwtdecode(const char *ptr, int size, int idx);


/* Print debug information with a formatted string as with `printf'. */
#if __STDC_VERSION__ >= 199901L
#define TCDPRINTF(...) \
  do { \
    fprintf(stderr, "%s:%d:%s: ", __FILE__, __LINE__, __func__); \
    fprintf(stderr, __VA_ARGS__); \
    fprintf(stderr, "\n"); \
  } while(false);
#else
#define TCDPRINTF(TC_str) \
  do { \
    fprintf(stderr, "%s:%d:%s: %s\n", __FILE__, __LINE__, __func__, TC_str); \
  } while(false);
#endif


/* Print hexadecimal pattern of a binary region. */
#define TCPRINTHEX(TC_ptr, TC_size) \
  do { \
    for(int TC_i = 0; TC_i < (TC_size); TC_i++){ \
      if(TC_i > 0) putchar(' '); \
      printf("%02X", ((unsigned char *)(TC_ptr))[TC_i]); \
    } \
    putchar('\n'); \
  } while(false);


/* Print an extensible string object. */
#define TCPRINTXSTR(TC_xstr) \
  do { \
    fwrite(tcxstrptr((TC_xstr)), tcxstrsize((TC_xstr)), 1, stdout); \
    putchar('\n'); \
  } while(false);


/* Print all elements of a list object. */
#define TCPRINTLIST(TC_list) \
  do { \
    for(int TC_i = 0; TC_i < tclistnum((TC_list)); TC_i++){ \
      int TC_size; \
      const char *TC_ptr = tclistval((TC_list), TC_i, &TC_size); \
      printf("%p\t", (void *)(TC_list)); \
      fwrite(TC_ptr, TC_size, 1, stdout); \
      putchar('\n'); \
    } \
    putchar('\n'); \
  } while(false);


/* Print all records of a list object. */
#define TCPRINTMAP(TC_map) \
  do { \
    TCLIST *TC_keys = tcmapkeys((TC_map)); \
    for(int TC_i = 0; TC_i < tclistnum(TC_keys); TC_i++){ \
      int TC_ksiz; \
      const char *TC_kbuf = tclistval(TC_keys, TC_i, &TC_ksiz); \
      int TC_vsiz; \
      const char *TC_vbuf = tcmapget((TC_map), TC_kbuf, TC_ksiz, &TC_vsiz); \
      printf("%p\t", (void *)(TC_map)); \
      fwrite(TC_kbuf, TC_ksiz, 1, stdout); \
      putchar('\t'); \
      fwrite(TC_vbuf, TC_vsiz, 1, stdout); \
      putchar('\n'); \
    } \
    putchar('\n'); \
    tclistdel(TC_keys); \
  } while(false);


/* Alias of `tcmemdup'. */
#if defined(_MYFASTEST)
#define TCMALLOC(TC_res, TC_size) \
  do { \
    (TC_res) = malloc(TC_size); \
  } while(false)
#else
#define TCMALLOC(TC_res, TC_size) \
  do { \
    if(!((TC_res) = malloc(TC_size))) tcmyfatal("out of memory"); \
  } while(false)
#endif

/* Alias of `tccalloc'. */
#if defined(_MYFASTEST)
#define TCCALLOC(TC_res, TC_nmemb, TC_size) \
  do { \
    (TC_res) = calloc((TC_nmemb), (TC_size)); \
  } while(false)
#else
#define TCCALLOC(TC_res, TC_nmemb, TC_size) \
  do { \
    if(!((TC_res) = calloc((TC_nmemb), (TC_size)))) tcmyfatal("out of memory"); \
  } while(false)
#endif

/* Alias of `tcrealloc'. */
#if defined(_MYFASTEST)
#define TCREALLOC(TC_res, TC_ptr, TC_size) \
  do { \
    (TC_res) = realloc((TC_ptr), (TC_size)); \
  } while(false)
#else
#define TCREALLOC(TC_res, TC_ptr, TC_size) \
  do { \
    if(!((TC_res) = realloc((TC_ptr), (TC_size)))) tcmyfatal("out of memory"); \
  } while(false)
#endif

/* Alias of `tcmemdup'. */
#define TCMEMDUP(TC_res, TC_ptr, TC_size) \
  do { \
    TCMALLOC((TC_res), (TC_size) + 1);   \
    memcpy((TC_res), (TC_ptr), (TC_size));      \
    (TC_res)[TC_size] = '\0'; \
  } while(false)


/* Alias of `tcxstrcat'. */
#define TCXSTRCAT(TC_xstr, TC_ptr, TC_size) \
  do { \
    int TC_mysize = (TC_size); \
    int TC_nsize = (TC_xstr)->size + TC_mysize + 1; \
    if((TC_xstr)->asize < TC_nsize){ \
      while((TC_xstr)->asize < TC_nsize){ \
        (TC_xstr)->asize *= 2; \
        if((TC_xstr)->asize < TC_nsize) (TC_xstr)->asize = TC_nsize; \
      } \
      TCREALLOC((TC_xstr)->ptr, (TC_xstr)->ptr, (TC_xstr)->asize); \
    } \
    memcpy((TC_xstr)->ptr + (TC_xstr)->size, (TC_ptr), TC_mysize); \
    (TC_xstr)->size += TC_mysize; \
    (TC_xstr)->ptr[(TC_xstr)->size] = '\0'; \
  } while(false)


/* Alias of `tcxstrptr'. */
#define TCXSTRPTR(TC_xstr) \
  ((TC_xstr)->ptr)


/* Alias of `tcxstrsize'. */
#define TCXSTRSIZE(TC_xstr) \
  ((TC_xstr)->size)


/* Alias of `tclistnum'. */
#define TCLISTNUM(TC_list) \
  ((TC_list)->num)


/* Alias of `tclistval' but not checking size and not using the third parameter. */
#define TCLISTVALPTR(TC_list, TC_index) \
  ((void *)((TC_list)->array[(TC_index)+(TC_list)->start].ptr))


/* Alias of `tclistval' but not checking size and returning the size of the value. */
#define TCLISTVALNUM(TC_list, TC_index) \
  ((TC_list)->array[(TC_index)+(TC_list)->start].size)


/* Add an element at the end of a list object. */
#define TCLISTPUSH(TC_list, TC_ptr, TC_size) \
  do { \
    int TC_mysize = (TC_size); \
    int TC_index = (TC_list)->start + (TC_list)->num; \
    if(TC_index >= (TC_list)->anum){ \
      (TC_list)->anum += (TC_list)->num + 1; \
      TCREALLOC((TC_list)->array, (TC_list)->array, \
                (TC_list)->anum * sizeof((TC_list)->array[0])); \
    } \
    TCLISTDATUM *array = (TC_list)->array; \
    TCMALLOC(array[TC_index].ptr, TC_mysize + 1);     \
    memcpy(array[TC_index].ptr, (TC_ptr), TC_mysize); \
    array[TC_index].ptr[TC_mysize] = '\0'; \
    array[TC_index].size = TC_mysize; \
    (TC_list)->num++; \
  } while(false)


/* Alias of `tcmaprnum'. */
#define TCMAPRNUM(TC_map) \
  ((TC_map)->rnum)



__TCUTIL_CLINKAGEEND
#endif                                   /* duplication check */


/* END OF FILE */
