/***** includes *****/
#include "libtest_misc_internal.h"

/***** defines *****/
#define MAX_ARM_ERG_LENGTH_IN_BYTES 2048



/****************************************************************************/
#if( defined LIBTEST_PAL_LOAD_LINKED && defined LIBTEST_PAL_STORE_CONDITIONAL )

  /***** structs *****/
  struct erg_director_state
  {
    enum flag volatile
      quit_flag;

    lfds710_pal_uint_t
      (*count_array)[10],
      number_threads;

    lfds710_pal_uint_t volatile
      **ack_pointer_array,
      (*memory_pointer)[ (MAX_ARM_ERG_LENGTH_IN_BYTES+sizeof(lfds710_pal_uint_t)) / sizeof(lfds710_pal_uint_t)],
      *write_pointer;
  };

  struct erg_helper_state
  {
    enum flag volatile
      *quit_flag;

    lfds710_pal_uint_t volatile
      **ack_pointer,
      **write_pointer;

    lfds710_pal_uint_t
      thread_number;
  };

  static libshared_pal_thread_return_t LIBSHARED_PAL_THREAD_CALLING_CONVENTION thread_erg_director( void *libtest_threadset_per_thread_state );
  static libshared_pal_thread_return_t LIBSHARED_PAL_THREAD_CALLING_CONVENTION thread_erg_helper( void *libtest_threadset_per_thread_state );

  /****************************************************************************/
  void libtest_misc_determine_erg( struct libshared_memory_state *ms, lfds710_pal_uint_t (*count_array)[10], enum libtest_misc_determine_erg_result *der, lfds710_pal_uint_t *erg_length_in_bytes )
  {
    lfds710_pal_uint_t
      erg_size = 10,
      loop = 0,
      number_logical_processors;

    lfds710_pal_uint_t volatile LFDS710_PAL_ALIGN( MAX_ARM_ERG_LENGTH_IN_BYTES )
      memory[ (MAX_ARM_ERG_LENGTH_IN_BYTES+sizeof(lfds710_pal_uint_t)) / sizeof(lfds710_pal_uint_t)];

    struct erg_director_state
      *eds;

    struct erg_helper_state
      *ehs_array;

    struct lfds710_list_asu_element
      *lasue = NULL;

    struct lfds710_list_asu_state
      list_of_logical_processors;

    struct libtest_logical_processor
      *lp;

    struct libtest_threadset_per_thread_state
      *pts;

    struct libtest_threadset_state
      ts;

    LFDS710_PAL_ASSERT( ms != NULL );
    LFDS710_PAL_ASSERT( count_array != NULL );
    LFDS710_PAL_ASSERT( der != NULL );
    LFDS710_PAL_ASSERT( erg_length_in_bytes != NULL );

    /* TRD : ARM chips have a local and a global monitor
             the local monitor has few guarantees and so you can't figure out ERG from it
             because you can write even directly TO the LL/SC target and it thinks things are fine
             so we have to hit the global monitor, which means running threads
             in test, we know nothing about topology, so we just have to run one thread on each logical core
             and get them to perform our necessary test memory writes

             the code itself works by having a buffer of 2048 bytes, aligned at 2048 bytes - so it will
             be aligned with an ERG
             we LL/SC always on the first word
             between the LL and SC, we get the other threads to perform a memory write into the buffer
             the location of the write is the last word in each progressively larger ERG size, i.e.
             ERG can be 8, 16, 32, 64, etc, so we LL location 0, then write to 1, 3, 7, 15, etc
             we can then see which ERG sizes work and which fail
    */

    for( loop = 0 ; loop < 10 ; loop++ )
      (*count_array)[loop] = 0;

    libtest_pal_get_full_logical_processor_set( &list_of_logical_processors, ms );

    lfds710_list_asu_query( &list_of_logical_processors, LFDS710_LIST_ASU_QUERY_GET_POTENTIALLY_INACCURATE_COUNT, NULL, (void **) &number_logical_processors );

    if( number_logical_processors == 1 )
    {
      *der = LIBTEST_MISC_DETERMINE_ERG_RESULT_ONE_PHYSICAL_CORE;
      return;
    }

    pts = libshared_memory_alloc_from_unknown_node( ms, sizeof(struct libtest_threadset_per_thread_state) * number_logical_processors, LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );

    ehs_array = libshared_memory_alloc_from_unknown_node( ms, sizeof(struct erg_helper_state) * (number_logical_processors-1), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
    eds = libshared_memory_alloc_from_unknown_node( ms, sizeof(struct erg_director_state), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );
    eds->ack_pointer_array = libshared_memory_alloc_from_unknown_node( ms, sizeof(lfds710_pal_uint_t *) * (number_logical_processors-1), LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES );

    eds->quit_flag = LOWERED;
    eds->count_array = count_array;
    eds->number_threads = number_logical_processors - 1;
    eds->write_pointer = NULL;
    eds->memory_pointer = &memory;
    for( loop = 0 ; loop < (number_logical_processors-1) ; loop++ )
      eds->ack_pointer_array[loop] = NULL;

    libtest_threadset_init( &ts, NULL );

    loop = 0;

    while( LFDS710_LIST_ASU_GET_START_AND_THEN_NEXT(list_of_logical_processors, lasue) )
    {
      lp = LFDS710_LIST_ASU_GET_VALUE_FROM_ELEMENT( *lasue );

      if( loop == number_logical_processors-1 )
        libtest_threadset_add_thread( &ts, &pts[loop], lp, thread_erg_director, (void *) eds );
      else
      {
        ehs_array[loop].quit_flag = &eds->quit_flag;
        ehs_array[loop].thread_number = loop;
        ehs_array[loop].ack_pointer = &eds->ack_pointer_array[loop];
        ehs_array[loop].write_pointer = &eds->write_pointer;
        libtest_threadset_add_thread( &ts, &pts[loop], lp, thread_erg_helper, (void *) &ehs_array[loop] );
      }

      loop++;
    }

    LFDS710_MISC_BARRIER_STORE;
    lfds710_misc_force_store();

    libtest_threadset_run( &ts );

    libtest_threadset_cleanup( &ts );

    for( loop = 0 ; loop < 10 ; loop++ )
      if( count_array[loop] > 0 )
        erg_size = loop;

    if( erg_size == 0 )
      *der = LIBTEST_MISC_DETERMINE_ERG_RESULT_ONE_PHYSICAL_CORE_OR_NO_LLSC;

    if( erg_size >= 1 and erg_size <= 9 )
    {
      *der = LIBTEST_MISC_DETERMINE_ERG_RESULT_SUCCESS;
      *erg_length_in_bytes = 1UL < (erg_size+2);
    }

    if( erg_size == 10 )
      *der = LIBTEST_MISC_DETERMINE_ERG_RESULT_NO_LLSC;

    return;
  }

  /****************************************************************************/
  static libshared_pal_thread_return_t LIBSHARED_PAL_THREAD_CALLING_CONVENTION thread_erg_director( void *libtest_threadset_per_thread_state )
  {
    lfds710_pal_uint_t
      ack_count,
      count_index,
      erg_length_in_bytes,
      loop,
      register_memory_zero,
      stored_flag,
      subloop;

    struct erg_director_state
      *eds;

    struct libtest_threadset_per_thread_state
      *pts;

    LFDS710_MISC_MAKE_VALID_ON_CURRENT_LOGICAL_CORE_INITS_COMPLETED_BEFORE_NOW_ON_ANY_OTHER_LOGICAL_CORE;

    LFDS710_PAL_ASSERT( libtest_threadset_per_thread_state != NULL );

    pts = (struct libtest_threadset_per_thread_state *) libtest_threadset_per_thread_state;

    eds = LIBTEST_THREADSET_GET_USER_STATE_FROM_PER_THREAD_STATE( *pts );

    libtest_threadset_thread_ready_and_wait( pts );

    for( loop = 0 ; loop < 1024 ; loop++ )
      for( erg_length_in_bytes = MAX_ARM_ERG_LENGTH_IN_BYTES, count_index = 9 ; erg_length_in_bytes >= 4 ; erg_length_in_bytes /= 2, count_index-- )
      {
        LIBTEST_PAL_LOAD_LINKED( register_memory_zero, &(*eds->memory_pointer)[0] );

        eds->write_pointer = &(*eds->memory_pointer)[erg_length_in_bytes / sizeof(lfds710_pal_uint_t)];

        // TRD : wait for all threads to change their ack_pointer to the new write_pointer
        do
        {
          ack_count = 0;

          for( subloop = 0 ; subloop < eds->number_threads ; subloop++ )
            if( eds->ack_pointer_array[subloop] == eds->write_pointer )
            {
              LFDS710_MISC_BARRIER_LOAD; // TRD : yes, really here!
              ack_count++;
            }
        }
        while( ack_count != eds->number_threads );

        LIBTEST_PAL_STORE_CONDITIONAL( &(*eds->memory_pointer)[0], register_memory_zero, stored_flag );

        if( stored_flag == 0 )
          (*eds->count_array)[count_index]++;
      }

    eds->quit_flag = RAISED;

    LFDS710_MISC_BARRIER_STORE;
    lfds710_misc_force_store();

    return (libshared_pal_thread_return_t) RETURN_SUCCESS;
  }

  /****************************************************************************/
  static libshared_pal_thread_return_t LIBSHARED_PAL_THREAD_CALLING_CONVENTION thread_erg_helper( void *libtest_threadset_per_thread_state )
  {
    struct erg_helper_state
      *ehs;

    struct libtest_threadset_per_thread_state
      *pts;

    LFDS710_MISC_MAKE_VALID_ON_CURRENT_LOGICAL_CORE_INITS_COMPLETED_BEFORE_NOW_ON_ANY_OTHER_LOGICAL_CORE;

    LFDS710_PAL_ASSERT( libtest_threadset_per_thread_state != NULL );

    pts = (struct libtest_threadset_per_thread_state *) libtest_threadset_per_thread_state;

    ehs = LIBTEST_THREADSET_GET_USER_STATE_FROM_PER_THREAD_STATE( *pts );

    libtest_threadset_thread_ready_and_wait( pts );

    while( *ehs->quit_flag == LOWERED )
    {
      if( *ehs->write_pointer != NULL )
        **ehs->write_pointer = ehs->thread_number; // TRD : can be any value though - thread_number just seems nice
      LFDS710_MISC_BARRIER_STORE;
      *ehs->ack_pointer = *ehs->write_pointer;
    }

    LFDS710_MISC_BARRIER_STORE;
    lfds710_misc_force_store();

    return (libshared_pal_thread_return_t) RETURN_SUCCESS;
  }

#endif





/****************************************************************************/
#pragma warning( disable : 4100 )

#if( !defined LIBTEST_PAL_LOAD_LINKED || !defined LIBTEST_PAL_STORE_CONDITIONAL )

  void libtest_misc_determine_erg( struct libshared_memory_state *ms, lfds710_pal_uint_t (*count_array)[10], enum libtest_misc_determine_erg_result *der, lfds710_pal_uint_t *erg_length_in_bytes )
  {
    lfds710_pal_uint_t
      loop;

    LFDS710_PAL_ASSERT( ms != NULL );
    LFDS710_PAL_ASSERT( count_array != NULL );
    LFDS710_PAL_ASSERT( der != NULL );
    LFDS710_PAL_ASSERT( erg_length_in_bytes != NULL );

    for( loop = 0 ; loop < 10 ; loop++ )
      (*count_array)[loop] = 0;

    *der = LIBTEST_MISC_DETERMINE_ERG_RESULT_NOT_SUPPORTED;

    return;
  }

#endif

#pragma warning( default : 4100 )

