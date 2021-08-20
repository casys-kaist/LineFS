/***** includes *****/
#include "libbenchmark_topology_internal.h"





/****************************************************************************/
int libbenchmark_topology_compare_lp_printing_offsets_function( void const *new_key, void const *existing_key )
{
  int
    cr;

  struct libbenchmark_topology_lp_printing_offset
    *tlpo_one,
    *tlpo_two;

  LFDS710_PAL_ASSERT( new_key != NULL );
  LFDS710_PAL_ASSERT( existing_key != NULL );

  tlpo_one = (struct libbenchmark_topology_lp_printing_offset *) new_key;
  tlpo_two = (struct libbenchmark_topology_lp_printing_offset *) existing_key;

  cr = libbenchmark_topology_node_compare_nodes_function( &tlpo_one->tns, &tlpo_two->tns );

  return cr;
}





/****************************************************************************/
int libbenchmark_topology_compare_node_against_lp_printing_offset_function( void const *new_key, void const *existing_key )
{
  int
    cr;

  struct libbenchmark_topology_node_state
    *tns;

  struct libbenchmark_topology_lp_printing_offset
    *tlpo;

  LFDS710_PAL_ASSERT( new_key != NULL );
  LFDS710_PAL_ASSERT( existing_key != NULL );

  tns = (struct libbenchmark_topology_node_state *) new_key;
  tlpo = (struct libbenchmark_topology_lp_printing_offset *) existing_key;

  cr = libbenchmark_topology_node_compare_nodes_function( tns, &tlpo->tns );

  return cr;
}

