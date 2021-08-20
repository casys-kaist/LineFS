/****************************************************************************/
#if( defined _WIN32 && NTDDI_VERSION >= NTDDI_WINXP && !defined KERNEL_MODE )

  /* TRD : Windows XP or better

           _WIN32         indicates 64-bit or 32-bit Windows
           NTDDI_VERSION  indicates Windows version
                            - CreateMutex requires XP
                            - CloseHandle requires XP
                            - WaitForSingleObject requires XP
                            - ReleaseMutex requires XP
  */

  /***** defines *****/
  #define LIBBENCHMARK_PAL_LOCK_WINDOWS_MUTEX 1

  #define LIBBENCHMARK_PAL_LOCK_WINDOWS_MUTEX_CREATE( pal_lock_windows_mutex_state )   pal_lock_windows_mutex_state = CreateMutex( NULL, FALSE, NULL )
  #define LIBBENCHMARK_PAL_LOCK_WINDOWS_MUTEX_DESTROY( pal_lock_windows_mutex_state )  CloseHandle( pal_lock_windows_mutex_state )
  #define LIBBENCHMARK_PAL_LOCK_WINDOWS_MUTEX_GET( pal_lock_windows_mutex_state )      WaitForSingleObject( pal_lock_windows_mutex_state, INFINITE )
  #define LIBBENCHMARK_PAL_LOCK_WINDOWS_MUTEX_RELEASE( pal_lock_windows_mutex_state )  ReleaseMutex( pal_lock_windows_mutex_state )

  /***** typedefs *****/
  typedef HANDLE pal_lock_windows_mutex_state;

#endif





/****************************************************************************/
#if( !defined LIBBENCHMARK_PAL_LOCK_WINDOWS_MUTEX )

  /***** defines *****/
  #define LIBBENCHMARK_PAL_LOCK_WINDOWS_MUTEX 0

  #define LIBBENCHMARK_PAL_LOCK_WINDOWS_MUTEX_CREATE( pal_lock_windows_mutex_state )
  #define LIBBENCHMARK_PAL_LOCK_WINDOWS_MUTEX_DESTROY( pal_lock_windows_mutex_state )
  #define LIBBENCHMARK_PAL_LOCK_WINDOWS_MUTEX_GET( pal_lock_windows_mutex_state )
  #define LIBBENCHMARK_PAL_LOCK_WINDOWS_MUTEX_RELEASE( pal_lock_windows_mutex_state )

  /***** typedefs *****/
  typedef void * pal_lock_windows_mutex_state;

#endif

