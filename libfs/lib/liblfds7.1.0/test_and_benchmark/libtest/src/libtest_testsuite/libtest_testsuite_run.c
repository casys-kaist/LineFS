/***** includes *****/
#include "libtest_testsuite_internal.h"





/****************************************************************************/
void libtest_testsuite_run( struct libtest_testsuite_state *ts, struct libtest_results_state *rs )
{
  enum libtest_test_id
    test_id;

  LFDS710_PAL_ASSERT( ts != NULL );
  LFDS710_PAL_ASSERT( rs != NULL );

  for( test_id = 0 ; test_id < LIBTEST_TEST_ID_COUNT ; test_id++ )
    if( ts->test_available_flag[test_id] == RAISED )
    {
      libshared_memory_set_rollback( ts->ms );

      if( ts->callback_test_start != NULL )
        ts->callback_test_start( ts->tests[test_id].name );

      libtest_test_run( &ts->tests[test_id], &ts->list_of_logical_processors, ts->ms, &rs->dvs[test_id] );

      if( ts->callback_test_finish != NULL )
        ts->callback_test_finish( libtest_misc_global_validity_names[ rs->dvs[test_id] ] );

      libshared_memory_rollback( ts->ms );
    }

  return;
}

