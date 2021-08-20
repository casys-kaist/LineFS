/***** includes *****/
#include "internal.h"





/****************************************************************************/
void internal_show_version()
{
  char const
    *version_and_build_string;

  char static const
    * const local_build_and_version_string = "benchmark " BENCHMARK_VERSION_STRING " (" BUILD_TYPE_STRING ", " MODE_TYPE_STRING ", " BENCHMARK_PAL_MEMORY_TYPE_STRING ")";

  printf( "%s\n", local_build_and_version_string );

  libbenchmark_misc_query( LIBBENCHMARK_MISC_QUERY_GET_BUILD_AND_VERSION_STRING, NULL, (void **) &version_and_build_string );

  printf( "%s\n", version_and_build_string );

  libshared_misc_query( LIBSHARED_MISC_QUERY_GET_BUILD_AND_VERSION_STRING, NULL, (void **) &version_and_build_string );

  printf( "%s\n", version_and_build_string );

  lfds710_misc_query( LFDS710_MISC_QUERY_GET_BUILD_AND_VERSION_STRING, NULL, (void **) &version_and_build_string );

  printf( "%s\n", version_and_build_string );

  lfds700_misc_query( LFDS700_MISC_QUERY_GET_BUILD_AND_VERSION_STRING, NULL, (void **) &version_and_build_string );

  printf( "%s\n", version_and_build_string );

  return;
}

