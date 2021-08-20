/***** includes *****/
#include "libshared_porting_abstraction_layer_internal.h"





/****************************************************************************/
#if( defined _WIN32 && NTDDI_VERSION >= NTDDI_WIN7 && !defined KERNEL_MODE )

  #ifdef LIBSHARED_PAL_THREAD_START
    #error More than one porting abstraction layer matches the current platform in "libshared_porting_abstraction_layer_thread_start.c".
  #endif

  #define LIBSHARED_PAL_THREAD_START

  int libshared_pal_thread_start( libshared_pal_thread_handle_t *thread_handle,
                                  struct libshared_pal_thread_info *pti )
  {
    BOOL
      brv;

    DWORD
      thread_id;

    GROUP_AFFINITY
      ga;

    int
      rv = 0;

    LPPROC_THREAD_ATTRIBUTE_LIST
      attribute_list;

    SIZE_T
      attribute_list_length;

    LFDS710_PAL_ASSERT( thread_handle != NULL );
    LFDS710_PAL_ASSERT( pti != NULL );

    /* TRD : here we're using CreateRemoteThreadEx() to start a thread in our own process
             we do this because as a function, it allows us to specify processor and processor group affinity in the create call
    */

    brv = InitializeProcThreadAttributeList( NULL, 1, 0, &attribute_list_length );
    attribute_list = VirtualAlloc( NULL, attribute_list_length, MEM_COMMIT, PAGE_READWRITE );
    brv = InitializeProcThreadAttributeList( attribute_list, 1, 0, &attribute_list_length );

    ga.Mask = ( (KAFFINITY) 1 << LIBSHARED_PAL_PTI_GET_LOGICAL_PROCESSOR_NUMBER(*pti) );
    ga.Group = (WORD) LIBSHARED_PAL_PTI_GET_WINDOWS_PROCESSOR_GROUP_NUMBER(*pti);
    ga.Reserved[0] = ga.Reserved[1] = ga.Reserved[2] = 0;

    brv = UpdateProcThreadAttribute( attribute_list, 0, PROC_THREAD_ATTRIBUTE_GROUP_AFFINITY, &ga, sizeof(GROUP_AFFINITY), NULL, NULL );
    *thread_handle = CreateRemoteThreadEx( GetCurrentProcess(), NULL, 0, LIBSHARED_PAL_PTI_GET_THREAD_FUNCTION(*pti), LIBSHARED_PAL_PTI_GET_THREAD_ARGUMENT(*pti), NO_FLAGS, attribute_list, &thread_id );

    DeleteProcThreadAttributeList( attribute_list );
    VirtualFree( attribute_list, 0, MEM_RELEASE );

    if( *thread_handle != NULL )
      rv = 1;

    return rv;
  }

#endif





/****************************************************************************/
#if( defined _WIN32 && NTDDI_VERSION >= NTDDI_WINXP && NTDDI_VERSION < NTDDI_WIN7 && !defined KERNEL_MODE )

  #ifdef LIBSHARED_PAL_THREAD_START
    #error More than one porting abstraction layer matches the current platform in "libshared_porting_abstraction_layer_thread_start.c".
  #endif

  #define LIBSHARED_PAL_THREAD_START

  int libshared_pal_thread_start( libshared_pal_thread_handle_t *thread_handle,
                                  struct libshared_pal_thread_info *pti )
  {
    int
      rv = 0;

    DWORD
      thread_id;

    DWORD_PTR
      affinity_mask,
      result;

    LFDS710_PAL_ASSERT( thread_handle != NULL );
    LFDS710_PAL_ASSERT( pti != NULL );

    /* TRD : Vista and earlier do not support processor groups
             as such, there is a single implicit processor group
             also, there's no support for actually starting a thread in its correct NUMA node / logical processor
             so we make the best of it; we start suspended, set the affinity, and then resume
             the thread itself internally is expected to be making allocs from the correct NUMA node
    */

    *thread_handle = CreateThread( NULL, 0, LIBSHARED_PAL_PTI_GET_THREAD_FUNCTION(*pti), LIBSHARED_PAL_PTI_GET_THREAD_ARGUMENT(*pti), CREATE_SUSPENDED, &thread_id );

    if( *thread_handle != NULL )
    {
      affinity_mask = (DWORD_PTR) (1 << LIBSHARED_PAL_PTI_GET_LOGICAL_PROCESSOR_NUMBER(*pti));
      SetThreadAffinityMask( *thread_handle, affinity_mask );
      ResumeThread( *thread_handle );
      rv = 1;
    }

    return rv;
  }

