/***** includes *****/
#include "libbenchmark_topology_node_internal.h"





/****************************************************************************/
void libbenchmark_topology_node_cleanup( struct libbenchmark_topology_node_state *tns, void (*element_cleanup_callback)(struct lfds710_list_aso_state *lasos, struct lfds710_list_aso_element *lasoe) )
{
  LFDS710_PAL_ASSERT( tns != NULL );
  // TRD : element_cleanup_callback can be NULL

  lfds710_list_aso_cleanup( &tns->logical_processor_children, element_cleanup_callback );

  return;
}

