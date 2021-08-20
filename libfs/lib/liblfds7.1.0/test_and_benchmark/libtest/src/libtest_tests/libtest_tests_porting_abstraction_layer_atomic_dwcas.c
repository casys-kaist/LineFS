/***** includes *****/
#include "libtest_tests_internal.h"

/***** structs *****/
struct libtest_tests_pal_atomic_dwcas_state
{
  lfds710_pal_uint_t
    local_counter;

  lfds710_pal_uint_t volatile
    (*shared_counter)[2];
};

/***** private prototyps *****/
static libshared_pal_thread_return_t LIBSHARED_PAL_THREAD_CALLING_CONVENTION thread_dwcas( void *libtest_threadset_per_thread_state );





/****************************************************************************/
void libtest_tests_pal_atomic_dwcas( struct lfds710_list_asu_state *list_of_logical_processors, struct libshared_memory_state *ms, enum lfds710_misc_validity *dvs )
{
  lfds710_pal_uint_t
    local_total = 0,
    loop,
    number_logical_processors;

  lfds710_pal_uint_t volatile LFDS710_PAL_ALIGN(LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES)
    shared_counter[2] = { 0, 0 };

  struct lfds710_list_asu_element
    *lasue = NULL;

  struct libtest_logical_processor
    *lp;

  struct libtest_threadset_per_thread_state
    *pts;

  struct libtest_threadset_state
    ts;

  struct libtest_tests_pal_atomic_dwcas_state
    *atds;

  LFDS710_PAL_ASSERT( list_of_logical_processors != NULL );
  LFDS710_PAL_ASSERT( ms != NULL );
  LFDS710_PAL_ASSERT( dvs != NULL );

  /* TRD : here we test pal_dwcas

           we run one thread per CPU
           we use pal_dwcas() to increment a shared counter
           every time a thread successfully increments the counter,
           it increments a thread local counter
           the threads run for ten seconds
           after the threads finish, we total the local counters
           they should equal the shared counter
  */

  lfds710_list_asu_query( list_of_logical_processors, LFDS710_LIST_ASU_QUERY_GET_POTENTIALLY_INACCURATE_COUNT, NULL, (void **) &number_logical_processors );
  pts = libshared_memory_alloc_from_unknown_node( ms, sizeof(struct libtest_threadset_per_thread_state) * number_logical_processors, LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
  atds = libshared_memory_alloc_from_unknown_node( ms, sizeof(struct libtest_tests_pal_atomic_dwcas_state) * number_logical_processors, LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );

  for( loop = 0 ; loop < number_logical_processors ; loop++ )
  {
    (atds+loop)->shared_counter = &shared_counter;
    (atds+loop)->local_counter = 0;
  }

  libtest_threadset_init( &ts, NULL );

  loop = 0;

  while( LFDS710_LIST_ASU_GET_START_AND_THEN_NEXT(*list_of_logical_processors, lasue) )
  {
    lp = LFDS710_LIST_ASU_GET_VALUE_FROM_ELEMENT( *lasue );
    libtest_threadset_add_thread( &ts, &pts[loop], lp, thread_dwcas, atds+loop );
    loop++;
  }

  LFDS710_MISC_BARRIER_STORE;

  lfds710_misc_force_store();

  // TRD : run the test
  libtest_threadset_run( &ts );

  libtest_threadset_cleanup( &ts );

  // TRD : results
  LFDS710_MISC_BARRIER_LOAD;

  for( loop = 0 ; loop < number_logical_processors ; loop++ )
    local_total += (atds+loop)->local_counter;

  if( local_total == shared_counter[0] )
    *dvs = LFDS710_MISC_VALIDITY_VALID;

  if( local_total != shared_counter[0] )
    *dvs = LFDS710_MISC_VALIDITY_INVALID_TEST_DATA;

  return;
}

#pragma warning( disable : 4702 )





/****************************************************************************/
static libshared_pal_thread_return_t LIBSHARED_PAL_THREAD_CALLING_CONVENTION thread_dwcas( void *libtest_threadset_per_thread_state )
{
  char unsigned
    result;

  lfds710_pal_uint_t
    loop = 0;

  lfds710_pal_uint_t LFDS710_PAL_ALIGN(LFDS710_PAL_ALIGN_DOUBLE_POINTER)
    exchange[2],
    compare[2];

  struct libtest_tests_pal_atomic_dwcas_state
    *atds;

  struct libtest_threadset_per_thread_state
    *pts;

  LFDS710_PAL_ASSERT( libtest_threadset_per_thread_state != NULL );

  pts = (struct libtest_threadset_per_thread_state *) libtest_threadset_per_thread_state;
  atds = LIBTEST_THREADSET_GET_USER_STATE_FROM_PER_THREAD_STATE( *pts );

  LFDS710_MISC_BARRIER_LOAD;

  libtest_threadset_thread_ready_and_wait( pts );

  while( loop++ < 10000000 )
  {
    compare[0] = (*atds->shared_counter)[0];
    compare[1] = (*atds->shared_counter)[1];

    do
    {
      exchange[0] = compare[0] + 1;
      exchange[1] = compare[1];
      LFDS710_PAL_ATOMIC_DWCAS( (*atds->shared_counter), compare, exchange, LFDS710_MISC_CAS_STRENGTH_WEAK, result );
    }
    while( result == 0 );

    atds->local_counter++;
  }

  LFDS710_MISC_BARRIER_STORE;

  lfds710_misc_force_store();

  return LIBSHARED_PAL_THREAD_RETURN_CAST(RETURN_SUCCESS);
}

