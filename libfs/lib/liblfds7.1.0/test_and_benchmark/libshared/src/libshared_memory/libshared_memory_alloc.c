/***** includes *****/
#include "libshared_memory_internal.h"

/***** private prototypes *****/
static void *alloc_from_memory_element( struct libshared_memory_element *me, lfds710_pal_uint_t size_in_bytes, lfds710_pal_uint_t alignment_in_bytes );





/****************************************************************************/
void *libshared_memory_alloc_from_unknown_node( struct libshared_memory_state *ms, lfds710_pal_uint_t size_in_bytes, lfds710_pal_uint_t alignment_in_bytes )
{
  struct lfds710_list_asu_element
    *lasue = NULL;

  struct libshared_memory_element
    *me;

  void
    *allocation = NULL;

  LFDS710_PAL_ASSERT( ms != NULL );
  // TRD : size_in_bytes can be any value in its range
  // TRD : alignment_in_bytes can be any value in its range

  while( allocation == NULL and LFDS710_LIST_ASU_GET_START_AND_THEN_NEXT(ms->list_of_allocations,lasue) )
  {
    me = LFDS710_LIST_ASU_GET_VALUE_FROM_ELEMENT( *lasue );

    if( me->known_numa_node_flag == LOWERED )
      allocation = alloc_from_memory_element( me, size_in_bytes, alignment_in_bytes );
  }

  return allocation;
}





/****************************************************************************/
void *libshared_memory_alloc_largest_possible_array_from_unknown_node( struct libshared_memory_state *ms, lfds710_pal_uint_t element_size_in_bytes, lfds710_pal_uint_t alignment_in_bytes, lfds710_pal_uint_t *number_elements )
{
  lfds710_pal_uint_t
    alignment_bump;

  struct lfds710_list_asu_element
    *lasue = NULL;

  struct libshared_memory_element
    *me = NULL,
    *temp_me;

  void
    *allocation;

  LFDS710_PAL_ASSERT( ms != NULL );
  // TRD : element_size_in_bytes can be any value in its range
  // TRD : alignment_in_bytes can be any value in its range
  LFDS710_PAL_ASSERT( number_elements != NULL );

  /* TRD : find the largest unknown-node memory element
           then alloc from that
  */

  // TRD : find the correct memory element - in this case, the one with most free space
  while( LFDS710_LIST_ASU_GET_START_AND_THEN_NEXT(ms->list_of_allocations,lasue) )
  {
    temp_me = LFDS710_LIST_ASU_GET_VALUE_FROM_ELEMENT( *lasue );

    if( temp_me->known_numa_node_flag == LOWERED )
      if( me == NULL or temp_me->current_memory_size_in_bytes > me->current_memory_size_in_bytes )
        me = temp_me;
  }

  alignment_bump = (lfds710_pal_uint_t) me->current_pointer % alignment_in_bytes;

  if( alignment_bump != 0 )
    alignment_bump = alignment_in_bytes - alignment_bump;

  *number_elements = (me->current_memory_size_in_bytes - alignment_bump) / element_size_in_bytes;

  allocation = alloc_from_memory_element( me, *number_elements * element_size_in_bytes, alignment_in_bytes );

  return allocation;
}





/****************************************************************************/
void *libshared_memory_alloc_from_specific_node( struct libshared_memory_state *ms, lfds710_pal_uint_t numa_node_id, lfds710_pal_uint_t size_in_bytes, lfds710_pal_uint_t alignment_in_bytes )
{
  struct lfds710_list_asu_element
    *lasue = NULL;

  struct libshared_memory_element
    *me;

  void
    *allocation = NULL;

  LFDS710_PAL_ASSERT( ms != NULL );
  // TRD : numa_node_id can be any value in its range
  // TRD : size_in_bytes can be any value in its range
  // TRD : alignment_in_bytes can be any value in its range

  while( allocation == NULL and LFDS710_LIST_ASU_GET_START_AND_THEN_NEXT(ms->list_of_allocations,lasue) )
  {
    me = LFDS710_LIST_ASU_GET_VALUE_FROM_ELEMENT( *lasue );

    if( me->known_numa_node_flag == RAISED and me->numa_node_id == numa_node_id )
      allocation = alloc_from_memory_element( me, size_in_bytes, alignment_in_bytes );
  }

  return allocation;
}





/****************************************************************************/
void *libshared_memory_alloc_from_most_free_space_node( struct libshared_memory_state *ms, lfds710_pal_uint_t size_in_bytes, lfds710_pal_uint_t alignment_in_bytes )
{
  struct lfds710_list_asu_element
    *lasue = NULL;

  struct libshared_memory_element
    *me = NULL,
    *temp_me;

  void
    *allocation;

  LFDS710_PAL_ASSERT( ms != NULL );
  // TRD : size_in_bytes can be any value in its range
  // TRD : alignment_in_bytes can be any value in its range

  // TRD : find the correct memory element - in this case, the one with most free space
  while( LFDS710_LIST_ASU_GET_START_AND_THEN_NEXT(ms->list_of_allocations,lasue) )
  {
    temp_me = LFDS710_LIST_ASU_GET_VALUE_FROM_ELEMENT( *lasue );

    if( me == NULL or temp_me->current_memory_size_in_bytes > me->current_memory_size_in_bytes )
      me = temp_me;
  }

  allocation = alloc_from_memory_element( me, size_in_bytes, alignment_in_bytes );

  return allocation;
}





/****************************************************************************/
static void *alloc_from_memory_element( struct libshared_memory_element *me, lfds710_pal_uint_t size_in_bytes, lfds710_pal_uint_t alignment_in_bytes )
{
  lfds710_pal_uint_t
    alignment_bump,
    total_size_in_bytes;

  void
    *allocation;

  LFDS710_PAL_ASSERT( me != NULL );
  // TRD : size_in_bytes can be any value in its range
  // TRD : alignment_in_bytes can be any value in its range

  alignment_bump = (lfds710_pal_uint_t) me->current_pointer % alignment_in_bytes;

  if( alignment_bump != 0 )
    alignment_bump = alignment_in_bytes - alignment_bump;

  total_size_in_bytes = size_in_bytes + alignment_bump;

  if( total_size_in_bytes > me->current_memory_size_in_bytes )
    return NULL;

  me->current_pointer += alignment_bump;

  allocation = me->current_pointer;

  me->current_pointer += size_in_bytes;

  me->current_memory_size_in_bytes -= total_size_in_bytes;

  return allocation;
}

