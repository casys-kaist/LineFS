/***** includes *****/
#include "libbenchmark_datastructure_queue_umm_internal.h"





/****************************************************************************/
void libbenchmark_datastructure_queue_umm_windows_mutex_init( struct libbenchmark_datastructure_queue_umm_windows_mutex_state *qs, struct libbenchmark_datastructure_queue_umm_windows_mutex_element *qe, void *user_state )
{
  LFDS710_PAL_ASSERT( qs != NULL );
  LFDS710_PAL_ASSERT( qe != NULL );
  LFDS710_PAL_ASSERT( user_state == NULL );

  qe->next = NULL;

  qs->enqueue_umm = qe;
  qs->dequeue_umm = qe;

  LIBBENCHMARK_PAL_LOCK_WINDOWS_MUTEX_CREATE( qs->lock_enqueue_umm );
  LIBBENCHMARK_PAL_LOCK_WINDOWS_MUTEX_CREATE( qs->lock_dequeue_umm );

  qs->user_state = user_state;

  LFDS710_MISC_BARRIER_STORE;

  lfds710_misc_force_store();

  return;
}





/****************************************************************************/
void libbenchmark_datastructure_queue_umm_windows_mutex_cleanup( struct libbenchmark_datastructure_queue_umm_windows_mutex_state *qs,
                                                void (*element_cleanup_callback)(struct libbenchmark_datastructure_queue_umm_windows_mutex_state *qs, struct libbenchmark_datastructure_queue_umm_windows_mutex_element *qe, enum flag dummy_element_flag) )
{
  struct libbenchmark_datastructure_queue_umm_windows_mutex_element
    *qe;

  LFDS710_PAL_ASSERT( qs != NULL );
  // TRD : element_cleanup_callback can be NULL

  LFDS710_MISC_BARRIER_LOAD;

  if( element_cleanup_callback != NULL )
    while( libbenchmark_datastructure_queue_umm_windows_mutex_dequeue_umm(qs, &qe) )
      element_cleanup_callback( qs, qe, LOWERED );

  LIBBENCHMARK_PAL_LOCK_WINDOWS_MUTEX_DESTROY( qs->lock_enqueue_umm );
  LIBBENCHMARK_PAL_LOCK_WINDOWS_MUTEX_DESTROY( qs->lock_dequeue_umm );

  return;
}





/****************************************************************************/
void libbenchmark_datastructure_queue_umm_windows_mutex_enqueue_umm( struct libbenchmark_datastructure_queue_umm_windows_mutex_state *qs, struct libbenchmark_datastructure_queue_umm_windows_mutex_element *qe )
{
  LFDS710_PAL_ASSERT( qs != NULL );
  LFDS710_PAL_ASSERT( qe != NULL );

  qe->next = NULL;

  LIBBENCHMARK_PAL_LOCK_WINDOWS_MUTEX_GET( qs->lock_enqueue_umm );

  qs->enqueue_umm->next = qe;
  qs->enqueue_umm = qe;

  LIBBENCHMARK_PAL_LOCK_WINDOWS_MUTEX_RELEASE( qs->lock_enqueue_umm );

  return;
}





/****************************************************************************/
int libbenchmark_datastructure_queue_umm_windows_mutex_dequeue_umm( struct libbenchmark_datastructure_queue_umm_windows_mutex_state *qs, struct libbenchmark_datastructure_queue_umm_windows_mutex_element **qe )
{
  int
    rv = 1;

  LFDS710_PAL_ASSERT( qs != NULL );
  LFDS710_PAL_ASSERT( qe != NULL );

  LIBBENCHMARK_PAL_LOCK_WINDOWS_MUTEX_GET( qs->lock_dequeue_umm );

  if( qs->dequeue_umm == qs->enqueue_umm )
  {
    *qe = NULL;
    rv = 0;
  }
  else
  {
    *qe = qs->dequeue_umm;
    (*qe)->key = qs->dequeue_umm->next->key;
    (*qe)->key = qs->dequeue_umm->next->value;
    qs->dequeue_umm = qs->dequeue_umm->next;
  }

  LIBBENCHMARK_PAL_LOCK_WINDOWS_MUTEX_RELEASE( qs->lock_dequeue_umm );

  return rv;
}