#endif






/****************************************************************************/
#if( defined _WIN32 && NTDDI_VERSION >= NTDDI_WINXP && NTDDI_VERSION < NTDDI_WIN7 && defined KERNEL_MODE )

  #ifdef LIBSHARED_PAL_THREAD_START
    #error More than one porting abstraction layer matches the current platform in "libshared_porting_abstraction_layer_thread_start.c".
  #endif

  #define LIBSHARED_PAL_THREAD_START

  /***** prototypes *****/
  static libshared_pal_thread_return_t LIBSHARED_PAL_THREAD_CALLING_CONVENTION test_pal_internal_thread_function( void *thread_user_state );

  int libshared_pal_thread_start( libshared_pal_thread_handle_t *thread_handle,
                                  struct libshared_pal_thread_info *pti )
  {
    int
      rv = 0;

    NTSTATUS
      nts_create;

    OBJECT_ATTRIBUTES
      oa;

    assert( thread_state != NULL );
    // TRD : cpu can be any value in its range
    assert( thread_function != NULL );
    // TRD : thread_user_state can be NULL

    InitializeObjectAttributes( &oa, NULL, OBJ_KERNEL_HANDLE, NULL, NULL );

    nts_create = PsCreateSystemThread( thread_handle, THREAD_ALL_ACCESS, &oa, NtCurrentProcess(), NULL, test_pal_internal_thread_function, pti );

    if( nts_create == STATUS_SUCCESS )
      rv = 1;

    return rv;
  }

  /****************************************************************************/
  static libshared_pal_thread_return_t LIBSHARED_PAL_THREAD_CALLING_CONVENTION test_pal_internal_thread_function( void *thread_user_state )
  {
    KAFFINITY
      affinity;

    struct libshared_pal_thread_info
      *pti;

    LFDS710_PAL_ASSERT( thread_user_state != NULL );

    pti = (struct libshared_pal_thread_info *) thread_user_state;

    affinity = 1 << LIBSHARED_PAL_PTI_GET_LOGICAL_PROCESSOR_NUMBER(*pti);

    KeSetSystemAffinityThread( affinity );

    LIBSHARED_PAL_PTI_GET_THREAD_FUNCTION(*pti)( LIBSHARED_PAL_PTI_GET_THREAD_ARGUMENT(*pti) );

    return;
  }

#endif





