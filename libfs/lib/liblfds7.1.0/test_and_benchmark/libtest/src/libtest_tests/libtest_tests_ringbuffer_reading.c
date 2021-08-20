/***** includes *****/
#include "libtest_tests_internal.h"

/***** structs *****/
struct test_per_thread_state
{
  enum flag
    error_flag;

  lfds710_pal_uint_t
    read_count;

  struct lfds710_ringbuffer_state
    *rs;
};

/***** private prototypes *****/
static libshared_pal_thread_return_t LIBSHARED_PAL_THREAD_CALLING_CONVENTION thread_simple_reader( void *libtest_threadset_per_thread_state );





/****************************************************************************/
void libtest_tests_ringbuffer_reading( struct lfds710_list_asu_state *list_of_logical_processors, struct libshared_memory_state *ms, enum lfds710_misc_validity *dvs )
{
  enum lfds710_misc_validity
    local_dvs[2] = { LFDS710_MISC_VALIDITY_VALID, LFDS710_MISC_VALIDITY_VALID };

  lfds710_pal_uint_t
    loop,
    number_elements,
    number_logical_processors,
    total_read = 0;

  struct lfds710_list_asu_element
    *lasue = NULL;

  struct lfds710_ringbuffer_element
    *re_array;

  struct lfds710_ringbuffer_state
    rs;

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

  /* TRD : we create a single ringbuffer
           with 1,000,000 elements
           we populate the ringbuffer, where the
           user data is an incrementing counter

           we create one thread per CPU
           where each thread busy-works,
           reading until the ringbuffer is empty

           each thread keep track of the number of reads it manages
           and that each user data it reads is greater than the
           previous user data that was read
  */

  *dvs = LFDS710_MISC_VALIDITY_VALID;

  lfds710_list_asu_query( list_of_logical_processors, LFDS710_LIST_ASU_QUERY_GET_POTENTIALLY_INACCURATE_COUNT, NULL, (void **) &number_logical_processors );

  // TRD : allocate
  tpts = libshared_memory_alloc_from_unknown_node( ms, sizeof(struct test_per_thread_state) * number_logical_processors, LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
  pts = libshared_memory_alloc_from_unknown_node( ms, sizeof(struct libtest_threadset_per_thread_state) * number_logical_processors, LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
  re_array = libshared_memory_alloc_largest_possible_array_from_unknown_node( ms, sizeof(struct lfds710_ringbuffer_element), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES, &number_elements );

  vi.min_elements = 0;
  vi.max_elements = number_elements - 1;

  lfds710_ringbuffer_init_valid_on_current_logical_core( &rs, re_array, number_elements, NULL );

  // TRD : init the ringbuffer contents for the test
  for( loop = 1 ; loop < number_elements ; loop++ )
    lfds710_ringbuffer_write( &rs, NULL, (void *) (lfds710_pal_uint_t) loop, NULL, NULL, NULL );

  libtest_threadset_init( &ts, NULL );

  loop = 0;

  while( LFDS710_LIST_ASU_GET_START_AND_THEN_NEXT(*list_of_logical_processors,lasue) )
  {
    lp = LFDS710_LIST_ASU_GET_VALUE_FROM_ELEMENT( *lasue );
    (tpts+loop)->rs = &rs;
    (tpts+loop)->read_count = 0;
    (tpts+loop)->error_flag = LOWERED;

    libtest_threadset_add_thread( &ts, &pts[loop], lp, thread_simple_reader, &tpts[loop] );

    loop++;
  }

  // TRD : run the test
  libtest_threadset_run( &ts );

  libtest_threadset_cleanup( &ts );

  // TRD : validate
  LFDS710_MISC_BARRIER_LOAD;

  lfds710_ringbuffer_query( &rs, LFDS710_RINGBUFFER_QUERY_SINGLETHREADED_VALIDATE, (void *) &vi, (void *) local_dvs );

  if( local_dvs[0] != LFDS710_MISC_VALIDITY_VALID )
    *dvs = local_dvs[0];

  if( local_dvs[1] != LFDS710_MISC_VALIDITY_VALID )
    *dvs = local_dvs[1];

  if( *dvs == LFDS710_MISC_VALIDITY_VALID )
  {
    // TRD : check for raised error flags
    for( loop = 0 ; loop < number_logical_processors ; loop++ )
      if( (tpts+loop)->error_flag == RAISED )
        *dvs = LFDS710_MISC_VALIDITY_INVALID_TEST_DATA;

    // TRD : check thread reads total to 1,000,000
    for( loop = 0 ; loop < number_logical_processors ; loop++ )
      total_read += (tpts+loop)->read_count;

    if( total_read < number_elements - 1 )
      *dvs = LFDS710_MISC_VALIDITY_INVALID_MISSING_ELEMENTS;

    if( total_read > number_elements - 1 )
      *dvs = LFDS710_MISC_VALIDITY_INVALID_ADDITIONAL_ELEMENTS;
  }

  lfds710_ringbuffer_cleanup( &rs, NULL );

  return;
}





/****************************************************************************/
static libshared_pal_thread_return_t LIBSHARED_PAL_THREAD_CALLING_CONVENTION thread_simple_reader( void *libtest_threadset_per_thread_state )
{
  lfds710_pal_uint_t
    *prev_value,
    *value;

  struct test_per_thread_state
    *tpts;

  struct libtest_threadset_per_thread_state
    *pts;

  LFDS710_MISC_MAKE_VALID_ON_CURRENT_LOGICAL_CORE_INITS_COMPLETED_BEFORE_NOW_ON_ANY_OTHER_LOGICAL_CORE;

  LFDS710_PAL_ASSERT( libtest_threadset_per_thread_state != NULL );

  pts = (struct libtest_threadset_per_thread_state *) libtest_threadset_per_thread_state;
  tpts = LIBTEST_THREADSET_GET_USER_STATE_FROM_PER_THREAD_STATE( *pts );

  lfds710_ringbuffer_read( tpts->rs, NULL, (void **) &prev_value );
  tpts->read_count++;

  libtest_threadset_thread_ready_and_wait( pts );

  while( lfds710_ringbuffer_read(tpts->rs, NULL, (void **) &value) )
  {
    if( value <= prev_value )
      tpts->error_flag = RAISED;

    prev_value = value;

    tpts->read_count++;
  }

  LFDS710_MISC_BARRIER_STORE;

  lfds710_misc_force_store();

  return LIBSHARED_PAL_THREAD_RETURN_CAST(RETURN_SUCCESS);
}

