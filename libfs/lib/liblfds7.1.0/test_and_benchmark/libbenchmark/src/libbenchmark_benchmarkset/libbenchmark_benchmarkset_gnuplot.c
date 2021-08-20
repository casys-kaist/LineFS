/***** includes *****/
#include "libbenchmark_benchmarkset_internal.h"





/****************************************************************************/
void libbenchmark_benchmarkset_gnuplot_emit( struct libbenchmark_benchmarkset_state *bsets,
                                             struct libbenchmark_results_state *rs,
                                             char *gnuplot_system_string,
                                             enum libbenchmark_topology_numa_mode numa_mode,
                                             struct libbenchmark_gnuplot_options *gpo,
                                             struct libbenchmark_benchmarkset_gnuplot *bg )
{
  char
    png_filename[512],
    temp_string[64],
    *topology_string;

  char const
    *libbenchmark_version_and_build_string,
    *liblfds_version_and_build_string,
    *libshared_version_and_build_string;

  enum flag
    found_flag;

  int
    rv;

  lfds710_pal_uint_t
    count,
    greatest_digit = 0,
    greatest_result = 0,
    length,
    loop,
    longest_line_length = 0,
    result,
    second_greatest_digit,
    temp_greatest_result,
    title_line_lengths[5],
    y_max = 0, // TRD : to remove compiler warning
    number_logical_cores,
    number_benchmarks,
    number_lp_sets,
    topology_string_length,
    one_block_xticks,
    one_block_plot,
    one_block_titles,
    one_block_numeric_data,
    one_inner_block,
    one_outer_block,
    one_block,
    total_length_in_bytes;

  struct libbenchmark_benchmarkinstance_state
    *bs;

  struct lfds710_btree_au_element
    *baue,
    *baue_inner,
    *baue_temp;

  struct lfds710_list_asu_element
    *lasue_benchmarks,
    *lasue_benchmarks_outer,
    *lasue_lpset,
    *lasue_temp;

  struct lfds710_list_aso_element
    *lasoe,
    *lasoe_inner;

  struct lfds710_list_aso_state
    *logical_processor_set;

  struct libbenchmark_topology_node_state
    *tns,
    *tns_results,
    *tns_inner = NULL; // TRD : to remove compiler warning

  LFDS710_PAL_ASSERT( bsets != NULL );
  LFDS710_PAL_ASSERT( rs != NULL );
  LFDS710_PAL_ASSERT( gnuplot_system_string != NULL );
  // TRD : numa_mode can be any value in its range
  LFDS710_PAL_ASSERT( gpo != NULL );
  LFDS710_PAL_ASSERT( bg != NULL );

  bg->datastructure_id = bsets->datastructure_id;
  bg->benchmark_id = bsets->benchmark_id;

  /* TRD : so, first, we're producing a string
           so we need to figure out how much store to allocate

           roughly, length is;

           topology string length
           fixed = 4096
           one_block_xticks = 64 + 6 * number logical cores
           one_block_plot = 16 + 40 * number_benchmarks
           one_block_titles = 32 * number_benchmarks
           one_block_numeric_data = (16 + number_benchmarks) * number_benchmarks * number logical cores
           one_inner_block = one_block_titles + one_block_numeric_data + 2
           one_outer_block = one_block_xticks + one_block_plot
           one_block = one_inner_block * number_benchmarks + one_outer_block

           (plus one on number_lp_sets for the blank key-only chart)
           total = topology_string_length + fixed + one_block * (number_lp_sets+1)
  */

  topology_string = libbenchmark_topology_generate_string( bsets->ts, bsets->ms, LIBBENCHMARK_TOPOLOGY_STRING_FORMAT_GNUPLOT );

  libbenchmark_topology_query( bsets->ts, LIBBENCHMARK_TOPOLOGY_QUERY_GET_NUMBER_OF_NODE_TYPE, (void *) (lfds710_pal_uint_t) LIBBENCHMARK_TOPOLOGY_NODE_TYPE_LOGICAL_PROCESSOR, &number_logical_cores );
  lfds710_list_asu_query( &bsets->benchmarks, LFDS710_LIST_ASU_QUERY_GET_POTENTIALLY_INACCURATE_COUNT, NULL, &number_benchmarks );
  lfds710_list_asu_query( bsets->logical_processor_sets, LFDS710_LIST_ASU_QUERY_GET_POTENTIALLY_INACCURATE_COUNT, NULL, (void **) &number_lp_sets );

