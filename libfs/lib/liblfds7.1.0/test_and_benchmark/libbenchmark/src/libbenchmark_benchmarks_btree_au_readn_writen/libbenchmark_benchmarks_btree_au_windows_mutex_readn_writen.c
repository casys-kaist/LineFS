/***** includes *****/
#include "libbenchmark_benchmarks_btree_au_internal.h"

/***** structs *****/
struct libbenchmark_benchmark_btree_au_windows_mutex_readn_writen_benchmark_element
{
  lfds710_pal_uint_t
    datum;

  struct libbenchmark_datastructure_btree_au_windows_mutex_element
    be;
};

struct libbenchmark_benchmark_btree_au_windows_mutex_readn_writen_per_thread_benchmark_state
{
  lfds710_pal_uint_t
    operation_count,
    per_thread_prng_seed;
};

struct libbenchmark_benchmark_btree_au_windows_mutex_readn_writen_per_numa_benchmark_state
{
  lfds710_pal_uint_t
    *element_key_array,
    number_element_keys;

  struct libbenchmark_benchmark_btree_au_windows_mutex_readn_writen_benchmark_element
    *bme;

  struct libbenchmark_datastructure_btree_au_windows_mutex_state
    *bs;
};

struct libbenchmark_benchmark_btree_au_windows_mutex_readn_writen_overall_benchmark_state
{
  enum libbenchmark_topology_numa_mode
    numa_mode;

  lfds710_pal_uint_t
    *element_key_array,
    number_element_keys;

  struct libbenchmark_benchmark_btree_au_windows_mutex_readn_writen_benchmark_element
    *bme;

  struct libbenchmark_datastructure_btree_au_windows_mutex_state
    *bs;
};

/***** private prototypes *****/
static int key_compare_function( void const *new_key, void const *existing_key );





/****************************************************************************/
void libbenchmark_benchmark_btree_au_windows_mutex_readn_writen_init( struct libbenchmark_topology_state *ts,
                                                                      struct lfds710_list_aso_state *logical_processor_set,
                                                                      struct libshared_memory_state *ms,
                                                                      enum libbenchmark_topology_numa_mode numa_mode,
                                                                      struct libbenchmark_threadset_state *tsets )
{
  enum libbenchmark_datastructure_btree_au_windows_mutex_insert_result
    ir;

  lfds710_pal_uint_t
    index = 0,
    loop,
    number_logical_processors,
    number_benchmark_elements,
    number_logical_processors_in_numa_node,
    largest_number_logical_processors_in_numa_node = 0,
    total_number_benchmark_elements;

  struct lfds710_list_asu_element
    *lasue = NULL,
    *lasue_lp;

  struct libbenchmark_benchmark_btree_au_windows_mutex_readn_writen_overall_benchmark_state
    *obs;

  struct libbenchmark_benchmark_btree_au_windows_mutex_readn_writen_per_numa_benchmark_state
    *ptns;

  struct libbenchmark_benchmark_btree_au_windows_mutex_readn_writen_per_thread_benchmark_state
    *ptbs;

  struct libbenchmark_datastructure_btree_au_windows_mutex_state
    *bs = NULL;

  struct libbenchmark_prng_state
    ps;

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

  lfds710_list_aso_query( logical_processor_set, LFDS710_LIST_ASO_QUERY_GET_POTENTIALLY_INACCURATE_COUNT, NULL, (void *) &number_logical_processors );

  libbenchmark_threadset_init( tsets, ts, logical_processor_set, ms, libbenchmark_benchmark_btree_au_windows_mutex_readn_writen_thread, NULL );

  total_number_benchmark_elements = number_logical_processors * 1024;

  obs = libshared_memory_alloc_from_most_free_space_node( ms, sizeof(struct libbenchmark_benchmark_btree_au_windows_mutex_readn_writen_overall_benchmark_state), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );

  LIBBENCHMARK_PRNG_INIT( ps, LFDS710_PRNG_SEED );

