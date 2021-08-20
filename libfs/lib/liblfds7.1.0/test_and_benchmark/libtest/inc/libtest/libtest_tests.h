/***** enums *****/
enum libtest_test_id
{
  LIBTEST_TEST_ID_PORTING_ABSTRACTION_LAYER_ADD,
  LIBTEST_TEST_ID_PORTING_ABSTRACTION_LAYER_CAS,
  LIBTEST_TEST_ID_PORTING_ABSTRACTION_LAYER_DCAS,
  LIBTEST_TEST_ID_PORTING_ABSTRACTION_LAYER_EXCHANGE,

  LIBTEST_TEST_ID_BTREE_ADDONLY_UNBALANCED_ALIGNMENT,
  LIBTEST_TEST_ID_BTREE_ADDONLY_UNBALANCED_RANDOM_ADDS_FAIL,
  LIBTEST_TEST_ID_BTREE_ADDONLY_UNBALANCED_RANDOM_ADDS_FAIL_AND_OVERWRITE,
  LIBTEST_TEST_ID_BTREE_ADDONLY_UNBALANCED_RANDOM_ADDS_OVERWRITE,

  LIBTEST_TEST_ID_FREELIST_ALIGNMENT,
  LIBTEST_TEST_ID_FREELIST_EA_POPPING,
  LIBTEST_TEST_ID_FREELIST_EA_POPPING_AND_PUSHING,
  LIBTEST_TEST_ID_FREELIST_EA_PUSHING,
  LIBTEST_TEST_ID_FREELIST_EA_RAPID_POPPING_AND_PUSHING,
  LIBTEST_TEST_ID_FREELIST_WITHOUT_EA_POPPING,
  LIBTEST_TEST_ID_FREELIST_WITHOUT_EA_POPPING_AND_PUSHING,
  LIBTEST_TEST_ID_FREELIST_WITHOUT_EA_PUSHING,
  LIBTEST_TEST_ID_FREELIST_WITHOUT_EA_RAPID_POPPING_AND_PUSHING,

  LIBTEST_TEST_ID_HASH_ADDONLY_ALIGNMENT,
  LIBTEST_TEST_ID_HASH_ADDONLY_FAIL_AND_OVERWRITE,
  LIBTEST_TEST_ID_HASH_ADDONLY_RANDOM_ADDS_FAIL,
  LIBTEST_TEST_ID_HASH_ADDONLY_RANDOM_ADDS_OVERWRITE,
  LIBTEST_TEST_ID_HASH_ADDONLY_ITERATE,

  LIBTEST_TEST_ID_LIST_ADDONLY_SINGLYLINKED_ORDERED_ALIGNMENT,
  LIBTEST_TEST_ID_LIST_ADDONLY_SINGLYLINKED_ORDERED_NEW_ORDERED,
  LIBTEST_TEST_ID_LIST_ADDONLY_SINGLYLINKED_ORDERED_NEW_ORDERED_WITH_CURSOR,

  LIBTEST_TEST_ID_LIST_ADDONLY_SINGLYLINKED_UNORDERED_ALIGNMENT,
  LIBTEST_TEST_ID_LIST_ADDONLY_SINGLYLINKED_UNORDERED_NEW_START,
  LIBTEST_TEST_ID_LIST_ADDONLY_SINGLYLINKED_UNORDERED_NEW_END,
  LIBTEST_TEST_ID_LIST_ADDONLY_SINGLYLINKED_UNORDERED_NEW_AFTER,

  LIBTEST_TEST_ID_PRNG_ALIGNMENT,
  LIBTEST_TEST_ID_PRNG_GENERATE,

  LIBTEST_TEST_ID_QUEUE_UMM_ALIGNMENT,
  LIBTEST_TEST_ID_QUEUE_UMM_DEQUEUING,
  LIBTEST_TEST_ID_QUEUE_UMM_ENQUEUING,
  LIBTEST_TEST_ID_QUEUE_UMM_ENQUEUING_AND_DEQUEUING,
  LIBTEST_TEST_ID_QUEUE_UMM_ENQUEUING_WITH_MALLOC_AND_DEQUEUING_WITH_FREE,
  LIBTEST_TEST_ID_QUEUE_UMM_RAPID_ENQUEUING_AND_DEQUEUING,

