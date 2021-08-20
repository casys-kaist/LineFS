/***** includes *****/
#include "libtest_tests_internal.h"

/***** structs *****/
struct test_per_thread_state
{
  lfds710_pal_uint_t
    thread_number;

  struct lfds710_queue_bmm_state
    *qbmms;
};

/***** private prototypes *****/
static libshared_pal_thread_return_t LIBSHARED_PAL_THREAD_CALLING_CONVENTION thread_simple_enqueuer( void *libtest_threadset_per_thread_state );





/****************************************************************************/
void libtest_tests_queue_bmm_enqueuing( struct lfds710_list_asu_state *list_of_logical_processors, struct libshared_memory_state *ms, enum lfds710_misc_validity *dvs )
{
  lfds710_pal_uint_t
    counter_number,
    loop = 0,
    *per_thread_counters,
    power_of_two_number_elements = 1,
    number_elements,
    number_logical_processors,
    thread_number;

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

  /* TRD : create an empty queue, with the largest possible number of elements
           then run one thread per CPU
           where each thread busy-works, enqueuing key/values, where the key is the thread ID and the value is an incrementing per-thread counter
           run until the queue is full

           when we're done, we check that all the elements are present
           and increment on a per-thread basis
  */

  *dvs = LFDS710_MISC_VALIDITY_VALID;

  lfds710_list_asu_query( list_of_logical_processors, LFDS710_LIST_ASU_QUERY_GET_POTENTIALLY_INACCURATE_COUNT, NULL, (void **) &number_logical_processors );

  // TRD : allocate
  lfds710_list_asu_query( list_of_logical_processors, LFDS710_LIST_ASU_QUERY_GET_POTENTIALLY_INACCURATE_COUNT, NULL, (void **) &number_logical_processors );
  tpts = libshared_memory_alloc_from_unknown_node( ms, sizeof(struct test_per_thread_state) * number_logical_processors, LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
  pts = libshared_memory_alloc_from_unknown_node( ms, sizeof(struct libtest_threadset_per_thread_state) * number_logical_processors, LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
  per_thread_counters = libshared_memory_alloc_from_unknown_node( ms, sizeof(lfds710_pal_uint_t) * number_logical_processors, LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
  qbmme_array = libshared_memory_alloc_largest_possible_array_from_unknown_node( ms, sizeof(struct lfds710_queue_bmm_element), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES, &number_elements );

  // TRD : need to only use a power of 2 number of elements
  number_elements >>= 1;

  while( number_elements != 0 )
  {
    number_elements >>= 1;
    power_of_two_number_elements <<= 1;
  }

  lfds710_queue_bmm_init_valid_on_current_logical_core( &qbmms, qbmme_array, power_of_two_number_elements, NULL );

  libtest_threadset_init( &ts, NULL );

  while( LFDS710_LIST_ASU_GET_START_AND_THEN_NEXT(*list_of_logical_processors,lasue) )
  {
    lp = LFDS710_LIST_ASU_GET_VALUE_FROM_ELEMENT( *lasue );
    (tpts+loop)->qbmms = &qbmms;
    (tpts+loop)->thread_number = loop;
    libtest_threadset_add_thread( &ts, &pts[loop], lp, thread_simple_enqueuer, &tpts[loop] );
    loop++;
  }

  LFDS710_MISC_BARRIER_STORE;

  lfds710_misc_force_store();

  // TRD : run the test
  libtest_threadset_run( &ts );

  libtest_threadset_cleanup( &ts );

  // TRD : validate
  LFDS710_MISC_BARRIER_LOAD;

  /* TRD : first, validate the queue
           then dequeue
           we expect to find element numbers increment on a per thread basis
  */

  vi.min_elements = vi.max_elements = power_of_two_number_elements;

  lfds710_queue_bmm_query( &qbmms, LFDS710_QUEUE_BMM_QUERY_SINGLETHREADED_VALIDATE, &vi, dvs );

  for( loop = 0 ; loop < number_logical_processors ; loop++ )
    *(per_thread_counters+loop) = 0;

  while( *dvs == LFDS710_MISC_VALIDITY_VALID and lfds710_queue_bmm_dequeue(&qbmms, (void **) &thread_number, (void **) &counter_number) )
  {
    if( thread_number >= number_logical_processors )
    {
      *dvs = LFDS710_MISC_VALIDITY_INVALID_TEST_DATA;
      break;
    }

    if( counter_number > per_thread_counters[thread_number] )
      *dvs = LFDS710_MISC_VALIDITY_INVALID_MISSING_ELEMENTS;

    if( counter_number < per_thread_counters[thread_number] )
      *dvs = LFDS710_MISC_VALIDITY_INVALID_ADDITIONAL_ELEMENTS;

    if( counter_number == per_thread_counters[thread_number] )
      per_thread_counters[thread_number]++;
  }

  lfds710_queue_bmm_cleanup( &qbmms, NULL );

  return;
}





/****************************************************************************/
static libshared_pal_thread_return_t LIBSHARED_PAL_THREAD_CALLING_CONVENTION thread_simple_enqueuer( void *libtest_threadset_per_thread_state )
{
  lfds710_pal_uint_t
    counter = 0;

  struct test_per_thread_state
    *tpts;

  struct libtest_threadset_per_thread_state
    *pts;

  LFDS710_MISC_MAKE_VALID_ON_CURRENT_LOGICAL_CORE_INITS_COMPLETED_BEFORE_NOW_ON_ANY_OTHER_LOGICAL_CORE;

  LFDS710_PAL_ASSERT( libtest_threadset_per_thread_state != NULL );

  pts = (struct libtest_threadset_per_thread_state *) libtest_threadset_per_thread_state;
  tpts = LIBTEST_THREADSET_GET_USER_STATE_FROM_PER_THREAD_STATE( *pts );

  libtest_threadset_thread_ready_and_wait( pts );

  while( lfds710_queue_bmm_enqueue(tpts->qbmms, (void *) (tpts->thread_number), (void *) (counter++)) );

  LFDS710_MISC_BARRIER_STORE;

  lfds710_misc_force_store();

  return LIBSHARED_PAL_THREAD_RETURN_CAST(RETURN_SUCCESS);
}

