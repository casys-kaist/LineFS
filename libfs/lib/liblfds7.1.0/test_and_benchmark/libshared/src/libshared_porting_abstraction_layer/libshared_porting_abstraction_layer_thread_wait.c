/***** includes *****/
#include "libshared_porting_abstraction_layer_internal.h"





/****************************************************************************/
#if( defined _WIN32 && !defined KERNEL_MODE && NTDDI_VERSION >= NTDDI_WINXP )

  #ifdef LIBSHARED_PAL_THREAD_WAIT
    #error More than one porting abstraction layer matches current platform in "libshared_porting_abstraction_layer_thread_wait.c".
  #endif

  #define LIBSHARED_PAL_THREAD_WAIT

  void libshared_pal_thread_wait( libshared_pal_thread_handle_t thread_handle )
  {
    // TRD : thread_handle can be any value in its range

    WaitForSingleObject( thread_handle, INFINITE );

    return;
  }

#endif





/****************************************************************************/
#if( defined _WIN32 && defined KERNEL_MODE && NTDDI_VERSION >= NTDDI_WINXP )

  #ifdef LIBSHARED_PAL_THREAD_WAIT
    #error More than one porting abstraction layer matches current platform in "libshared_porting_abstraction_layer_thread_wait.c".
  #endif

  #define LIBSHARED_PAL_THREAD_WAIT

  void libshared_pal_thread_wait( libshared_pal_thread_handle_t thread_handle )
  {
    // TRD : thread_handle can be any value in its range

    KeWaitForSingleObject( thread_handle, Executive, KernelMode, FALSE, NULL );

    return;
  }

#endif





/****************************************************************************/
#if( defined _POSIX_THREADS && _POSIX_THREADS > 0 && !defined KERNEL_MODE )

  #ifdef LIBSHARED_PAL_THREAD_WAIT
    #error More than one porting abstraction layer matches current platform in "libshared_porting_abstraction_layer_thread_wait.c".
  #endif

  #define LIBSHARED_PAL_THREAD_WAIT

  void libshared_pal_thread_wait( libshared_pal_thread_handle_t thread_handle )
  {
    // TRD : thread_handle can be any value in its range

    pthread_join( thread_handle, NULL );

    return;
  }

#endif





/****************************************************************************/
#if( defined __linux__ && defined KERNEL_MODE )

  #ifdef LIBSHARED_PAL_THREAD_WAIT
    #error More than one porting abstraction layer matches current platform in "libshared_porting_abstraction_layer_thread_wait.c".
  #endif

  #define LIBSHARED_PAL_THREAD_WAIT

  void libshared_pal_thread_wait( libshared_pal_thread_handle_t thread_handle )
  {
    // TRD : thread_handle can be any value in its range

    /* TRD : turns out this function does not exist in the linux kernel
             you have to manage your own inter-thread sync
             that breaks the lfds abstraction for thread start/wait
             so this isn't going to get fixed in time for this release
             leaving the function here so compilation will pass
    */

    return;
  }

#endif





/****************************************************************************/
#if( !defined LIBSHARED_PAL_THREAD_WAIT )

  #error No matching porting abstraction layer in "libshared_porting_abstraction_layer_thread_wait.c".

#endif

