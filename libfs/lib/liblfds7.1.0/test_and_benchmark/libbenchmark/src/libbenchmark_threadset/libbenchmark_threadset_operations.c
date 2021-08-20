/***** includes *****/
#include "libbenchmark_threadset_internal.h"





/****************************************************************************/
void libbenchmark_threadset_run( struct libbenchmark_threadset_state *tsets )
{
  struct lfds710_list_asu_element
    *lasue = NULL;

  struct libbenchmark_threadset_per_thread_state
    *pts;

  LFDS710_PAL_ASSERT( tsets != NULL );

  LFDS710_MISC_BARRIER_STORE;

  lfds710_misc_force_store();

  while( LFDS710_LIST_ASU_GET_START_AND_THEN_NEXT(tsets->list_of_per_thread_states,lasue) )
  {
    pts = LFDS710_LIST_ASU_GET_VALUE_FROM_ELEMENT( *lasue );

    pts->pti.logical_processor_number = LIBBENCHMARK_TOPOLOGY_NODE_GET_LOGICAL_PROCESSOR_NUMBER( *pts->tns_lp );
    pts->pti.windows_processor_group_number = LIBBENCHMARK_TOPOLOGY_NODE_GET_WINDOWS_GROUP_NUMBER( *pts->tns_lp );
    pts->pti.thread_function = tsets->thread_function;
    pts->pti.thread_argument = pts;

    libshared_pal_thread_start( &pts->thread_handle, &pts->pti );
  }

  tsets->threadset_start_flag = RAISED;

  LFDS710_PAL_ASSERT( tsets != NULL );

  while( LFDS710_LIST_ASU_GET_START_AND_THEN_NEXT(tsets->list_of_per_thread_states,lasue) )
  {
    pts = LFDS710_LIST_ASU_GET_VALUE_FROM_ELEMENT( *lasue );
    libshared_pal_thread_wait( pts->thread_handle );
  }

  return;
}





/****************************************************************************/
void libbenchmark_threadset_thread_ready_and_wait( struct libbenchmark_threadset_per_thread_state *pts )
{
  LFDS710_PAL_ASSERT( pts != NULL );

  pts->thread_ready_flag = RAISED;

  LFDS710_MISC_BARRIER_FULL;

  while( *pts->threadset_start_flag == LOWERED )
    LFDS710_MISC_BARRIER_LOAD;

  return;
}

