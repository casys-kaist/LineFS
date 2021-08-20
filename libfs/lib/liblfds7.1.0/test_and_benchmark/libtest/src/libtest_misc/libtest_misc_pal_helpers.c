/***** includes *****/
#include "libtest_misc_internal.h"





/****************************************************************************/
void libtest_misc_pal_helper_add_logical_processor_to_list_of_logical_processors( struct lfds710_list_asu_state *list_of_logical_processors,
                                                                                  struct libshared_memory_state *ms,
                                                                                  lfds710_pal_uint_t logical_processor_number,
                                                                                  lfds710_pal_uint_t windows_processor_group_number )
{
  struct libtest_logical_processor
    *lp;

  LFDS710_PAL_ASSERT( list_of_logical_processors != NULL );
  LFDS710_PAL_ASSERT( ms != NULL );
  // TRD : logical_processor_number can be any value in its range
  // TRD : windows_processor_group_number can be any value in its range

  lp = libshared_memory_alloc_from_most_free_space_node( ms, sizeof(struct libtest_logical_processor), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );

  LIBTEST_PAL_SET_LOGICAL_PROCESSOR_NUMBER( *lp, logical_processor_number );
  LIBTEST_PAL_SET_WINDOWS_PROCESSOR_GROUP_NUMBER( *lp, windows_processor_group_number );

  LFDS710_LIST_ASU_SET_VALUE_IN_ELEMENT( lp->lasue, lp );
  lfds710_list_asu_insert_at_start( list_of_logical_processors, &lp->lasue );

  return;
}

