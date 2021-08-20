/***** includes *****/
#include "libtest_tests_internal.h"

/***** structs *****/
struct test_per_thread_state
{
  struct lfds710_queue_umm_state
    *qs;
};

/***** private prototypes *****/
static libshared_pal_thread_return_t LIBSHARED_PAL_THREAD_CALLING_CONVENTION thread_enqueuer_with_malloc_and_dequeuer_with_free( void *libtest_threadset_per_thread_state );





/****************************************************************************/
void libtest_tests_queue_umm_enqueuing_with_malloc_and_dequeuing_with_free( struct lfds710_list_asu_state *list_of_logical_processors, struct libshared_memory_state *ms, enum lfds710_misc_validity *dvs )
{
  lfds710_pal_uint_t
    loop = 0,
    number_logical_processors;

  struct lfds710_list_asu_element
    *lasue = NULL;

  struct lfds710_queue_umm_element
    dummy_element;

  struct lfds710_queue_umm_state
    qs;

  struct lfds710_misc_validation_info
    vi;

  struct libtest_logical_processor
    *lp;

  struct libtest_threadset_per_thread_state
    *pts;

  struct libtest_threadset_state
    ts;

  struct test_per_thread_state
    *tpts;

  LFDS710_PAL_ASSERT( list_of_logical_processors != NULL );
  LFDS710_PAL_ASSERT( ms != NULL );
  LFDS710_PAL_ASSERT( dvs != NULL );

  /* TRD : one thread per logical core
           each thread loops for ten seconds
           mallocs and enqueues 1k elements, then dequeues and frees 1k elements
  */

  *dvs = LFDS710_MISC_VALIDITY_VALID;

  // TRD : allocate
  lfds710_list_asu_query( list_of_logical_processors, LFDS710_LIST_ASU_QUERY_GET_POTENTIALLY_INACCURATE_COUNT, NULL, (void **) &number_logical_processors );
  tpts = libshared_memory_alloc_from_unknown_node( ms, sizeof(struct test_per_thread_state) * number_logical_processors, LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
  pts = libshared_memory_alloc_from_unknown_node( ms, sizeof(struct libtest_threadset_per_thread_state) * number_logical_processors, LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );

  lfds710_queue_umm_init_valid_on_current_logical_core( &qs, &dummy_element, NULL );

  libtest_threadset_init( &ts, NULL );

  while( LFDS710_LIST_ASU_GET_START_AND_THEN_NEXT(*list_of_logical_processors,lasue) )
  {
    lp = LFDS710_LIST_ASU_GET_VALUE_FROM_ELEMENT( *lasue );
    (tpts+loop)->qs = &qs;

    libtest_threadset_add_thread( &ts, &pts[loop], lp, thread_enqueuer_with_malloc_and_dequeuer_with_free, &tpts[loop] );
    loop++;
  }

  LFDS710_MISC_BARRIER_STORE;

  lfds710_misc_force_store();

  // TRD : run the test
  libtest_threadset_run( &ts );

  libtest_threadset_cleanup( &ts );

  // TRD : validate
  LFDS710_MISC_BARRIER_LOAD;

  vi.min_elements = vi.max_elements = 0;

  lfds710_queue_umm_query( &qs, LFDS710_QUEUE_UMM_QUERY_SINGLETHREADED_VALIDATE, &vi, dvs );

  lfds710_queue_umm_cleanup( &qs, NULL );

  return;
}





/****************************************************************************/
static libshared_pal_thread_return_t LIBSHARED_PAL_THREAD_CALLING_CONVENTION thread_enqueuer_with_malloc_and_dequeuer_with_free( void *libtest_threadset_per_thread_state )
{
  lfds710_pal_uint_t
    loop,
    time_loop = 0;

  struct lfds710_queue_umm_element
    *qe;

  struct test_per_thread_state
    *tpts;

  struct libtest_threadset_per_thread_state
    *pts;

  time_t
    current_time,
    start_time;

  LFDS710_MISC_MAKE_VALID_ON_CURRENT_LOGICAL_CORE_INITS_COMPLETED_BEFORE_NOW_ON_ANY_OTHER_LOGICAL_CORE;

  LFDS710_PAL_ASSERT( libtest_threadset_per_thread_state != NULL );

  pts = (struct libtest_threadset_per_thread_state *) libtest_threadset_per_thread_state;
  tpts = LIBTEST_THREADSET_GET_USER_STATE_FROM_PER_THREAD_STATE( *pts );

  libtest_threadset_thread_ready_and_wait( pts );

  current_time = start_time = time( NULL );

  while( current_time < start_time + TEST_DURATION_IN_SECONDS )
  {
    for( loop = 0 ; loop < 1000 ; loop++ )
    {
      qe = libtest_misc_aligned_malloc( sizeof(struct lfds710_queue_umm_element), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
      lfds710_queue_umm_enqueue( tpts->qs, qe );
    }

    for( loop = 0 ; loop < 1000 ; loop++ )
    {
      lfds710_queue_umm_dequeue( tpts->qs, &qe );
      libtest_misc_aligned_free( qe );
    }

    if( time_loop++ == REDUCED_TIME_LOOP_COUNT )
    {
      time_loop = 0;
      time( &current_time );
    }
  }

  LFDS710_MISC_BARRIER_STORE;

  lfds710_misc_force_store();

  return LIBSHARED_PAL_THREAD_RETURN_CAST(RETURN_SUCCESS);
}

