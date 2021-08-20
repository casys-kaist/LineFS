/***** includes *****/
#include "libtest_tests_internal.h"

/***** structs *****/
struct test_element
{
  struct lfds710_hash_a_element
    hae;

  lfds710_pal_uint_t
    key;
};

struct test_per_thread_state
{
  lfds710_pal_uint_t
    number_elements_per_thread,
    overwrite_count;

  struct lfds710_hash_a_state
    *has;

  struct test_element
    *element_array;
};

/***** private prototypes *****/
static libshared_pal_thread_return_t LIBSHARED_PAL_THREAD_CALLING_CONVENTION thread_adding( void *libtest_threadset_per_thread_state );
static int key_compare_function( void const *new_key, void const *existing_key );
static void key_hash_function( void const *key, lfds710_pal_uint_t *hash );
static int LIBTEST_PAL_STDLIB_CALLBACK_CALLING_CONVENTION qsort_and_bsearch_key_compare_function( void const *e1, void const *e2 );





/****************************************************************************/
void libtest_tests_hash_a_random_adds_overwrite_on_existing( struct lfds710_list_asu_state *list_of_logical_processors, struct libshared_memory_state *ms, enum lfds710_misc_validity *dvs )
{
  int
    rv;

  lfds710_pal_uint_t
    actual_sum_overwrite_existing_count,
    expected_sum_overwrite_existing_count,
    *key_count_array,
    loop,
    number_elements_per_thread,
    number_elements_total,
    number_logical_processors,
    random_value;

  struct lfds710_hash_a_iterate
    hai;

  struct lfds710_hash_a_element
    *hae;

  struct lfds710_hash_a_state
    has;

  struct lfds710_list_asu_element
    *lasue = NULL;

  struct lfds710_btree_au_state
    *baus;

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
    *element_array;

  struct test_per_thread_state
    *tpts;

  void
    *key_pointer,
    *key;

  LFDS710_PAL_ASSERT( list_of_logical_processors != NULL );
  LFDS710_PAL_ASSERT( ms != NULL );
  LFDS710_PAL_ASSERT( dvs != NULL );

  /* TRD : we create a single hash_a
           we generate n elements per thread
           each element contains a key value, which is set to a random value
           (we don't use value, so it's just set to 0)
           the threads then run, putting
           the threads count their number of overwrite hits
           once the threads are done, then we
           count the number of each key
           from this we figure out the min/max element for hash_a validation, so we call validation
           we check the sum of overwrites for each thread is what it should be
           then using the hash_a get() we check all the elements we expect are present
           and then we iterate over the hash_a
           checking we see each key once
  */

  *dvs = LFDS710_MISC_VALIDITY_VALID;

  // TRD : allocate
  lfds710_list_asu_query( list_of_logical_processors, LFDS710_LIST_ASU_QUERY_GET_POTENTIALLY_INACCURATE_COUNT, NULL, (void **) &number_logical_processors );
  tpts = libshared_memory_alloc_from_unknown_node( ms, sizeof(struct test_per_thread_state) * number_logical_processors, LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
  pts = libshared_memory_alloc_from_unknown_node( ms, sizeof(struct libtest_threadset_per_thread_state) * number_logical_processors, LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
  baus = libshared_memory_alloc_from_unknown_node( ms, sizeof(struct lfds710_btree_au_state) * 1000, LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
  element_array = libshared_memory_alloc_largest_possible_array_from_unknown_node( ms, sizeof(struct test_element) + sizeof(lfds710_pal_uint_t), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES, &number_elements_total );
  key_count_array = (lfds710_pal_uint_t *) ( element_array + number_elements_total );

  // TRD : per thread first, for correct rounding, for later code
  number_elements_per_thread = number_elements_total / number_logical_processors;
  number_elements_total = number_elements_per_thread * number_logical_processors;

  lfds710_prng_init_valid_on_current_logical_core( &ps, LFDS710_PRNG_SEED );

  lfds710_hash_a_init_valid_on_current_logical_core( &has, baus, 1000, key_compare_function, key_hash_function, LFDS710_HASH_A_EXISTING_KEY_OVERWRITE, NULL );

  // TRD : created an ordered list of unique numbers

  for( loop = 0 ; loop < number_elements_total ; loop++ )
  {
    LFDS710_PRNG_GENERATE( ps, random_value );
    (element_array+loop)->key = (lfds710_pal_uint_t) ( (number_elements_total/2) * ((double) random_value / (double) LFDS710_PRNG_MAX) );
  }

  // TRD : get the threads ready
  libtest_threadset_init( &ts, NULL );

  loop = 0;

  while( LFDS710_LIST_ASU_GET_START_AND_THEN_NEXT(*list_of_logical_processors,lasue) )
  {
    lp = LFDS710_LIST_ASU_GET_VALUE_FROM_ELEMENT( *lasue );
    (tpts+loop)->has = &has;
    (tpts+loop)->element_array = element_array + number_elements_per_thread*loop;
    (tpts+loop)->overwrite_count = 0;
    (tpts+loop)->number_elements_per_thread = number_elements_per_thread;
    libtest_threadset_add_thread( &ts, &pts[loop], lp, thread_adding, &tpts[loop] );
    loop++;
  }

  // TRD : run the test
  libtest_threadset_run( &ts );

  libtest_threadset_cleanup( &ts );

  // TRD : validate
  LFDS710_MISC_BARRIER_LOAD;

  // TRD : now for validation
  for( loop = 0 ; loop < number_elements_total ; loop++ )
    *(key_count_array+loop) = 0;

  for( loop = 0 ; loop < number_elements_total ; loop++ )
    ( *(key_count_array + (element_array+loop)->key) )++;

  vi.min_elements = number_elements_total;

  for( loop = 0 ; loop < number_elements_total ; loop++ )
    if( *(key_count_array+loop) == 0 )
      vi.min_elements--;

  vi.max_elements = vi.min_elements;

  lfds710_hash_a_query( &has, LFDS710_HASH_A_QUERY_SINGLETHREADED_VALIDATE, (void *) &vi, (void *) dvs );

  expected_sum_overwrite_existing_count = 0;

  for( loop = 0 ; loop < number_elements_total ; loop++ )
    if( *(key_count_array+loop) != 0 )
      expected_sum_overwrite_existing_count += *(key_count_array+loop) - 1;

  actual_sum_overwrite_existing_count = 0;

  for( loop = 0 ; loop < number_logical_processors ; loop++ )
    actual_sum_overwrite_existing_count += (tpts+loop)->overwrite_count;

  if( expected_sum_overwrite_existing_count != actual_sum_overwrite_existing_count )
    *dvs = LFDS710_MISC_VALIDITY_INVALID_TEST_DATA;

  // TRD : now loop over the expected array and check we can get() every element
  for( loop = 0 ; loop < number_elements_total ; loop++ )
    if( *(key_count_array+loop) > 0 )
    {
      rv = lfds710_hash_a_get_by_key( &has, NULL, NULL, (void *) loop, &hae );

      if( rv != 1 )
        *dvs = LFDS710_MISC_VALIDITY_INVALID_TEST_DATA;
    }

  /* TRD : now iterate, checking we find every element and no others
           to do this in a timely manner, we need to qsort() the key values
           and use bsearch() to check for items in the array
  */

  for( loop = 0 ; loop < number_elements_total ; loop++ )
    if( *(key_count_array+loop) != 0 )
      *(key_count_array+loop) = loop;
    else
      *(key_count_array+loop) = 0;

  qsort( key_count_array, number_elements_total, sizeof(lfds710_pal_uint_t), qsort_and_bsearch_key_compare_function );

  lfds710_hash_a_iterate_init( &has, &hai );

  while( *dvs == LFDS710_MISC_VALIDITY_VALID and lfds710_hash_a_iterate(&hai, &hae) )
  {
    key = LFDS710_HASH_A_GET_KEY_FROM_ELEMENT( *hae );

    key_pointer = bsearch( &key, key_count_array, number_elements_total, sizeof(lfds710_pal_uint_t), qsort_and_bsearch_key_compare_function );

    if( key_pointer == NULL )
      *dvs = LFDS710_MISC_VALIDITY_INVALID_TEST_DATA;
  }

  // TRD : cleanup
  lfds710_hash_a_cleanup( &has, NULL );

  return;
}





/****************************************************************************/
static libshared_pal_thread_return_t LIBSHARED_PAL_THREAD_CALLING_CONVENTION thread_adding( void *libtest_threadset_per_thread_state )
{
  enum lfds710_hash_a_insert_result
    apr;

  lfds710_pal_uint_t
    index = 0;

  struct test_per_thread_state
    *tpts;

  struct libtest_threadset_per_thread_state
    *pts;

  LFDS710_MISC_MAKE_VALID_ON_CURRENT_LOGICAL_CORE_INITS_COMPLETED_BEFORE_NOW_ON_ANY_OTHER_LOGICAL_CORE;

  LFDS710_PAL_ASSERT( libtest_threadset_per_thread_state != NULL );

  pts = (struct libtest_threadset_per_thread_state *) libtest_threadset_per_thread_state;

  tpts = LIBTEST_THREADSET_GET_USER_STATE_FROM_PER_THREAD_STATE( *pts );

  libtest_threadset_thread_ready_and_wait( pts );

  while( index < tpts->number_elements_per_thread )
  {
    LFDS710_HASH_A_SET_KEY_IN_ELEMENT( (tpts->element_array+index)->hae, (tpts->element_array+index)->key );
    LFDS710_HASH_A_SET_VALUE_IN_ELEMENT( (tpts->element_array+index)->hae, 0 );
    apr = lfds710_hash_a_insert( tpts->has, &(tpts->element_array+index)->hae, NULL );

    if( apr == LFDS710_HASH_A_PUT_RESULT_SUCCESS_OVERWRITE )
      tpts->overwrite_count++;

    index++;
  }

  LFDS710_MISC_BARRIER_STORE;

  lfds710_misc_force_store();

  return LIBSHARED_PAL_THREAD_RETURN_CAST(RETURN_SUCCESS);
}





/****************************************************************************/
#pragma warning( disable : 4100 )

static int key_compare_function( void const *new_key, void const *existing_key )
{
  int
    cr = 0;

  // TRD : new_key can be NULL (i.e. 0)
  // TRD : existing_key can be NULL (i.e. 0)

  if( (lfds710_pal_uint_t) new_key < (lfds710_pal_uint_t) existing_key )
    cr = -1;

  if( (lfds710_pal_uint_t) new_key > (lfds710_pal_uint_t) existing_key )
    cr = 1;

  return cr;
}

#pragma warning( default : 4100 )





/****************************************************************************/
#pragma warning( disable : 4100 )

static void key_hash_function( void const *key, lfds710_pal_uint_t *hash )
{
  // TRD : key can be NULL
  LFDS710_PAL_ASSERT( hash != NULL );

  *hash = 0;

  /* TRD : this function iterates over the user data
           and we are using the void pointer *as* key data
           so here we need to pass in the addy of key
  */

  LFDS710_HASH_A_HASH_FUNCTION( (void *) &key, sizeof(lfds710_pal_uint_t), *hash );

  return;
}

#pragma warning( default : 4100 )





/****************************************************************************/
static int LIBTEST_PAL_STDLIB_CALLBACK_CALLING_CONVENTION qsort_and_bsearch_key_compare_function( void const *e1, void const *e2 )
{
  int
    cr = 0;

  lfds710_pal_uint_t
    s1,
    s2;

  s1 = *(lfds710_pal_uint_t *) e1;
  s2 = *(lfds710_pal_uint_t *) e2;

  if( s1 > s2 )
    cr = 1;

  if( s1 < s2 )
    cr = -1;

  return cr;
}

