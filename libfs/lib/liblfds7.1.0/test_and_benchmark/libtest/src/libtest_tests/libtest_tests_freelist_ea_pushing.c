/***** includes *****/
#include "libtest_tests_internal.h"

/***** structs *****/
struct test_per_thread_state
{
  lfds710_pal_uint_t
    number_elements_per_thread,
    thread_number;

  struct lfds710_freelist_state
    *fs;

  struct lfds710_prng_st_state
    psts;

  struct test_element
    *te_array;
};

struct test_element
{
  struct lfds710_freelist_element
    fe;

  lfds710_pal_uint_t
    datum,
    thread_number;
};

/***** private prototypes *****/
static libshared_pal_thread_return_t LIBSHARED_PAL_THREAD_CALLING_CONVENTION thread_pushing( void *libtest_threadset_per_thread_state );





/****************************************************************************/
void libtest_tests_freelist_ea_pushing( struct lfds710_list_asu_state *list_of_logical_processors, struct libshared_memory_state *ms, enum lfds710_misc_validity *dvs )
{
  lfds710_pal_uint_t
    loop = 0,
    number_elements,
    number_elements_per_thread,
    number_logical_processors,
    random_value,
    smallest_power_of_two_larger_than_or_equal_to_number_logical_processors = 2,
    temp_number_logical_processors;

  struct lfds710_freelist_element * volatile
    (*ea)[LFDS710_FREELIST_ELIMINATION_ARRAY_ELEMENT_SIZE_IN_FREELIST_ELEMENTS];

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

  /* TRD : we create an empty freelist

           we then create one thread per CPU, where each thread
           pushes 100,000 elements each as quickly as possible to the freelist

           the data pushed is a counter and a thread ID

           the threads exit when the freelist is full

           we then validate the freelist;

           checking that the counts increment on a per unique ID basis
           and that the number of elements we pop equals 100,000 per thread
           (since each element has an incrementing counter which is
            unique on a per unique ID basis, we can know we didn't lofe
            any elements)
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

  number_elements_per_thread = number_elements / number_logical_processors;

  // TRD : the main freelist
  lfds710_freelist_init_valid_on_current_logical_core( &fs, ea, smallest_power_of_two_larger_than_or_equal_to_number_logical_processors, NULL );

  libtest_threadset_init( &ts, NULL );

  while( LFDS710_LIST_ASU_GET_START_AND_THEN_NEXT(*list_of_logical_processors, lasue) )
  {
    lp = LFDS710_LIST_ASU_GET_VALUE_FROM_ELEMENT( *lasue );

    (tpts+loop)->fs = &fs;
    (tpts+loop)->thread_number = loop;
    (tpts+loop)->number_elements_per_thread = number_elements_per_thread;
    (tpts+loop)->te_array = te_array + loop * number_elements_per_thread;
    LFDS710_PRNG_ST_GENERATE( psts, random_value );
    LFDS710_PRNG_ST_MIXING_FUNCTION( random_value );
    lfds710_prng_st_init( &(tpts+loop)->psts, random_value );

    libtest_threadset_add_thread( &ts, &pts[loop], lp, thread_pushing, &tpts[loop] );

    loop++;
  }

  LFDS710_MISC_BARRIER_STORE;

  lfds710_misc_force_store();

  // TRD : run the test
  libtest_threadset_run( &ts );

  libtest_threadset_cleanup( &ts );

  // TRD : validate
  LFDS710_MISC_BARRIER_LOAD;

  vi.min_elements = vi.max_elements = number_elements_per_thread * number_logical_processors;

  lfds710_freelist_query( &fs, LFDS710_FREELIST_QUERY_SINGLETHREADED_VALIDATE, &vi, dvs );

  lfds710_freelist_cleanup( &fs, NULL );

  return;
}





/****************************************************************************/
static libshared_pal_thread_return_t LIBSHARED_PAL_THREAD_CALLING_CONVENTION thread_pushing( void *libtest_threadset_per_thread_state )
{
  lfds710_pal_uint_t
    loop;

  struct test_per_thread_state
    *tpts;

  struct libtest_threadset_per_thread_state
    *pts;

  LFDS710_MISC_BARRIER_LOAD;

  LFDS710_PAL_ASSERT( libtest_threadset_per_thread_state != NULL );

  pts = (struct libtest_threadset_per_thread_state *) libtest_threadset_per_thread_state;

  tpts = LIBTEST_THREADSET_GET_USER_STATE_FROM_PER_THREAD_STATE( *pts );

  for( loop = 0 ; loop < tpts->number_elements_per_thread ; loop++ )
  {
    (tpts->te_array+loop)->thread_number = tpts->thread_number;
    (tpts->te_array+loop)->datum = loop;
  }

  libtest_threadset_thread_ready_and_wait( pts );

  for( loop = 0 ; loop < tpts->number_elements_per_thread ; loop++ )
  {
    LFDS710_FREELIST_SET_VALUE_IN_ELEMENT( (tpts->te_array+loop)->fe, tpts->te_array+loop );
    lfds710_freelist_push( tpts->fs, &(tpts->te_array+loop)->fe, &tpts->psts );
  }

  LFDS710_MISC_BARRIER_STORE;

  lfds710_misc_force_store();

  return LIBSHARED_PAL_THREAD_RETURN_CAST(RETURN_SUCCESS);
}

