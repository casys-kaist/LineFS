/***** includes *****/
#include "libbenchmark_datastructure_btree_au_internal.h"

/***** private prototypes *****/
static void libbenchmark_datastructure_btree_au_internal_inorder_walk_from_largest_get_next_smallest_element( struct libbenchmark_datastructure_btree_au_pthread_rwlock_state *baus, struct libbenchmark_datastructure_btree_au_pthread_rwlock_element **baue );
static void libbenchmark_datastructure_btree_au_internal_inorder_walk_from_smallest_get_next_largest_element( struct libbenchmark_datastructure_btree_au_pthread_rwlock_state *baus, struct libbenchmark_datastructure_btree_au_pthread_rwlock_element **baue );





/****************************************************************************/
void libbenchmark_datastructure_btree_au_pthread_rwlock_init( struct libbenchmark_datastructure_btree_au_pthread_rwlock_state *baus,
                                                              int (*key_compare_function)(void const *new_key, void const *existing_key),
                                                              enum libbenchmark_datastructure_btree_au_pthread_rwlock_existing_key existing_key,
                                                              void *user_state )
{
  LFDS710_PAL_ASSERT( baus != NULL );
  LFDS710_PAL_ASSERT( key_compare_function != NULL );
  // TRD : existing_key can be any value in its range
  // TRD : user_state can be NULL

  baus->root = NULL;
  baus->key_compare_function = key_compare_function;
  baus->existing_key = existing_key;
  baus->user_state = user_state;

  LIBBENCHMARK_PAL_LOCK_PTHREAD_RWLOCK_CREATE( baus->lock );

  LFDS710_MISC_BARRIER_STORE;

  lfds710_misc_force_store();

  return;
}





/****************************************************************************/
void libbenchmark_datastructure_btree_au_pthread_rwlock_cleanup( struct libbenchmark_datastructure_btree_au_pthread_rwlock_state *baus,
                                                                 void (*element_cleanup_callback)(struct libbenchmark_datastructure_btree_au_pthread_rwlock_state *baus, struct libbenchmark_datastructure_btree_au_pthread_rwlock_element *baue) )
{
  enum libbenchmark_datastructure_btree_au_delete_action
    delete_action = LIBBENCHMARK_DATASTRUCTURE_BTREE_AU_DELETE_SELF; // TRD : to remove compiler warning

  struct libbenchmark_datastructure_btree_au_pthread_rwlock_element
    *baue,
    *temp;

  LFDS710_PAL_ASSERT( baus != NULL );
  // TRD : element_delete_function can be NULL

  if( element_cleanup_callback != NULL )
  {
    libbenchmark_datastructure_btree_au_pthread_rwlock_get_by_absolute_position_for_read( baus, &baue, LIBBENCHMARK_DATASTRUCTURE_BTREE_AU_PTHREAD_RWLOCK_ABSOLUTE_POSITION_ROOT );

    while( baue != NULL )
    {
      if( baue->left == NULL and baue->right == NULL )
        delete_action = LIBBENCHMARK_DATASTRUCTURE_BTREE_AU_DELETE_SELF;

      if( baue->left != NULL and baue->right == NULL )
        delete_action = LIBBENCHMARK_DATASTRUCTURE_BTREE_AU_DELETE_SELF_REPLACE_WITH_LEFT_CHILD;

      if( baue->left == NULL and baue->right != NULL )
        delete_action = LIBBENCHMARK_DATASTRUCTURE_BTREE_AU_DELETE_SELF_REPLACE_WITH_RIGHT_CHILD;

      if( baue->left != NULL and baue->right != NULL )
        delete_action = LIBBENCHMARK_DATASTRUCTURE_BTREE_AU_DELETE_MOVE_LEFT;

      switch( delete_action )
      {
        case LIBBENCHMARK_DATASTRUCTURE_BTREE_AU_DELETE_SELF:
          // TRD : if we have a parent (we could be root) set his point to us to NULL
          if( baue->up != NULL )
          {
            if( baue->up->left == baue )
              baue->up->left = NULL;
            if( baue->up->right == baue )
              baue->up->right = NULL;
          }

          temp = baue;
          libbenchmark_datastructure_btree_au_pthread_rwlock_get_by_relative_position_for_read( baus, &baue, LIBBENCHMARK_DATASTRUCTURE_BTREE_AU_PTHREAD_RWLOCK_RELATIVE_POSITION_UP );
          element_cleanup_callback( baus, temp );
        break;

        case LIBBENCHMARK_DATASTRUCTURE_BTREE_AU_DELETE_SELF_REPLACE_WITH_LEFT_CHILD:
          baue->left->up = baue->up;
          if( baue->up != NULL )
          {
            if( baue->up->left == baue )
              baue->up->left = baue->left;
            if( baue->up->right == baue )
              baue->up->right = baue->left;
          }

          temp = baue;
          libbenchmark_datastructure_btree_au_pthread_rwlock_get_by_relative_position_for_read( baus, &baue, LIBBENCHMARK_DATASTRUCTURE_BTREE_AU_PTHREAD_RWLOCK_RELATIVE_POSITION_LEFT );
          element_cleanup_callback( baus, temp );
        break;

        case LIBBENCHMARK_DATASTRUCTURE_BTREE_AU_DELETE_SELF_REPLACE_WITH_RIGHT_CHILD:
          baue->right->up = baue->up;
          if( baue->up != NULL )
          {
            if( baue->up->left == baue )
              baue->up->left = baue->right;
            if( baue->up->right == baue )
              baue->up->right = baue->right;
          }

          temp = baue;
          libbenchmark_datastructure_btree_au_pthread_rwlock_get_by_relative_position_for_read( baus, &baue, LIBBENCHMARK_DATASTRUCTURE_BTREE_AU_PTHREAD_RWLOCK_RELATIVE_POSITION_RIGHT );
          element_cleanup_callback( baus, temp );
        break;

        case LIBBENCHMARK_DATASTRUCTURE_BTREE_AU_DELETE_MOVE_LEFT:
          libbenchmark_datastructure_btree_au_pthread_rwlock_get_by_relative_position_for_read( baus, &baue, LIBBENCHMARK_DATASTRUCTURE_BTREE_AU_PTHREAD_RWLOCK_RELATIVE_POSITION_LEFT );
        break;
      }
    }
  }

  LIBBENCHMARK_PAL_LOCK_PTHREAD_RWLOCK_DESTROY( baus->lock );

  return;
}





