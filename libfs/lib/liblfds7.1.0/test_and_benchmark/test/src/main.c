/***** includes *****/
#include "internal.h"





/****************************************************************************/
int main( int argc, char **argv )
{
  enum flag
    determine_erg_flag = LOWERED,
    run_flag = LOWERED,
    show_error_flag = LOWERED,
    show_help_flag = LOWERED,
    show_version_flag = LOWERED;

  int
    rv;

  lfds710_pal_uint_t
    loop,
    iterations = 1,
    memory_in_megabytes = TEST_DEFAULT_TEST_MEMORY_IN_MEGABYTES;

  struct util_cmdline_state
    cs;

  union util_cmdline_arg_data
    *arg_data;

  assert( argc >= 1 );
  assert( argv != NULL );

  util_cmdline_init( &cs );

  util_cmdline_add_arg( &cs, 'e', UTIL_CMDLINE_ARG_TYPE_FLAG );
  util_cmdline_add_arg( &cs, 'h', UTIL_CMDLINE_ARG_TYPE_FLAG );
  util_cmdline_add_arg( &cs, 'i', UTIL_CMDLINE_ARG_TYPE_INTEGER );
  util_cmdline_add_arg( &cs, 'm', UTIL_CMDLINE_ARG_TYPE_INTEGER );
  util_cmdline_add_arg( &cs, 'r', UTIL_CMDLINE_ARG_TYPE_FLAG );
  util_cmdline_add_arg( &cs, 'v', UTIL_CMDLINE_ARG_TYPE_FLAG );

  rv = util_cmdline_process_args( &cs, argc, argv );

  if( rv == 0 )
    show_error_flag = RAISED;

  if( rv == 1 )
  {
    util_cmdline_get_arg_data( &cs, 'e', &arg_data );
    if( arg_data != NULL )
      determine_erg_flag = RAISED;

    util_cmdline_get_arg_data( &cs, 'h', &arg_data );
    if( arg_data != NULL )
      show_help_flag = RAISED;

    util_cmdline_get_arg_data( &cs, 'i', &arg_data );
    if( arg_data != NULL )
    {
      if( arg_data->integer.integer < 1 )
      {
        puts( "Number of iterations needs to be 1 or greater." );
        exit( EXIT_FAILURE );
      }

      iterations = (lfds710_pal_uint_t) arg_data->integer.integer;
    }

    util_cmdline_get_arg_data( &cs, 'm', &arg_data );
    if( arg_data != NULL )
    {
      if( arg_data->integer.integer < 32 )
      {
        puts( "Memory for tests needs to be 32 or greater." );
        exit( EXIT_FAILURE );
      }

      memory_in_megabytes = (lfds710_pal_uint_t) arg_data->integer.integer;
    }

    util_cmdline_get_arg_data( &cs, 'r', &arg_data );
    if( arg_data != NULL )
      run_flag = RAISED;

    util_cmdline_get_arg_data( &cs, 'v', &arg_data );
    if( arg_data != NULL )
      show_version_flag = RAISED;
  }

  util_cmdline_cleanup( &cs );

  if( argc == 1 or (run_flag == LOWERED and show_version_flag == LOWERED) )
    show_help_flag = RAISED;

  if( show_error_flag == RAISED )
  {
    printf( "\nInvalid arguments.  Sorry - it's a simple parser, so no clues.\n"
            "-h or run with no args to see the help text.\n" );

    return EXIT_SUCCESS;
  }

  if( determine_erg_flag == RAISED )
  {
    enum libtest_misc_determine_erg_result
      der;

    lfds710_pal_uint_t
      count_array[10],
      erg_size_in_bytes;

    struct libshared_memory_state
      ms;

    void
      *memory;

    memory = malloc( ONE_MEGABYTE_IN_BYTES );

    libshared_memory_init( &ms );

    libshared_memory_add_memory( &ms, memory, ONE_MEGABYTE_IN_BYTES );

    libtest_misc_determine_erg( &ms, &count_array, &der, &erg_size_in_bytes );

    if( der == LIBTEST_MISC_DETERMINE_ERG_RESULT_NOT_SUPPORTED )
      printf( "Determine ERG not supported on the current platform.\n" );
    else
    {
      printf( "\n"
              "Results\n"
              "=======\n"
              "\n" );

      printf( "  ERG length in bytes : Number successful LL/SC ops\n"
              "  =================================================\n" );

      for( loop = 0 ; loop < 10 ; loop++ )
        printf( "  %lu bytes : %llu\n", 1UL << (loop+2), (int long long unsigned) count_array[loop] );

      printf( "\n"
              "Conclusions\n"
              "===========\n"
              "\n" );

      switch( der )
      {
        case LIBTEST_MISC_DETERMINE_ERG_RESULT_SUCCESS:
          printf( "  The smallest ERG size with successful results is %llu bytes, which\n"
                  "  is therefore hopefully if all has gone well the ERG size.\n"
                  "  \n"
                  "  In the file 'lfds710_porting_abstraction_layer_processor.h', in all\n"
                  "  the sections for ARM, please replace the existing the value of the\n"
                  "  #define LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES to %llu, like so;\n"
                  "  \n"
                  "  #define LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES %llu\n",
                  (int long long unsigned) erg_size_in_bytes,
                  (int long long unsigned) erg_size_in_bytes,
                  (int long long unsigned) erg_size_in_bytes );
        break;

        case LIBTEST_MISC_DETERMINE_ERG_RESULT_ONE_PHYSICAL_CORE:
          printf( "  This system has only one physical core, and as such this\n"
                  "  code cannot determine the ERG.\n" );
        break;

        case LIBTEST_MISC_DETERMINE_ERG_RESULT_ONE_PHYSICAL_CORE_OR_NO_LLSC:
          printf( "  The results are indeterminate.  Either this system has only one\n"
                  "  physical core, and as such this code cannot determine the ERG, or\n"
                  "  the system has no support for LL/SC.\n" );
        break;

        case LIBTEST_MISC_DETERMINE_ERG_RESULT_NO_LLSC:
          printf( "  There appears to be no LL/SC support on the current platform.\n" );
        break;

        case LIBTEST_MISC_DETERMINE_ERG_RESULT_NOT_SUPPORTED:
          printf( "  Determine ERG not supported on the current platform.\n" );
        break;
      }

      printf( "\n"
              "Explanations\n"
              "============\n"
              "\n"
              "  This code is for ARM and works to empirically determine the ERG size,\n"
              "  which is the value which needs to be used for the #define\n"
              "  LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES in the file\n"
              "  'lfds710_porting_abstraction_layer_processor.h'.\n"
              "  \n"
              "  It is VERY VERY IMPORTANT to set this value because the default on ARM\n"
              "  is the worst-case, which is 2048 bytes, and this makes all the lfds\n"
              "  data structure structs HUGE.\n"
              "  \n"
              "  If this value is set too small, then absolutely none of the liblfds data\n"
              "  structures should work *at all*, so getting it wrong should be very obvious.\n"
              "  \n"
              "  Each ERG length is tried 1024 times.  All ERG sizes which are smaller than the\n"
              "  actual ERG size should have 0 successful ops.  All ERG sizes equal to or\n"
              "  greater than the actual ERG size should have 1024, or maybe a few less,\n"
              "  successful ops.  A few spurious failures are not unusual, it's the\n"
              "  nature of LL/SC, so it's normal.  The correct ERG size then is the smallest\n"
              "  size which has about 1024 successful ops.\n"
              "  \n"
              "  This code however can only work if there are at least two physical cores\n"
              "  in the system.  It's not enough to have one physical core with multiple\n"
              "  logical cores.  If the ERG size of 4 bytes has any successes, then the\n"
              "  current systems has only a single physical processor, or it has no\n"
              "  support for LL/SC (which can happen - there are SO many ARM system\n"
              "  variants).\n" );
    }

    return EXIT_SUCCESS;
  }

  if( show_help_flag == RAISED )
  {
    printf( "test -e -h -i [n] -m [n] -r -v\n"
            "  -e     : empirically determine Exclusive Reservation Granule\n"
            "           (currently supports only ARM32)\n"
            "  -h     : help\n"
            "  -i [n] : number of iterations     (default : 1)\n"
            "  -m [n] : memory for tests, in mb  (default : %u)\n"
            "  -r     : run (causes test to run; present so no args gives help)\n"
            "  -v     : version\n", (int unsigned) TEST_DEFAULT_TEST_MEMORY_IN_MEGABYTES );

    return EXIT_SUCCESS;
  }

  if( show_version_flag == RAISED )
  {
    internal_show_version();
    return EXIT_SUCCESS;
  }

  if( run_flag == RAISED )
  {
    struct libshared_memory_state
      ms;

    struct libtest_results_state
      rs;

    struct libtest_testsuite_state
      ts;

    void
      *test_memory;

    test_memory = malloc( memory_in_megabytes * ONE_MEGABYTE_IN_BYTES );

    libshared_memory_init( &ms );

    libshared_memory_add_memory( &ms, test_memory, memory_in_megabytes * ONE_MEGABYTE_IN_BYTES );

    libtest_testsuite_init( &ts, &ms, callback_test_start, callback_test_finish );

    for( loop = 0 ; loop < (lfds710_pal_uint_t) iterations ; loop++ )
    {
      libtest_results_init( &rs );

      printf( "\n"
              "Test Iteration %02llu\n"
              "=================\n", (int long long unsigned) (loop+1) );

      libtest_testsuite_run( &ts, &rs );

      libtest_results_cleanup( &rs );
    }

    libtest_testsuite_cleanup( &ts );

    libshared_memory_cleanup( &ms, NULL );

    free( test_memory );
  }

  return EXIT_SUCCESS;
}

