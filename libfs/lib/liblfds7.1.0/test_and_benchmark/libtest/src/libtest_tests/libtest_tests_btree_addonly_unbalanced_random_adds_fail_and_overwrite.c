/***** includes *****/
#include "libtest_tests_internal.h"

/***** private prototypes *****/
static int key_compare_function( void const *new_value, void const *value_in_tree );





/****************************************************************************/
#pragma warning( disable : 4100 )

void libtest_tests_btree_au_fail_and_overwrite_on_existing_key( struct lfds710_list_asu_state *list_of_logical_processors, struct libshared_memory_state *ms, enum lfds710_misc_validity *dvs )
{
  enum lfds710_btree_au_insert_result
    alr;

  struct lfds710_btree_au_element
    baue_one,
    baue_two,
    *existing_baue;

  struct lfds710_btree_au_state
    baus;

  void
    *value;

  LFDS710_PAL_ASSERT( list_of_logical_processors != NULL );
  LFDS710_PAL_ASSERT( ms != NULL );
  LFDS710_PAL_ASSERT( dvs != NULL );

  /* TRD : the random_adds tests with fail and overwrite don't (can't, not in a performant manner)
           test that the fail and/or overwrite of user data has *actually* happened - they use the
           return value from the link function call, rather than empirically observing the final
           state of the tree

           as such, we now have a couple of single threaded tests where we check that the user data
           value really is being modified (or not modified, as the case may be)
  */

  // internal_display_test_name( "Fail and overwrite on existing key" );

  *dvs = LFDS710_MISC_VALIDITY_VALID;

  /* TRD : so, we make a tree which is fail on existing
           add one element, with a known user data
           we then try to add the same key again, with a different user data
           the call should fail, and then we get the element by its key
           and check its user data is unchanged
           (and confirm the failed link returned the correct existing_baue)
           that's the first test done
  */

  lfds710_btree_au_init_valid_on_current_logical_core( &baus, key_compare_function, LFDS710_BTREE_AU_EXISTING_KEY_FAIL, NULL );

  LFDS710_BTREE_AU_SET_KEY_IN_ELEMENT( baue_one, 0 );
  LFDS710_BTREE_AU_SET_VALUE_IN_ELEMENT( baue_one, 1 );
  alr = lfds710_btree_au_insert( &baus, &baue_one, NULL );

  if( alr != LFDS710_BTREE_AU_INSERT_RESULT_SUCCESS )
    *dvs = LFDS710_MISC_VALIDITY_INVALID_TEST_DATA;

  LFDS710_BTREE_AU_SET_KEY_IN_ELEMENT( baue_two, 0 );
  LFDS710_BTREE_AU_SET_VALUE_IN_ELEMENT( baue_two, 2 );
  alr = lfds710_btree_au_insert( &baus, &baue_two, &existing_baue );

  if( alr != LFDS710_BTREE_AU_INSERT_RESULT_FAILURE_EXISTING_KEY )
    *dvs = LFDS710_MISC_VALIDITY_INVALID_TEST_DATA;

  if( existing_baue != &baue_one )
    *dvs = LFDS710_MISC_VALIDITY_INVALID_TEST_DATA;

  value = LFDS710_BTREE_AU_GET_VALUE_FROM_ELEMENT( *existing_baue );

  if( (void *) (lfds710_pal_uint_t) value != (void *) (lfds710_pal_uint_t) 1 )
    *dvs = LFDS710_MISC_VALIDITY_INVALID_TEST_DATA;

  lfds710_btree_au_cleanup( &baus, NULL );

  /* TRD : second test, make a tree which is overwrite on existing
           add one element, with a known user data
           we then try to add the same key again, with a different user data
           the call should succeed, and then we get the element by its key
           and check its user data is changed
           (and confirm the failed link returned the correct existing_baue)
           that's the secondtest done
  */

  lfds710_btree_au_init_valid_on_current_logical_core( &baus, key_compare_function, LFDS710_BTREE_AU_EXISTING_KEY_OVERWRITE, NULL );

  LFDS710_BTREE_AU_SET_KEY_IN_ELEMENT( baue_one, 0 );
  LFDS710_BTREE_AU_SET_VALUE_IN_ELEMENT( baue_one, 1 );
  alr = lfds710_btree_au_insert( &baus, &baue_one, NULL );

  if( alr != LFDS710_BTREE_AU_INSERT_RESULT_SUCCESS )
    *dvs = LFDS710_MISC_VALIDITY_INVALID_TEST_DATA;

  LFDS710_BTREE_AU_SET_KEY_IN_ELEMENT( baue_two, 0 );
  LFDS710_BTREE_AU_SET_VALUE_IN_ELEMENT( baue_two, 2 );
  alr = lfds710_btree_au_insert( &baus, &baue_two, NULL );

  if( alr != LFDS710_BTREE_AU_INSERT_RESULT_SUCCESS_OVERWRITE )
    *dvs = LFDS710_MISC_VALIDITY_INVALID_TEST_DATA;

  lfds710_btree_au_cleanup( &baus, NULL );

  return;
}

#pragma warning( default : 4100 )





/****************************************************************************/
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

