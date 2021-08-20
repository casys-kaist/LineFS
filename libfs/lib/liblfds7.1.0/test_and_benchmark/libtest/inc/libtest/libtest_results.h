/***** defines *****/

/***** enums *****/

/***** structs *****/
struct libtest_results_state
{
  enum lfds710_misc_validity
    dvs[LIBTEST_TEST_ID_COUNT];
};

/***** public prototypes *****/
void libtest_results_init( struct libtest_results_state *rs );
void libtest_results_cleanup( struct libtest_results_state *rs );

void libtest_results_put_result( struct libtest_results_state *rs,
                                 enum libtest_test_id test_id,
                                 enum lfds710_misc_validity result );
void libtest_results_get_result( struct libtest_results_state *rs,
                                 enum libtest_test_id test_id,
                                 enum lfds710_misc_validity *result );

