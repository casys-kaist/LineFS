/***** includes *****/
#include "libtest_tests_internal.h"

/***** structs *****/
struct test_element
{
  lfds710_pal_uint_t
    thread_number,
    datum;
};

struct test_per_thread_state
{
  lfds710_pal_uint_t
    thread_number,
    write_count;

  struct test_element
    te;

  struct lfds710_ringbuffer_state
    *rs;
};

/***** private prototypes *****/
static libshared_pal_thread_return_t LIBSHARED_PAL_THREAD_CALLING_CONVENTION thread_simple_writer( void *libtest_threadset_per_thread_state );





/****************************************************************************/
void libtest_tests_ringbuffer_writing( struct lfds710_list_asu_state *list_of_logical_processors, struct libshared_memory_state *ms, enum lfds710_misc_validity *dvs )
{
  enum lfds710_misc_validity
    local_dvs[2] = { LFDS710_MISC_VALIDITY_VALID, LFDS710_MISC_VALIDITY_VALID };

  lfds710_pal_uint_t
    loop,
    number_elements,
    number_logical_processors,
    *per_thread_counters;

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
    *te,
    *te_array;

  struct test_per_thread_state
    *tpts;

  LFDS710_PAL_ASSERT( list_of_logical_processors != NULL );
  LFDS710_PAL_ASSERT( ms != NULL );
  LFDS710_PAL_ASSERT( dvs != NULL );

  /* TRD : we create a single ringbuffer
           with n elements
           we create n test elements
           which are thread_number/counter pairs
           init them to safe values
           and fully populate the ringbuffer

           we create one thread per CPU
           where each thread busy-works writing
           for ten seconds; each thread has one extra element
           which it uses for the first write and after that
           it uses the element it picks up from overwriting

           the user data in each written element is a combination
           of the thread number and the counter

           after the threads are complete, we validate by
           checking the user data counters increment on a per thread
           basis
  */

  *dvs = LFDS710_MISC_VALIDITY_VALID;

  lfds710_list_asu_query( list_of_logical_processors, LFDS710_LIST_ASU_QUERY_GET_POTENTIALLY_INACCURATE_COUNT, NULL, (void **) &number_logical_processors );

  // TRD : allocate
  tpts = libshared_memory_alloc_from_unknown_node( ms, sizeof(struct test_per_thread_state) * number_logical_processors, LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
  pts = libshared_memory_alloc_from_unknown_node( ms, sizeof(struct libtest_threadset_per_thread_state) * number_logical_processors, LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
  per_thread_counters = libshared_memory_alloc_from_unknown_node( ms, sizeof(lfds710_pal_uint_t) * number_logical_processors, LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
  re_array = libshared_memory_alloc_largest_possible_array_from_unknown_node( ms, sizeof(struct test_element) + sizeof(struct lfds710_ringbuffer_element), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES, &number_elements );
  te_array = (struct test_element *) ( re_array + number_elements );

  vi.min_elements = number_elements;
  vi.max_elements = number_elements;

  lfds710_ringbuffer_init_valid_on_current_logical_core( &rs, re_array, number_elements, NULL );

  // TRD : init the test elements and write them into the ringbuffer
  for( loop = 1 ; loop < number_elements ; loop++ )
  {
    te_array[loop].thread_number = 0;
    te_array[loop].datum = 0;
    lfds710_ringbuffer_write( &rs, NULL, &te_array[loop], NULL, NULL, NULL );
  }

  libtest_threadset_init( &ts, NULL );

  loop = 0;

  while( LFDS710_LIST_ASU_GET_START_AND_THEN_NEXT(*list_of_logical_processors,lasue) )
  {
    lp = LFDS710_LIST_ASU_GET_VALUE_FROM_ELEMENT( *lasue );
    (tpts+loop)->rs = &rs;
    (tpts+loop)->thread_number = loop;
    (tpts+loop)->write_count = 0;

    libtest_threadset_add_thread( &ts, &pts[loop], lp, thread_simple_writer, &tpts[loop] );

    loop++;
  }

  // TRD : run the test
  libtest_threadset_run( &ts );

  libtest_threadset_cleanup( &ts );

  // TRD : validate
  LFDS710_MISC_BARRIER_LOAD;

  // TRD : now check results
  for( loop = 0 ; loop < number_logical_processors ; loop++ )
    *(per_thread_counters+loop) = 0;

  lfds710_ringbuffer_query( &rs, LFDS710_RINGBUFFER_QUERY_SINGLETHREADED_VALIDATE, &vi, (void *) local_dvs );

  if( local_dvs[0] != LFDS710_MISC_VALIDITY_VALID )
    *dvs = local_dvs[0];

  if( local_dvs[1] != LFDS710_MISC_VALIDITY_VALID )
    *dvs = local_dvs[1];

  while( *dvs == LFDS710_MISC_VALIDITY_VALID and lfds710_ringbuffer_read(&rs, NULL, (void **) &te) )
  {
    if( te->thread_number >= number_logical_processors )
    {
      *dvs = LFDS710_MISC_VALIDITY_INVALID_TEST_DATA;
      break;
    }

    if( per_thread_counters[te->thread_number] == 0 )
      per_thread_counters[te->thread_number] = te->datum;

    if( te->datum < per_thread_counters[te->thread_number] )
      *dvs = LFDS710_MISC_VALIDITY_INVALID_ADDITIONAL_ELEMENTS;

    if( te->datum >= per_thread_counters[te->thread_number] )
      per_thread_counters[te->thread_number] = te->datum+1;
  }

  lfds710_ringbuffer_cleanup( &rs, NULL );

  return;
}





/****************************************************************************/
static libshared_pal_thread_return_t LIBSHARED_PAL_THREAD_CALLING_CONVENTION thread_simple_writer( void *libtest_threadset_per_thread_state )
{
  enum lfds710_misc_flag
    overwrite_occurred_flag;

  lfds710_pal_uint_t
    time_loop = 0;

  struct libtest_threadset_per_thread_state
    *pts;

  struct test_element
    *te;

  struct test_per_thread_state
    *tpts;

  time_t
    current_time,
    start_time;

  LFDS710_MISC_MAKE_VALID_ON_CURRENT_LOGICAL_CORE_INITS_COMPLETED_BEFORE_NOW_ON_ANY_OTHER_LOGICAL_CORE;

  LFDS710_PAL_ASSERT( libtest_threadset_per_thread_state != NULL );

  pts = (struct libtest_threadset_per_thread_state *) libtest_threadset_per_thread_state;
  tpts = LIBTEST_THREADSET_GET_USER_STATE_FROM_PER_THREAD_STATE( *pts );

  tpts->te.thread_number = 0;
  tpts->te.datum = 0;

  lfds710_ringbuffer_write( tpts->rs, NULL, &tpts->te, &overwrite_occurred_flag, NULL, (void **) &te );

  libtest_threadset_thread_ready_and_wait( pts );

  current_time = start_time = time( NULL );

  while( current_time < start_time + TEST_DURATION_IN_SECONDS )
  {
    te->thread_number = tpts->thread_number;
    te->datum = tpts->write_count++;

    lfds710_ringbuffer_write( tpts->rs, NULL, te, &overwrite_occurred_flag, NULL, (void **) &te );

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

