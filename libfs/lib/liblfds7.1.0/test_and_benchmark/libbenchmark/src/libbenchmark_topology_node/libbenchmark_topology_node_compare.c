/***** includes *****/
#include "libbenchmark_topology_node_internal.h"

/***** enums *****/
enum libbenchmark_topology_node_set_type
{
  LIBBENCHMARK_TOPOLOGY_NODE_SET_TYPE_SUBSET,
  LIBBENCHMARK_TOPOLOGY_NODE_SET_TYPE_SET,
  LIBBENCHMARK_TOPOLOGY_NODE_SET_TYPE_SUPERSET,
  LIBBENCHMARK_TOPOLOGY_NODE_SET_TYPE_NOT_A_SET
};





/****************************************************************************/
int libbenchmark_topology_node_compare_nodes_function( void const *new_key, void const *existing_key )
{
  enum flag
    finished_flag = LOWERED,
    not_a_set_flag = LOWERED;

  enum libbenchmark_topology_node_set_type
    st = LIBBENCHMARK_TOPOLOGY_NODE_SET_TYPE_NOT_A_SET;

  int
    cr = 0,
    not_a_set_compare = 0, // TRD : remove compiler warning
    rv = 0;

  lfds710_pal_uint_t
    lps_one_count = 0,
    lps_two_count = 0,
    shared_count = 0;

  struct lfds710_list_aso_element
    *lasoe_one,
    *lasoe_two;

  struct libbenchmark_topology_node_state
    *new_tns,
    *existing_tns,
    *tns_one,
    *tns_two;

  LFDS710_PAL_ASSERT( new_key != NULL );
  LFDS710_PAL_ASSERT( existing_key != NULL );

  /* TRD : we compare the range of logic processors serviced by the nodes
           the basic rule for cache/bus/memory arrangment is that the number of logical processors supported by a node
           remains the same width or gets wider, as we go up the tree

           so, first, we compare the logical processor list of each node

           if the set in new_node is a subset of existing_node, then we are less than
           if the set in new_node is the set of existing_node, then we compare on type (we know the ordering of types)
           if the set in new_node is a superset of existing_node, then we are greater than
           if the set in new_node is not a subset, set or superset of existing_node then we scan the logical processor
           numbers and the first of new_node or existing_node to -have- the lowest number the other does -not-, is less than
  */

  new_tns = (struct libbenchmark_topology_node_state *) new_key;
  existing_tns = (struct libbenchmark_topology_node_state *) existing_key;

  if( new_tns->type == LIBBENCHMARK_TOPOLOGY_NODE_TYPE_LOGICAL_PROCESSOR and existing_tns->type == LIBBENCHMARK_TOPOLOGY_NODE_TYPE_LOGICAL_PROCESSOR )
  {
    if( new_tns->extended_node_info.logical_processor.windows_group_number == existing_tns->extended_node_info.logical_processor.windows_group_number )
    {
      if( new_tns->extended_node_info.logical_processor.number > existing_tns->extended_node_info.logical_processor.number )
        cr = 1;

      if( new_tns->extended_node_info.logical_processor.number < existing_tns->extended_node_info.logical_processor.number )
        cr = -1;
    }

    if( new_tns->extended_node_info.logical_processor.windows_group_number < existing_tns->extended_node_info.logical_processor.windows_group_number )
        cr = -1;

    if( new_tns->extended_node_info.logical_processor.windows_group_number > existing_tns->extended_node_info.logical_processor.windows_group_number )
        cr = 1;

    if( cr == -1 )
      not_a_set_compare = -1;

    if( cr == 1 )
      not_a_set_compare = 1;

    if( cr == 0 )
      shared_count = 1;

    lps_one_count = lps_two_count = 1;
  }

  if( new_tns->type == LIBBENCHMARK_TOPOLOGY_NODE_TYPE_LOGICAL_PROCESSOR and existing_tns->type != LIBBENCHMARK_TOPOLOGY_NODE_TYPE_LOGICAL_PROCESSOR )
  {
    tns_one = new_tns;

    lasoe_two = LFDS710_LIST_ASO_GET_START( existing_tns->logical_processor_children );

    while( lasoe_two != NULL and finished_flag == LOWERED )
    {
      tns_two = LFDS710_LIST_ASO_GET_VALUE_FROM_ELEMENT( *lasoe_two );

      cr = 0;

      if( tns_one->extended_node_info.logical_processor.windows_group_number == tns_two->extended_node_info.logical_processor.windows_group_number )
      {
        if( tns_one->extended_node_info.logical_processor.number > tns_two->extended_node_info.logical_processor.number )
          cr = 1;

        if( tns_one->extended_node_info.logical_processor.number < tns_two->extended_node_info.logical_processor.number )
          cr = -1;
      }

      if( tns_one->extended_node_info.logical_processor.windows_group_number < tns_two->extended_node_info.logical_processor.windows_group_number )
          cr = -1;

      if( tns_one->extended_node_info.logical_processor.windows_group_number > tns_two->extended_node_info.logical_processor.windows_group_number )
          cr = 1;

      if( cr == -1 )
      {
        finished_flag = RAISED;
        if( not_a_set_flag == LOWERED )
        {
          not_a_set_compare = -1;
          not_a_set_flag = RAISED;
        }
      }

      if( cr == 1 )
      {
        lasoe_two = LFDS710_LIST_ASO_GET_NEXT( *lasoe_two );
        if( not_a_set_flag == LOWERED )
        {
          not_a_set_compare = 1;
          not_a_set_flag = RAISED;
        }
      }

      if( cr == 0 )
      {
        shared_count++;
        finished_flag = RAISED;
      }
    }

    lps_one_count = 1;
    lfds710_list_aso_query( &existing_tns->logical_processor_children, LFDS710_LIST_ASO_QUERY_GET_POTENTIALLY_INACCURATE_COUNT, NULL, (void **) &lps_two_count );
  }

  if( new_tns->type != LIBBENCHMARK_TOPOLOGY_NODE_TYPE_LOGICAL_PROCESSOR and existing_tns->type == LIBBENCHMARK_TOPOLOGY_NODE_TYPE_LOGICAL_PROCESSOR )
  {
    tns_two = existing_tns;

    lasoe_one = LFDS710_LIST_ASO_GET_START( new_tns->logical_processor_children );

    while( lasoe_one != NULL and finished_flag == LOWERED )
    {
      tns_one = LFDS710_LIST_ASO_GET_VALUE_FROM_ELEMENT( *lasoe_one );

      cr = 0;

      if( tns_one->extended_node_info.logical_processor.windows_group_number == tns_two->extended_node_info.logical_processor.windows_group_number )
      {
        if( tns_one->extended_node_info.logical_processor.number > tns_two->extended_node_info.logical_processor.number )
          cr = 1;

        if( tns_one->extended_node_info.logical_processor.number < tns_two->extended_node_info.logical_processor.number )
          cr = -1;
      }

      if( tns_one->extended_node_info.logical_processor.windows_group_number < tns_two->extended_node_info.logical_processor.windows_group_number )
          cr = -1;

      if( tns_one->extended_node_info.logical_processor.windows_group_number > tns_two->extended_node_info.logical_processor.windows_group_number )
          cr = 1;

      if( cr == -1 )
      {
        lasoe_one = LFDS710_LIST_ASO_GET_NEXT( *lasoe_one );
        if( not_a_set_flag == LOWERED )
        {
          not_a_set_compare = -1;
          not_a_set_flag = RAISED;
        }
      }

      if( cr == 1 )
      {
        finished_flag = RAISED;
        if( not_a_set_flag == LOWERED )
        {
          not_a_set_compare = 1;
          not_a_set_flag = RAISED;
        }
      }

      if( cr == 0 )
      {
        shared_count++;
        finished_flag = RAISED;
      }
    }

    lfds710_list_aso_query( &new_tns->logical_processor_children, LFDS710_LIST_ASO_QUERY_GET_POTENTIALLY_INACCURATE_COUNT, NULL, (void **) &lps_one_count );
    lps_two_count = 1;
  }

  if( new_tns->type != LIBBENCHMARK_TOPOLOGY_NODE_TYPE_LOGICAL_PROCESSOR and existing_tns->type != LIBBENCHMARK_TOPOLOGY_NODE_TYPE_LOGICAL_PROCESSOR )
  {
    // TRD : count the number of shared logical processors
    lasoe_one = LFDS710_LIST_ASO_GET_START( new_tns->logical_processor_children );
    lasoe_two = LFDS710_LIST_ASO_GET_START( existing_tns->logical_processor_children );

    while( lasoe_one != NULL and lasoe_two != NULL )
    {
      tns_one = LFDS710_LIST_ASO_GET_VALUE_FROM_ELEMENT( *lasoe_one );
      tns_two = LFDS710_LIST_ASO_GET_VALUE_FROM_ELEMENT( *lasoe_two );

      cr = 0;

      if( tns_one->extended_node_info.logical_processor.windows_group_number == tns_two->extended_node_info.logical_processor.windows_group_number )
      {
        if( tns_one->extended_node_info.logical_processor.number > tns_two->extended_node_info.logical_processor.number )
          cr = 1;

        if( tns_one->extended_node_info.logical_processor.number < tns_two->extended_node_info.logical_processor.number )
          cr = -1;
      }

      if( tns_one->extended_node_info.logical_processor.windows_group_number < tns_two->extended_node_info.logical_processor.windows_group_number )
          cr = -1;

      if( tns_one->extended_node_info.logical_processor.windows_group_number > tns_two->extended_node_info.logical_processor.windows_group_number )
          cr = 1;

      if( cr == -1 )
      {
        lasoe_one = LFDS710_LIST_ASO_GET_NEXT( *lasoe_one );
        if( not_a_set_flag == LOWERED )
        {
          not_a_set_compare = -1;
          not_a_set_flag = RAISED;
        }
      }

      if( cr == 1 )
      {
        lasoe_two = LFDS710_LIST_ASO_GET_NEXT( *lasoe_two );
        if( not_a_set_flag == LOWERED )
        {
          not_a_set_compare = 1;
          not_a_set_flag = RAISED;
        }
      }

      if( cr == 0 )
      {
        shared_count++;
        lasoe_one = LFDS710_LIST_ASO_GET_NEXT( *lasoe_one );
        lasoe_two = LFDS710_LIST_ASO_GET_NEXT( *lasoe_two );
      }
    }

    lfds710_list_aso_query( &new_tns->logical_processor_children, LFDS710_LIST_ASO_QUERY_GET_POTENTIALLY_INACCURATE_COUNT, NULL, (void **) &lps_one_count );
    lfds710_list_aso_query( &existing_tns->logical_processor_children, LFDS710_LIST_ASO_QUERY_GET_POTENTIALLY_INACCURATE_COUNT, NULL, (void **) &lps_two_count );
  }

  // TRD : same number of logical processors, and they're fully shared
  if( lps_one_count == lps_two_count and shared_count == lps_one_count )
    st = LIBBENCHMARK_TOPOLOGY_NODE_SET_TYPE_SET;

  // TRD : smaller number of logical processors, but they're all shared
  if( lps_one_count < lps_two_count and shared_count == lps_one_count )
    st = LIBBENCHMARK_TOPOLOGY_NODE_SET_TYPE_SUBSET;

  // TRD : larger number of logical processors, but lps_two is fully represented in lps_one
  if( lps_one_count > lps_two_count and shared_count == lps_two_count )
    st = LIBBENCHMARK_TOPOLOGY_NODE_SET_TYPE_SUPERSET;

  // TRD : otherwise, we're LIBBENCHMARK_TOPOLOGY_NODE_SET_TYPE_NOT_A_SET, which is the default value

  switch( st )
  {
    case LIBBENCHMARK_TOPOLOGY_NODE_SET_TYPE_SUBSET:
      rv = -1;
    break;

    case LIBBENCHMARK_TOPOLOGY_NODE_SET_TYPE_SET:
      rv = libbenchmark_topology_node_compare_node_types_function( new_tns, existing_tns );
    break;

    case LIBBENCHMARK_TOPOLOGY_NODE_SET_TYPE_SUPERSET:
      rv = 1;
    break;

    case LIBBENCHMARK_TOPOLOGY_NODE_SET_TYPE_NOT_A_SET:
      rv = not_a_set_compare;
    break;
  }

  return rv;
}





