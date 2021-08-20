/***** includes *****/
#include "libbenchmark_misc_internal.h"





/****************************************************************************/
void libbenchmark_misc_pal_helper_new_topology_node( struct libbenchmark_topology_node_state **tns,
                                                     struct libshared_memory_state *ms )
{
  LFDS710_PAL_ASSERT( tns != NULL );
  LFDS710_PAL_ASSERT( ms != NULL );

  *tns = libshared_memory_alloc_from_unknown_node( ms, sizeof(struct libbenchmark_topology_node_state), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );

  libbenchmark_topology_node_init( *tns );

  return;
}





/****************************************************************************/
void libbenchmark_misc_pal_helper_add_logical_processor_to_topology_node( struct libbenchmark_topology_node_state *tns,
                                                                          struct libshared_memory_state *ms,
                                                                          lfds710_pal_uint_t logical_processor_number,
                                                                          enum flag windows_processor_group_inuse_flag,
                                                                          lfds710_pal_uint_t windows_processor_group_number )
{
  struct libbenchmark_topology_node_state
    *tns_temp;

  LFDS710_PAL_ASSERT( tns != NULL );
  LFDS710_PAL_ASSERT( ms != NULL );
  // TRD : logical_processor_number can be any value in its range
  // TRD : windows_processor_group_inuse_flag can be any value in its range
  // TRD : windows_processor_group_number can be any value in its range

  libbenchmark_misc_pal_helper_new_topology_node( &tns_temp, ms );

  LIBBENCHMARK_TOPOLOGY_NODE_SET_TYPE( *tns_temp, LIBBENCHMARK_TOPOLOGY_NODE_TYPE_LOGICAL_PROCESSOR );
  LIBBENCHMARK_TOPOLOGY_NODE_SET_LOGICAL_PROCESSOR_NUMBER( *tns_temp, logical_processor_number );

  if( windows_processor_group_inuse_flag == RAISED )
   LIBBENCHMARK_TOPOLOGY_NODE_SET_WINDOWS_GROUP_NUMBER( *tns_temp, windows_processor_group_number );
  else
    LIBBENCHMARK_TOPOLOGY_NODE_UNSET_WINDOWS_GROUP_NUMBER( *tns_temp );

  LFDS710_LIST_ASO_SET_KEY_IN_ELEMENT( tns_temp->lasoe, tns_temp );
  LFDS710_LIST_ASO_SET_VALUE_IN_ELEMENT( tns_temp->lasoe, tns_temp );
  lfds710_list_aso_insert( &tns->logical_processor_children, &tns_temp->lasoe, NULL );

  return;
}





/****************************************************************************/
void libbenchmark_misc_pal_helper_add_system_node_to_topology_tree( struct libbenchmark_topology_state *ts,
                                                                    struct libbenchmark_topology_node_state *tns )
{
  LFDS710_PAL_ASSERT( ts != NULL );
  LFDS710_PAL_ASSERT( tns != NULL );

  LIBBENCHMARK_TOPOLOGY_NODE_SET_TYPE( *tns, LIBBENCHMARK_TOPOLOGY_NODE_TYPE_SYSTEM );

  libbenchmark_topology_insert( ts, tns );

  return;
}





/****************************************************************************/
void libbenchmark_misc_pal_helper_add_numa_node_to_topology_tree( struct libbenchmark_topology_state *ts,
                                                                  struct libbenchmark_topology_node_state *tns,
                                                                  lfds710_pal_uint_t numa_node_id )
{
  LFDS710_PAL_ASSERT( ts != NULL );
  LFDS710_PAL_ASSERT( tns != NULL );
  // TRD : numa_node_id can be NULL

  LIBBENCHMARK_TOPOLOGY_NODE_SET_TYPE( *tns, LIBBENCHMARK_TOPOLOGY_NODE_TYPE_NUMA );
  LIBBENCHMARK_TOPOLOGY_NODE_SET_NUMA_ID( *tns, numa_node_id );

  libbenchmark_topology_insert( ts, tns );

  return;
}





