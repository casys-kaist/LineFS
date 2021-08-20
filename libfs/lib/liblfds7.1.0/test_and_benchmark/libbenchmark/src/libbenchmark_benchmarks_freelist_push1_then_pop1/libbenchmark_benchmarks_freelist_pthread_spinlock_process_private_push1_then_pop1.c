/***** includes *****/
#include "libbenchmark_benchmarks_freelist_internal.h"

/***** structs *****/
struct libbenchmark_benchmark_freelist_pthread_spinlock_process_private_push1_pop1_per_thread_benchmark_state
{
  lfds710_pal_uint_t
    operation_count;
};





/****************************************************************************/
void libbenchmark_benchmark_freelist_pthread_spinlock_process_private_push1_pop1_init( struct libbenchmark_topology_state *ts,
                                                                                       struct lfds710_list_aso_state *logical_processor_set,
                                                                                       struct libshared_memory_state *ms,
                                                                                       enum libbenchmark_topology_numa_mode numa_mode,
                                                                                       struct libbenchmark_threadset_state *tsets )
{
  lfds710_pal_uint_t
    loop,
    number_logical_processors,
    number_logical_processors_in_numa_node,
    largest_number_logical_processors_in_numa_node = 0;

  struct lfds710_list_asu_element
    *lasue = NULL,
    *lasue_lp = NULL;

  struct libbenchmark_benchmark_freelist_pthread_spinlock_process_private_push1_pop1_per_thread_benchmark_state
    *ptbs;

  struct libbenchmark_datastructure_freelist_pthread_spinlock_process_private_element
    *fe;

  struct libbenchmark_datastructure_freelist_pthread_spinlock_process_private_state
    *fs = NULL;

  struct libbenchmark_threadset_per_numa_state
    *pns,
    *largest_pns = NULL;

  struct libbenchmark_threadset_per_thread_state
    *pts;

  struct libbenchmark_topology_node_state
    *numa_node_for_lp;

  LFDS710_PAL_ASSERT( ts != NULL );
  LFDS710_PAL_ASSERT( logical_processor_set != NULL );
  LFDS710_PAL_ASSERT( ms != NULL );
  // TRD : numa_mode can be any value in its range
  LFDS710_PAL_ASSERT( tsets != NULL );

  libbenchmark_threadset_init( tsets, ts, logical_processor_set, ms, libbenchmark_benchmark_freelist_pthread_spinlock_process_private_push1_pop1_thread, NULL );

