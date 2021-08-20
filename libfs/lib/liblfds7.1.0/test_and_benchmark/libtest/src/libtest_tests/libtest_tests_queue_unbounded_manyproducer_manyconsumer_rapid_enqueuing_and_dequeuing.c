/***** includes *****/
#include "libtest_tests_internal.h"

/***** structs *****/
struct test_per_thread_state
{
  lfds710_pal_uint_t
    counter,
    thread_number;

  struct lfds710_queue_umm_state
    *qs;
};

struct test_element
{
  struct lfds710_queue_umm_element
    qe,
    *qe_use;

  lfds710_pal_uint_t
    counter,
    thread_number;
};

/***** private prototypes *****/
static libshared_pal_thread_return_t LIBSHARED_PAL_THREAD_CALLING_CONVENTION thread_rapid_enqueuer_and_dequeuer( void *libtest_threadset_per_thread_state );





/****************************************************************************/
void libtest_tests_queue_umm_rapid_enqueuing_and_dequeuing( struct lfds710_list_asu_state *list_of_logical_processors, struct libshared_memory_state *ms, enum lfds710_misc_validity *dvs )
{
  lfds710_pal_uint_t
    loop,
    number_logical_processors,
    *per_thread_counters;

  struct lfds710_list_asu_element
    *lasue = NULL;

  struct lfds710_queue_umm_element LFDS710_PAL_ALIGN(LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES)
    qe_dummy;

  struct lfds710_queue_umm_element
    *qe;

  struct lfds710_misc_validation_info
    vi;

  struct lfds710_queue_umm_state
    qs;

  struct libtest_logical_processor
    *lp;

  struct libtest_threadset_per_thread_state
    *pts;

  struct libtest_threadset_state
    ts;

  struct test_element
    *te_array,
    *te;

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
  te_array = libshared_memory_alloc_from_unknown_node( ms, sizeof(struct test_element) * 10000 * number_logical_processors, LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );

  vi.min_elements = vi.max_elements = 10000;

  lfds710_queue_umm_init_valid_on_current_logical_core( &qs, &qe_dummy, NULL );

  // TRD : we assume the test will iterate at least once (or we'll have a false negative)
  for( loop = 0 ; loop < 10000 ; loop++ )
  {
    (te_array+loop)->thread_number = loop;
    (te_array+loop)->counter = 0;
    LFDS710_QUEUE_UMM_SET_VALUE_IN_ELEMENT( (te_array+loop)->qe, te_array+loop );
    lfds710_queue_umm_enqueue( &qs, &(te_array+loop)->qe );
  }

  libtest_threadset_init( &ts, NULL );

  loop = 0;

  while( LFDS710_LIST_ASU_GET_START_AND_THEN_NEXT(*list_of_logical_processors,lasue) )
  {
    lp = LFDS710_LIST_ASU_GET_VALUE_FROM_ELEMENT( *lasue );
    (tpts+loop)->qs = &qs;
    (tpts+loop)->thread_number = loop;
    (tpts+loop)->counter = 0;
    libtest_threadset_add_thread( &ts, &pts[loop], lp, thread_rapid_enqueuer_and_dequeuer, &tpts[loop] );
    loop++;
  }

  // TRD : run the test
  libtest_threadset_run( &ts );

  libtest_threadset_cleanup( &ts );

  // TRD : validate
  LFDS710_MISC_BARRIER_LOAD;

  lfds710_queue_umm_query( &qs, LFDS710_QUEUE_UMM_QUERY_SINGLETHREADED_VALIDATE, &vi, dvs );

  // TRD : now check results
  for( loop = 0 ; loop < number_logical_processors ; loop++ )
    *(per_thread_counters+loop) = 0;

  while( *dvs == LFDS710_MISC_VALIDITY_VALID and lfds710_queue_umm_dequeue(&qs, &qe) )
  {
    te = LFDS710_QUEUE_UMM_GET_VALUE_FROM_ELEMENT( *qe );

    if( te->thread_number >= number_logical_processors )
    {
      *dvs = LFDS710_MISC_VALIDITY_INVALID_TEST_DATA;
      break;
    }

    if( per_thread_counters[te->thread_number] == 0 )
      per_thread_counters[te->thread_number] = te->counter;

    if( te->counter > per_thread_counters[te->thread_number] )
      *dvs = LFDS710_MISC_VALIDITY_INVALID_MISSING_ELEMENTS;

    if( te->counter < per_thread_counters[te->thread_number] )
      *dvs = LFDS710_MISC_VALIDITY_INVALID_ADDITIONAL_ELEMENTS;

    if( te->counter == per_thread_counters[te->thread_number] )
      per_thread_counters[te->thread_number]++;
  }

  lfds710_queue_umm_cleanup( &qs, NULL );

  return;
}





/****************************************************************************/
static libshared_pal_thread_return_t LIBSHARED_PAL_THREAD_CALLING_CONVENTION thread_rapid_enqueuer_and_dequeuer( void *libtest_threadset_per_thread_state )
{
  lfds710_pal_uint_t
    time_loop = 0;

  struct lfds710_queue_umm_element
    *qe;

  struct test_element
    *te;

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
    lfds710_queue_umm_dequeue( tpts->qs, &qe );
    te = LFDS710_QUEUE_UMM_GET_VALUE_FROM_ELEMENT( *qe );

    te->thread_number = tpts->thread_number;
    te->counter = tpts->counter++;

    LFDS710_QUEUE_UMM_SET_VALUE_IN_ELEMENT( *qe, te );
    lfds710_queue_umm_enqueue( tpts->qs, qe );

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

