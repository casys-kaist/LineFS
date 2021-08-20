/***** includes *****/
#include "libbenchmark_porting_abstraction_layer_internal.h"





/****************************************************************************/
#if( defined _MSC_VER )
  /* TRD : MSVC compiler

           an unfortunately necessary hack for MSVC
           MSVC only defines __STDC__ if /Za is given, where /Za turns off MSVC C extensions - 
           which prevents Windows header files from compiling.
  */

  #define __STDC__         1
  #define __STDC_HOSTED__  1
#endif





/****************************************************************************/
#if( defined __STDC__ && __STDC_HOSTED__ == 1 && !defined KERNEL_MODE )

  #define LIBBENCHMARK_PAL_PRINT_STRING

  #include <stdio.h>

  void libbenchmark_pal_print_string( char const * const string )
  {
    LFDS710_PAL_ASSERT( string != NULL );

    printf( "%s", string );

    fflush( stdout );

    return;
  }

#endif





/****************************************************************************/
#if( !defined LIBBENCHMARK_PAL_PRINT_STRING )

  #pragma warning( disable : 4100 )

  void libbenchmark_pal_print_string( char const * const string )
  {
    LFDS710_PAL_ASSERT( string != NULL );

    return;
  }

  #pragma warning( default : 4100 )

#endif

