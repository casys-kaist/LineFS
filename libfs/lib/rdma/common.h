#ifndef RDMA_COMMON_H
#define RDMA_COMMON_H

#include <rdma/rdma_cma.h>
#include <semaphore.h>
#include "globals.h"
#include "uthash.h"


//---------device metadata
struct context {
	struct ibv_context *ctx;
	struct ibv_pd *pd;
	struct id_record *id_by_addr; //addr to id hashmap
	struct id_record *id_by_qp; //qp to id hashmap
};

//---------connection metadata
struct conn_context
{
	//unique connection (socket) descriptor
	int sockfd;

	// socket type. ex) SOCK_IO, SOCK_BG, ...
	int sock_type;

	//connection state
	int state;

	//app identifier
	int app_type;

	//completion queue
	struct ibv_cq *cq;

	//completion channel
	struct ibv_comp_channel *comp_channel;

	//background cq polling thread
	pthread_t cq_poller_thread;

	//polling mode: 1 means cq thread is always active, 0 means only during bootstrap
	int poll_always;

	//enables or disables background polling thread
	int poll_enable;

	//provides completion poll permission (in case of multiple threads)
	int poll_permission;

	//registered memory regions
	struct ibv_mr **local_mr;
	struct mr_context **remote_mr;

	//checks whether mr init message have been sent/recv'd
	int mr_init_sent;
	int mr_init_recv;

	//bootstrap flags (signifies whether access permissions are available for an mr)
	int *local_mr_ready;
	int *remote_mr_ready;
	int *local_mr_sent;

	//total number of remote MRs
	int remote_mr_total;

	//idx of local_mr to be sent next;
	int local_mr_to_sync;

	//send/rcv buffers
	struct ibv_mr **msg_send_mr;
	struct ibv_mr **msg_rcv_mr;
	struct message **msg_send;
	struct message **msg_rcv;

	//determines acquisitions of send buffers
	int *send_slots;
	uint8_t send_idx;

#ifdef MSG_BUFFER_PROFILE
	uint64_t rcv_buf_posted_cnt;
	uint64_t send_buf_posted_cnt;
#endif
#ifdef PRINT_PREV_MSG
	// Stores previous app data.
	char *prev_app_data;
#endif

	//connection id
	struct rdma_cm_id *id;

	//locking and synchronization
	uint64_t last_send;
	uint64_t last_send_compl;
	uint64_t last_rcv;
	uint64_t last_rcv_compl;
	uint64_t per_libfs_last_rcv_compl[RDMALIB_SYNC_CTX_LEN];
	uint64_t last_msg; //used to ensure no conflicts when writing on msg send buffer

	pthread_mutex_t wr_lock;
	pthread_cond_t wr_completed;
	pthread_spinlock_t post_lock; //ensures that rdma ops on the same socket have monotonically increasing wr id

	struct app_response *pendings; //hashmap of pending application responses (used exclusively by application)
	struct buffer_record *buffer_bindings; //hashmap of send buffer ownership per wr_id
	pthread_spinlock_t buffer_lock; //concurrent access to buffer hashtable
	pthread_spinlock_t acquire_buf_lock;

	UT_hash_handle qp_hh;

	// Synchronization between event_loop() and add_connection().
	sem_t conn_sem;

#ifdef PRINT_ASYNC_EVENT
	pthread_t async_event_polling_thread;
#endif
};

//--------memory region metadta
struct mr_context
{
	//type enum
	int type;

	// sock type enum. ex) SOCK_IO, SOCK_BG, ...
	int sock_type;

	//start address
	addr_t addr;

	//length
	addr_t length;

	//access keys
	uint32_t lkey;
	uint32_t rkey;
};

//---------rdma operation metadata
typedef struct rdma_metadata {
	int op;
	uint32_t wr_id;
	addr_t addr;
	addr_t length;
	uint32_t imm;
	int sge_count;
	struct rdma_metadata *next;
	struct ibv_sge sge_entries[];
} rdma_meta_t;

//---------user-level metadata
//msg payload
struct app_context {
	int sockfd;  //socket id on which the msg came on
	int libfs_id; // Used for per libfs seqn. 0 means no libfs is related.
	uint64_t id; //can be used as an app-specific request-response matcher (seqn)
	char* data;  //convenience pointer to data blocks
};

//msg response tracker
struct app_response { //used to keep track of pending responses
	uint32_t id;
	int ready;
	UT_hash_handle hh;
};

#endif
