/***** includes *****/
#include "libbenchmark_benchmarks_freelist_internal.h"

/***** structs *****/
struct libbenchmark_benchmark_freelist_liblfds710_lockfree_push1_pop1_per_thread_benchmark_state
{
  lfds710_pal_uint_t
    operation_count;

  struct lfds710_prng_st_state
    psts;
};

struct libbenchmark_benchmark_freelist_liblfds710_lockfree_push1_pop1_overall_benchmark_state
{
  struct lfds710_freelist_state
    *fs;
};





/****************************************************************************/
void libbenchmark_benchmark_freelist_liblfds710_lockfree_push1_pop1_init( struct libbenchmark_topology_state *ts,
                                                                       struct lfds710_list_aso_state *logical_processor_set,
                                                                       struct libshared_memory_state *ms,
                                                                       enum libbenchmark_topology_numa_mode numa_mode,
                                                                       struct libbenchmark_threadset_state *tsets )
{
  enum flag
    finished_flag = LOWERED;

  lfds710_pal_uint_t
    ea_size_in_freelist_elements,
    *fe_array_count,
    index = 0,
    loop,
    number_freelist_elements,
    number_logical_processors,
    number_logical_processors_in_numa_node,
    number_numa_nodes,
    number_freelist_element_pointers_per_atomic_isolation = LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES / sizeof(struct lfds710_freelist_element *),
    largest_number_logical_processors_in_numa_node = 0,
    random_value,
    smallest_power_of_two_larger_than_or_equal_to_number_logical_processors = 2,
    temp_number_logical_processors;

  struct lfds710_freelist_element * volatile
    (*ea)[LFDS710_FREELIST_ELIMINATION_ARRAY_ELEMENT_SIZE_IN_FREELIST_ELEMENTS];

  struct lfds710_list_asu_element
    *lasue = NULL,
    *lasue_lp = NULL;

  struct lfds710_prng_st_state
    psts;

  struct libbenchmark_benchmark_freelist_liblfds710_lockfree_push1_pop1_overall_benchmark_state
    *obs;

  struct libbenchmark_benchmark_freelist_liblfds710_lockfree_push1_pop1_per_thread_benchmark_state
    *ptbs;

  struct lfds710_freelist_element
    *fe,
    **fe_array_pointers;

  struct lfds710_freelist_state
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

  lfds710_prng_st_init( &psts, LFDS710_PRNG_SEED );

  obs = libshared_memory_alloc_from_most_free_space_node( ms, sizeof(struct libbenchmark_benchmark_freelist_liblfds710_lockfree_push1_pop1_overall_benchmark_state), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );

  libbenchmark_threadset_init( tsets, ts, logical_processor_set, ms, libbenchmark_benchmark_freelist_liblfds710_lockfree_push1_pop1_thread, NULL );

