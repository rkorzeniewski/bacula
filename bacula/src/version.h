/* */
#undef  VERSION
#define VERSION "1.35.0"
#define VSTRING "1"
#define BDATE   "16 June 2004"
#define LSMDATE "16Jun04"

/* Debug flags */
#undef  DEBUG
#define DEBUG 1
#define TRACEBACK 1
#define SMCHECK     
#define TRACE_FILE 1  

/* #define FULL_DEBUG 1 */   /* normally on for testing only */

/* Turn this on ONLY if you want all Dmsg() to append to the
 *   trace file. Implemented mainly for Win32 ...
 */
/*  #define SEND_DMSG_TO_FILE 1 */


/* #define NO_ATTRIBUTES_TEST 1 */
/* #define NO_TAPE_WRITE_TEST 1 */
/* #define FD_NO_SEND TEST 1 */
/* #define DEBUG_MUTEX 1 */
