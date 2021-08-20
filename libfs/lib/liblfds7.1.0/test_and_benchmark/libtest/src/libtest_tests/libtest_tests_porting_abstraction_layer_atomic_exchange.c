/***** includes *****/
#include "libtest_tests_internal.h"

/***** structs *****/
struct test_per_thread_state
{
  lfds710_pal_uint_t
    counter,
    *counter_array,
    number_elements_per_thread,
    number_logical_processors;

  lfds710_pal_uint_t volatile
    *shared_exchange;
};

/***** private prototyps *****/
static libshared_pal_thread_return_t LIBSHARED_PAL_THREAD_CALLING_CONVENTION thread_exchange( void *libtest_threadset_per_thread_state );
static libshared_pal_thread_return_t LIBSHARED_PAL_THREAD_CALLING_CONVENTION thread_atomic_exchange( void *libtest_threadset_per_thread_state );





/****************************************************************************/
void libtest_tests_pal_atomic_exchange( struct lfds710_list_asu_state *list_of_logical_processors, struct libshared_memory_state *ms, enum lfds710_misc_validity *dvs )
{
  enum flag
    atomic_exchange_success_flag = RAISED,
    exchange_success_flag = RAISED;

  lfds710_pal_uint_t
    loop,
    *merged_counter_arrays,
    number_elements,
    number_elements_per_thread,
    number_logical_processors,
    subloop;

  lfds710_pal_uint_t volatile LFDS710_PAL_ALIGN(LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES)
    exchange = 0;

  struct lfds710_list_asu_element
    *lasue = NULL;

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

  /* TRD : here we test pal_atomic_exchange

           we have one thread per logical core
           there is one variable which every thread will exchange to/from 
           we know the number of logical cores
           the threads have a counter each, which begins with their logical core number plus one
           (plus one because the exchange counter begins with 0 already in place)
           (e.g. thread 0 begins with its counter at 1, thread 1 begins with its counter at 2, etc)

           there is an array per thread of 1 million elements, each a counter, set to 0

           when running, each thread increments its counter by the number of threads
           the threads busy loop, exchanging
           every time aa thread pulls a number off the central, shared exchange variable,
           it increments the counter for that variable in its thread-local counter array

           (we're not using a global array, because we'd have to be atomic in our increments,
            which is a slow-down we don't want)

           at the end, we merge all the counter arrays and if the frequency for a counter is a value
           other than 1, the exchange was not atomic

           we perform the test twice, once with pal_atomic_exchange, once with a non-atomic exchange

           we expect the atomic to pass and the non-atomic to fail
  */

  *dvs = LFDS710_MISC_VALIDITY_VALID;

  // TRD : allocate
  lfds710_list_asu_query( list_of_logical_processors, LFDS710_LIST_ASU_QUERY_GET_POTENTIALLY_INACCURATE_COUNT, NULL, (void **) &number_logical_processors );
  tpts = libshared_memory_alloc_from_unknown_node( ms, sizeof(struct test_per_thread_state) * number_logical_processors, LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
  pts = libshared_memory_alloc_from_unknown_node( ms, sizeof(struct libtest_threadset_per_thread_state) * number_logical_processors, LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
  merged_counter_arrays = libshared_memory_alloc_largest_possible_array_from_unknown_node( ms, sizeof(lfds710_pal_uint_t), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES, &number_elements );

  /* TRD : one array per thread, one array for merging
           +1 as we need store for a merged counter array
  */

  number_elements_per_thread = number_elements / (number_logical_processors+1);

  // TRD : non-atomic

  for( loop = 0 ; loop < number_elements_per_thread ; loop++ )
    *(merged_counter_arrays+loop) = 0;

  libtest_threadset_init( &ts, NULL );

  loop = 0;

  while( LFDS710_LIST_ASU_GET_START_AND_THEN_NEXT(*list_of_logical_processors,lasue) )
  {
    lp = LFDS710_LIST_ASU_GET_VALUE_FROM_ELEMENT( *lasue );

    (tpts+loop)->counter = loop + 1;
    // TRD : +1 on loop to move past merged_counter_arrays
    (tpts+loop)->counter_array = merged_counter_arrays + ((loop+1)*number_elements_per_thread);
    for( subloop = 0 ; subloop < number_elements_per_thread ; subloop++ )
      *((tpts+loop)->counter_array+subloop) = 0;
    (tpts+loop)->number_logical_processors = number_logical_processors;
    (tpts+loop)->shared_exchange = &exchange;
    (tpts+loop)->number_elements_per_thread = number_elements_per_thread;

    libtest_threadset_add_thread( &ts, &pts[loop], lp, thread_exchange, &tpts[loop] );

    loop++;
  }

  LFDS710_MISC_BARRIER_STORE;

  lfds710_misc_force_store();

  // TRD : run the test
  libtest_threadset_run( &ts );
  libtest_threadset_cleanup( &ts );

  // TRD : validate
  LFDS710_MISC_BARRIER_LOAD;

  for( loop = 0 ; loop < number_elements_per_thread ; loop++ )
    for( subloop = 0 ; subloop < number_logical_processors ; subloop++ )
      *(merged_counter_arrays+loop) += *( (tpts+subloop)->counter_array+loop );

  /* TRD : the worker threads exit when their per-thread counter exceeds number_elements_per_thread
           as such the final number_logical_processors numbers are not read
           we could change the threads to exit when the number they read exceeds number_elements_per_thread
           but then we'd need an if() in their work-loop,
           and we want to go as fast as possible
  */

  for( loop = 0 ; loop < number_elements_per_thread - number_logical_processors ; loop++ )
    if( *(merged_counter_arrays+loop) != 1 )
      exchange_success_flag = LOWERED;

  // TRD : now for atomic exchange - we need to re-init the data structures

  for( loop = 0 ; loop < number_elements_per_thread ; loop++ )
    *(merged_counter_arrays+loop) = 0;

  libtest_threadset_init( &ts, NULL );

  for( loop = 0 ; loop < number_logical_processors ; loop++ )
    for( subloop = 0 ; subloop < number_elements_per_thread ; subloop++ )
      *((tpts+loop)->counter_array+subloop) = 0;

  loop = 0;
  lasue = NULL;

  while( LFDS710_LIST_ASU_GET_START_AND_THEN_NEXT(*list_of_logical_processors,lasue) )
  {
    lp = LFDS710_LIST_ASU_GET_VALUE_FROM_ELEMENT( *lasue );

    libtest_threadset_add_thread( &ts, &pts[loop], lp, thread_atomic_exchange, &tpts[loop] );
    loop++;
  }

  exchange = 0;

  LFDS710_MISC_BARRIER_STORE;

  lfds710_misc_force_store();

  libtest_threadset_run( &ts );

  libtest_threadset_cleanup( &ts );

  // TRD : validate
  LFDS710_MISC_BARRIER_LOAD;

  for( loop = 0 ; loop < number_elements_per_thread ; loop++ )
    for( subloop = 0 ; subloop < number_logical_processors ; subloop++ )
      *(merged_counter_arrays+loop) += *( (tpts+subloop)->counter_array+loop );

  for( loop = 0 ; loop < number_elements_per_thread - number_logical_processors ; loop++ )
    if( *(merged_counter_arrays+loop) != 1 )
      atomic_exchange_success_flag = LOWERED;

  /* TRD : results

           on a single core, atomic and non-atomic exchange should both work

           if we find our non-atomic test passes, then we can't really say anything
           about whether or not the atomic test is really working
  */

  LFDS710_MISC_BARRIER_LOAD;

  if( number_logical_processors == 1 )
  {
    if( exchange_success_flag == RAISED and atomic_exchange_success_flag == RAISED )
      *dvs = LFDS710_MISC_VALIDITY_VALID;

    if( exchange_success_flag != RAISED or atomic_exchange_success_flag != RAISED )
      *dvs = LFDS710_MISC_VALIDITY_INVALID_ATOMIC_FAILED;
  }

  if( number_logical_processors >= 2 )
  {
    if( atomic_exchange_success_flag == RAISED and exchange_success_flag == LOWERED )
      *dvs = LFDS710_MISC_VALIDITY_VALID;

    if( atomic_exchange_success_flag == RAISED and exchange_success_flag == RAISED )
      *dvs = LFDS710_MISC_VALIDITY_INDETERMINATE_NONATOMIC_PASSED;

    if( atomic_exchange_success_flag == LOWERED )
      *dvs = LFDS710_MISC_VALIDITY_INVALID_ATOMIC_FAILED;
  }

  return;
}





/****************************************************************************/
static libshared_pal_thread_return_t LIBSHARED_PAL_THREAD_CALLING_CONVENTION thread_exchange( void *libtest_threadset_per_thread_state )
{
  lfds710_pal_uint_t
    local_counter,
    exchange;

  struct test_per_thread_state
    *tpts;

  struct libtest_threadset_per_thread_state
    *pts;

  LFDS710_MISC_BARRIER_LOAD;

  LFDS710_PAL_ASSERT( libtest_threadset_per_thread_state != NULL );

  pts = (struct libtest_threadset_per_thread_state *) libtest_threadset_per_thread_state;
  tpts = LIBTEST_THREADSET_GET_USER_STATE_FROM_PER_THREAD_STATE( *pts );

  libtest_threadset_thread_ready_and_wait( pts );

  local_counter = tpts->counter;

  while( local_counter < tpts->number_elements_per_thread )
  {
    exchange = *tpts->shared_exchange;
    *tpts->shared_exchange = local_counter;

    ( *(tpts->counter_array + exchange) )++;

    local_counter += tpts->number_logical_processors;
  }

  LFDS710_MISC_BARRIER_STORE;

  lfds710_misc_force_store();

  return LIBSHARED_PAL_THREAD_RETURN_CAST(RETURN_SUCCESS);
}





/****************************************************************************/
static libshared_pal_thread_return_t LIBSHARED_PAL_THREAD_CALLING_CONVENTION thread_atomic_exchange( void *libtest_threadset_per_thread_state )
{
  lfds710_pal_uint_t
    local_counter,
    exchange;

  struct test_per_thread_state
    *tpts;

  struct libtest_threadset_per_thread_state
    *pts;

  LFDS710_MISC_BARRIER_LOAD;

  LFDS710_PAL_ASSERT( libtest_threadset_per_thread_state != NULL );

  pts = (struct libtest_threadset_per_thread_state *) libtest_threadset_per_thread_state;
  tpts = LIBTEST_THREADSET_GET_USER_STATE_FROM_PER_THREAD_STATE( *pts );

  libtest_threadset_thread_ready_and_wait( pts );

  local_counter = tpts->counter;

  while( local_counter < tpts->number_elements_per_thread )
  {
    exchange = local_counter;

    LFDS710_PAL_ATOMIC_EXCHANGE( tpts->shared_exchange, exchange, lfds710_pal_uint_t );

    // TRD : increment the original value in shared_exchange, which exchange has now been set to
    ( *(tpts->counter_array + exchange) )++;

    local_counter += (lfds710_pal_uint_t) tpts->number_logical_processors;
  }

  LFDS710_MISC_BARRIER_STORE;

  lfds710_misc_force_store();

  return LIBSHARED_PAL_THREAD_RETURN_CAST(RETURN_SUCCESS);
}

