/***** includes *****/
#include "libtest_tests_internal.h"

/***** structs *****/
struct test_per_thread_state
{
  enum flag
    error_flag;

  struct lfds710_queue_bmm_state
    *qbmms;
};

/***** private prototypes *****/
static libshared_pal_thread_return_t LIBSHARED_PAL_THREAD_CALLING_CONVENTION thread_simple_dequeuer( void *libtest_threadset_per_thread_state );





/****************************************************************************/
void libtest_tests_queue_bmm_dequeuing( struct lfds710_list_asu_state *list_of_logical_processors, struct libshared_memory_state *ms, enum lfds710_misc_validity *dvs )
{
  lfds710_pal_uint_t
    counter = 0,
    loop = 0,
    power_of_two_number_elements = 1,
    number_elements,
    number_logical_processors;

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
           do a single-threaded (in the prep function) full enqueue
           with the value being an incrementing counter
           then run one thread per CPU
           where each thread busy-works, dequeuing, and checks the dequeued value is greater than the previously dequeued value
           run until the queue is empty
  */

  *dvs = LFDS710_MISC_VALIDITY_VALID;

  lfds710_list_asu_query( list_of_logical_processors, LFDS710_LIST_ASU_QUERY_GET_POTENTIALLY_INACCURATE_COUNT, NULL, (void **) &number_logical_processors );

  // TRD : allocate
  lfds710_list_asu_query( list_of_logical_processors, LFDS710_LIST_ASU_QUERY_GET_POTENTIALLY_INACCURATE_COUNT, NULL, (void **) &number_logical_processors );
  tpts = libshared_memory_alloc_from_unknown_node( ms, sizeof(struct test_per_thread_state) * number_logical_processors, LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
  pts = libshared_memory_alloc_from_unknown_node( ms, sizeof(struct libtest_threadset_per_thread_state) * number_logical_processors, LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
  qbmme_array = libshared_memory_alloc_largest_possible_array_from_unknown_node( ms, sizeof(struct lfds710_queue_bmm_element), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES, &number_elements );

  // TRD : need to only use a power of 2 number of elements
  number_elements >>= 1;

  while( number_elements != 0 )
  {
    number_elements >>= 1;
    power_of_two_number_elements <<= 1;
  }

  lfds710_queue_bmm_init_valid_on_current_logical_core( &qbmms, qbmme_array, power_of_two_number_elements, NULL );

  // TRD : fill the queue
  while( lfds710_queue_bmm_enqueue(&qbmms, NULL, (void *) (counter++)) );

  libtest_threadset_init( &ts, NULL );

  while( LFDS710_LIST_ASU_GET_START_AND_THEN_NEXT(*list_of_logical_processors,lasue) )
  {
    lp = LFDS710_LIST_ASU_GET_VALUE_FROM_ELEMENT( *lasue );
    (tpts+loop)->qbmms = &qbmms;
    (tpts+loop)->error_flag = LOWERED;
    libtest_threadset_add_thread( &ts, &pts[loop], lp, thread_simple_dequeuer, &tpts[loop] );
    loop++;
  }

  LFDS710_MISC_BARRIER_STORE;

  lfds710_misc_force_store();

  // TRD : run the test
  libtest_threadset_run( &ts );

  libtest_threadset_cleanup( &ts );

  // TRD : validate
  LFDS710_MISC_BARRIER_LOAD;

  /* TRD : we just check the per-thread error flags and validate the queue
           most of the checking happened in the threads
  */

  for( loop = 0 ; loop < number_logical_processors ; loop++ )
    if( (tpts+loop)->error_flag == RAISED )
      *dvs = LFDS710_MISC_VALIDITY_INVALID_ORDER;

  if( *dvs == LFDS710_MISC_VALIDITY_VALID )
  {
    vi.min_elements = vi.max_elements = 0;
    lfds710_queue_bmm_query( &qbmms, LFDS710_QUEUE_BMM_QUERY_SINGLETHREADED_VALIDATE, &vi, dvs );
  }

  lfds710_queue_bmm_cleanup( &qbmms, NULL );

  return;
}





/****************************************************************************/
static libshared_pal_thread_return_t LIBSHARED_PAL_THREAD_CALLING_CONVENTION thread_simple_dequeuer( void *libtest_threadset_per_thread_state )
{
  lfds710_pal_uint_t
    counter = 0,
    key,
    value;

  struct test_per_thread_state
    *tpts;

  struct libtest_threadset_per_thread_state
    *pts;

  LFDS710_MISC_MAKE_VALID_ON_CURRENT_LOGICAL_CORE_INITS_COMPLETED_BEFORE_NOW_ON_ANY_OTHER_LOGICAL_CORE;

  LFDS710_PAL_ASSERT( libtest_threadset_per_thread_state != NULL );

  pts = (struct libtest_threadset_per_thread_state *) libtest_threadset_per_thread_state;
  tpts = LIBTEST_THREADSET_GET_USER_STATE_FROM_PER_THREAD_STATE( *pts );

  libtest_threadset_thread_ready_and_wait( pts );

  while( lfds710_queue_bmm_dequeue(tpts->qbmms, (void *) &key, (void *) &value) )
  {
    if( value < counter )
      tpts->error_flag = RAISED;

    if( value > counter )
      counter = value;
  }

  LFDS710_MISC_BARRIER_STORE;

  lfds710_misc_force_store();

  return LIBSHARED_PAL_THREAD_RETURN_CAST(RETURN_SUCCESS);
}