  topology_string_length = libshared_ansi_strlen( topology_string );
  one_block_xticks = 64 + 6 * number_logical_cores;
  one_block_plot = 16 + 40 * number_benchmarks;
  one_block_titles = 32 * number_benchmarks;
  one_block_numeric_data = (16 + number_benchmarks) * number_benchmarks * number_logical_cores;
  one_inner_block = one_block_titles + one_block_numeric_data + 2;
  one_outer_block = one_block_xticks + one_block_plot;
  one_block = one_inner_block * number_benchmarks + one_outer_block;
  total_length_in_bytes = topology_string_length + 4096 + one_block * (number_lp_sets+1);

  bg->gnuplot_string = libshared_memory_alloc_from_most_free_space_node( bsets->ms, total_length_in_bytes, sizeof(char) );

  lfds710_misc_query( LFDS710_MISC_QUERY_GET_BUILD_AND_VERSION_STRING, NULL, (void **) &liblfds_version_and_build_string );
  libbenchmark_misc_query( LIBBENCHMARK_MISC_QUERY_GET_BUILD_AND_VERSION_STRING, NULL, (void **) &libbenchmark_version_and_build_string );
  libshared_misc_query( LIBSHARED_MISC_QUERY_GET_BUILD_AND_VERSION_STRING, NULL, (void **) &libshared_version_and_build_string );

  libshared_ansi_strcpy( bg->filename, "liblfds" );
  libshared_ansi_strcat_number( bg->filename, (lfds710_pal_uint_t) LFDS710_MISC_VERSION_INTEGER );
  libshared_ansi_strcat( bg->filename, "_" );
  libshared_ansi_strcat( bg->filename, libbenchmark_globals_datastructure_names[bsets->datastructure_id] );
  libshared_ansi_strcat( bg->filename, "_" );
  libshared_ansi_strcat( bg->filename, libbenchmark_globals_benchmark_names[bsets->benchmark_id] );
  libshared_ansi_strcat( bg->filename, "_" );
  libshared_ansi_strcat( bg->filename, libbenchmark_globals_numa_mode_names[numa_mode] );
  libshared_ansi_strcat( bg->filename, "_" );
  libshared_ansi_strcat( bg->filename, gnuplot_system_string );
  libshared_ansi_strcat( bg->filename, ".gnuplot" );

  libshared_ansi_strcpy( png_filename, "liblfds" );
  libshared_ansi_strcat_number( png_filename, (lfds710_pal_uint_t) LFDS710_MISC_VERSION_INTEGER );
  libshared_ansi_strcat( png_filename, "_" );
  libshared_ansi_strcat( png_filename, libbenchmark_globals_datastructure_names[bsets->datastructure_id] );
  libshared_ansi_strcat( png_filename, "_" );
  libshared_ansi_strcat( png_filename, libbenchmark_globals_benchmark_names[bsets->benchmark_id] );
  libshared_ansi_strcat( png_filename, "_" );
  libshared_ansi_strcat( png_filename, libbenchmark_globals_numa_mode_names[numa_mode] );
  libshared_ansi_strcat( png_filename, "_" );
  libshared_ansi_strcat( png_filename, gnuplot_system_string );
  libshared_ansi_strcat( png_filename, ".png" );

  // TRD : now for main gnuplot header
  libshared_ansi_strcpy( bg->gnuplot_string, "set output \"" );
  libshared_ansi_strcat( bg->gnuplot_string, png_filename );
  libshared_ansi_strcat( bg->gnuplot_string, "\"\n" );

  libshared_ansi_strcat( bg->gnuplot_string, "set terminal pngcairo enhanced font \"Courier New,14\" size " );

  if( gpo->width_in_pixels_set_flag == RAISED )
    libshared_ansi_strcat_number( bg->gnuplot_string, gpo->width_in_pixels );

  // TRD : 300px wide per logical core
  if( gpo->width_in_pixels_set_flag == LOWERED )
    libshared_ansi_strcat_number( bg->gnuplot_string, number_logical_cores * 300 );

