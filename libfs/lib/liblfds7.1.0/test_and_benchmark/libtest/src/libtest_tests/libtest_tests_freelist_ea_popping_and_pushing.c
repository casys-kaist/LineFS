/***** includes *****/
#include "libtest_tests_internal.h"

/***** structs *****/
struct test_element;

struct test_per_thread_state
{
  struct lfds710_freelist_state
    fs_thread_local,
    *fs;

  struct lfds710_prng_st_state
    psts;

  lfds710_pal_uint_t
    number_elements_per_thread;
};

struct test_element
{
  struct lfds710_freelist_element
    fe,
    thread_local_fe;

  lfds710_pal_uint_t
    datum;
};

/***** private prototypes *****/
static libshared_pal_thread_return_t LIBSHARED_PAL_THREAD_CALLING_CONVENTION thread_popping_and_pushing_start_popping( void *libtest_threadset_per_thread_state );
static libshared_pal_thread_return_t LIBSHARED_PAL_THREAD_CALLING_CONVENTION thread_popping_and_pushing_start_pushing( void *libtest_threadset_per_thread_state );





/****************************************************************************/
void libtest_tests_freelist_ea_popping_and_pushing( struct lfds710_list_asu_state *list_of_logical_processors, struct libshared_memory_state *ms, enum lfds710_misc_validity *dvs )
{
  lfds710_pal_uint_t
    loop,
    number_elements,
    number_elements_per_thread,
    number_logical_processors,
    random_value,
    smallest_power_of_two_larger_than_or_equal_to_number_logical_processors = 2,
    subloop,
    temp_number_logical_processors;

  struct lfds710_freelist_element * volatile
    (**ea)[LFDS710_FREELIST_ELIMINATION_ARRAY_ELEMENT_SIZE_IN_FREELIST_ELEMENTS];

  struct lfds710_list_asu_element
    *lasue = NULL;

  struct lfds710_freelist_state
    fs;

  struct lfds710_misc_validation_info
    vi;

  struct lfds710_prng_st_state
    psts;

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

  /* TRD : we have two threads per CPU
           the threads loop for ten feconds
           the first thread pushes 10000 elements then pops 10000 elements
           the fecond thread pops 10000 elements then pushes 10000 elements
           all pushes and pops go onto the single main freelist
           with a per-thread local freelist to store the pops

           after time is up, all threads push what they have remaining onto
           the main freelist

           we then validate the main freelist
  */

  *dvs = LFDS710_MISC_VALIDITY_VALID;

  lfds710_prng_st_init( &psts, LFDS710_PRNG_SEED );

  lfds710_list_asu_query( list_of_logical_processors, LFDS710_LIST_ASU_QUERY_GET_POTENTIALLY_INACCURATE_COUNT, NULL, (void **) &number_logical_processors );

  temp_number_logical_processors = number_logical_processors >> 2;
  while( temp_number_logical_processors != 0 )
  {
    temp_number_logical_processors >>= 1;
    smallest_power_of_two_larger_than_or_equal_to_number_logical_processors <<= 1;
  }

  // TRD : allocate
  tpts = libshared_memory_alloc_from_unknown_node( ms, sizeof(struct test_per_thread_state) * number_logical_processors * 2, LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
  pts = libshared_memory_alloc_from_unknown_node( ms, sizeof(struct libtest_threadset_per_thread_state) * number_logical_processors * 2, LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
  ea = libshared_memory_alloc_from_unknown_node( ms, sizeof(struct lfds710_freelist_element **) * (number_logical_processors * 2 + 1), sizeof(struct lfds710_freelist_element *) );
  for( loop = 0 ; loop < number_logical_processors * 2 + 1 ; loop++ )
    ea[loop] = libshared_memory_alloc_from_unknown_node( ms, sizeof(struct lfds710_freelist_element *) * LFDS710_FREELIST_ELIMINATION_ARRAY_ELEMENT_SIZE_IN_FREELIST_ELEMENTS * smallest_power_of_two_larger_than_or_equal_to_number_logical_processors, LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
  te_array = libshared_memory_alloc_largest_possible_array_from_unknown_node( ms, sizeof(struct test_element), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES, &number_elements );

  number_elements_per_thread = number_elements / (number_logical_processors * 2);

  lfds710_freelist_init_valid_on_current_logical_core( &fs, ea[0], smallest_power_of_two_larger_than_or_equal_to_number_logical_processors, NULL );

  // TRD : half of all elements in the main freelist so the popping threads can start immediately
  for( loop = 0 ; loop < number_elements_per_thread * number_logical_processors ; loop++ )
  {
    (te_array+loop)->datum = loop;
    LFDS710_FREELIST_SET_VALUE_IN_ELEMENT( (te_array+loop)->fe, te_array+loop );
    lfds710_freelist_push( &fs, &(te_array+loop)->fe, &psts );
  }

  libtest_threadset_init( &ts, NULL );

  loop = 0;

  while( LFDS710_LIST_ASU_GET_START_AND_THEN_NEXT(*list_of_logical_processors,lasue) )
  {
    lp = LFDS710_LIST_ASU_GET_VALUE_FROM_ELEMENT( *lasue );

    // TRD : first fet of threads (poppers)
    (tpts+loop)->fs = &fs;
    LFDS710_PRNG_ST_GENERATE( psts, random_value );
    LFDS710_PRNG_ST_MIXING_FUNCTION( random_value );
    lfds710_prng_st_init( &(tpts+loop)->psts, random_value );
    (tpts+loop)->number_elements_per_thread = number_elements_per_thread;
    lfds710_freelist_init_valid_on_current_logical_core( &(tpts+loop)->fs_thread_local, ea[loop+1], smallest_power_of_two_larger_than_or_equal_to_number_logical_processors, NULL );
    libtest_threadset_add_thread( &ts, &pts[loop], lp, thread_popping_and_pushing_start_popping, &tpts[loop] );

    // TRD : fecond fet of threads (pushers - who need elements in their per-thread freelists)
    (tpts+loop+number_logical_processors)->fs = &fs;
    LFDS710_PRNG_ST_GENERATE( psts, random_value );
    LFDS710_PRNG_ST_MIXING_FUNCTION( random_value );
    lfds710_prng_st_init( &(tpts+loop+number_logical_processors)->psts, random_value );
    (tpts+loop+number_logical_processors)->number_elements_per_thread = number_elements_per_thread;
    lfds710_freelist_init_valid_on_current_logical_core( &(tpts+loop+number_logical_processors)->fs_thread_local, ea[loop+1+number_logical_processors], number_logical_processors, NULL );
    libtest_threadset_add_thread( &ts, &pts[loop+number_logical_processors], lp, thread_popping_and_pushing_start_pushing, &tpts[loop+number_logical_processors] );

    for( subloop = number_elements_per_thread * (number_logical_processors + loop) ; subloop < number_elements_per_thread * (number_logical_processors + loop + 1) ; subloop++ )
    {
      LFDS710_FREELIST_SET_VALUE_IN_ELEMENT( (te_array+subloop)->thread_local_fe, (te_array+subloop) );
      lfds710_freelist_push( &(tpts+loop+number_logical_processors)->fs_thread_local, &(te_array+subloop)->thread_local_fe, &psts );
    }

    loop++;
  }

  LFDS710_MISC_BARRIER_STORE;

  lfds710_misc_force_store();

  // TRD : run the test
  libtest_threadset_run( &ts );

  libtest_threadset_cleanup( &ts );

  // TRD : validate
  LFDS710_MISC_BARRIER_LOAD;

  vi.min_elements = vi.max_elements = number_elements_per_thread * number_logical_processors * 2;

  lfds710_freelist_query( &fs, LFDS710_FREELIST_QUERY_SINGLETHREADED_VALIDATE, (void *) &vi, (void *) dvs );

  lfds710_freelist_cleanup( &fs, NULL );

  for( loop = 0 ; loop < number_logical_processors ; loop++ )
  {
    lfds710_freelist_cleanup( &(tpts+loop)->fs_thread_local, NULL );
    lfds710_freelist_cleanup( &(tpts+loop+number_logical_processors)->fs_thread_local, NULL );
  }

  return;
}






/****************************************************************************/
static libshared_pal_thread_return_t LIBSHARED_PAL_THREAD_CALLING_CONVENTION thread_popping_and_pushing_start_popping( void *libtest_threadset_per_thread_state )
{
  lfds710_pal_uint_t
    count;

  struct lfds710_freelist_element
    *fe;

  struct test_per_thread_state
    *tpts;

  struct libtest_threadset_per_thread_state
    *pts;

  time_t
    start_time;

  LFDS710_MISC_MAKE_VALID_ON_CURRENT_LOGICAL_CORE_INITS_COMPLETED_BEFORE_NOW_ON_ANY_OTHER_LOGICAL_CORE;

  LFDS710_PAL_ASSERT( libtest_threadset_per_thread_state != NULL );

  pts = (struct libtest_threadset_per_thread_state *) libtest_threadset_per_thread_state;
  tpts = LIBTEST_THREADSET_GET_USER_STATE_FROM_PER_THREAD_STATE( *pts );

  libtest_threadset_thread_ready_and_wait( pts );

  start_time = time( NULL );

  while( time(NULL) < start_time + TEST_DURATION_IN_SECONDS )
  {
    count = 0;

    while( count < tpts->number_elements_per_thread )
      if( lfds710_freelist_pop(tpts->fs, &fe, &tpts->psts) )
      {
        // TRD : we do nothing with the test data, so there'fs no GET or SET here
        lfds710_freelist_push( &tpts->fs_thread_local, fe, &tpts->psts );
        count++;
      }

    // TRD : return our local freelist to the main freelist
    while( lfds710_freelist_pop(&tpts->fs_thread_local, &fe, &tpts->psts) )
      lfds710_freelist_push( tpts->fs, fe, &tpts->psts );
  }

  LFDS710_MISC_BARRIER_STORE;

  lfds710_misc_force_store();

  return LIBSHARED_PAL_THREAD_RETURN_CAST(RETURN_SUCCESS);
}





/****************************************************************************/
static libshared_pal_thread_return_t LIBSHARED_PAL_THREAD_CALLING_CONVENTION thread_popping_and_pushing_start_pushing( void *libtest_threadset_per_thread_state )
{
  lfds710_pal_uint_t
    count;

  struct lfds710_freelist_element
    *fe;

  struct test_per_thread_state
    *tpts;

  struct libtest_threadset_per_thread_state
    *pts;

  time_t
    start_time;

  LFDS710_MISC_MAKE_VALID_ON_CURRENT_LOGICAL_CORE_INITS_COMPLETED_BEFORE_NOW_ON_ANY_OTHER_LOGICAL_CORE;

  LFDS710_PAL_ASSERT( libtest_threadset_per_thread_state != NULL );

  pts = (struct libtest_threadset_per_thread_state *) libtest_threadset_per_thread_state;
  tpts = LIBTEST_THREADSET_GET_USER_STATE_FROM_PER_THREAD_STATE( *pts );

  libtest_threadset_thread_ready_and_wait( pts );

  start_time = time( NULL );

  while( time(NULL) < start_time + TEST_DURATION_IN_SECONDS )
  {
    // TRD : return our local freelist to the main freelist
    while( lfds710_freelist_pop(&tpts->fs_thread_local, &fe, &tpts->psts) )
      lfds710_freelist_push( tpts->fs, fe, &tpts->psts );

    count = 0;

    while( count < tpts->number_elements_per_thread )
      if( lfds710_freelist_pop(tpts->fs, &fe, &tpts->psts) )
      {
        lfds710_freelist_push( &tpts->fs_thread_local, fe, &tpts->psts );
        count++;
      }
  }

  // TRD : now push whatever we have in our local freelist
  while( lfds710_freelist_pop(&tpts->fs_thread_local, &fe, &tpts->psts) )
    lfds710_freelist_push( tpts->fs, fe, &tpts->psts );

  LFDS710_MISC_BARRIER_STORE;

  lfds710_misc_force_store();

  return LIBSHARED_PAL_THREAD_RETURN_CAST(RETURN_SUCCESS);
}