/****************************************************************************/
#if( defined _WIN32 && NTDDI_VERSION >= NTDDI_WIN7 && defined KERNEL_MODE )

  #ifdef LIBSHARED_PAL_THREAD_START
    #error More than one porting abstraction layer matches the current platform in "libshared_porting_abstraction_layer_thread_start.c".
  #endif

  #define LIBSHARED_PAL_THREAD_START

  /***** prototypes *****/
  static libshared_pal_thread_return_t LIBSHARED_PAL_THREAD_CALLING_CONVENTION test_pal_internal_thread_function( void *thread_user_state );

  int libshared_pal_thread_start( libshared_pal_thread_handle_t *thread_handle,
                                  struct libshared_pal_thread_info *pti )
  {
    int
      rv = 0;

    NTSTATUS
      nts_create;

    OBJECT_ATTRIBUTES
      oa;

    assert( thread_state != NULL );
    // TRD : cpu can be any value in its range
    assert( thread_function != NULL );
    // TRD : thread_user_state can be NULL

    InitializeObjectAttributes( &oa, NULL, OBJ_KERNEL_HANDLE, NULL, NULL );

    nts_create = PsCreateSystemThread( thread_handle, THREAD_ALL_ACCESS, &oa, NtCurrentProcess(), NULL, test_pal_internal_thread_function, pti );

    if( nts_create == STATUS_SUCCESS )
      rv = 1;

    return rv;
  }

  /****************************************************************************/
  static libshared_pal_thread_return_t LIBSHARED_PAL_THREAD_CALLING_CONVENTION test_pal_internal_thread_function( void *thread_user_state )
  {
    GROUP_AFFINITY
      group_affinity,
      previous_group_affinity;

    struct libshared_pal_thread_info
      *pti;

    LFDS710_PAL_ASSERT( thread_user_state != NULL );

    pti = (struct libshared_pal_thread_info *) thread_user_state;

    KeSetSystemGroupAffinityThread( &group_affinity, &previous_group_affinity );

    group_affinity.Mask = ( (KAFFINITY) 1 ) << LIBSHARED_PAL_PTI_GET_LOGICAL_PROCESSOR_NUMBER(*pti);
    // TRD : Group is a WORD in the user-mode MS docs, but a USHORT in WDK7.1 headers
    group_affinity.Group = (USHORT) LIBSHARED_PAL_PTI_GET_WINDOWS_PROCESSOR_GROUP_NUMBER(*pti);
    group_affinity.Reserved[0] = group_affinity.Reserved[1] = group_affinity.Reserved[2] = 0;

    LIBSHARED_PAL_PTI_GET_THREAD_FUNCTION(*pti)( LIBSHARED_PAL_PTI_GET_THREAD_ARGUMENT(*pti) );

    return;
  }

#endif





/****************************************************************************/
#if( defined __linux__ && defined _POSIX_THREADS && _POSIX_THREADS > 0 && !defined KERNEL_MODE )

  #ifdef LIBSHARED_PAL_THREAD_START
    #error More than one porting abstraction layer matches the current platform in "libshared_porting_abstraction_layer_thread_start.c".
  #endif

  #define LIBSHARED_PAL_THREAD_START

  /***** prototypes *****/
  static libshared_pal_thread_return_t LIBSHARED_PAL_THREAD_CALLING_CONVENTION test_pal_internal_thread_function( void *thread_user_state );

  /****************************************************************************/
  int libshared_pal_thread_start( libshared_pal_thread_handle_t *thread_handle,
                                  struct libshared_pal_thread_info *pti )
  {
    int
      rv;

    /* TRD : this implementation exists because the pthreads function for setting thread affinity,
             pthread_attr_setaffinity_np(), works on Linux, but not Android
    */

    LFDS710_PAL_ASSERT( thread_handle != NULL );
    LFDS710_PAL_ASSERT( pti != NULL );

    rv = pthread_create( thread_handle, NULL, test_pal_internal_thread_function, pti );

    if( rv == 0 )
      rv = 1;

    return rv;
  }

  /****************************************************************************/
  static libshared_pal_thread_return_t LIBSHARED_PAL_THREAD_CALLING_CONVENTION test_pal_internal_thread_function( void *thread_user_state )
  {
    cpu_set_t
      cpuset;

    pid_t
      tid;

    struct libshared_pal_thread_info
      *pti;

    LIBSHARED_PAL_THREAD_RETURN_TYPE
      rv;

    LFDS710_PAL_ASSERT( thread_user_state != NULL );

    /* TRD : the APIs under Linux/POSIX for setting thread affinity are in a mess

             pthreads offers pthread_attr_setaffinity_np(), which glibc supports, but which is not supported by Android

             Linux offers sched_setaffinity(), but this needs a *thread pid*,
             and the only API to get a thread pid is gettid(), which works for
             and only for *the calling thread*

             so we come to this - a wrapper thread function, which is the function used
             when starting a thread; this calls gettid() and then sched_setaffinity(),
             and then calls into the actual thread function

             generally shaking my head in disbelief at this point
    */

    pti = (struct libshared_pal_thread_info *) thread_user_state;

    CPU_ZERO( &cpuset );
    CPU_SET( LIBSHARED_PAL_PTI_GET_LOGICAL_PROCESSOR_NUMBER(*pti), &cpuset );

    tid = syscall( SYS_gettid );

    sched_setaffinity( tid, sizeof(cpu_set_t), &cpuset );

    rv = LIBSHARED_PAL_PTI_GET_THREAD_FUNCTION(*pti)( LIBSHARED_PAL_PTI_GET_THREAD_ARGUMENT(*pti) );

    return LIBSHARED_PAL_THREAD_RETURN_CAST(rv);
  }