  libshared_ansi_strcat( bg->gnuplot_string, "," );

  // TRD : height is 300 pixels per chart, plus 300 for the title, plus 300 for the key
  if( gpo->height_in_pixels_set_flag == RAISED )
    libshared_ansi_strcat_number( bg->gnuplot_string, gpo->height_in_pixels );

  if( gpo->height_in_pixels_set_flag == LOWERED )
    libshared_ansi_strcat_number( bg->gnuplot_string, (number_lp_sets+2) * 300 );

  libshared_ansi_strcat( bg->gnuplot_string, "\n" );
  libshared_ansi_strcat( bg->gnuplot_string, "set multiplot title \"" );

  // TRD : compute longest line in first header part so we can know how many spaces needed for right padding on each line
  title_line_lengths[0] = libshared_ansi_strlen( "data structure : " ) + libshared_ansi_strlen( libbenchmark_globals_datastructure_names[bsets->datastructure_id] );
  title_line_lengths[1] = libshared_ansi_strlen( "benchmark      : " ) + libshared_ansi_strlen( libbenchmark_globals_benchmark_names[bsets->benchmark_id] );
  title_line_lengths[2] = libshared_ansi_strlen( "numa mode      : " ) + libshared_ansi_strlen( libbenchmark_globals_numa_mode_names[numa_mode] );
  temp_string[0] = '\0';
  libshared_ansi_strcat_number( temp_string, libbenchmark_globals_benchmark_duration_in_seconds );
  title_line_lengths[3] = libshared_ansi_strlen( "duration       :  seconds(s)" ) + libshared_ansi_strlen( temp_string );
  title_line_lengths[4] = libshared_ansi_strlen( "system         : " ) + libshared_ansi_strlen( gnuplot_system_string );

  for( loop = 0 ; loop < 5 ; loop++ )
    if( title_line_lengths[loop] > longest_line_length )
      longest_line_length = title_line_lengths[loop];

  // TRD : now emit
  libshared_ansi_strcat( bg->gnuplot_string, "data structure : " );
  libshared_ansi_strcat( bg->gnuplot_string, libbenchmark_globals_datastructure_names[bsets->datastructure_id] );
  for( loop = 0 ; loop < longest_line_length - title_line_lengths[0] ; loop++ )
    libshared_ansi_strcat( bg->gnuplot_string, " " );
  libshared_ansi_strcat( bg->gnuplot_string, "\\n\\\n" );

  libshared_ansi_strcat( bg->gnuplot_string, "benchmark      : " );
  libshared_ansi_strcat( bg->gnuplot_string, libbenchmark_globals_benchmark_names[bsets->benchmark_id] );
  for( loop = 0 ; loop < longest_line_length - title_line_lengths[1] ; loop++ )
    libshared_ansi_strcat( bg->gnuplot_string, " " );
  libshared_ansi_strcat( bg->gnuplot_string, "\\n\\\n" );

  libshared_ansi_strcat( bg->gnuplot_string, "numa mode      : " );
  libshared_ansi_strcat( bg->gnuplot_string, libbenchmark_globals_numa_mode_names[numa_mode] );
  for( loop = 0 ; loop < longest_line_length - title_line_lengths[2] ; loop++ )
    libshared_ansi_strcat( bg->gnuplot_string, " " );
  libshared_ansi_strcat( bg->gnuplot_string, "\\n\\\n" );

  libshared_ansi_strcat( bg->gnuplot_string, "duration       : " );
  temp_string[0] = '\0';
  libshared_ansi_strcat_number( bg->gnuplot_string, libbenchmark_globals_benchmark_duration_in_seconds );
  libshared_ansi_strcat( bg->gnuplot_string, " seconds(s)" );
  for( loop = 0 ; loop < longest_line_length - title_line_lengths[3] ; loop++ )
    libshared_ansi_strcat( bg->gnuplot_string, " " );
  libshared_ansi_strcat( bg->gnuplot_string, "\\n\\\n" );

