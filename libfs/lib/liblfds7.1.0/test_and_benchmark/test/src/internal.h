/***** includes *****/
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include "../../../liblfds710/inc/liblfds710.h"
#include "../../libtest/inc/libtest.h"
#include "util_cmdline.h"

/***** defines *****/
#define and &&
#define or  ||

#define BITS_PER_BYTE 8

#define NO_FLAGS 0x0

#define TEST_DEFAULT_TEST_MEMORY_IN_MEGABYTES  512
#define ONE_MEGABYTE_IN_BYTES                  (1024 * 1024)

#define TEST_VERSION_STRING   "7.1.0"
#define TEST_VERSION_INTEGER  710

#if( defined KERNEL_MODE )
  #define MODE_TYPE_STRING "kernel-mode"
#endif

#if( !defined KERNEL_MODE )
  #define MODE_TYPE_STRING "user-mode"
#endif

#if( defined NDEBUG && !defined COVERAGE && !defined TSAN && !defined PROF )
  #define BUILD_TYPE_STRING "release"
#endif

#if( !defined NDEBUG && !defined COVERAGE && !defined TSAN && !defined PROF )
  #define BUILD_TYPE_STRING "debug"
#endif

#if( !defined NDEBUG && defined COVERAGE && !defined TSAN && !defined PROF )
  #define BUILD_TYPE_STRING "coverage"
#endif

#if( !defined NDEBUG && !defined COVERAGE && defined TSAN && !defined PROF )
  #define BUILD_TYPE_STRING "threadsanitizer"
#endif

#if( !defined NDEBUG && !defined COVERAGE && !defined TSAN && defined PROF )
  #define BUILD_TYPE_STRING "profiling"
#endif

/***** enums *****/

/***** structs *****/

/***** externs *****/

/***** prototypes *****/
int main( int argc, char **argv );

void callback_test_start( char *test_name );
void callback_test_finish( char *result );

void internal_show_version();
void internal_logical_core_id_element_cleanup_callback( struct lfds710_list_asu_state *lasus, struct lfds710_list_asu_element *lasue );


