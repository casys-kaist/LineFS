/***** includes *****/
#include "libbenchmark_datastructure_freelist_internal.h"





/****************************************************************************/
void libbenchmark_datastructure_freelist_pthread_spinlock_process_shared_init( struct libbenchmark_datastructure_freelist_pthread_spinlock_process_shared_state *fs, void *user_state )
{
  LFDS710_PAL_ASSERT( fs != NULL );
  LFDS710_PAL_ASSERT( user_state == NULL );

  fs->top = NULL;
  fs->user_state = user_state;

  LIBBENCHMARK_PAL_LOCK_PTHREAD_SPINLOCK_PROCESS_SHARED_CREATE( fs->lock );

  LFDS710_MISC_BARRIER_STORE;

  lfds710_misc_force_store();

  return;
}





/****************************************************************************/
void libbenchmark_datastructure_freelist_pthread_spinlock_process_shared_cleanup( struct libbenchmark_datastructure_freelist_pthread_spinlock_process_shared_state *fs, void (*element_pop_callback)(struct libbenchmark_datastructure_freelist_pthread_spinlock_process_shared_state *fs, struct libbenchmark_datastructure_freelist_pthread_spinlock_process_shared_element *fe, void *user_state) )
{
  struct libbenchmark_datastructure_freelist_pthread_spinlock_process_shared_element
    *fe,
    *fe_temp;

  LFDS710_PAL_ASSERT( fs != NULL );
  // TRD : element_pop_callback can be NULL

  LFDS710_MISC_BARRIER_LOAD;

  if( element_pop_callback != NULL )
  {
    fe = fs->top;

    while( fe != NULL )
    {
      fe_temp = fe;
      fe = fe->next;

      element_pop_callback( fs, fe_temp, (void *) fs->user_state );
    }
  }

  LIBBENCHMARK_PAL_LOCK_PTHREAD_SPINLOCK_PROCESS_SHARED_DESTROY( fs->lock );

  return;
}





/****************************************************************************/
void libbenchmark_datastructure_freelist_pthread_spinlock_process_shared_push( struct libbenchmark_datastructure_freelist_pthread_spinlock_process_shared_state *fs, struct libbenchmark_datastructure_freelist_pthread_spinlock_process_shared_element *fe )
{
  LFDS710_PAL_ASSERT( fs != NULL );
  LFDS710_PAL_ASSERT( fe != NULL );

  LIBBENCHMARK_PAL_LOCK_PTHREAD_SPINLOCK_PROCESS_SHARED_GET( fs->lock );

  fe->next = fs->top;
  fs->top = fe;

  LIBBENCHMARK_PAL_LOCK_PTHREAD_SPINLOCK_PROCESS_SHARED_RELEASE( fs->lock );

  return;
}





/****************************************************************************/
#pragma warning( disable : 4100 )

int libbenchmark_datastructure_freelist_pthread_spinlock_process_shared_pop( struct libbenchmark_datastructure_freelist_pthread_spinlock_process_shared_state *fs, struct libbenchmark_datastructure_freelist_pthread_spinlock_process_shared_element **fe )
{
  int
    rv = 1;

  LFDS710_PAL_ASSERT( fs != NULL );
  LFDS710_PAL_ASSERT( fe != NULL );

  LIBBENCHMARK_PAL_LOCK_PTHREAD_SPINLOCK_PROCESS_SHARED_GET( fs->lock );

  *fe = fs->top;

  if( fs->top != NULL )
    fs->top = fs->top->next;
  else
    rv = 0;

  LIBBENCHMARK_PAL_LOCK_PTHREAD_SPINLOCK_PROCESS_SHARED_RELEASE( fs->lock );

  return rv;
}

#pragma warning( default : 4100 )

