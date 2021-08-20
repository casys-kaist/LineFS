/***** defines *****/

/***** enums *****/

/***** structs *****/
struct libbenchmark_result
{
  enum libbenchmark_benchmark_id
    benchmark_id;

  enum libbenchmark_datastructure_id
    datastructure_id;

  enum libbenchmark_lock_id
    lock_id;

  enum libbenchmark_topology_numa_mode
    numa_mode;

  lfds710_pal_uint_t
    result;

  struct lfds710_btree_au_element
    baue;

  struct lfds710_list_aso_state
    *lpset;

  struct libbenchmark_topology_node_state
    tns;
};

struct libbenchmark_results_state
{
  struct libshared_memory_state
    *ms;

  struct lfds710_btree_au_state
    results_tree;
};

/***** public prototypes *****/
void libbenchmark_results_init( struct libbenchmark_results_state *rs,
                                struct libshared_memory_state *ms );
void libbenchmark_results_cleanup( struct libbenchmark_results_state *rs );

void libbenchmark_results_put_result( struct libbenchmark_results_state *rs,
                                      enum libbenchmark_datastructure_id datastructure_id,
                                      enum libbenchmark_benchmark_id benchmark_id,
                                      enum libbenchmark_lock_id lock_id,
                                      enum libbenchmark_topology_numa_mode numa_mode,
                                      struct lfds710_list_aso_state *lpset,
                                      lfds710_pal_uint_t logical_processor_number,
                                      lfds710_pal_uint_t windows_logical_processor_group_number,
                                      lfds710_pal_uint_t result );

int libbenchmark_results_get_result( struct libbenchmark_results_state *rs,
                                     enum libbenchmark_datastructure_id datastructure_id,
                                     enum libbenchmark_benchmark_id benchmark_id,
                                     enum libbenchmark_lock_id lock_id,
                                     enum libbenchmark_topology_numa_mode numa_mode,
                                     struct lfds710_list_aso_state *lpset,
                                     struct libbenchmark_topology_node_state *tns,
                                     lfds710_pal_uint_t *result );

int libbenchmark_result_compare_function( void const *new_key, void const *existing_key );

