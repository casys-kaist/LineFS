# from ../../mlfs_conf.sh
set environment NET_INTERFACE_NAME=enp59s0f0
set environment PERSIST_NVM=1
set environment PERSIST_NVM_WITH_CLFLUSH=0
set environment PERSIST_NVM_WITH_RDMA_READ=1
set environment LOG_COALESCE=0	# Not supported in NIC-offloading setup.
set environment DIGEST_THRESHOLD=30
set environment DIGEST_OPT_FCONCURRENT=0
set environment IOAT_MEMCPY_OFFLOAD=1
set environment ASYNC_REPLICATION=0

set environment DIGEST_OPT_HOST_MEMCPY=1
set environment DIGEST_OPT_PARALLEL_RDMA_MEMCPY=0
set environment LOG_PREFETCHING=1
set environment LOG_PREFETCH_THRESHOLD=1024
set environment M_TO_N_REP_THREAD=0
set environment NIC_SLAB_THRESHOLD_HIGH=80
set environment NIC_SLAB_THRESHOLD_LOW=30

set environment BREAKDOWN=1
set environment REPLICATION_BREAKDOWN=1
set environment DIGEST_BREAKDOWN=1
set environment BREAKDOWN_MP=0

set environment THREAD_NUM_DIGEST=1
set environment THREAD_NUM_DIGEST_FCONCURRENT=8
set environment THREAD_NUM_REP=8
set environment THREAD_NUM_DIGEST_RDMA_MEMCPY=1
set environment THREAD_NUM_DIGEST_HOST_MEMCPY=1
set environment THREAD_NUM_LOG_PREFETCH=2
set environment THREAD_NUM_LOG_PREFETCH_REQ=1
set environment THREAD_NUM_PREPARE_LOGHDRS=8

set environment THREAD_NUM_COALESCE=1
set environment THREAD_NUM_LOGHDR_BUILD=1
set environment THREAD_NUM_LOGHDR_FETCH=1
set environment THREAD_NUM_COMPRESS=1
set environment THREAD_NUM_LOG_FETCH=2
set environment THREAD_NUM_FSYNC=1
set environment THREAD_NUM_COPY_LOG_TO_LOCAL_NVM=2
set environment THREAD_NUM_COPY_LOG_TO_LAST_REPLICA=2
set environment THREAD_NUM_PERSIST_LOG=4

set environment REQUEST_RATE_LIMIT_THRESHOLD=500

set environment DIGEST_NOOP=0
set environment HYPERLOOP_OPS_FILE_PATH="/home/yulistic/assise-nic/libfs/lib/hyperloop/trace/micro/tpu/sw.12g.1th.1M"
