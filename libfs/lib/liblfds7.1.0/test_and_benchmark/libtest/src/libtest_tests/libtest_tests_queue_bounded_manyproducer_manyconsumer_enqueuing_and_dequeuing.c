/***** includes *****/
#include "libtest_tests_internal.h"

/***** structs *****/
struct test_per_thread_state
{
  enum flag
    error_flag;

  lfds710_pal_uint_t
    counter,
    number_logical_processors,
    *per_thread_counters,
    thread_number;

  struct lfds710_queue_bmm_state
    *qbmms;
};

/***** private prototypes *****/
static libshared_pal_thread_return_t LIBSHARED_PAL_THREAD_CALLING_CONVENTION thread_enqueuer_and_dequeuer( void *libtest_threadset_per_thread_state );





/****************************************************************************/
void libtest_tests_queue_bmm_enqueuing_and_dequeuing( struct lfds710_list_asu_state *list_of_logical_processors, struct libshared_memory_state *ms, enum lfds710_misc_validity *dvs )
{
  lfds710_pal_uint_t
    loop,
    number_elements,
    number_logical_processors,
    *per_thread_counters,
    power_of_two_number_elements = 1,
    subloop;

  struct lfds710_list_asu_element
    *lasue = NULL;

  struct lfds710_queue_bmm_element
    *qbmme_array;

  struct lfds710_queue_bmm_state
    qbmms;

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

  /* TRD : create a queue with one element per thread
           each thread constly dequeues and enqueues from that one queue
           where when enqueuing sets in the element
           its thread number and counter
           and when dequeuing, checks the thread number and counter
           against previously seen counter for that thread
           where it should always see a higher number
  */

  *dvs = LFDS710_MISC_VALIDITY_VALID;

  // TRD : allocate
  lfds710_list_asu_query( list_of_logical_processors, LFDS710_LIST_ASU_QUERY_GET_POTENTIALLY_INACCURATE_COUNT, NULL, (void **) &number_logical_processors );
  tpts = libshared_memory_alloc_from_unknown_node( ms, sizeof(struct test_per_thread_state) * number_logical_processors, LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
  pts = libshared_memory_alloc_from_unknown_node( ms, sizeof(struct libtest_threadset_per_thread_state) * number_logical_processors, LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
  per_thread_counters = libshared_memory_alloc_from_unknown_node( ms, sizeof(lfds710_pal_uint_t) * number_logical_processors * number_logical_processors, LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
  qbmme_array = libshared_memory_alloc_largest_possible_array_from_unknown_node( ms, sizeof(struct lfds710_queue_bmm_element), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES, &number_elements );

  // TRD : need to only use a power of 2 number of elements
  number_elements >>= 1;

  while( number_elements != 0 )
  {
    number_elements >>= 1;
    power_of_two_number_elements <<= 1;
  }

  /* TRD : to make the test more demanding, smallest number of elements (greater than number of logical cores)
           we really want one element per core
           but with the power-of-2 requirements, we can't have it
  */

  while( power_of_two_number_elements > number_logical_processors )
    power_of_two_number_elements >>= 1;

  lfds710_queue_bmm_init_valid_on_current_logical_core( &qbmms, qbmme_array, power_of_two_number_elements, NULL );

  // TRD : we assume the test will iterate at least once (or we'll have a false negative)
  for( loop = 0 ; loop < number_logical_processors ; loop++ )
    lfds710_queue_bmm_enqueue( &qbmms, (void *) loop, (void *) 0 );

  libtest_threadset_init( &ts, NULL );

  loop = 0;

  while( LFDS710_LIST_ASU_GET_START_AND_THEN_NEXT(*list_of_logical_processors,lasue) )
  {
    lp = LFDS710_LIST_ASU_GET_VALUE_FROM_ELEMENT( *lasue );
    (tpts+loop)->qbmms = &qbmms;
    (tpts+loop)->thread_number = loop;
    (tpts+loop)->counter = 0;
    (tpts+loop)->error_flag = LOWERED;
    (tpts+loop)->per_thread_counters = per_thread_counters + loop * number_logical_processors;
    (tpts+loop)->number_logical_processors = number_logical_processors;

    for( subloop = 0 ; subloop < number_logical_processors ; subloop++ )
      *((tpts+loop)->per_thread_counters+subloop) = 0;

    libtest_threadset_add_thread( &ts, &pts[loop], lp, thread_enqueuer_and_dequeuer, &tpts[loop] );
    loop++;
  }

  LFDS710_MISC_BARRIER_STORE;

  lfds710_misc_force_store();

  // TRD : run the test
  libtest_threadset_run( &ts );

  libtest_threadset_cleanup( &ts );

  // TRD : validate
  LFDS710_MISC_BARRIER_LOAD;

  vi.min_elements = vi.max_elements = power_of_two_number_elements;

  lfds710_queue_bmm_query( &qbmms, LFDS710_QUEUE_BMM_QUERY_SINGLETHREADED_VALIDATE, &vi, dvs );

  for( loop = 0 ; loop < number_logical_processors ; loop++ )
    if( (tpts+loop)->error_flag == RAISED )
      *dvs = LFDS710_MISC_VALIDITY_INVALID_TEST_DATA;

  lfds710_queue_bmm_cleanup( &qbmms, NULL );

  return;
}





/****************************************************************************/
static libshared_pal_thread_return_t LIBSHARED_PAL_THREAD_CALLING_CONVENTION thread_enqueuer_and_dequeuer( void *libtest_threadset_per_thread_state )
{
  lfds710_pal_uint_t
    counter,
    thread_number,
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
    /* TRD : this is a soft queue, so dequeue/enqueue operations although always occurring
             may not be visible by the time the enqueue/dequeue function returns
             i.e. all threads may have dequeued, and then enqueued, but not seen each others enqueues yet
             so the queue looks empty
    */

    while( 0 == lfds710_queue_bmm_dequeue(tpts->qbmms, (void *) &thread_number, (void *) &counter) );

    if( thread_number >= tpts->number_logical_processors )
      tpts->error_flag = RAISED;
    else
    {
      if( counter < tpts->per_thread_counters[thread_number] )
        tpts->error_flag = RAISED;

      if( counter >= tpts->per_thread_counters[thread_number] )
        tpts->per_thread_counters[thread_number] = counter+1;
    }

    thread_number = tpts->thread_number;
    counter = ++tpts->counter;

    // TRD : the enqueue can only succeed once a dequeue of the *very next element* in the queue has become visible (i.e. our down earlier dequeue may not be the right dequeue)
    while( 0 == lfds710_queue_bmm_enqueue(tpts->qbmms, (void *) thread_number, (void *) counter) );

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

