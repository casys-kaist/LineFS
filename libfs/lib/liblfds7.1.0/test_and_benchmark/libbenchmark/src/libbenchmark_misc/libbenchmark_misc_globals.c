/***** includes *****/
#include "libbenchmark_misc_internal.h"





/****************************************************************************/
char const
  * const libbenchmark_globals_datastructure_names[] = 
  {
    "btree_au",
    "freelist",
    "queue_umm"
  },
  * const libbenchmark_globals_benchmark_names[] = 
  {
    "readn_then_writen",
    "push1_then_pop1",
    "enqueue1_then_dequeue1"
  },
  * const libbenchmark_globals_lock_names[] = 
  {
    "GCC spinlock (atomic)",
    "GCC spinlock (sync)",
    "liblfds700 (lock-free)",
    "liblfds710 (lock-free)",
    "MSVC spinlock",
    "pthread mutex",
    "pthread rwlock",
    "pthread spinlock (private)",
    "pthread spinlock (shared)",
    "windows critical section",
    "windows mutex"
  },
  * const libbenchmark_globals_numa_mode_names[] = 
  {
    "smp",
    "numa",
    "numa_unused"
  };

lfds710_pal_uint_t
  libbenchmark_globals_benchmark_duration_in_seconds = DEFAULT_BENCHMARK_DURATION_IN_SECONDS;


