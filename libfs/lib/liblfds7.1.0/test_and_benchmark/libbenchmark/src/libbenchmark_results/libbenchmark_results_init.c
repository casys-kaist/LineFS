/***** includes *****/
#include "libbenchmark_results_internal.h"





/****************************************************************************/
void libbenchmark_results_init( struct libbenchmark_results_state *rs,
                                struct libshared_memory_state *ms )
{
  LFDS710_PAL_ASSERT( rs != NULL );
  LFDS710_PAL_ASSERT( ms != NULL );

  lfds710_btree_au_init_valid_on_current_logical_core( &rs->results_tree, libbenchmark_result_compare_function, LFDS710_BTREE_AU_EXISTING_KEY_FAIL, NULL );

  rs->ms = ms;

  return;
}

