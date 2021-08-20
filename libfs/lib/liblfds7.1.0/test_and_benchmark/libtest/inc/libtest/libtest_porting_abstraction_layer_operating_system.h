/****************************************************************************/
#if( defined _WIN32 && NTDDI_VERSION >= NTDDI_WINXP && !defined KERNEL_MODE )

  #ifdef LIBTEST_PAL_OPERATING_SYSTEM
    #error More than one porting abstraction layer matches current platform in "libtest_porting_abstraction_layer_operating_system.h".
  #endif

  #define LIBTEST_PAL_OPERATING_SYSTEM

  #include <stdlib.h>
  #include <time.h>
  #include <windows.h>

  #define LIBTEST_PAL_OS_STRING  "Windows"

#endif





/****************************************************************************/
#if( defined _WIN32 && NTDDI_VERSION >= NTDDI_WINXP && defined KERNEL_MODE )

  #ifdef LIBTEST_PAL_OPERATING_SYSTEM
    #error More than one porting abstraction layer matches current platform in "libtest_porting_abstraction_layer_operating_system.h".
  #endif

  #define LIBTEST_PAL_OPERATING_SYSTEM

  #include <stdlib.h>
  #include <time.h>
  #include <wdm.h>

  #define LIBTEST_PAL_OS_STRING  "Windows"

#endif





/****************************************************************************/
#if( defined __linux__ && !defined KERNEL_MODE )

  #ifdef LIBTEST_PAL_OPERATING_SYSTEM
    #error More than one porting abstraction layer matches current platform in "libtest_porting_abstraction_layer_operating_system.h".
  #endif

  #define LIBTEST_PAL_OPERATING_SYSTEM

  #define _GNU_SOURCE

  #include <unistd.h>
  #include <stdio.h>
  #include <stdlib.h>
  #include <time.h>

  #define LIBTEST_PAL_OS_STRING  "Linux"

#endif





/****************************************************************************/
#if( defined __linux__ && defined KERNEL_MODE )

  #ifdef LIBTEST_PAL_OPERATING_SYSTEM
    #error More than one porting abstraction layer matches current platform in "libtest_porting_abstraction_layer_operating_system.h".
  #endif

  #define LIBTEST_PAL_OPERATING_SYSTEM

  #error libtest not quite yet ready for Linux kernel - it uses time() all over the place

  #define LIBTEST_PAL_OS_STRING  "Linux"

#endif





/****************************************************************************/
#if( !defined LIBTEST_PAL_OPERATING_SYSTEM )

  #error No matching porting abstraction layer in "libtest_porting_abstraction_layer_operating_system.h".

#endif

