/***** includes *****/
#include "libtest_porting_abstraction_layer_internal.h"

/* TRD : libtest_pal_malloc() and libtest_pal_free() are used for and only for
         one queue_umm test

         if either is not implemented, the test will not run

         that's the only impact of their presence or absence
*/





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
#if( defined __STDC__ && defined __STDC_HOSTED__ && __STDC_HOSTED__ == 1 && !defined KERNEL_MODE )

  #ifdef LIBTEST_PAL_FREE
    #error More than one porting abstraction layer matches the current platform in "libtest_porting_abstraction_layer_free.c".
  #endif

  #define LIBTEST_PAL_FREE

  void libtest_pal_free( void *memory )
  {
    LFDS710_PAL_ASSERT( memory != NULL );

    free( memory );

    return;
  }

#endif





/****************************************************************************/
#if( defined _WIN32 && defined KERNEL_MODE )

  #ifdef LIBTEST_PAL_FREE
    #error More than one porting abstraction layer matches the current platform in "libtest_porting_abstraction_layer_free.c".
  #endif

  #define LIBTEST_PAL_FREE

  void libtest_pal_free( void *memory )
  {
    LFDS710_PAL_ASSERT( memory != NULL );

    ExFreePoolWithTag( memory, 'sdfl' );

    return;
  }

#endif





/****************************************************************************/
#if( defined __linux__ && defined KERNEL_MODE )

  #ifdef LIBTEST_PAL_FREE
    #error More than one porting abstraction layer matches the current platform in "libtest_porting_abstraction_layer_free.c".
  #endif

  #define LIBTEST_PAL_FREE

  void libtest_pal_free( void *memory )
  {
    LFDS710_PAL_ASSERT( memory != NULL );

    vfree( memory );

    return;
  }

#endif





/****************************************************************************/
#if( !defined LIBTEST_PAL_FREE )

  void libtest_pal_free( void *memory )
  {
    LFDS710_PAL_ASSERT( memory != NULL );

    return;
  }

#endif

