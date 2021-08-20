/***** includes *****/
#include "libtest_tests_internal.h"





/****************************************************************************/
#pragma warning( disable : 4100 4127 ) // TRD : disables MSVC warning for condition expressions being const

void libtest_tests_queue_bmm_alignment( struct lfds710_list_asu_state *list_of_logical_processors, struct libshared_memory_state *ms, enum lfds710_misc_validity *dvs )
{
  /* TRD : these are compile time checks
           but we do them here because this is a test programme
           and it should indicate issues to users when it is *run*,
           not when it is compiled, because a compile error normally
           indicates a problem with the code itself and so is misleading
  */

  LFDS710_PAL_ASSERT( list_of_logical_processors != NULL );
  LFDS710_PAL_ASSERT( ms != NULL );
  LFDS710_PAL_ASSERT( dvs != NULL );

  *dvs = LFDS710_MISC_VALIDITY_VALID;

  // TRD : struct lfds710_queue_bmm_state
  if( LIBTEST_MISC_OFFSETOF(struct lfds710_queue_bmm_state,read_index) % LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES != 0 )
    *dvs = LFDS710_MISC_VALIDITY_INVALID_TEST_DATA;

  if( LIBTEST_MISC_OFFSETOF(struct lfds710_queue_bmm_state,write_index) % LFDS710_PAL_ATOMIC_ISOLATION_IN_BYTES != 0 )
    *dvs = LFDS710_MISC_VALIDITY_INVALID_TEST_DATA;

  return;
}

#pragma warning( default : 4100 4127 )

