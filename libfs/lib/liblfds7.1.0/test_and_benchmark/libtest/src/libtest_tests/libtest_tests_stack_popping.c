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

  enum flag
    popped_flag;
};

/***** private prototypes *****/
static libshared_pal_thread_return_t LIBSHARED_PAL_THREAD_CALLING_CONVENTION thread_popping( void *libtest_threadset_per_thread_state );





/****************************************************************************/
void libtest_tests_stack_popping( struct lfds710_list_asu_state *list_of_logical_processors, struct libshared_memory_state *ms, enum lfds710_misc_validity *dvs )
{
  lfds710_pal_uint_t
    loop = 0,
    number_elements,
    number_logical_processors;

  struct lfds710_stack_state
    ss;

  struct lfds710_list_asu_element
    *lasue = NULL;

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

  /* TRD : we create a stack with as many elements as possible elements

           the creation function runs in a single thread and creates
           and pushes those elements onto the stack

           each element contains a void pointer to the container test element

           we then run one thread per CPU
           where each thread loops, popping as quickly as possible
           each test element has a flag which indicates it has been popped

           the threads run till the source stack is empty

           we then check the test elements
           every element should have been popped

           then tidy up
  */

  *dvs = LFDS710_MISC_VALIDITY_VALID;

  // TRD : allocate
  lfds710_list_asu_query( list_of_logical_processors, LFDS710_LIST_ASU_QUERY_GET_POTENTIALLY_INACCURATE_COUNT, NULL, (void **) &number_logical_processors );
  tpts = libshared_memory_alloc_from_unknown_node( ms, sizeof(struct test_per_thread_state) * number_logical_processors, LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
  pts = libshared_memory_alloc_from_unknown_node( ms, sizeof(struct libtest_threadset_per_thread_state) * number_logical_processors, LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
  te_array = libshared_memory_alloc_largest_possible_array_from_unknown_node( ms, sizeof(struct test_element), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES, &number_elements );

  lfds710_stack_init_valid_on_current_logical_core( &ss, NULL );

  for( loop = 0 ; loop < number_elements ; loop++ )
  {
    (te_array+loop)->popped_flag = LOWERED;
    LFDS710_STACK_SET_VALUE_IN_ELEMENT( (te_array+loop)->se, te_array+loop );
    lfds710_stack_push( &ss, &(te_array+loop)->se );
  }

  libtest_threadset_init( &ts, NULL );

  loop = 0;

  while( LFDS710_LIST_ASU_GET_START_AND_THEN_NEXT(*list_of_logical_processors, lasue) )
  {
    lp = LFDS710_LIST_ASU_GET_VALUE_FROM_ELEMENT( *lasue );
    tpts[loop].ss = &ss;

    libtest_threadset_add_thread( &ts, &pts[loop], lp, thread_popping, &tpts[loop] );

    loop++;
  }

  LFDS710_MISC_BARRIER_STORE;

  lfds710_misc_force_store();

  // TRD : run the test
  libtest_threadset_run( &ts );

  libtest_threadset_cleanup( &ts );

  // TRD : validate
  LFDS710_MISC_BARRIER_LOAD;

  lfds710_stack_query( &ss, LFDS710_STACK_QUERY_SINGLETHREADED_VALIDATE, &vi, dvs );

  // TRD : now we check each element has popped_flag set to RAISED
  for( loop = 0 ; loop < number_elements ; loop++ )
    if( (te_array+loop)->popped_flag == LOWERED )
      *dvs = LFDS710_MISC_VALIDITY_INVALID_TEST_DATA;

  // TRD : cleanup
  lfds710_stack_cleanup( &ss, NULL );

  return;
}





/****************************************************************************/
static libshared_pal_thread_return_t LIBSHARED_PAL_THREAD_CALLING_CONVENTION thread_popping( void *libtest_threadset_per_thread_state )
{
  struct lfds710_stack_element
    *se;

  struct test_per_thread_state
    *tpts;

  struct libtest_threadset_per_thread_state
    *pts;

  struct test_element
    *te;

  LFDS710_MISC_MAKE_VALID_ON_CURRENT_LOGICAL_CORE_INITS_COMPLETED_BEFORE_NOW_ON_ANY_OTHER_LOGICAL_CORE;

  LFDS710_PAL_ASSERT( libtest_threadset_per_thread_state != NULL );

  pts = (struct libtest_threadset_per_thread_state *) libtest_threadset_per_thread_state;

  tpts = LIBTEST_THREADSET_GET_USER_STATE_FROM_PER_THREAD_STATE( *pts );

  libtest_threadset_thread_ready_and_wait( pts );

  while( lfds710_stack_pop(tpts->ss, &se) )
  {
    te = LFDS710_STACK_GET_VALUE_FROM_ELEMENT( *se );
    te->popped_flag = RAISED;
  }

  LFDS710_MISC_BARRIER_STORE;

  lfds710_misc_force_store();

  return LIBSHARED_PAL_THREAD_RETURN_CAST(RETURN_SUCCESS);
}

