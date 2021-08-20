/***** defines *****/
#define LIBBENCHMARK_TOPOLOGY_NODE_GET_TYPE( tns, node_type )           (tns).type
#define LIBBENCHMARK_TOPOLOGY_NODE_SET_TYPE( tns, node_type )           (tns).type = (node_type)

#define LIBBENCHMARK_TOPOLOGY_NODE_GET_CACHE_TYPE( tns, cache_type )    (tns).extended_node_info.cache.type
#define LIBBENCHMARK_TOPOLOGY_NODE_SET_CACHE_TYPE( tns, cache_type )    (tns).extended_node_info.cache.type = (cache_type)

#define LIBBENCHMARK_TOPOLOGY_NODE_GET_CACHE_LEVEL( tns, cache_level )  (tns).extended_node_info.cache.level
#define LIBBENCHMARK_TOPOLOGY_NODE_SET_CACHE_LEVEL( tns, cache_level )  (tns).extended_node_info.cache.level = (cache_level)

#define LIBBENCHMARK_TOPOLOGY_NODE_GET_NUMA_ID( tns )                   (tns).extended_node_info.numa.id
#define LIBBENCHMARK_TOPOLOGY_NODE_SET_NUMA_ID( tns, numa_node_id )     (tns).extended_node_info.numa.id = (numa_node_id)

#define LIBBENCHMARK_TOPOLOGY_NODE_GET_LOGICAL_PROCESSOR_NUMBER( tns )                    (tns).extended_node_info.logical_processor.number
#define LIBBENCHMARK_TOPOLOGY_NODE_SET_LOGICAL_PROCESSOR_NUMBER( tns, processor_number )  (tns).extended_node_info.logical_processor.number = (processor_number)

#define LIBBENCHMARK_TOPOLOGY_NODE_GET_WINDOWS_GROUP_NUMBER( tns )                    (tns).extended_node_info.logical_processor.windows_group_number
#define LIBBENCHMARK_TOPOLOGY_NODE_SET_WINDOWS_GROUP_NUMBER( tns, win_group_number )  (tns).extended_node_info.logical_processor.windows_group_number = (win_group_number), (tns).extended_node_info.logical_processor.windows_group_number_set_flag = RAISED
#define LIBBENCHMARK_TOPOLOGY_NODE_UNSET_WINDOWS_GROUP_NUMBER( tns )                  LIBBENCHMARK_TOPOLOGY_NODE_SET_WINDOWS_GROUP_NUMBER( tns, 0 ), (tns).extended_node_info.logical_processor.windows_group_number_set_flag = LOWERED
#define LIBBENCHMARK_TOPOLOGY_NODE_IS_WINDOWS_GROUP_NUMBER( tns )                     (tns).extended_node_info.logical_processor.windows_group_number_set_flag

/***** enums *****/
enum libbenchmark_topology_node_type
{
  LIBBENCHMARK_TOPOLOGY_NODE_TYPE_LOGICAL_PROCESSOR,
  LIBBENCHMARK_TOPOLOGY_NODE_TYPE_CACHE,
  LIBBENCHMARK_TOPOLOGY_NODE_TYPE_PHYSICAL_PROCESSOR,
  LIBBENCHMARK_TOPOLOGY_NODE_TYPE_SOCKET,
  LIBBENCHMARK_TOPOLOGY_NODE_TYPE_NUMA,
  LIBBENCHMARK_TOPOLOGY_NODE_TYPE_SYSTEM
};

enum libbenchmark_topology_node_cache_type
{
  LIBBENCHMARK_TOPOLOGY_NODE_CACHE_TYPE_DATA,
  LIBBENCHMARK_TOPOLOGY_NODE_CACHE_TYPE_INSTRUCTION,
  LIBBENCHMARK_TOPOLOGY_NODE_CACHE_TYPE_UNIFIED,
  LIBBENCHMARK_TOPOLOGY_NODE_CACHE_TYPE_COUNT
};

enum libbenchmark_topology_logical_processor_set_encoding
{
  LIBBENCHMARK_TOPOLOGY_LOGICAL_PROCESSOR_SET_ENCODING_BITMASK,
  LIBBENCHMARK_TOPOLOGY_LOGICAL_PROCESSOR_SET_ENCODING_PATH_TO_CSV_HEX,
  LIBBENCHMARK_TOPOLOGY_LOGICAL_PROCESSOR_SET_ENCODING_SINGLE_LOGICAL_PROCESSOR
};

/***** structs *****/
struct libbenchmark_topology_node_cache
{
  enum libbenchmark_topology_node_cache_type
    type;

  lfds710_pal_uint_t
    level;
};

struct libbenchmark_topology_node_logical_processor
{
  enum flag
    windows_group_number_set_flag;

  lfds710_pal_uint_t
    number,
    windows_group_number;
};

struct libbenchmark_topology_node_numa
{
  lfds710_pal_uint_t
    id;
};

// TRD : most node types just *are* (a socket is a socket, etc), but caches, NUMA nodes and LPs have some extra info
union libbenchmark_topology_node_extended_info
{
  struct libbenchmark_topology_node_cache
    cache;

  struct libbenchmark_topology_node_logical_processor
    logical_processor;

  struct libbenchmark_topology_node_numa
    numa;
};

struct libbenchmark_topology_node_state
{
  enum libbenchmark_topology_node_type
    type;

  struct lfds710_btree_au_element
    baue;

  struct lfds710_list_aso_element
    lasoe;

  struct lfds710_list_aso_state
    logical_processor_children;

  union libbenchmark_topology_node_extended_info
    extended_node_info;
};

/***** public prototypes *****/
void libbenchmark_topology_node_init( struct libbenchmark_topology_node_state *tns );
void libbenchmark_topology_node_cleanup( struct libbenchmark_topology_node_state *tns, void (*element_cleanup_callback)(struct lfds710_list_aso_state *lasos, struct lfds710_list_aso_element *lasoe) );

int libbenchmark_topology_node_compare_nodes_function( void const *new_key, void const *existing_key );
int libbenchmark_topology_node_compare_node_types_function( void const *new_key, void const *existing_key );

int libbenchmark_topology_node_compare_lpsets_function( struct lfds710_list_aso_state *lpset_one, struct lfds710_list_aso_state *lpset_two );
  // TRD : only used in results compare function, where we comapre two lists of nodes which are logical prceossors

