/****************************************************************************/
#if( defined _WIN32 && !defined KERNEL_MODE && NTDDI_VERSION >= NTDDI_WINXP )

  #ifdef LIBBENCHMARK_PAL_OPERATING_SYSTEM
    #error More than one porting abstraction layer matches current platform. in "libbenchmark_porting_abstraction_layer_operating_system.h".
  #endif

  #define LIBBENCHMARK_PAL_OPERATING_SYSTEM

  #define LIBBENCHMARK_PAL_OS_STRING  "Windows"

  #include <windows.h>

  #define LIBBENCHMARK_PAL_TIME_UNITS_PER_SECOND( pointer_to_time_units_per_second )  QueryPerformanceFrequency( (LARGE_INTEGER *)(pointer_to_time_units_per_second) )

  #define LIBBENCHMARK_PAL_GET_HIGHRES_TIME( pointer_to_time )                        QueryPerformanceCounter( (LARGE_INTEGER *)(pointer_to_time) );

#endif





/****************************************************************************/
#if( defined _WIN32 && defined KERNEL_MODE && NTDDI_VERSION >= NTDDI_WINXP )

  #ifdef LIBBENCHMARK_PAL_OPERATING_SYSTEM
    #error More than one porting abstraction layer matches current platform. in "libbenchmark_porting_abstraction_layer_operating_system.h".
  #endif

  #define LIBBENCHMARK_PAL_OPERATING_SYSTEM

  #define LIBBENCHMARK_PAL_OS_STRING  "Windows"

  #include <wdm.h>

  #define LIBBENCHMARK_PAL_TIME_UNITS_PER_SECOND( pointer_to_time_units_per_second )  KeQueryPerformanceCounter( (LARGE_INTEGER *)(pointer_to_time_units_per_second) )

  #define LIBBENCHMARK_PAL_GET_HIGHRES_TIME( pointer_to_time )                        *( (LARGE_INTEGER *) pointer_to_time ) = KeQueryPerformanceCounter( NULL );

#endif





/****************************************************************************/
#if( defined __linux__ && !defined KERNEL_MODE )

  #ifdef LIBBENCHMARK_PAL_OPERATING_SYSTEM
    #error More than one porting abstraction layer matches current platform. in "libbenchmark_porting_abstraction_layer_operating_system.h".
  #endif

  #define LIBBENCHMARK_PAL_OPERATING_SYSTEM

  #define LIBBENCHMARK_PAL_OS_STRING  "Linux"

  #define _GNU_SOURCE

  #include <unistd.h>
  #include <stdarg.h>
  #include <stdio.h>

  #if( defined _POSIX_THREADS && _POSIX_TIMERS >= 0 && _POSIX_MONOTONIC_CLOCK >= 0 )
    #define LIBBENCHMARK_PAL_TIME_UNITS_PER_SECOND( pointer_to_time_units_per_second )  *(pointer_to_time_units_per_second) = NUMBER_OF_NANOSECONDS_IN_ONE_SECOND

    #define LIBBENCHMARK_PAL_GET_HIGHRES_TIME( pointer_to_time )                          \
    {                                                                                     \
      struct timespec tp;                                                                 \
      clock_gettime( CLOCK_MONOTONIC_RAW, &tp );                                          \
      *(pointer_to_time) = tp.tv_sec * NUMBER_OF_NANOSECONDS_IN_ONE_SECOND + tp.tv_nsec;  \
    }
  #else
    #error Linux without high resolution timers.
  #endif

#endif





/****************************************************************************/
#if( defined __linux__ && defined KERNEL_MODE )

  #ifdef LIBBENCHMARK_PAL_OPERATING_SYSTEM
    #error More than one porting abstraction layer matches current platform. in "libbenchmark_porting_abstraction_layer_operating_system.h".
  #endif

  #define LIBBENCHMARK_PAL_OPERATING_SYSTEM

  #define LIBBENCHMARK_PAL_OS_STRING  "Linux"

  #define _GNU_SOURCE

  #include <linux/module.h>

  /* TRD : not clear to me how to obtain high res freq and current count, in Linux kernel
           it doesn't matter right now because it becamse clear earlier there's no wait-for-thread-to-complete
           function either, which breaks the lfds thread abstraction, and until I sort that out, benchmark
           can't run anyway
  */

  #error No high resolution time abstraction for the Linux kernel.

#endif





/****************************************************************************/
#if( !defined LIBBENCHMARK_PAL_OPERATING_SYSTEM )

  #error No matching porting abstraction layer in "libbenchmark_porting_abstraction_layer_operating_system.h".

#endif

