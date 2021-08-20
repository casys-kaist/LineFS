/***** includes *****/
#include "libtest_tests_internal.h"

/***** private prototyps *****/
static libshared_pal_thread_return_t LIBSHARED_PAL_THREAD_CALLING_CONVENTION thread_add( void *libtest_threadset_per_thread_state );
static libshared_pal_thread_return_t LIBSHARED_PAL_THREAD_CALLING_CONVENTION thread_atomic_add( void *libtest_threadset_per_thread_state );





/****************************************************************************/
void libtest_tests_pal_atomic_add( struct lfds710_list_asu_state *list_of_logical_processors, struct libshared_memory_state *ms, enum lfds710_misc_validity *dvs )
{
  lfds710_pal_uint_t volatile LFDS710_PAL_ALIGN(LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES)
    number_logical_processors,
    atomic_shared_counter,
    shared_counter;

  lfds710_pal_uint_t
    loop = 0;

  struct lfds710_list_asu_element
    *lasue = NULL;

  struct libtest_logical_processor
    *lp;

  struct libtest_threadset_per_thread_state
    *pts;

  struct libtest_threadset_state
    ts;

  LFDS710_PAL_ASSERT( list_of_logical_processors != NULL );
  LFDS710_PAL_ASSERT( ms != NULL );
  LFDS710_PAL_ASSERT( dvs != NULL );

  /* TRD : here we test abstraction_atomic_add

           first, we run one thread per CPU where each thread adds
           a shared counter 10,000,000 times - however, this first test
           does NOT use atomic add; it uses "++"

           second, we repeat the exercise, but this time using
           abstraction_add()

           if the final value in the first test is less than (10,000,000*asi->number_of_components[LFDS710_ABSTRACTION_COMPONENT_LOGICAL_PROCESSOR])
           then the system is sensitive to non-atomic adds; this means if
           our atomic version of the test passes, we can have some degree of confidence
           that it works

           if the final value in the first test is in fact correct, then we can't know
           that our atomic version has changed anything

           and of course if the final value in the atomic test is wrong, we know things
           are broken
  */

  // TRD : allocate
  lfds710_list_asu_query( list_of_logical_processors, LFDS710_LIST_ASU_QUERY_GET_POTENTIALLY_INACCURATE_COUNT, NULL, (void **) &number_logical_processors );
  pts = libshared_memory_alloc_from_unknown_node( ms, sizeof(struct libtest_threadset_per_thread_state) * number_logical_processors * 2, LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );

  shared_counter = 0;
  atomic_shared_counter = 0;

  LFDS710_MISC_BARRIER_STORE;

  // TRD : non-atomic

  libtest_threadset_init( &ts, NULL );

  while( LFDS710_LIST_ASU_GET_START_AND_THEN_NEXT(*list_of_logical_processors, lasue) )
  {
    lp = LFDS710_LIST_ASU_GET_VALUE_FROM_ELEMENT( *lasue );
    libtest_threadset_add_thread( &ts, &pts[loop], lp, thread_add, (void *) &shared_counter );
    loop++;
  }

  LFDS710_MISC_BARRIER_STORE;
  lfds710_misc_force_store();
  libtest_threadset_run( &ts );
  libtest_threadset_cleanup( &ts );

  // TRD : atomic

  libtest_threadset_init( &ts, NULL );

  loop = 0;
  lasue = NULL;

  while( LFDS710_LIST_ASU_GET_START_AND_THEN_NEXT(*list_of_logical_processors, lasue) )
  {
    lp = LFDS710_LIST_ASU_GET_VALUE_FROM_ELEMENT( *lasue );
    libtest_threadset_add_thread( &ts, &pts[loop], lp, thread_atomic_add, (void *) &atomic_shared_counter );
    loop++;
  }

  LFDS710_MISC_BARRIER_STORE;
  lfds710_misc_force_store();
  libtest_threadset_run( &ts );
  libtest_threadset_cleanup( &ts );
  LFDS710_MISC_BARRIER_LOAD;

  /* TRD : results

           on a single core, "++" and atomic add should be equal

           if we find our non-atomic test passes, then we can't really say anything
           about whether or not the atomic test is really working
  */

  if( number_logical_processors == 1 )
  {
    if( shared_counter == (10000000 * number_logical_processors) and atomic_shared_counter == (10000000 * number_logical_processors) )
      *dvs = LFDS710_MISC_VALIDITY_VALID;

    if( shared_counter != (10000000 * number_logical_processors) or atomic_shared_counter != (10000000 * number_logical_processors) )
      *dvs = LFDS710_MISC_VALIDITY_INVALID_ATOMIC_FAILED;
  }

  if( number_logical_processors >= 2 )
  {
    if( shared_counter < (10000000 * number_logical_processors) and atomic_shared_counter == (10000000 * number_logical_processors) )
      *dvs = LFDS710_MISC_VALIDITY_VALID;

    if( shared_counter == (10000000 * number_logical_processors) and atomic_shared_counter == (10000000 * number_logical_processors) )
      *dvs = LFDS710_MISC_VALIDITY_INDETERMINATE_NONATOMIC_PASSED;

    if( atomic_shared_counter < (10000000 * number_logical_processors) )
      *dvs = LFDS710_MISC_VALIDITY_INVALID_ATOMIC_FAILED;
  }

  return;
}





/****************************************************************************/
static libshared_pal_thread_return_t LIBSHARED_PAL_THREAD_CALLING_CONVENTION thread_add( void *libtest_threadset_per_thread_state )
{
  struct libtest_threadset_per_thread_state
    *pts;

  lfds710_pal_uint_t volatile
    *shared_counter;

  lfds710_pal_uint_t volatile
    count = 0;

  LFDS710_PAL_ASSERT( libtest_threadset_per_thread_state != NULL );

  LFDS710_MISC_BARRIER_LOAD;

  pts = (struct libtest_threadset_per_thread_state *) libtest_threadset_per_thread_state;
  shared_counter = LIBTEST_THREADSET_GET_USER_STATE_FROM_PER_THREAD_STATE( *pts );

  libtest_threadset_thread_ready_and_wait( pts );

  while( count++ < 10000000 )
    (*shared_counter)++;

  return LIBSHARED_PAL_THREAD_RETURN_CAST(RETURN_SUCCESS);
}





/****************************************************************************/
static libshared_pal_thread_return_t LIBSHARED_PAL_THREAD_CALLING_CONVENTION thread_atomic_add( void *libtest_threadset_per_thread_state )
{
  struct libtest_threadset_per_thread_state
    *pts;

  lfds710_pal_uint_t volatile
    result,
    *shared_counter;

  lfds710_pal_uint_t volatile
    count = 0;

  LFDS710_PAL_ASSERT( libtest_threadset_per_thread_state != NULL );

  LFDS710_MISC_BARRIER_LOAD;

  pts = (struct libtest_threadset_per_thread_state *) libtest_threadset_per_thread_state;
  shared_counter = LIBTEST_THREADSET_GET_USER_STATE_FROM_PER_THREAD_STATE( *pts );

  libtest_threadset_thread_ready_and_wait( pts );

  while( count++ < 10000000 )
  {
    LFDS710_PAL_ATOMIC_ADD( shared_counter, 1, result, lfds710_pal_uint_t );
    (void) result;
  }

  return LIBSHARED_PAL_THREAD_RETURN_CAST(RETURN_SUCCESS);
}