/****************************************************************************/
int libbenchmark_topology_node_compare_node_types_function( void const *new_key, void const *existing_key )
{
  int
    rv = 0;

  struct libbenchmark_topology_node_state
    *new_tns,
    *existing_tns;

  LFDS710_PAL_ASSERT( new_key != NULL );
  LFDS710_PAL_ASSERT( existing_key != NULL );

  new_tns = (struct libbenchmark_topology_node_state *) new_key;
  existing_tns = (struct libbenchmark_topology_node_state *) existing_key;

  if( new_tns->type < existing_tns->type )
    return -1;

  if( new_tns->type > existing_tns->type )
    return 1;

  if( new_tns->type == LIBBENCHMARK_TOPOLOGY_NODE_TYPE_CACHE and existing_tns->type == LIBBENCHMARK_TOPOLOGY_NODE_TYPE_CACHE )
  {
    if( new_tns->extended_node_info.cache.level > existing_tns->extended_node_info.cache.level )
      rv = 1;

    if( new_tns->extended_node_info.cache.level < existing_tns->extended_node_info.cache.level )
      rv = -1;

    if( new_tns->extended_node_info.cache.level == existing_tns->extended_node_info.cache.level )
    {
      if( new_tns->extended_node_info.cache.type > existing_tns->extended_node_info.cache.type )
        rv = 1;

      if( new_tns->extended_node_info.cache.type < existing_tns->extended_node_info.cache.type )
        rv = -1;
    }
  }

  return rv;
}