/****************************************************************************/
enum libbenchmark_datastructure_btree_au_pthread_rwlock_insert_result libbenchmark_datastructure_btree_au_pthread_rwlock_insert( struct libbenchmark_datastructure_btree_au_pthread_rwlock_state *baus,
                                                                                                                                 struct libbenchmark_datastructure_btree_au_pthread_rwlock_element *baue,
                                                                                                                                 struct libbenchmark_datastructure_btree_au_pthread_rwlock_element **existing_baue )
{
  int
    compare_result = 0;

  struct libbenchmark_datastructure_btree_au_pthread_rwlock_element
    *baue_next = NULL,
    *baue_parent = NULL,
    *baue_temp;

  LFDS710_PAL_ASSERT( baus != NULL );
  LFDS710_PAL_ASSERT( baue != NULL );
  // TRD : existing_baue can be NULL

  LIBBENCHMARK_PAL_LOCK_PTHREAD_RWLOCK_GET_WRITE( baus->lock );

  baue->up = baue->left = baue->right = NULL;

  baue_temp = baus->root;

  while( baue_temp != NULL )
  {
    compare_result = baus->key_compare_function( baue->key, baue_temp->key );

    if( compare_result == 0 )
    {
      if( existing_baue != NULL )
        *existing_baue = baue_temp;

      switch( baus->existing_key )
      {
        case LIBBENCHMARK_DATASTRUCTURE_BTREE_AU_PTHREAD_RWLOCK_EXISTING_KEY_OVERWRITE:
          LIBBENCHMARK_DATASTRUCTURE_BTREE_AU_PTHREAD_RWLOCK_SET_VALUE_IN_ELEMENT( *baus, *baue_temp, baue->value );
          return LIBBENCHMARK_DATASTRUCTURE_BTREE_AU_PTHREAD_RWLOCK_INSERT_RESULT_SUCCESS_OVERWRITE;
        break;

        case LIBBENCHMARK_DATASTRUCTURE_BTREE_AU_PTHREAD_RWLOCK_EXISTING_KEY_FAIL:
          return LIBBENCHMARK_DATASTRUCTURE_BTREE_AU_PTHREAD_RWLOCK_INSERT_RESULT_FAILURE_EXISTING_KEY;
        break;
      }
    }

    if( compare_result < 0 )
      baue_next = baue_temp->left;

    if( compare_result > 0 )
      baue_next = baue_temp->right;

    baue_parent = baue_temp;
    baue_temp = baue_next;
  }

