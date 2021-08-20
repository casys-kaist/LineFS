/***** includes *****/
#include "libbenchmark_topology_internal.h"





/****************************************************************************/
void libbenchmark_topology_insert( struct libbenchmark_topology_state *ts, struct libbenchmark_topology_node_state *tns )
{
  LFDS710_PAL_ASSERT( ts != NULL );
  LFDS710_PAL_ASSERT( tns != NULL );

  LFDS710_BTREE_AU_SET_KEY_IN_ELEMENT( tns->baue, tns );
  LFDS710_BTREE_AU_SET_VALUE_IN_ELEMENT( tns->baue, tns );
  lfds710_btree_au_insert( &ts->topology_tree, &tns->baue, NULL );

  return;
}

