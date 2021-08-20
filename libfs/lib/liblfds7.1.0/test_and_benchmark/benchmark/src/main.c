/***** includes *****/
#include "internal.h"

/***** structs *****/
struct operations_to_perform
{
  char
    *gnuplot_system_string;

  enum flag
    benchmark_duration_flag,
    gnuplot_file_flag,
    gnuplot_logarithmic_yaxis_flag,
    gnuplot_png_flag,
    gnuplot_height_flag,
    gnuplot_width_flag,
    run_flag,
    show_cpu_topology_only_flag,
    show_error_flag,
    show_help_flag,
    show_version_flag,
    memory_flag;

  lfds710_pal_uint_t
    memory_in_megabytes,
    benchmark_duration_in_seconds,
    gnuplot_height_in_pixels,
    gnuplot_width_in_pixels;
};

/***** prototypes *****/
static void convert_command_line_args_to_operations_to_perform( int argc, char **argv, struct operations_to_perform *otp );
static void perform_operations( struct operations_to_perform *otp );
static void memory_cleanup_callback( enum flag known_numa_node_flag, void *store, lfds710_pal_uint_t size_in_bytes );





/****************************************************************************/
int main( int argc, char **argv )
{
  struct operations_to_perform
    otp;

  assert( argc >= 1 );
  assert( argv != NULL );

  convert_command_line_args_to_operations_to_perform( argc, argv, &otp );

  perform_operations( &otp );

  return EXIT_SUCCESS;
}





/****************************************************************************/
static void convert_command_line_args_to_operations_to_perform( int argc, char **argv, struct operations_to_perform *otp )
{
  int
    rv;

  struct util_cmdline_state
    cs;

  union util_cmdline_arg_data
    *arg_data;

  assert( argc >= 1 );
  assert( argv != NULL );
  assert( otp != NULL );

  otp->benchmark_duration_flag = LOWERED;
  otp->gnuplot_file_flag = LOWERED;
  otp->gnuplot_logarithmic_yaxis_flag = LOWERED;
  otp->gnuplot_png_flag = LOWERED;
  otp->gnuplot_height_flag = LOWERED;
  otp->gnuplot_width_flag = LOWERED;
  otp->run_flag = LOWERED;
  otp->show_cpu_topology_only_flag = LOWERED;
  otp->show_error_flag = LOWERED;
  otp->show_help_flag = RAISED;
  otp->show_version_flag = LOWERED;
  otp->memory_flag = LOWERED;

  /* TRD : the numeric options are used by libbenchmark
           if we pass in a bitmark indicating they are set
           however, we ourselves use otp->memory_in_megabytes
           when we alloc for printing topology, so we need
           to initialize it
  */

  otp->memory_in_megabytes = BENCHMARK_DEFAULT_MEMORY_IN_MEGABYTES;

  util_cmdline_init( &cs );

  util_cmdline_add_arg( &cs, 'g', UTIL_CMDLINE_ARG_TYPE_STRING );
  util_cmdline_add_arg( &cs, 'h', UTIL_CMDLINE_ARG_TYPE_FLAG );
  util_cmdline_add_arg( &cs, 'l', UTIL_CMDLINE_ARG_TYPE_FLAG );
  util_cmdline_add_arg( &cs, 'm', UTIL_CMDLINE_ARG_TYPE_INTEGER );
  util_cmdline_add_arg( &cs, 'p', UTIL_CMDLINE_ARG_TYPE_FLAG );
  util_cmdline_add_arg( &cs, 'r', UTIL_CMDLINE_ARG_TYPE_FLAG );
  util_cmdline_add_arg( &cs, 's', UTIL_CMDLINE_ARG_TYPE_INTEGER );
  util_cmdline_add_arg( &cs, 't', UTIL_CMDLINE_ARG_TYPE_FLAG );
  util_cmdline_add_arg( &cs, 'v', UTIL_CMDLINE_ARG_TYPE_FLAG );
  util_cmdline_add_arg( &cs, 'x', UTIL_CMDLINE_ARG_TYPE_INTEGER );
  util_cmdline_add_arg( &cs, 'y', UTIL_CMDLINE_ARG_TYPE_INTEGER );

  rv = util_cmdline_process_args( &cs, argc, argv );

  if( rv == 0 )
    otp->show_error_flag = RAISED;
  
  if( rv == 1 )
  {
    util_cmdline_get_arg_data( &cs, 'g', &arg_data );
    if( arg_data != NULL )
    {
      otp->gnuplot_file_flag = RAISED;
      otp->gnuplot_system_string = arg_data->string.string;
    }

    util_cmdline_get_arg_data( &cs, 'h', &arg_data );
    if( arg_data != NULL )
      otp->show_help_flag = RAISED;

    util_cmdline_get_arg_data( &cs, 'l', &arg_data );
    if( arg_data != NULL )
      otp->gnuplot_logarithmic_yaxis_flag = RAISED;

    util_cmdline_get_arg_data( &cs, 'm', &arg_data );
    if( arg_data != NULL )
    {
      if( arg_data->integer.integer < 2 )
      {
        puts( "Memory (in megabytes) needs to be 2 or greater." );
        exit( EXIT_FAILURE );
      }

      otp->memory_in_megabytes = (lfds710_pal_uint_t) arg_data->integer.integer;
      otp->memory_flag = RAISED;
    }

    util_cmdline_get_arg_data( &cs, 'p', &arg_data );
    if( arg_data != NULL )
    {
      otp->gnuplot_png_flag = RAISED;

      // TRD : if -p, -g must be present
      util_cmdline_get_arg_data( &cs, 'g', &arg_data );
      if( arg_data == NULL )
      {
        puts( "If -p is given, -g must also be given." );
        exit( EXIT_FAILURE );
      }
    }

    util_cmdline_get_arg_data( &cs, 'r', &arg_data );
    if( arg_data != NULL )
    {
      otp->show_help_flag = LOWERED;
      otp->run_flag = RAISED;
    }

    util_cmdline_get_arg_data( &cs, 's', &arg_data );
    if( arg_data != NULL )
    {
      if( arg_data->integer.integer < 1 )
      {
        puts( "Duration in seconds needs to be 1 or greater." );
        exit( EXIT_FAILURE );
      }

      otp->benchmark_duration_in_seconds = (lfds710_pal_uint_t) arg_data->integer.integer;
      otp->benchmark_duration_flag = RAISED;
    }

    util_cmdline_get_arg_data( &cs, 't', &arg_data );
    if( arg_data != NULL )
    {
      otp->show_help_flag = LOWERED;
      otp->show_cpu_topology_only_flag = RAISED;
    }

    util_cmdline_get_arg_data( &cs, 'v', &arg_data );
    if( arg_data != NULL )
    {
      otp->show_help_flag = LOWERED;
      otp->show_version_flag = RAISED;
    }

    util_cmdline_get_arg_data( &cs, 'x', &arg_data );
    if( arg_data != NULL )
    {
      if( arg_data->integer.integer < 1 )
      {
        puts( "Gnuplot width in pixels needs to be 1 or greater." );
        exit( EXIT_FAILURE );
      }

      otp->gnuplot_width_in_pixels = (lfds710_pal_uint_t) arg_data->integer.integer;
      otp->gnuplot_width_flag = RAISED;
    }

    util_cmdline_get_arg_data( &cs, 'y', &arg_data );
    if( arg_data != NULL )
    {
      if( arg_data->integer.integer < 1 )
      {
        puts( "Gnuplot height in pixels needs to be 1 or greater." );
        exit( EXIT_FAILURE );
      }

      otp->gnuplot_height_in_pixels = (lfds710_pal_uint_t) arg_data->integer.integer;
      otp->gnuplot_height_flag = RAISED;
    }
  }

  util_cmdline_cleanup( &cs );

  return;
}