  LIBTEST_TEST_ID_QUEUE_BMM_ALIGNMENT,
  LIBTEST_TEST_ID_QUEUE_BMM_COUNT,
  LIBTEST_TEST_ID_QUEUE_BMM_ENQUEUING,
  LIBTEST_TEST_ID_QUEUE_BMM_DEQUEUING,
  LIBTEST_TEST_ID_QUEUE_BMM_ENQUEUING_AND_DEQUEUING,
  LIBTEST_TEST_ID_QUEUE_BMM_RAPID_ENQUEUING_AND_DEQUEUING,

  LIBTEST_TEST_ID_QUEUE_BSS_DEQUEUING,
  LIBTEST_TEST_ID_QUEUE_BSS_ENQUEUING,
  LIBTEST_TEST_ID_QUEUE_BSS_ENQUEUING_AND_DEQUEUING,

  LIBTEST_TEST_ID_RINGBUFFER_READING,
  LIBTEST_TEST_ID_RINGBUFFER_WRITING,
  LIBTEST_TEST_ID_RINGBUFFER_READING_AND_WRITING,

  LIBTEST_TEST_ID_STACK_ALIGNMENT,
  LIBTEST_TEST_ID_STACK_POPPING,
  LIBTEST_TEST_ID_STACK_POPPING_AND_PUSHING,
  LIBTEST_TEST_ID_STACK_PUSHING,
  LIBTEST_TEST_ID_STACK_RAPID_POPPING_AND_PUSHING,

  LIBTEST_TEST_ID_COUNT
};

/***** public prototypes *****/
void libtest_tests_btree_au_alignment( struct lfds710_list_asu_state *list_of_logical_processors, struct libshared_memory_state *ms, enum lfds710_misc_validity *dvs );
void libtest_tests_btree_au_random_adds_fail_on_existing( struct lfds710_list_asu_state *list_of_logical_processors, struct libshared_memory_state *ms, enum lfds710_misc_validity *dvs );
void libtest_tests_btree_au_random_adds_overwrite_on_existing( struct lfds710_list_asu_state *list_of_logical_processors, struct libshared_memory_state *ms, enum lfds710_misc_validity *dvs );
void libtest_tests_btree_au_fail_and_overwrite_on_existing_key( struct lfds710_list_asu_state *list_of_logical_processors, struct libshared_memory_state *ms, enum lfds710_misc_validity *dvs );

void libtest_tests_freelist_alignment( struct lfds710_list_asu_state *list_of_logical_processors, struct libshared_memory_state *ms, enum lfds710_misc_validity *dvs );
void libtest_tests_freelist_ea_popping( struct lfds710_list_asu_state *list_of_logical_processors, struct libshared_memory_state *ms, enum lfds710_misc_validity *dvs );
void libtest_tests_freelist_ea_popping_and_pushing( struct lfds710_list_asu_state *list_of_logical_processors, struct libshared_memory_state *ms, enum lfds710_misc_validity *dvs );
void libtest_tests_freelist_ea_pushing( struct lfds710_list_asu_state *list_of_logical_processors, struct libshared_memory_state *ms, enum lfds710_misc_validity *dvs );
void libtest_tests_freelist_ea_rapid_popping_and_pushing( struct lfds710_list_asu_state *list_of_logical_processors, struct libshared_memory_state *ms, enum lfds710_misc_validity *dvs );
void libtest_tests_freelist_without_ea_popping( struct lfds710_list_asu_state *list_of_logical_processors, struct libshared_memory_state *ms, enum lfds710_misc_validity *dvs );
void libtest_tests_freelist_without_ea_popping_and_pushing( struct lfds710_list_asu_state *list_of_logical_processors, struct libshared_memory_state *ms, enum lfds710_misc_validity *dvs );
void libtest_tests_freelist_without_ea_pushing( struct lfds710_list_asu_state *list_of_logical_processors, struct libshared_memory_state *ms, enum lfds710_misc_validity *dvs );
void libtest_tests_freelist_without_ea_rapid_popping_and_pushing( struct lfds710_list_asu_state *list_of_logical_processors, struct libshared_memory_state *ms, enum lfds710_misc_validity *dvs );

