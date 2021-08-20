/****************************************************************************/
#if( defined _WIN32 && NTDDI_VERSION >= NTDDI_WINXP && !defined KERNEL_MODE )

  #ifdef LIBSHARED_PAL_OPERATING_SYSTEM
    #error More than one porting abstraction layer matches current platform in "libshared_porting_abstraction_layer_operating_system.h".
  #endif

  #define LIBSHARED_PAL_OPERATING_SYSTEM

  #define LIBSHARED_PAL_OS_STRING  "Windows"

  #include <windows.h>

  typedef HANDLE  libshared_pal_thread_handle_t;
  typedef DWORD   libshared_pal_thread_return_t;

  #define LIBSHARED_PAL_THREAD_CALLING_CONVENTION           WINAPI
  #define LIBSHARED_PAL_THREAD_RETURN_TYPE                  libshared_pal_thread_return_t
  #define LIBSHARED_PAL_THREAD_RETURN_CAST( return_value )  return_value

#endif





/****************************************************************************/
#if( defined _WIN32 && NTDDI_VERSION >= NTDDI_WINXP && defined KERNEL_MODE )

  #ifdef LIBSHARED_PAL_OPERATING_SYSTEM
    #error More than one porting abstraction layer matches current platform in "libshared_porting_abstraction_layer_operating_system.h".
  #endif

  #define LIBSHARED_PAL_OPERATING_SYSTEM

  #define LIBSHARED_PAL_OS_STRING  "Windows"

  #include <wdm.h>

  typedef HANDLE libshared_pal_thread_handle_t;
  typedef VOID   libshared_pal_thread_return_t;

  #define LIBSHARED_PAL_THREAD_CALLING_CONVENTION
  #define LIBSHARED_PAL_THREAD_RETURN_TYPE                  int
  #define LIBSHARED_PAL_THREAD_RETURN_CAST( return_value )

#endif





/****************************************************************************/
#if( defined __linux__ && !defined KERNEL_MODE )

  #ifdef LIBSHARED_PAL_OPERATING_SYSTEM
    #error More than one porting abstraction layer matches current platform in "libshared_porting_abstraction_layer_operating_system.h".
  #endif

  #define LIBSHARED_PAL_OPERATING_SYSTEM

  #define _GNU_SOURCE
  #include <unistd.h>

  #define LIBSHARED_PAL_OS_STRING  "Linux"

  #if( _POSIX_THREADS >= 0 )
    #include <pthread.h>
    #include <sched.h>
    #include <sys/syscall.h>
    #include <sys/types.h>

    typedef pthread_t  libshared_pal_thread_handle_t;
    typedef void *     libshared_pal_thread_return_t;

    #define LIBSHARED_PAL_THREAD_CALLING_CONVENTION
    #define LIBSHARED_PAL_THREAD_RETURN_TYPE              libshared_pal_thread_return_t
    #define LIBSHARED_PAL_THREAD_RETURN_CAST( return_value )  ( (libshared_pal_thread_return_t) (return_value) )
  #endif

  #if( _POSIX_THREADS == -1 )
    #error No pthread support under Linux in libshared_porting_abstraction_layer_operating_system.h
  #endif

#endif





/****************************************************************************/
#if( defined __linux__ && defined KERNEL_MODE )

  #ifdef LIBSHARED_PAL_OPERATING_SYSTEM
    #error More than one porting abstraction layer matches current platform. in "libshared_porting_abstraction_layer_operating_system.h".
  #endif

  #define LIBSHARED_PAL_OPERATING_SYSTEM

  #define LIBSHARED_PAL_OS_STRING  "Linux"

  #define _GNU_SOURCE

  #include <linux/module.h>
  #include <linux/kthread.h>

  typedef struct task_struct * libshared_pal_thread_handle_t;
  typedef int                  libshared_pal_thread_return_t;

  #define LIBSHARED_PAL_THREAD_CALLING_CONVENTION  
  #define LIBSHARED_PAL_THREAD_RETURN_TYPE              libshared_pal_thread_return_t
  #define LIBSHARED_PAL_THREAD_RETURN_CAST( return_value )  ( (libshared_pal_thread_return_t) (return_value) )

#endif





/****************************************************************************/
#if( !defined LIBSHARED_PAL_OPERATING_SYSTEM )

  #error No matching porting abstraction layer in "libshared_porting_abstraction_layer_operating_system.h".

#endif

