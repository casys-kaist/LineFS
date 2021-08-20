/****************************************************************************/
#if( defined _WIN32 && NTDDI_VERSION >= NTDDI_WINXP && !defined KERNEL_MODE )

  /* TRD : Windows XP or better

           _WIN32         indicates 64-bit or 32-bit Windows
           NTDDI_VERSION  indicates Windows version
                            - InitializeCriticalSection requires XP
                            - DeleteCriticalSection requires XP
                            - EnterCriticalSection requires XP
                            - LeaveCriticalSection requires XP
  */

  /***** defines *****/
  #define LIBBENCHMARK_PAL_LOCK_WINDOWS_CRITICAL_SECTION 1

  #define LIBBENCHMARK_PAL_LOCK_WINDOWS_CRITICAL_SECTION_CREATE( pal_lock_windows_critical_section_state )   InitializeCriticalSection( &pal_lock_windows_critical_section_state )
  #define LIBBENCHMARK_PAL_LOCK_WINDOWS_CRITICAL_SECTION_DESTROY( pal_lock_windows_critical_section_state )  DeleteCriticalSection( &pal_lock_windows_critical_section_state )
  #define LIBBENCHMARK_PAL_LOCK_WINDOWS_CRITICAL_SECTION_GET( pal_lock_windows_critical_section_state )      EnterCriticalSection( &pal_lock_windows_critical_section_state )
  #define LIBBENCHMARK_PAL_LOCK_WINDOWS_CRITICAL_SECTION_RELEASE( pal_lock_windows_critical_section_state )  LeaveCriticalSection( &pal_lock_windows_critical_section_state )

  /***** typedefs *****/
  typedef CRITICAL_SECTION pal_lock_windows_critical_section_state;

#endif





/****************************************************************************/
#if( !defined LIBBENCHMARK_PAL_LOCK_WINDOWS_CRITICAL_SECTION )

  /***** defines *****/
  #define LIBBENCHMARK_PAL_LOCK_WINDOWS_CRITICAL_SECTION 0

  #define LIBBENCHMARK_PAL_LOCK_WINDOWS_CRITICAL_SECTION_CREATE( pal_lock_windows_critical_section_state )
  #define LIBBENCHMARK_PAL_LOCK_WINDOWS_CRITICAL_SECTION_DESTROY( pal_lock_windows_critical_section_state )
  #define LIBBENCHMARK_PAL_LOCK_WINDOWS_CRITICAL_SECTION_GET( pal_lock_windows_critical_section_state )
  #define LIBBENCHMARK_PAL_LOCK_WINDOWS_CRITICAL_SECTION_RELEASE( pal_lock_windows_critical_section_state )

  /***** typedefs *****/
  typedef void * pal_lock_windows_critical_section_state;

#endif

