/* */
#undef  VERSION
#define VERSION "1.37.4"
#define BDATE   "18 February 2005"
#define LSMDATE "18Feb05"

/* Debug flags */
#undef  DEBUG
#define DEBUG 1
#define TRACEBACK 1
#define SMCHECK
#define TRACE_FILE 1

/* If this is set stdout will not be closed on startup */
#define DEVELOPER 1



/* Debug flags not normally turned on */

/* #define TRACE_JCR_CHAIN 1 */
/* #define TRACE_RES 1 */
/* #define DEBUG_MEMSET 1 */

/* Check if header of tape block is zero before writing */
#define DEBUG_BLOCK_ZEROING 1

/* #define FULL_DEBUG 1 */   /* normally on for testing only */

/* Turn this on ONLY if you want all Dmsg() to append to the
 *   trace file. Implemented mainly for Win32 ...
 */
/*  #define SEND_DMSG_TO_FILE 1 */


/* #define NO_ATTRIBUTES_TEST 1 */
/* #define NO_TAPE_WRITE_TEST 1 */
/* #define FD_NO_SEND TEST 1 */
/* #define DEBUG_MUTEX 1 */
