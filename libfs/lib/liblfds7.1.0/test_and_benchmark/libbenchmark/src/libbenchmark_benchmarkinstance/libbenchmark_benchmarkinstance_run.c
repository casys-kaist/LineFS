/***** includes *****/
#include "libbenchmark_benchmarkinstance_internal.h"





/****************************************************************************/
void libbenchmark_benchmarkinstance_run( struct libbenchmark_benchmarkinstance_state *bs,
                                         struct lfds710_list_aso_state *lpset,
                                         enum libbenchmark_topology_numa_mode numa_mode,
                                         struct libshared_memory_state *ms,
                                         struct libbenchmark_results_state *rs )
{
  char
    *lpset_string,
    temp[64];

  lfds710_pal_uint_t
    operation_count;

  struct lfds710_list_aso_element
    *lasoe;

  struct libbenchmark_topology_node_state
    *tns;

  LFDS710_PAL_ASSERT( bs != NULL );
  LFDS710_PAL_ASSERT( lpset != NULL );
  // TRD : numa_mode can be any value in its range
  LFDS710_PAL_ASSERT( ms != NULL );
  LFDS710_PAL_ASSERT( rs != NULL );

  libshared_memory_set_rollback( ms );

  lpset_string = libbenchmark_topology_generate_lpset_string( bs->ts, ms, lpset );

  libbenchmark_pal_print_string( lpset_string );

  libbenchmark_pal_print_string( " " );
  libbenchmark_pal_print_string( libbenchmark_globals_datastructure_names[bs->datastructure_id] );
  libbenchmark_pal_print_string( " " );
  libbenchmark_pal_print_string( libbenchmark_globals_lock_names[bs->lock_id] );

  libbenchmark_pal_print_string( " (" );
  libbenchmark_pal_print_string( libbenchmark_globals_numa_mode_names[numa_mode] );
  libbenchmark_pal_print_string( ")" );

  bs->init_function( bs->ts, lpset, ms, numa_mode, &bs->tsets );

  libbenchmark_threadset_run( &bs->tsets );

  // TRD : cleanup transfers results to the resultset
  bs->cleanup_function( lpset, numa_mode, rs, &bs->tsets );

  libbenchmark_threadset_cleanup( &bs->tsets );

  // TRD : print the results

  lasoe = NULL;

  while( LFDS710_LIST_ASO_GET_START_AND_THEN_NEXT(*lpset, lasoe) )
  {
    tns = LFDS710_LIST_ASU_GET_VALUE_FROM_ELEMENT( *lasoe );

    libbenchmark_results_get_result( rs, 
                                     bs->datastructure_id,
                                     bs->benchmark_id,
                                     bs->lock_id,
                                     numa_mode,
                                     lpset,
                                     tns,
                                     &operation_count );

    libshared_ansi_strcpy( temp, ", " );
    libshared_ansi_strcat_number( temp, operation_count / libbenchmark_globals_benchmark_duration_in_seconds );
    libshared_ansi_strcat( temp, " (" );

    // libshared_ansi_strcat_number_with_leading_zeros( temp, LIBBENCHMARK_TOPOLOGY_NODE_GET_LOGICAL_PROCESSOR_NUMBER(*tns), 3 );
    libshared_ansi_strcat_number( temp, LIBBENCHMARK_TOPOLOGY_NODE_GET_LOGICAL_PROCESSOR_NUMBER(*tns) );

    if( LIBBENCHMARK_TOPOLOGY_NODE_IS_WINDOWS_GROUP_NUMBER(*tns) )
    {
      libshared_ansi_strcat( temp, "/" );
      libshared_ansi_strcat_number( temp, LIBBENCHMARK_TOPOLOGY_NODE_GET_WINDOWS_GROUP_NUMBER(*tns) );
    }

    libshared_ansi_strcat( temp, ")" );

    libbenchmark_pal_print_string( temp );
  }

  libbenchmark_pal_print_string( "\n" );

  libshared_memory_rollback( ms );

  return;
}

