/***** defines *****/
#define LIBBENCHMARK_MISC_VERSION_STRING   "7.1.0"
#define LIBBENCHMARK_MISC_VERSION_INTEGER  710

/***** enums *****/
enum libbenchmark_misc_query
{
  LIBBENCHMARK_MISC_QUERY_GET_BUILD_AND_VERSION_STRING
};

/***** externs *****/
extern char const
  * const libbenchmark_globals_datastructure_names[],
  * const libbenchmark_globals_benchmark_names[],
  * const libbenchmark_globals_lock_names[],
  * const libbenchmark_globals_numa_mode_names[];

extern lfds710_pal_uint_t
  libbenchmark_globals_benchmark_duration_in_seconds;

/***** public prototypes *****/
void libbenchmark_misc_pal_helper_new_topology_node( struct libbenchmark_topology_node_state **tns,
                                                     struct libshared_memory_state *ms );

void libbenchmark_misc_pal_helper_add_logical_processor_to_topology_node( struct libbenchmark_topology_node_state *tns,
                                                                          struct libshared_memory_state *ms,
                                                                          lfds710_pal_uint_t logical_processor_number,
                                                                          enum flag windows_processor_group_inuse_flag,
                                                                          lfds710_pal_uint_t windows_processor_group_number );

void libbenchmark_misc_pal_helper_add_system_node_to_topology_tree( struct libbenchmark_topology_state *ts,
                                                                    struct libbenchmark_topology_node_state *tns );

void libbenchmark_misc_pal_helper_add_numa_node_to_topology_tree( struct libbenchmark_topology_state *ts,
                                                                  struct libbenchmark_topology_node_state *tns,
                                                                  lfds710_pal_uint_t numa_node_id );

void libbenchmark_misc_pal_helper_add_socket_node_to_topology_tree( struct libbenchmark_topology_state *ts,
                                                                    struct libbenchmark_topology_node_state *tns );

void libbenchmark_misc_pal_helper_add_physical_processor_node_to_topology_tree( struct libbenchmark_topology_state *ts,
                                                                                struct libbenchmark_topology_node_state *tns );

void libbenchmark_misc_pal_helper_add_cache_node_to_topology_tree( struct libbenchmark_topology_state *ts,
                                                                   struct libbenchmark_topology_node_state *tns,
                                                                   lfds710_pal_uint_t level,
                                                                   enum libbenchmark_topology_node_cache_type type );

void libbenchmark_misc_pal_helper_add_logical_processor_node_to_topology_tree( struct libbenchmark_topology_state *ts,
                                                                               struct libshared_memory_state *ms,
                                                                               lfds710_pal_uint_t logical_processor_number,
                                                                               enum flag windows_processor_group_inuse_flag,
                                                                               lfds710_pal_uint_t windows_processor_group_number );

void libbenchmark_misc_query( enum libbenchmark_misc_query query_type, void *query_input, void *query_output );

