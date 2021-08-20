/***** includes *****/
#include "libtest_tests_internal.h"

/***** structs *****/
struct test_per_thread_state
{
  enum flag
    error_flag;

  struct lfds710_queue_umm_state
    *qs;
};

struct test_element
{
  struct lfds710_queue_umm_element
    qe;
};

/***** private prototypes *****/
static libshared_pal_thread_return_t LIBSHARED_PAL_THREAD_CALLING_CONVENTION thread_simple_dequeuer( void *libtest_threadset_per_thread_state );





/****************************************************************************/
void libtest_tests_queue_umm_dequeuing( struct lfds710_list_asu_state *list_of_logical_processors, struct libshared_memory_state *ms, enum lfds710_misc_validity *dvs )
{
  lfds710_pal_uint_t
    loop,
    number_elements,
    number_logical_processors;

  struct lfds710_list_asu_element
    *lasue = NULL;

  struct lfds710_queue_umm_element LFDS710_PAL_ALIGN(LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES)
    qe_dummy;

  struct lfds710_queue_umm_state
    qs;

  struct lfds710_misc_validation_info
    vi = { 0, 0 };

  struct libtest_logical_processor
    *lp;

  struct libtest_threadset_per_thread_state
    *pts;

  struct libtest_threadset_state
    ts;

  struct test_element
    *te_array;

  struct test_per_thread_state
    *tpts;

  LFDS710_PAL_ASSERT( list_of_logical_processors != NULL );
  LFDS710_PAL_ASSERT( ms != NULL );
  LFDS710_PAL_ASSERT( dvs != NULL );

  /* TRD : create a queue, add 1,000,000 elements

           use a single thread to enqueue every element
           each elements user data is an incrementing counter

           then run one thread per CPU
           where each busy-works dequeuing

           when an element is dequeued, we check (on a per-thread basis) the
           value dequeued is greater than the element previously dequeued
  */

  *dvs = LFDS710_MISC_VALIDITY_VALID;

  // TRD : allocate
  lfds710_list_asu_query( list_of_logical_processors, LFDS710_LIST_ASU_QUERY_GET_POTENTIALLY_INACCURATE_COUNT, NULL, (void **) &number_logical_processors );
  tpts = libshared_memory_alloc_from_unknown_node( ms, sizeof(struct test_per_thread_state) * number_logical_processors, LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
  pts = libshared_memory_alloc_from_unknown_node( ms, sizeof(struct libtest_threadset_per_thread_state) * number_logical_processors, LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
  te_array = libshared_memory_alloc_largest_possible_array_from_unknown_node( ms, sizeof(struct test_element), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES, &number_elements );

  lfds710_queue_umm_init_valid_on_current_logical_core( &qs, &qe_dummy, NULL );

  for( loop = 0 ; loop < number_elements ; loop++ )
  {
    LFDS710_QUEUE_UMM_SET_VALUE_IN_ELEMENT( (te_array+loop)->qe, loop );
    lfds710_queue_umm_enqueue( &qs, &(te_array+loop)->qe );
  }

  libtest_threadset_init( &ts, NULL );

  loop = 0;

  while( LFDS710_LIST_ASU_GET_START_AND_THEN_NEXT(*list_of_logical_processors,lasue) )
  {
    lp = LFDS710_LIST_ASU_GET_VALUE_FROM_ELEMENT( *lasue );
    (tpts+loop)->qs = &qs;
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

  // TRD : check queue is empty
  lfds710_queue_umm_query( &qs, LFDS710_QUEUE_UMM_QUERY_SINGLETHREADED_VALIDATE, &vi, dvs );

  // TRD : check for raised error flags
  for( loop = 0 ; loop < number_logical_processors ; loop++ )
    if( (tpts+loop)->error_flag == RAISED )
      *dvs = LFDS710_MISC_VALIDITY_INVALID_TEST_DATA;

  lfds710_queue_umm_cleanup( &qs, NULL );

  return;
}





/****************************************************************************/
static libshared_pal_thread_return_t LIBSHARED_PAL_THREAD_CALLING_CONVENTION thread_simple_dequeuer( void *libtest_threadset_per_thread_state )
{
  lfds710_pal_uint_t
    *prev_value,
    *value;

  struct lfds710_queue_umm_element
    *qe;

  struct test_per_thread_state
    *tpts;

  struct libtest_threadset_per_thread_state
    *pts;

  LFDS710_MISC_MAKE_VALID_ON_CURRENT_LOGICAL_CORE_INITS_COMPLETED_BEFORE_NOW_ON_ANY_OTHER_LOGICAL_CORE;

  LFDS710_PAL_ASSERT( libtest_threadset_per_thread_state != NULL );

  pts = (struct libtest_threadset_per_thread_state *) libtest_threadset_per_thread_state;
  tpts = LIBTEST_THREADSET_GET_USER_STATE_FROM_PER_THREAD_STATE( *pts );

  lfds710_queue_umm_dequeue( tpts->qs, &qe );
  prev_value = LFDS710_QUEUE_UMM_GET_VALUE_FROM_ELEMENT( *qe );

  libtest_threadset_thread_ready_and_wait( pts );

  while( lfds710_queue_umm_dequeue(tpts->qs, &qe) )
  {
    value = LFDS710_QUEUE_UMM_GET_VALUE_FROM_ELEMENT( *qe );

    if( value <= prev_value )
      tpts->error_flag = RAISED;

    prev_value = value;
  }

  LFDS710_MISC_BARRIER_STORE;

  lfds710_misc_force_store();

  return LIBSHARED_PAL_THREAD_RETURN_CAST(RETURN_SUCCESS);
}

