/***** includes *****/
#include "libtest_results_internal.h"





/****************************************************************************/
void libtest_results_put_result( struct libtest_results_state *rs, enum libtest_test_id test_id, enum lfds710_misc_validity result )
{
  LFDS710_PAL_ASSERT( rs != NULL );
  // TRD : test_id can be any value in its range
  // TRD : result can be any value in its range

  rs->dvs[test_id] = result;

  return;
}