void libtest_tests_hash_a_alignment( struct lfds710_list_asu_state *list_of_logical_processors, struct libshared_memory_state *ms, enum lfds710_misc_validity *dvs );
void libtest_tests_hash_a_fail_and_overwrite_on_existing_key( struct lfds710_list_asu_state *list_of_logical_processors, struct libshared_memory_state *ms, enum lfds710_misc_validity *dvs );
void libtest_tests_hash_a_random_adds_fail_on_existing( struct lfds710_list_asu_state *list_of_logical_processors, struct libshared_memory_state *ms, enum lfds710_misc_validity *dvs );
void libtest_tests_hash_a_random_adds_overwrite_on_existing( struct lfds710_list_asu_state *list_of_logical_processors, struct libshared_memory_state *ms, enum lfds710_misc_validity *dvs );
void libtest_tests_hash_a_iterate( struct lfds710_list_asu_state *list_of_logical_processors, struct libshared_memory_state *ms, enum lfds710_misc_validity *dvs );

void libtest_tests_list_aso_alignment( struct lfds710_list_asu_state *list_of_logical_processors, struct libshared_memory_state *ms, enum lfds710_misc_validity *dvs );
void libtest_tests_list_aso_new_ordered( struct lfds710_list_asu_state *list_of_logical_processors, struct libshared_memory_state *ms, enum lfds710_misc_validity *dvs );
void libtest_tests_list_aso_new_ordered_with_cursor( struct lfds710_list_asu_state *list_of_logical_processors, struct libshared_memory_state *ms, enum lfds710_misc_validity *dvs );

void libtest_tests_list_asu_alignment( struct lfds710_list_asu_state *list_of_logical_processors, struct libshared_memory_state *ms, enum lfds710_misc_validity *dvs );
void libtest_tests_list_asu_new_start( struct lfds710_list_asu_state *list_of_logical_processors, struct libshared_memory_state *ms, enum lfds710_misc_validity *dvs );
void libtest_tests_list_asu_new_after( struct lfds710_list_asu_state *list_of_logical_processors, struct libshared_memory_state *ms, enum lfds710_misc_validity *dvs );
void libtest_tests_list_asu_new_end( struct lfds710_list_asu_state *list_of_logical_processors, struct libshared_memory_state *ms, enum lfds710_misc_validity *dvs );

void libtest_tests_pal_atomic_add( struct lfds710_list_asu_state *list_of_logical_processors, struct libshared_memory_state *ms, enum lfds710_misc_validity *dvs );
void libtest_tests_pal_atomic_cas( struct lfds710_list_asu_state *list_of_logical_processors, struct libshared_memory_state *ms, enum lfds710_misc_validity *dvs );
void libtest_tests_pal_atomic_dwcas( struct lfds710_list_asu_state *list_of_logical_processors, struct libshared_memory_state *ms, enum lfds710_misc_validity *dvs );
void libtest_tests_pal_atomic_exchange( struct lfds710_list_asu_state *list_of_logical_processors, struct libshared_memory_state *ms, enum lfds710_misc_validity *dvs );

void libtest_tests_prng_alignment( struct lfds710_list_asu_state *list_of_logical_processors, struct libshared_memory_state *ms, enum lfds710_misc_validity *dvs );
void libtest_tests_prng_generate( struct lfds710_list_asu_state *list_of_logical_processors, struct libshared_memory_state *ms, enum lfds710_misc_validity *dvs );

