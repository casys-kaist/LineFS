/***** includes *****/
#include "libtest_tests_internal.h"

/***** structs *****/
struct test_per_thread_state
{
  struct lfds710_prng_state
    *ps;

  lfds710_pal_uint_t
    read_index,
    *output_array;
};

/***** private prototyps *****/
static libshared_pal_thread_return_t LIBSHARED_PAL_THREAD_CALLING_CONVENTION thread_generate( void *libtest_threadset_per_thread_state );





/****************************************************************************/
void libtest_tests_prng_generate( struct lfds710_list_asu_state *list_of_logical_processors, struct libshared_memory_state *ms, enum lfds710_misc_validity *dvs )
{
  enum flag
    duplicate_flag = LOWERED,
    finished_flag = LOWERED;

  lfds710_pal_uint_t
    *output_arrays,
    index = 0,
    loop = 0,
    mean = 0,
    *merged_output_arrays,
    merged_write_index = 0,
    number_logical_processors,
    smallest_prng_value,
    ten_percent,
    thread_to_bump = 0;

  struct lfds710_list_asu_element
    *lasue = NULL;

  struct libtest_logical_processor
    *lp;

  struct lfds710_prng_state
    ps;

  struct libtest_threadset_per_thread_state
    *pts;

  struct libtest_threadset_state
    ts;

  struct test_per_thread_state
    *tpts;

  LFDS710_PAL_ASSERT( list_of_logical_processors != NULL );
  LFDS710_PAL_ASSERT( ms != NULL );
  LFDS710_PAL_ASSERT( dvs != NULL );

  /* TRD : here we test the atomic PRNG
           we create an array, an output buffer, of 128 elements per thread
           we have a single, global PRNG
           we start all the threads and let them run for test duration seconds
           (to ensure they are all running together)
           each thread loops, writing new numbers to its output array
           obviously in test duration seconds it will write many more than 128 elements - 
           it just loops over the output array

           then when we're done we merge sort the output arrays (no qsort, not using standard library)
           the number of duplicates should be 0
           and the standard deviation should be 25% of LFDS710_PRNG_MAX
  */

  *dvs = LFDS710_MISC_VALIDITY_VALID;

  // TRD : allocate
  lfds710_list_asu_query( list_of_logical_processors, LFDS710_LIST_ASU_QUERY_GET_POTENTIALLY_INACCURATE_COUNT, NULL, (void **) &number_logical_processors );
  tpts = libshared_memory_alloc_from_unknown_node( ms, sizeof(struct test_per_thread_state) * number_logical_processors, LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
  pts = libshared_memory_alloc_from_unknown_node( ms, sizeof(struct libtest_threadset_per_thread_state) * number_logical_processors, LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
  output_arrays = libshared_memory_alloc_from_unknown_node( ms, sizeof(lfds710_pal_uint_t) * 128 * number_logical_processors, LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
  merged_output_arrays = libshared_memory_alloc_from_unknown_node( ms, sizeof(lfds710_pal_uint_t) * 128 * number_logical_processors, LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );

  lfds710_prng_init_valid_on_current_logical_core( &ps, LFDS710_PRNG_SEED );

  libtest_threadset_init( &ts, NULL );

  while( LFDS710_LIST_ASU_GET_START_AND_THEN_NEXT(*list_of_logical_processors,lasue) )
  {
    lp = LFDS710_LIST_ASU_GET_VALUE_FROM_ELEMENT( *lasue );

    (tpts+loop)->ps = &ps;
    (tpts+loop)->output_array = output_arrays + (loop * 128);
    (tpts+loop)->read_index = 0; // TRD : convenient to alloc here, as we need one per thread, used in validation

    libtest_threadset_add_thread( &ts, &pts[loop], lp, thread_generate, &tpts[loop] );

    loop++;
  }

  LFDS710_MISC_BARRIER_STORE;

  lfds710_misc_force_store();

  // TRD : run the test
  libtest_threadset_run( &ts );
  libtest_threadset_cleanup( &ts );

  // TRD : validate
  LFDS710_MISC_BARRIER_LOAD;

  // TRD : merge sort the counter arrays into merged_output_array
  while( finished_flag == LOWERED )
  {
    smallest_prng_value = LFDS710_PRNG_MAX;
    finished_flag = RAISED;

    for( loop = 0 ; loop < number_logical_processors ; loop++ )
      if( tpts[loop].read_index < 128 and tpts[loop].output_array[ tpts[loop].read_index ] < smallest_prng_value )
      {
        smallest_prng_value = tpts[loop].output_array[ tpts[loop].read_index ];
        thread_to_bump = loop;
        finished_flag = LOWERED;
      }

    tpts[thread_to_bump].read_index++;
    merged_output_arrays[ merged_write_index++ ] = smallest_prng_value;
  }

  // TRD : now check for duplicates
  while( duplicate_flag == LOWERED and index < (128 * number_logical_processors) - 2 )
  {
    if( merged_output_arrays[index] == merged_output_arrays[index+1] )
      duplicate_flag = RAISED;
    index++;
  }

  if( duplicate_flag == RAISED )
    *dvs = LFDS710_MISC_VALIDITY_INVALID_TEST_DATA;

  // TRD : now for standard deviation (integer math only is allowed, and we can't sum the outputs because we'll overflow)
  for( loop = 0 ; loop < 128 * number_logical_processors ; loop++ )
    mean += merged_output_arrays[loop] / (128*number_logical_processors);

  /* TRD : the mean of an unsigned 64 bit is 9223372036854775808
           the mean of an unsigned 32 bit is 2147483648
           there are 128 random numbers per thread
           the more numbers there are, the more closely we should approach the expected mean
           it'd take me a while - if I could - to work out the expected deviation for a given number of numbers
           empirically, a single logical core (128 numbers) shouldn't be more than 10% off
  */

  ten_percent = LFDS710_PRNG_MAX / 10;

  if( mean < (LFDS710_PRNG_MAX / 2) - ten_percent or mean > (LFDS710_PRNG_MAX / 2) + ten_percent )
    *dvs = LFDS710_MISC_VALIDITY_INVALID_TEST_DATA;

  // TRD : should add a standard deviation check here

  return;
}





/****************************************************************************/
static libshared_pal_thread_return_t LIBSHARED_PAL_THREAD_CALLING_CONVENTION thread_generate( void *libtest_threadset_per_thread_state )
{
  lfds710_pal_uint_t
    index = 0,
    time_loop = 0;

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
    LFDS710_PRNG_GENERATE( *tpts->ps, tpts->output_array[index] );

    // TRD : 128 element array, so masking on 128-1 makes us loop, much faster than modulus
    index = ( (index+1) & 0x7F );

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

