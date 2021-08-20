/***** includes *****/
#include "libtest_misc_internal.h"





/****************************************************************************/
void *libtest_misc_aligned_malloc( lfds710_pal_uint_t size, lfds710_pal_uint_t align_in_bytes )
{
  lfds710_pal_uint_t
    offset;

  void
    *memory,
    *original_memory;

  // TRD : size can be any value in its range
  // TRD : align_in_bytes can be any value in its range

  /* TRD : helper function to provide aligned allocations
           no porting required
  */

  original_memory = memory = libtest_pal_malloc( size + sizeof(void *) + align_in_bytes );

  if( memory != NULL )
  {
    memory = (void **) memory + 1;
    offset = align_in_bytes - (lfds710_pal_uint_t) memory % align_in_bytes;
    memory = (char unsigned *) memory + offset;
    *( (void **) memory - 1 ) = original_memory;
  }

  return memory;
}





/****************************************************************************/
void libtest_misc_aligned_free( void *memory )
{
  LFDS710_PAL_ASSERT( memory != NULL );

  // TRD : the "void *" stored above memory points to the root of the allocation
  libtest_pal_free( *( (void **) memory - 1 ) );

  return;
}

