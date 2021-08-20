/***** includes *****/
#include "libbenchmark_benchmarkset_internal.h"





/****************************************************************************/
void libbenchmark_benchmarkset_add_benchmark( struct libbenchmark_benchmarkset_state *bss, struct libbenchmark_benchmarkinstance_state *bs )
{
  LFDS710_PAL_ASSERT( bss != NULL );
  LFDS710_PAL_ASSERT( bs != NULL );

  LFDS710_LIST_ASU_SET_VALUE_IN_ELEMENT( bs->lasue, bs );
  lfds710_list_asu_insert_at_end( &bss->benchmarks, &bs->lasue );

  return;
}

