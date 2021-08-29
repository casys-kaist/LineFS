# options are copied from mlfs_conf.sh

# set environment NET_INTERFACE_NAME="enp175s0f1"
set environment X86_NET_INTERFACE_NAME="enp59s0f0"
set environment ARM_NET_INTERFACE_NAME="enp3s0f0"
set environment PORT_NUM="12345"
set environment PERSIST_NVM=1
set environment PERSIST_NVM_WITH_CLFLUSH=0
set environment PERSIST_NVM_WITH_RDMA_READ=1
set environment LOG_COALESCE=0	# Not supported in NIC-offloading setup.
set environment DIGEST_THRESHOLD=30
set environment DIGEST_OPT_FCONCURRENT=0
set environment IOAT_MEMCPY_OFFLOAD=1
set environment ASYNC_REPLICATION=0	# background log copy.
set environment NUMA_NODE=0		# CPU core binding.

# nic-offload
set environment DIGEST_OPT_HOST_MEMCPY=1
set environment DIGEST_OPT_PARALLEL_RDMA_MEMCPY=0
set environment LOG_PREFETCHING=1
set environment LOG_PREFETCH_THRESHOLD=4096  # Used in Libfs.
set environment M_TO_N_REP_THREAD=0
set environment NIC_SLAB_THRESHOLD_HIGH=60
set environment NIC_SLAB_THRESHOLD_LOW=50
set environment HOST_MEMCPY_BATCH_MAX=30

# breakdown.
set environment BREAKDOWN=0
set environment REPLICATION_BREAKDOWN=1
set environment DIGEST_BREAKDOWN=1
set environment BREAKDOWN_MP=0

set environment THREAD_NUM_DIGEST=8 # Not used in NIC offloading.
set environment THREAD_NUM_DIGEST_FCONCURRENT=8
set environment THREAD_NUM_REP=2
set environment THREAD_NUM_DIGEST_RDMA_MEMCPY=1
set environment THREAD_NUM_DIGEST_HOST_MEMCPY=1 # host kernfs. max=8
set environment THREAD_NUM_LOG_PREFETCH=1 # From local NVM to NICFS in Primary.
set environment THREAD_NUM_LOG_PREFETCH_REQ=1 # libfs.
set environment THREAD_NUM_PREPARE_LOGHDRS=8

set environment THREAD_NUM_COALESCE=1
set environment THREAD_NUM_LOGHDR_BUILD=1
set environment THREAD_NUM_LOGHDR_FETCH=1
set environment THREAD_NUM_COMPRESS=8
set environment THREAD_NUM_LOG_FETCH=1 # Primary to Replica 1
set environment THREAD_NUM_FSYNC=1
set environment THREAD_NUM_COPY_LOG_TO_LOCAL_NVM=1
set environment THREAD_NUM_COPY_LOG_TO_LAST_REPLICA=1
set environment THREAD_NUM_PERSIST_LOG=4
set environment THREAD_NUM_END_PIPELINE=2

# Parameters.
set environment REQUEST_RATE_LIMIT_THRESHOLD=500
set environment PREFETCH_DATA_CAP=2500	# Prefetch data cap in MB.

# For experiment
set environment DIGEST_NOOP=0	# Not supported in NIC-offloading setup.
set environment HYPERLOOP_OPS_FILE_PATH='/home/yulistic/assise-host-only/libfs/lib/hyperloop/trace/micro/latency/sw.1g.4K'

set pagination off
handle SIG32 nostop
