/***** includes *****/
#include "libtest_threadset_internal.h"





/****************************************************************************/
void libtest_threadset_init( struct libtest_threadset_state *ts,
                             void *user_state )
{
  LFDS710_PAL_ASSERT( ts != NULL );
  // TRD : user_state can be NULL

  ts->threadset_start_flag = LOWERED;
  ts->user_state = user_state;

  lfds710_list_asu_init_valid_on_current_logical_core( &ts->list_of_per_thread_states, NULL );

  return;
}

