/***** defines *****/
#define LIBBENCHMARK_DATA_STRUCTURE_FREELIST_WINDOWS_MUTEX_GET_KEY_FROM_ELEMENT( freelist_element )             ( (freelist_element).key )
#define LIBBENCHMARK_DATA_STRUCTURE_FREELIST_WINDOWS_MUTEX_SET_KEY_IN_ELEMENT( freelist_element, new_key )      ( (freelist_element).key = (void *) (lfds710_pal_uint_t) (new_key) )
#define LIBBENCHMARK_DATA_STRUCTURE_FREELIST_WINDOWS_MUTEX_GET_VALUE_FROM_ELEMENT( freelist_element )           ( (freelist_element).value )
#define LIBBENCHMARK_DATA_STRUCTURE_FREELIST_WINDOWS_MUTEX_SET_VALUE_IN_ELEMENT( freelist_element, new_value )  ( (freelist_element).value = (void *) (lfds710_pal_uint_t) (new_value) )
#define LIBBENCHMARK_DATA_STRUCTURE_FREELIST_WINDOWS_MUTEX_GET_USER_STATE_FROM_STATE( freelist_state )          ( (freelist_state).user_state )

/***** structures *****/
struct libbenchmark_datastructure_freelist_windows_mutex_state
{
  struct libbenchmark_datastructure_freelist_windows_mutex_element
    *top;

  pal_lock_windows_mutex_state
    lock;

  void
    *user_state;
};

struct libbenchmark_datastructure_freelist_windows_mutex_element
{
  struct libbenchmark_datastructure_freelist_windows_mutex_element
    *next;

  void
    *key,
    *value;
};

/***** public prototypes *****/
void libbenchmark_datastructure_freelist_windows_mutex_init( struct libbenchmark_datastructure_freelist_windows_mutex_state *fs, void *user_state );
void libbenchmark_datastructure_freelist_windows_mutex_cleanup( struct libbenchmark_datastructure_freelist_windows_mutex_state *fs, void (*element_pop_callback)(struct libbenchmark_datastructure_freelist_windows_mutex_state *fs, struct libbenchmark_datastructure_freelist_windows_mutex_element *fe, void *user_state) );

void libbenchmark_datastructure_freelist_windows_mutex_push( struct libbenchmark_datastructure_freelist_windows_mutex_state *fs, struct libbenchmark_datastructure_freelist_windows_mutex_element *fe );
int libbenchmark_datastructure_freelist_windows_mutex_pop( struct libbenchmark_datastructure_freelist_windows_mutex_state *fs, struct libbenchmark_datastructure_freelist_windows_mutex_element **fe );

