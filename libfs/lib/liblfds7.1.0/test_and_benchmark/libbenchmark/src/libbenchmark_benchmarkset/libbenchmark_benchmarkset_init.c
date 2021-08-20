/***** includes *****/
#include "libbenchmark_benchmarkset_internal.h"





/****************************************************************************/
void libbenchmark_benchmarkset_init( struct libbenchmark_benchmarkset_state *bsets,
                                     enum libbenchmark_datastructure_id datastructure_id,
                                     enum libbenchmark_benchmark_id benchmark_id,
                                     struct lfds710_list_asu_state *logical_processor_sets,
                                     struct lfds710_list_asu_state *numa_modes_list,
                                     struct libbenchmark_topology_state *ts,
                                     struct libshared_memory_state *ms )
{
  LFDS710_PAL_ASSERT( bsets != NULL );
  // TRD : datastructure_id can be any value in its range
  // TRD : benchmark_id can be any value in its range
  LFDS710_PAL_ASSERT( logical_processor_sets != NULL );
  LFDS710_PAL_ASSERT( numa_modes_list != NULL );
  LFDS710_PAL_ASSERT( ts != NULL );
  LFDS710_PAL_ASSERT( ms != NULL );

  bsets->datastructure_id = datastructure_id;
  bsets->benchmark_id = benchmark_id;
  lfds710_list_asu_init_valid_on_current_logical_core( &bsets->benchmarks, NULL );
  bsets->logical_processor_sets = logical_processor_sets;
  bsets->numa_modes_list = numa_modes_list;
  bsets->ts = ts;
  bsets->ms = ms;

  return;
}

