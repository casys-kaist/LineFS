/***** includes *****/
#include "libtest_test_internal.h"





/****************************************************************************/
void libtest_test_run( struct libtest_test_state *ts,
                       struct lfds710_list_asu_state *list_of_logical_processors,
                       struct libshared_memory_state *ms,
                       enum lfds710_misc_validity *dvs )
{
  LFDS710_PAL_ASSERT( ts != NULL );
  LFDS710_PAL_ASSERT( list_of_logical_processors != NULL );
  LFDS710_PAL_ASSERT( ms != NULL );
  LFDS710_PAL_ASSERT( dvs != NULL );

  ts->test_function( list_of_logical_processors, ms, dvs );

  return;
}

