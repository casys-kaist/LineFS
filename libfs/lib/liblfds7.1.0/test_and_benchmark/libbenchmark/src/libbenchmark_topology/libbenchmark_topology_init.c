/***** includes *****/
#include "libbenchmark_topology_internal.h"





/****************************************************************************/
int libbenchmark_topology_init( struct libbenchmark_topology_state *ts, struct libshared_memory_state *ms )
{
  int
    offset = 0,
    rv;

  lfds710_pal_uint_t
    lp_count;

  struct lfds710_btree_au_element
    *baue;

  struct libbenchmark_topology_lp_printing_offset
    *tlpo;

  struct libbenchmark_topology_node_state
    *tns;

  LFDS710_PAL_ASSERT( ts != NULL );
  LFDS710_PAL_ASSERT( ms != NULL );

  lfds710_btree_au_init_valid_on_current_logical_core( &ts->topology_tree, libbenchmark_topology_node_compare_nodes_function, LFDS710_BTREE_AU_EXISTING_KEY_FAIL, NULL );

  rv = libbenchmark_porting_abstraction_layer_populate_topology( ts, ms );

  lfds710_btree_au_get_by_absolute_position( &ts->topology_tree, &baue, LFDS710_BTREE_AU_ABSOLUTE_POSITION_LARGEST_IN_TREE );
  lp_count = count_of_logical_processors_below_node( baue );
  ts->line_width = (int) ( lp_count * 3 + lp_count - 1 );

  // TRD : now form up the printing offset tree
  lfds710_btree_au_init_valid_on_current_logical_core( &ts->lp_printing_offset_lookup_tree, libbenchmark_topology_compare_lp_printing_offsets_function, LFDS710_BTREE_AU_EXISTING_KEY_FAIL, NULL );

  baue = NULL;

  while( lfds710_btree_au_get_by_absolute_position_and_then_by_relative_position(&ts->topology_tree, &baue, LFDS710_BTREE_AU_ABSOLUTE_POSITION_LARGEST_IN_TREE, LFDS710_BTREE_AU_RELATIVE_POSITION_NEXT_SMALLER_ELEMENT_IN_ENTIRE_TREE) )
  {
    tns = LFDS710_BTREE_AU_GET_KEY_FROM_ELEMENT( *baue );

    if( tns->type == LIBBENCHMARK_TOPOLOGY_NODE_TYPE_LOGICAL_PROCESSOR )
    {
      tlpo = libshared_memory_alloc_from_most_free_space_node( ms, sizeof(struct libbenchmark_topology_lp_printing_offset), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );

      tlpo->tns = *tns;
      tlpo->offset = offset;

      LFDS710_BTREE_AU_SET_KEY_IN_ELEMENT( tlpo->baue, tlpo );
      LFDS710_BTREE_AU_SET_VALUE_IN_ELEMENT( tlpo->baue, tlpo );

      lfds710_btree_au_insert( &ts->lp_printing_offset_lookup_tree, &tlpo->baue, NULL );

      offset += 4;
    }
  }

  return rv;
}