  libshared_ansi_strcat( bg->gnuplot_string, "system         : " );
  libshared_ansi_strcat( bg->gnuplot_string, gnuplot_system_string );
  for( loop = 0 ; loop < longest_line_length - title_line_lengths[4] ; loop++ )
    libshared_ansi_strcat( bg->gnuplot_string, " " );
  libshared_ansi_strcat( bg->gnuplot_string, "\\n\\\n" );

  libshared_ansi_strcat( bg->gnuplot_string, "\\n\\\n" );
  libshared_ansi_strcat( bg->gnuplot_string, topology_string );
  libshared_ansi_strcat( bg->gnuplot_string, "\\n\\\n" );

  libshared_ansi_strcat( bg->gnuplot_string, "Y axis = ops/sec, X axis = logical cores in use\\n\\\n" );
  libshared_ansi_strcat( bg->gnuplot_string, "\\n\\\n" );
  libshared_ansi_strcat( bg->gnuplot_string, (char *) liblfds_version_and_build_string );
  libshared_ansi_strcat( bg->gnuplot_string, "\\n\\\n" );
  libshared_ansi_strcat( bg->gnuplot_string, (char *) libshared_version_and_build_string );
  libshared_ansi_strcat( bg->gnuplot_string, "\\n\\\n" );
  libshared_ansi_strcat( bg->gnuplot_string, (char *) libbenchmark_version_and_build_string );

  libshared_ansi_strcat( bg->gnuplot_string, "\" layout " );
  // TRD : +1 for key-only chart
  libshared_ansi_strcat_number( bg->gnuplot_string, number_lp_sets+1 );
  libshared_ansi_strcat( bg->gnuplot_string, ",1 rowsfirst noenhanced\n"
                                                "set format y \"%.0f\"\n"
                                                "set boxwidth 1 absolute\n"
                                                "set style data histograms\n"
                                                "set style histogram cluster\n"
                                                "set style histogram gap 4\n"
                                                "set style fill solid border -1\n"
                                                "set key autotitle columnheader center top\n"
                                                "set noxtics\n"
                                                "set noytics\n"
                                                "set noborder\n"
                                                "set noxlabel\n"
                                                "set noylabel\n" );

  /* TRD : we're drawing the plot for benchmarks in one set (i.e. a single logical benchmark, but all the different lock type variants of it)
           over all of its logical processor sets
           so we have one chart per logical processor set
  */

  /* TRD : first, we need to compute the y range
           this itself requires us to iterate over the result for every core in every test :-)
  */

  // TRD : loop over every logical processor set
  lasue_lpset = NULL;

  while( LFDS710_LIST_ASU_GET_START_AND_THEN_NEXT(*bsets->logical_processor_sets,lasue_lpset) )
  {
    logical_processor_set = LFDS710_LIST_ASU_GET_VALUE_FROM_ELEMENT( *lasue_lpset );

    // TRD : now loop over every benchmark
    lasue_benchmarks = NULL;

    while( LFDS710_LIST_ASU_GET_START_AND_THEN_NEXT(bsets->benchmarks,lasue_benchmarks) )
    {
      bs = LFDS710_LIST_ASU_GET_VALUE_FROM_ELEMENT( *lasue_benchmarks );

      // TRD : now for this processor set, loop over every logical core
      lasoe = NULL;

      while( LFDS710_LIST_ASO_GET_START_AND_THEN_NEXT(*logical_processor_set, lasoe) )
      {
        tns_results = LFDS710_LIST_ASU_GET_VALUE_FROM_ELEMENT( *lasoe );

        // TRD : now, finally, let's go shopping
        libbenchmark_results_get_result( rs, 
                                         bsets->datastructure_id,
                                         bsets->benchmark_id,
                                         bs->lock_id,
                                         numa_mode,
                                         logical_processor_set,
                                         tns_results,
                                         &result );

        if( result > greatest_result )
          greatest_result = result;
      }
    }
  }

  if( gpo->y_axis_scale_type == LIBBENCHMARK_GNUPLOT_Y_AXIS_SCALE_TYPE_LINEAR )
    libshared_ansi_strcat( bg->gnuplot_string, "set yrange [0:" );

  if( gpo->y_axis_scale_type == LIBBENCHMARK_GNUPLOT_Y_AXIS_SCALE_TYPE_LOGARITHMIC )
    libshared_ansi_strcat( bg->gnuplot_string, "set logscale y\n"
                                               "set yrange [1:" );

