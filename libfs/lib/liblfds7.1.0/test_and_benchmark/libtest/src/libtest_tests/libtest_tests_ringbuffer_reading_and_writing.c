/***** includes *****/
#include "libtest_tests_internal.h"

/***** structs *****/
struct test_per_thread_state
{
  enum flag
    error_flag;

  lfds710_pal_uint_t
    counter,
    number_logical_processors,
    *per_thread_counters,
    thread_number;

  struct lfds710_ringbuffer_state
    *rs;
};

struct test_element
{
  lfds710_pal_uint_t
    datum,
    thread_number;
};

/***** private prototypes *****/
static libshared_pal_thread_return_t LIBSHARED_PAL_THREAD_CALLING_CONVENTION thread_reader_writer( void *libtest_threadset_per_thread_state );





/****************************************************************************/
void libtest_tests_ringbuffer_reading_and_writing( struct lfds710_list_asu_state *list_of_logical_processors, struct libshared_memory_state *ms, enum lfds710_misc_validity *dvs )
{
  enum lfds710_misc_validity
    local_dvs[2] = { LFDS710_MISC_VALIDITY_VALID, LFDS710_MISC_VALIDITY_VALID };

  lfds710_pal_uint_t
    *counters,
    loop,
    number_elements,
    number_logical_processors,
    subloop;

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

  struct test_element
    *te_array;

  struct test_per_thread_state
    *tpts;

  LFDS710_PAL_ASSERT( list_of_logical_processors != NULL );
  LFDS710_PAL_ASSERT( ms != NULL );
  LFDS710_PAL_ASSERT( dvs != NULL );

  /* TRD : we create a single ringbuffer
           with 100,000 elements
           the ringbuffers starts empty

           we create one thread per CPU
           where each thread busy-works writing
           and then immediately reading
           for ten seconds

           the user data in each written element is a combination
           of the thread number and the counter

           while a thread runs, it keeps track of the
           counters for the other threads and throws an error
           if it sees the number stay the same or decrease
  */

  *dvs = LFDS710_MISC_VALIDITY_VALID;

  lfds710_list_asu_query( list_of_logical_processors, LFDS710_LIST_ASU_QUERY_GET_POTENTIALLY_INACCURATE_COUNT, NULL, (void **) &number_logical_processors );

  // TRD : allocate
  tpts = libshared_memory_alloc_from_unknown_node( ms, sizeof(struct test_per_thread_state) * number_logical_processors, LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
  pts = libshared_memory_alloc_from_unknown_node( ms, sizeof(struct libtest_threadset_per_thread_state) * number_logical_processors, LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
  counters = libshared_memory_alloc_from_unknown_node( ms, sizeof(lfds710_pal_uint_t) * number_logical_processors * number_logical_processors, LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
  re_array = libshared_memory_alloc_largest_possible_array_from_unknown_node( ms, sizeof(struct test_element) + sizeof(struct lfds710_ringbuffer_element), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES, &number_elements );
  te_array = (struct test_element *) ( re_array + number_elements );

  vi.min_elements = 0;
  vi.max_elements = number_elements;

  lfds710_ringbuffer_init_valid_on_current_logical_core( &rs, re_array, number_elements, NULL );

  // TRD : populate the ringbuffer
  for( loop = 1 ; loop < number_elements ; loop++ )
  {
    te_array[loop].thread_number = 0;
    te_array[loop].datum = (lfds710_pal_uint_t) -1 ;
    lfds710_ringbuffer_write( &rs, NULL, &te_array[loop], NULL, NULL, NULL );
  }

  libtest_threadset_init( &ts, NULL );

  loop = 0;

  while( LFDS710_LIST_ASU_GET_START_AND_THEN_NEXT(*list_of_logical_processors,lasue) )
  {
    lp = LFDS710_LIST_ASU_GET_VALUE_FROM_ELEMENT( *lasue );

    (tpts+loop)->rs = &rs;
    (tpts+loop)->thread_number = loop;
    (tpts+loop)->counter = 0;
    (tpts+loop)->number_logical_processors = number_logical_processors;
    (tpts+loop)->error_flag = LOWERED;
    (tpts+loop)->per_thread_counters = counters + loop * number_logical_processors;

    for( subloop = 0 ; subloop < number_logical_processors ; subloop++ )
      *((tpts+loop)->per_thread_counters+subloop) = 0;

    libtest_threadset_add_thread( &ts, &pts[loop], lp, thread_reader_writer, &tpts[loop] );

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
    for( loop = 0 ; loop < number_logical_processors ; loop++ )
      if( (tpts+loop)->error_flag == RAISED )
        *dvs = LFDS710_MISC_VALIDITY_INVALID_TEST_DATA;

  lfds710_ringbuffer_cleanup( &rs, NULL );

  return;
}





/****************************************************************************/
static libshared_pal_thread_return_t LIBSHARED_PAL_THREAD_CALLING_CONVENTION thread_reader_writer( void *libtest_threadset_per_thread_state )
{
  lfds710_pal_uint_t
    time_loop = 0;

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
    lfds710_ringbuffer_read( tpts->rs, NULL, (void **) &te );

    if( te->thread_number >= tpts->number_logical_processors )
      tpts->error_flag = RAISED;
    else
    {
      if( te->datum < tpts->per_thread_counters[te->thread_number] )
        tpts->error_flag = RAISED;

      if( te->datum >= tpts->per_thread_counters[te->thread_number] )
        tpts->per_thread_counters[te->thread_number] = te->datum+1;
    }

    te->thread_number = tpts->thread_number;
    te->datum = tpts->counter++;

    lfds710_ringbuffer_write( tpts->rs, NULL, te, NULL, NULL, NULL );

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