/****************************************************************************/
int libbenchmark_topology_node_compare_lpsets_function( struct lfds710_list_aso_state *lpset_one, struct lfds710_list_aso_state *lpset_two )
{
  enum flag
    not_a_set_flag = LOWERED;

  enum libbenchmark_topology_node_set_type
    st = LIBBENCHMARK_TOPOLOGY_NODE_SET_TYPE_NOT_A_SET;

  int
    cr = 0,
    rv = 0,
    not_a_set_compare = 0;

  lfds710_pal_uint_t
    lps_one_count,
    lps_two_count,
    shared_count = 0;

  struct lfds710_list_aso_element
    *lasoe_one,
    *lasoe_two;

  struct libbenchmark_topology_node_state
    *tns_one,
    *tns_two;

  LFDS710_PAL_ASSERT( lpset_one != NULL );
  LFDS710_PAL_ASSERT( lpset_two != NULL );

  /* TRD : this function is utterly annoying
           it is word for word identical to one of the compare cases in the general topology node compare function
           except the compare result for IS_A_SET is 0 rather than a call to the type comparer
           and yet - !
           I cannot factorize the code with the general topology node compare function
           ahhhhhhh
           this function is used only by the compare function in the results API
  */

  // TRD : first, count the number of shared logical processors
  lasoe_one = LFDS710_LIST_ASO_GET_START( *lpset_one );
  lasoe_two = LFDS710_LIST_ASO_GET_START( *lpset_two );

  while( lasoe_one != NULL and lasoe_two != NULL )
  {
    tns_one = LFDS710_LIST_ASO_GET_VALUE_FROM_ELEMENT( *lasoe_one );
    tns_two = LFDS710_LIST_ASO_GET_VALUE_FROM_ELEMENT( *lasoe_two );

    cr = 0;

    if( tns_one->extended_node_info.logical_processor.windows_group_number == tns_two->extended_node_info.logical_processor.windows_group_number )
    {
      if( tns_one->extended_node_info.logical_processor.number > tns_two->extended_node_info.logical_processor.number )
        cr = 1;

      if( tns_one->extended_node_info.logical_processor.number < tns_two->extended_node_info.logical_processor.number )
        cr = -1;
    }

    if( tns_one->extended_node_info.logical_processor.windows_group_number < tns_two->extended_node_info.logical_processor.windows_group_number )
        cr = -1;

    if( tns_one->extended_node_info.logical_processor.windows_group_number > tns_two->extended_node_info.logical_processor.windows_group_number )
        cr = 1;

    if( cr == -1 )
    {
      lasoe_one = LFDS710_LIST_ASO_GET_NEXT( *lasoe_one );
      if( not_a_set_flag == LOWERED )
      {
        not_a_set_compare = -1;
        not_a_set_flag = RAISED;
      }
    }

    if( cr == 1 )
    {
      lasoe_two = LFDS710_LIST_ASO_GET_NEXT( *lasoe_two );
      if( not_a_set_flag == LOWERED )
      {
        not_a_set_compare = 1;
        not_a_set_flag = RAISED;
      }
    }

    if( cr == 0 )
    {
      shared_count++;
      lasoe_one = LFDS710_LIST_ASO_GET_NEXT( *lasoe_one );
      lasoe_two = LFDS710_LIST_ASO_GET_NEXT( *lasoe_two );
    }
  }

  lfds710_list_aso_query( lpset_one, LFDS710_LIST_ASO_QUERY_GET_POTENTIALLY_INACCURATE_COUNT, NULL, (void **) &lps_one_count );
  lfds710_list_aso_query( lpset_two, LFDS710_LIST_ASO_QUERY_GET_POTENTIALLY_INACCURATE_COUNT, NULL, (void **) &lps_two_count );

  // TRD : same number of logical processors, and they're fully shared
  if( lps_one_count == lps_two_count and shared_count == lps_one_count )
    st = LIBBENCHMARK_TOPOLOGY_NODE_SET_TYPE_SET;

  // TRD : smaller number of logical processors, but they're all shared
  if( lps_one_count < lps_two_count and shared_count == lps_one_count )
    st = LIBBENCHMARK_TOPOLOGY_NODE_SET_TYPE_SUBSET;

  // TRD : larger number of logical processors, but lps_two is fully represented in lps_one
  if( lps_one_count > lps_two_count and shared_count == lps_two_count )
    st = LIBBENCHMARK_TOPOLOGY_NODE_SET_TYPE_SUPERSET;

  switch( st )
  {
    case LIBBENCHMARK_TOPOLOGY_NODE_SET_TYPE_SUBSET:
      rv = -1;
    break;

    case LIBBENCHMARK_TOPOLOGY_NODE_SET_TYPE_SET:
      rv = 0;
    break;

    case LIBBENCHMARK_TOPOLOGY_NODE_SET_TYPE_SUPERSET:
      rv = 1;
    break;

    case LIBBENCHMARK_TOPOLOGY_NODE_SET_TYPE_NOT_A_SET:
      rv = not_a_set_compare;
    break;
  }

  return rv;
}

