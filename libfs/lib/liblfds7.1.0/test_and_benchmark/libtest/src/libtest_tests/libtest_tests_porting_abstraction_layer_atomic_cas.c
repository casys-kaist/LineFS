/***** includes *****/
#include "libtest_tests_internal.h"

/***** structs *****/
struct libtest_tests_pal_atomic_cas_state
{
  lfds710_pal_uint_t
    local_counter;

  lfds710_pal_uint_t volatile
    *shared_counter;
};

/***** private prototyps *****/
static libshared_pal_thread_return_t LIBSHARED_PAL_THREAD_CALLING_CONVENTION thread_cas( void *libtest_threadset_per_thread_state );





/****************************************************************************/
void libtest_tests_pal_atomic_cas( struct lfds710_list_asu_state *list_of_logical_processors, struct libshared_memory_state *ms, enum lfds710_misc_validity *dvs )
{
  lfds710_pal_uint_t volatile LFDS710_PAL_ALIGN(LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES)
    shared_counter;

  lfds710_pal_uint_t
    local_total = 0;

  lfds710_pal_uint_t
    loop = 0,
    number_logical_processors;

  struct lfds710_list_asu_element
    *lasue = NULL;

  struct libtest_logical_processor
    *lp;

  struct libtest_threadset_per_thread_state
    *pts;

  struct libtest_threadset_state
    ts;

  struct libtest_tests_pal_atomic_cas_state
    *atcs;

  LFDS710_PAL_ASSERT( list_of_logical_processors != NULL );
  LFDS710_PAL_ASSERT( ms != NULL );
  LFDS710_PAL_ASSERT( dvs != NULL );

  /* TRD : here we test pal_cas

           we run one thread per CPU
           we use pal_cas() to increment a shared counter
           every time a thread successfully increments the counter,
           it increments a thread local counter
           the threads run for ten seconds
           after the threads finish, we total the local counters
           they should equal the shared counter
  */

  // TRD : allocate
  lfds710_list_asu_query( list_of_logical_processors, LFDS710_LIST_ASU_QUERY_GET_POTENTIALLY_INACCURATE_COUNT, NULL, (void **) &number_logical_processors );
  pts = libshared_memory_alloc_from_unknown_node( ms, sizeof(struct libtest_threadset_per_thread_state) * number_logical_processors, LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
  atcs = libshared_memory_alloc_from_unknown_node( ms, sizeof(struct libtest_tests_pal_atomic_cas_state) * number_logical_processors, LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );

  shared_counter = 0;

  for( loop = 0 ; loop < number_logical_processors ; loop++ )
  {
    (atcs+loop)->shared_counter = &shared_counter;
    (atcs+loop)->local_counter = 0;
  }

  libtest_threadset_init( &ts, NULL );

  loop = 0;

  while( LFDS710_LIST_ASU_GET_START_AND_THEN_NEXT(*list_of_logical_processors,lasue) )
  {
    lp = LFDS710_LIST_ASU_GET_VALUE_FROM_ELEMENT( *lasue );
    libtest_threadset_add_thread( &ts, &pts[loop], lp, thread_cas, atcs+loop );
    loop++;
  }

  LFDS710_MISC_BARRIER_STORE;

  lfds710_misc_force_store();

  // TRD : run the test
  libtest_threadset_run( &ts );

  libtest_threadset_cleanup( &ts );

  // TRD : validate
  LFDS710_MISC_BARRIER_LOAD;

  for( loop = 0 ; loop < number_logical_processors ; loop++ )
    local_total += (atcs+loop)->local_counter;

  if( local_total == shared_counter )
    *dvs = LFDS710_MISC_VALIDITY_VALID;

  if( local_total != shared_counter )
    *dvs = LFDS710_MISC_VALIDITY_INVALID_TEST_DATA;

  return;
}





/****************************************************************************/
static libshared_pal_thread_return_t LIBSHARED_PAL_THREAD_CALLING_CONVENTION thread_cas( void *libtest_threadset_per_thread_state )
{
  char unsigned 
    result;

  lfds710_pal_uint_t
    loop = 0;

  lfds710_pal_uint_t LFDS710_PAL_ALIGN(LFDS710_PAL_ALIGN_SINGLE_POINTER)
    exchange,
    compare;

  struct libtest_tests_pal_atomic_cas_state
    *atcs;

  struct libtest_threadset_per_thread_state
    *pts;

  LFDS710_MISC_BARRIER_LOAD;

  LFDS710_PAL_ASSERT( libtest_threadset_per_thread_state != NULL );

  pts = (struct libtest_threadset_per_thread_state *) libtest_threadset_per_thread_state;
  atcs = LIBTEST_THREADSET_GET_USER_STATE_FROM_PER_THREAD_STATE( *pts );

  LFDS710_MISC_BARRIER_LOAD;

  libtest_threadset_thread_ready_and_wait( pts );

  while( loop++ < 10000000 )
  {
    compare = *atcs->shared_counter;

    do
    {
      exchange = compare + 1;
      LFDS710_PAL_ATOMIC_CAS( atcs->shared_counter, &compare, exchange, LFDS710_MISC_CAS_STRENGTH_WEAK, result );
    }
    while( result == 0 );

    atcs->local_counter++;
  }

  LFDS710_MISC_BARRIER_STORE;

  lfds710_misc_force_store();

  return LIBSHARED_PAL_THREAD_RETURN_CAST(RETURN_SUCCESS);
}

