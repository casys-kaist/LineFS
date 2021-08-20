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

  #ifdef LIBTEST_PAL_MALLOC
    #error More than one porting abstraction layer matches the current platform in "libtest_porting_abstraction_layer_malloc.c".
  #endif

  #define LIBTEST_PAL_MALLOC

  void *libtest_pal_malloc( lfds710_pal_uint_t size )
  {
    void
      *rv;

    // TRD : size can be any value in its range

    rv = malloc( (size_t) size );

    return rv;
  }

#endif





/****************************************************************************/
#if( defined _WIN32 && defined KERNEL_MODE )

  #ifdef LIBTEST_PAL_MALLOC
    #error More than one porting abstraction layer matches the current platform in "libtest_porting_abstraction_layer_malloc.c".
  #endif

  #define LIBTEST_PAL_MALLOC

  void *libtest_pal_malloc( lfds710_pal_uint_t size )
  {
    void
      *rv;

    // TRD : size can be any value in its range

    /* TRD : if it assumed if lock-free data structures are being used
             it is because they will be accessed at DISPATCH_LEVEL
             and so the hard coded memory type is NonPagedPool
    */

    rv = ExAllocatePoolWithTag( NonPagedPool, size, 'sdfl' );

    return rv;
  }

#endif






/****************************************************************************/
#if( defined __linux__ && defined KERNEL_MODE )

  #ifdef LIBTEST_PAL_MALLOC
    #error More than one porting abstraction layer matches the current platform in "libtest_porting_abstraction_layer_malloc.c".
  #endif

  #define LIBTEST_PAL_MALLOC

  void *libtest_pal_malloc( lfds710_pal_uint_t size )
  {
    void
      *rv;

    // TRD : size can be any value in its range

    rv = vmalloc( (int long unsigned) size );

    return rv;
  }

#endif





/****************************************************************************/
#if( !defined LIBTEST_PAL_MALLOC )

  void *libtest_pal_malloc( lfds710_pal_uint_t size )
  {
    // TRD : size can be any value in its range

    return NULL;
  }

#endif

