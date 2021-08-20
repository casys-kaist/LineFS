#ifndef RDMA_GLOBALS_H
#define RDMA_GLOBALS_H

#include <stdio.h>
#include <inttypes.h>

#define MAX_CONNECTIONS 1000 // max # of RDMA connections per peer
#define MAX_MR 10 // max # of memory regions per connection
#define MAX_PENDING 500 // max # of pending app responses per connection
#define MAX_BUFFER 50 // max # of msg buffers per connection TODO check with smaller size. (Original was 5)
// #define MAX_SEND_QUEUE_SIZE 1024 // depth of rdma send queue
// #define MAX_RECV_QUEUE_SIZE 1024 // depth of rdma recv queue
#define MAX_SEND_QUEUE_SIZE 50 // depth of rdma send queue
#define MAX_RECV_QUEUE_SIZE 50 // depth of rdma recv queue
// #define MAX_SEND_QUEUE_SIZE 32768 // depth of rdma send queue
// #define MAX_RECV_QUEUE_SIZE 32768 // depth of rdma recv queue
#define MAX_SEND_SGE_SIZE 30 // Max number of send SGE (Ref. ibv_devinfo -v) # It should coincide with the constant in libfs/src/global/global.h
#define MAX_RECV_SGE_SIZE 30 // Max number of recv SGE
#define MAX_CQE 32768   // Max number of CQ entry. 32768 is arbitrary. Just enough value.

#define RDMALIB_MAX_LIBFS_PROCESSES 12	// It should be the same as (or larger than)
				// MAX_LIBFS_PROCESSES in libfs/src/global/global.h
#define RDMALIB_N_NODES 6 // host kernfs(3) + nic kernfs(3)
#define RDMALIB_SYNC_CTX_LEN (RDMALIB_MAX_LIBFS_PROCESSES + RDMALIB_N_NODES)

#define N_LOW_LAT_POLLING_THREAD 1

// max # of rdma operations that can be batched together
// must be < MAX_SEND_QUEUE_SIZE
#define MAX_BATCH_SIZE 50

typedef uint64_t addr_t;

#endif
