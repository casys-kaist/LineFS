/***** includes *****/
#include "libbenchmark_topology_internal.h"

/***** private prototypes *****/
static void libbenchmark_topology_internal_generate_thread_set_one_lp_per_lowest_level_cache( struct libbenchmark_topology_state *ts, struct libshared_memory_state *ms, struct lfds710_list_asu_state *lp_sets );
static void libbenchmark_topology_internal_generate_thread_set_one_to_all_lps_per_lowest_level_cache( struct libbenchmark_topology_state *ts, struct libshared_memory_state *ms, struct lfds710_list_asu_state *lp_sets );
static void libbenchmark_topology_internal_generate_thread_set_all_lps_per_lowest_level_cache( struct libbenchmark_topology_state *ts, struct libshared_memory_state *ms, struct lfds710_list_asu_state *lp_sets );
static void libbenchmark_topology_internal_generate_thread_set_all_lps( struct libbenchmark_topology_state *ts, struct libshared_memory_state *ms, struct lfds710_list_asu_state *lp_sets );





/****************************************************************************/
void libbenchmark_topology_generate_deduplicated_logical_processor_sets( struct libbenchmark_topology_state *ts, struct libshared_memory_state *ms, struct lfds710_list_asu_state *lp_sets )
{
  int
    cr;

  struct lfds710_list_asu_element
    *local_lasue = NULL,
    *lasue;

  struct lfds710_list_asu_state
    throw_lp_sets,
    local_lp_sets;

  struct libbenchmark_topology_logical_processor_set
    *local_lps,
    *lps,
    *new_lps;

  LFDS710_PAL_ASSERT( ts != NULL );
  LFDS710_PAL_ASSERT( ms != NULL );
  LFDS710_PAL_ASSERT( lp_sets != NULL );

  lfds710_list_asu_init_valid_on_current_logical_core( &throw_lp_sets, NULL );
  lfds710_list_asu_init_valid_on_current_logical_core( &local_lp_sets, NULL );

  // TRD : order is a useful hack - we want the full set to come last after deduplication, this order achieves this
  libbenchmark_topology_internal_generate_thread_set_one_lp_per_lowest_level_cache( ts, ms, &local_lp_sets );
  libbenchmark_topology_internal_generate_thread_set_all_lps_per_lowest_level_cache( ts, ms, &local_lp_sets );
  libbenchmark_topology_internal_generate_thread_set_one_to_all_lps_per_lowest_level_cache( ts, ms, &local_lp_sets );
  libbenchmark_topology_internal_generate_thread_set_all_lps( ts, ms, &throw_lp_sets );

  /*

  while( LFDS710_LIST_ASU_GET_START_AND_THEN_NEXT(local_lp_sets, local_lasue) )
  {
    char
      *lps_string;

    local_lps = LFDS710_LIST_ASU_GET_VALUE_FROM_ELEMENT( *local_lasue );

    benchmark_topology_construct_benchmark_logical_processor_set_string( t, local_lps, &lps_string );
    printf( "%s\n", lps_string );
  }

  exit( 1 );

  */

  /* TRD : now de-duplicate local_lp_sets
           dumbo algorithm, loop over every value in local_lp_sets
           and if not present in lp_sets, add to lp_sets

           algorithmically better to sort and then pass over once, removing duplicates
           however, this is not a coding test - it's real life - and in this case
           the extra coding work makes no sense at all
  */

  lfds710_list_asu_init_valid_on_current_logical_core( lp_sets, NULL );

  while( LFDS710_LIST_ASU_GET_START_AND_THEN_NEXT(local_lp_sets, local_lasue) )
  {
    local_lps = LFDS710_LIST_ASU_GET_VALUE_FROM_ELEMENT( *local_lasue );

    cr = !0;
    lasue = NULL;

    // TRD : exit loop if cr is 0, which means we found the set exists already
    while( cr != 0 and LFDS710_LIST_ASU_GET_START_AND_THEN_NEXT(*lp_sets, lasue) )
    {
      lps = LFDS710_LIST_ASU_GET_VALUE_FROM_ELEMENT( *lasue );
      cr = libbenchmark_topology_node_compare_lpsets_function( &local_lps->logical_processors, &lps->logical_processors );
    }

    // TRD : if we did NOT find this set already, then keep it
    if( cr != 0 )
    {
      new_lps = libshared_memory_alloc_from_most_free_space_node( ms, sizeof(struct libbenchmark_topology_logical_processor_set), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
      new_lps->logical_processors = local_lps->logical_processors;
      LFDS710_LIST_ASU_SET_VALUE_IN_ELEMENT( new_lps->lasue, new_lps );
      lfds710_list_asu_insert_at_end( lp_sets, &new_lps->lasue );
    }
  }

  lfds710_list_asu_cleanup( &local_lp_sets, NULL );

  return;
}





/****************************************************************************/
static void libbenchmark_topology_internal_generate_thread_set_one_lp_per_lowest_level_cache( struct libbenchmark_topology_state *ts, struct libshared_memory_state *ms, struct lfds710_list_asu_state *lp_sets )
{
  lfds710_pal_uint_t
    loop,
    lowest_level_type_count = 0,
    lps_count;

  struct lfds710_btree_au_element
    *be = NULL,
    *be_llc = NULL,
    *be_lp = NULL;

  struct libbenchmark_topology_logical_processor_set
    *lps;

  struct libbenchmark_topology_node_state
    *new_tns,
    *node = NULL,
    *node_llc = NULL,
    *node_lp = NULL;

  LFDS710_PAL_ASSERT( ts != NULL );
  LFDS710_PAL_ASSERT( ms != NULL );
  LFDS710_PAL_ASSERT( lp_sets != NULL );

  /* TRD : we find the lowest level cache type
           there are a certain number of these caches in the system
           each will service a certain number of logical processors

           we create one thread set per lowest level cache type,
           with the the first thread set having only the first lowest
           level cache and following sets adding one more lowest level
           cache at a time, until the final thread set has all the
           lowest level caches; for each lowest level cache in a thread set,
           that thread set has the first logical processor of each lowest
           level cache
  */

  // TRD : find lowest level memory, bus or cache
  while( lfds710_btree_au_get_by_absolute_position_and_then_by_relative_position(&ts->topology_tree, &be_llc, LFDS710_BTREE_AU_ABSOLUTE_POSITION_SMALLEST_IN_TREE, LFDS710_BTREE_AU_RELATIVE_POSITION_NEXT_LARGER_ELEMENT_IN_ENTIRE_TREE) )
  {
    node_llc = LFDS710_BTREE_AU_GET_VALUE_FROM_ELEMENT( *be_llc );

    if( node_llc->type == LIBBENCHMARK_TOPOLOGY_NODE_TYPE_SYSTEM or node_llc->type == LIBBENCHMARK_TOPOLOGY_NODE_TYPE_NUMA or node_llc->type == LIBBENCHMARK_TOPOLOGY_NODE_TYPE_CACHE )
      break;
  }

  // TRD : count the number of the lowest level type
  while( lfds710_btree_au_get_by_absolute_position_and_then_by_relative_position(&ts->topology_tree, &be, LFDS710_BTREE_AU_ABSOLUTE_POSITION_LARGEST_IN_TREE, LFDS710_BTREE_AU_RELATIVE_POSITION_NEXT_SMALLER_ELEMENT_IN_ENTIRE_TREE) )
  {
    node = LFDS710_BTREE_AU_GET_VALUE_FROM_ELEMENT( *be );

    if( 0 == libbenchmark_topology_node_compare_node_types_function(node, node_llc) )
      lowest_level_type_count++;
  }

  // TRD : create the thread sets
  for( loop = 0 ; loop < lowest_level_type_count ; loop++ )
  {
    /* TRD : find the first 0 to (loop+1) lowest level types
             add the smallest LP under that type to the thread set
    */

    lps = libshared_memory_alloc_from_most_free_space_node( ms, sizeof(struct libbenchmark_topology_logical_processor_set), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
    lfds710_list_aso_init_valid_on_current_logical_core( &lps->logical_processors, libbenchmark_topology_node_compare_nodes_function, LFDS710_LIST_ASO_INSERT_RESULT_FAILURE_EXISTING_KEY, NULL );
    lps_count = 0;
    be = NULL;

    while( lps_count < loop+1 and lfds710_btree_au_get_by_absolute_position_and_then_by_relative_position(&ts->topology_tree, &be, LFDS710_BTREE_AU_ABSOLUTE_POSITION_SMALLEST_IN_TREE, LFDS710_BTREE_AU_RELATIVE_POSITION_NEXT_LARGER_ELEMENT_IN_ENTIRE_TREE) )
    {
      node = LFDS710_BTREE_AU_GET_VALUE_FROM_ELEMENT( *be );

      // TRD : if we've found a lowest level type...
      if( 0 == libbenchmark_topology_node_compare_node_types_function(node, node_llc) )
      {
        // TRD : now use a temp copy of be and go LARGEST_TO_SMALLEST until we find an LP
        be_lp = be;

        while( lfds710_btree_au_get_by_relative_position(&be_lp, LFDS710_BTREE_AU_RELATIVE_POSITION_NEXT_SMALLER_ELEMENT_IN_ENTIRE_TREE) )
        {
          node_lp = LFDS710_BTREE_AU_GET_VALUE_FROM_ELEMENT( *be_lp );

          if( node_lp->type == LIBBENCHMARK_TOPOLOGY_NODE_TYPE_LOGICAL_PROCESSOR )
            break;
        }

        // TRD : now add LP
        new_tns = libshared_memory_alloc_from_most_free_space_node( ms, sizeof(struct libbenchmark_topology_node_state), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
        *new_tns = *node_lp;
        LFDS710_LIST_ASO_SET_KEY_IN_ELEMENT( new_tns->lasoe, new_tns );
        LFDS710_LIST_ASO_SET_VALUE_IN_ELEMENT( new_tns->lasoe, new_tns );
        lfds710_list_aso_insert( &lps->logical_processors, &new_tns->lasoe, NULL );
        lps_count++;
      }
    }

    LFDS710_LIST_ASU_SET_KEY_IN_ELEMENT( lps->lasue, lps );
    LFDS710_LIST_ASU_SET_VALUE_IN_ELEMENT( lps->lasue, lps );
    lfds710_list_asu_insert_at_end( lp_sets, &lps->lasue );
  }

  return;
}





/****************************************************************************/
static void libbenchmark_topology_internal_generate_thread_set_one_to_all_lps_per_lowest_level_cache( struct libbenchmark_topology_state *ts, struct libshared_memory_state *ms, struct lfds710_list_asu_state *lp_sets )
{
  lfds710_pal_uint_t
    loop,
    lowest_level_type_count = 0,
    lp_per_llc = 0,
    lp_count,
    lps_count;

  struct lfds710_btree_au_element
    *be = NULL,
    *be_llc = NULL,
    *be_lp = NULL;

  struct libbenchmark_topology_logical_processor_set
    *lps;

  struct libbenchmark_topology_node_state
    *new_lp,
    *node = NULL,
    *node_llc = NULL,
    *node_lp = NULL;

  LFDS710_PAL_ASSERT( ts != NULL );
  LFDS710_PAL_ASSERT( ms != NULL );
  LFDS710_PAL_ASSERT( lp_sets != NULL );

  /* TRD : we find the lowest level cache type
           there are a certain number of these caches in the system
           each will service a certain number of logical processors

           we create one thread set per logical processor under
           the lowest level type (they will have all have the same
           number of logical processors)

           each set contains an increasing number of LPs from each
           lowest level type, e.g. the first set has one LP from
           each lowest level type, the second has two, enode, until
         s  all LPs are in use
  */

  // TRD : find lowest level memory, bus or cache
  while( lfds710_btree_au_get_by_absolute_position_and_then_by_relative_position(&ts->topology_tree, &be_llc, LFDS710_BTREE_AU_ABSOLUTE_POSITION_SMALLEST_IN_TREE, LFDS710_BTREE_AU_RELATIVE_POSITION_NEXT_LARGER_ELEMENT_IN_ENTIRE_TREE) )
  {
    node_llc = LFDS710_BTREE_AU_GET_VALUE_FROM_ELEMENT( *be_llc );

    if( node_llc->type == LIBBENCHMARK_TOPOLOGY_NODE_TYPE_SYSTEM or node_llc->type == LIBBENCHMARK_TOPOLOGY_NODE_TYPE_NUMA or node_llc->type == LIBBENCHMARK_TOPOLOGY_NODE_TYPE_CACHE )
      break;
  }

  /* TRD : count the number of LPs under the lowest level type
           since be_llc points at the smallest of the lowest level types
           we will once we've counted all it's LPs naturally exit the tree
           since we're walking LFDS710_ADDONLY_UNBALANCED_BTREE_WALK_FROM_LARGEST_TO_SMALLEST
  */
  be_lp = be_llc;

  while( lfds710_btree_au_get_by_relative_position(&be_lp, LFDS710_BTREE_AU_RELATIVE_POSITION_NEXT_SMALLER_ELEMENT_IN_ENTIRE_TREE) )
  {
    node_lp = LFDS710_BTREE_AU_GET_VALUE_FROM_ELEMENT( *be_lp );

    if( node_lp->type == LIBBENCHMARK_TOPOLOGY_NODE_TYPE_LOGICAL_PROCESSOR )
      lp_per_llc++;
  }

  // TRD : count the number of the lowest level type
  while( lfds710_btree_au_get_by_absolute_position_and_then_by_relative_position(&ts->topology_tree, &be, LFDS710_BTREE_AU_ABSOLUTE_POSITION_LARGEST_IN_TREE, LFDS710_BTREE_AU_RELATIVE_POSITION_NEXT_SMALLER_ELEMENT_IN_ENTIRE_TREE) )
  {
    node = LFDS710_BTREE_AU_GET_VALUE_FROM_ELEMENT( *be );

    if( 0 == libbenchmark_topology_node_compare_node_types_function(node, node_llc) )
      lowest_level_type_count++;
  }

  // TRD : create the thread sets
  for( loop = 0 ; loop < lp_per_llc ; loop++ )
  {
    /* TRD : visit each lowest level type
             add from 0 to loop+1 of its LPs to the set
    */

    lps = libshared_memory_alloc_from_most_free_space_node( ms, sizeof(struct libbenchmark_topology_logical_processor_set), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
    lfds710_list_aso_init_valid_on_current_logical_core( &lps->logical_processors, libbenchmark_topology_node_compare_nodes_function, LFDS710_LIST_ASO_INSERT_RESULT_FAILURE_EXISTING_KEY, NULL );
    lps_count = 0;

    be = NULL;

    while( lps_count < lowest_level_type_count*(loop+1) and lfds710_btree_au_get_by_absolute_position_and_then_by_relative_position(&ts->topology_tree, &be, LFDS710_BTREE_AU_ABSOLUTE_POSITION_SMALLEST_IN_TREE, LFDS710_BTREE_AU_RELATIVE_POSITION_NEXT_LARGER_ELEMENT_IN_ENTIRE_TREE) )
    {
      node = LFDS710_BTREE_AU_GET_VALUE_FROM_ELEMENT( *be );

      // TRD : if we've found a lowest level type...
      if( 0 == libbenchmark_topology_node_compare_node_types_function(node, node_llc) )
      {
        // TRD : now use a temp copy of be and go LARGEST_TO_SMALLEST until we have (loop+1) LPs
        be_lp = be;
        lp_count = 0;

        while( lp_count < loop+1 and lfds710_btree_au_get_by_relative_position(&be_lp, LFDS710_BTREE_AU_RELATIVE_POSITION_NEXT_SMALLER_ELEMENT_IN_ENTIRE_TREE) )
        {
          node_lp = LFDS710_BTREE_AU_GET_VALUE_FROM_ELEMENT( *be_lp );

          if( node_lp->type == LIBBENCHMARK_TOPOLOGY_NODE_TYPE_LOGICAL_PROCESSOR )
          {
            new_lp = libshared_memory_alloc_from_most_free_space_node( ms, sizeof(struct libbenchmark_topology_node_state), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
            *new_lp = *node_lp;
            LFDS710_LIST_ASO_SET_KEY_IN_ELEMENT( new_lp->lasoe, new_lp );
            LFDS710_LIST_ASO_SET_VALUE_IN_ELEMENT( new_lp->lasoe, new_lp );
            lfds710_list_aso_insert( &lps->logical_processors, &new_lp->lasoe, NULL );
            lp_count++;
            lps_count++;
          }
        }
      }
    }

    LFDS710_LIST_ASU_SET_KEY_IN_ELEMENT( lps->lasue, lps );
    LFDS710_LIST_ASU_SET_VALUE_IN_ELEMENT( lps->lasue, lps );
    lfds710_list_asu_insert_at_end( lp_sets, &lps->lasue );
  }

  return;
}





/****************************************************************************/
static void libbenchmark_topology_internal_generate_thread_set_all_lps_per_lowest_level_cache( struct libbenchmark_topology_state *ts, struct libshared_memory_state *ms, struct lfds710_list_asu_state *lp_sets )
{
  lfds710_pal_uint_t
    loop,
    lowest_level_type_count = 0,
    lp_per_llc = 0,
    lps_count;

  struct lfds710_btree_au_element
    *be = NULL,
    *be_llc = NULL,
    *be_lp = NULL;

  struct libbenchmark_topology_logical_processor_set
    *lps;

  struct libbenchmark_topology_node_state
    *new_lp,
    *node = NULL,
    *node_llc = NULL,
    *node_lp = NULL;

  LFDS710_PAL_ASSERT( ts != NULL );
  LFDS710_PAL_ASSERT( ms != NULL );
  LFDS710_PAL_ASSERT( lp_sets != NULL );

  /* TRD : we find the lowest level cache type
           there are a certain number of these caches in the system
           each will service a certain number of logical processors

           each lowest level type has a given number of logical processors
           (the number is the same for each lowest level type)
           we create one thread set per lowest level type,
           where we're adding in full blocks of lowest level type LPs,
           e.g. the first set has all the LPs of the first lowest level
           type, the second set has all the LPs of the first and second
           lowest level types, enode
  */

  // TRD : find lowest level memory, bus or cache
  while( lfds710_btree_au_get_by_absolute_position_and_then_by_relative_position(&ts->topology_tree, &be_llc, LFDS710_BTREE_AU_ABSOLUTE_POSITION_SMALLEST_IN_TREE, LFDS710_BTREE_AU_RELATIVE_POSITION_NEXT_LARGER_ELEMENT_IN_ENTIRE_TREE) )
  {
    node_llc = LFDS710_BTREE_AU_GET_VALUE_FROM_ELEMENT( *be_llc );

    if( node_llc->type == LIBBENCHMARK_TOPOLOGY_NODE_TYPE_SYSTEM or node_llc->type == LIBBENCHMARK_TOPOLOGY_NODE_TYPE_NUMA or node_llc->type == LIBBENCHMARK_TOPOLOGY_NODE_TYPE_CACHE )
      break;
  }

  /* TRD : count the number of LPs under the lowest level type
           since be_llc points at the smallest of the lowest level types
           we will once we've counted all it's LPs naturally exit the tree
           since we're walking LFDS710_ADDONLY_UNBALANCED_BTREE_WALK_FROM_LARGEST_TO_SMALLEST
  */
  be_lp = be_llc;

  while( lfds710_btree_au_get_by_relative_position(&be_lp, LFDS710_BTREE_AU_RELATIVE_POSITION_NEXT_SMALLER_ELEMENT_IN_ENTIRE_TREE) )
  {
    node_lp = LFDS710_BTREE_AU_GET_VALUE_FROM_ELEMENT( *be_lp );

    if( node_lp->type == LIBBENCHMARK_TOPOLOGY_NODE_TYPE_LOGICAL_PROCESSOR )
      lp_per_llc++;
  }

  // TRD : count the number of the lowest level type
  while( lfds710_btree_au_get_by_absolute_position_and_then_by_relative_position(&ts->topology_tree, &be, LFDS710_BTREE_AU_ABSOLUTE_POSITION_LARGEST_IN_TREE, LFDS710_BTREE_AU_RELATIVE_POSITION_NEXT_SMALLER_ELEMENT_IN_ENTIRE_TREE) )
  {
    node = LFDS710_BTREE_AU_GET_VALUE_FROM_ELEMENT( *be );

    if( 0 == libbenchmark_topology_node_compare_node_types_function(node, node_llc) )
      lowest_level_type_count++;
  }

  // TRD : create the thread sets
  for( loop = 0 ; loop < lowest_level_type_count ; loop++ )
  {
    /* TRD : find the first 0 to (loop+1) lowest level types
             add all LPs under those lowest level types to the set
    */

    lps = libshared_memory_alloc_from_most_free_space_node( ms, sizeof(struct libbenchmark_topology_logical_processor_set), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
    lfds710_list_aso_init_valid_on_current_logical_core( &lps->logical_processors, libbenchmark_topology_node_compare_nodes_function, LFDS710_LIST_ASO_INSERT_RESULT_FAILURE_EXISTING_KEY, NULL );
    lps_count = 0;
    be = NULL;

    while( lps_count < lp_per_llc*(loop+1) and lfds710_btree_au_get_by_absolute_position_and_then_by_relative_position(&ts->topology_tree, &be, LFDS710_BTREE_AU_ABSOLUTE_POSITION_SMALLEST_IN_TREE, LFDS710_BTREE_AU_RELATIVE_POSITION_NEXT_LARGER_ELEMENT_IN_ENTIRE_TREE) )
    {
      node = LFDS710_BTREE_AU_GET_VALUE_FROM_ELEMENT( *be );

      // TRD : if we've found a lowest level type...
      if( 0 == libbenchmark_topology_node_compare_node_types_function(node, node_llc) )
      {
        // TRD : now use a temp copy of be and go LARGEST_TO_SMALLEST until we exit the tree or find another lowest level type
        be_lp = be;

        while( lfds710_btree_au_get_by_relative_position(&be_lp, LFDS710_BTREE_AU_RELATIVE_POSITION_NEXT_SMALLER_ELEMENT_IN_ENTIRE_TREE) )
        {
          node_lp = LFDS710_BTREE_AU_GET_VALUE_FROM_ELEMENT( *be_lp );

          if( node_lp->type == LIBBENCHMARK_TOPOLOGY_NODE_TYPE_LOGICAL_PROCESSOR )
          {
            // TRD : now add LP
            new_lp = libshared_memory_alloc_from_most_free_space_node( ms, sizeof(struct libbenchmark_topology_node_state), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
            *new_lp = *node_lp;
            LFDS710_LIST_ASO_SET_KEY_IN_ELEMENT( new_lp->lasoe, new_lp );
            LFDS710_LIST_ASO_SET_VALUE_IN_ELEMENT( new_lp->lasoe, new_lp );
            lfds710_list_aso_insert( &lps->logical_processors, &new_lp->lasoe, NULL );
            lps_count++;
          }

          if( node_lp->type == node_llc->type )
            break;
        }
      }
    }

    LFDS710_LIST_ASU_SET_KEY_IN_ELEMENT( lps->lasue, lps );
    LFDS710_LIST_ASU_SET_VALUE_IN_ELEMENT( lps->lasue, lps );
    lfds710_list_asu_insert_at_end( lp_sets, &lps->lasue );
  }

  return;
}





/****************************************************************************/
static void libbenchmark_topology_internal_generate_thread_set_all_lps( struct libbenchmark_topology_state *ts, struct libshared_memory_state *ms, struct lfds710_list_asu_state *lp_sets )
{
  struct lfds710_btree_au_element
    *be = NULL;

  struct libbenchmark_topology_logical_processor_set
    *lps;

  struct libbenchmark_topology_node_state
    *new_lp,
    *node;

  LFDS710_PAL_ASSERT( ts != NULL );
  LFDS710_PAL_ASSERT( ms != NULL );
  LFDS710_PAL_ASSERT( lp_sets != NULL );

  lps = libshared_memory_alloc_from_most_free_space_node( ms, sizeof(struct libbenchmark_topology_logical_processor_set), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
  lfds710_list_aso_init_valid_on_current_logical_core( &lps->logical_processors, libbenchmark_topology_node_compare_nodes_function, LFDS710_LIST_ASO_INSERT_RESULT_FAILURE_EXISTING_KEY, NULL );

  // TRD : iterate over tree - add in every logical processor
  while( lfds710_btree_au_get_by_absolute_position_and_then_by_relative_position(&ts->topology_tree, &be, LFDS710_BTREE_AU_ABSOLUTE_POSITION_SMALLEST_IN_TREE, LFDS710_BTREE_AU_RELATIVE_POSITION_NEXT_LARGER_ELEMENT_IN_ENTIRE_TREE) )
  {
    node = LFDS710_BTREE_AU_GET_VALUE_FROM_ELEMENT( *be );

    if( node->type == LIBBENCHMARK_TOPOLOGY_NODE_TYPE_LOGICAL_PROCESSOR )
    {
      // TRD : now add LP
      new_lp = libshared_memory_alloc_from_most_free_space_node( ms, sizeof(struct libbenchmark_topology_node_state), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
      *new_lp = *node;
      LFDS710_LIST_ASO_SET_KEY_IN_ELEMENT( new_lp->lasoe, new_lp );
      LFDS710_LIST_ASO_SET_VALUE_IN_ELEMENT( new_lp->lasoe, new_lp );
      lfds710_list_aso_insert( &lps->logical_processors, &new_lp->lasoe, NULL );
    }
  }

  LFDS710_LIST_ASU_SET_KEY_IN_ELEMENT( lps->lasue, lps );
  LFDS710_LIST_ASU_SET_VALUE_IN_ELEMENT( lps->lasue, lps );
  lfds710_list_asu_insert_at_end( lp_sets, &lps->lasue );

  return;
}

