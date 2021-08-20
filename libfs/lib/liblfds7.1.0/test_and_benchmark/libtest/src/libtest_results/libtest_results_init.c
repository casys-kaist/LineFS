/***** includes *****/
#include "libtest_results_internal.h"





/****************************************************************************/
void libtest_results_init( struct libtest_results_state *rs )
{
  enum libtest_test_id
    test_id;

  LFDS710_PAL_ASSERT( rs != NULL );

  for( test_id = 0 ; test_id < LIBTEST_TEST_ID_COUNT ; test_id++ )
    rs->dvs[test_id] = LFDS710_MISC_VALIDITY_UNKNOWN;

  return;
}

