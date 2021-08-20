/***** includes *****/
#include "libtest_tests_internal.h"

/***** structs *****/
struct test_element
{
  struct lfds710_list_aso_element
    lasoe;

  lfds710_pal_uint_t
    element_number,
    thread_number;
};

struct test_per_thread_state
{
  enum flag
    error_flag;

  lfds710_pal_uint_t
    number_elements_per_thread;

  struct lfds710_list_aso_state
    *lasos;

  struct test_element
    *element_array;
};

/***** private prototypes *****/
static int new_ordered_with_cursor_compare_function( void const *value_new, void const *value_in_list );
static libshared_pal_thread_return_t LIBSHARED_PAL_THREAD_CALLING_CONVENTION new_ordered_with_cursor_insert_thread( void *libtest_threadset_per_thread_state );
static libshared_pal_thread_return_t LIBSHARED_PAL_THREAD_CALLING_CONVENTION new_ordered_with_cursor_cursor_thread( void *libtest_threadset_per_thread_state );





/****************************************************************************/
void libtest_tests_list_aso_new_ordered_with_cursor( struct lfds710_list_asu_state *list_of_logical_processors, struct libshared_memory_state *ms, enum lfds710_misc_validity *dvs )
{
  lfds710_pal_uint_t
    loop,
    number_elements,
    number_elements_per_thread,
    number_elements_total,
    number_logical_processors,
    offset,
    temp;

  struct lfds710_list_aso_state
    lasos;

  struct lfds710_list_asu_element
    *lasue = NULL;

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

  LFDS710_PAL_ASSERT( list_of_logical_processors != NULL );
  LFDS710_PAL_ASSERT( ms != NULL );
  LFDS710_PAL_ASSERT( dvs != NULL );

  /* TRD : run two threads per logical processor

           the test runs for 10 seconds

           the first thread loops over a pre-set list of random numbers
           continually adding them using ordered insert

           the second thread keeps iterating over the list, checking that
           each element is larger than its predecessor
  */

  *dvs = LFDS710_MISC_VALIDITY_VALID;

  lfds710_list_asu_query( list_of_logical_processors, LFDS710_LIST_ASU_QUERY_GET_POTENTIALLY_INACCURATE_COUNT, NULL, (void **) &number_logical_processors );
  tpts = libshared_memory_alloc_from_unknown_node( ms, sizeof(struct test_per_thread_state) * number_logical_processors * 2, LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
  pts = libshared_memory_alloc_from_unknown_node( ms, sizeof(struct libtest_threadset_per_thread_state) * number_logical_processors * 2, LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
  element_array = libshared_memory_alloc_largest_possible_array_from_unknown_node( ms, sizeof(struct test_element), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES, &number_elements );

  number_elements_per_thread = number_elements / number_logical_processors;

  lfds710_prng_init_valid_on_current_logical_core( &ps, LFDS710_PRNG_SEED );

  lfds710_list_aso_init_valid_on_current_logical_core( &lasos, new_ordered_with_cursor_compare_function, LFDS710_LIST_ASO_INSERT_RESULT_FAILURE_EXISTING_KEY, NULL );

  /* TRD : create randomly ordered number array with unique elements

           unique isn't necessary - the list will sort anyway - but
           it permits slightly better validation
  */

  // TRD : or the test takes a looooooong time...
  if( number_elements_per_thread > 1000 )
    number_elements_per_thread = 1000;

  number_elements_total = number_elements_per_thread * number_logical_processors;

  for( loop = 0 ; loop < number_elements_total ; loop++ )
    (element_array+loop)->element_number = loop;

  for( loop = 0 ; loop < number_elements_total ; loop++ )
  {
    LFDS710_PRNG_GENERATE( ps, offset );
    offset %= number_elements_total;
    temp = (element_array + offset)->element_number;
    (element_array + offset)->element_number = (element_array + loop)->element_number;
    (element_array + loop)->element_number = temp;
  }

  // TRD : get the threads ready
  libtest_threadset_init( &ts, NULL );

  loop = 0;

  while( LFDS710_LIST_ASU_GET_START_AND_THEN_NEXT(*list_of_logical_processors,lasue) )
  {
    lp = LFDS710_LIST_ASU_GET_VALUE_FROM_ELEMENT( *lasue );

    // TRD : the insert threads
    (tpts+loop)->lasos = &lasos;
    (tpts+loop)->element_array = element_array + number_elements_per_thread*loop;
    (tpts+loop)->error_flag = LOWERED;
    (tpts+loop)->number_elements_per_thread = number_elements_per_thread;
    libtest_threadset_add_thread( &ts, &pts[loop], lp, new_ordered_with_cursor_insert_thread, &tpts[loop] );

    // TRD : the cursor threads
    (tpts+loop+number_logical_processors)->lasos = &lasos;
    (tpts+loop+number_logical_processors)->element_array = NULL;
    (tpts+loop+number_logical_processors)->error_flag = LOWERED;
    libtest_threadset_add_thread( &ts, &pts[loop+number_logical_processors], lp, new_ordered_with_cursor_cursor_thread, &tpts[loop+number_logical_processors] );

    loop++;
  }

  LFDS710_MISC_BARRIER_STORE;

  lfds710_misc_force_store();

  libtest_threadset_run( &ts );

  libtest_threadset_cleanup( &ts );

  /* TRD : validate the resultant list

           the cursor threads were checking for orderedness
           if that failed, they raise their error_flag
           so validate the list, then check error_flags
  */

  LFDS710_MISC_BARRIER_LOAD;

  vi.min_elements = vi.max_elements = number_elements_total;

  lfds710_list_aso_query( &lasos, LFDS710_LIST_ASO_QUERY_SINGLETHREADED_VALIDATE, &vi, dvs );

  if( *dvs == LFDS710_MISC_VALIDITY_VALID )
    for( loop = number_logical_processors ; loop < number_logical_processors * 2 ; loop++ )
      if( (tpts+loop)->error_flag == RAISED )
        *dvs = LFDS710_MISC_VALIDITY_INVALID_ORDER;

  lfds710_list_aso_cleanup( &lasos, NULL );

  return;
}





/****************************************************************************/
#pragma warning( disable : 4100 )

static int new_ordered_with_cursor_compare_function( void const *value_new, void const *value_in_list )
{
  int
    cr = 0;

  struct test_element
    *e1,
    *e2;

  // TRD : value_new can be any value in its range
  // TRD : value_in_list can be any value in its range

  e1 = (struct test_element *) value_new;
  e2 = (struct test_element *) value_in_list;

  if( e1->element_number < e2->element_number )
    cr = -1;

  if( e1->element_number > e2->element_number )
    cr = 1;

  return cr;
}

#pragma warning( default : 4100 )





/****************************************************************************/
static libshared_pal_thread_return_t LIBSHARED_PAL_THREAD_CALLING_CONVENTION new_ordered_with_cursor_insert_thread( void *libtest_threadset_per_thread_state )
{
  lfds710_pal_uint_t
    loop;

  struct libtest_threadset_per_thread_state
    *pts;

  struct test_per_thread_state
    *tpts;

  LFDS710_MISC_BARRIER_LOAD;

  LFDS710_PAL_ASSERT( libtest_threadset_per_thread_state != NULL );

  pts = (struct libtest_threadset_per_thread_state *) libtest_threadset_per_thread_state;
  tpts = LIBTEST_THREADSET_GET_USER_STATE_FROM_PER_THREAD_STATE( *pts );

  libtest_threadset_thread_ready_and_wait( pts );

  for( loop = 0 ; loop < tpts->number_elements_per_thread ; loop++ )
  {
    LFDS710_LIST_ASO_SET_KEY_IN_ELEMENT( (tpts->element_array+loop)->lasoe, tpts->element_array+loop );
    LFDS710_LIST_ASO_SET_VALUE_IN_ELEMENT( (tpts->element_array+loop)->lasoe, tpts->element_array+loop );
    lfds710_list_aso_insert( tpts->lasos, &(tpts->element_array+loop)->lasoe, NULL );
  }

  LFDS710_MISC_BARRIER_STORE;

  lfds710_misc_force_store();

  return LIBSHARED_PAL_THREAD_RETURN_CAST(RETURN_SUCCESS);
}





/****************************************************************************/
static libshared_pal_thread_return_t LIBSHARED_PAL_THREAD_CALLING_CONVENTION new_ordered_with_cursor_cursor_thread( void *libtest_threadset_per_thread_state )
{
  lfds710_pal_uint_t
    prev_element_number;

  lfds710_pal_uint_t
    time_loop = 0;

  struct lfds710_list_aso_element
    *lasoe;

  struct test_element
    *element;

  struct libtest_threadset_per_thread_state
    *pts;

  struct test_per_thread_state
    *tpts;

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
    prev_element_number = 0;

    lasoe = LFDS710_LIST_ASO_GET_START( *tpts->lasos );

    // TRD : we may get start before any element has been added to the list
    if( lasoe == NULL )
      continue;

    element = LFDS710_LIST_ASO_GET_VALUE_FROM_ELEMENT( *lasoe );

    if( element->element_number < prev_element_number )
      tpts->error_flag = RAISED;

    prev_element_number = element->element_number;

    lasoe = LFDS710_LIST_ASO_GET_NEXT( *lasoe );

    while( lasoe != NULL )
    {
      element = LFDS710_LIST_ASO_GET_VALUE_FROM_ELEMENT( *lasoe );

      if( element->element_number <= prev_element_number )
        tpts->error_flag = RAISED;

      prev_element_number = element->element_number;

      lasoe = LFDS710_LIST_ASO_GET_NEXT( *lasoe );
    }

    if( time_loop++ == REDUCED_TIME_LOOP_COUNT )
    {
      time_loop = 0;
      time( &current_time );
    }
  }

  LFDS710_MISC_BARRIER_STORE;

  lfds710_misc_force_store();

  return LIBSHARED_PAL_THREAD_RETURN_CAST(RETURN_SUCCESS);
}

