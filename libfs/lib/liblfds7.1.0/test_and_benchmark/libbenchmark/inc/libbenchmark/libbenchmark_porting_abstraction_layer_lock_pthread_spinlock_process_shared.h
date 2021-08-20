/****************************************************************************/
#if( defined _POSIX_SPIN_LOCKS && _POSIX_SPIN_LOCKS >= 0 && !defined KERNEL_MODE )

  /* TRD : POSIX spin locks

           _POSIX_SPIN_LOCKS  indicates POSIX spin locks
                                - pthreads_spin_init requires POSIX
                                - pthreads_spin_destroy requires POSIX
                                - pthreads_spin_lock requires POSIX
                                - pthreads_spin_unlock requires POSIX
  */

  /***** defines *****/
  #define LIBBENCHMARK_PAL_LOCK_PTHREAD_SPINLOCK_PROCESS_SHARED 1

  #define LIBBENCHMARK_PAL_LOCK_PTHREAD_SPINLOCK_PROCESS_SHARED_CREATE( pal_lock_pthread_spinlock_process_shared_state )   pthread_spin_init( &pal_lock_pthread_spinlock_process_shared_state, PTHREAD_PROCESS_SHARED );
  #define LIBBENCHMARK_PAL_LOCK_PTHREAD_SPINLOCK_PROCESS_SHARED_DESTROY( pal_lock_pthread_spinlock_process_shared_state )  pthread_spin_destroy( &pal_lock_pthread_spinlock_process_shared_state );
  #define LIBBENCHMARK_PAL_LOCK_PTHREAD_SPINLOCK_PROCESS_SHARED_GET( pal_lock_pthread_spinlock_process_shared_state )      pthread_spin_lock( &pal_lock_pthread_spinlock_process_shared_state );
  #define LIBBENCHMARK_PAL_LOCK_PTHREAD_SPINLOCK_PROCESS_SHARED_RELEASE( pal_lock_pthread_spinlock_process_shared_state )  pthread_spin_unlock( &pal_lock_pthread_spinlock_process_shared_state );

  /***** typedefs *****/
  typedef pthread_spinlock_t pal_lock_pthread_spinlock_process_shared_state;

#endif





/****************************************************************************/
#if( !defined LIBBENCHMARK_PAL_LOCK_PTHREAD_SPINLOCK_PROCESS_SHARED )

  /***** defines *****/
  #define LIBBENCHMARK_PAL_LOCK_PTHREAD_SPINLOCK_PROCESS_SHARED 0

  #define LIBBENCHMARK_PAL_LOCK_PTHREAD_SPINLOCK_PROCESS_SHARED_CREATE( pal_lock_pthread_spinlock_process_shared_state )
  #define LIBBENCHMARK_PAL_LOCK_PTHREAD_SPINLOCK_PROCESS_SHARED_DESTROY( pal_lock_pthread_spinlock_process_shared_state )
  #define LIBBENCHMARK_PAL_LOCK_PTHREAD_SPINLOCK_PROCESS_SHARED_GET( pal_lock_pthread_spinlock_process_shared_state )
  #define LIBBENCHMARK_PAL_LOCK_PTHREAD_SPINLOCK_PROCESS_SHARED_RELEASE( pal_lock_pthread_spinlock_process_shared_state )

  /***** typedefs *****/
  typedef void * pal_lock_pthread_spinlock_process_shared_state;

#endif

