define load_sl
	sharedlibrary libshim.so
	sharedlibrary libmlfs.so
end

set auto-solib-add on
set auto-load safe-path ../../shim/glibc-build:../../shim/glibc-2.19
set libthread-db-search-path ../../shim/glibc-build/nptl_db/
set environment LD_PRELOAD ../../shim/libshim/libshim.so:../lib/jemalloc-4.5.0/lib/libjemalloc.so.2
set environment LD_LIBRARY_PATH ../lib/nvml/src/nondebug/:../build/:../../shim/glibc-build/rt/:../src/storage/spdk/
#set environment DEV_ID 4

set environment PERSIST_NVM=1
set environment PERSIST_NVM_WITH_CLFLUSH=0
set environment PERSIST_NVM_WITH_RDMA_READ=1
set environment LOG_COALESCE=0	# Not supported in NIC-offloading setup.
set environment DIGEST_OPT_FCONCURRENT=0
set environment IOAT_MEMCPY_OFFLOAD=1
set environment ASYNC_REPLICATION=0
set environment DIGEST_OPT_HOST_MEMCPY=1
set environment DIGEST_OPT_PARALLEL_RDMA_MEMCPY=0
set environment BREAKDOWN=0
set environment REPLICATION_BREAKDOWN=0
set environment DIGEST_BREAKDOWN=0
set environment BREAKDOWN_MP=1
set environment THREAD_NUM_DIGEST=8
set environment THREAD_NUM_DIGEST_FCONCURRENT=8
set environment THREAD_NUM_REP=1
set environment THREAD_NUM_DIGEST_RDMA_MEMCPY=1
set environment THREAD_NUM_DIGEST_HOST_MEMCPY=1
set environment DIGEST_NOOP=0

set follow-fork-mode child

# loading python modules.
source ../../gdb_python_modules/load_modules.py

# this is macro to setup for breakpoint
define setup_br
	b 60
# run programe
	r w aa 40K

	l add_to_log
	b 541  if $caller_is("posix_write", 8)
	#b balloc if $caller_is("posix_write", 8)
end
