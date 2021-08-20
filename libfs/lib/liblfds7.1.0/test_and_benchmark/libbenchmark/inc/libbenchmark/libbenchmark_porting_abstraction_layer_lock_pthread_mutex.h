/****************************************************************************/
#if( defined _POSIX_THREADS && _POSIX_THREADS >= 0 && !defined KERNEL_MODE )

  /* TRD : POSIX threads

           _POSIX_THREADS  indicates POSIX threads
                             - pthreads_mutex_init requires POSIX
                             - pthreads_mutex_destroy requires POSIX
                             - pthreads_mutex_lock requires POSIX
                             - pthreads_mutex_unlock requires POSIX
  */

  /***** defines *****/
  #define LIBBENCHMARK_PAL_LOCK_PTHREAD_MUTEX 1

  #define LIBBENCHMARK_PAL_LOCK_PTHREAD_MUTEX_CREATE( pal_lock_pthread_mutex_state )   pthread_mutex_init( &pal_lock_pthread_mutex_state, NULL )
  #define LIBBENCHMARK_PAL_LOCK_PTHREAD_MUTEX_DESTROY( pal_lock_pthread_mutex_state )  pthread_mutex_destroy( &pal_lock_pthread_mutex_state )
  #define LIBBENCHMARK_PAL_LOCK_PTHREAD_MUTEX_GET( pal_lock_pthread_mutex_state )      pthread_mutex_lock( &pal_lock_pthread_mutex_state )
  #define LIBBENCHMARK_PAL_LOCK_PTHREAD_MUTEX_RELEASE( pal_lock_pthread_mutex_state )  pthread_mutex_unlock( &pal_lock_pthread_mutex_state )

  /***** typedefs *****/
  typedef pthread_mutex_t pal_lock_pthread_mutex_state;

#endif





/****************************************************************************/
#if( !defined LIBBENCHMARK_PAL_LOCK_PTHREAD_MUTEX )

  /***** defines *****/
  #define LIBBENCHMARK_PAL_LOCK_PTHREAD_MUTEX 0

  #define LIBBENCHMARK_PAL_LOCK_PTHREAD_MUTEX_CREATE( pal_lock_pthread_mutex_state )
  #define LIBBENCHMARK_PAL_LOCK_PTHREAD_MUTEX_DESTROY( pal_lock_pthread_mutex_state )
  #define LIBBENCHMARK_PAL_LOCK_PTHREAD_MUTEX_GET( pal_lock_pthread_mutex_state )
  #define LIBBENCHMARK_PAL_LOCK_PTHREAD_MUTEX_RELEASE( pal_lock_pthread_mutex_state )

  /***** typedefs *****/
  typedef void * pal_lock_pthread_mutex_state;

#endif

