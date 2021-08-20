/***** includes *****/
#include "libbenchmark_topology_node_internal.h"





/****************************************************************************/
void libbenchmark_topology_node_init( struct libbenchmark_topology_node_state *tns )
{
  LFDS710_PAL_ASSERT( tns != NULL );

  // TRD : we only ever add logical processor nodes to the logical_processor_children list
  lfds710_list_aso_init_valid_on_current_logical_core( &tns->logical_processor_children, libbenchmark_topology_node_compare_nodes_function, LFDS710_LIST_ASO_EXISTING_KEY_FAIL, NULL );

  return;
}