/****************************************************************************/
void libbenchmark_misc_pal_helper_add_socket_node_to_topology_tree( struct libbenchmark_topology_state *ts,
                                                                    struct libbenchmark_topology_node_state *tns )
{
  LFDS710_PAL_ASSERT( ts != NULL );
  LFDS710_PAL_ASSERT( tns != NULL );

  LIBBENCHMARK_TOPOLOGY_NODE_SET_TYPE( *tns, LIBBENCHMARK_TOPOLOGY_NODE_TYPE_SOCKET );

  libbenchmark_topology_insert( ts, tns );

  return;
}





/****************************************************************************/
void libbenchmark_misc_pal_helper_add_physical_processor_node_to_topology_tree( struct libbenchmark_topology_state *ts,
                                                                                struct libbenchmark_topology_node_state *tns )
{
  LFDS710_PAL_ASSERT( ts != NULL );
  LFDS710_PAL_ASSERT( tns != NULL );

  LIBBENCHMARK_TOPOLOGY_NODE_SET_TYPE( *tns, LIBBENCHMARK_TOPOLOGY_NODE_TYPE_PHYSICAL_PROCESSOR );

  libbenchmark_topology_insert( ts, tns );

  return;
}





/****************************************************************************/
void libbenchmark_misc_pal_helper_add_cache_node_to_topology_tree( struct libbenchmark_topology_state *ts,
                                                                   struct libbenchmark_topology_node_state *tns,
                                                                   lfds710_pal_uint_t level,
                                                                   enum libbenchmark_topology_node_cache_type type )
{
  LFDS710_PAL_ASSERT( ts != NULL );
  LFDS710_PAL_ASSERT( tns != NULL );
  // TRD : level can be any value in its range
  // TRD : type can be any value in its range

  LIBBENCHMARK_TOPOLOGY_NODE_SET_TYPE( *tns, LIBBENCHMARK_TOPOLOGY_NODE_TYPE_CACHE );
  LIBBENCHMARK_TOPOLOGY_NODE_SET_CACHE_LEVEL( *tns, level );
  LIBBENCHMARK_TOPOLOGY_NODE_SET_CACHE_TYPE( *tns, type );

  libbenchmark_topology_insert( ts, tns );

  return;
}





/****************************************************************************/
void libbenchmark_misc_pal_helper_add_logical_processor_node_to_topology_tree( struct libbenchmark_topology_state *ts,
                                                                               struct libshared_memory_state *ms,
                                                                               lfds710_pal_uint_t logical_processor_number,
                                                                               enum flag windows_processor_group_inuse_flag,
                                                                               lfds710_pal_uint_t windows_processor_group_number )
{
  struct libbenchmark_topology_node_state
    *tns;

  LFDS710_PAL_ASSERT( ts != NULL );
  LFDS710_PAL_ASSERT( ms != NULL );
  // TRD : logical_processor_number can be any value in its range
  // TRD : windows_processor_group_inuse_flag can be any value in its range
  // TRD : windows_processor_group_number can be any value in its range

  libbenchmark_misc_pal_helper_new_topology_node( &tns, ms );

  LIBBENCHMARK_TOPOLOGY_NODE_SET_TYPE( *tns, LIBBENCHMARK_TOPOLOGY_NODE_TYPE_LOGICAL_PROCESSOR );
  LIBBENCHMARK_TOPOLOGY_NODE_SET_LOGICAL_PROCESSOR_NUMBER( *tns, logical_processor_number );

  if( windows_processor_group_inuse_flag == RAISED )
   LIBBENCHMARK_TOPOLOGY_NODE_SET_WINDOWS_GROUP_NUMBER( *tns, windows_processor_group_number );
  else
    LIBBENCHMARK_TOPOLOGY_NODE_UNSET_WINDOWS_GROUP_NUMBER( *tns );

  libbenchmark_topology_insert( ts, tns );

  return;
}

