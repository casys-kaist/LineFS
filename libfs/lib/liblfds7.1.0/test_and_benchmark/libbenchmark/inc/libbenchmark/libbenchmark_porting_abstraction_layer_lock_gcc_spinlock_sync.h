/****************************************************************************/
#if( defined __GNUC__ && LIBLFDS_GCC_VERSION >= 412 )

  /***** defines *****/
  #define LIBBENCHMARK_PAL_LOCK_GCC_SPINLOCK_SYNC 1

  #define LIBBENCHMARK_PAL_LOCK_GCC_SPINLOCK_SYNC_UNINITIALIZED  0
  #define LIBBENCHMARK_PAL_LOCK_GCC_SPINLOCK_SYNC_AVAILABLE      1
  #define LIBBENCHMARK_PAL_LOCK_GCC_SPINLOCK_SYNC_UNAVAILABLE    2

  #define LIBBENCHMARK_PAL_LOCK_GCC_SPINLOCK_SYNC_CREATE( pal_lock_gcc_spinlock_sync_state )  \
  {                                                                                           \
    (pal_lock_gcc_spinlock_sync_state) = LIBBENCHMARK_PAL_LOCK_GCC_SPINLOCK_SYNC_AVAILABLE;   \
    LFDS710_MISC_BARRIER_STORE;                                                               \
    lfds710_misc_force_store();                                                               \
  }

  #define LIBBENCHMARK_PAL_LOCK_GCC_SPINLOCK_SYNC_DESTROY( pal_lock_gcc_spinlock_sync_state )  \
  {                                                                                            \
    (pal_lock_gcc_spinlock_sync_state) = LIBBENCHMARK_PAL_LOCK_GCC_SPINLOCK_SYNC_UNAVAILABLE;  \
    LFDS710_MISC_BARRIER_STORE;                                                                \
    lfds710_misc_force_store();                                                                \
  }

  #define LIBBENCHMARK_PAL_LOCK_GCC_SPINLOCK_SYNC_GET( pal_lock_gcc_spinlock_sync_state )      while( LIBBENCHMARK_PAL_LOCK_GCC_SPINLOCK_SYNC_AVAILABLE != __sync_val_compare_and_swap(&(pal_lock_gcc_spinlock_sync_state), (lfds710_pal_uint_t) LIBBENCHMARK_PAL_LOCK_GCC_SPINLOCK_SYNC_AVAILABLE, (lfds710_pal_uint_t) LIBBENCHMARK_PAL_LOCK_GCC_SPINLOCK_SYNC_UNAVAILABLE) )

  #define LIBBENCHMARK_PAL_LOCK_GCC_SPINLOCK_SYNC_RELEASE( pal_lock_gcc_spinlock_sync_state )  __sync_lock_test_and_set( &(pal_lock_gcc_spinlock_sync_state), LIBBENCHMARK_PAL_LOCK_GCC_SPINLOCK_SYNC_AVAILABLE )

  /***** typedefs *****/
  typedef lfds710_pal_uint_t pal_lock_gcc_spinlock_sync_state;

#endif





/****************************************************************************/
#if( !defined LIBBENCHMARK_PAL_LOCK_GCC_SPINLOCK_SYNC )

  /***** defines *****/
  #define LIBBENCHMARK_PAL_LOCK_GCC_SPINLOCK_SYNC 0

  #define LIBBENCHMARK_PAL_LOCK_GCC_SPINLOCK_SYNC_CREATE( pal_lock_gcc_spinlock_atomic_sync )
  #define LIBBENCHMARK_PAL_LOCK_GCC_SPINLOCK_SYNC_DESTROY( pal_lock_gcc_spinlock_atomic_sync )
  #define LIBBENCHMARK_PAL_LOCK_GCC_SPINLOCK_SYNC_GET( pal_lock_gcc_spinlock_atomic_sync )
  #define LIBBENCHMARK_PAL_LOCK_GCC_SPINLOCK_SYNC_RELEASE( pal_lock_gcc_spinlock_atomic_sync )

  /***** typedefs *****/
  typedef void * pal_lock_gcc_spinlock_sync_state;

#endif

