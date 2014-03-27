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
/**
 * Assertion definitions
 *
 */


#ifndef _ASSERT_MACRO_H
#define _ASSERT_MACRO_H 1

/* Assertions definitions */

/* check valid pointer if not return */
#ifndef ASSERT_NVAL_RET
#define ASSERT_NVAL_RET(value) \
   if ( ! value ){ \
      return; \
   }
#endif

/* check an error if true return */
#ifndef ASSERT_VAL_RET
#define ASSERT_VAL_RET(value) \
   if ( value ){ \
      return; \
   }
#endif

/* check valid pointer with Null return */
#ifndef ASSERT_NVAL_RET_NULL
#define ASSERT_NVAL_RET_NULL(value) \
   if ( ! value ) \
   { \
      return NULL; \
   }
#endif

/* if value then Null return */
#ifndef ASSERT_VAL_RET_NULL
#define ASSERT_VAL_RET_NULL(value) \
   if ( value ) \
   { \
      return NULL; \
   }
#endif

/* check valid pointer with int/err return */
#ifndef ASSERT_NVAL_RET_ONE
#define ASSERT_NVAL_RET_ONE(value) \
   if ( ! value ) \
   { \
      return 1; \
   }
#endif

/* check valid pointer with int/err return */
#ifndef ASSERT_NVAL_RET_NONE
#define ASSERT_NVAL_RET_NONE(value) \
   if ( ! value ) \
   { \
      return -1; \
   }
#endif

/* check error if not exit with error */
#ifndef ASSERT_NVAL_EXIT_ONE
#define ASSERT_NVAL_EXIT_ONE(value) \
   if ( ! value ){ \
      exit ( 1 ); \
   }
#endif

/* check error if not exit with error */
#ifndef ASSERT_NVAL_EXIT_E
#define ASSERT_NVAL_EXIT_E(value,ev) \
   if ( ! value ){ \
      exit ( ev ); \
   }
#endif

/* check error if not return zero */
#ifndef ASSERT_NVAL_RET_ZERO
#define ASSERT_NVAL_RET_ZERO(value) \
   if ( ! value ){ \
      return 0; \
   }
#endif

/* check error if not return value */
#ifndef ASSERT_NVAL_RET_V
#define ASSERT_NVAL_RET_V(value,rv) \
   if ( ! value ){ \
      return rv; \
   }
#endif

/* checks error value then int/err return */
#ifndef ASSERT_VAL_RET_ONE
#define ASSERT_VAL_RET_ONE(value) \
   if ( value ) \
   { \
      return 1; \
   }
#endif

/* checks error value then int/err return */
#ifndef ASSERT_VAL_RET_NONE
#define ASSERT_VAL_RET_NONE(value) \
   if ( value ) \
   { \
      return -1; \
   }
#endif

/* checks error value then exit one */
#ifndef ASSERT_VAL_EXIT_ONE
#define ASSERT_VAL_EXIT_ONE(value) \
   if ( value ) \
   { \
      exit (1); \
   }
#endif

/* check error if not return zero */
#ifndef ASSERT_VAL_RET_ZERO
#define ASSERT_VAL_RET_ZERO(value) \
   if ( value ){ \
      return 0; \
   }
#endif

/* check error if not return value */
#ifndef ASSERT_VAL_RET_V
#define ASSERT_VAL_RET_V(value,rv) \
   if ( value ){ \
      return rv; \
   }
#endif

#endif /* _ASSERT_MACRO_H */