  if( baue_parent == NULL )
  {
    baue->up = baus->root;
    baus->root = baue;  }

  if( baue_parent != NULL )
  {
    if( compare_result <= 0 )
    {
      baue->up = baue_parent;
      baue_parent->left = baue;
    }

    if( compare_result > 0 )
    {
      baue->up = baue_parent;
      baue_parent->right = baue;
    }
  }

  LIBBENCHMARK_PAL_LOCK_PTHREAD_RWLOCK_RELEASE( baus->lock );

  // TRD : if we get to here, we added (not failed or overwrite on exist) a new element
  if( existing_baue != NULL )
    *existing_baue = NULL;

  return LIBBENCHMARK_DATASTRUCTURE_BTREE_AU_PTHREAD_RWLOCK_INSERT_RESULT_SUCCESS;
}





/****************************************************************************/
int libbenchmark_datastructure_btree_au_pthread_rwlock_get_by_key_for_read( struct libbenchmark_datastructure_btree_au_pthread_rwlock_state *baus,
                                                                            int (*key_compare_function)(void const *new_key, void const *existing_key),
                                                                            void *key,
                                                                            struct libbenchmark_datastructure_btree_au_pthread_rwlock_element **baue )
{
  int
    compare_result = !0,
    rv = 1;

  LFDS710_PAL_ASSERT( baus != NULL );
  // TRD : key_compare_function can be NULL
  // TRD : key can be NULL
  LFDS710_PAL_ASSERT( baue != NULL );

  if( key_compare_function == NULL )
    key_compare_function = baus->key_compare_function;

  LIBBENCHMARK_PAL_LOCK_PTHREAD_RWLOCK_GET_READ( baus->lock );

  *baue = baus->root;

  while( *baue != NULL and compare_result != 0 )
  {
    compare_result = key_compare_function( key, (*baue)->key );

    if( compare_result < 0 )
      *baue = (*baue)->left;

    if( compare_result > 0 )
      *baue = (*baue)->right;
  }

  LIBBENCHMARK_PAL_LOCK_PTHREAD_RWLOCK_RELEASE( baus->lock );

  if( *baue == NULL )
    rv = 0;

  return rv;
}





/****************************************************************************/
int libbenchmark_datastructure_btree_au_pthread_rwlock_get_by_key_for_write( struct libbenchmark_datastructure_btree_au_pthread_rwlock_state *baus,
                                                                             int (*key_compare_function)(void const *new_key, void const *existing_key),
                                                                             void *key,
                                                                             struct libbenchmark_datastructure_btree_au_pthread_rwlock_element **baue )
{
  int
    compare_result = !0,
    rv = 1;

  LFDS710_PAL_ASSERT( baus != NULL );
  // TRD : key_compare_function can be NULL
  // TRD : key can be NULL
  LFDS710_PAL_ASSERT( baue != NULL );

  if( key_compare_function == NULL )
    key_compare_function = baus->key_compare_function;

  LIBBENCHMARK_PAL_LOCK_PTHREAD_RWLOCK_GET_WRITE( baus->lock );

  *baue = baus->root;

  while( *baue != NULL and compare_result != 0 )
  {
    compare_result = key_compare_function( key, (*baue)->key );

    if( compare_result < 0 )
      *baue = (*baue)->left;

    if( compare_result > 0 )
      *baue = (*baue)->right;
  }

