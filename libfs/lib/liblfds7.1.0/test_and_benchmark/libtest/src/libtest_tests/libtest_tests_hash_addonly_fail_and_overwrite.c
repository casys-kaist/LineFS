/***** includes *****/
#include "libtest_tests_internal.h"

/***** private prototypes *****/
static int key_compare_function( void const *new_value, void const *value_in_tree );
static void key_hash_function( void const *key, lfds710_pal_uint_t *hash );





/****************************************************************************/
#pragma warning( disable : 4100 )

void libtest_tests_hash_a_fail_and_overwrite_on_existing_key( struct lfds710_list_asu_state *list_of_logical_processors, struct libshared_memory_state *ms, enum lfds710_misc_validity *dvs )
{
  enum lfds710_hash_a_insert_result
    apr;

  struct lfds710_hash_a_element
    hae_one,
    hae_two;

  struct lfds710_hash_a_state
    has;

  struct lfds710_btree_au_state
    *baus;

  LFDS710_PAL_ASSERT( list_of_logical_processors != NULL );
  LFDS710_PAL_ASSERT( ms != NULL );
  LFDS710_PAL_ASSERT( dvs != NULL );

  *dvs = LFDS710_MISC_VALIDITY_VALID;

  baus = libshared_memory_alloc_from_unknown_node( ms, sizeof(struct lfds710_btree_au_state) * 10, LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );

  // TRD : fail on overwrite
  lfds710_hash_a_init_valid_on_current_logical_core( &has, baus, 10, key_compare_function, key_hash_function, LFDS710_HASH_A_EXISTING_KEY_FAIL, NULL );

  LFDS710_HASH_A_SET_KEY_IN_ELEMENT( hae_one, 1 );
  LFDS710_HASH_A_SET_VALUE_IN_ELEMENT( hae_one, 0 );
  apr = lfds710_hash_a_insert( &has, &hae_one, NULL );

  if( apr != LFDS710_HASH_A_PUT_RESULT_SUCCESS )
    *dvs = LFDS710_MISC_VALIDITY_INVALID_TEST_DATA;

  LFDS710_HASH_A_SET_KEY_IN_ELEMENT( hae_two, 1 );
  LFDS710_HASH_A_SET_VALUE_IN_ELEMENT( hae_two, 1 );
  apr = lfds710_hash_a_insert( &has, &hae_two, NULL );

  if( apr != LFDS710_HASH_A_PUT_RESULT_FAILURE_EXISTING_KEY )
    *dvs = LFDS710_MISC_VALIDITY_INVALID_TEST_DATA;

  lfds710_hash_a_cleanup( &has, NULL );

  // TRD : success on overwrite
  lfds710_hash_a_init_valid_on_current_logical_core( &has, baus, 10, key_compare_function, key_hash_function, LFDS710_HASH_A_EXISTING_KEY_OVERWRITE, NULL );

  LFDS710_HASH_A_SET_KEY_IN_ELEMENT( hae_one, 1 );
  LFDS710_HASH_A_SET_VALUE_IN_ELEMENT( hae_one, 1 );
  apr = lfds710_hash_a_insert( &has, &hae_one, NULL );

  if( apr != LFDS710_HASH_A_PUT_RESULT_SUCCESS )
    *dvs = LFDS710_MISC_VALIDITY_INVALID_TEST_DATA;

  LFDS710_HASH_A_SET_KEY_IN_ELEMENT( hae_two, 1 );
  LFDS710_HASH_A_SET_VALUE_IN_ELEMENT( hae_two, 1 );
  apr = lfds710_hash_a_insert( &has, &hae_two, NULL );

  if( apr != LFDS710_HASH_A_PUT_RESULT_SUCCESS_OVERWRITE )
    *dvs = LFDS710_MISC_VALIDITY_INVALID_TEST_DATA;

  lfds710_hash_a_cleanup( &has, NULL );

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





/****************************************************************************/
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