  /* TRD : for y-range max, we look at the second greatest digit in greatest_result
           i.e. for 3429111 the second greatest digit is 4
           if that digit is 0 to 4, inclusive, the y-max value is that digit converted to 5, and everything to the right (smaller values to 0)
           if that digit is 5 to 9, inclusive, the y-max value is that digit and everything to the right converted to 0, and the greatest digit increased by 1
           I am assuming I will not exceed a 32-bit unsigned max :-)
           if the greatest_result is a single digit, we set second_greatest_digit to greatest_digit and greatest_digit to 0, and it works properly
  */

  temp_greatest_result = greatest_result / libbenchmark_globals_benchmark_duration_in_seconds;
  length = 0;

  do
  {
    second_greatest_digit = greatest_digit;
    greatest_digit = temp_greatest_result % 10;
    length++;
    temp_greatest_result -= greatest_digit;
    temp_greatest_result /= 10;
  }
  while( temp_greatest_result > 0 );

  if( length == 1 )
  {
    second_greatest_digit = greatest_digit;
    greatest_digit = 0;
  }

  if( second_greatest_digit < 5 )
    y_max = greatest_digit * 10 + 5;

  if( second_greatest_digit >= 5 )
    y_max = (greatest_digit+1) * 10;

  if( length >= 2 )
    for( loop = 0 ; loop < length-2 ; loop++ )
      y_max *= 10;

  libshared_ansi_strcat_number( bg->gnuplot_string, y_max );
  libshared_ansi_strcat( bg->gnuplot_string, "]\n" );

  // TRD : now print one empty chart which is just for the key

  libshared_ansi_strcat( bg->gnuplot_string, "plot " );

  lasue_benchmarks = NULL;
  count = 1;

  while( LFDS710_LIST_ASU_GET_START_AND_THEN_NEXT(bsets->benchmarks,lasue_benchmarks) )
  {
    lasue_temp = lasue_benchmarks;
    lasue_temp = LFDS710_LIST_ASU_GET_NEXT( *lasue_temp );

    libshared_ansi_strcat( bg->gnuplot_string, "     '-' using " );
    libshared_ansi_strcat_number( bg->gnuplot_string, count++ );
    libshared_ansi_strcat( bg->gnuplot_string, lasue_temp != NULL ? ", \\\n" : "\n" );
  }

  /* TRD : simpler output for the key-only chart
           for each benchmark
             print the title (name of all benchmarks)
             for number of lp sets
               print "0 " for number of benchmarks
             print "e"

           print;
            set key off
            set border
            set ytics mirror
  */

  lasue_benchmarks_outer = NULL;

  while( LFDS710_LIST_ASU_GET_START_AND_THEN_NEXT(bsets->benchmarks,lasue_benchmarks_outer) )
  {
    // TRD : now loop over every benchmark and print its lock name, in quotes
    lasue_benchmarks = NULL;

    while( LFDS710_LIST_ASU_GET_START_AND_THEN_NEXT(bsets->benchmarks,lasue_benchmarks) )
    {
      bs = LFDS710_LIST_ASU_GET_VALUE_FROM_ELEMENT( *lasue_benchmarks );

      libshared_ansi_strcat( bg->gnuplot_string, "\"" );
      libshared_ansi_strcat( bg->gnuplot_string, libbenchmark_globals_lock_names[bs->lock_id] );
      libshared_ansi_strcat( bg->gnuplot_string, "\" " );
    }

    libshared_ansi_strcat( bg->gnuplot_string, "\n" );

    // TRD : loop over every logical processor set
    lasue_lpset = NULL;

    while( LFDS710_LIST_ASU_GET_START_AND_THEN_NEXT(*bsets->logical_processor_sets,lasue_lpset) )
    {
      logical_processor_set = LFDS710_LIST_ASU_GET_VALUE_FROM_ELEMENT( *lasue_lpset );

      // TRD : now loop over every benchmark and print its lock name, in quotes
      lasue_benchmarks = NULL;

      while( LFDS710_LIST_ASU_GET_START_AND_THEN_NEXT(bsets->benchmarks,lasue_benchmarks) )
        libshared_ansi_strcat( bg->gnuplot_string, "0 " );

      libshared_ansi_strcat( bg->gnuplot_string, "\n" );
    }

    libshared_ansi_strcat( bg->gnuplot_string, "e\n" );
  }