  LIBBENCHMARK_PAL_LOCK_PTHREAD_RWLOCK_RELEASE( baus->lock );

  if( *baue == NULL )
    rv = 0;

  return rv;
}





/****************************************************************************/
int libbenchmark_datastructure_btree_au_pthread_rwlock_get_by_absolute_position_for_read( struct libbenchmark_datastructure_btree_au_pthread_rwlock_state *baus, struct libbenchmark_datastructure_btree_au_pthread_rwlock_element **baue, enum libbenchmark_datastructure_btree_au_pthread_rwlock_absolute_position absolute_position )
{
  int
    rv = 1;

  LFDS710_PAL_ASSERT( baus != NULL );
  LFDS710_PAL_ASSERT( baue != NULL );
  // TRD : absolute_position can be any value in its range

  LIBBENCHMARK_PAL_LOCK_PTHREAD_RWLOCK_GET_READ( baus->lock );

  *baue = baus->root;

  switch( absolute_position )
  {
    case LIBBENCHMARK_DATASTRUCTURE_BTREE_AU_PTHREAD_RWLOCK_ABSOLUTE_POSITION_ROOT:
    break;

    case LIBBENCHMARK_DATASTRUCTURE_BTREE_AU_PTHREAD_RWLOCK_ABSOLUTE_POSITION_LARGEST_IN_TREE:
      if( *baue != NULL )
        while( (*baue)->right != NULL )
          *baue = (*baue)->right;
    break;

    case LIBBENCHMARK_DATASTRUCTURE_BTREE_AU_PTHREAD_RWLOCK_ABSOLUTE_POSITION_SMALLEST_IN_TREE:
      if( *baue != NULL )
        while( (*baue)->left != NULL )
          *baue = (*baue)->left;
    break;
  }

  LIBBENCHMARK_PAL_LOCK_PTHREAD_RWLOCK_RELEASE( baus->lock );

  if( *baue == NULL )
    rv = 0;

  return rv;
}





/****************************************************************************/
int libbenchmark_datastructure_btree_au_pthread_rwlock_get_by_relative_position_for_read( struct libbenchmark_datastructure_btree_au_pthread_rwlock_state *baus, struct libbenchmark_datastructure_btree_au_pthread_rwlock_element **baue, enum libbenchmark_datastructure_btree_au_pthread_rwlock_relative_position relative_position )
{
  int
    rv = 1;

  LFDS710_PAL_ASSERT( baus != NULL );
  LFDS710_PAL_ASSERT( baue != NULL );
  // TRD : relative_position can baue any value in its range

  if( *baue == NULL )
    return 0;

  LIBBENCHMARK_PAL_LOCK_PTHREAD_RWLOCK_GET_READ( baus->lock );

  switch( relative_position )
  {
    case LIBBENCHMARK_DATASTRUCTURE_BTREE_AU_PTHREAD_RWLOCK_RELATIVE_POSITION_UP:
      *baue = (*baue)->up;
    break;

    case LIBBENCHMARK_DATASTRUCTURE_BTREE_AU_PTHREAD_RWLOCK_RELATIVE_POSITION_LEFT:
      *baue = (*baue)->left;
    break;

    case LIBBENCHMARK_DATASTRUCTURE_BTREE_AU_PTHREAD_RWLOCK_RELATIVE_POSITION_RIGHT:
      *baue = (*baue)->right;
    break;

    case LIBBENCHMARK_DATASTRUCTURE_BTREE_AU_PTHREAD_RWLOCK_RELATIVE_POSITION_SMALLEST_ELEMENT_BELOW_CURRENT_ELEMENT:
      *baue = (*baue)->left;
      if( *baue != NULL )
        while( (*baue)->right != NULL )
          *baue = (*baue)->right;
    break;

    case LIBBENCHMARK_DATASTRUCTURE_BTREE_AU_PTHREAD_RWLOCK_RELATIVE_POSITION_LARGEST_ELEMENT_BELOW_CURRENT_ELEMENT:
      *baue = (*baue)->right;
      if( *baue != NULL )
        while( (*baue)->left != NULL )
          *baue = (*baue)->left;
    break;

    case LIBBENCHMARK_DATASTRUCTURE_BTREE_AU_PTHREAD_RWLOCK_RELATIVE_POSITION_NEXT_SMALLER_ELEMENT_IN_ENTIRE_TREE:
      libbenchmark_datastructure_btree_au_internal_inorder_walk_from_largest_get_next_smallest_element( baus, baue );
    break;

    case LIBBENCHMARK_DATASTRUCTURE_BTREE_AU_PTHREAD_RWLOCK_RELATIVE_POSITION_NEXT_LARGER_ELEMENT_IN_ENTIRE_TREE:
      libbenchmark_datastructure_btree_au_internal_inorder_walk_from_smallest_get_next_largest_element( baus, baue );
    break;
  }

  LIBBENCHMARK_PAL_LOCK_PTHREAD_RWLOCK_RELEASE( baus->lock );

  if( *baue == NULL )
    rv = 0;

  return rv;
}





