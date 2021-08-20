/***** defines *****/
#define LIBSHARED_PAL_PTI_GET_LOGICAL_PROCESSOR_NUMBER( libshared_pal_thread_info )        (libshared_pal_thread_info).logical_processor_number
#define LIBSHARED_PAL_PTI_GET_WINDOWS_PROCESSOR_GROUP_NUMBER( libshared_pal_thread_info )  (libshared_pal_thread_info).windows_processor_group_number
#define LIBSHARED_PAL_PTI_GET_NUMA_NODE_ID( libshared_pal_thread_info )                    (libshared_pal_thread_info).numa_node_id
#define LIBSHARED_PAL_PTI_GET_THREAD_FUNCTION( libshared_pal_thread_info )                 (libshared_pal_thread_info).thread_function
#define LIBSHARED_PAL_PTI_GET_THREAD_ARGUMENT( libshared_pal_thread_info )                 (libshared_pal_thread_info).thread_argument

/***** structs *****/
struct libshared_pal_thread_info
{
  // TRD : this struct must be user-allocated and last till the thread ends - needed for thread pinning on android

  lfds710_pal_uint_t
    logical_processor_number,
    numa_node_id,
    windows_processor_group_number;

  libshared_pal_thread_return_t
    (LIBSHARED_PAL_THREAD_CALLING_CONVENTION *thread_function)( void *thread_argument );

  void
    *thread_argument;
};

/***** public prototypes *****/
int libshared_pal_thread_start( libshared_pal_thread_handle_t *thread_handle,
                                struct libshared_pal_thread_info *pti );

void libshared_pal_thread_wait( libshared_pal_thread_handle_t thread_handle );

