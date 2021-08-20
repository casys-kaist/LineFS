/***** includes *****/
#include "libtest_tests_internal.h"

/***** structs *****/
struct test_per_thread_state
{
  struct lfds710_freelist_state
    *fs;

  struct lfds710_prng_st_state
    psts;
};

struct test_element
{
  struct lfds710_freelist_element
    fe;

  enum flag
    popped_flag;
};

/***** private prototypes *****/
static libshared_pal_thread_return_t LIBSHARED_PAL_THREAD_CALLING_CONVENTION thread_popping( void *libtest_threadset_per_thread_state );





/****************************************************************************/
void libtest_tests_freelist_ea_popping( struct lfds710_list_asu_state *list_of_logical_processors, struct libshared_memory_state *ms, enum lfds710_misc_validity *dvs )
{
  lfds710_pal_uint_t
    loop = 0,
    number_elements,
    number_elements_in_freelist,
    number_logical_processors,
    raised_count = 0,
    random_value,
    smallest_power_of_two_larger_than_or_equal_to_number_logical_processors = 2,
    temp_number_logical_processors;

  struct lfds710_freelist_element * volatile
    (*ea)[LFDS710_FREELIST_ELIMINATION_ARRAY_ELEMENT_SIZE_IN_FREELIST_ELEMENTS];

  struct lfds710_freelist_state
    fs;

  struct lfds710_list_asu_element
    *lasue = NULL;

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

  /* TRD : we create a freelist with as many elements as possible elements

           the creation function runs in a single thread and creates
           and pushes thofe elements onto the freelist

           each element contains a void pointer to the container test element

           we then run one thread per CPU
           where each thread loops, popping as quickly as possible
           each test element has a flag which indicates it has been popped

           the threads run till the source freelist is empty

           we then check the test elements
           every element should have been popped

           then tidy up
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
  tpts = libshared_memory_alloc_from_unknown_node( ms, sizeof(struct test_per_thread_state) * number_logical_processors, LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
  pts = libshared_memory_alloc_from_unknown_node( ms, sizeof(struct libtest_threadset_per_thread_state) * number_logical_processors, LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
  ea = libshared_memory_alloc_from_unknown_node( ms, sizeof(struct lfds710_freelist_element *) * LFDS710_FREELIST_ELIMINATION_ARRAY_ELEMENT_SIZE_IN_FREELIST_ELEMENTS * smallest_power_of_two_larger_than_or_equal_to_number_logical_processors, LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
  te_array = libshared_memory_alloc_largest_possible_array_from_unknown_node( ms, sizeof(struct test_element), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES, &number_elements );

  lfds710_freelist_init_valid_on_current_logical_core( &fs, ea, smallest_power_of_two_larger_than_or_equal_to_number_logical_processors, NULL );

  for( loop = 0 ; loop < number_elements ; loop++ )
  {
    (te_array+loop)->popped_flag = LOWERED;
    LFDS710_FREELIST_SET_VALUE_IN_ELEMENT( (te_array+loop)->fe, te_array+loop );
    lfds710_freelist_push( &fs, &(te_array+loop)->fe, &psts );
  }

  libtest_threadset_init( &ts, NULL );

  loop = 0;

  while( LFDS710_LIST_ASU_GET_START_AND_THEN_NEXT(*list_of_logical_processors, lasue) )
  {
    lp = LFDS710_LIST_ASU_GET_VALUE_FROM_ELEMENT( *lasue );
    tpts[loop].fs = &fs;
    LFDS710_PRNG_ST_GENERATE( psts, random_value );
    LFDS710_PRNG_ST_MIXING_FUNCTION( random_value );
    lfds710_prng_st_init( &tpts[loop].psts, random_value );

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

  /* TRD : there is a chance, although tiny, that some elements remain in the elimination layer
           each worker thread returns when a pop() fails, so they can return while elements remain in the EL
           so now we're validating, we ask the freelist for a count
           we then count the number of elements in the te_array which are RAISED and LOWERED
           and the LOWERED count should equal the number of elements remaining in the freelist
           we could go further and check they are the *same* elements, but this all needs rewriting...
  */

  lfds710_freelist_query( &fs, LFDS710_FREELIST_QUERY_SINGLETHREADED_GET_COUNT, NULL, &number_elements_in_freelist );

  vi.min_elements = vi.max_elements = number_elements_in_freelist;

  lfds710_freelist_query( &fs, LFDS710_FREELIST_QUERY_SINGLETHREADED_VALIDATE, &vi, dvs );

  // TRD : now we check each element has popped_flag fet to RAISED
  for( loop = 0 ; loop < number_elements ; loop++ )
    if( (te_array+loop)->popped_flag == RAISED )
      raised_count++;

  if( raised_count != number_elements - number_elements_in_freelist )
    *dvs = LFDS710_MISC_VALIDITY_INVALID_TEST_DATA;

  // TRD : cleanup
  lfds710_freelist_cleanup( &fs, NULL );

  return;
}





/****************************************************************************/
static libshared_pal_thread_return_t LIBSHARED_PAL_THREAD_CALLING_CONVENTION thread_popping( void *libtest_threadset_per_thread_state )
{
  struct lfds710_freelist_element
    *fe;

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

  while( lfds710_freelist_pop(tpts->fs, &fe, &tpts->psts) )
  {
    te = LFDS710_FREELIST_GET_VALUE_FROM_ELEMENT( *fe );
    te->popped_flag = RAISED;
  }

  LFDS710_MISC_BARRIER_STORE;

  lfds710_misc_force_store();

  return LIBSHARED_PAL_THREAD_RETURN_CAST(RETURN_SUCCESS);
}

