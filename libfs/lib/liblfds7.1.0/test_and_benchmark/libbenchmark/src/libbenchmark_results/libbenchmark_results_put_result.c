/***** includes *****/
#include "libbenchmark_results_internal.h"





/****************************************************************************/
void libbenchmark_results_put_result( struct libbenchmark_results_state *rs,
                                      enum libbenchmark_datastructure_id datastructure_id,
                                      enum libbenchmark_benchmark_id benchmark_id,
                                      enum libbenchmark_lock_id lock_id,
                                      enum libbenchmark_topology_numa_mode numa_mode,
                                      struct lfds710_list_aso_state *lpset,
                                      lfds710_pal_uint_t logical_processor_number,
                                      lfds710_pal_uint_t windows_logical_processor_group_number,
                                      lfds710_pal_uint_t result )
{
  struct libbenchmark_result
    *r;

  LFDS710_PAL_ASSERT( rs != NULL );
  // TRD : datastructure_id can be any value in its range
  // TRD : benchmark_id can be any value in its range
  // TRD : lock_id can be any value in its range
  // TRD : numa_mode can be any value in its range
  LFDS710_PAL_ASSERT( lpset != NULL );
  // TRD : logical_processor_number can be any value in its range
  // TRD : windows_logical_processor_group_number can be any value in its range
  // TRD : result can be any value in its range

  r = libshared_memory_alloc_from_most_free_space_node( rs->ms, sizeof(struct libbenchmark_result), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );

  r->benchmark_id = benchmark_id;
  r->datastructure_id = datastructure_id;
  r->lock_id  = lock_id;
  r->numa_mode = numa_mode;
  r->lpset = lpset;
  LIBBENCHMARK_TOPOLOGY_NODE_SET_TYPE( r->tns, LIBBENCHMARK_TOPOLOGY_NODE_TYPE_LOGICAL_PROCESSOR );
  LIBBENCHMARK_TOPOLOGY_NODE_SET_LOGICAL_PROCESSOR_NUMBER( r->tns, logical_processor_number );
  LIBBENCHMARK_TOPOLOGY_NODE_SET_WINDOWS_GROUP_NUMBER( r->tns, windows_logical_processor_group_number );
  r->result = result;

  LFDS710_BTREE_AU_SET_KEY_IN_ELEMENT( r->baue, r );
  
  lfds710_btree_au_insert( &rs->results_tree, &r->baue, NULL );

  return;
}

