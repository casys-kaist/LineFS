/***** enums *****/
enum libbenchmark_topology_string_format
{
  LIBBENCHMARK_TOPOLOGY_STRING_FORMAT_STDOUT,
  LIBBENCHMARK_TOPOLOGY_STRING_FORMAT_GNUPLOT
};

enum libbenchmark_topology_numa_mode
{
  LIBBENCHMARK_TOPOLOGY_NUMA_MODE_SMP,
  LIBBENCHMARK_TOPOLOGY_NUMA_MODE_NUMA,
  LIBBENCHMARK_TOPOLOGY_NUMA_MODE_NUMA_BUT_NOT_USED
};

enum libbenchmark_topology_query
{
  LIBBENCHMARK_TOPOLOGY_QUERY_GET_NUMBER_OF_NODE_TYPE,
  LIBBENCHMARK_TOPOLOGY_QUERY_GET_NUMA_NODE_FOR_LOGICAL_PROCESSOR
};

/***** structs *****/
struct libbenchmark_topology_state
{
  int
    line_width;

  struct lfds710_btree_au_state
    lp_printing_offset_lookup_tree,
    topology_tree;
};

struct libbenchmark_topology_logical_processor_set
{
  struct lfds710_list_aso_state
    logical_processors;

  struct lfds710_list_asu_element
    lasue;
};

struct libbenchmark_topology_iterate_state
{
  enum libbenchmark_topology_node_type
    type;

  struct lfds710_btree_au_element
    *baue;
};

struct libbenchmark_topology_numa_node
{
  enum libbenchmark_topology_numa_mode
    mode;

  struct lfds710_list_asu_element
    lasue;
};

/***** public prototypes *****/
int libbenchmark_topology_init( struct libbenchmark_topology_state *ts, struct libshared_memory_state *ms );
void libbenchmark_topology_cleanup( struct libbenchmark_topology_state *ts );

void libbenchmark_topology_insert( struct libbenchmark_topology_state *ts, struct libbenchmark_topology_node_state *tns );

int libbenchmark_topology_compare_logical_processor_function( void const *new_key, void const *existing_key );

void libbenchmark_topology_generate_deduplicated_logical_processor_sets( struct libbenchmark_topology_state *ts, struct libshared_memory_state *ms, struct lfds710_list_asu_state *lp_sets );

void libbenchmark_topology_generate_numa_modes_list( struct libbenchmark_topology_state *ts, enum libbenchmark_topology_numa_mode numa_mode, struct libshared_memory_state *ms, struct lfds710_list_asu_state *numa_modes_list );

char *libbenchmark_topology_generate_string( struct libbenchmark_topology_state *ts, struct libshared_memory_state *ms, enum libbenchmark_topology_string_format format );
char *libbenchmark_topology_generate_lpset_string( struct libbenchmark_topology_state *ts, struct libshared_memory_state *ms, struct lfds710_list_aso_state *lpset );

int libbenchmark_topology_compare_lp_printing_offsets_function( void const *new_key, void const *existing_key );
int libbenchmark_topology_compare_node_against_lp_printing_offset_function( void const *new_key, void const *existing_key );

void libbenchmark_topology_iterate_init( struct libbenchmark_topology_iterate_state *tis, enum libbenchmark_topology_node_type type );
int libbenchmark_topology_iterate( struct libbenchmark_topology_state *ts, struct libbenchmark_topology_iterate_state *tis, struct libbenchmark_topology_node_state **tns );

void libbenchmark_topology_query( struct libbenchmark_topology_state *ts, enum libbenchmark_topology_query query_type, void *query_input, void *query_output );

