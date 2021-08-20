/***** defines *****/
#define LIBSHARED_MISC_VERSION_STRING   "7.1.0"
#define LIBSHARED_MISC_VERSION_INTEGER  710

/***** enums *****/
enum libshared_misc_query
{
  LIBSHARED_MISC_QUERY_GET_BUILD_AND_VERSION_STRING
};

/***** externs *****/

/***** public prototypes *****/
void libshared_misc_query( enum libshared_misc_query query_type, void *query_input, void *query_output );

