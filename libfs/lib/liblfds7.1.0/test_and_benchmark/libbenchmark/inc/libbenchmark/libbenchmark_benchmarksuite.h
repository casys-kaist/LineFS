/***** defines *****/
#define LIBBENCHMARK_BENCHMARKSUITE_OPTION_DURATION  0x1

/***** enums *****/

/***** structs *****/
struct libbenchmark_benchmarksuite_state
{
  struct lfds710_list_asu_state
    benchmarksets,
    lpsets,
    numa_modes_list;

  struct libshared_memory_state
    *ms;

  struct libbenchmark_topology_state
    *ts;
};

/***** public prototypes *****/
void libbenchmark_benchmarksuite_init( struct libbenchmark_benchmarksuite_state *bss,
                                       struct libbenchmark_topology_state *ts,
                                       struct libshared_memory_state *ms,
                                       enum libbenchmark_topology_numa_mode numa_mode,
                                       lfds710_pal_uint_t options_bitmask,
                                       lfds710_pal_uint_t benchmark_duration_in_seconds );

void libbenchmark_benchmarksuite_cleanup( struct libbenchmark_benchmarksuite_state *bss );

void libbenchmark_benchmarksuite_add_benchmarkset( struct libbenchmark_benchmarksuite_state *bss,
                                                   struct libbenchmark_benchmarkset_state *bsets );

void libbenchmark_benchmarksuite_run( struct libbenchmark_benchmarksuite_state *bss,
                                      struct libbenchmark_results_state *rs );

void libbenchmark_benchmarksuite_get_list_of_gnuplot_strings( struct libbenchmark_benchmarksuite_state *bss,
                                                              struct libbenchmark_results_state *rs,
                                                              char *gnuplot_system_string,
                                                              struct libbenchmark_gnuplot_options *gpo,
                                                              struct lfds710_list_asu_state *list_of_gnuplot_strings );