  switch( numa_mode )
  {
    case LIBBENCHMARK_TOPOLOGY_NUMA_MODE_SMP:
      bs = libshared_memory_alloc_from_most_free_space_node( ms, sizeof(struct libbenchmark_datastructure_btree_au_windows_mutex_state), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
      libbenchmark_datastructure_btree_au_windows_mutex_init( bs, key_compare_function, LIBBENCHMARK_DATASTRUCTURE_BTREE_AU_WINDOWS_MUTEX_EXISTING_KEY_FAIL, NULL );

      obs->bme = libshared_memory_alloc_from_most_free_space_node( ms, sizeof(struct libbenchmark_benchmark_btree_au_windows_mutex_readn_writen_benchmark_element) * total_number_benchmark_elements, LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
      obs->element_key_array = libshared_memory_alloc_from_most_free_space_node( ms, sizeof(lfds710_pal_uint_t) * total_number_benchmark_elements, sizeof(lfds710_pal_uint_t) );

      for( loop = 0 ; loop < total_number_benchmark_elements ; loop++ )
        do
        {
          LIBBENCHMARK_PRNG_GENERATE( ps, obs->bme[loop].datum );
          obs->element_key_array[loop] = obs->bme[loop].datum;

          LIBBENCHMARK_DATASTRUCTURE_BTREE_AU_WINDOWS_MUTEX_SET_KEY_IN_ELEMENT( *bs, obs->bme[loop].be, &obs->bme[loop] );
          LIBBENCHMARK_DATASTRUCTURE_BTREE_AU_WINDOWS_MUTEX_SET_VALUE_IN_ELEMENT( *bs, obs->bme[loop].be, &obs->bme[loop] );
          ir = libbenchmark_datastructure_btree_au_windows_mutex_insert( bs, &obs->bme[loop].be, NULL );
        }
        while( ir == LIBBENCHMARK_DATASTRUCTURE_BTREE_AU_WINDOWS_MUTEX_INSERT_RESULT_FAILURE_EXISTING_KEY );

      lasue = NULL;

      while( LFDS710_LIST_ASU_GET_START_AND_THEN_NEXT(tsets->list_of_per_thread_states,lasue) )
      {
        pts = LFDS710_LIST_ASU_GET_VALUE_FROM_ELEMENT( *lasue );
        ptbs = libshared_memory_alloc_from_most_free_space_node( ms, sizeof(struct libbenchmark_benchmark_btree_au_windows_mutex_readn_writen_per_thread_benchmark_state), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
        LIBBENCHMARK_PRNG_GENERATE( ps, ptbs->per_thread_prng_seed );
        pts->users_per_thread_state = ptbs;
      }
    break;

    case LIBBENCHMARK_TOPOLOGY_NUMA_MODE_NUMA:
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

      bs = libshared_memory_alloc_from_specific_node( ms, largest_pns->numa_node_id, sizeof(struct libbenchmark_datastructure_btree_au_windows_mutex_state), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
      libbenchmark_datastructure_btree_au_windows_mutex_init( bs, key_compare_function, LIBBENCHMARK_DATASTRUCTURE_BTREE_AU_WINDOWS_MUTEX_EXISTING_KEY_FAIL, NULL );

      obs->element_key_array = libshared_memory_alloc_from_most_free_space_node( ms, sizeof(lfds710_pal_uint_t) * total_number_benchmark_elements, sizeof(lfds710_pal_uint_t) );

      lasue = NULL;

      while( LFDS710_LIST_ASU_GET_START_AND_THEN_NEXT(tsets->list_of_per_numa_states,lasue) )
      {
        pns = LFDS710_LIST_ASU_GET_VALUE_FROM_ELEMENT( *lasue );

        /* TRD : for each NUMA node, figure out how many LPs in the current set are in that NUMA node
                 and allocate then the correct number of elements from this NUMA node (1024 per LP)
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

        number_benchmark_elements = number_logical_processors_in_numa_node * 1024;

        ptns = libshared_memory_alloc_from_specific_node( ms, pns->numa_node_id, sizeof(struct libbenchmark_benchmark_btree_au_windows_mutex_readn_writen_per_numa_benchmark_state), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
        ptns->element_key_array = libshared_memory_alloc_from_specific_node( ms, pns->numa_node_id, sizeof(lfds710_pal_uint_t) * total_number_benchmark_elements, sizeof(lfds710_pal_uint_t) );
        ptns->number_element_keys = total_number_benchmark_elements;

        ptns->bme = libshared_memory_alloc_from_specific_node( ms, pns->numa_node_id, sizeof(struct libbenchmark_benchmark_btree_au_windows_mutex_readn_writen_benchmark_element) * number_benchmark_elements, LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );

        for( loop = 0 ; loop < number_benchmark_elements ; loop++, index++ )
          do
          {
            LIBBENCHMARK_PRNG_GENERATE( ps, ptns->bme[loop].datum );
            obs->element_key_array[index] = ptns->bme[loop].datum;

            LIBBENCHMARK_DATASTRUCTURE_BTREE_AU_WINDOWS_MUTEX_SET_KEY_IN_ELEMENT( *bs, ptns->bme[loop].be, &ptns->bme[loop] );
            LIBBENCHMARK_DATASTRUCTURE_BTREE_AU_WINDOWS_MUTEX_SET_VALUE_IN_ELEMENT( *bs, ptns->bme[loop].be, &ptns->bme[loop] );
            ir = libbenchmark_datastructure_btree_au_windows_mutex_insert( bs, &ptns->bme[loop].be, NULL );
          }
          while( ir == LIBBENCHMARK_DATASTRUCTURE_BTREE_AU_WINDOWS_MUTEX_INSERT_RESULT_FAILURE_EXISTING_KEY );

        pns->users_per_numa_state = ptns;
      }

      // TRD : now copy over into each NUMA node state the element_key_array
      lasue = NULL;

      while( LFDS710_LIST_ASU_GET_START_AND_THEN_NEXT(tsets->list_of_per_numa_states,lasue) )
      {
        pns = LFDS710_LIST_ASU_GET_VALUE_FROM_ELEMENT( *lasue );
        ptns = pns->users_per_numa_state;

        for( loop = 0 ; loop < total_number_benchmark_elements ; loop++ )
          ptns->element_key_array[loop] = obs->element_key_array[loop];         
      }

      lasue = NULL;

      while( LFDS710_LIST_ASU_GET_START_AND_THEN_NEXT(tsets->list_of_per_thread_states,lasue) )
      {
        pts = LFDS710_LIST_ASU_GET_VALUE_FROM_ELEMENT( *lasue );
        ptbs = libshared_memory_alloc_from_most_free_space_node( ms, sizeof(struct libbenchmark_benchmark_btree_au_windows_mutex_readn_writen_per_thread_benchmark_state), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
        LIBBENCHMARK_PRNG_GENERATE( ps, ptbs->per_thread_prng_seed );
        LIBBENCHMARK_PRNG_MURMURHASH3_MIXING_FUNCTION( ptbs->per_thread_prng_seed );
        pts->users_per_thread_state = ptbs;
      }
    break;

    case LIBBENCHMARK_TOPOLOGY_NUMA_MODE_NUMA_BUT_NOT_USED:
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

      bs = libshared_memory_alloc_from_specific_node( ms, largest_pns->numa_node_id, sizeof(struct libbenchmark_datastructure_btree_au_windows_mutex_state), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
      libbenchmark_datastructure_btree_au_windows_mutex_init( bs, key_compare_function, LIBBENCHMARK_DATASTRUCTURE_BTREE_AU_WINDOWS_MUTEX_EXISTING_KEY_FAIL, NULL );

      obs->element_key_array = libshared_memory_alloc_from_most_free_space_node( ms, sizeof(lfds710_pal_uint_t) * total_number_benchmark_elements, sizeof(lfds710_pal_uint_t) );

      lasue = NULL;

      while( LFDS710_LIST_ASU_GET_START_AND_THEN_NEXT(tsets->list_of_per_numa_states,lasue) )
      {
        pns = LFDS710_LIST_ASU_GET_VALUE_FROM_ELEMENT( *lasue );

        /* TRD : for each NUMA node, figure out how many LPs in the current set are in that NUMA node
                 and allocate then the correct number of elements from this NUMA node (1024 per LP)
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

        number_benchmark_elements = number_logical_processors_in_numa_node * 1024;

        ptns = libshared_memory_alloc_from_specific_node( ms, pns->numa_node_id, sizeof(struct libbenchmark_benchmark_btree_au_windows_mutex_readn_writen_per_numa_benchmark_state), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
        ptns->element_key_array = libshared_memory_alloc_from_specific_node( ms, pns->numa_node_id, sizeof(lfds710_pal_uint_t) * total_number_benchmark_elements, sizeof(lfds710_pal_uint_t) );
        ptns->number_element_keys = total_number_benchmark_elements;

        // TRD : everyone stores their elements in the same NUMA node
        ptns->bme = libshared_memory_alloc_from_specific_node( ms, largest_pns->numa_node_id, sizeof(struct libbenchmark_benchmark_btree_au_windows_mutex_readn_writen_benchmark_element) * number_benchmark_elements, LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );

        for( loop = 0 ; loop < number_benchmark_elements ; loop++, index++ )
          do
          {
            LIBBENCHMARK_PRNG_GENERATE( ps, ptns->bme[loop].datum );
            obs->element_key_array[index] = ptns->bme[loop].datum;

            LIBBENCHMARK_DATASTRUCTURE_BTREE_AU_WINDOWS_MUTEX_SET_KEY_IN_ELEMENT( *bs, ptns->bme[loop].be, &ptns->bme[loop] );
            LIBBENCHMARK_DATASTRUCTURE_BTREE_AU_WINDOWS_MUTEX_SET_VALUE_IN_ELEMENT( *bs, ptns->bme[loop].be, &ptns->bme[loop] );
            ir = libbenchmark_datastructure_btree_au_windows_mutex_insert( bs, &ptns->bme[loop].be, NULL );
          }
          while( ir == LIBBENCHMARK_DATASTRUCTURE_BTREE_AU_WINDOWS_MUTEX_INSERT_RESULT_FAILURE_EXISTING_KEY );

        pns->users_per_numa_state = ptns;
      }

      // TRD : now copy over into each NUMA node state the element_key_array
      lasue = NULL;

      while( LFDS710_LIST_ASU_GET_START_AND_THEN_NEXT(tsets->list_of_per_numa_states,lasue) )
      {
        pns = LFDS710_LIST_ASU_GET_VALUE_FROM_ELEMENT( *lasue );
        ptns = pns->users_per_numa_state;

        for( loop = 0 ; loop < total_number_benchmark_elements ; loop++ )
          ptns->element_key_array[loop] = obs->element_key_array[loop];         
      }

      lasue = NULL;

      while( LFDS710_LIST_ASU_GET_START_AND_THEN_NEXT(tsets->list_of_per_thread_states,lasue) )
      {
        pts = LFDS710_LIST_ASU_GET_VALUE_FROM_ELEMENT( *lasue );
        ptbs = libshared_memory_alloc_from_most_free_space_node( ms, sizeof(struct libbenchmark_benchmark_btree_au_windows_mutex_readn_writen_per_thread_benchmark_state), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
        LIBBENCHMARK_PRNG_GENERATE( ps, ptbs->per_thread_prng_seed );
        LIBBENCHMARK_PRNG_MURMURHASH3_MIXING_FUNCTION( ptbs->per_thread_prng_seed );
        pts->users_per_thread_state = ptbs;
      }
    break;
  }

  obs->number_element_keys = total_number_benchmark_elements;
  obs->bs = bs;
  obs->numa_mode = numa_mode;

  tsets->users_threadset_state = obs;

  return;
}





/****************************************************************************/
libshared_pal_thread_return_t LIBSHARED_PAL_THREAD_CALLING_CONVENTION libbenchmark_benchmark_btree_au_windows_mutex_readn_writen_thread( void *libbenchmark_threadset_per_thread_state )
{
  int long long unsigned
    current_time = 0,
    end_time,
    time_units_per_second;

  lfds710_pal_uint_t
    *element_key_array = NULL,
    index,
    number_element_keys = 0,
    operation_count = 0,
    random_value,
    time_loop = 0;

  struct libbenchmark_prng_state
    ps;

  struct libbenchmark_datastructure_btree_au_windows_mutex_element
    *existing_be;

  struct libbenchmark_datastructure_btree_au_windows_mutex_state
    *bs;

  struct libbenchmark_benchmark_btree_au_windows_mutex_readn_writen_per_thread_benchmark_state
    *ptbs;

  struct libbenchmark_benchmark_btree_au_windows_mutex_readn_writen_per_numa_benchmark_state
    *pnbs = NULL;

  struct libbenchmark_benchmark_btree_au_windows_mutex_readn_writen_overall_benchmark_state
    *obs;

  struct libbenchmark_threadset_per_thread_state
    *pts;

  LFDS710_MISC_BARRIER_LOAD;

  LFDS710_PAL_ASSERT( libbenchmark_threadset_per_thread_state != NULL );

  pts = (struct libbenchmark_threadset_per_thread_state *) libbenchmark_threadset_per_thread_state;

  ptbs = LIBBENCHMARK_THREADSET_PER_THREAD_STATE_GET_USERS_PER_THREAD_STATE( *pts );
  obs = LIBBENCHMARK_THREADSET_PER_THREAD_STATE_GET_USERS_OVERALL_STATE( *pts );
  if( obs->numa_mode == LIBBENCHMARK_TOPOLOGY_NUMA_MODE_NUMA or obs->numa_mode == LIBBENCHMARK_TOPOLOGY_NUMA_MODE_NUMA_BUT_NOT_USED )
    pnbs = LIBBENCHMARK_THREADSET_PER_THREAD_STATE_GET_USERS_PER_NUMA_STATE( *pts );

  bs = obs->bs;

  LIBBENCHMARK_PRNG_INIT( ps, ptbs->per_thread_prng_seed );

  LIBBENCHMARK_PAL_TIME_UNITS_PER_SECOND( &time_units_per_second );

  switch( obs->numa_mode )
  {
    case LIBBENCHMARK_TOPOLOGY_NUMA_MODE_SMP:
      element_key_array = obs->element_key_array;
      number_element_keys = obs->number_element_keys;
    break;

    case LIBBENCHMARK_TOPOLOGY_NUMA_MODE_NUMA:
      element_key_array = pnbs->element_key_array;
      number_element_keys = pnbs->number_element_keys;
    break;

    case LIBBENCHMARK_TOPOLOGY_NUMA_MODE_NUMA_BUT_NOT_USED:
      element_key_array = pnbs->element_key_array;
      number_element_keys = pnbs->number_element_keys;
    break;
  }

  libbenchmark_threadset_thread_ready_and_wait( pts );

  LIBBENCHMARK_PAL_GET_HIGHRES_TIME( &current_time );

  end_time = current_time + time_units_per_second * libbenchmark_globals_benchmark_duration_in_seconds;

  while( current_time < end_time )
  {
    LIBBENCHMARK_PRNG_GENERATE( ps, random_value );
    index = random_value % number_element_keys;

    // TRD : read
    libbenchmark_datastructure_btree_au_windows_mutex_get_by_key( bs, NULL, &element_key_array[index], &existing_be );
    // LIBBENCHMARK_DATASTRUCTURE_BTREE_AU_WINDOWS_MUTEX_GET_VALUE_FROM_ELEMENT( benchmark_state->bs, *existing_be, datum );

    LIBBENCHMARK_PRNG_GENERATE( ps, random_value );
    index = random_value % number_element_keys;

    // TRD : write
    libbenchmark_datastructure_btree_au_windows_mutex_get_by_key( bs, NULL, &element_key_array[index], &existing_be );
    // LIBBENCHMARK_DATASTRUCTURE_BTREE_AU_WINDOWS_MUTEX_SET_VALUE_IN_ELEMENT( benchmark_state->bs, *existing_be, benchmark_state->benchmark_element_array[index].datum );

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
void libbenchmark_benchmark_btree_au_windows_mutex_readn_writen_cleanup( struct lfds710_list_aso_state *logical_processor_set,
                                                                         enum libbenchmark_topology_numa_mode numa_mode,
                                                                         struct libbenchmark_results_state *rs,
                                                                         struct libbenchmark_threadset_state *tsets )
{
  struct lfds710_list_asu_element
    *lasue = NULL;

  struct libbenchmark_benchmark_btree_au_windows_mutex_readn_writen_overall_benchmark_state
    *obs;

  struct libbenchmark_benchmark_btree_au_windows_mutex_readn_writen_per_thread_benchmark_state
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
                                     LIBBENCHMARK_DATASTRUCTURE_ID_BTREE_AU,
                                     LIBBENCHMARK_BENCHMARK_ID_READN_THEN_WRITEN,
                                     LIBBENCHMARK_LOCK_ID_WINDOWS_MUTEX,
                                     numa_mode,
                                     logical_processor_set,
                                     LIBBENCHMARK_TOPOLOGY_NODE_GET_LOGICAL_PROCESSOR_NUMBER( *pts->tns_lp ),
                                     LIBBENCHMARK_TOPOLOGY_NODE_GET_WINDOWS_GROUP_NUMBER( *pts->tns_lp ),
                                     ptbs->operation_count );
  }

  obs = tsets->users_threadset_state;;

  libbenchmark_datastructure_btree_au_windows_mutex_cleanup( obs->bs, NULL );

  return;
}





/****************************************************************************/
static int key_compare_function( void const *new_key, void const *existing_key )
{
  struct libbenchmark_benchmark_btree_au_windows_mutex_readn_writen_benchmark_element
    *new_benchmark_element,
    *existing_benchmark_element;

  LFDS710_PAL_ASSERT( new_key != NULL );
  LFDS710_PAL_ASSERT( existing_key != NULL );

  new_benchmark_element = (struct libbenchmark_benchmark_btree_au_windows_mutex_readn_writen_benchmark_element *) new_key;
  existing_benchmark_element = (struct libbenchmark_benchmark_btree_au_windows_mutex_readn_writen_benchmark_element *) existing_key;

  if( new_benchmark_element->datum < existing_benchmark_element->datum )
    return -1;

  if( new_benchmark_element->datum > existing_benchmark_element->datum )
    return 1;

  return 0;
}

