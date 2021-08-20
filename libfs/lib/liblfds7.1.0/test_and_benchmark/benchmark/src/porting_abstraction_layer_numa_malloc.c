/***** includes *****/
#include "internal.h"





/****************************************************************************/
#if( defined _WIN32 && !defined KERNEL_MODE && NTDDI_VERSION >= NTDDI_VISTA )

  #ifdef BENCHMARK_PAL_NUMA_MALLOC
    #error More than one porting abstraction layer matches the current platform in porting_abstraction_malloc.c
  #endif

  #define BENCHMARK_PAL_NUMA_MALLOC

  void *benchmark_pal_numa_malloc( lfds710_pal_uint_t numa_node_id, lfds710_pal_uint_t size_in_bytes )
  {
    HANDLE
      process_handle;

    LPVOID
      memory;

    // TRD : numa_node_id can be any value in its range
    // TRD : size_in_bytes can be any value in its range

    process_handle = GetCurrentProcess();

    memory = VirtualAllocExNuma( process_handle, NULL, size_in_bytes, MEM_COMMIT, PAGE_READWRITE, (DWORD) numa_node_id );

    return memory;
  }

#endif





/****************************************************************************/
#if( defined __linux__ && defined LIBNUMA )

  #ifdef BENCHMARK_PAL_NUMA_MALLOC
    #error More than one porting abstraction layer matches the current platform in porting_abstraction_malloc.c
  #endif

  #define BENCHMARK_PAL_NUMA_MALLOC

  void *benchmark_pal_numa_malloc( lfds710_pal_uint_t numa_node_id, lfds710_pal_uint_t size_in_bytes )
  {
    void
      *memory;

    // TRD : numa_node_id can be any value in its range
    // TRD : size_in_bytes can be any value in its range

    memory = numa_alloc_onnode( size_in_bytes, (int) numa_node_id );

    /* TRD : mlock prevents paging
             this is unfortunately necessary on Linux
             due to serious shortcomings in the way NUMA is handled

             in particular that the NUMA node is re-chosen if a memory page is paged out and then paged back in
             but also because Linux doesn't page in a single page at a time, but a line of pages
             so another process can end up moving *your* pages into *its* NUMA node (e.g. your pages are
             in the line of pages), because the NUMA policy for *its* pages would put them in that node!

             it seems to me this is one of the very rare occasions
             where Windows has something right and Linux has it wrong
             (Windows has the notion of an ideal NUMA node for a thread, and continually works
              to move any pages which leave that node back into that node, and on page-in will
              try first to re-use that node)

             since we use small amounts of memory, I address the whole sorry mess
             simply by locking the pages into memory - this way they will stay in the NUMA node
             they were allocated into (assuming they've not been paged out and then back in,
             between the numa_alloc_onnode() call and the mlock() call)
    */

    #if( defined _POSIX_MEMLOCK_RANGE > 0 )
      mlock( memory, size_in_bytes );
    #endif

    return memory;
  }

#endif