#endif





/****************************************************************************/
#if( !defined __linux__ && defined _POSIX_THREADS && _POSIX_THREADS > 0 && !defined KERNEL_MODE )

  #ifdef LIBSHARED_PAL_THREAD_START
    #error More than one porting abstraction layer matches the current platform in "libshared_porting_abstraction_layer_thread_start.c".
  #endif

  #define LIBSHARED_PAL_THREAD_START

  int libshared_pal_thread_start( libshared_pal_thread_handle_t *thread_handle,
                                  struct libshared_pal_thread_info *pti )
  {
    int
      rv = 0,
      rv_create;

    cpu_set_t
      cpuset;

    pthread_attr_t
      attr;

    LFDS710_PAL_ASSERT( thread_handle != NULL );
    LFDS710_PAL_ASSERT( pti != NULL );

    pthread_attr_init( &attr );

    CPU_ZERO( &cpuset );
    CPU_SET( LIBSHARED_PAL_PTI_GET_LOGICAL_PROCESSOR_NUMBER(*pti), &cpuset );
    pthread_attr_setaffinity_np( &attr, sizeof(cpuset), &cpuset );

    rv_create = pthread_create( thread_handle, &attr, LIBSHARED_PAL_PTI_GET_THREAD_FUNCTION(*pti), LIBSHARED_PAL_PTI_GET_THREAD_ARGUMENT(*pti) );

    if( rv_create == 0 )
      rv = 1;

    pthread_attr_destroy( &attr );

    return rv;
  }

#endif





/****************************************************************************/
#if( defined __linux__ && defined KERNEL_MODE )

  #ifdef LIBSHARED_PAL_THREAD_START
    #error More than one porting abstraction layer matches the current platform in "libshared_porting_abstraction_layer_thread_start.c".
  #endif

  #define LIBSHARED_PAL_THREAD_START

  int libshared_pal_thread_start( libshared_pal_thread_handle_t *thread_handle,
                                  struct libshared_pal_thread_info *pti )
  {
    LFDS710_PAL_ASSERT( thread_handle != NULL );
    LFDS710_PAL_ASSERT( pti != NULL );

    *thread_handle = kthread_create_on_node( LIBSHARED_PAL_PTI_GET_THREAD_FUNCTION(*pti), LIBSHARED_PAL_PTI_GET_THREAD_ARGUMENT(*pti), (int) LIBSHARED_PAL_PTI_GET_NUMA_NODE_ID(*pti), "lfds" );

    kthread_bind( *thread_handle, LIBSHARED_PAL_PTI_GET_LOGICAL_PROCESSOR_NUMBER(*pti) );

    wake_up_process( *thread_handle );

    return 1;
  }

#endif





/****************************************************************************/
#if( !defined LIBSHARED_PAL_THREAD_START )

  #error No matching porting abstraction layer in "libshared_porting_abstraction_layer_thread_start.c".

#endif

