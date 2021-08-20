/***** includes *****/
#include "libshared_memory_internal.h"





/****************************************************************************/
void libshared_memory_init( struct libshared_memory_state *ms )
{
  LFDS710_PAL_ASSERT( ms != NULL );

  lfds710_list_asu_init_valid_on_current_logical_core( &ms->list_of_allocations, NULL );

  return;
}

