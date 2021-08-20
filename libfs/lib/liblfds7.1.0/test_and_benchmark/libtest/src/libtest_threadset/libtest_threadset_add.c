/***** includes *****/
#include "libtest_threadset_internal.h"





/****************************************************************************/
void libtest_threadset_add_thread( struct libtest_threadset_state *ts,
                                   struct libtest_threadset_per_thread_state *pts,
                                   struct libtest_logical_processor *lp,
                                   libshared_pal_thread_return_t (LIBSHARED_PAL_THREAD_CALLING_CONVENTION *thread_function)( void *thread_user_state ),
                                   void *user_state )
{
  LFDS710_PAL_ASSERT( ts != NULL );
  LFDS710_PAL_ASSERT( pts != NULL );
  LFDS710_PAL_ASSERT( lp != NULL );
  LFDS710_PAL_ASSERT( thread_function != NULL );
  // TRD : user_state can be NULL

  pts->thread_ready_flag = LOWERED;
  pts->threadset_start_flag = &ts->threadset_start_flag;
  pts->pti.logical_processor_number = lp->logical_processor_number;
  pts->pti.windows_processor_group_number = lp->windows_processor_group_number;
  pts->pti.thread_function = thread_function;
  pts->ts = ts;
  pts->pti.thread_argument = pts;
  pts->user_state = user_state;

  LFDS710_LIST_ASU_SET_VALUE_IN_ELEMENT( pts->lasue, pts );
  lfds710_list_asu_insert_at_start( &ts->list_of_per_thread_states, &pts->lasue );

  return;
}

