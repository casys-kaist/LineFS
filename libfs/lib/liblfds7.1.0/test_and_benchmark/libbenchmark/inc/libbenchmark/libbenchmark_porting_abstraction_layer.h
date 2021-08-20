/***** defines *****/
#if( !defined NULL )
  #define NULL (void *) 0
#endif

#if( defined __GNUC__ )
  // TRD : makes checking GCC versions much tidier
  #define LIBLFDS_GCC_VERSION ( __GNUC__ * 100 + __GNUC_MINOR__ * 10 + __GNUC_PATCHLEVEL__ )
#endif

/***** structs *****/

/***** public prototypes *****/
int libbenchmark_porting_abstraction_layer_populate_topology( struct libbenchmark_topology_state *ts, struct libshared_memory_state *ms );
void libbenchmark_porting_abstraction_layer_topology_node_cleanup( struct libbenchmark_topology_node_state *tns );

void libbenchmark_pal_print_string( char const * const string );

