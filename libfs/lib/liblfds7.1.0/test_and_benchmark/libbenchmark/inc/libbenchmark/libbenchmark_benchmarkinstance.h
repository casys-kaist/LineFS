/***** enums *****/

/***** structs *****/
struct libbenchmark_benchmarkinstance_state
{
  enum flag
    numa_awareness_flag;

  enum libbenchmark_datastructure_id
    datastructure_id;

  enum libbenchmark_benchmark_id
    benchmark_id;

  enum libbenchmark_lock_id
    lock_id;

  struct libbenchmark_threadset_state
    tsets;

  struct libbenchmark_topology_state
    *ts;

  struct lfds710_list_asu_element
    lasue;

  void
    (*init_function)( struct libbenchmark_topology_state *ts,
                      struct lfds710_list_aso_state *logical_processor_set,
                      struct libshared_memory_state *ms,
                      enum libbenchmark_topology_numa_mode numa_node,
                      struct libbenchmark_threadset_state *tsets ),
    (*cleanup_function)( struct lfds710_list_aso_state *logical_processor_set,
                         enum libbenchmark_topology_numa_mode numa_node,
                         struct libbenchmark_results_state *rs,
                         struct libbenchmark_threadset_state *tsets );
};

/***** public prototypes *****/
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
                                                                    struct libbenchmark_threadset_state *tsets ) );

void libbenchmark_benchmarkinstance_cleanup( struct libbenchmark_benchmarkinstance_state *bs );

void libbenchmark_benchmarkinstance_run( struct libbenchmark_benchmarkinstance_state *bs,
                                         struct lfds710_list_aso_state *lpset,
                                         enum libbenchmark_topology_numa_mode numa_mode,
                                         struct libshared_memory_state *ms,
                                         struct libbenchmark_results_state *rs );

