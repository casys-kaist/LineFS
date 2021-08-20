/***** defines *****/

/***** enums *****/

/***** structs *****/
struct libtest_testsuite_state
{
  enum flag
    test_available_flag[LIBTEST_TEST_ID_COUNT];

  struct lfds710_list_asu_state
    list_of_logical_processors;

  struct libshared_memory_state
    *ms;

  struct libtest_test_state
    tests[LIBTEST_TEST_ID_COUNT];

  void
    (*callback_test_start)( char *test_name ),
    (*callback_test_finish)( char *result );
};

/***** public prototypes *****/
void libtest_testsuite_init( struct libtest_testsuite_state *ts,
                             struct libshared_memory_state *ms,
                             void (*callback_test_start)(char *test_name),
                             void (*callback_test_finish)(char *result) );
void libtest_testsuite_cleanup( struct libtest_testsuite_state *ts );

void libtest_testsuite_run( struct libtest_testsuite_state *ts,
                            struct libtest_results_state *rs );

