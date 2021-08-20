/***** defines *****/

/***** enums *****/

/***** structs *****/
struct libbenchmark_benchmarkset_state
{
  enum libbenchmark_datastructure_id
    datastructure_id;

  enum libbenchmark_benchmark_id
    benchmark_id;

  struct lfds710_list_asu_element
    lasue;

  struct lfds710_list_asu_state
    benchmarks,
    *logical_processor_sets,
    *numa_modes_list;

  struct libshared_memory_state
    *ms;

  struct libbenchmark_topology_state
    *ts;
};

struct libbenchmark_benchmarkset_gnuplot
{
  char
    filename[256],
    *gnuplot_string;

  enum libbenchmark_benchmark_id
    benchmark_id;

  enum libbenchmark_datastructure_id
    datastructure_id;

  struct lfds710_list_asu_element
    lasue;
};

/***** public prototypes *****/
void libbenchmark_benchmarkset_init( struct libbenchmark_benchmarkset_state *bsets,
                                     enum libbenchmark_datastructure_id datastructure_id,
                                     enum libbenchmark_benchmark_id benchmark_id,
                                     struct lfds710_list_asu_state *logical_processor_sets,
                                     struct lfds710_list_asu_state *numa_modes_list,
                                     struct libbenchmark_topology_state *ts,
                                     struct libshared_memory_state *ms );

void libbenchmark_benchmarkset_cleanup( struct libbenchmark_benchmarkset_state *bsets );

void libbenchmark_benchmarkset_add_benchmark( struct libbenchmark_benchmarkset_state *bsets, struct libbenchmark_benchmarkinstance_state *bs );

void libbenchmark_benchmarkset_run( struct libbenchmark_benchmarkset_state *bsets, struct libbenchmark_results_state *rs );

void libbenchmark_benchmarkset_gnuplot_emit( struct libbenchmark_benchmarkset_state *bsets,
                                             struct libbenchmark_results_state *rs,
                                             char *gnuplot_system_string,
                                             enum libbenchmark_topology_numa_mode numa_mode,
                                             struct libbenchmark_gnuplot_options *gpo,
                                             struct libbenchmark_benchmarkset_gnuplot *bg );

