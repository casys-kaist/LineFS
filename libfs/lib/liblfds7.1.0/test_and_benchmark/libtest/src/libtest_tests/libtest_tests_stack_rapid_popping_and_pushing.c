/***** includes *****/
#include "libtest_tests_internal.h"

/***** structs *****/
struct test_per_thread_state
{
  struct lfds710_stack_state
    *ss;
};

struct test_element
{
  struct lfds710_stack_element
    se;

  lfds710_pal_uint_t
    datum;
};

/***** private prototypes *****/
static libshared_pal_thread_return_t LIBSHARED_PAL_THREAD_CALLING_CONVENTION thread_rapid_popping_and_pushing( void *libtest_threadset_per_thread_state );





/****************************************************************************/
void libtest_tests_stack_rapid_popping_and_pushing( struct lfds710_list_asu_state *list_of_logical_processors, struct libshared_memory_state *ms, enum lfds710_misc_validity *dvs )
{
  lfds710_pal_uint_t
    index = 0,
    loop,
    number_logical_processors;

  struct lfds710_list_asu_element
    *lasue = NULL;

  struct lfds710_misc_validation_info
    vi = { 0, 0 };

  struct lfds710_stack_state
    ss;

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

  /* TRD : in these tests there is a fundamental antagonism between
           how much checking/memory clean up that we do and the
           likelyhood of collisions between threads in their lock-free
           operations

           the lock-free operations are very quick; if we do anything
           much at all between operations, we greatly reduce the chance
           of threads colliding

           so we have some tests which do enough checking/clean up that
           they can tell the stack is valid and don't leak memory
           and here, this test now is one of those which does minimal
           checking - in fact, the nature of the test is that you can't
           do any real checking - but goes very quickly

           what we do is create a small stack and then run one thread
           per CPU, where each thread simply pushes and then immediately
           pops

           the test runs for ten seconds

           after the test is done, the only check we do is to traverse
           the stack, checking for loops and ensuring the number of
           elements is correct
  */

  // TRD : allocate
  lfds710_list_asu_query( list_of_logical_processors, LFDS710_LIST_ASU_QUERY_GET_POTENTIALLY_INACCURATE_COUNT, NULL, (void **) &number_logical_processors );
  tpts = libshared_memory_alloc_from_unknown_node( ms, sizeof(struct test_per_thread_state) * number_logical_processors, LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
  pts = libshared_memory_alloc_from_unknown_node( ms, sizeof(struct libtest_threadset_per_thread_state) * number_logical_processors, LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
  te_array = libshared_memory_alloc_from_unknown_node( ms, sizeof(struct test_element) * number_logical_processors, LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );

  lfds710_stack_init_valid_on_current_logical_core( &ss, NULL );

  for( loop = 0 ; loop < number_logical_processors ; loop++ )
    tpts[loop].ss = &ss;

  for( loop = 0 ; loop < number_logical_processors ; loop++ )
  {
    LFDS710_STACK_SET_VALUE_IN_ELEMENT( te_array[loop].se, &te_array[loop] );
    lfds710_stack_push( &ss, &te_array[loop].se );
  }

  // TRD : get the threads ready
  libtest_threadset_init( &ts, NULL );

  while( LFDS710_LIST_ASU_GET_START_AND_THEN_NEXT(*list_of_logical_processors,lasue) )
  {
    lp = LFDS710_LIST_ASU_GET_VALUE_FROM_ELEMENT( *lasue );
    libtest_threadset_add_thread( &ts, &pts[index], lp, thread_rapid_popping_and_pushing, &tpts[index] );
    index++;
  }

  // TRD : run the test
  libtest_threadset_run( &ts );

  libtest_threadset_cleanup( &ts );

  // TRD : validate
  LFDS710_MISC_BARRIER_LOAD;

  vi.min_elements = vi.max_elements = number_logical_processors;

  lfds710_stack_query( &ss, LFDS710_STACK_QUERY_SINGLETHREADED_VALIDATE, &vi, dvs );

  lfds710_stack_cleanup( &ss, NULL );

  return;
}





/****************************************************************************/
static libshared_pal_thread_return_t LIBSHARED_PAL_THREAD_CALLING_CONVENTION thread_rapid_popping_and_pushing( void *libtest_threadset_per_thread_state )
{
  lfds710_pal_uint_t
    time_loop = 0;

  struct lfds710_stack_element
    *se;

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

  while( current_time < start_time + TEST_DURATION_IN_SECONDS )
  {
    lfds710_stack_pop( tpts->ss, &se );
    lfds710_stack_push( tpts->ss, se );

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

