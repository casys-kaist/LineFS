/***** includes *****/
#include "libshared_memory_internal.h"





/****************************************************************************/
void libshared_memory_set_rollback( struct libshared_memory_state *ms )
{
  struct lfds710_list_asu_element
    *lasue = NULL;

  struct libshared_memory_element
    *me;

  LFDS710_PAL_ASSERT( ms != NULL );

  while( LFDS710_LIST_ASU_GET_START_AND_THEN_NEXT(ms->list_of_allocations,lasue) )
  {
    me = LFDS710_LIST_ASU_GET_VALUE_FROM_ELEMENT( *lasue );

    me->rollback = me->current_pointer;
    me->rollback_memory_size_in_bytes = me->current_memory_size_in_bytes;
  }

  return;
}





/****************************************************************************/
void libshared_memory_rollback( struct libshared_memory_state *ms )
{
  struct lfds710_list_asu_element
    *lasue = NULL;

  struct libshared_memory_element
    *me;

  LFDS710_PAL_ASSERT( ms != NULL );

  while( LFDS710_LIST_ASU_GET_START_AND_THEN_NEXT(ms->list_of_allocations,lasue) )
  {
    me = LFDS710_LIST_ASU_GET_VALUE_FROM_ELEMENT( *lasue );

    me->current_pointer = me->rollback;
    me->current_memory_size_in_bytes = me->rollback_memory_size_in_bytes;
  }

  return;
}

