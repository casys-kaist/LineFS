#! /bin/bash

#PATH=$PATH:.
##LD_PRELOAD=../shim/libshim/libshim.so LD_LIBRARY_PATH=../libfs/:../submodules/nvml/src/nondebug/ ${@}
#export LD_LIBRARY_PATH=/lib/x86_64-linux-gnu/:../lib/nvml/src/nondebug/:../build:../../shim/glibc-build/:../lib/rdma/librdma.so:../../shim/libshim:../lib/rdma-core/build/lib/:/lib/
##LD_LIBRARY_PATH=../lib/nvml/src/nondebug/:../../shim/glibc-build/rt/:../lib/libspdk/libspdk/libspdk.so 
#
##LD_PRELOAD=../../shim/libshim/libshim.so ${@} |& tee r.strace
##LD_PRELOAD=../../shim/libshim/libshim.so:../../deps/mutrace/.libs/libmutrace.so MUTRACE_HASH_SIZE=2000000 taskset -c 0,7 ${@} 
##LD_PRELOAD=../../shim/libshim/libshim.so:../../deps/mutrace/.libs/libmutrace.so MUTRACE_HASH_SIZE=2000000  MLFS_PROFILE=1 ${@} 
##LD_PRELOAD=../../shim/libshim/libshim.so MLFS_PROFILE=1 ${@}
##LD_PRELOAD=../../shim/libshim/libshim.so:../lib/jemalloc-4.5.0/lib/libjemalloc.so.2 taskset -c 0,7 ${@}
#LD_PRELOAD=../../shim/libshim/libshim.so:../lib/jemalloc-4.5.0/lib/libjemalloc.so.2:../lib/rdma/librdma.so:../lib/rdma-core/build/lib/librdmacm.so.1:../lib/rdma-core/build/lib/libibverbs.so.1 MLFS_PROFILE=1 ${@}
##LD_PRELOAD=../../shim/libshim/libshim.so:../lib/jemalloc-4.5.0/lib/libjemalloc.so.2 MLFS_PROFILE=1 ${@}
##LD_PRELOAD=../../shim/libshim/libshim.so taskset -c 0,7 ${@}
##LD_PRELOAD=../../shim/libshim/libshim.so ${@}

source ../../mlfs_config.sh
$@
