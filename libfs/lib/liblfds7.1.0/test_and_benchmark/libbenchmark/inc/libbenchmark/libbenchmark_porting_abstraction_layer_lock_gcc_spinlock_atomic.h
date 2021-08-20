/****************************************************************************/
#if( defined __GNUC__ && LIBLFDS_GCC_VERSION >= 473 )

  /***** defines *****/
  #define LIBBENCHMARK_PAL_LOCK_GCC_SPINLOCK_ATOMIC 1

  #define LIBBENCHMARK_PAL_LOCK_GCC_SPINLOCK_ATOMIC_UNINITIALIZED  0
  #define LIBBENCHMARK_PAL_LOCK_GCC_SPINLOCK_ATOMIC_AVAILABLE      1
  #define LIBBENCHMARK_PAL_LOCK_GCC_SPINLOCK_ATOMIC_UNAVAILABLE    2

  #define LIBBENCHMARK_PAL_LOCK_GCC_SPINLOCK_ATOMIC_CREATE( pal_lock_gcc_spinlock_atomic_state )  \
  {                                                                                               \
    (pal_lock_gcc_spinlock_atomic_state) = LIBBENCHMARK_PAL_LOCK_GCC_SPINLOCK_ATOMIC_AVAILABLE;   \
    LFDS710_MISC_BARRIER_STORE;                                                                   \
    lfds710_misc_force_store();                                                                   \
  }

  #define LIBBENCHMARK_PAL_LOCK_GCC_SPINLOCK_ATOMIC_DESTROY( pal_lock_gcc_spinlock_atomic_state )  \
  {                                                                                                \
    (pal_lock_gcc_spinlock_atomic_state) = LIBBENCHMARK_PAL_LOCK_GCC_SPINLOCK_ATOMIC_UNAVAILABLE;  \
    LFDS710_MISC_BARRIER_STORE;                                                                    \
    lfds710_misc_force_store();                                                                    \
  }

  #define LIBBENCHMARK_PAL_LOCK_GCC_SPINLOCK_ATOMIC_GET( pal_lock_gcc_spinlock_atomic_state )                                                                                                                                                                                                             \
  {                                                                                                                                                                                                                                                                                                       \
    lfds710_pal_uint_t expected = (lfds710_pal_uint_t) LIBBENCHMARK_PAL_LOCK_GCC_SPINLOCK_ATOMIC_AVAILABLE;                                                                                                                                                                                               \
    while( 0 == __atomic_compare_exchange_n(&(pal_lock_gcc_spinlock_atomic_state), &expected, (lfds710_pal_uint_t) LIBBENCHMARK_PAL_LOCK_GCC_SPINLOCK_ATOMIC_UNAVAILABLE, 1, __ATOMIC_RELAXED, __ATOMIC_RELAXED) )  expected = (lfds710_pal_uint_t) LIBBENCHMARK_PAL_LOCK_GCC_SPINLOCK_ATOMIC_AVAILABLE;  \
  }

  #define LIBBENCHMARK_PAL_LOCK_GCC_SPINLOCK_ATOMIC_RELEASE( pal_lock_gcc_spinlock_atomic_state )  __atomic_exchange_n( &(pal_lock_gcc_spinlock_atomic_state), (void *) LIBBENCHMARK_PAL_LOCK_GCC_SPINLOCK_ATOMIC_AVAILABLE, __ATOMIC_RELAXED )

  /***** typedefs *****/
  typedef lfds710_pal_uint_t pal_lock_gcc_spinlock_atomic_state;

#endif





/****************************************************************************/
#if( !defined LIBBENCHMARK_PAL_LOCK_GCC_SPINLOCK_ATOMIC )

  /***** defines *****/
  #define LIBBENCHMARK_PAL_LOCK_GCC_SPINLOCK_ATOMIC 0

  #define LIBBENCHMARK_PAL_LOCK_GCC_SPINLOCK_ATOMIC_CREATE( pal_lock_gcc_spinlock_atomic_state )
  #define LIBBENCHMARK_PAL_LOCK_GCC_SPINLOCK_ATOMIC_DESTROY( pal_lock_gcc_spinlock_atomic_state )
  #define LIBBENCHMARK_PAL_LOCK_GCC_SPINLOCK_ATOMIC_GET( pal_lock_gcc_spinlock_atomic_state )
  #define LIBBENCHMARK_PAL_LOCK_GCC_SPINLOCK_ATOMIC_RELEASE( pal_lock_gcc_spinlock_atomic_state )

  /***** typedefs *****/
  typedef void * pal_lock_gcc_spinlock_atomic_state;

#endif

