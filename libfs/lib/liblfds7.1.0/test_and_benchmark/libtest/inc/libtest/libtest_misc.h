/***** defines *****/
#define LIBTEST_MISC_VERSION_STRING   "7.1.0"
#define LIBTEST_MISC_VERSION_INTEGER  710

#define LIBTEST_MISC_OFFSETOF( structure, member )  (lfds710_pal_uint_t) ( ( &( (structure *) NULL )->member ) )

/***** enums *****/
enum libtest_misc_determine_erg_result
{
  LIBTEST_MISC_DETERMINE_ERG_RESULT_SUCCESS,
  LIBTEST_MISC_DETERMINE_ERG_RESULT_ONE_PHYSICAL_CORE,
  LIBTEST_MISC_DETERMINE_ERG_RESULT_ONE_PHYSICAL_CORE_OR_NO_LLSC,
  LIBTEST_MISC_DETERMINE_ERG_RESULT_NO_LLSC,
  LIBTEST_MISC_DETERMINE_ERG_RESULT_NOT_SUPPORTED
};

enum libtest_misc_query
{
  LIBTEST_MISC_QUERY_GET_BUILD_AND_VERSION_STRING
};

/***** externs *****/
extern char
  *libtest_misc_global_validity_names[];

/***** public prototypes *****/
void *libtest_misc_aligned_malloc( lfds710_pal_uint_t size, lfds710_pal_uint_t align_in_bytes );
void libtest_misc_aligned_free( void *memory );

void libtest_misc_determine_erg( struct libshared_memory_state *ms,
                                 lfds710_pal_uint_t (*count_array)[10],
                                 enum libtest_misc_determine_erg_result *der,
                                 lfds710_pal_uint_t *erg_length_in_bytes );

void libtest_misc_pal_helper_add_logical_processor_to_list_of_logical_processors( struct lfds710_list_asu_state *list_of_logical_processors,
                                                                                  struct libshared_memory_state *ms,
                                                                                  lfds710_pal_uint_t logical_processor_number,
                                                                                  lfds710_pal_uint_t windows_processor_group_number );

void libtest_misc_query( enum libtest_misc_query query_type,
                         void *query_input,
                         void *query_output );

