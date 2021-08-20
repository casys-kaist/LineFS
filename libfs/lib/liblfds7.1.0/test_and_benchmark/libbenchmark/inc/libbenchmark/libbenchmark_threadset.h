/***** defines *****/
#define LIBBENCHMARK_THREADSET_PER_THREAD_STATE_GET_USERS_PER_THREAD_STATE( libbenchmark_threadset_per_thread_state )   (libbenchmark_threadset_per_thread_state).users_per_thread_state
#define LIBBENCHMARK_THREADSET_PER_THREAD_STATE_GET_USERS_PER_NUMA_STATE( libbenchmark_threadset_per_thread_state )     (libbenchmark_threadset_per_thread_state).numa_node_state->users_per_numa_state
#define LIBBENCHMARK_THREADSET_PER_THREAD_STATE_GET_USERS_OVERALL_STATE( libbenchmark_threadset_per_thread_state )      (libbenchmark_threadset_per_thread_state).threadset_state->users_threadset_state

/***** structs *****/
struct libbenchmark_threadset_per_thread_state
{
  enum flag volatile
    thread_ready_flag,
    *threadset_start_flag;

  libshared_pal_thread_handle_t
    thread_handle;

  struct lfds710_list_asu_element
    lasue;

  struct libbenchmark_topology_node_state
    *tns_lp;

  struct libbenchmark_threadset_per_numa_state
    *numa_node_state;

  struct libbenchmark_threadset_state
    *threadset_state;

  struct libshared_pal_thread_info
    pti;

  void
    *users_per_thread_state;
};

struct libbenchmark_threadset_per_numa_state
{
  lfds710_pal_uint_t
    numa_node_id;

  struct lfds710_list_asu_element
    lasue;

  void
    *users_per_numa_state;
};

struct libbenchmark_threadset_state
{
  enum flag volatile
    threadset_start_flag;

  libshared_pal_thread_return_t
    (LIBSHARED_PAL_THREAD_CALLING_CONVENTION *thread_function)( void *thread_user_state );

  struct lfds710_list_asu_state
    list_of_per_numa_states,
    list_of_per_thread_states;

  void
    *users_threadset_state;
};

/***** prototypes *****/
void libbenchmark_threadset_init( struct libbenchmark_threadset_state *tsets,
                                  struct libbenchmark_topology_state *ts,
                                  struct lfds710_list_aso_state *logical_processor_set,
                                  struct libshared_memory_state *ms,
                                  libshared_pal_thread_return_t (LIBSHARED_PAL_THREAD_CALLING_CONVENTION *thread_function)( void *thread_user_state ),
                                  void *users_threadset_state );
void libbenchmark_threadset_cleanup( struct libbenchmark_threadset_state *ts );

void libbenchmark_threadset_run( struct libbenchmark_threadset_state *tsets );

void libbenchmark_threadset_thread_ready_and_wait( struct libbenchmark_threadset_per_thread_state *ts );

