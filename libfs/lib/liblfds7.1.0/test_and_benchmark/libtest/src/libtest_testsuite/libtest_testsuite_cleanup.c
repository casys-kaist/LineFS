/***** includes *****/
#include "libtest_testsuite_internal.h"





/****************************************************************************/
void libtest_testsuite_cleanup( struct libtest_testsuite_state *ts )
{
  LFDS710_PAL_ASSERT( ts != NULL );

  lfds710_list_asu_cleanup( &ts->list_of_logical_processors, NULL );

  return;
}