/****************************************************************************/
#pragma warning( disable : 4100 )

static void libbenchmark_datastructure_btree_au_internal_inorder_walk_from_largest_get_next_smallest_element( struct libbenchmark_datastructure_btree_au_pthread_rwlock_state *baus, struct libbenchmark_datastructure_btree_au_pthread_rwlock_element **baue )
{
  enum libbenchmark_datastructure_btree_au_move
    action = LIBBENCHMARK_DATASTRUCTURE_BTREE_AU_MOVE_INVALID;

  enum flag
    finished_flag = LOWERED;

  struct libbenchmark_datastructure_btree_au_pthread_rwlock_element
    *left = NULL,
    *up = NULL,
    *up_left = NULL,
    *up_right = NULL;

  LFDS710_PAL_ASSERT( baus != NULL );
  LFDS710_PAL_ASSERT( baue != NULL );

  /* TRD : from any given element, the next smallest element is;
           1. if we have a left, it's the largest element on the right branch of our left child
           2. if we don't have a left, and we're on the right of our parent, then it's our parent
           3. if we don't have a left, and we're on the left of our parent or we have no parent,
              iterative up the tree until we find the first child who is on the right of its parent; then it's the parent
  */

  left = (*baue)->left;
  up = (*baue)->up;
  if( up != NULL )
  {
    up_left = (*baue)->up->left;
    up_right = (*baue)->up->right;
  }

  if( left != NULL )
    action = LIBBENCHMARK_DATASTRUCTURE_BTREE_AU_MOVE_LARGEST_FROM_LEFT_CHILD;

  if( left == NULL and up != NULL and up_right == *baue )
    action = LIBBENCHMARK_DATASTRUCTURE_BTREE_AU_MOVE_GET_PARENT;

  if( (left == NULL and up == NULL) or (up != NULL and up_left == *baue and left == NULL) )
    action = LIBBENCHMARK_DATASTRUCTURE_BTREE_AU_MOVE_MOVE_UP_TREE;

  switch( action )
  {
    case LIBBENCHMARK_DATASTRUCTURE_BTREE_AU_MOVE_INVALID:
    case LIBBENCHMARK_DATASTRUCTURE_BTREE_AU_MOVE_SMALLEST_FROM_RIGHT_CHILD:
      // TRD : eliminates a compiler warning
    break;

    case LIBBENCHMARK_DATASTRUCTURE_BTREE_AU_MOVE_LARGEST_FROM_LEFT_CHILD:
      *baue = left;
      if( *baue != NULL )
        while( (*baue)->right != NULL )
          *baue = (*baue)->right;
    break;

    case LIBBENCHMARK_DATASTRUCTURE_BTREE_AU_MOVE_GET_PARENT:
      *baue = up;
    break;

    case LIBBENCHMARK_DATASTRUCTURE_BTREE_AU_MOVE_MOVE_UP_TREE:
      while( finished_flag == LOWERED )
      {
        up = (*baue)->up;
        if( up != NULL )
          up_left = (*baue)->up->left;

        if( *baue != NULL and up != NULL and *baue == up_left )
          *baue = up;
        else
          finished_flag = RAISED;
      }

      *baue = up;
    break;
  }

  return;
}

