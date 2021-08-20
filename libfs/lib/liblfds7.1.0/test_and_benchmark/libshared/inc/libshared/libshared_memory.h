/***** enums *****/
enum libshared_memory_query
{
  LIBSHARED_MEMORY_QUERY_GET_AVAILABLE
};

/***** structs *****/
struct libshared_memory_element
{
  char unsigned
    *original,
    *original_after_me_alloc,
    *current_pointer, // TRD : "_pointer" on the end 'cause Linux kernel has lower case defines one of which is "current"
    *rollback;

  enum flag
    known_numa_node_flag;

  lfds710_pal_uint_t
    current_memory_size_in_bytes,
    numa_node_id,
    original_memory_size_in_bytes,
    original_after_me_alloc_memory_size_in_bytes,
    rollback_memory_size_in_bytes;

  struct lfds710_list_asu_element
    lasue;
};

struct libshared_memory_state
{
  struct lfds710_list_asu_state
    list_of_allocations;
};

/***** public prototypes *****/
void libshared_memory_init( struct libshared_memory_state *ms );
void libshared_memory_cleanup( struct libshared_memory_state *ms,
                               void (*memory_cleanup_callback)(enum flag known_numa_node_flag,
                                                               void *store,
                                                               lfds710_pal_uint_t size) );

void libshared_memory_add_memory( struct libshared_memory_state *ms,
                                  void *memory,
                                  lfds710_pal_uint_t memory_size_in_bytes );
void libshared_memory_add_memory_from_numa_node( struct libshared_memory_state *ms,
                                                 lfds710_pal_uint_t numa_node_id,
                                                 void *memory,
                                                 lfds710_pal_uint_t memory_size_in_bytes );

void *libshared_memory_alloc_from_unknown_node( struct libshared_memory_state *ms, lfds710_pal_uint_t size_in_bytes, lfds710_pal_uint_t alignment_in_bytes );
void *libshared_memory_alloc_from_specific_node( struct libshared_memory_state *ms, lfds710_pal_uint_t numa_node_id, lfds710_pal_uint_t size_in_bytes, lfds710_pal_uint_t alignment_in_bytes );
void *libshared_memory_alloc_from_most_free_space_node( struct libshared_memory_state *ms, lfds710_pal_uint_t size_in_bytes, lfds710_pal_uint_t alignment_in_bytes );
void *libshared_memory_alloc_largest_possible_array_from_unknown_node( struct libshared_memory_state *ms, lfds710_pal_uint_t element_size_in_bytes, lfds710_pal_uint_t alignment_in_bytes, lfds710_pal_uint_t *number_elements );

void libshared_memory_set_rollback( struct libshared_memory_state *ms );
void libshared_memory_rollback( struct libshared_memory_state *ms );

void libshared_memory_query( struct libshared_memory_state *ms,
                             enum libshared_memory_query query_type,
                             void *query_input,
                             void *query_output );