/****************************************************************************/
static void perform_operations( struct operations_to_perform *otp )
{
  assert( otp != NULL );

  if( otp->show_error_flag == RAISED )
  {
    printf( "\nInvalid arguments.  Sorry - it's a simple parser, so no clues.\n"
            "-h or run with no args to see the help text.\n" );

    return;
  }

  if( otp->show_help_flag == RAISED )
  {
    printf( "benchmark -g [s] -h -l -m [n] -p -r -s [n] -t -v -x [n] -y [n]\n"
            "  -g [s] : emit gnuplots, where [s] is an arbitrary string (in quotes if spaces) describing the system\n"
            "  -h     : help (this text you're reading now)\n"
            "  -l     : logarithmic gnuplot y-axis (normally linear)\n"
            "  -m [n] : alloc [n] mb RAM for benchmarks, default is %u (minimum 2 (two))\n"
            "           (user specifies RAM as libbenchmark performs no allocs - rather it is handed a block of memory\n"
            "            on NUMA systems, each node allocates an equal fraction of the total - benchmark knows about\n"
            "            NUMA and does the right things, including NUMA and non-NUMA versions of the benchmarks)\n"
            "  -p     : call gnuplot to emit PNGs (requires -g and gnuplot must be on the path)\n"
            "  -r     : run (causes benchmarks to run; present so no args gives help)\n"
            "  -s [n] : individual benchmark duration in integer seconds (min 1, duh)\n"
            "  -t     : show CPU topology, uses -m (or its default) for amount of RAM to alloc\n"
            "  -v     : build and version info\n"
            "  -x [n] : gnuplot width in pixels (in case the computed values are no good)\n"
            "  -y [n] : gnuplot height in pixels (in case the computed values are no good)\n",
            (int unsigned) BENCHMARK_DEFAULT_MEMORY_IN_MEGABYTES );

    #if( BENCHMARK_PAL_MEMORY_TYPE == BENCHMARK_MEMORY_TYPE_SMP )
    {
      printf( "\n"
              "WARNING : This is the SMP build of benchmark.  Do not use it on a NUMA\n"
              "          system as the results will be wrong - and not a bit wrong, but\n"
              "          VERY VERY WRONG.\n"
              "\n"
              "          The benchmarks measure the performance of liblfds, but to do so,\n"
              "          themselves have work to do, which involves what can be many\n"
              "          memory accesses, and so threads running on the primary NUMA node\n" 
              "          have no penalty for say twenty memory accesses, whereas threads\n"
              "          off the primary NUMA node are paying the penalty for all of them,\n"
              "          when maybe only five of those accesses are actually by liblfds.\n"
              "\n"
              "          As a result, SMP builds on NUMA systems make off-primary-node\n"
              "          threads look MUCH worse than they actually are, because you think\n"
              "          they're only measuring liblfds, when in fact they're not.\n" );
    }
    #endif

    return;
  }

  if( otp->show_cpu_topology_only_flag == RAISED )
  {
    char
      *topology_string;

    struct libshared_memory_state
      ms;

    struct libbenchmark_topology_state
      ts;

    void
      *store;

    libshared_memory_init( &ms );
    store = malloc( otp->memory_in_megabytes * ONE_MEGABYTE_IN_BYTES );
    libshared_memory_add_memory( &ms, store, otp->memory_in_megabytes * ONE_MEGABYTE_IN_BYTES );
    libbenchmark_topology_init( &ts, &ms );
    topology_string = libbenchmark_topology_generate_string( &ts, &ms, LIBBENCHMARK_TOPOLOGY_STRING_FORMAT_STDOUT );
    printf( "%s", topology_string );
    libbenchmark_topology_cleanup( &ts );
    libshared_memory_cleanup( &ms, NULL );
    free( store );

    return;
  }

  if( otp->run_flag == RAISED )
  {
    char
      diskbuffer[BUFSIZ];

    enum libbenchmark_topology_numa_mode
      numa_mode = LIBBENCHMARK_TOPOLOGY_NUMA_MODE_SMP;

    FILE
      *diskfile;

    lfds710_pal_uint_t
      options_bitmask = NO_FLAGS,
      size_in_bytes;

    struct lfds710_list_asu_element
      *lasue = NULL;

    struct lfds710_list_asu_state
      list_of_gnuplots;

    struct libbenchmark_benchmarkset_gnuplot
      *bg;

    struct libbenchmark_benchmarksuite_state
      bss;

    struct libshared_memory_state
      ms_for_benchmarks,
      ms_for_rs_and_ts;

    struct libbenchmark_topology_state
      ts;

    struct libbenchmark_results_state
      rs;

    void
      *store;

    // TRD : for the liblfds700 benchmarks
    lfds700_misc_library_init_valid_on_current_logical_core();

    libshared_memory_init( &ms_for_rs_and_ts );
    size_in_bytes = (otp->memory_in_megabytes / 2) * ONE_MEGABYTE_IN_BYTES;
    store = malloc( size_in_bytes );
    libshared_memory_add_memory( &ms_for_rs_and_ts, store, size_in_bytes );
    libbenchmark_topology_init( &ts, &ms_for_rs_and_ts );

    libshared_memory_init( &ms_for_benchmarks );

    #if( BENCHMARK_PAL_MEMORY_TYPE == BENCHMARK_MEMORY_TYPE_SMP )
      numa_mode = LIBBENCHMARK_TOPOLOGY_NUMA_MODE_SMP;
      size_in_bytes = (otp->memory_in_megabytes / 2) * ONE_MEGABYTE_IN_BYTES;
      store = malloc( size_in_bytes );
      libshared_memory_add_memory( &ms_for_benchmarks, store, size_in_bytes );
    #endif

    #if( BENCHMARK_PAL_MEMORY_TYPE == BENCHMARK_MEMORY_TYPE_NUMA )
    {
      lfds710_pal_uint_t
        numa_node_id,
        number_numa_nodes;

      struct libbenchmark_topology_iterate_state
        tis;

      struct libbenchmark_topology_node_state
        *tns;

      numa_mode = LIBBENCHMARK_TOPOLOGY_NUMA_MODE_NUMA;

      libbenchmark_topology_query( &ts, LIBBENCHMARK_TOPOLOGY_QUERY_GET_NUMBER_OF_NODE_TYPE, (void *) (lfds710_pal_uint_t) LIBBENCHMARK_TOPOLOGY_NODE_TYPE_NUMA, (void *) &number_numa_nodes );

      libbenchmark_topology_iterate_init( &tis, LIBBENCHMARK_TOPOLOGY_NODE_TYPE_NUMA );

      while( libbenchmark_topology_iterate(&ts, &tis, &tns) )
      {
        numa_node_id = LIBBENCHMARK_TOPOLOGY_NODE_GET_NUMA_ID( *tns );
        size_in_bytes = ( (otp->memory_in_megabytes / 2) * ONE_MEGABYTE_IN_BYTES ) / number_numa_nodes;
        store = benchmark_pal_numa_malloc( (int) numa_node_id, size_in_bytes );
        libshared_memory_add_memory_from_numa_node( &ms_for_benchmarks, numa_node_id, store, size_in_bytes );
      }
    }
    #endif

    if( otp->benchmark_duration_flag == RAISED )
      options_bitmask |= LIBBENCHMARK_BENCHMARKSUITE_OPTION_DURATION;

    libbenchmark_benchmarksuite_init( &bss, &ts, &ms_for_benchmarks, numa_mode, options_bitmask, otp->benchmark_duration_in_seconds );

    libbenchmark_results_init( &rs, &ms_for_rs_and_ts );

    libbenchmark_benchmarksuite_run( &bss, &rs );

    if( otp->gnuplot_file_flag == RAISED or otp->gnuplot_png_flag == RAISED )
    {
      char
        system_command[1024];

      struct libbenchmark_gnuplot_options
        gpo;

      LIBBENCHMARK_GNUPLOT_OPTIONS_INIT( gpo );

      if( otp->gnuplot_logarithmic_yaxis_flag == RAISED )
        LIBBENCHMARK_GNUPLOT_OPTIONS_SET_Y_AXIS_SCALE_TYPE_LOGARITHMIC( gpo );

      if( otp->gnuplot_height_flag == RAISED )
        LIBBENCHMARK_GNUPLOT_OPTIONS_SET_HEIGHT_IN_PIXELS( gpo, otp->gnuplot_height_in_pixels );

      if( otp->gnuplot_width_flag == RAISED )
        LIBBENCHMARK_GNUPLOT_OPTIONS_SET_WIDTH_IN_PIXELS( gpo, otp->gnuplot_width_in_pixels );

      libbenchmark_benchmarksuite_get_list_of_gnuplot_strings( &bss, &rs, otp->gnuplot_system_string, &gpo, &list_of_gnuplots );

      // TRD : write the gnuplot strings to disk
      while( LFDS710_LIST_ASU_GET_START_AND_THEN_NEXT(list_of_gnuplots,lasue) )
      {
        bg = LFDS710_LIST_ASU_GET_VALUE_FROM_ELEMENT( *lasue );

        diskfile = fopen( bg->filename, "w" );
        setbuf( diskfile, diskbuffer );
        // TRD : standard only requires fprintf() to support up to 509 characters of output
        fwrite( bg->gnuplot_string, libshared_ansi_strlen(bg->gnuplot_string), 1, diskfile );
        fclose( diskfile );

        if( otp->gnuplot_png_flag == RAISED )
        {
          sprintf( system_command, "gnuplot \"%s\"", bg->filename );
          system( system_command );
        }
      }
    }

    libbenchmark_results_cleanup( &rs );

    libbenchmark_benchmarksuite_cleanup( &bss );

    libshared_memory_cleanup( &ms_for_benchmarks, memory_cleanup_callback );

    libshared_memory_cleanup( &ms_for_rs_and_ts, memory_cleanup_callback );

    lfds700_misc_library_cleanup();
  }

  if( otp->show_version_flag == RAISED )
    internal_show_version();

  return;
}





/****************************************************************************/
#pragma warning( disable : 4100 )

static void memory_cleanup_callback( enum flag known_numa_node_flag, void *store, lfds710_pal_uint_t size_in_bytes )
{
  assert( store != NULL );
  // TRD : size_in_bytes can be any value in its range

  #if( BENCHMARK_PAL_MEMORY_TYPE == BENCHMARK_MEMORY_TYPE_SMP )
    free( store );
  #endif

  #if( BENCHMARK_PAL_MEMORY_TYPE == BENCHMARK_MEMORY_TYPE_NUMA )
    benchmark_pal_numa_free( store, size_in_bytes );
  #endif

  return;
}

#pragma warning( default : 4100 )

