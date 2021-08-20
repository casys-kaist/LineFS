/***** includes *****/
#include "libshared_memory_internal.h"





/****************************************************************************/
void libshared_memory_cleanup( struct libshared_memory_state *ms, void (*memory_cleanup_callback)(enum flag known_numa_node_flag, void *store, lfds710_pal_uint_t size) )
{
  struct lfds710_list_asu_element
    *lasue,
    *lasue_next;

  struct libshared_memory_element
    *me;

  LFDS710_PAL_ASSERT( ms != NULL );
  // TRD : memory_cleanup_callback can be NULL

  if( memory_cleanup_callback != NULL )
  {
    // TRD : remember that the allocation we're free()ing may contain the list element itself

    lasue = LFDS710_LIST_ASU_GET_START( ms->list_of_allocations );

    while( lasue != NULL )
    {
      lasue_next = LFDS710_LIST_ASU_GET_NEXT( *lasue );
      me = LFDS710_LIST_ASU_GET_VALUE_FROM_ELEMENT( *lasue );
      memory_cleanup_callback( me->known_numa_node_flag, me->original, me->original_memory_size_in_bytes );
      lasue = lasue_next;
    }
  }

  lfds710_list_asu_cleanup( &ms->list_of_allocations, NULL );

  return;
}

