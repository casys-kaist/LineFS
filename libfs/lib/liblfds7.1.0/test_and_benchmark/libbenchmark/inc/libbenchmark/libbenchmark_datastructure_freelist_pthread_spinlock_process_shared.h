/***** defines *****/
#define LIBBENCHMARK_DATA_STRUCTURE_FREELIST_PTHREAD_SPINLOCK_PROCESS_SHARED_GET_KEY_FROM_ELEMENT( freelist_element )             ( (freelist_element).key )
#define LIBBENCHMARK_DATA_STRUCTURE_FREELIST_PTHREAD_SPINLOCK_PROCESS_SHARED_SET_KEY_IN_ELEMENT( freelist_element, new_key )      ( (freelist_element).key = (void *) (lfds710_pal_uint_t) (new_key) )
#define LIBBENCHMARK_DATA_STRUCTURE_FREELIST_PTHREAD_SPINLOCK_PROCESS_SHARED_GET_VALUE_FROM_ELEMENT( freelist_element )           ( (freelist_element).value )
#define LIBBENCHMARK_DATA_STRUCTURE_FREELIST_PTHREAD_SPINLOCK_PROCESS_SHARED_SET_VALUE_IN_ELEMENT( freelist_element, new_value )  ( (freelist_element).value = (void *) (lfds710_pal_uint_t) (new_value) )
#define LIBBENCHMARK_DATA_STRUCTURE_FREELIST_PTHREAD_SPINLOCK_PROCESS_SHARED_GET_USER_STATE_FROM_STATE( freelist_state )          ( (freelist_state).user_state )

/***** structures *****/
struct libbenchmark_datastructure_freelist_pthread_spinlock_process_shared_state
{
  struct libbenchmark_datastructure_freelist_pthread_spinlock_process_shared_element
    *top;

  pal_lock_pthread_spinlock_process_shared_state
    lock;

  void
    *user_state;
};

struct libbenchmark_datastructure_freelist_pthread_spinlock_process_shared_element
{
  struct libbenchmark_datastructure_freelist_pthread_spinlock_process_shared_element
    *next;

  void
    *key,
    *value;
};

/***** public prototypes *****/
void libbenchmark_datastructure_freelist_pthread_spinlock_process_shared_init( struct libbenchmark_datastructure_freelist_pthread_spinlock_process_shared_state *fs, void *user_state );
void libbenchmark_datastructure_freelist_pthread_spinlock_process_shared_cleanup( struct libbenchmark_datastructure_freelist_pthread_spinlock_process_shared_state *fs, void (*element_pop_callback)(struct libbenchmark_datastructure_freelist_pthread_spinlock_process_shared_state *fs, struct libbenchmark_datastructure_freelist_pthread_spinlock_process_shared_element *fe, void *user_state) );

void libbenchmark_datastructure_freelist_pthread_spinlock_process_shared_push( struct libbenchmark_datastructure_freelist_pthread_spinlock_process_shared_state *fs, struct libbenchmark_datastructure_freelist_pthread_spinlock_process_shared_element *fe );
int libbenchmark_datastructure_freelist_pthread_spinlock_process_shared_pop( struct libbenchmark_datastructure_freelist_pthread_spinlock_process_shared_state *fs, struct libbenchmark_datastructure_freelist_pthread_spinlock_process_shared_element **fe );