void libtest_tests_queue_bmm_alignment( struct lfds710_list_asu_state *list_of_logical_processors, struct libshared_memory_state *ms, enum lfds710_misc_validity *dvs );
void libtest_tests_queue_bmm_count( struct lfds710_list_asu_state *list_of_logical_processors, struct libshared_memory_state *ms, enum lfds710_misc_validity *dvs );
void libtest_tests_queue_bmm_enqueuing( struct lfds710_list_asu_state *list_of_logical_processors, struct libshared_memory_state *ms, enum lfds710_misc_validity *dvs );
void libtest_tests_queue_bmm_dequeuing( struct lfds710_list_asu_state *list_of_logical_processors, struct libshared_memory_state *ms, enum lfds710_misc_validity *dvs );
void libtest_tests_queue_bmm_enqueuing_and_dequeuing( struct lfds710_list_asu_state *list_of_logical_processors, struct libshared_memory_state *ms, enum lfds710_misc_validity *dvs );
void libtest_tests_queue_bmm_rapid_enqueuing_and_dequeuing( struct lfds710_list_asu_state *list_of_logical_processors, struct libshared_memory_state *ms, enum lfds710_misc_validity *dvs );

void libtest_tests_queue_bss_dequeuing( struct lfds710_list_asu_state *list_of_logical_processors, struct libshared_memory_state *ms, enum lfds710_misc_validity *dvs );
void libtest_tests_queue_bss_enqueuing( struct lfds710_list_asu_state *list_of_logical_processors, struct libshared_memory_state *ms, enum lfds710_misc_validity *dvs );
void libtest_tests_queue_bss_enqueuing_and_dequeuing( struct lfds710_list_asu_state *list_of_logical_processors, struct libshared_memory_state *ms, enum lfds710_misc_validity *dvs );

void libtest_tests_queue_umm_alignment( struct lfds710_list_asu_state *list_of_logical_processors, struct libshared_memory_state *ms, enum lfds710_misc_validity *dvs );
void libtest_tests_queue_umm_enqueuing( struct lfds710_list_asu_state *list_of_logical_processors, struct libshared_memory_state *ms, enum lfds710_misc_validity *dvs );
void libtest_tests_queue_umm_dequeuing( struct lfds710_list_asu_state *list_of_logical_processors, struct libshared_memory_state *ms, enum lfds710_misc_validity *dvs );
void libtest_tests_queue_umm_enqueuing_and_dequeuing( struct lfds710_list_asu_state *list_of_logical_processors, struct libshared_memory_state *ms, enum lfds710_misc_validity *dvs );
void libtest_tests_queue_umm_enqueuing_with_malloc_and_dequeuing_with_free( struct lfds710_list_asu_state *list_of_logical_processors, struct libshared_memory_state *ms, enum lfds710_misc_validity *dvs );
void libtest_tests_queue_umm_rapid_enqueuing_and_dequeuing( struct lfds710_list_asu_state *list_of_logical_processors, struct libshared_memory_state *ms, enum lfds710_misc_validity *dvs );

void libtest_tests_ringbuffer_reading( struct lfds710_list_asu_state *list_of_logical_processors, struct libshared_memory_state *ms, enum lfds710_misc_validity *dvs );
void libtest_tests_ringbuffer_writing( struct lfds710_list_asu_state *list_of_logical_processors, struct libshared_memory_state *ms, enum lfds710_misc_validity *dvs );
void libtest_tests_ringbuffer_reading_and_writing( struct lfds710_list_asu_state *list_of_logical_processors, struct libshared_memory_state *ms, enum lfds710_misc_validity *dvs );

void libtest_tests_stack_alignment( struct lfds710_list_asu_state *list_of_logical_processors, struct libshared_memory_state *ms, enum lfds710_misc_validity *dvs );
void libtest_tests_stack_popping( struct lfds710_list_asu_state *list_of_logical_processors, struct libshared_memory_state *ms, enum lfds710_misc_validity *dvs );
void libtest_tests_stack_popping_and_pushing( struct lfds710_list_asu_state *list_of_logical_processors, struct libshared_memory_state *ms, enum lfds710_misc_validity *dvs );
void libtest_tests_stack_pushing( struct lfds710_list_asu_state *list_of_logical_processors, struct libshared_memory_state *ms, enum lfds710_misc_validity *dvs );
void libtest_tests_stack_rapid_popping_and_pushing( struct lfds710_list_asu_state *list_of_logical_processors, struct libshared_memory_state *ms, enum lfds710_misc_validity *dvs );


