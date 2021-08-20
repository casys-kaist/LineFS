/***** includes *****/
#include "libbenchmark_benchmarkinstance_internal.h"





/****************************************************************************/
void libbenchmark_benchmarkinstance_init( struct libbenchmark_benchmarkinstance_state *bs,
                                          enum libbenchmark_datastructure_id datastructure_id,
                                          enum libbenchmark_benchmark_id benchmark_id,
                                          enum libbenchmark_lock_id lock_id,
                                          struct libbenchmark_topology_state *ts,
                                          void (*init_function)( struct libbenchmark_topology_state *ts,
                                                                 struct lfds710_list_aso_state *logical_processor_set,
                                                                 struct libshared_memory_state *ms,
                                                                 enum libbenchmark_topology_numa_mode numa_node,
                                                                 struct libbenchmark_threadset_state *tsets ),
                                          void (*cleanup_function)( struct lfds710_list_aso_state *logical_processor_set,
                                                                    enum libbenchmark_topology_numa_mode numa_node,
                                                                    struct libbenchmark_results_state *rs,
                                                                    struct libbenchmark_threadset_state *tsets ) )
{
  LFDS710_PAL_ASSERT( bs != NULL );
  // TRD : datastructure_id can be any value in its range
  // TRD : benchmark_id can be any value in its range
  // TRD : lock_id can be any value in its range
  LFDS710_PAL_ASSERT( ts != NULL );
  LFDS710_PAL_ASSERT( init_function != NULL );
  LFDS710_PAL_ASSERT( cleanup_function != NULL );

  bs->datastructure_id = datastructure_id;
  bs->benchmark_id = benchmark_id;
  bs->lock_id = lock_id;
  bs->ts = ts;
  bs->init_function = init_function;
  bs->cleanup_function = cleanup_function;

  return;
}