  switch( numa_mode )
  {
    case LIBBENCHMARK_TOPOLOGY_NUMA_MODE_SMP:
      fs = libshared_memory_alloc_from_most_free_space_node( ms, sizeof(struct libbenchmark_datastructure_freelist_pthread_spinlock_process_private_state), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
      libbenchmark_datastructure_freelist_pthread_spinlock_process_private_init( fs, NULL );
      lfds710_list_aso_query( logical_processor_set, LFDS710_LIST_ASO_QUERY_GET_POTENTIALLY_INACCURATE_COUNT, NULL, (void *) &number_logical_processors );
      fe = libshared_memory_alloc_from_most_free_space_node( ms, sizeof(struct libbenchmark_datastructure_freelist_pthread_spinlock_process_private_element) * number_logical_processors, LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
      for( loop = 0 ; loop < number_logical_processors ; loop++ )
        libbenchmark_datastructure_freelist_pthread_spinlock_process_private_push( fs, &fe[loop] );
      while( LFDS710_LIST_ASU_GET_START_AND_THEN_NEXT(tsets->list_of_per_thread_states,lasue) )
      {
        pts = LFDS710_LIST_ASU_GET_VALUE_FROM_ELEMENT( *lasue );
        ptbs = libshared_memory_alloc_from_most_free_space_node( ms, sizeof(struct libbenchmark_benchmark_freelist_pthread_spinlock_process_private_push1_pop1_per_thread_benchmark_state), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
        pts->users_per_thread_state = ptbs;
      }
    break;

    case LIBBENCHMARK_TOPOLOGY_NUMA_MODE_NUMA:
      /* TRD : init the freelist from the NUMA node with most processors from the current set
               or, if equal threads, with lowest NUMA
               iterate over the NUMA node list
               for each NUMA node, allocate one freelist element per thread on that node
               and push those elements onto the stack

               the loop over the threads, and give each one the freelist state as it's user state
      */

      while( LFDS710_LIST_ASU_GET_START_AND_THEN_NEXT(tsets->list_of_per_numa_states,lasue) )
      {
        pns = LFDS710_LIST_ASU_GET_VALUE_FROM_ELEMENT( *lasue );

        lasue_lp = NULL;
        number_logical_processors_in_numa_node = 0;

        while( LFDS710_LIST_ASU_GET_START_AND_THEN_NEXT(tsets->list_of_per_thread_states,lasue_lp) )
        {
          pts = LFDS710_LIST_ASU_GET_VALUE_FROM_ELEMENT( *lasue_lp );

          libbenchmark_topology_query( ts, LIBBENCHMARK_TOPOLOGY_QUERY_GET_NUMA_NODE_FOR_LOGICAL_PROCESSOR, pts->tns_lp, &numa_node_for_lp );

          if( LIBBENCHMARK_TOPOLOGY_NODE_GET_NUMA_ID(*numa_node_for_lp) == pns->numa_node_id )
            number_logical_processors_in_numa_node++;
        }

        if( number_logical_processors_in_numa_node > largest_number_logical_processors_in_numa_node )
          largest_pns = pns;
      }

      fs = libshared_memory_alloc_from_specific_node( ms, largest_pns->numa_node_id, sizeof(struct libbenchmark_datastructure_freelist_pthread_spinlock_process_private_state), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
      libbenchmark_datastructure_freelist_pthread_spinlock_process_private_init( fs, NULL );

      lasue = NULL;

      while( LFDS710_LIST_ASU_GET_START_AND_THEN_NEXT(tsets->list_of_per_numa_states,lasue) )
      {
        pns = LFDS710_LIST_ASU_GET_VALUE_FROM_ELEMENT( *lasue );

        /* TRD : for each NUMA node, figure out how many LPs in the current set are in that NUMA node
                 and allocate then the correct number of elements from this NUMA node (1 per LP)
        */

        lasue_lp = NULL;
        number_logical_processors_in_numa_node = 0;

        while( LFDS710_LIST_ASU_GET_START_AND_THEN_NEXT(tsets->list_of_per_thread_states,lasue_lp) )
        {
          pts = LFDS710_LIST_ASU_GET_VALUE_FROM_ELEMENT( *lasue_lp );

          libbenchmark_topology_query( ts, LIBBENCHMARK_TOPOLOGY_QUERY_GET_NUMA_NODE_FOR_LOGICAL_PROCESSOR, pts->tns_lp, &numa_node_for_lp );

          if( LIBBENCHMARK_TOPOLOGY_NODE_GET_NUMA_ID(*numa_node_for_lp) == pns->numa_node_id )
            number_logical_processors_in_numa_node++;
        }

        fe = libshared_memory_alloc_from_specific_node( ms, pns->numa_node_id, sizeof(struct libbenchmark_datastructure_freelist_pthread_spinlock_process_private_element) * number_logical_processors_in_numa_node, LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
        for( loop = 0 ; loop < number_logical_processors_in_numa_node ; loop++ )
          libbenchmark_datastructure_freelist_pthread_spinlock_process_private_push( fs, &fe[loop] );
      }

      lasue = NULL;

      while( LFDS710_LIST_ASU_GET_START_AND_THEN_NEXT(tsets->list_of_per_thread_states,lasue) )
      {
        pts = LFDS710_LIST_ASU_GET_VALUE_FROM_ELEMENT( *lasue );
        ptbs = libshared_memory_alloc_from_specific_node( ms, largest_pns->numa_node_id, sizeof(struct libbenchmark_benchmark_freelist_pthread_spinlock_process_private_push1_pop1_per_thread_benchmark_state), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
        pts->users_per_thread_state = ptbs;
      }
    break;

    case LIBBENCHMARK_TOPOLOGY_NUMA_MODE_NUMA_BUT_NOT_USED:
      /* TRD : freelist state in the NUMA node with most threads from the current set
               or, if equal threads, with lowest NUMA
               all elements alloced from that node as well
      */

      while( LFDS710_LIST_ASU_GET_START_AND_THEN_NEXT(tsets->list_of_per_numa_states,lasue) )
      {
        pns = LFDS710_LIST_ASU_GET_VALUE_FROM_ELEMENT( *lasue );

        lasue_lp = NULL;
        number_logical_processors_in_numa_node = 0;

        while( LFDS710_LIST_ASU_GET_START_AND_THEN_NEXT(tsets->list_of_per_thread_states,lasue_lp) )
        {
          pts = LFDS710_LIST_ASU_GET_VALUE_FROM_ELEMENT( *lasue_lp );

          libbenchmark_topology_query( ts, LIBBENCHMARK_TOPOLOGY_QUERY_GET_NUMA_NODE_FOR_LOGICAL_PROCESSOR, pts->tns_lp, &numa_node_for_lp );

          if( LIBBENCHMARK_TOPOLOGY_NODE_GET_NUMA_ID(*numa_node_for_lp) == pns->numa_node_id )
            number_logical_processors_in_numa_node++;
        }

        if( number_logical_processors_in_numa_node > largest_number_logical_processors_in_numa_node )
          largest_pns = pns;
      }

      fs = libshared_memory_alloc_from_specific_node( ms, largest_pns->numa_node_id, sizeof(struct libbenchmark_datastructure_freelist_pthread_spinlock_process_private_state), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
      libbenchmark_datastructure_freelist_pthread_spinlock_process_private_init( fs, NULL );

      lfds710_list_aso_query( logical_processor_set, LFDS710_LIST_ASO_QUERY_GET_POTENTIALLY_INACCURATE_COUNT, NULL, (void *) &number_logical_processors );
      fe = libshared_memory_alloc_from_specific_node( ms, largest_pns->numa_node_id, sizeof(struct libbenchmark_datastructure_freelist_pthread_spinlock_process_private_element) * number_logical_processors, LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
      for( loop = 0 ; loop < number_logical_processors ; loop++ )
        libbenchmark_datastructure_freelist_pthread_spinlock_process_private_push( fs, &fe[loop] );

      lasue = NULL;

      while( LFDS710_LIST_ASU_GET_START_AND_THEN_NEXT(tsets->list_of_per_thread_states,lasue) )
      {
        pts = LFDS710_LIST_ASU_GET_VALUE_FROM_ELEMENT( *lasue );
        ptbs = libshared_memory_alloc_from_specific_node( ms, largest_pns->numa_node_id, sizeof(struct libbenchmark_benchmark_freelist_pthread_spinlock_process_private_push1_pop1_per_thread_benchmark_state), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
        pts->users_per_thread_state = ptbs;
      }
    break;
  }

  tsets->users_threadset_state = fs;

  return;
}





/****************************************************************************/
libshared_pal_thread_return_t LIBSHARED_PAL_THREAD_CALLING_CONVENTION libbenchmark_benchmark_freelist_pthread_spinlock_process_private_push1_pop1_thread( void *libbenchmark_threadset_per_thread_state )
{
  int long long unsigned
    current_time = 0,
    end_time,
    time_units_per_second;

  lfds710_pal_uint_t
    operation_count = 0,
    time_loop = 0;

  struct libbenchmark_datastructure_freelist_pthread_spinlock_process_private_state
    *fs;

  struct libbenchmark_datastructure_freelist_pthread_spinlock_process_private_element
    *fe;

  struct libbenchmark_benchmark_freelist_pthread_spinlock_process_private_push1_pop1_per_thread_benchmark_state
    *ptbs;

  struct libbenchmark_threadset_per_thread_state
    *pts;

  LFDS710_MISC_BARRIER_LOAD;

  LFDS710_PAL_ASSERT( libbenchmark_threadset_per_thread_state != NULL );

  pts = (struct libbenchmark_threadset_per_thread_state *) libbenchmark_threadset_per_thread_state;

  ptbs = LIBBENCHMARK_THREADSET_PER_THREAD_STATE_GET_USERS_PER_THREAD_STATE( *pts );
  fs = LIBBENCHMARK_THREADSET_PER_THREAD_STATE_GET_USERS_OVERALL_STATE( *pts );

  LIBBENCHMARK_PAL_TIME_UNITS_PER_SECOND( &time_units_per_second );

  libbenchmark_threadset_thread_ready_and_wait( pts );

  LIBBENCHMARK_PAL_GET_HIGHRES_TIME( &current_time );

  end_time = current_time + time_units_per_second * libbenchmark_globals_benchmark_duration_in_seconds;

  while( current_time < end_time )
  {
    libbenchmark_datastructure_freelist_pthread_spinlock_process_private_pop( fs, &fe );
    libbenchmark_datastructure_freelist_pthread_spinlock_process_private_push( fs, fe );
    operation_count++;

    if( time_loop++ == TIME_LOOP_COUNT )
    {
      time_loop = 0;
      LIBBENCHMARK_PAL_GET_HIGHRES_TIME( &current_time );
    }
  }

  ptbs->operation_count = operation_count;

  LFDS710_MISC_BARRIER_STORE;

  lfds710_misc_force_store();

  return LIBSHARED_PAL_THREAD_RETURN_CAST(RETURN_SUCCESS);
}





/****************************************************************************/
void libbenchmark_benchmark_freelist_pthread_spinlock_process_private_push1_pop1_cleanup( struct lfds710_list_aso_state *logical_processor_set,
                                                                                          enum libbenchmark_topology_numa_mode numa_mode,
                                                                                          struct libbenchmark_results_state *rs,
                                                                                          struct libbenchmark_threadset_state *tsets )
{
  struct libbenchmark_datastructure_freelist_pthread_spinlock_process_private_state
    *fs;

  struct lfds710_list_asu_element
    *lasue = NULL;

  struct libbenchmark_benchmark_freelist_pthread_spinlock_process_private_push1_pop1_per_thread_benchmark_state
    *ptbs;

  struct libbenchmark_threadset_per_thread_state
    *pts;

  LFDS710_PAL_ASSERT( logical_processor_set != NULL );
  // TRD : numa_mode can be any value in its range
  LFDS710_PAL_ASSERT( rs != NULL );
  LFDS710_PAL_ASSERT( tsets != NULL );

  while( LFDS710_LIST_ASU_GET_START_AND_THEN_NEXT(tsets->list_of_per_thread_states,lasue) )
  {
    pts = LFDS710_LIST_ASU_GET_VALUE_FROM_ELEMENT( *lasue );

    ptbs = LIBBENCHMARK_THREADSET_PER_THREAD_STATE_GET_USERS_PER_THREAD_STATE( *pts );

    libbenchmark_results_put_result( rs,
                                     LIBBENCHMARK_DATASTRUCTURE_ID_FREELIST,
                                     LIBBENCHMARK_BENCHMARK_ID_PUSH1_THEN_POP1,
                                     LIBBENCHMARK_LOCK_ID_PTHREAD_SPINLOCK_PROCESS_PRIVATE,
                                     numa_mode,
                                     logical_processor_set,
                                     LIBBENCHMARK_TOPOLOGY_NODE_GET_LOGICAL_PROCESSOR_NUMBER( *pts->tns_lp ),
                                     LIBBENCHMARK_TOPOLOGY_NODE_GET_WINDOWS_GROUP_NUMBER( *pts->tns_lp ),
                                     ptbs->operation_count );
  }

  fs = tsets->users_threadset_state;

  libbenchmark_datastructure_freelist_pthread_spinlock_process_private_cleanup( fs, NULL );

  return;
}

