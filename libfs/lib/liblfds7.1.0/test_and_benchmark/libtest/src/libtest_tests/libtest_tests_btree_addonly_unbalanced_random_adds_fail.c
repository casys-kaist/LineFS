/***** includes *****/
#include "libtest_tests_internal.h"

/***** structs *****/
struct test_element
{
  struct lfds710_btree_au_element
    baue;

  lfds710_pal_uint_t
    key;
};

struct test_per_thread_state
{
  lfds710_pal_uint_t
    insert_fail_count,
    number_elements_per_thread;

  struct lfds710_btree_au_state
    *baus;

  struct test_element
    *element_array;
};

/***** private prototypes *****/
static int key_compare_function( void const *new_value, void const *value_in_tree );
static libshared_pal_thread_return_t LIBSHARED_PAL_THREAD_CALLING_CONVENTION thread_adding( void *libtest_threadset_per_thread_state );





/****************************************************************************/
void libtest_tests_btree_au_random_adds_fail_on_existing( struct lfds710_list_asu_state *list_of_logical_processors, struct libshared_memory_state *ms, enum lfds710_misc_validity *dvs )
{
  lfds710_pal_uint_t
    actual_sum_insert_failure_count,
    expected_sum_insert_failure_count,
    index = 0,
    *key_count_array,
    loop = 0,
    number_elements,
    number_elements_per_thread,
    number_logical_processors,
    random_value,
    subloop;

  struct lfds710_list_asu_element
    *lasue = NULL;

  struct lfds710_btree_au_element
    *baue = NULL;

  struct lfds710_btree_au_state
    baus;

  struct lfds710_prng_state
    ps;

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

  void
    *key;

  LFDS710_PAL_ASSERT( list_of_logical_processors != NULL );
  LFDS710_PAL_ASSERT( ms != NULL );
  LFDS710_PAL_ASSERT( dvs != NULL );

  /* TRD : we create a single btree_au
           we generate 10k elements per thread (one per logical processor) in an array
           we set a random number in each element, which is the key
           random numbers are generated are from 0 to 5000, so we must have some duplicates
           (we don't use value, so we always pass in a NULL for that when we insert)

           each thread loops, adds those elements into the btree, and counts the total number of insert fails
           (we don't count on a per value basis because of the performance hit - we'll be TLBing all the time)
           this test has the btree_au set to fail on add, so duplicates should be eliminated

           we then merge the per-thread arrays

           we should find in the tree one of every value, and the sum of the counts of each value (beyond the
           first value, which was inserted) in the merged arrays should equal the sum of the insert fails from
           each thread

           we check the count of unique values in the merged array and use that when calling the btree_au validation function

           we in-order walk and check that what we have in the tree matches what we have in the merged array
           and then check the fail counts
  */

  *dvs = LFDS710_MISC_VALIDITY_VALID;

  // TRD : allocate
  lfds710_list_asu_query( list_of_logical_processors, LFDS710_LIST_ASU_QUERY_GET_POTENTIALLY_INACCURATE_COUNT, NULL, (void **) &number_logical_processors );
  tpts = libshared_memory_alloc_from_unknown_node( ms, sizeof(struct test_per_thread_state) * number_logical_processors, LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
  pts = libshared_memory_alloc_from_unknown_node( ms, sizeof(struct libtest_threadset_per_thread_state) * number_logical_processors, LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
  // TRD : need a counter array later
  te_array = libshared_memory_alloc_largest_possible_array_from_unknown_node( ms, sizeof(struct test_element) + sizeof(lfds710_pal_uint_t), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES, &number_elements );

  number_elements_per_thread = number_elements / number_logical_processors;
  key_count_array = (lfds710_pal_uint_t *) ( te_array + number_elements );

  lfds710_prng_init_valid_on_current_logical_core( &ps, LFDS710_PRNG_SEED );

  lfds710_btree_au_init_valid_on_current_logical_core( &baus, key_compare_function, LFDS710_BTREE_AU_EXISTING_KEY_FAIL, NULL );

  // TRD : get the threads ready
  libtest_threadset_init( &ts, NULL );

  while( LFDS710_LIST_ASU_GET_START_AND_THEN_NEXT(*list_of_logical_processors,lasue) )
  {
    lp = LFDS710_LIST_ASU_GET_VALUE_FROM_ELEMENT( *lasue );
    (tpts+loop)->baus = &baus;
    (tpts+loop)->element_array = te_array + loop * number_elements_per_thread;
    (tpts+loop)->number_elements_per_thread = number_elements_per_thread;
    (tpts+loop)->insert_fail_count = 0;
    libtest_threadset_add_thread( &ts, &pts[loop], lp, thread_adding, &tpts[loop] );

    for( subloop = 0 ; subloop < number_elements_per_thread ; subloop++ )
    {
      LFDS710_PRNG_GENERATE( ps, random_value );
      ((tpts+loop)->element_array+subloop)->key = (lfds710_pal_uint_t) ( (number_elements/2) * ((double) random_value / (double) LFDS710_PRNG_MAX) );
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

  /* TRD : now for validation
           make an array equal to number_elements, set all to 0
           iterate over every per-thread array, counting the number of each value into this array
           so we can know how many elements ought to have failed to be inserted
           as well as being able to work out the actual number of elements which should be present in the btree, for the btree validation call
  */

  for( loop = 0 ; loop < number_elements ; loop++ )
    *(key_count_array+loop) = 0;

  for( loop = 0 ; loop < number_logical_processors ; loop++ )
    for( subloop = 0 ; subloop < number_elements_per_thread ; subloop++ )
      ( *(key_count_array+( (tpts+loop)->element_array+subloop)->key) )++;

  // TRD : first, btree validation function
  vi.min_elements = number_elements;

  for( loop = 0 ; loop < number_elements ; loop++ )
    if( *(key_count_array+loop) == 0 )
      vi.min_elements--;

  vi.max_elements = vi.min_elements;

  lfds710_btree_au_query( &baus, LFDS710_BTREE_AU_QUERY_SINGLETHREADED_VALIDATE, (void *) &vi, (void *) dvs );

  /* TRD : now check the sum of per-thread insert failures
           is what it should be, which is the sum of key_count_array,
           but with every count minus one (for the single succesful insert)
           and where elements of 0 are ignored (i.e. do not have -1 applied)
  */

  expected_sum_insert_failure_count = 0;

  for( loop = 0 ; loop < number_elements ; loop++ )
    if( *(key_count_array+loop) != 0 )
      expected_sum_insert_failure_count += *(key_count_array+loop) - 1;

  actual_sum_insert_failure_count = 0;

  for( loop = 0 ; loop < number_logical_processors ; loop++ )
    actual_sum_insert_failure_count += (tpts+loop)->insert_fail_count;

  if( expected_sum_insert_failure_count != actual_sum_insert_failure_count )
    *dvs = LFDS710_MISC_VALIDITY_INVALID_TEST_DATA;

  /* TRD : now compared the combined array and an in-order walk of the tree
           ignoring array elements with the value 0, we should find an exact match
  */

  if( *dvs == LFDS710_MISC_VALIDITY_VALID )
  {
    // TRD : in-order walk over btree_au and check key_count_array matches
    while( *dvs == LFDS710_MISC_VALIDITY_VALID and lfds710_btree_au_get_by_absolute_position_and_then_by_relative_position(&baus, &baue, LFDS710_BTREE_AU_ABSOLUTE_POSITION_SMALLEST_IN_TREE, LFDS710_BTREE_AU_RELATIVE_POSITION_NEXT_LARGER_ELEMENT_IN_ENTIRE_TREE) )
    {
      key = LFDS710_BTREE_AU_GET_KEY_FROM_ELEMENT( *baue );

      while( *(key_count_array+index) == 0 )
        index++;

      if( index++ != (lfds710_pal_uint_t) key )
        *dvs = LFDS710_MISC_VALIDITY_INVALID_TEST_DATA;
    }
  }

  lfds710_btree_au_cleanup( &baus, NULL );

  return;
}





/****************************************************************************/
#pragma warning( disable : 4100 )

static int key_compare_function( void const *new_key, void const *key_in_tree )
{
  int
    cr = 0;

  // TRD : key_new can be any value in its range
  // TRD : key_in_tree can be any value in its range

  if( (lfds710_pal_uint_t) new_key < (lfds710_pal_uint_t) key_in_tree )
    cr = -1;

  if( (lfds710_pal_uint_t) new_key > (lfds710_pal_uint_t) key_in_tree )
    cr = 1;

  return cr;
}

#pragma warning( default : 4100 )





/****************************************************************************/
static libshared_pal_thread_return_t LIBSHARED_PAL_THREAD_CALLING_CONVENTION thread_adding( void *libtest_threadset_per_thread_state )
{
  enum lfds710_btree_au_insert_result
    alr;

  lfds710_pal_uint_t
    index = 0;

  struct libtest_threadset_per_thread_state
    *pts;

  struct test_per_thread_state
    *tpts;

  LFDS710_MISC_MAKE_VALID_ON_CURRENT_LOGICAL_CORE_INITS_COMPLETED_BEFORE_NOW_ON_ANY_OTHER_LOGICAL_CORE;

  LFDS710_PAL_ASSERT( libtest_threadset_per_thread_state != NULL );

  pts = (struct libtest_threadset_per_thread_state *) libtest_threadset_per_thread_state;
  tpts = LIBTEST_THREADSET_GET_USER_STATE_FROM_PER_THREAD_STATE( *pts );

  libtest_threadset_thread_ready_and_wait( pts );

  while( index < tpts->number_elements_per_thread )
  {
    LFDS710_BTREE_AU_SET_KEY_IN_ELEMENT( (tpts->element_array+index)->baue, (tpts->element_array+index)->key );
    LFDS710_BTREE_AU_SET_VALUE_IN_ELEMENT( (tpts->element_array+index)->baue, 0 );
    alr = lfds710_btree_au_insert( tpts->baus, &(tpts->element_array+index)->baue, NULL );

    if( alr == LFDS710_BTREE_AU_INSERT_RESULT_FAILURE_EXISTING_KEY )
      tpts->insert_fail_count++;

    index++;
  }

  LFDS710_MISC_BARRIER_STORE;

  lfds710_misc_force_store();

  return LIBSHARED_PAL_THREAD_RETURN_CAST(RETURN_SUCCESS);
}

