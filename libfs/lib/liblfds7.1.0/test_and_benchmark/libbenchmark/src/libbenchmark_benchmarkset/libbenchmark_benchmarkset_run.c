/***** includes *****/
#include "libbenchmark_benchmarkset_internal.h"





/****************************************************************************/
void libbenchmark_benchmarkset_run( struct libbenchmark_benchmarkset_state *bsets, struct libbenchmark_results_state *rs )
{
  lfds710_pal_uint_t
    number_numa_nodes;

  struct libbenchmark_benchmarkinstance_state
    *bs;

  struct lfds710_list_asu_element
    *lasue_benchmarks = NULL,
    *lasue_lpset = NULL,
    *lasue_numa = NULL;

  struct lfds710_list_aso_state
    *logical_processor_set;

  struct libbenchmark_topology_numa_node
    *numa_mode;

  LFDS710_PAL_ASSERT( bsets != NULL );
  LFDS710_PAL_ASSERT( rs != NULL );

  libbenchmark_topology_query( bsets->ts, LIBBENCHMARK_TOPOLOGY_QUERY_GET_NUMBER_OF_NODE_TYPE, (void *) (lfds710_pal_uint_t) LIBBENCHMARK_TOPOLOGY_NODE_TYPE_NUMA, &number_numa_nodes );

  // TRD : loop over every logical processor set
  while( LFDS710_LIST_ASU_GET_START_AND_THEN_NEXT(*bsets->logical_processor_sets,lasue_lpset) )
  {
    logical_processor_set = LFDS710_LIST_ASU_GET_VALUE_FROM_ELEMENT( *lasue_lpset );

    // TRD : now for this logical processor set, execute all benchmarks
    while( LFDS710_LIST_ASU_GET_START_AND_THEN_NEXT(bsets->benchmarks,lasue_benchmarks) )
    {
      bs = LFDS710_LIST_ASU_GET_VALUE_FROM_ELEMENT( *lasue_benchmarks );

      // TRD : run each benchmark instance over each NUMA mode
      while( LFDS710_LIST_ASU_GET_START_AND_THEN_NEXT(*bsets->numa_modes_list,lasue_numa) )
      {
        numa_mode = LFDS710_LIST_ASU_GET_VALUE_FROM_ELEMENT( *lasue_numa );
        libbenchmark_benchmarkinstance_run( bs, logical_processor_set, numa_mode->mode, bsets->ms, rs );
      }
    }
  }

  return;
}

