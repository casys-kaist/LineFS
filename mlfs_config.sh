#!/bin/bash
# Runtime configurations of Assise. They are passed as env variables.

# basic.
# export NET_INTERFACE_NAME="enp175s0f1"
export X86_NET_INTERFACE_NAME="ens1f0"
export ARM_NET_INTERFACE_NAME="enp3s0f0"
export PORT_NUM="12345"
export PERSIST_NVM=1
export PERSIST_NVM_WITH_CLFLUSH=0
export PERSIST_NVM_WITH_RDMA_READ=1
export LOG_COALESCE=0	# Not supported in NIC-offloading setup.
export DIGEST_THRESHOLD=30
export DIGEST_OPT_FCONCURRENT=0
export IOAT_MEMCPY_OFFLOAD=1
export ASYNC_REPLICATION=0	# background log copy.
export KERNFS_NUM_CORES=16 # The number of cores pinned for kernfs.
export NUMA_NODE=1		# CPU core binding.

# nic-offload
#export NIC_OFFLOAD=1 # Not used currently.
export DIGEST_OPT_HOST_MEMCPY=1
export DIGEST_OPT_PARALLEL_RDMA_MEMCPY=0
export LOG_PREFETCHING=1
export LOG_PREFETCH_THRESHOLD=4096  # Used in Libfs.
export M_TO_N_REP_THREAD=0
export NIC_SLAB_THRESHOLD_HIGH=60
export NIC_SLAB_THRESHOLD_LOW=50
export HOST_MEMCPY_BATCH_MAX=30

# breakdown.
export BREAKDOWN=0
export REPLICATION_BREAKDOWN=1
export DIGEST_BREAKDOWN=1
export BREAKDOWN_MP=0

export THREAD_NUM_DIGEST=8 # Not used in NIC offloading.
export THREAD_NUM_DIGEST_FCONCURRENT=8
export THREAD_NUM_REP=2
export THREAD_NUM_DIGEST_RDMA_MEMCPY=1
export THREAD_NUM_DIGEST_HOST_MEMCPY=1 # host kernfs. max=8
export THREAD_NUM_LOG_PREFETCH=1 # From local NVM to NICFS in Primary.
export THREAD_NUM_LOG_PREFETCH_REQ=1 # libfs.
export THREAD_NUM_PREPARE_LOGHDRS=8

export THREAD_NUM_COALESCE=1
export THREAD_NUM_LOGHDR_BUILD=1
export THREAD_NUM_LOGHDR_FETCH=1
export THREAD_NUM_COMPRESS=8
export THREAD_NUM_LOG_FETCH=1 # Primary to Replica 1
export THREAD_NUM_FSYNC=1
export THREAD_NUM_COPY_LOG_TO_LOCAL_NVM=1
export THREAD_NUM_COPY_LOG_TO_LAST_REPLICA=1
export THREAD_NUM_PERSIST_LOG=4
export THREAD_NUM_END_PIPELINE=2

# Parameters.
export REQUEST_RATE_LIMIT_THRESHOLD=500
export PREFETCH_DATA_CAP=2500	# Prefetch data cap in MB.

# For experiment
export DIGEST_NOOP=0	# Not supported in NIC-offloading setup.
export HYPERLOOP_OPS_FILE_PATH='/home/yulistic/assise-host-only/libfs/lib/hyperloop/trace/micro/latency/sw.1g.4K'
