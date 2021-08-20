/***** includes *****/
#include "libbenchmark_results_internal.h"





/****************************************************************************/
int libbenchmark_results_get_result( struct libbenchmark_results_state *rs,
                                     enum libbenchmark_datastructure_id datastructure_id,
                                     enum libbenchmark_benchmark_id benchmark_id,
                                     enum libbenchmark_lock_id lock_id,
                                     enum libbenchmark_topology_numa_mode numa_mode,
                                     struct lfds710_list_aso_state *lpset,
                                     struct libbenchmark_topology_node_state *tns,
                                     lfds710_pal_uint_t *result )
{
  int
    rv;

  struct lfds710_btree_au_element
    *baue;

  struct libbenchmark_result
    *r,
    search_key;

  LFDS710_PAL_ASSERT( rs != NULL );
  // TRD : datastructure_id can be any value in its range
  // TRD : benchmark_id can be any value in its range
  // TRD : lock_id can be any value in its range
  // TRD : numa_mode can be any value in its range
  LFDS710_PAL_ASSERT( lpset != NULL );
  LFDS710_PAL_ASSERT( tns!= NULL );
  LFDS710_PAL_ASSERT( result != NULL );

  search_key.datastructure_id = datastructure_id;
  search_key.benchmark_id = benchmark_id;
  search_key.lock_id  = lock_id;
  search_key.numa_mode = numa_mode;
  search_key.lpset = lpset;
  search_key.tns = *tns;

  rv = lfds710_btree_au_get_by_key( &rs->results_tree, NULL, &search_key, &baue );

  if( rv == 1 )
  {
    r = LFDS710_BTREE_AU_GET_KEY_FROM_ELEMENT( *baue );
    *result = r->result;
  }

  return rv;
}

