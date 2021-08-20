/***** includes *****/
#include "libbenchmark_threadset_internal.h"





/****************************************************************************/
void libbenchmark_threadset_init( struct libbenchmark_threadset_state *tsets,
                                  struct libbenchmark_topology_state *ts,
                                  struct lfds710_list_aso_state *logical_processor_set,
                                  struct libshared_memory_state *ms,
                                  libshared_pal_thread_return_t (LIBSHARED_PAL_THREAD_CALLING_CONVENTION *thread_function)( void *thread_user_state ),
                                  void *users_threadset_state )
{
  struct lfds710_list_aso_element
    *lasoe = NULL;

  struct lfds710_list_asu_element
    *lasue = NULL;

  struct libbenchmark_threadset_per_numa_state
    *pns = NULL;

  struct libbenchmark_threadset_per_thread_state
    *pts;

  struct libbenchmark_topology_node_state
    *tns,
    *tns_numa_node;

  LFDS710_PAL_ASSERT( tsets != NULL );
  LFDS710_PAL_ASSERT( ts != NULL );
  LFDS710_PAL_ASSERT( logical_processor_set != NULL );
  LFDS710_PAL_ASSERT( ms != NULL );
  LFDS710_PAL_ASSERT( thread_function != NULL );
  // TRD : users_threadset_state can be NULL

  tsets->threadset_start_flag = LOWERED;

  tsets->thread_function = thread_function;
  tsets->users_threadset_state = users_threadset_state;
  lfds710_list_asu_init_valid_on_current_logical_core( &tsets->list_of_per_numa_states, NULL );
  lfds710_list_asu_init_valid_on_current_logical_core( &tsets->list_of_per_thread_states, NULL );

  /* TRD : loop over the logical_processor_set
           make a thread_state for each
           and make a NUMA node state for each unique NUMA node
  */

  while( LFDS710_LIST_ASO_GET_START_AND_THEN_NEXT(*logical_processor_set,lasoe) )
  {
    tns = LFDS710_LIST_ASO_GET_VALUE_FROM_ELEMENT( *lasoe );

    // TRD : first, make a NUMA node entry, if we need to

    libbenchmark_topology_query( ts, LIBBENCHMARK_TOPOLOGY_QUERY_GET_NUMA_NODE_FOR_LOGICAL_PROCESSOR, tns, &tns_numa_node );

    if( tns_numa_node != NULL )
    {
      lasue = NULL;

      while( LFDS710_LIST_ASU_GET_START_AND_THEN_NEXT(tsets->list_of_per_numa_states,lasue) )
      {
        pns = LFDS710_LIST_ASO_GET_VALUE_FROM_ELEMENT( *lasue );

        if( pns->numa_node_id == LIBBENCHMARK_TOPOLOGY_NODE_GET_NUMA_ID(*tns_numa_node) )
          break;
      }

      if( lasue == NULL )
      {
        pns = libshared_memory_alloc_from_specific_node( ms, LIBBENCHMARK_TOPOLOGY_NODE_GET_NUMA_ID(*tns_numa_node), sizeof(struct libbenchmark_threadset_per_numa_state), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );

        pns->numa_node_id = LIBBENCHMARK_TOPOLOGY_NODE_GET_NUMA_ID( *tns_numa_node );
        pns->users_per_numa_state = NULL;
        LFDS710_LIST_ASU_SET_VALUE_IN_ELEMENT( pns->lasue, pns );
        lfds710_list_asu_insert_at_start( &tsets->list_of_per_numa_states, &pns->lasue );
      }
    }

    // TRD : now make a thread entry (pns now points at the correct NUMA node entry)
    if( pns == NULL )
      pts = libshared_memory_alloc_from_most_free_space_node( ms, sizeof(struct libbenchmark_threadset_per_thread_state), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
    else
      pts = libshared_memory_alloc_from_specific_node( ms, pns->numa_node_id, sizeof(struct libbenchmark_threadset_per_thread_state), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );

    pts->thread_ready_flag = LOWERED;
    pts->threadset_start_flag = &tsets->threadset_start_flag;
    pts->tns_lp = tns;
    pts->numa_node_state = pns; // TRD : pns is NULL on SMP
    pts->threadset_state = tsets;
    pts->users_per_thread_state = NULL;

    LFDS710_LIST_ASU_SET_VALUE_IN_ELEMENT( pts->lasue, pts );
    lfds710_list_asu_insert_at_start( &tsets->list_of_per_thread_states, &pts->lasue );
  }

  LFDS710_MISC_BARRIER_STORE;

  lfds710_misc_force_store();

  return;
}

