/***** includes *****/
#include "libtest_tests_internal.h"

/***** structs *****/
struct test_element
{
  struct lfds710_list_asu_element
    lasue;

  lfds710_pal_uint_t
    element_number,
    thread_number;
};

struct test_per_thread_state
{
  lfds710_pal_uint_t
    number_elements_per_thread;

  struct lfds710_list_asu_state
    *lasus;

  struct test_element
    *element_array;
};

/***** private prototypes *****/
static libshared_pal_thread_return_t LIBSHARED_PAL_THREAD_CALLING_CONVENTION new_start_thread( void *libtest_threadset_per_thread_state );





/****************************************************************************/
void libtest_tests_list_asu_new_start( struct lfds710_list_asu_state *list_of_logical_processors, struct libshared_memory_state *ms, enum lfds710_misc_validity *dvs )
{
  lfds710_pal_uint_t
    loop,
    number_elements,
    number_elements_per_thread,
    number_logical_processors,
    *per_thread_counters,
    subloop;

  struct lfds710_list_asu_element
    *lasue = NULL;

  struct lfds710_list_asu_state
    lasus;

  struct lfds710_misc_validation_info
    vi;

  struct test_element
    *element_array,
    *element;

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

  /* TRD : run one thread per logical processor
           run for 250k elements
           each thread loops, calling lfds710_list_asu_new_element_by_position( LFDS710_LIST_ASU_POSITION_START )
           data element contain s thread_number and element_number
           verification should show element_number decreasing on a per thread basis
  */

  *dvs = LFDS710_MISC_VALIDITY_VALID;

  lfds710_list_asu_query( list_of_logical_processors, LFDS710_LIST_ASU_QUERY_GET_POTENTIALLY_INACCURATE_COUNT, NULL, (void **) &number_logical_processors );
  tpts = libshared_memory_alloc_from_unknown_node( ms, sizeof(struct test_per_thread_state) * number_logical_processors, LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
  pts = libshared_memory_alloc_from_unknown_node( ms, sizeof(struct libtest_threadset_per_thread_state) * number_logical_processors, LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
  per_thread_counters = libshared_memory_alloc_from_unknown_node( ms, sizeof(lfds710_pal_uint_t) * number_logical_processors, LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
  element_array = libshared_memory_alloc_largest_possible_array_from_unknown_node( ms, sizeof(struct test_element), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES, &number_elements );

  number_elements_per_thread = number_elements / number_logical_processors;

  lfds710_list_asu_init_valid_on_current_logical_core( &lasus, NULL );

  for( loop = 0 ; loop < number_logical_processors ; loop++ )
    for( subloop = 0 ; subloop < number_elements_per_thread ; subloop++ )
    {
      (element_array+(loop*number_elements_per_thread)+subloop)->thread_number = loop;
      (element_array+(loop*number_elements_per_thread)+subloop)->element_number = subloop;
    }

  libtest_threadset_init( &ts, NULL );

  loop = 0;

  while( LFDS710_LIST_ASU_GET_START_AND_THEN_NEXT(*list_of_logical_processors,lasue) )
  {
    lp = LFDS710_LIST_ASU_GET_VALUE_FROM_ELEMENT( *lasue );

    (tpts+loop)->lasus = &lasus;
    (tpts+loop)->element_array = element_array + (loop*number_elements_per_thread);
    (tpts+loop)->number_elements_per_thread = number_elements_per_thread;

    libtest_threadset_add_thread( &ts, &pts[loop], lp, new_start_thread, &tpts[loop] );

    loop++;
  }

  LFDS710_MISC_BARRIER_STORE;

  lfds710_misc_force_store();

  // TRD : run the test
  libtest_threadset_run( &ts );

  libtest_threadset_cleanup( &ts );

  /* TRD : validate the resultant list
           iterate over each element
           we expect to find element numbers increment on a per thread basis
  */

  LFDS710_MISC_BARRIER_LOAD;

  vi.min_elements = vi.max_elements = number_elements_per_thread * number_logical_processors;

  lfds710_list_asu_query( &lasus, LFDS710_LIST_ASU_QUERY_SINGLETHREADED_VALIDATE, &vi, dvs );

  for( loop = 0 ; loop < number_logical_processors ; loop++ )
    *(per_thread_counters+loop) = number_elements_per_thread - 1;

  lasue = NULL;

  while( *dvs == LFDS710_MISC_VALIDITY_VALID and LFDS710_LIST_ASU_GET_START_AND_THEN_NEXT(lasus, lasue) )
  {
    element = LFDS710_LIST_ASU_GET_VALUE_FROM_ELEMENT( *lasue );

    if( element->thread_number >= number_logical_processors )
    {
      *dvs = LFDS710_MISC_VALIDITY_INVALID_TEST_DATA;
      break;
    }

    if( element->element_number < per_thread_counters[element->thread_number] )
      *dvs = LFDS710_MISC_VALIDITY_INVALID_MISSING_ELEMENTS;

    if( element->element_number > per_thread_counters[element->thread_number] )
      *dvs = LFDS710_MISC_VALIDITY_INVALID_ADDITIONAL_ELEMENTS;

    if( element->element_number == per_thread_counters[element->thread_number] )
      per_thread_counters[element->thread_number]--;
  }

  lfds710_list_asu_cleanup( &lasus, NULL );

  return;
}





/****************************************************************************/
static libshared_pal_thread_return_t LIBSHARED_PAL_THREAD_CALLING_CONVENTION new_start_thread( void *libtest_threadset_per_thread_state )
{
  lfds710_pal_uint_t
    loop;

  struct libtest_threadset_per_thread_state
    *pts;

  struct test_per_thread_state
    *tpts;

  LFDS710_MISC_MAKE_VALID_ON_CURRENT_LOGICAL_CORE_INITS_COMPLETED_BEFORE_NOW_ON_ANY_OTHER_LOGICAL_CORE;

  LFDS710_PAL_ASSERT( libtest_threadset_per_thread_state != NULL );

  pts = (struct libtest_threadset_per_thread_state *) libtest_threadset_per_thread_state;
  tpts = LIBTEST_THREADSET_GET_USER_STATE_FROM_PER_THREAD_STATE( *pts );

  libtest_threadset_thread_ready_and_wait( pts );

  for( loop = 0 ; loop < tpts->number_elements_per_thread ; loop++ )
  {
    LFDS710_LIST_ASU_SET_KEY_IN_ELEMENT( (tpts->element_array+loop)->lasue, tpts->element_array+loop );
    LFDS710_LIST_ASU_SET_VALUE_IN_ELEMENT( (tpts->element_array+loop)->lasue, tpts->element_array+loop );
    lfds710_list_asu_insert_at_position( tpts->lasus, &(tpts->element_array+loop)->lasue, NULL, LFDS710_LIST_ASU_POSITION_START );
  }

  LFDS710_MISC_BARRIER_STORE;

  lfds710_misc_force_store();

  return LIBSHARED_PAL_THREAD_RETURN_CAST(RETURN_SUCCESS);
}

