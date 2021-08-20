/***** includes *****/
#include "libtest_tests_internal.h"

/***** structs *****/
struct test_per_thread_state
{
  lfds710_pal_uint_t
    thread_number,
    total_dequeues,
    total_enqueues;

  struct lfds710_queue_bmm_state
    *qbmms;
};

/***** private prototypes *****/
static libshared_pal_thread_return_t LIBSHARED_PAL_THREAD_CALLING_CONVENTION thread_rapid_enqueuer_and_dequeuer( void *libtest_threadset_per_thread_state );





/****************************************************************************/
void libtest_tests_queue_bmm_rapid_enqueuing_and_dequeuing( struct lfds710_list_asu_state *list_of_logical_processors, struct libshared_memory_state *ms, enum lfds710_misc_validity *dvs )
{
  lfds710_pal_uint_t
    counter,
    loop,
    number_logical_processors,
    *per_thread_counters,
    thread_number;

  struct lfds710_list_asu_element
    *lasue = NULL;

  struct lfds710_queue_bmm_element
    *qbmme_array;

  struct lfds710_misc_validation_info
    vi;

  struct lfds710_queue_bmm_state
    qbmms;

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

  /* TRD : we create a single queue with 50,000 elements
           we don't want too many elements, so we ensure plenty of element re-use
           each thread simply loops dequeuing and enqueuing
           where the user data indicates thread number and an increment counter
           vertification is that the counter increments on a per-thread basis
  */

  *dvs = LFDS710_MISC_VALIDITY_VALID;

  lfds710_list_asu_query( list_of_logical_processors, LFDS710_LIST_ASU_QUERY_GET_POTENTIALLY_INACCURATE_COUNT, NULL, (void **) &number_logical_processors );

  lfds710_list_asu_query( list_of_logical_processors, LFDS710_LIST_ASU_QUERY_GET_POTENTIALLY_INACCURATE_COUNT, NULL, (void **) &number_logical_processors );
  tpts = libshared_memory_alloc_from_unknown_node( ms, sizeof(struct test_per_thread_state) * number_logical_processors, LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
  pts = libshared_memory_alloc_from_unknown_node( ms, sizeof(struct libtest_threadset_per_thread_state) * number_logical_processors, LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
  per_thread_counters = libshared_memory_alloc_from_unknown_node( ms, sizeof(lfds710_pal_uint_t) * number_logical_processors, LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
  // TRD : must be a power of 2
  qbmme_array = libshared_memory_alloc_from_unknown_node( ms, sizeof(struct lfds710_queue_bmm_element) * 8192, LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );

  lfds710_queue_bmm_init_valid_on_current_logical_core( &qbmms, qbmme_array, 8192, NULL );

  // TRD : we assume the test will iterate at least once (or we'll have a false negative)
  for( loop = 0 ; loop < 8192 ; loop++ )
    lfds710_queue_bmm_enqueue( &qbmms, (void *) 0, (void *) 0 );

  libtest_threadset_init( &ts, NULL );

  loop = 0;

  while( LFDS710_LIST_ASU_GET_START_AND_THEN_NEXT(*list_of_logical_processors,lasue) )
  {
    lp = LFDS710_LIST_ASU_GET_VALUE_FROM_ELEMENT( *lasue );
    (tpts+loop)->qbmms = &qbmms;
    (tpts+loop)->thread_number = loop;
    (tpts+loop)->total_dequeues = 0;
    (tpts+loop)->total_enqueues = 0;
    libtest_threadset_add_thread( &ts, &pts[loop], lp, thread_rapid_enqueuer_and_dequeuer, &tpts[loop] );
    loop++;
  }

  // TRD : run the test
  libtest_threadset_run( &ts );

  libtest_threadset_cleanup( &ts );

  // TRD : validate
  LFDS710_MISC_BARRIER_LOAD;

  vi.min_elements = 8192;

  for( loop = 0 ; loop < number_logical_processors ; loop++ )
  {
    vi.min_elements -= tpts[loop].total_dequeues;
    vi.min_elements += tpts[loop].total_enqueues;
  }

  vi.max_elements = vi.min_elements;

  lfds710_queue_bmm_query( &qbmms, LFDS710_QUEUE_BMM_QUERY_SINGLETHREADED_VALIDATE, &vi, dvs );

  // TRD : now check results
  for( loop = 0 ; loop < number_logical_processors ; loop++ )
    *(per_thread_counters+loop) = 0;

  while( *dvs == LFDS710_MISC_VALIDITY_VALID and lfds710_queue_bmm_dequeue(&qbmms, (void **) &thread_number, (void **) &counter) )
  {
    if( thread_number >= number_logical_processors )
    {
      *dvs = LFDS710_MISC_VALIDITY_INVALID_TEST_DATA;
      break;
    }

    if( per_thread_counters[thread_number] == 0 )
      per_thread_counters[thread_number] = counter;

    if( counter > per_thread_counters[thread_number] )
      *dvs = LFDS710_MISC_VALIDITY_INVALID_MISSING_ELEMENTS;

    if( counter < per_thread_counters[thread_number] )
      *dvs = LFDS710_MISC_VALIDITY_INVALID_ADDITIONAL_ELEMENTS;

    if( counter == per_thread_counters[thread_number] )
      per_thread_counters[thread_number]++;
  }

  lfds710_queue_bmm_cleanup( &qbmms, NULL );

  return;
}





/****************************************************************************/
static libshared_pal_thread_return_t LIBSHARED_PAL_THREAD_CALLING_CONVENTION thread_rapid_enqueuer_and_dequeuer( void *libtest_threadset_per_thread_state )
{
  int
    rv;

  lfds710_pal_uint_t
    key,
    value,
    time_loop = 0;

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
    // TRD : with BMM, both dequeue and so enqueue may spuriously fail, so we only increment counter if we did enqueue

    tpts->total_dequeues += lfds710_queue_bmm_dequeue( tpts->qbmms, (void **) &key, (void **) &value );

    // TRD : disgard the dequeue content - this is the rapid test

    // TRD : counter (total_enqueues works in the same way, so using that) needs to increment, for validation checks
    rv = lfds710_queue_bmm_enqueue( tpts->qbmms, (void *) tpts->thread_number, (void *) (tpts->total_enqueues) );

    if( rv == 1 )
      tpts->total_enqueues++;

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

