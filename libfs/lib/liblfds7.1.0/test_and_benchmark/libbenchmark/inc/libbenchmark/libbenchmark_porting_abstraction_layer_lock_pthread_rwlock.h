/****************************************************************************/
#if( defined _POSIX_THREADS && _POSIX_THREADS >= 0 && !defined KERNEL_MODE )

  /* TRD : POSIX threads

           _POSIX_THREADS  indicates POSIX threads
                             - pthreads_rwlock_init requires POSIX
                             - pthreads_rwlock_destroy requires POSIX
                             - pthreads_rwlock_lock requires POSIX
                             - pthreads_rwlock_unlock requires POSIX
  */

  /***** defines *****/
  #define LIBBENCHMARK_PAL_LOCK_PTHREAD_RWLOCK 1

  #define LIBBENCHMARK_PAL_LOCK_PTHREAD_RWLOCK_CREATE( pal_lock_pthread_rwlock_state )     pthread_rwlock_init( &pal_lock_pthread_rwlock_state, NULL )
  #define LIBBENCHMARK_PAL_LOCK_PTHREAD_RWLOCK_DESTROY( pal_lock_pthread_rwlock_state )    pthread_rwlock_destroy( &pal_lock_pthread_rwlock_state )
  #define LIBBENCHMARK_PAL_LOCK_PTHREAD_RWLOCK_GET_READ( pal_lock_pthread_rwlock_state )   pthread_rwlock_rdlock( &pal_lock_pthread_rwlock_state )
  #define LIBBENCHMARK_PAL_LOCK_PTHREAD_RWLOCK_GET_WRITE( pal_lock_pthread_rwlock_state )  pthread_rwlock_wrlock( &pal_lock_pthread_rwlock_state )
  #define LIBBENCHMARK_PAL_LOCK_PTHREAD_RWLOCK_RELEASE( pal_lock_pthread_rwlock_state )    pthread_rwlock_unlock( &pal_lock_pthread_rwlock_state )

  /***** typedefs *****/
  typedef pthread_rwlock_t pal_lock_pthread_rwlock_state;

#endif





/****************************************************************************/
#if( !defined LIBBENCHMARK_PAL_LOCK_PTHREAD_RWLOCK )

  /***** defines *****/
  #define LIBBENCHMARK_PAL_LOCK_PTHREAD_RWLOCK 0

  #define LIBBENCHMARK_PAL_LOCK_PTHREAD_RWLOCK_CREATE( pal_lock_pthread_rwlock_state )
  #define LIBBENCHMARK_PAL_LOCK_PTHREAD_RWLOCK_DESTROY( pal_lock_pthread_rwlock_state )
  #define LIBBENCHMARK_PAL_LOCK_PTHREAD_RWLOCK_GET_READ( pal_lock_pthread_rwlock_state )
  #define LIBBENCHMARK_PAL_LOCK_PTHREAD_RWLOCK_GET_WRITE( pal_lock_pthread_rwlock_state )
  #define LIBBENCHMARK_PAL_LOCK_PTHREAD_RWLOCK_RELEASE( pal_lock_pthread_rwlock_state )

  /***** typedefs *****/
  typedef void * pal_lock_pthread_rwlock_state;

#endif