#pragma warning( default : 4100 )





/****************************************************************************/
#pragma warning( disable : 4100 )

static void libbenchmark_datastructure_btree_au_internal_inorder_walk_from_smallest_get_next_largest_element( struct libbenchmark_datastructure_btree_au_pthread_rwlock_state *baus, struct libbenchmark_datastructure_btree_au_pthread_rwlock_element **baue )
{
  enum libbenchmark_datastructure_btree_au_move
    action = LIBBENCHMARK_DATASTRUCTURE_BTREE_AU_MOVE_INVALID;

  enum flag
    finished_flag = LOWERED;

  struct libbenchmark_datastructure_btree_au_pthread_rwlock_element
    *right = NULL,
    *up = NULL,
    *up_left = NULL,
    *up_right = NULL;

  LFDS710_PAL_ASSERT( baus != NULL );
  LFDS710_PAL_ASSERT( baue != NULL );

  right = (*baue)->right;
  up = (*baue)->up;
  if( up != NULL )
  {
    up_left = (*baue)->up->left;
    up_right = (*baue)->up->right;
  }

  if( right != NULL )
    action = LIBBENCHMARK_DATASTRUCTURE_BTREE_AU_MOVE_SMALLEST_FROM_RIGHT_CHILD;

  if( right == NULL and up != NULL and up_left == *baue )
    action = LIBBENCHMARK_DATASTRUCTURE_BTREE_AU_MOVE_GET_PARENT;

  if( (right == NULL and up == NULL) or (up != NULL and up_right == *baue and right == NULL) )
    action = LIBBENCHMARK_DATASTRUCTURE_BTREE_AU_MOVE_MOVE_UP_TREE;

  switch( action )
  {
    case LIBBENCHMARK_DATASTRUCTURE_BTREE_AU_MOVE_INVALID:
    case LIBBENCHMARK_DATASTRUCTURE_BTREE_AU_MOVE_LARGEST_FROM_LEFT_CHILD:
      // TRD : remove compiler warning
    break;

    case LIBBENCHMARK_DATASTRUCTURE_BTREE_AU_MOVE_SMALLEST_FROM_RIGHT_CHILD:
      *baue = right;
      if( *baue != NULL )
        while( (*baue)->left != NULL )
          *baue = (*baue)->left;
    break;

    case LIBBENCHMARK_DATASTRUCTURE_BTREE_AU_MOVE_GET_PARENT:
      *baue = up;
    break;

    case LIBBENCHMARK_DATASTRUCTURE_BTREE_AU_MOVE_MOVE_UP_TREE:
      while( finished_flag == LOWERED )
      {
        up = (*baue)->up;
        if( up != NULL )
          up_right = (*baue)->up->right;

        if( *baue != NULL and up != NULL and *baue == up_right )
          *baue = up;
        else
          finished_flag = RAISED;
      }

      *baue = up;
    break;
  }

  return;
}

#pragma warning( default : 4100 )





/****************************************************************************/
int libbenchmark_datastructure_btree_au_pthread_rwlock_get_by_absolute_position_and_then_by_relative_position( struct libbenchmark_datastructure_btree_au_pthread_rwlock_state *baus, struct libbenchmark_datastructure_btree_au_pthread_rwlock_element **baue, enum libbenchmark_datastructure_btree_au_pthread_rwlock_absolute_position absolute_position, enum libbenchmark_datastructure_btree_au_pthread_rwlock_relative_position relative_position )
{
  int
    rv;

  LFDS710_PAL_ASSERT( baus != NULL );
  LFDS710_PAL_ASSERT( baue != NULL );
  // TRD: absolute_position can be any value in its range
  // TRD: relative_position can be any value in its range

  if( *baue == NULL )
    rv = libbenchmark_datastructure_btree_au_pthread_rwlock_get_by_absolute_position_for_read( baus, baue, absolute_position );
  else
    rv = libbenchmark_datastructure_btree_au_pthread_rwlock_get_by_relative_position_for_read( baus, baue, relative_position );

  return rv;
}

