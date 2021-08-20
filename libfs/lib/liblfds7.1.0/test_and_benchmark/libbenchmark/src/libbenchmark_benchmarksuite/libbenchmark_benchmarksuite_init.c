/***** includes *****/
#include "libbenchmark_benchmarksuite_internal.h"





/****************************************************************************/
#pragma warning( disable : 4127 )

void libbenchmark_benchmarksuite_init( struct libbenchmark_benchmarksuite_state *bss,
                                       struct libbenchmark_topology_state *ts,
                                       struct libshared_memory_state *ms,
                                       enum libbenchmark_topology_numa_mode numa_mode,
                                       lfds710_pal_uint_t options_bitmask,
                                       lfds710_pal_uint_t benchmark_duration_in_seconds )
{
  struct libbenchmark_benchmarkinstance_state
    *bs;

  struct libbenchmark_benchmarkset_state
    *bsets_btree_au,
    *bsets_freelist,
    *bsets_queue_umm;

  LFDS710_PAL_ASSERT( bss != NULL );
  LFDS710_PAL_ASSERT( ts != NULL );
  LFDS710_PAL_ASSERT( ms != NULL );
  LFDS710_PAL_ASSERT( numa_mode == LIBBENCHMARK_TOPOLOGY_NUMA_MODE_SMP or numa_mode == LIBBENCHMARK_TOPOLOGY_NUMA_MODE_NUMA );
  // TRD : options_bitmask is a bitmask and difficult to assert
  // TRD : benchmark_duration_in_seconds can be any value in its range

  bss->ts = ts;
  bss->ms = ms;
  libbenchmark_topology_generate_deduplicated_logical_processor_sets( bss->ts, ms, &bss->lpsets );
  libbenchmark_topology_generate_numa_modes_list( bss->ts, numa_mode, ms, &bss->numa_modes_list );
  lfds710_list_asu_init_valid_on_current_logical_core( &bss->benchmarksets, NULL );

  if( options_bitmask & LIBBENCHMARK_BENCHMARKSUITE_OPTION_DURATION )
    libbenchmark_globals_benchmark_duration_in_seconds = benchmark_duration_in_seconds;

  // TRD : btree_au
  if( LFDS710_MISC_ATOMIC_SUPPORT_COMPILER_BARRIERS and LFDS710_MISC_ATOMIC_SUPPORT_PROCESSOR_BARRIERS and LFDS710_MISC_ATOMIC_SUPPORT_CAS )
  {
    // TRD : btree_au set
    bsets_btree_au = libshared_memory_alloc_from_most_free_space_node( ms, sizeof(struct libbenchmark_benchmarkset_state), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
    libbenchmark_benchmarkset_init( bsets_btree_au, LIBBENCHMARK_DATASTRUCTURE_ID_BTREE_AU, LIBBENCHMARK_BENCHMARK_ID_READN_THEN_WRITEN, &bss->lpsets, &bss->numa_modes_list, bss->ts, ms );
    libbenchmark_benchmarksuite_add_benchmarkset( bss, bsets_btree_au );

    if( LIBBENCHMARK_PAL_LOCK_GCC_SPINLOCK_ATOMIC )
    {
      bs = libshared_memory_alloc_from_most_free_space_node( ms, sizeof(struct libbenchmark_benchmarkinstance_state), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
      libbenchmark_benchmarkinstance_init( bs, LIBBENCHMARK_DATASTRUCTURE_ID_BTREE_AU, LIBBENCHMARK_BENCHMARK_ID_READN_THEN_WRITEN, LIBBENCHMARK_LOCK_ID_GCC_SPINLOCK_ATOMIC, bss->ts, libbenchmark_benchmark_btree_au_gcc_spinlock_atomic_readn_writen_init, libbenchmark_benchmark_btree_au_gcc_spinlock_atomic_readn_writen_cleanup );
      libbenchmark_benchmarkset_add_benchmark( bsets_btree_au, bs );
    }

    if( LIBBENCHMARK_PAL_LOCK_GCC_SPINLOCK_SYNC )
    {
      bs = libshared_memory_alloc_from_most_free_space_node( ms, sizeof(struct libbenchmark_benchmarkinstance_state), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
      libbenchmark_benchmarkinstance_init( bs, LIBBENCHMARK_DATASTRUCTURE_ID_BTREE_AU, LIBBENCHMARK_BENCHMARK_ID_READN_THEN_WRITEN, LIBBENCHMARK_LOCK_ID_GCC_SPINLOCK_SYNC, bss->ts, libbenchmark_benchmark_btree_au_gcc_spinlock_sync_readn_writen_init, libbenchmark_benchmark_btree_au_gcc_spinlock_sync_readn_writen_cleanup );
      libbenchmark_benchmarkset_add_benchmark( bsets_btree_au, bs );
    }

    bs = libshared_memory_alloc_from_most_free_space_node( ms, sizeof(struct libbenchmark_benchmarkinstance_state), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
    libbenchmark_benchmarkinstance_init( bs, LIBBENCHMARK_DATASTRUCTURE_ID_BTREE_AU, LIBBENCHMARK_BENCHMARK_ID_READN_THEN_WRITEN, LIBBENCHMARK_LOCK_ID_LIBLFDS700_LOCKFREE, bss->ts, libbenchmark_benchmark_btree_au_liblfds700_lockfree_readn_writen_init, libbenchmark_benchmark_btree_au_liblfds700_lockfree_readn_writen_cleanup );
    libbenchmark_benchmarkset_add_benchmark( bsets_btree_au, bs );

    bs = libshared_memory_alloc_from_most_free_space_node( ms, sizeof(struct libbenchmark_benchmarkinstance_state), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
    libbenchmark_benchmarkinstance_init( bs, LIBBENCHMARK_DATASTRUCTURE_ID_BTREE_AU, LIBBENCHMARK_BENCHMARK_ID_READN_THEN_WRITEN, LIBBENCHMARK_LOCK_ID_LIBLFDS710_LOCKFREE, bss->ts, libbenchmark_benchmark_btree_au_liblfds710_lockfree_readn_writen_init, libbenchmark_benchmark_btree_au_liblfds710_lockfree_readn_writen_cleanup );
    libbenchmark_benchmarkset_add_benchmark( bsets_btree_au, bs );

    if( LIBBENCHMARK_PAL_LOCK_MSVC_SPINLOCK )
    {
      bs = libshared_memory_alloc_from_most_free_space_node( ms, sizeof(struct libbenchmark_benchmarkinstance_state), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
      libbenchmark_benchmarkinstance_init( bs, LIBBENCHMARK_DATASTRUCTURE_ID_BTREE_AU, LIBBENCHMARK_BENCHMARK_ID_READN_THEN_WRITEN, LIBBENCHMARK_LOCK_ID_MSVC_SPINLOCK, bss->ts, libbenchmark_benchmark_btree_au_msvc_spinlock_readn_writen_init, libbenchmark_benchmark_btree_au_msvc_spinlock_readn_writen_cleanup );
      libbenchmark_benchmarkset_add_benchmark( bsets_btree_au, bs );
    }

    if( LIBBENCHMARK_PAL_LOCK_PTHREAD_MUTEX )
    {
      bs = libshared_memory_alloc_from_most_free_space_node( ms, sizeof(struct libbenchmark_benchmarkinstance_state), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
      libbenchmark_benchmarkinstance_init( bs, LIBBENCHMARK_DATASTRUCTURE_ID_BTREE_AU, LIBBENCHMARK_BENCHMARK_ID_READN_THEN_WRITEN, LIBBENCHMARK_LOCK_ID_PTHREAD_MUTEX, bss->ts, libbenchmark_benchmark_btree_au_pthread_mutex_readn_writen_init, libbenchmark_benchmark_btree_au_pthread_mutex_readn_writen_cleanup );
      libbenchmark_benchmarkset_add_benchmark( bsets_btree_au, bs );
    }

    if( LIBBENCHMARK_PAL_LOCK_PTHREAD_RWLOCK )
    {
      bs = libshared_memory_alloc_from_most_free_space_node( ms, sizeof(struct libbenchmark_benchmarkinstance_state), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
      libbenchmark_benchmarkinstance_init( bs, LIBBENCHMARK_DATASTRUCTURE_ID_BTREE_AU, LIBBENCHMARK_BENCHMARK_ID_READN_THEN_WRITEN, LIBBENCHMARK_LOCK_ID_PTHREAD_RWLOCK, bss->ts, libbenchmark_benchmark_btree_au_pthread_rwlock_readn_writen_init, libbenchmark_benchmark_btree_au_pthread_rwlock_readn_writen_cleanup );
      libbenchmark_benchmarkset_add_benchmark( bsets_btree_au, bs );
    }

    if( LIBBENCHMARK_PAL_LOCK_PTHREAD_SPINLOCK_PROCESS_PRIVATE )
    {
      bs = libshared_memory_alloc_from_most_free_space_node( ms, sizeof(struct libbenchmark_benchmarkinstance_state), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
      libbenchmark_benchmarkinstance_init( bs, LIBBENCHMARK_DATASTRUCTURE_ID_BTREE_AU, LIBBENCHMARK_BENCHMARK_ID_READN_THEN_WRITEN, LIBBENCHMARK_LOCK_ID_PTHREAD_SPINLOCK_PROCESS_PRIVATE, bss->ts, libbenchmark_benchmark_btree_au_pthread_spinlock_process_private_readn_writen_init, libbenchmark_benchmark_btree_au_pthread_spinlock_process_private_readn_writen_cleanup );
      libbenchmark_benchmarkset_add_benchmark( bsets_btree_au, bs );
    }

    if( LIBBENCHMARK_PAL_LOCK_PTHREAD_SPINLOCK_PROCESS_SHARED )
    {
      bs = libshared_memory_alloc_from_most_free_space_node( ms, sizeof(struct libbenchmark_benchmarkinstance_state), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
      libbenchmark_benchmarkinstance_init( bs, LIBBENCHMARK_DATASTRUCTURE_ID_BTREE_AU, LIBBENCHMARK_BENCHMARK_ID_READN_THEN_WRITEN, LIBBENCHMARK_LOCK_ID_PTHREAD_SPINLOCK_PROCESS_SHARED, bss->ts, libbenchmark_benchmark_btree_au_pthread_spinlock_process_shared_readn_writen_init, libbenchmark_benchmark_btree_au_pthread_spinlock_process_shared_readn_writen_cleanup );
      libbenchmark_benchmarkset_add_benchmark( bsets_btree_au, bs );
    }

    if( LIBBENCHMARK_PAL_LOCK_WINDOWS_CRITICAL_SECTION )
    {
      bs = libshared_memory_alloc_from_most_free_space_node( ms, sizeof(struct libbenchmark_benchmarkinstance_state), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
      libbenchmark_benchmarkinstance_init( bs, LIBBENCHMARK_DATASTRUCTURE_ID_BTREE_AU, LIBBENCHMARK_BENCHMARK_ID_READN_THEN_WRITEN, LIBBENCHMARK_LOCK_ID_WINDOWS_CRITICAL_SECTION, bss->ts, libbenchmark_benchmark_btree_au_windows_critical_section_readn_writen_init, libbenchmark_benchmark_btree_au_windows_critical_section_readn_writen_cleanup );
      libbenchmark_benchmarkset_add_benchmark( bsets_btree_au, bs );
    }

    if( LIBBENCHMARK_PAL_LOCK_WINDOWS_MUTEX )
    {
      bs = libshared_memory_alloc_from_most_free_space_node( ms, sizeof(struct libbenchmark_benchmarkinstance_state), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
      libbenchmark_benchmarkinstance_init( bs, LIBBENCHMARK_DATASTRUCTURE_ID_BTREE_AU, LIBBENCHMARK_BENCHMARK_ID_READN_THEN_WRITEN, LIBBENCHMARK_LOCK_ID_WINDOWS_MUTEX, bss->ts, libbenchmark_benchmark_btree_au_windows_mutex_readn_writen_init, libbenchmark_benchmark_btree_au_windows_mutex_readn_writen_cleanup );
      libbenchmark_benchmarkset_add_benchmark( bsets_btree_au, bs );
    }
  }

  // TRD : freelist
  if( LFDS710_MISC_ATOMIC_SUPPORT_COMPILER_BARRIERS and LFDS710_MISC_ATOMIC_SUPPORT_PROCESSOR_BARRIERS and LFDS710_MISC_ATOMIC_SUPPORT_DWCAS )
  {
    // TRD : freelist set
    bsets_freelist = libshared_memory_alloc_from_most_free_space_node( ms, sizeof(struct libbenchmark_benchmarkset_state), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
    libbenchmark_benchmarkset_init( bsets_freelist, LIBBENCHMARK_DATASTRUCTURE_ID_FREELIST, LIBBENCHMARK_BENCHMARK_ID_PUSH1_THEN_POP1, &bss->lpsets, &bss->numa_modes_list, bss->ts, ms );
    libbenchmark_benchmarksuite_add_benchmarkset( bss, bsets_freelist );

    if( LIBBENCHMARK_PAL_LOCK_GCC_SPINLOCK_ATOMIC )
    {
      bs = libshared_memory_alloc_from_most_free_space_node( ms, sizeof(struct libbenchmark_benchmarkinstance_state), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
      libbenchmark_benchmarkinstance_init( bs, LIBBENCHMARK_DATASTRUCTURE_ID_FREELIST, LIBBENCHMARK_BENCHMARK_ID_PUSH1_THEN_POP1, LIBBENCHMARK_LOCK_ID_GCC_SPINLOCK_ATOMIC, bss->ts, libbenchmark_benchmark_freelist_gcc_spinlock_atomic_push1_pop1_init, libbenchmark_benchmark_freelist_gcc_spinlock_atomic_push1_pop1_cleanup );
      libbenchmark_benchmarkset_add_benchmark( bsets_freelist, bs );
    }

    if( LIBBENCHMARK_PAL_LOCK_GCC_SPINLOCK_SYNC )
    {
      bs = libshared_memory_alloc_from_most_free_space_node( ms, sizeof(struct libbenchmark_benchmarkinstance_state), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
      libbenchmark_benchmarkinstance_init( bs, LIBBENCHMARK_DATASTRUCTURE_ID_FREELIST, LIBBENCHMARK_BENCHMARK_ID_PUSH1_THEN_POP1, LIBBENCHMARK_LOCK_ID_GCC_SPINLOCK_SYNC, bss->ts, libbenchmark_benchmark_freelist_gcc_spinlock_sync_push1_pop1_init, libbenchmark_benchmark_freelist_gcc_spinlock_sync_push1_pop1_cleanup );
      libbenchmark_benchmarkset_add_benchmark( bsets_freelist, bs );
    }

    bs = libshared_memory_alloc_from_most_free_space_node( ms, sizeof(struct libbenchmark_benchmarkinstance_state), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
    libbenchmark_benchmarkinstance_init( bs, LIBBENCHMARK_DATASTRUCTURE_ID_FREELIST, LIBBENCHMARK_BENCHMARK_ID_PUSH1_THEN_POP1, LIBBENCHMARK_LOCK_ID_LIBLFDS700_LOCKFREE, bss->ts, libbenchmark_benchmark_freelist_liblfds700_lockfree_push1_pop1_init, libbenchmark_benchmark_freelist_liblfds700_lockfree_push1_pop1_cleanup );
    libbenchmark_benchmarkset_add_benchmark( bsets_freelist, bs );

    bs = libshared_memory_alloc_from_most_free_space_node( ms, sizeof(struct libbenchmark_benchmarkinstance_state), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
    libbenchmark_benchmarkinstance_init( bs, LIBBENCHMARK_DATASTRUCTURE_ID_FREELIST, LIBBENCHMARK_BENCHMARK_ID_PUSH1_THEN_POP1, LIBBENCHMARK_LOCK_ID_LIBLFDS710_LOCKFREE, bss->ts, libbenchmark_benchmark_freelist_liblfds710_lockfree_push1_pop1_init, libbenchmark_benchmark_freelist_liblfds710_lockfree_push1_pop1_cleanup );
    libbenchmark_benchmarkset_add_benchmark( bsets_freelist, bs );

    if( LIBBENCHMARK_PAL_LOCK_MSVC_SPINLOCK )
    {
      bs = libshared_memory_alloc_from_most_free_space_node( ms, sizeof(struct libbenchmark_benchmarkinstance_state), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
      libbenchmark_benchmarkinstance_init( bs, LIBBENCHMARK_DATASTRUCTURE_ID_FREELIST, LIBBENCHMARK_BENCHMARK_ID_PUSH1_THEN_POP1, LIBBENCHMARK_LOCK_ID_MSVC_SPINLOCK, bss->ts, libbenchmark_benchmark_freelist_msvc_spinlock_push1_pop1_init, libbenchmark_benchmark_freelist_msvc_spinlock_push1_pop1_cleanup );
      libbenchmark_benchmarkset_add_benchmark( bsets_freelist, bs );
    }

    if( LIBBENCHMARK_PAL_LOCK_PTHREAD_MUTEX )
    {
      bs = libshared_memory_alloc_from_most_free_space_node( ms, sizeof(struct libbenchmark_benchmarkinstance_state), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
      libbenchmark_benchmarkinstance_init( bs, LIBBENCHMARK_DATASTRUCTURE_ID_FREELIST, LIBBENCHMARK_BENCHMARK_ID_PUSH1_THEN_POP1, LIBBENCHMARK_LOCK_ID_PTHREAD_MUTEX, bss->ts, libbenchmark_benchmark_freelist_pthread_mutex_push1_pop1_init, libbenchmark_benchmark_freelist_pthread_mutex_push1_pop1_cleanup );
      libbenchmark_benchmarkset_add_benchmark( bsets_freelist, bs );
    }

    if( LIBBENCHMARK_PAL_LOCK_PTHREAD_RWLOCK )
    {
      bs = libshared_memory_alloc_from_most_free_space_node( ms, sizeof(struct libbenchmark_benchmarkinstance_state), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
      libbenchmark_benchmarkinstance_init( bs, LIBBENCHMARK_DATASTRUCTURE_ID_FREELIST, LIBBENCHMARK_BENCHMARK_ID_PUSH1_THEN_POP1, LIBBENCHMARK_LOCK_ID_PTHREAD_RWLOCK, bss->ts, libbenchmark_benchmark_freelist_pthread_rwlock_push1_pop1_init,  libbenchmark_benchmark_freelist_pthread_rwlock_push1_pop1_cleanup );
      libbenchmark_benchmarkset_add_benchmark( bsets_freelist, bs );
    }

    if( LIBBENCHMARK_PAL_LOCK_PTHREAD_SPINLOCK_PROCESS_PRIVATE )
    {
      bs = libshared_memory_alloc_from_most_free_space_node( ms, sizeof(struct libbenchmark_benchmarkinstance_state), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
      libbenchmark_benchmarkinstance_init( bs, LIBBENCHMARK_DATASTRUCTURE_ID_FREELIST, LIBBENCHMARK_BENCHMARK_ID_PUSH1_THEN_POP1, LIBBENCHMARK_LOCK_ID_PTHREAD_SPINLOCK_PROCESS_PRIVATE, bss->ts, libbenchmark_benchmark_freelist_pthread_spinlock_process_private_push1_pop1_init, libbenchmark_benchmark_freelist_pthread_spinlock_process_private_push1_pop1_cleanup );
      libbenchmark_benchmarkset_add_benchmark( bsets_freelist, bs );
    }

    if( LIBBENCHMARK_PAL_LOCK_PTHREAD_SPINLOCK_PROCESS_SHARED )
    {
      bs = libshared_memory_alloc_from_most_free_space_node( ms, sizeof(struct libbenchmark_benchmarkinstance_state), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
      libbenchmark_benchmarkinstance_init( bs, LIBBENCHMARK_DATASTRUCTURE_ID_FREELIST, LIBBENCHMARK_BENCHMARK_ID_PUSH1_THEN_POP1, LIBBENCHMARK_LOCK_ID_PTHREAD_SPINLOCK_PROCESS_SHARED, bss->ts, libbenchmark_benchmark_freelist_pthread_spinlock_process_shared_push1_pop1_init, libbenchmark_benchmark_freelist_pthread_spinlock_process_shared_push1_pop1_cleanup );
      libbenchmark_benchmarkset_add_benchmark( bsets_freelist, bs );
    }

    if( LIBBENCHMARK_PAL_LOCK_WINDOWS_CRITICAL_SECTION )
    {
      bs = libshared_memory_alloc_from_most_free_space_node( ms, sizeof(struct libbenchmark_benchmarkinstance_state), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
      libbenchmark_benchmarkinstance_init( bs, LIBBENCHMARK_DATASTRUCTURE_ID_FREELIST, LIBBENCHMARK_BENCHMARK_ID_PUSH1_THEN_POP1, LIBBENCHMARK_LOCK_ID_WINDOWS_CRITICAL_SECTION, bss->ts, libbenchmark_benchmark_freelist_windows_critical_section_push1_pop1_init, libbenchmark_benchmark_freelist_windows_critical_section_push1_pop1_cleanup );
      libbenchmark_benchmarkset_add_benchmark( bsets_freelist, bs );
    }

    if( LIBBENCHMARK_PAL_LOCK_WINDOWS_MUTEX )
    {
      bs = libshared_memory_alloc_from_most_free_space_node( ms, sizeof(struct libbenchmark_benchmarkinstance_state), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
      libbenchmark_benchmarkinstance_init( bs, LIBBENCHMARK_DATASTRUCTURE_ID_FREELIST, LIBBENCHMARK_BENCHMARK_ID_PUSH1_THEN_POP1, LIBBENCHMARK_LOCK_ID_WINDOWS_MUTEX, bss->ts, libbenchmark_benchmark_freelist_windows_mutex_push1_pop1_init, libbenchmark_benchmark_freelist_windows_mutex_push1_pop1_cleanup );
      libbenchmark_benchmarkset_add_benchmark( bsets_freelist, bs );
    }
  }

  // TRD : queue_umm
  if( LFDS710_MISC_ATOMIC_SUPPORT_COMPILER_BARRIERS and LFDS710_MISC_ATOMIC_SUPPORT_PROCESSOR_BARRIERS and LFDS710_MISC_ATOMIC_SUPPORT_DWCAS )
  {
    // TRD : queue_umm set
    bsets_queue_umm = libshared_memory_alloc_from_most_free_space_node( ms, sizeof(struct libbenchmark_benchmarkset_state), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
    libbenchmark_benchmarkset_init( bsets_queue_umm, LIBBENCHMARK_DATASTRUCTURE_ID_QUEUE_UMM, LIBBENCHMARK_BENCHMARK_ID_ENQUEUE_UMM1_THEN_DEQUEUE_UMM1, &bss->lpsets, &bss->numa_modes_list, bss->ts, ms );
    libbenchmark_benchmarksuite_add_benchmarkset( bss, bsets_queue_umm );

    if( LIBBENCHMARK_PAL_LOCK_GCC_SPINLOCK_ATOMIC )
    {
      bs = libshared_memory_alloc_from_most_free_space_node( ms, sizeof(struct libbenchmark_benchmarkinstance_state), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
      libbenchmark_benchmarkinstance_init( bs, LIBBENCHMARK_DATASTRUCTURE_ID_QUEUE_UMM, LIBBENCHMARK_BENCHMARK_ID_ENQUEUE_UMM1_THEN_DEQUEUE_UMM1, LIBBENCHMARK_LOCK_ID_GCC_SPINLOCK_ATOMIC, bss->ts, libbenchmark_benchmark_queue_umm_gcc_spinlock_atomic_enqueue1_dequeue1_init, libbenchmark_benchmark_queue_umm_gcc_spinlock_atomic_enqueue1_dequeue1_cleanup );
      libbenchmark_benchmarkset_add_benchmark( bsets_queue_umm, bs );
    }

    if( LIBBENCHMARK_PAL_LOCK_GCC_SPINLOCK_SYNC )
    {
      bs = libshared_memory_alloc_from_most_free_space_node( ms, sizeof(struct libbenchmark_benchmarkinstance_state), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
      libbenchmark_benchmarkinstance_init( bs, LIBBENCHMARK_DATASTRUCTURE_ID_QUEUE_UMM, LIBBENCHMARK_BENCHMARK_ID_ENQUEUE_UMM1_THEN_DEQUEUE_UMM1, LIBBENCHMARK_LOCK_ID_GCC_SPINLOCK_SYNC, bss->ts, libbenchmark_benchmark_queue_umm_gcc_spinlock_sync_enqueue1_dequeue1_init, libbenchmark_benchmark_queue_umm_gcc_spinlock_sync_enqueue1_dequeue1_cleanup );
      libbenchmark_benchmarkset_add_benchmark( bsets_queue_umm, bs );
    }

    bs = libshared_memory_alloc_from_most_free_space_node( ms, sizeof(struct libbenchmark_benchmarkinstance_state), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
    libbenchmark_benchmarkinstance_init( bs, LIBBENCHMARK_DATASTRUCTURE_ID_QUEUE_UMM, LIBBENCHMARK_BENCHMARK_ID_ENQUEUE_UMM1_THEN_DEQUEUE_UMM1, LIBBENCHMARK_LOCK_ID_LIBLFDS700_LOCKFREE, bss->ts, libbenchmark_benchmark_queue_umm_liblfds700_lockfree_enqueue1_dequeue1_init, libbenchmark_benchmark_queue_umm_liblfds700_lockfree_enqueue1_dequeue1_cleanup );
    libbenchmark_benchmarkset_add_benchmark( bsets_queue_umm, bs );

    bs = libshared_memory_alloc_from_most_free_space_node( ms, sizeof(struct libbenchmark_benchmarkinstance_state), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
    libbenchmark_benchmarkinstance_init( bs, LIBBENCHMARK_DATASTRUCTURE_ID_QUEUE_UMM, LIBBENCHMARK_BENCHMARK_ID_ENQUEUE_UMM1_THEN_DEQUEUE_UMM1, LIBBENCHMARK_LOCK_ID_LIBLFDS710_LOCKFREE, bss->ts, libbenchmark_benchmark_queue_umm_liblfds710_lockfree_enqueue1_dequeue1_init, libbenchmark_benchmark_queue_umm_liblfds710_lockfree_enqueue1_dequeue1_cleanup );
    libbenchmark_benchmarkset_add_benchmark( bsets_queue_umm, bs );

    if( LIBBENCHMARK_PAL_LOCK_MSVC_SPINLOCK )
    {
      bs = libshared_memory_alloc_from_most_free_space_node( ms, sizeof(struct libbenchmark_benchmarkinstance_state), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
      libbenchmark_benchmarkinstance_init( bs, LIBBENCHMARK_DATASTRUCTURE_ID_QUEUE_UMM, LIBBENCHMARK_BENCHMARK_ID_ENQUEUE_UMM1_THEN_DEQUEUE_UMM1, LIBBENCHMARK_LOCK_ID_MSVC_SPINLOCK, bss->ts, libbenchmark_benchmark_queue_umm_msvc_spinlock_enqueue1_dequeue1_init, libbenchmark_benchmark_queue_umm_msvc_spinlock_enqueue1_dequeue1_cleanup );
      libbenchmark_benchmarkset_add_benchmark( bsets_queue_umm, bs );
    }

    if( LIBBENCHMARK_PAL_LOCK_PTHREAD_MUTEX )
    {
      bs = libshared_memory_alloc_from_most_free_space_node( ms, sizeof(struct libbenchmark_benchmarkinstance_state), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
      libbenchmark_benchmarkinstance_init( bs, LIBBENCHMARK_DATASTRUCTURE_ID_QUEUE_UMM, LIBBENCHMARK_BENCHMARK_ID_ENQUEUE_UMM1_THEN_DEQUEUE_UMM1, LIBBENCHMARK_LOCK_ID_PTHREAD_MUTEX, bss->ts, libbenchmark_benchmark_queue_umm_pthread_mutex_enqueue1_dequeue1_init, libbenchmark_benchmark_queue_umm_pthread_mutex_enqueue1_dequeue1_cleanup );
      libbenchmark_benchmarkset_add_benchmark( bsets_queue_umm, bs );
    }

    if( LIBBENCHMARK_PAL_LOCK_PTHREAD_RWLOCK )
    {
      bs = libshared_memory_alloc_from_most_free_space_node( ms, sizeof(struct libbenchmark_benchmarkinstance_state), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
      libbenchmark_benchmarkinstance_init( bs, LIBBENCHMARK_DATASTRUCTURE_ID_QUEUE_UMM, LIBBENCHMARK_BENCHMARK_ID_ENQUEUE_UMM1_THEN_DEQUEUE_UMM1, LIBBENCHMARK_LOCK_ID_PTHREAD_RWLOCK, bss->ts, libbenchmark_benchmark_queue_umm_pthread_rwlock_enqueue1_dequeue1_init, libbenchmark_benchmark_queue_umm_pthread_rwlock_enqueue1_dequeue1_cleanup );
      libbenchmark_benchmarkset_add_benchmark( bsets_queue_umm, bs );
    }

    if( LIBBENCHMARK_PAL_LOCK_PTHREAD_SPINLOCK_PROCESS_PRIVATE )
    {
      bs = libshared_memory_alloc_from_most_free_space_node( ms, sizeof(struct libbenchmark_benchmarkinstance_state), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
      libbenchmark_benchmarkinstance_init( bs, LIBBENCHMARK_DATASTRUCTURE_ID_QUEUE_UMM, LIBBENCHMARK_BENCHMARK_ID_ENQUEUE_UMM1_THEN_DEQUEUE_UMM1, LIBBENCHMARK_LOCK_ID_PTHREAD_SPINLOCK_PROCESS_PRIVATE, bss->ts, libbenchmark_benchmark_queue_umm_pthread_spinlock_process_private_enqueue1_dequeue1_init, libbenchmark_benchmark_queue_umm_pthread_spinlock_process_private_enqueue1_dequeue1_cleanup );
      libbenchmark_benchmarkset_add_benchmark( bsets_queue_umm, bs );
    }

    if( LIBBENCHMARK_PAL_LOCK_PTHREAD_SPINLOCK_PROCESS_SHARED )
    {
      bs = libshared_memory_alloc_from_most_free_space_node( ms, sizeof(struct libbenchmark_benchmarkinstance_state), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
      libbenchmark_benchmarkinstance_init( bs, LIBBENCHMARK_DATASTRUCTURE_ID_QUEUE_UMM, LIBBENCHMARK_BENCHMARK_ID_ENQUEUE_UMM1_THEN_DEQUEUE_UMM1, LIBBENCHMARK_LOCK_ID_PTHREAD_SPINLOCK_PROCESS_SHARED, bss->ts, libbenchmark_benchmark_queue_umm_pthread_spinlock_process_shared_enqueue1_dequeue1_init, libbenchmark_benchmark_queue_umm_pthread_spinlock_process_shared_enqueue1_dequeue1_cleanup );
      libbenchmark_benchmarkset_add_benchmark( bsets_queue_umm, bs );
    }

    if( LIBBENCHMARK_PAL_LOCK_WINDOWS_CRITICAL_SECTION )
    {
      bs = libshared_memory_alloc_from_most_free_space_node( ms, sizeof(struct libbenchmark_benchmarkinstance_state), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
      libbenchmark_benchmarkinstance_init( bs, LIBBENCHMARK_DATASTRUCTURE_ID_QUEUE_UMM, LIBBENCHMARK_BENCHMARK_ID_ENQUEUE_UMM1_THEN_DEQUEUE_UMM1, LIBBENCHMARK_LOCK_ID_WINDOWS_CRITICAL_SECTION, bss->ts, libbenchmark_benchmark_queue_umm_windows_critical_section_enqueue1_dequeue1_init, libbenchmark_benchmark_queue_umm_windows_critical_section_enqueue1_dequeue1_cleanup );
      libbenchmark_benchmarkset_add_benchmark( bsets_queue_umm, bs );
    }

    if( LIBBENCHMARK_PAL_LOCK_WINDOWS_MUTEX )
    {
      bs = libshared_memory_alloc_from_most_free_space_node( ms, sizeof(struct libbenchmark_benchmarkinstance_state), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
      libbenchmark_benchmarkinstance_init( bs, LIBBENCHMARK_DATASTRUCTURE_ID_QUEUE_UMM, LIBBENCHMARK_BENCHMARK_ID_ENQUEUE_UMM1_THEN_DEQUEUE_UMM1, LIBBENCHMARK_LOCK_ID_WINDOWS_MUTEX, bss->ts, libbenchmark_benchmark_queue_umm_windows_mutex_enqueue1_dequeue1_init, libbenchmark_benchmark_queue_umm_windows_mutex_enqueue1_dequeue1_cleanup );
      libbenchmark_benchmarkset_add_benchmark( bsets_queue_umm, bs );
    }
  }

  return;
}

#pragma warning( default : 4127 )

