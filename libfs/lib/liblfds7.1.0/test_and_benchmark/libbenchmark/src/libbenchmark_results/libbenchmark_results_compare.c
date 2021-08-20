/***** includes *****/
#include "libbenchmark_results_internal.h"





/****************************************************************************/
int libbenchmark_result_compare_function( void const *new_key, void const *existing_key )
{
  int
    rv;

  struct libbenchmark_result
    *rs_new,
    *rs_existing;

  LFDS710_PAL_ASSERT( new_key != NULL );
  LFDS710_PAL_ASSERT( existing_key != NULL );

  rs_new = (struct libbenchmark_result *) new_key;
  rs_existing = (struct libbenchmark_result *) existing_key;

  if( rs_new->datastructure_id > rs_existing->datastructure_id )
    return 1;

  if( rs_new->datastructure_id < rs_existing->datastructure_id )
    return -1;

  if( rs_new->benchmark_id > rs_existing->benchmark_id )
    return 1;

  if( rs_new->benchmark_id < rs_existing->benchmark_id )
    return -1;

  if( rs_new->lock_id > rs_existing->lock_id )
    return 1;

  if( rs_new->lock_id < rs_existing->lock_id )
    return -1;

  if( rs_new->numa_mode > rs_existing->numa_mode )
    return 1;

  if( rs_new->numa_mode < rs_existing->numa_mode )
    return -1;

  rv = libbenchmark_topology_node_compare_lpsets_function( rs_new->lpset, rs_existing->lpset );

  if( rv != 0 )
    return rv;

  rv = libbenchmark_topology_node_compare_nodes_function( &rs_new->tns, &rs_existing->tns );

  // TRD : for better or worse, it's what we are :-)
  return rv;
}

