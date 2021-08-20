/***** includes *****/
#include "internal.h"





/****************************************************************************/
#if( defined _WIN32 && !defined KERNEL_MODE && NTDDI_VERSION >= NTDDI_WINXP )

  #pragma warning( disable : 4100 )

  void benchmark_pal_numa_free( void *memory, lfds710_pal_uint_t size_in_bytes )
  {
    HANDLE
      process_handle;

    assert( memory != NULL );
    // TRD : size_in_bytes can be any value in its range

    process_handle = GetCurrentProcess();

    VirtualFreeEx( process_handle, memory, 0, MEM_RELEASE );

    return;
  }

  #pragma warning( default : 4100 )

#endif





/****************************************************************************/
#if( defined __linux__ && defined LIBNUMA )

  #ifdef BENCHMARK_PAL_NUMA_FREE
    #error More than one porting abstraction layer matches the current platform in porting_abstraction_free.c
  #endif

  #define BENCHMARK_PAL_NUMA_FREE

  void benchmark_pal_numa_free( void *memory, lfds710_pal_uint_t size_in_bytes )
  {
    assert( memory != NULL );
    // TRD : size_in_bytes can be any value in its range

    #if( defined _POSIX_MEMLOCK_RANGE > 0 )
      munlock( memory, size_in_bytes );
    #endif

    numa_free( memory, size_in_bytes );

    return;
  }

#endif