  libshared_ansi_strcat( bg->gnuplot_string, "set key off\n"
                                             "set border\n"
                                             "set ytics mirror\n" );

  // TRD : now repeat, this time emitting actual charts

  // TRD : loop over every logical processor set
  lasue_lpset = NULL;

  while( LFDS710_LIST_ASU_GET_START_AND_THEN_NEXT(*bsets->logical_processor_sets,lasue_lpset) )
  {
    logical_processor_set = LFDS710_LIST_ASU_GET_VALUE_FROM_ELEMENT( *lasue_lpset );

    /* TRD : emit the chart header; xtics and plot
             to get the display order right
             we need to loop over every logical processor in the topology set
             we then check each of each LP to see if they're in the lpset
             if so, we print the LP number, otherwise a "-"
    */

    libshared_ansi_strcat( bg->gnuplot_string, "set xtics 1 out nomirror ( " );

    baue = NULL;
    count = 0;

    while( lfds710_btree_au_get_by_absolute_position_and_then_by_relative_position(&bsets->ts->topology_tree, &baue, LFDS710_BTREE_AU_ABSOLUTE_POSITION_LARGEST_IN_TREE, LFDS710_BTREE_AU_RELATIVE_POSITION_NEXT_SMALLER_ELEMENT_IN_ENTIRE_TREE) )
    {
      tns = LFDS710_BTREE_AU_GET_KEY_FROM_ELEMENT( *baue );

      if( tns->type == LIBBENCHMARK_TOPOLOGY_NODE_TYPE_LOGICAL_PROCESSOR )
      {
        /* TRD : now for this processor set, loop over every logical core and see if lp is in this set
                 if in set, and processor_group is not set, print the processor number, or "-"
                 if in set, and processor_group is set, print "processor number/group number", or "-"
        */

        lasoe_inner = NULL;
        found_flag = LOWERED;

        while( found_flag == LOWERED and LFDS710_LIST_ASO_GET_START_AND_THEN_NEXT(*logical_processor_set, lasoe_inner) )
        {
          tns_inner = LFDS710_LIST_ASU_GET_VALUE_FROM_ELEMENT( *lasoe_inner );

          if( 0 == libbenchmark_topology_node_compare_nodes_function(tns, tns_inner) )
            found_flag = RAISED;
        }

        /* TRD : check to see if we're the last element - if so, no trailing comman
                 the final LP is always the smallest element in the tree, so it's always the final element in the tree
        */
        baue_temp = baue;
        lfds710_btree_au_get_by_relative_position( &baue_temp, LFDS710_BTREE_AU_RELATIVE_POSITION_NEXT_SMALLER_ELEMENT_IN_ENTIRE_TREE );
        libshared_ansi_strcat( bg->gnuplot_string, "\"" );

        if( found_flag == RAISED )
        {
          libshared_ansi_strcat_number( bg->gnuplot_string, LIBBENCHMARK_TOPOLOGY_NODE_GET_LOGICAL_PROCESSOR_NUMBER(*tns_inner) );

          if( LIBBENCHMARK_TOPOLOGY_NODE_IS_WINDOWS_GROUP_NUMBER(*tns_inner) )
          {
            libshared_ansi_strcat( bg->gnuplot_string, "/" );
            libshared_ansi_strcat_number( bg->gnuplot_string, LIBBENCHMARK_TOPOLOGY_NODE_GET_WINDOWS_GROUP_NUMBER(*tns_inner) );
          }
        }

        if( found_flag == LOWERED )
          libshared_ansi_strcat( bg->gnuplot_string, "-" );

        libshared_ansi_strcat( bg->gnuplot_string, "\" " );
        libshared_ansi_strcat_number( bg->gnuplot_string, count++ );
        libshared_ansi_strcat( bg->gnuplot_string, baue_temp != NULL ? ", " : " " );
      }
    }

    libshared_ansi_strcat( bg->gnuplot_string, " )\n" );

    /* TRD : now the plot command

             christ, I need an API for this, to build up the plot in memory and then dump it out
             this hardcoded in-line output is insane

             we print one line per lock type (i.e. one per benchmark)
    */

    libshared_ansi_strcat( bg->gnuplot_string, "plot " );

    // TRD : now loop over every benchmark
    lasue_benchmarks = NULL;
    count = 1;

    while( LFDS710_LIST_ASU_GET_START_AND_THEN_NEXT(bsets->benchmarks,lasue_benchmarks) )
    {
      lasue_temp = lasue_benchmarks;
      lasue_temp = LFDS710_LIST_ASU_GET_NEXT( *lasue_temp );

      libshared_ansi_strcat( bg->gnuplot_string, "     '-' using " );
      libshared_ansi_strcat_number( bg->gnuplot_string, count++ );
      libshared_ansi_strcat( bg->gnuplot_string, lasue_temp != NULL ? ", \\\n" : "\n" );
    }

    /* TRD : now for the results
             we need to print 0s for the LPs not in the set
             and only the topology shows the LPs we do not have
             so iterate over the LPs in topology
             for each LP in topology
             we can search the result set for it, because if it's not there, we'll print a 0

             we need to print these all once per benchmark/lock
    */

    lasue_benchmarks_outer = NULL;

    while( LFDS710_LIST_ASU_GET_START_AND_THEN_NEXT(bsets->benchmarks,lasue_benchmarks_outer) )
    {
      baue = NULL;

      // TRD : now loop over every benchmark and print its lock name, in quotes
      lasue_benchmarks = NULL;

      while( LFDS710_LIST_ASU_GET_START_AND_THEN_NEXT(bsets->benchmarks,lasue_benchmarks) )
      {
        bs = LFDS710_LIST_ASU_GET_VALUE_FROM_ELEMENT( *lasue_benchmarks );

        libshared_ansi_strcat( bg->gnuplot_string, "\"" );
        libshared_ansi_strcat( bg->gnuplot_string, libbenchmark_globals_lock_names[bs->lock_id] );
        libshared_ansi_strcat( bg->gnuplot_string, "\" " );
      }

      libshared_ansi_strcat( bg->gnuplot_string, "\n" );

      baue_inner = NULL;

      while( lfds710_btree_au_get_by_absolute_position_and_then_by_relative_position(&bsets->ts->topology_tree, &baue_inner, LFDS710_BTREE_AU_ABSOLUTE_POSITION_LARGEST_IN_TREE, LFDS710_BTREE_AU_RELATIVE_POSITION_NEXT_SMALLER_ELEMENT_IN_ENTIRE_TREE) )
      {
        tns = LFDS710_BTREE_AU_GET_KEY_FROM_ELEMENT( *baue_inner );

        if( tns->type == LIBBENCHMARK_TOPOLOGY_NODE_TYPE_LOGICAL_PROCESSOR )
        {
          // TRD : so we have an LP - now loop over every benchmark, and print 0 if not found, result if found
          lasue_benchmarks = NULL;

          while( LFDS710_LIST_ASU_GET_START_AND_THEN_NEXT(bsets->benchmarks,lasue_benchmarks) )
          {
            bs = LFDS710_LIST_ASU_GET_VALUE_FROM_ELEMENT( *lasue_benchmarks );

            // TRD : now, finally, let's go shopping
            rv = libbenchmark_results_get_result( rs, 
                                                  bsets->datastructure_id,
                                                  bsets->benchmark_id,
                                                  bs->lock_id,
                                                  numa_mode,
                                                  logical_processor_set,
                                                  tns,
                                                  &result );

            if( rv == 0 )
              libshared_ansi_strcat( bg->gnuplot_string, "0 " );

            if( rv == 1 )
            {
              libshared_ansi_strcat_number( bg->gnuplot_string, result / libbenchmark_globals_benchmark_duration_in_seconds);
              libshared_ansi_strcat( bg->gnuplot_string, " " );
            }
          }

          libshared_ansi_strcat( bg->gnuplot_string, "\n" );
        }
      }

      libshared_ansi_strcat( bg->gnuplot_string, "e\n" );
    }
  }

  return;
}

