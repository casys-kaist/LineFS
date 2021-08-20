/***** includes *****/
#include "porting_abstraction_layer.h"
#include "porting_abstraction_layer_operating_system.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "../../../liblfds710/inc/liblfds710.h"
#include "../../libbenchmark/inc/libbenchmark.h"
#include "util_cmdline.h"

/***** defines *****/
#define and &&
#define or  ||

#define BITS_PER_BYTE 8

#define NO_FLAGS 0x0

#define BENCHMARK_VERSION_STRING   "7.1.0"
#define BENCHMARK_VERSION_INTEGER  710

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

#define ONE_KILOBYTES_IN_BYTES                 1024
#define ONE_MEGABYTE_IN_BYTES                  (ONE_KILOBYTES_IN_BYTES * 1024)
#define BENCHMARK_DEFAULT_MEMORY_IN_MEGABYTES  64

/***** enums *****/

/***** structs *****/

/***** externs *****/

/***** prototypes *****/
int main( int argc, char **argv );
void internal_show_version( void );
void callback_stdout( char *string );

void *benchmark_pal_numa_malloc( lfds710_pal_uint_t numa_node_id, lfds710_pal_uint_t size_in_bytes );
void benchmark_pal_numa_free( void *memory, lfds710_pal_uint_t size_in_bytes );

