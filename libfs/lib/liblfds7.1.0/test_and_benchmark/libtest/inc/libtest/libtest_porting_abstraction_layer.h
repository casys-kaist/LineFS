/***** defines *****/
#define LIBTEST_PAL_SET_LOGICAL_PROCESSOR_NUMBER( libtest_logical_processor, logical_processor_number )              (libtest_logical_processor).logical_processor_number = logical_processor_number
#define LIBTEST_PAL_SET_WINDOWS_PROCESSOR_GROUP_NUMBER( libtest_logical_processor, windows_processor_group_number )  (libtest_logical_processor).windows_processor_group_number = windows_processor_group_number

/***** structs *****/
struct libtest_logical_processor
{
  lfds710_pal_uint_t
    logical_processor_number,
    windows_processor_group_number;

  struct lfds710_list_asu_element
    lasue;
};

struct libtest_thread_state
{
  libshared_pal_thread_handle_t
    thread_state;

  libshared_pal_thread_return_t
    (LIBSHARED_PAL_THREAD_CALLING_CONVENTION *thread_function)( void *thread_user_state );

  struct libtest_logical_processor
    lp;

  void
    *thread_user_state;
};

/***** public prototypes *****/
void libtest_pal_get_full_logical_processor_set( struct lfds710_list_asu_state *lasus, struct libshared_memory_state *ms );

void *libtest_pal_malloc( lfds710_pal_uint_t size );
void libtest_pal_free( void *memory );

