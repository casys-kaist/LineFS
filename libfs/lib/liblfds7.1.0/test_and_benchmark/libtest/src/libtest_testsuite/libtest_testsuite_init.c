/***** includes *****/
#include "libtest_testsuite_internal.h"





/****************************************************************************/
#pragma warning( disable : 4127 )

void libtest_testsuite_init( struct libtest_testsuite_state *ts,
                             struct libshared_memory_state *ms,
                             void (*callback_test_start)(char *test_name),
                             void (*callback_test_finish)(char *result) )
{
  enum libtest_test_id
    test_id;

  LFDS710_PAL_ASSERT( ts != NULL );
  LFDS710_PAL_ASSERT( ms != NULL );
  // TRD : callback_test_start can be NULL
  // TRD : callback_test_finish can be NULL

  // TRD : configure the testsuite state with all the test supported by this platform

  libtest_pal_get_full_logical_processor_set( &ts->list_of_logical_processors, ms );
  ts->ms = ms;
  ts->callback_test_start = callback_test_start;
  ts->callback_test_finish = callback_test_finish;

  for( test_id = 0 ; test_id < LIBTEST_TEST_ID_COUNT ; test_id++ )
    ts->test_available_flag[test_id] = LOWERED;

  if( LFDS710_MISC_ATOMIC_SUPPORT_COMPILER_BARRIERS and LFDS710_MISC_ATOMIC_SUPPORT_ADD )
  {
    libtest_test_init( &ts->tests[LIBTEST_TEST_ID_PRNG_ALIGNMENT], "PRNG alignment", LIBTEST_TEST_ID_PRNG_ALIGNMENT, libtest_tests_prng_alignment );
    libtest_test_init( &ts->tests[LIBTEST_TEST_ID_PRNG_GENERATE], "PRNG generation", LIBTEST_TEST_ID_PRNG_GENERATE, libtest_tests_prng_generate );
    ts->test_available_flag[LIBTEST_TEST_ID_PRNG_ALIGNMENT] = RAISED;
    ts->test_available_flag[LIBTEST_TEST_ID_PRNG_GENERATE] = RAISED;
  }

  if( LFDS710_MISC_ATOMIC_SUPPORT_COMPILER_BARRIERS and LFDS710_MISC_ATOMIC_SUPPORT_PROCESSOR_BARRIERS )
  {
    libtest_test_init( &ts->tests[LIBTEST_TEST_ID_QUEUE_BSS_DEQUEUING], "Queue (bounded, single producer, single consumer) dequeuing", LIBTEST_TEST_ID_QUEUE_BSS_DEQUEUING, libtest_tests_queue_bss_dequeuing );
    libtest_test_init( &ts->tests[LIBTEST_TEST_ID_QUEUE_BSS_ENQUEUING], "Queue (bounded, single producer, single consumer) enqueuing", LIBTEST_TEST_ID_QUEUE_BSS_ENQUEUING, libtest_tests_queue_bss_enqueuing );
    libtest_test_init( &ts->tests[LIBTEST_TEST_ID_QUEUE_BSS_ENQUEUING_AND_DEQUEUING], "Queue (bounded, single producer, single consumer) enqueuing and dequeuing", LIBTEST_TEST_ID_QUEUE_BSS_ENQUEUING_AND_DEQUEUING, libtest_tests_queue_bss_enqueuing_and_dequeuing );
    ts->test_available_flag[LIBTEST_TEST_ID_QUEUE_BSS_DEQUEUING] = RAISED;
    ts->test_available_flag[LIBTEST_TEST_ID_QUEUE_BSS_ENQUEUING] = RAISED;
    ts->test_available_flag[LIBTEST_TEST_ID_QUEUE_BSS_ENQUEUING_AND_DEQUEUING] = RAISED;
  }

  if( LFDS710_MISC_ATOMIC_SUPPORT_COMPILER_BARRIERS and LFDS710_MISC_ATOMIC_SUPPORT_PROCESSOR_BARRIERS and LFDS710_MISC_ATOMIC_SUPPORT_CAS )
  {
    libtest_test_init( &ts->tests[LIBTEST_TEST_ID_BTREE_ADDONLY_UNBALANCED_ALIGNMENT], "BTree (addonly, unbalanced) alignment", LIBTEST_TEST_ID_BTREE_ADDONLY_UNBALANCED_ALIGNMENT, libtest_tests_btree_au_alignment );
    libtest_test_init( &ts->tests[LIBTEST_TEST_ID_BTREE_ADDONLY_UNBALANCED_RANDOM_ADDS_FAIL], "BTree (addonly, unbalanced) adds and walking (fail on existing key)", LIBTEST_TEST_ID_BTREE_ADDONLY_UNBALANCED_RANDOM_ADDS_FAIL, libtest_tests_btree_au_random_adds_fail_on_existing );
    libtest_test_init( &ts->tests[LIBTEST_TEST_ID_BTREE_ADDONLY_UNBALANCED_RANDOM_ADDS_FAIL_AND_OVERWRITE], "BTree (addonly, unbalanced) adds and walking (ovewrite on existing key)", LIBTEST_TEST_ID_BTREE_ADDONLY_UNBALANCED_RANDOM_ADDS_FAIL_AND_OVERWRITE, libtest_tests_btree_au_random_adds_overwrite_on_existing );
    libtest_test_init( &ts->tests[LIBTEST_TEST_ID_BTREE_ADDONLY_UNBALANCED_RANDOM_ADDS_OVERWRITE], "BTree (addonly, unbalanced) fail and overwrite on existing key", LIBTEST_TEST_ID_BTREE_ADDONLY_UNBALANCED_RANDOM_ADDS_OVERWRITE, libtest_tests_btree_au_fail_and_overwrite_on_existing_key );
    ts->test_available_flag[LIBTEST_TEST_ID_BTREE_ADDONLY_UNBALANCED_ALIGNMENT] = RAISED;
    ts->test_available_flag[LIBTEST_TEST_ID_BTREE_ADDONLY_UNBALANCED_RANDOM_ADDS_FAIL] = RAISED;
    ts->test_available_flag[LIBTEST_TEST_ID_BTREE_ADDONLY_UNBALANCED_RANDOM_ADDS_FAIL_AND_OVERWRITE] = RAISED;
    ts->test_available_flag[LIBTEST_TEST_ID_BTREE_ADDONLY_UNBALANCED_RANDOM_ADDS_OVERWRITE] = RAISED;

    libtest_test_init( &ts->tests[LIBTEST_TEST_ID_HASH_ADDONLY_ALIGNMENT], "Hash (addonly) alignment", LIBTEST_TEST_ID_HASH_ADDONLY_ALIGNMENT, libtest_tests_hash_a_alignment );
    libtest_test_init( &ts->tests[LIBTEST_TEST_ID_HASH_ADDONLY_FAIL_AND_OVERWRITE], "Hash (addonly) fail and overwrite", LIBTEST_TEST_ID_HASH_ADDONLY_FAIL_AND_OVERWRITE, libtest_tests_hash_a_fail_and_overwrite_on_existing_key );
    libtest_test_init( &ts->tests[LIBTEST_TEST_ID_HASH_ADDONLY_RANDOM_ADDS_FAIL], "Hash (addonly) random adds (fail on existing key)", LIBTEST_TEST_ID_HASH_ADDONLY_RANDOM_ADDS_FAIL, libtest_tests_hash_a_random_adds_fail_on_existing );
    libtest_test_init( &ts->tests[LIBTEST_TEST_ID_HASH_ADDONLY_RANDOM_ADDS_OVERWRITE], "Hash (addonly) random adds (overwrite on existing key)", LIBTEST_TEST_ID_HASH_ADDONLY_RANDOM_ADDS_OVERWRITE, libtest_tests_hash_a_random_adds_overwrite_on_existing );
    libtest_test_init( &ts->tests[LIBTEST_TEST_ID_HASH_ADDONLY_ITERATE], "Hash (addonly) iterate", LIBTEST_TEST_ID_HASH_ADDONLY_ITERATE, libtest_tests_hash_a_iterate );
    ts->test_available_flag[LIBTEST_TEST_ID_HASH_ADDONLY_ALIGNMENT] = RAISED;
    ts->test_available_flag[LIBTEST_TEST_ID_HASH_ADDONLY_FAIL_AND_OVERWRITE] = RAISED;
    ts->test_available_flag[LIBTEST_TEST_ID_HASH_ADDONLY_RANDOM_ADDS_FAIL] = RAISED;
    ts->test_available_flag[LIBTEST_TEST_ID_HASH_ADDONLY_RANDOM_ADDS_OVERWRITE] = RAISED;
    ts->test_available_flag[LIBTEST_TEST_ID_HASH_ADDONLY_ITERATE] = RAISED;

    libtest_test_init( &ts->tests[LIBTEST_TEST_ID_LIST_ADDONLY_SINGLYLINKED_ORDERED_ALIGNMENT], "List (addonly, ordered, singlylinked) alignment", LIBTEST_TEST_ID_LIST_ADDONLY_SINGLYLINKED_ORDERED_ALIGNMENT, libtest_tests_list_aso_alignment );
    libtest_test_init( &ts->tests[LIBTEST_TEST_ID_LIST_ADDONLY_SINGLYLINKED_ORDERED_NEW_ORDERED], "List (addonly, ordered, singlylinked) new ordered", LIBTEST_TEST_ID_LIST_ADDONLY_SINGLYLINKED_ORDERED_NEW_ORDERED, libtest_tests_list_aso_new_ordered );
    libtest_test_init( &ts->tests[LIBTEST_TEST_ID_LIST_ADDONLY_SINGLYLINKED_ORDERED_NEW_ORDERED_WITH_CURSOR], "List (addonly, ordered, singlylinked) new ordered with cursor", LIBTEST_TEST_ID_LIST_ADDONLY_SINGLYLINKED_ORDERED_NEW_ORDERED_WITH_CURSOR, libtest_tests_list_aso_new_ordered_with_cursor );
    ts->test_available_flag[LIBTEST_TEST_ID_LIST_ADDONLY_SINGLYLINKED_ORDERED_ALIGNMENT] = RAISED;
    ts->test_available_flag[LIBTEST_TEST_ID_LIST_ADDONLY_SINGLYLINKED_ORDERED_NEW_ORDERED] = RAISED;
    ts->test_available_flag[LIBTEST_TEST_ID_LIST_ADDONLY_SINGLYLINKED_ORDERED_NEW_ORDERED_WITH_CURSOR] = RAISED;

    libtest_test_init( &ts->tests[LIBTEST_TEST_ID_LIST_ADDONLY_SINGLYLINKED_UNORDERED_ALIGNMENT], "List (addonly, singlylinked, unordered) alignment", LIBTEST_TEST_ID_LIST_ADDONLY_SINGLYLINKED_ORDERED_ALIGNMENT, libtest_tests_list_asu_alignment );
    libtest_test_init( &ts->tests[LIBTEST_TEST_ID_LIST_ADDONLY_SINGLYLINKED_UNORDERED_NEW_START], "List (addonly, singlylinked, unordered) new start", LIBTEST_TEST_ID_LIST_ADDONLY_SINGLYLINKED_UNORDERED_NEW_START, libtest_tests_list_asu_new_start );
    libtest_test_init( &ts->tests[LIBTEST_TEST_ID_LIST_ADDONLY_SINGLYLINKED_UNORDERED_NEW_END], "List (addonly, singlylinked, unordered) new end", LIBTEST_TEST_ID_LIST_ADDONLY_SINGLYLINKED_UNORDERED_NEW_END, libtest_tests_list_asu_new_end );
    libtest_test_init( &ts->tests[LIBTEST_TEST_ID_LIST_ADDONLY_SINGLYLINKED_UNORDERED_NEW_AFTER], "List (addonly, singlylinked, unordered) new after", LIBTEST_TEST_ID_LIST_ADDONLY_SINGLYLINKED_UNORDERED_NEW_AFTER, libtest_tests_list_asu_new_after );
    ts->test_available_flag[LIBTEST_TEST_ID_LIST_ADDONLY_SINGLYLINKED_UNORDERED_ALIGNMENT] = RAISED;
    ts->test_available_flag[LIBTEST_TEST_ID_LIST_ADDONLY_SINGLYLINKED_UNORDERED_NEW_START] = RAISED;
    ts->test_available_flag[LIBTEST_TEST_ID_LIST_ADDONLY_SINGLYLINKED_UNORDERED_NEW_END] = RAISED;
    ts->test_available_flag[LIBTEST_TEST_ID_LIST_ADDONLY_SINGLYLINKED_UNORDERED_NEW_AFTER] = RAISED;

    libtest_test_init( &ts->tests[LIBTEST_TEST_ID_QUEUE_BMM_ALIGNMENT], "Queue (bounded, many consumer, many producer) alignment", LIBTEST_TEST_ID_QUEUE_BMM_ALIGNMENT, libtest_tests_queue_bmm_alignment );
    libtest_test_init( &ts->tests[LIBTEST_TEST_ID_QUEUE_BMM_COUNT], "Queue (bounded, many consumer, many producer) count", LIBTEST_TEST_ID_QUEUE_BMM_COUNT, libtest_tests_queue_bmm_count );
    libtest_test_init( &ts->tests[LIBTEST_TEST_ID_QUEUE_BMM_ENQUEUING], "Queue (bounded, many consumer, many producer) enqueuing", LIBTEST_TEST_ID_QUEUE_BMM_ENQUEUING, libtest_tests_queue_bmm_enqueuing );
    libtest_test_init( &ts->tests[LIBTEST_TEST_ID_QUEUE_BMM_DEQUEUING], "Queue (bounded, many consumer, many producer) dequeuing", LIBTEST_TEST_ID_QUEUE_BMM_DEQUEUING, libtest_tests_queue_bmm_dequeuing );
    libtest_test_init( &ts->tests[LIBTEST_TEST_ID_QUEUE_BMM_ENQUEUING_AND_DEQUEUING], "Queue (bounded, many consumer, many producer) enqueuing and dequeuing", LIBTEST_TEST_ID_QUEUE_BMM_ENQUEUING_AND_DEQUEUING, libtest_tests_queue_bmm_enqueuing_and_dequeuing );
    libtest_test_init( &ts->tests[LIBTEST_TEST_ID_QUEUE_BMM_RAPID_ENQUEUING_AND_DEQUEUING], "Queue (bounded, many consumer, many producer) rapid enqueuing and dequeuing", LIBTEST_TEST_ID_QUEUE_BMM_RAPID_ENQUEUING_AND_DEQUEUING, libtest_tests_queue_bmm_rapid_enqueuing_and_dequeuing );
    ts->test_available_flag[LIBTEST_TEST_ID_QUEUE_BMM_ALIGNMENT] = RAISED;
    ts->test_available_flag[LIBTEST_TEST_ID_QUEUE_BMM_COUNT] = RAISED;
    ts->test_available_flag[LIBTEST_TEST_ID_QUEUE_BMM_ENQUEUING] = RAISED;
    ts->test_available_flag[LIBTEST_TEST_ID_QUEUE_BMM_DEQUEUING] = RAISED;
    ts->test_available_flag[LIBTEST_TEST_ID_QUEUE_BMM_ENQUEUING_AND_DEQUEUING] = RAISED;
    ts->test_available_flag[LIBTEST_TEST_ID_QUEUE_BMM_RAPID_ENQUEUING_AND_DEQUEUING] = RAISED;
  }

  if( LFDS710_MISC_ATOMIC_SUPPORT_COMPILER_BARRIERS and LFDS710_MISC_ATOMIC_SUPPORT_PROCESSOR_BARRIERS and LFDS710_MISC_ATOMIC_SUPPORT_DWCAS )
  {
    libtest_test_init( &ts->tests[LIBTEST_TEST_ID_FREELIST_ALIGNMENT], "Freelist alignment", LIBTEST_TEST_ID_FREELIST_ALIGNMENT, libtest_tests_freelist_alignment );
    ts->test_available_flag[LIBTEST_TEST_ID_FREELIST_ALIGNMENT] = RAISED;

    libtest_test_init( &ts->tests[LIBTEST_TEST_ID_FREELIST_EA_POPPING], "Freelist (with EA) popping", LIBTEST_TEST_ID_FREELIST_EA_POPPING, libtest_tests_freelist_ea_popping );
    libtest_test_init( &ts->tests[LIBTEST_TEST_ID_FREELIST_EA_POPPING_AND_PUSHING], "Freelist (with EA) popping and pushing", LIBTEST_TEST_ID_FREELIST_EA_POPPING_AND_PUSHING, libtest_tests_freelist_ea_popping_and_pushing );
    libtest_test_init( &ts->tests[LIBTEST_TEST_ID_FREELIST_EA_PUSHING], "Freelist (with EA) pushing", LIBTEST_TEST_ID_FREELIST_EA_PUSHING, libtest_tests_freelist_ea_pushing );
    libtest_test_init( &ts->tests[LIBTEST_TEST_ID_FREELIST_EA_RAPID_POPPING_AND_PUSHING], "Freelist (with EA) rapid popping and pushing", LIBTEST_TEST_ID_FREELIST_EA_RAPID_POPPING_AND_PUSHING, libtest_tests_freelist_ea_rapid_popping_and_pushing );
    ts->test_available_flag[LIBTEST_TEST_ID_FREELIST_EA_POPPING] = RAISED;
    ts->test_available_flag[LIBTEST_TEST_ID_FREELIST_EA_POPPING_AND_PUSHING] = RAISED;
    ts->test_available_flag[LIBTEST_TEST_ID_FREELIST_EA_PUSHING] = RAISED;
    ts->test_available_flag[LIBTEST_TEST_ID_FREELIST_EA_RAPID_POPPING_AND_PUSHING] = RAISED;
    libtest_test_init( &ts->tests[LIBTEST_TEST_ID_FREELIST_WITHOUT_EA_POPPING], "Freelist (without EA) popping", LIBTEST_TEST_ID_FREELIST_WITHOUT_EA_POPPING, libtest_tests_freelist_without_ea_popping );
    libtest_test_init( &ts->tests[LIBTEST_TEST_ID_FREELIST_WITHOUT_EA_POPPING_AND_PUSHING], "Freelist (without EA) popping and pushing", LIBTEST_TEST_ID_FREELIST_WITHOUT_EA_POPPING_AND_PUSHING, libtest_tests_freelist_without_ea_popping_and_pushing );
    libtest_test_init( &ts->tests[LIBTEST_TEST_ID_FREELIST_WITHOUT_EA_PUSHING], "Freelist (without EA) pushing", LIBTEST_TEST_ID_FREELIST_WITHOUT_EA_PUSHING, libtest_tests_freelist_without_ea_pushing );
    libtest_test_init( &ts->tests[LIBTEST_TEST_ID_FREELIST_WITHOUT_EA_RAPID_POPPING_AND_PUSHING], "Freelist (without EA) rapid popping and pushing", LIBTEST_TEST_ID_FREELIST_WITHOUT_EA_RAPID_POPPING_AND_PUSHING, libtest_tests_freelist_without_ea_rapid_popping_and_pushing );
    ts->test_available_flag[LIBTEST_TEST_ID_FREELIST_WITHOUT_EA_POPPING] = RAISED;
    ts->test_available_flag[LIBTEST_TEST_ID_FREELIST_WITHOUT_EA_POPPING_AND_PUSHING] = RAISED;
    ts->test_available_flag[LIBTEST_TEST_ID_FREELIST_WITHOUT_EA_PUSHING] = RAISED;
    ts->test_available_flag[LIBTEST_TEST_ID_FREELIST_WITHOUT_EA_RAPID_POPPING_AND_PUSHING] = RAISED;

    libtest_test_init( &ts->tests[LIBTEST_TEST_ID_RINGBUFFER_READING], "Ringbuffer reading", LIBTEST_TEST_ID_RINGBUFFER_READING, libtest_tests_ringbuffer_reading );
    libtest_test_init( &ts->tests[LIBTEST_TEST_ID_RINGBUFFER_WRITING], "Ringbuffer writing", LIBTEST_TEST_ID_RINGBUFFER_WRITING, libtest_tests_ringbuffer_writing );
    libtest_test_init( &ts->tests[LIBTEST_TEST_ID_RINGBUFFER_READING_AND_WRITING], "Ringbuffer reading and writing", LIBTEST_TEST_ID_RINGBUFFER_READING_AND_WRITING, libtest_tests_ringbuffer_reading_and_writing );
    ts->test_available_flag[LIBTEST_TEST_ID_RINGBUFFER_READING] = RAISED;
    ts->test_available_flag[LIBTEST_TEST_ID_RINGBUFFER_WRITING] = RAISED;
    ts->test_available_flag[LIBTEST_TEST_ID_RINGBUFFER_READING_AND_WRITING] = RAISED;

    libtest_test_init( &ts->tests[LIBTEST_TEST_ID_STACK_ALIGNMENT], "Stack alignment", LIBTEST_TEST_ID_STACK_ALIGNMENT, libtest_tests_stack_alignment );
    libtest_test_init( &ts->tests[LIBTEST_TEST_ID_STACK_POPPING], "Stack popping", LIBTEST_TEST_ID_STACK_POPPING, libtest_tests_stack_popping );
    libtest_test_init( &ts->tests[LIBTEST_TEST_ID_STACK_POPPING_AND_PUSHING], "Stack popping and pushing", LIBTEST_TEST_ID_STACK_POPPING_AND_PUSHING, libtest_tests_stack_popping_and_pushing );
    libtest_test_init( &ts->tests[LIBTEST_TEST_ID_STACK_PUSHING], "Stack pushing", LIBTEST_TEST_ID_STACK_PUSHING, libtest_tests_stack_pushing );
    libtest_test_init( &ts->tests[LIBTEST_TEST_ID_STACK_RAPID_POPPING_AND_PUSHING], "Stack rapid popping and pushing", LIBTEST_TEST_ID_STACK_RAPID_POPPING_AND_PUSHING, libtest_tests_stack_rapid_popping_and_pushing );
    ts->test_available_flag[LIBTEST_TEST_ID_STACK_ALIGNMENT] = RAISED;
    ts->test_available_flag[LIBTEST_TEST_ID_STACK_POPPING] = RAISED;
    ts->test_available_flag[LIBTEST_TEST_ID_STACK_POPPING_AND_PUSHING] = RAISED;
    ts->test_available_flag[LIBTEST_TEST_ID_STACK_PUSHING] = RAISED;
    ts->test_available_flag[LIBTEST_TEST_ID_STACK_RAPID_POPPING_AND_PUSHING] = RAISED;

    libtest_test_init( &ts->tests[LIBTEST_TEST_ID_QUEUE_UMM_ALIGNMENT], "Queue (unbounded, many producer, many consumer) alignment", LIBTEST_TEST_ID_QUEUE_UMM_ALIGNMENT, libtest_tests_queue_umm_alignment );
    libtest_test_init( &ts->tests[LIBTEST_TEST_ID_QUEUE_UMM_ENQUEUING], "Queue (unbounded, many producer, many consumer) enqueuing", LIBTEST_TEST_ID_QUEUE_UMM_ENQUEUING, libtest_tests_queue_umm_enqueuing );
    libtest_test_init( &ts->tests[LIBTEST_TEST_ID_QUEUE_UMM_DEQUEUING], "Queue (unbounded, many producer, many consumer) dequeuing", LIBTEST_TEST_ID_QUEUE_UMM_DEQUEUING, libtest_tests_queue_umm_dequeuing );
    libtest_test_init( &ts->tests[LIBTEST_TEST_ID_QUEUE_UMM_ENQUEUING_AND_DEQUEUING], "Queue (unbounded, many producer, many consumer) enqueuing and dequeuing", LIBTEST_TEST_ID_QUEUE_UMM_ENQUEUING_AND_DEQUEUING, libtest_tests_queue_umm_enqueuing_and_dequeuing );
    #if( defined LIBTEST_PAL_MALLOC && defined LIBTEST_PAL_FREE )
      libtest_test_init( &ts->tests[LIBTEST_TEST_ID_QUEUE_UMM_ENQUEUING_WITH_MALLOC_AND_DEQUEUING_WITH_FREE], "Queue (unbounded, many producer, many consumer) enqueuing with malloc and dequeuing with free", LIBTEST_TEST_ID_QUEUE_UMM_ENQUEUING_WITH_MALLOC_AND_DEQUEUING_WITH_FREE, libtest_tests_queue_umm_enqueuing_with_malloc_and_dequeuing_with_free );
      ts->test_available_flag[LIBTEST_TEST_ID_QUEUE_UMM_ENQUEUING_WITH_MALLOC_AND_DEQUEUING_WITH_FREE] = RAISED;
    #endif
    libtest_test_init( &ts->tests[LIBTEST_TEST_ID_QUEUE_UMM_RAPID_ENQUEUING_AND_DEQUEUING], "Queue (unbounded, many producer, many consumer) rapid enqueuing and dequeuing", LIBTEST_TEST_ID_QUEUE_UMM_RAPID_ENQUEUING_AND_DEQUEUING, libtest_tests_queue_umm_rapid_enqueuing_and_dequeuing );
    ts->test_available_flag[LIBTEST_TEST_ID_QUEUE_UMM_ALIGNMENT] = RAISED;
    ts->test_available_flag[LIBTEST_TEST_ID_QUEUE_UMM_ENQUEUING] = RAISED;
    ts->test_available_flag[LIBTEST_TEST_ID_QUEUE_UMM_DEQUEUING] = RAISED;
    ts->test_available_flag[LIBTEST_TEST_ID_QUEUE_UMM_ENQUEUING_AND_DEQUEUING] = RAISED;
    ts->test_available_flag[LIBTEST_TEST_ID_QUEUE_UMM_RAPID_ENQUEUING_AND_DEQUEUING] = RAISED;
  }

  if( LFDS710_MISC_ATOMIC_SUPPORT_COMPILER_BARRIERS and LFDS710_MISC_ATOMIC_SUPPORT_ADD )
  {
    libtest_test_init( &ts->tests[LIBTEST_TEST_ID_PORTING_ABSTRACTION_LAYER_ADD], "Atomic add", LIBTEST_TEST_ID_PORTING_ABSTRACTION_LAYER_ADD, libtest_tests_pal_atomic_add );
    ts->test_available_flag[LIBTEST_TEST_ID_PORTING_ABSTRACTION_LAYER_ADD] = RAISED;
  }

  if( LFDS710_MISC_ATOMIC_SUPPORT_COMPILER_BARRIERS and LFDS710_MISC_ATOMIC_SUPPORT_CAS )
  {
    libtest_test_init( &ts->tests[LIBTEST_TEST_ID_PORTING_ABSTRACTION_LAYER_CAS], "Atomic CAS", LIBTEST_TEST_ID_PORTING_ABSTRACTION_LAYER_CAS, libtest_tests_pal_atomic_cas );
    ts->test_available_flag[LIBTEST_TEST_ID_PORTING_ABSTRACTION_LAYER_CAS] = RAISED;
  }

  if( LFDS710_MISC_ATOMIC_SUPPORT_COMPILER_BARRIERS and LFDS710_MISC_ATOMIC_SUPPORT_DWCAS )
  {
    libtest_test_init( &ts->tests[LIBTEST_TEST_ID_PORTING_ABSTRACTION_LAYER_DCAS], "Atomic DWCAS", LIBTEST_TEST_ID_PORTING_ABSTRACTION_LAYER_DCAS, libtest_tests_pal_atomic_dwcas );
    ts->test_available_flag[LIBTEST_TEST_ID_PORTING_ABSTRACTION_LAYER_DCAS] = RAISED;
  }

  if( LFDS710_MISC_ATOMIC_SUPPORT_COMPILER_BARRIERS and LFDS710_MISC_ATOMIC_SUPPORT_EXCHANGE )
  {
    libtest_test_init( &ts->tests[LIBTEST_TEST_ID_PORTING_ABSTRACTION_LAYER_EXCHANGE], "Atomic exchange", LIBTEST_TEST_ID_PORTING_ABSTRACTION_LAYER_EXCHANGE, libtest_tests_pal_atomic_exchange );
    ts->test_available_flag[LIBTEST_TEST_ID_PORTING_ABSTRACTION_LAYER_EXCHANGE] = RAISED;
  }

  return;
}

#pragma warning( default : 4127 )

