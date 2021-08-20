/***** includes *****/
#include "libshared_memory_internal.h"

/***** private prototypes *****/
static void alloc_and_init_memory_element( struct libshared_memory_element **me, void *memory, lfds710_pal_uint_t memory_size_in_bytes );




/****************************************************************************/
void libshared_memory_add_memory_from_numa_node( struct libshared_memory_state *ms, lfds710_pal_uint_t numa_node_id, void *memory, lfds710_pal_uint_t memory_size_in_bytes )
{
  struct libshared_memory_element
    *me;

  LFDS710_PAL_ASSERT( ms != NULL );
  // TRD : numa_node_id can be any value in its range
  LFDS710_PAL_ASSERT( memory != NULL );
  // TRD : memory_size_in_bytes can be any value in its range

  alloc_and_init_memory_element( &me, memory, memory_size_in_bytes );

  me->known_numa_node_flag = RAISED;
  me->numa_node_id = numa_node_id;

  LFDS710_LIST_ASU_SET_KEY_IN_ELEMENT( me->lasue, me );
  LFDS710_LIST_ASU_SET_VALUE_IN_ELEMENT( me->lasue, me );
  lfds710_list_asu_insert_at_start( &ms->list_of_allocations, &me->lasue );

  return;
}





/****************************************************************************/
void libshared_memory_add_memory( struct libshared_memory_state *ms, void *memory, lfds710_pal_uint_t memory_size_in_bytes )
{
  struct libshared_memory_element
    *me;

  LFDS710_PAL_ASSERT( ms != NULL );
  LFDS710_PAL_ASSERT( memory != NULL );
  // TRD : memory_size_in_bytes can be any value in its range

  alloc_and_init_memory_element( &me, memory, memory_size_in_bytes );

  me->known_numa_node_flag = LOWERED;

  LFDS710_LIST_ASU_SET_KEY_IN_ELEMENT( me->lasue, me );
  LFDS710_LIST_ASU_SET_VALUE_IN_ELEMENT( me->lasue, me );
  lfds710_list_asu_insert_at_start( &ms->list_of_allocations, &me->lasue );

  return;
}





/****************************************************************************/
static void alloc_and_init_memory_element( struct libshared_memory_element **me, void *memory, lfds710_pal_uint_t memory_size_in_bytes )
{
  lfds710_pal_uint_t
    alignment_bump,
    size_in_bytes,
    total_size_in_bytes;

  LFDS710_PAL_ASSERT( me != NULL );
  LFDS710_PAL_ASSERT( memory != NULL );
  // TRD : memory_size_in_bytes can be any value in its range

  alignment_bump = (lfds710_pal_uint_t) memory % LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES;

  if( alignment_bump != 0 )
    alignment_bump = LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES - alignment_bump;

  size_in_bytes = sizeof( struct libshared_memory_element );

  total_size_in_bytes = size_in_bytes + alignment_bump;

  *me = (struct libshared_memory_element *) ( (char unsigned *) memory + alignment_bump );

  (*me)->original = memory;
  (*me)->original_memory_size_in_bytes = memory_size_in_bytes;

  (*me)->original_after_me_alloc = (*me)->current_pointer = (char unsigned *) memory + total_size_in_bytes;
  (*me)->original_after_me_alloc_memory_size_in_bytes = (*me)->current_memory_size_in_bytes = memory_size_in_bytes - total_size_in_bytes;

  return;
}

