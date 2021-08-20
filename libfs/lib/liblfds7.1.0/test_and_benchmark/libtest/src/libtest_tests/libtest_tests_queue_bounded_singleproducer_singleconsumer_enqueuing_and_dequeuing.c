/***** includes *****/
#include "libtest_tests_internal.h"

/***** structs *****/
struct test_per_thread_state
{
  enum flag
    error_flag;

  struct lfds710_queue_bss_state
    *qs;
};

/***** private prototypes *****/
static libshared_pal_thread_return_t LIBSHARED_PAL_THREAD_CALLING_CONVENTION thread_enqueuer( void *libtest_threadset_per_thread_state );
static libshared_pal_thread_return_t LIBSHARED_PAL_THREAD_CALLING_CONVENTION thread_dequeuer( void *libtest_threadset_per_thread_state );





/****************************************************************************/
void libtest_tests_queue_bss_enqueuing_and_dequeuing( struct lfds710_list_asu_state *list_of_logical_processors, struct libshared_memory_state *ms, enum lfds710_misc_validity *dvs )
{
  lfds710_pal_uint_t
    loop,
    number_logical_processors;

  struct lfds710_list_asu_element
    *lasue;

  struct lfds710_queue_bss_element
    element_array[4];

  struct lfds710_queue_bss_state
    qs;

  struct libtest_logical_processor
    *lp,
    *lp_first;

  struct libtest_threadset_per_thread_state
    *pts;

  struct libtest_threadset_state
    ts;

  struct test_per_thread_state
    *tpts;

  LFDS710_PAL_ASSERT( list_of_logical_processors != NULL );
  LFDS710_PAL_ASSERT( ms != NULL );
  LFDS710_PAL_ASSERT( dvs != NULL );

  /* TRD : so, this is the real test
           problem is, because we use memory barriers only
           and we only support one producer and one consumer
           we need to ensure these threads are on different physical cores
           if they're on the same core, the code would work even without memory barriers

           problem is, in the test application, we only know the *number* of logical cores
           obtaining topology information adds a great deal of complexity to the test app
           and makes porting much harder

           so, we know how many logical cores there are; my thought is to partially
           permutate over them - we always run the producer on core 0, but we iterate
           over the other logical cores, running the test once each time, with the
           consumer being run on core 0, then core 1, then core 2, etc

           (we run on core 0 for the single-cpu case; it's redundent, since a single
            logical core running both producer and consumer will work, but otherwise
            we have to skip the test, which is confusing for the user)

           the test is one thread enqueuing and one thread dequeuing for two seconds
  */

  *dvs = LFDS710_MISC_VALIDITY_VALID;

  // TRD : allocate
  lfds710_list_asu_query( list_of_logical_processors, LFDS710_LIST_ASU_QUERY_GET_POTENTIALLY_INACCURATE_COUNT, NULL, (void **) &number_logical_processors );

  tpts = libshared_memory_alloc_from_unknown_node( ms, sizeof(struct test_per_thread_state) * 2, LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
  pts = libshared_memory_alloc_from_unknown_node( ms, sizeof(struct libtest_threadset_per_thread_state) * 2, LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );

  for( loop = 0 ; loop < 2 ; loop++ )
  {
    (tpts+loop)->qs = &qs;
    (tpts+loop)->error_flag = LOWERED;
  }

  /* TRD : producer always on core 0
           iterate over the other cores with consumer
  */
  
  lasue = LFDS710_LIST_ASU_GET_START( *list_of_logical_processors );
  lp_first = LFDS710_LIST_ASU_GET_VALUE_FROM_ELEMENT( *lasue );

  while( lasue != NULL )
  {
    lp = LFDS710_LIST_ASU_GET_VALUE_FROM_ELEMENT( *lasue );

    lfds710_queue_bss_init_valid_on_current_logical_core( &qs, element_array, 4, NULL );

    libtest_threadset_init( &ts, NULL );

    LFDS710_MISC_BARRIER_STORE;
    lfds710_misc_force_store();

    libtest_threadset_add_thread( &ts, &pts[0], lp_first, thread_enqueuer, &tpts[0] );
    libtest_threadset_add_thread( &ts, &pts[1], lp, thread_dequeuer, &tpts[1] );

    libtest_threadset_run( &ts );
    libtest_threadset_cleanup( &ts );

    LFDS710_MISC_BARRIER_LOAD;

    lfds710_queue_bss_cleanup( &qs, NULL );

    lasue = LFDS710_LIST_ASU_GET_NEXT( *lasue );
  }

  if( (tpts+1)->error_flag == RAISED )
    *dvs = LFDS710_MISC_VALIDITY_INVALID_TEST_DATA;

  return;
}





/****************************************************************************/
static libshared_pal_thread_return_t LIBSHARED_PAL_THREAD_CALLING_CONVENTION thread_enqueuer( void *libtest_threadset_per_thread_state )
{
  int
    rv;

  lfds710_pal_uint_t
    datum = 0,
    time_loop = 0;

  struct libtest_threadset_per_thread_state
    *pts;

  struct test_per_thread_state
    *tpts;

  time_t
    current_time,
    start_time;

  LFDS710_MISC_MAKE_VALID_ON_CURRENT_LOGICAL_CORE_INITS_COMPLETED_BEFORE_NOW_ON_ANY_OTHER_LOGICAL_CORE;

  LFDS710_PAL_ASSERT( libtest_threadset_per_thread_state != NULL );

  pts = (struct libtest_threadset_per_thread_state *) libtest_threadset_per_thread_state;
  tpts = LIBTEST_THREADSET_GET_USER_STATE_FROM_PER_THREAD_STATE( *pts );

  libtest_threadset_thread_ready_and_wait( pts );

  current_time = start_time = time( NULL );

  while( current_time < start_time + 2 )
  {
    rv = lfds710_queue_bss_enqueue( tpts->qs, NULL, (void *) datum );

    if( rv == 1 )
      if( ++datum == 4 )
        datum = 0;

    if( time_loop++ == TIME_LOOP_COUNT )
    {
      time_loop = 0;
      time( &current_time );
    }
  }

  LFDS710_MISC_BARRIER_STORE;

  lfds710_misc_force_store();

  return LIBSHARED_PAL_THREAD_RETURN_CAST(RETURN_SUCCESS);
}





/****************************************************************************/
static libshared_pal_thread_return_t LIBSHARED_PAL_THREAD_CALLING_CONVENTION thread_dequeuer( void *libtest_threadset_per_thread_state )
{
  int
    rv;

  lfds710_pal_uint_t
    datum,
    expected_datum = 0,
    time_loop = 0;

  struct libtest_threadset_per_thread_state
    *pts;

  struct test_per_thread_state
    *tpts;

  time_t
    current_time,
    start_time;

  LFDS710_MISC_BARRIER_LOAD;

  LFDS710_PAL_ASSERT( libtest_threadset_per_thread_state != NULL );

  pts = (struct libtest_threadset_per_thread_state *) libtest_threadset_per_thread_state;
  tpts = LIBTEST_THREADSET_GET_USER_STATE_FROM_PER_THREAD_STATE( *pts );

  libtest_threadset_thread_ready_and_wait( pts );

  current_time = start_time = time( NULL );

  while( current_time < start_time + 2 )
  {
    rv = lfds710_queue_bss_dequeue( tpts->qs, NULL, (void *) &datum );

    if( rv == 1 )
    {
      if( datum != expected_datum )
        tpts->error_flag = RAISED;

      if( ++expected_datum == 4 )
        expected_datum = 0;
    }

    if( time_loop++ == TIME_LOOP_COUNT )
    {
      time_loop = 0;
      time( &current_time );
    }
  }

  LFDS710_MISC_BARRIER_STORE;

  lfds710_misc_force_store();

  return LIBSHARED_PAL_THREAD_RETURN_CAST(RETURN_SUCCESS);
}

