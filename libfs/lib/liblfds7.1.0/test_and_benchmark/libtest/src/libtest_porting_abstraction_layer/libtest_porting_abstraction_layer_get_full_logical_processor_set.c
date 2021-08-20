/***** includes *****/
#include "libtest_porting_abstraction_layer_internal.h"





/****************************************************************************/
#if( defined _WIN32 && !defined KERNEL_MODE && NTDDI_VERSION >= NTDDI_WIN7 )

  #ifdef LIBTEST_PAL_GET_LOGICAL_CORE_IDS
    #error More than one porting abstraction layer matches current platform in "libtest_porting_abstraction_layer_get_full_logical_processor_set.c".
  #endif

  #define LIBTEST_PAL_GET_LOGICAL_CORE_IDS

  void libtest_pal_get_full_logical_processor_set( struct lfds710_list_asu_state *list_of_logical_processors,
                                                   struct libshared_memory_state *ms )
  {
    BOOL
      rv;

    DWORD
      offset = 0,
      slpie_length = 0;

    lfds710_pal_uint_t
      bitmask,
      logical_processor_number,
      windows_processor_group_number;

    SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX
      *slpie,
      *slpie_buffer = NULL;

    LFDS710_PAL_ASSERT( list_of_logical_processors != NULL );
    LFDS710_PAL_ASSERT( ms != NULL );

    lfds710_list_asu_init_valid_on_current_logical_core( list_of_logical_processors, NULL );

    rv = GetLogicalProcessorInformationEx( RelationGroup, slpie_buffer, &slpie_length );
    slpie_buffer = libshared_memory_alloc_from_most_free_space_node( ms, slpie_length, LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
    rv = GetLogicalProcessorInformationEx( RelationGroup, slpie_buffer, &slpie_length );

    while( offset < slpie_length )
    {
      slpie = (SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *) ( (char unsigned *) slpie_buffer + offset );

      offset += slpie->Size;

      if( slpie->Relationship == RelationGroup )
        for( windows_processor_group_number = 0 ; windows_processor_group_number < slpie->Group.ActiveGroupCount ; windows_processor_group_number++ )
          for( logical_processor_number = 0 ; logical_processor_number < sizeof(KAFFINITY) * BITS_PER_BYTE ; logical_processor_number++ )
          {
            bitmask = (lfds710_pal_uint_t) 1 << logical_processor_number;

            // TRD : if we've found a processor for this group, add it to the list
            if( slpie->Group.GroupInfo[windows_processor_group_number].ActiveProcessorMask & bitmask )
              libtest_misc_pal_helper_add_logical_processor_to_list_of_logical_processors( list_of_logical_processors, ms, logical_processor_number, windows_processor_group_number );
          }
    }

    return;
  }

#endif





/****************************************************************************/
#if( defined _WIN32 && !defined KERNEL_MODE && NTDDI_VERSION >= NTDDI_WINXP && NTDDI_VERSION < NTDDI_WIN7 )

  #ifdef LIBTEST_PAL_GET_LOGICAL_CORE_IDS
    #error More than one porting abstraction layer matches current platform in "libtest_porting_abstraction_layer_get_full_logical_processor_set.c".
  #endif

  #define LIBTEST_PAL_GET_LOGICAL_CORE_IDS

  void libtest_pal_get_full_logical_processor_set( struct lfds710_list_asu_state *list_of_logical_processors,
                                                   struct libshared_memory_state *ms )
  {
    DWORD
      slpi_length = 0;

    lfds710_pal_uint_t
      number_slpi,
      loop;

    SYSTEM_LOGICAL_PROCESSOR_INFORMATION
      *slpi = NULL;

    ULONG_PTR
      mask;

    LFDS710_PAL_ASSERT( list_of_logical_processors != NULL );
    LFDS710_PAL_ASSERT( ms != NULL );

    lfds710_list_asu_init_valid_on_current_logical_core( list_of_logical_processors, NULL, NULL );

    *number_logical_processors = 0;

    GetLogicalProcessorInformation( slpi, &slpi_length );
    slpi = libshared_memory_alloc_from_most_free_space_node( ms, slpi_length, LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
    GetLogicalProcessorInformation( slpi, &slpi_length );
    number_slpi = slpi_length / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);

    for( loop = 0 ; loop < number_slpi ; loop++ )
      if( (slpi+loop)->Relationship == RelationProcessorCore )
        for( logical_processor_number = 0 ; logical_processor_number < sizeof(ULONG_PTR) * BITS_PER_BYTE ; logical_processor_number++ )
        {
          bitmask = 1 << logical_processor_number;

          if( (slpi+loop)->ProcessorMask & bitmask )
            libtest_misc_pal_helper_add_logical_processor_to_list_of_logical_processors( list_of_logical_processors, ms, logical_processor_number, windows_processor_group_number );

    return;
  }

#endif





/****************************************************************************/
#if( defined _WIN32 && defined KERNEL_MODE && NTDDI_VERSION >= NTDDI_WIN7 )

  #ifdef LIBTEST_PAL_GET_LOGICAL_CORE_IDS
    #error More than one porting abstraction layer matches current platform in "libtest_porting_abstraction_layer_get_full_logical_processor_set.c".
  #endif

  #define LIBTEST_PAL_GET_LOGICAL_CORE_IDS

  void libtest_pal_get_full_logical_processor_set( struct lfds710_list_asu_state *list_of_logical_processors,
                                                   struct libshared_memory_state *ms )
  {
    lfds710_pal_uint_t
      bitmask,
      logical_processor_number,
      windows_processor_group_number;

    NTSTATUS
      rv;

    SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX
      *slpie,
      *slpie_buffer = NULL;

    ULONG
      offset = 0,
      slpie_length = 0;

    LFDS710_PAL_ASSERT( list_of_logical_processors != NULL );
    LFDS710_PAL_ASSERT( ms != NULL );

    lfds710_list_asu_init_valid_on_current_logical_core( list_of_logical_processors, NULL );

    rv = KeQueryLogicalProcessorRelationship( NULL, RelationGroup, slpie_buffer, &slpie_length );
    slpie_buffer = libshared_memory_alloc_from_most_free_space_node( ms, slpie_length, LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
    rv = KeQueryLogicalProcessorRelationship( NULL, RelationGroup, slpie_buffer, &slpie_length );

    while( offset < slpie_length )
    {
      slpie = (SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *) ( (char unsigned *) slpie_buffer + offset );

      offset += slpie->Size;

      if( slpie->Relationship == RelationGroup )
        for( windows_processor_group_number = 0 ; windows_processor_group_number < slpie->Group.ActiveGroupCount ; windows_processor_group_number++ )
          for( logical_processor_number = 0 ; logical_processor_number < sizeof(KAFFINITY) * BITS_PER_BYTE ; logical_processor_number++ )
          {
            bitmask = (lfds710_pal_uint_t) 1 << logical_processor_number;

            // TRD : if we've found a processor for this group, add it to the list
            if( slpie->Group.GroupInfo[windows_processor_group_number].ActiveProcessorMask & bitmask )
              libtest_misc_pal_helper_add_logical_processor_to_list_of_logical_processors( list_of_logical_processors, ms, logical_processor_number, windows_processor_group_number );
          }
    }

    return;
  }

#endif





/****************************************************************************/
#if( defined _WIN32 && defined KERNEL_MODE && NTDDI_VERSION >= NTDDI_WINXP && NTDDI_VERSION < NTDDI_WIN7 )

  #ifdef LIBTEST_PAL_GET_LOGICAL_CORE_IDS
    #error More than one porting abstraction layer matches current platform in "libtest_porting_abstraction_layer_get_full_logical_processor_set.c".
  #endif

  #define LIBTEST_PAL_GET_LOGICAL_CORE_IDS

  void libtest_pal_get_full_logical_processor_set( struct lfds710_list_asu_state *list_of_logical_processors,
                                                   struct libshared_memory_state *ms )
  {
    CCHAR
      loop;

    LFDS710_PAL_ASSERT( list_of_logical_processors != NULL );
    LFDS710_PAL_ASSERT( ms != NULL );

    /* TRD : in XP, KeNumberProcessors is a CCHAR indicating the number of processors
             the docs say nothing about whether the actual logical processor numbers are contigious or not...
             ...which is absolutely normal for MS docs on anything to do with CPU topology - bloody useless
             just to make the point about bloody useless, this same variable is only a CCHAR in XP
             prior to XP, it is a pointer to a CCHAR, where that CCHAR holds the same data

             jesus...*facepalm*
    */

    for( loop = 0 ; loop < KeNumberProcessors ; loop++ )
      libtest_misc_pal_helper_add_logical_processor_to_list_of_logical_processors( list_of_logical_processors, ms, loop, 0 );

    return;
  }

#endif





/****************************************************************************/
#if( defined __linux__ && defined __STDC__ and __STDC_HOSTED__ == 1 )

  #ifdef LIBTEST_PAL_GET_LOGICAL_CORE_IDS
    #error More than one porting abstraction layer matches current platform in "libtest_porting_abstraction_layer_get_full_logical_processor_set.c".
  #endif

  #define LIBTEST_PAL_GET_LOGICAL_CORE_IDS

  void libtest_pal_get_full_logical_processor_set( struct lfds710_list_asu_state *list_of_logical_processors,
                                                   struct libshared_memory_state *ms )
  {
    char
      diskbuffer[BUFSIZ],
      string[1024];

    FILE
      *diskfile;

    int long long unsigned
      logical_processor_number;

    LFDS710_PAL_ASSERT( list_of_logical_processors != NULL );
    LFDS710_PAL_ASSERT( ms != NULL );

    lfds710_list_asu_init_valid_on_current_logical_core( list_of_logical_processors, NULL );

    diskfile = fopen( "/proc/cpuinfo", "r" );

    if( diskfile != NULL )
    {
      setbuf( diskfile, diskbuffer );

      while( NULL != fgets(string, 1024, diskfile) )
        if( 1 == sscanf(string, "processor : %llu", &logical_processor_number) )
          libtest_misc_pal_helper_add_logical_processor_to_list_of_logical_processors( list_of_logical_processors, ms, logical_processor_number, 0 );

      fclose( diskfile );
    }

    return;
  }

#endif





/****************************************************************************/
#if( !defined LIBTEST_PAL_GET_LOGICAL_CORE_IDS )

    #error No matching porting abstraction layer in "libtest_porting_abstraction_layer_get_full_logical_processor_set.c".

#endif

