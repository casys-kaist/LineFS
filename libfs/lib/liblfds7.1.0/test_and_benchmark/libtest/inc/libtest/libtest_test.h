/***** defines *****/

/***** structs *****/
struct libtest_test_state
{
  char
    *name;

  enum libtest_test_id
    test_id;

  void
    (*test_function)( struct lfds710_list_asu_state *list_of_logical_processors, struct libshared_memory_state *ms, enum lfds710_misc_validity *dvs );
};

/***** public prototypes *****/
void libtest_test_init( struct libtest_test_state *ts,
                        char *name,
                        enum libtest_test_id test_id,
                        void (*test_function)(struct lfds710_list_asu_state *list_of_logical_processors, struct libshared_memory_state *ms, enum lfds710_misc_validity *dvs) );

void libtest_test_cleanup( struct libtest_test_state *ts );

void libtest_test_run( struct libtest_test_state *ts,
                       struct lfds710_list_asu_state *list_of_logical_processors,
                       struct libshared_memory_state *ms,
                       enum lfds710_misc_validity *dvs );

