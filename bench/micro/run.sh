#! /bin/bash

#PATH=$PATH:.
#PROJECT_ROOT=../../
##LD_PRELOAD=../shim/libshim/libshim.so LD_LIBRARY_PATH=../libfs/:../submodules/nvml/src/nondebug/ ${@}
#export LD_LIBRARY_PATH=../lib/nvml/src/nondebug/:../build:../../shim/glibc-build/rt/:$PROJECT_ROOT/libfs/lib/rdma/librdma.so 
##LD_LIBRARY_PATH=../lib/nvml/src/nondebug/:../../shim/glibc-build/rt/:../lib/libspdk/libspdk/libspdk.so 
#
##LD_PRELOAD=../../shim/libshim/libshim.so ${@} |& tee r.strace
##LD_PRELOAD=../../shim/libshim/libshim.so:../../deps/mutrace/.libs/libmutrace.so MUTRACE_HASH_SIZE=2000000 taskset -c 0,7 ${@} 
##LD_PRELOAD=../../shim/libshim/libshim.so:../../deps/mutrace/.libs/libmutrace.so MUTRACE_HASH_SIZE=2000000  MLFS_PROFILE=1 ${@} 
##LD_PRELOAD=../../shim/libshim/libshim.so MLFS_PROFILE=1 ${@}
##LD_PRELOAD=../../shim/libshim/libshim.so:../lib/jemalloc-4.5.0/lib/libjemalloc.so.2 taskset -c 0,7 ${@}
#LD_PRELOAD=$PROJECT_ROOT/libfs/lib/jemalloc-4.5.0/lib/libjemalloc.so.2:$PROJECT_ROOT/libfs/lib/rdma/librdma.so MLFS_PROFILE=1 ${@}
##LD_PRELOAD=../../shim/libshim/libshim.so:../lib/jemalloc-4.5.0/lib/libjemalloc.so.2 MLFS_PROFILE=1 ${@}
##LD_PRELOAD=../../shim/libshim/libshim.so taskset -c 0,7 ${@}
##LD_PRELOAD=../../shim/libshim/libshim.so ${@}

source ../../mlfs_config.sh
$@
