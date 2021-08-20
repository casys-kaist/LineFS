/****************************************************************************/
#if( defined _MSC_VER )
  /* TRD : MSVC compiler

           an unfortunately necessary hack for MSVC
           MSVC only defines __STDC__ if /Za is given, where /Za turns off MSVC C extensions - 
           which prevents Windows header files from compiling.
  */

  #define __STDC__         1
  #define __STDC_HOSTED__  1
#endif





/****************************************************************************/
#if( defined _WIN32 && NTDDI_VERSION < NTDDI_VISTA )

  #ifdef BENCHMARK_PAL_OPERATING_SYSTEM
    #error More than one porting abstraction layer matches current platform.
  #endif

  #define BENCHMARK_PAL_OPERATING_SYSTEM

  #define BENCHMARK_PAL_OS_STRING           "Windows"
  #define BENCHMARK_PAL_MEMORY_TYPE         BENCHMARK_MEMORY_TYPE_SMP
  #define BENCHMARK_PAL_MEMORY_TYPE_STRING  "SMP"

#endif





/****************************************************************************/
#if( defined _WIN32 && NTDDI_VERSION >= NTDDI_VISTA )

  #ifdef BENCHMARK_PAL_OPERATING_SYSTEM
    #error More than one porting abstraction layer matches current platform.
  #endif

  #define BENCHMARK_PAL_OPERATING_SYSTEM

  #define BENCHMARK_PAL_OS_STRING           "Windows"
  #define BENCHMARK_PAL_MEMORY_TYPE         BENCHMARK_MEMORY_TYPE_NUMA
  #define BENCHMARK_PAL_MEMORY_TYPE_STRING  "NUMA"

#endif





/****************************************************************************/
#if( defined __linux__ && !defined KERNEL_MODE && !defined LIBNUMA )

  #ifdef BENCHMARK_PAL_OPERATING_SYSTEM
    #error More than one porting abstraction layer matches current platform.
  #endif

  #define BENCHMARK_PAL_OPERATING_SYSTEM

  #define BENCHMARK_PAL_OS_STRING           "Linux"
  #define BENCHMARK_PAL_MEMORY_TYPE         BENCHMARK_MEMORY_TYPE_SMP
  #define BENCHMARK_PAL_MEMORY_TYPE_STRING  "SMP"

  #include <unistd.h>
  #if( _POSIX_MEMLOCK_RANGE > 0 )
    #include <sys/mman.h>
  #endif

#endif





/****************************************************************************/
#if( defined __linux__ && !defined KERNEL_MODE && defined LIBNUMA )

  #ifdef BENCHMARK_PAL_OPERATING_SYSTEM
    #error More than one porting abstraction layer matches current platform.
  #endif

  #define BENCHMARK_PAL_OPERATING_SYSTEM

  #define BENCHMARK_PAL_OS_STRING           "Linux"
  #define BENCHMARK_PAL_MEMORY_TYPE         BENCHMARK_MEMORY_TYPE_NUMA
  #define BENCHMARK_PAL_MEMORY_TYPE_STRING  "NUMA"

  #include <unistd.h>
  #include <numa.h>
  #if( _POSIX_MEMLOCK_RANGE > 0 )
    #include <sys/mman.h>
  #endif

#endif





/****************************************************************************/
#if( !defined BENCHMARK_PAL_OPERATING_SYSTEM )

  #error No operating system porting abstraction layer.

#endif

