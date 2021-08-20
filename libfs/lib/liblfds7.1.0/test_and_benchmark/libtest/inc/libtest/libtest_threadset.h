/***** defines *****/
#define LIBTEST_THREADSET_GET_USER_STATE_FROM_PER_THREAD_STATE( libtest_threadset_per_thread_state )  (libtest_threadset_per_thread_state).user_state
#define LIBTEST_THREADSET_SET_USER_STATE( libtest_threadtest_state, userstate )                       (libtest_threadtest_state).user_state = userstate

/***** structs *****/
struct libtest_threadset_per_thread_state
{
  enum flag volatile
    thread_ready_flag,
    *threadset_start_flag;

  libshared_pal_thread_handle_t
    thread_handle;

  struct lfds710_list_asu_element
    lasue;

  struct libshared_pal_thread_info
    pti;

  struct libtest_threadset_state
    *ts;

  void
    *user_state;
};

struct libtest_threadset_state
{
  enum flag volatile
    threadset_start_flag;

  struct lfds710_list_asu_state
    list_of_per_thread_states;

  struct libshared_memory_state
    *ms;

  void
    *user_state;
};

/***** prototypes *****/
void libtest_threadset_init( struct libtest_threadset_state *ts,
                             void *user_state );
void libtest_threadset_cleanup( struct libtest_threadset_state *ts );

void libtest_threadset_add_thread( struct libtest_threadset_state *ts,
                                   struct libtest_threadset_per_thread_state *pts,
                                   struct libtest_logical_processor *lp,
                                   libshared_pal_thread_return_t (LIBSHARED_PAL_THREAD_CALLING_CONVENTION *thread_function)( void *thread_user_state ),
                                   void *user_state );

void libtest_threadset_run( struct libtest_threadset_state *ts );
void libtest_threadset_thread_ready_and_wait( struct libtest_threadset_per_thread_state *pts );

