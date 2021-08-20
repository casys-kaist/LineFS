/***** includes *****/
#include "libbenchmark_topology_internal.h"





/****************************************************************************/
void libbenchmark_topology_cleanup( struct libbenchmark_topology_state *ts )
{
  LFDS710_PAL_ASSERT( ts != NULL );

  lfds710_btree_au_cleanup( &ts->topology_tree, NULL );

  lfds710_btree_au_cleanup( &ts->lp_printing_offset_lookup_tree, NULL );

  return;
}

