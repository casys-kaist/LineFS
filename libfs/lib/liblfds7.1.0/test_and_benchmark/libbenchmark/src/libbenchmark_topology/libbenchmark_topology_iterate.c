/***** includes *****/
#include "libbenchmark_topology_internal.h"





/****************************************************************************/
void libbenchmark_topology_iterate_init( struct libbenchmark_topology_iterate_state *tis, enum libbenchmark_topology_node_type type )
{
  LFDS710_PAL_ASSERT( tis != NULL );
  // TRD : type can be any value in its range

  tis->baue = NULL;
  tis->type = type;

  return;
}





/****************************************************************************/
int libbenchmark_topology_iterate( struct libbenchmark_topology_state *ts, struct libbenchmark_topology_iterate_state *tis, struct libbenchmark_topology_node_state **tns )
{
  int
    rv = 1;

  LFDS710_PAL_ASSERT( ts != NULL );
  LFDS710_PAL_ASSERT( tis != NULL );
  LFDS710_PAL_ASSERT( tns != NULL );

  while( lfds710_btree_au_get_by_absolute_position_and_then_by_relative_position(&ts->topology_tree, &tis->baue, LFDS710_BTREE_AU_ABSOLUTE_POSITION_LARGEST_IN_TREE, LFDS710_BTREE_AU_RELATIVE_POSITION_NEXT_SMALLER_ELEMENT_IN_ENTIRE_TREE) )
  {
    *tns = LFDS710_BTREE_AU_GET_VALUE_FROM_ELEMENT( *tis->baue );

    if( (*tns)->type == tis->type )
      break;
  }

  if( tis->baue == NULL )
  {
    *tns = NULL;
    rv = 0;
  }

  return rv;
}

