/***** public prototypes *****/
#include "../inc/libbenchmark.h"

/***** defines *****/
#define and &&
#define or  ||

#define NO_FLAGS 0x0

#define NUMBER_UPPERCASE_LETTERS_IN_LATIN_ALPHABET  26
#define NUMBER_OF_NANOSECONDS_IN_ONE_SECOND         1000000000LLU
#define TIME_LOOP_COUNT                             1000
#define DEFAULT_BENCHMARK_DURATION_IN_SECONDS       5

#define ONE_KILOBYTES_IN_BYTES                      1024

#define LIBBENCHMARK_VERSION_STRING   "7.1.0"
#define LIBBENCHMARK_VERSION_INTEGER  710

#define RETURN_SUCCESS 0
#define RETURN_FAILURE 1

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

/***** library-wide prototypes *****/