  switch( numa_mode )
  {
    case LIBBENCHMARK_TOPOLOGY_NUMA_MODE_SMP:
      lfds710_list_aso_query( logical_processor_set, LFDS710_LIST_ASO_QUERY_GET_POTENTIALLY_INACCURATE_COUNT, NULL, (void *) &number_logical_processors );
      temp_number_logical_processors = number_logical_processors >> 2;
      while( temp_number_logical_processors != 0 )
      {
        temp_number_logical_processors >>= 1;
        smallest_power_of_two_larger_than_or_equal_to_number_logical_processors <<= 1;
      }
      fs = libshared_memory_alloc_from_most_free_space_node( ms, sizeof(struct lfds710_freelist_state), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
      ea = libshared_memory_alloc_from_unknown_node( ms, sizeof(struct lfds710_freelist_element *) * LFDS710_FREELIST_ELIMINATION_ARRAY_ELEMENT_SIZE_IN_FREELIST_ELEMENTS * smallest_power_of_two_larger_than_or_equal_to_number_logical_processors, LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
      lfds710_freelist_init_valid_on_current_logical_core( fs, ea, smallest_power_of_two_larger_than_or_equal_to_number_logical_processors, NULL );

      // TRD : fill the elimination array and have one element per thread in the freelist proper
      number_freelist_elements = (smallest_power_of_two_larger_than_or_equal_to_number_logical_processors * number_freelist_element_pointers_per_atomic_isolation) + number_logical_processors;
      fe = libshared_memory_alloc_from_most_free_space_node( ms, sizeof(struct lfds710_freelist_element) * number_freelist_elements, LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
      for( loop = 0 ; loop < number_freelist_elements ; loop++ )
        lfds710_freelist_push( fs, &fe[loop], &psts );
      while( LFDS710_LIST_ASU_GET_START_AND_THEN_NEXT(tsets->list_of_per_thread_states,lasue) )
      {
        pts = LFDS710_LIST_ASU_GET_VALUE_FROM_ELEMENT( *lasue );
        ptbs = libshared_memory_alloc_from_most_free_space_node( ms, sizeof(struct libbenchmark_benchmark_freelist_liblfds710_lockfree_push1_pop1_per_thread_benchmark_state), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
        LFDS710_PRNG_ST_GENERATE( psts, random_value );
        LFDS710_PRNG_ST_MIXING_FUNCTION( random_value );
        lfds710_prng_st_init( &ptbs->psts, random_value );
        pts->users_per_thread_state = ptbs;
      }
    break;

    case LIBBENCHMARK_TOPOLOGY_NUMA_MODE_NUMA:
      /* TRD : init the freelist from the NUMA node with most processors from the current set
               or, if equal threads, with lowest NUMA
               iterate over the NUMA node list
               for each NUMA node, allocate one freelist element per thread on that node
               and push those elements onto the freelist

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

      lfds710_list_aso_query( logical_processor_set, LFDS710_LIST_ASO_QUERY_GET_POTENTIALLY_INACCURATE_COUNT, NULL, (void *) &number_logical_processors );
      temp_number_logical_processors = number_logical_processors >> 2;
      while( temp_number_logical_processors != 0 )
      {
        temp_number_logical_processors >>= 1;
        smallest_power_of_two_larger_than_or_equal_to_number_logical_processors <<= 1;
      }

      fs = libshared_memory_alloc_from_specific_node( ms, largest_pns->numa_node_id, sizeof(struct lfds710_freelist_state), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
      ea = libshared_memory_alloc_from_specific_node( ms, largest_pns->numa_node_id, sizeof(struct lfds710_freelist_element *) * LFDS710_FREELIST_ELIMINATION_ARRAY_ELEMENT_SIZE_IN_FREELIST_ELEMENTS * smallest_power_of_two_larger_than_or_equal_to_number_logical_processors, LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
      lfds710_freelist_init_valid_on_current_logical_core( fs, ea, smallest_power_of_two_larger_than_or_equal_to_number_logical_processors, NULL );

      /* TRD : now figure out how many elements are needed from each NUMA node
               allocate them all
               them push them interleaved, round-robin, to the freelist
      */

      libbenchmark_topology_query( ts, LIBBENCHMARK_TOPOLOGY_QUERY_GET_NUMBER_OF_NODE_TYPE, (void *) (lfds710_pal_uint_t) LIBBENCHMARK_TOPOLOGY_NODE_TYPE_NUMA, (void *) &number_numa_nodes );

      fe_array_pointers = libshared_memory_alloc_from_most_free_space_node( ms, sizeof(struct lfds710_freelist_element *) * number_numa_nodes, LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
      fe_array_count = libshared_memory_alloc_from_most_free_space_node( ms, sizeof(lfds710_pal_uint_t) * number_numa_nodes, LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
      for( loop = 0 ; loop < number_numa_nodes ; loop++ )
        fe_array_count[loop] = 0;

      // TRD : now query the freelist for the EL size
      lfds710_freelist_query( fs, LFDS710_FREELIST_QUERY_GET_ELIMINATION_ARRAY_EXTRA_ELEMENTS_IN_FREELIST_ELEMENTS, NULL, (void *) &ea_size_in_freelist_elements );

      // TRD : we need to divide that number of elements over the NUMA nodes in proportion to their number of LPs...

      lasue = NULL;

      while( LFDS710_LIST_ASU_GET_START_AND_THEN_NEXT(tsets->list_of_per_numa_states,lasue) )
      {
        pns = LFDS710_LIST_ASU_GET_VALUE_FROM_ELEMENT( *lasue );

        /* TRD : for each NUMA node, figure out how many LPs in the current set are in that NUMA node
                 and allocate then the correct number of elements from this NUMA node (1 per LP, plus for the node the correct proportion of the EA layer)
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

        // TRD : blind +1 to deal with rounding, it will skew results but only slightly
        fe_array_count[index] = number_logical_processors_in_numa_node + (ea_size_in_freelist_elements * number_logical_processors_in_numa_node) / number_logical_processors + 1;
        fe_array_pointers[index] = libshared_memory_alloc_from_specific_node( ms, pns->numa_node_id, sizeof(struct lfds710_freelist_element) * fe_array_count[index], LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
        index++;
      }

      while( finished_flag == LOWERED )
      {
        for( loop = 0 ; loop < index ; loop++ )
          if( fe_array_count[loop] > 0 )
            lfds710_freelist_push( fs, &fe_array_pointers[loop][ fe_array_count[loop]-- ], &psts );

        finished_flag = RAISED;

        for( loop = 0 ; loop < index ; loop++ )
          if( fe_array_count[loop] > 0 )
            finished_flag = LOWERED;
      }

      lasue = NULL;

      while( LFDS710_LIST_ASU_GET_START_AND_THEN_NEXT(tsets->list_of_per_thread_states,lasue) )
      {
        pts = LFDS710_LIST_ASU_GET_VALUE_FROM_ELEMENT( *lasue );
        ptbs = libshared_memory_alloc_from_specific_node( ms, largest_pns->numa_node_id, sizeof(struct libbenchmark_benchmark_freelist_liblfds710_lockfree_push1_pop1_per_thread_benchmark_state), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
        LFDS710_PRNG_ST_GENERATE( psts, random_value );
        LFDS710_PRNG_ST_MIXING_FUNCTION( random_value );
        lfds710_prng_st_init( &ptbs->psts, random_value );
        pts->users_per_thread_state = ptbs;
      }
    break;

    case LIBBENCHMARK_TOPOLOGY_NUMA_MODE_NUMA_BUT_NOT_USED:
      /* TRD : freelist state in the NUMA node with most threads from the current set
               or, if equal threads, with lowest NUMA
               all elements alloced from that node as well

               SO much easier to figure out allocs than with NUMA OMG
               all of this code needs rewriting
               and the NUMA-but-not-used stuff is interesting but I don't think it carries its own weight
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

      lfds710_list_aso_query( logical_processor_set, LFDS710_LIST_ASO_QUERY_GET_POTENTIALLY_INACCURATE_COUNT, NULL, (void *) &number_logical_processors );
      temp_number_logical_processors = number_logical_processors >> 2;
      while( temp_number_logical_processors != 0 )
      {
        temp_number_logical_processors >>= 1;
        smallest_power_of_two_larger_than_or_equal_to_number_logical_processors <<= 1;
      }

      fs = libshared_memory_alloc_from_specific_node( ms, largest_pns->numa_node_id, sizeof(struct lfds710_freelist_state), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
      ea = libshared_memory_alloc_from_specific_node( ms, largest_pns->numa_node_id, sizeof(struct lfds710_freelist_element *) * LFDS710_FREELIST_ELIMINATION_ARRAY_ELEMENT_SIZE_IN_FREELIST_ELEMENTS * smallest_power_of_two_larger_than_or_equal_to_number_logical_processors, LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
      lfds710_freelist_init_valid_on_current_logical_core( fs, ea, smallest_power_of_two_larger_than_or_equal_to_number_logical_processors, NULL );

      // TRD : fill the elimination array and have one element per thread in the freelist proper
      number_freelist_elements = (smallest_power_of_two_larger_than_or_equal_to_number_logical_processors * number_freelist_element_pointers_per_atomic_isolation) + number_logical_processors;
      fe = libshared_memory_alloc_from_specific_node( ms, largest_pns->numa_node_id, sizeof(struct lfds710_freelist_element) * number_freelist_elements, LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
      for( loop = 0 ; loop < number_freelist_elements ; loop++ )
        lfds710_freelist_push( fs, &fe[loop], &psts );

      lasue = NULL;

      while( LFDS710_LIST_ASU_GET_START_AND_THEN_NEXT(tsets->list_of_per_thread_states,lasue) )
      {
        pts = LFDS710_LIST_ASU_GET_VALUE_FROM_ELEMENT( *lasue );
        ptbs = libshared_memory_alloc_from_specific_node( ms, largest_pns->numa_node_id, sizeof(struct libbenchmark_benchmark_freelist_liblfds710_lockfree_push1_pop1_per_thread_benchmark_state), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
        LFDS710_PRNG_ST_GENERATE( psts, random_value );
        LFDS710_PRNG_ST_MIXING_FUNCTION( random_value );
        lfds710_prng_st_init( &ptbs->psts, random_value );
        pts->users_per_thread_state = ptbs;
      }
    break;
  }

  obs->fs = fs;
  tsets->users_threadset_state = obs;

  return;
}





/****************************************************************************/
libshared_pal_thread_return_t LIBSHARED_PAL_THREAD_CALLING_CONVENTION libbenchmark_benchmark_freelist_liblfds710_lockfree_push1_pop1_thread( void *libbenchmark_threadset_per_thread_state )
{
  int long long unsigned
    current_time = 0,
    end_time,
    time_units_per_second;

  lfds710_pal_uint_t
    operation_count = 0,
    time_loop = 0;

  struct lfds710_freelist_state
    *fs;

  struct lfds710_freelist_element
    *fe;

  struct libbenchmark_benchmark_freelist_liblfds710_lockfree_push1_pop1_overall_benchmark_state
    *obs;

  struct libbenchmark_benchmark_freelist_liblfds710_lockfree_push1_pop1_per_thread_benchmark_state
    *ptbs;

  struct libbenchmark_threadset_per_thread_state
    *pts;

  LFDS710_MISC_BARRIER_LOAD;

  LFDS710_PAL_ASSERT( libbenchmark_threadset_per_thread_state != NULL );

  pts = (struct libbenchmark_threadset_per_thread_state *) libbenchmark_threadset_per_thread_state;

  ptbs = LIBBENCHMARK_THREADSET_PER_THREAD_STATE_GET_USERS_PER_THREAD_STATE( *pts );
  obs = LIBBENCHMARK_THREADSET_PER_THREAD_STATE_GET_USERS_OVERALL_STATE( *pts );
  fs = obs->fs;

  LIBBENCHMARK_PAL_TIME_UNITS_PER_SECOND( &time_units_per_second );

  libbenchmark_threadset_thread_ready_and_wait( pts );

  LIBBENCHMARK_PAL_GET_HIGHRES_TIME( &current_time );

  end_time = current_time + time_units_per_second * libbenchmark_globals_benchmark_duration_in_seconds;

  while( current_time < end_time )
  {
    lfds710_freelist_pop( fs, &fe, &ptbs->psts );
    lfds710_freelist_push( fs, fe, &ptbs->psts );
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
void libbenchmark_benchmark_freelist_liblfds710_lockfree_push1_pop1_cleanup( struct lfds710_list_aso_state *logical_processor_set,
                                                                          enum libbenchmark_topology_numa_mode numa_mode,
                                                                          struct libbenchmark_results_state *rs,
                                                                          struct libbenchmark_threadset_state *tsets )
{
  struct lfds710_list_asu_element
    *lasue = NULL;

  struct libbenchmark_benchmark_freelist_liblfds710_lockfree_push1_pop1_overall_benchmark_state
    *obs;

  struct libbenchmark_benchmark_freelist_liblfds710_lockfree_push1_pop1_per_thread_benchmark_state
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
                                     LIBBENCHMARK_LOCK_ID_LIBLFDS710_LOCKFREE,
                                     numa_mode,
                                     logical_processor_set,
                                     LIBBENCHMARK_TOPOLOGY_NODE_GET_LOGICAL_PROCESSOR_NUMBER( *pts->tns_lp ),
                                     LIBBENCHMARK_TOPOLOGY_NODE_GET_WINDOWS_GROUP_NUMBER( *pts->tns_lp ),
                                     ptbs->operation_count );
  }

  obs = tsets->users_threadset_state;

  lfds710_freelist_cleanup( obs->fs, NULL );

  return;
}


