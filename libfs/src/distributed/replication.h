#ifndef _REPLICATION_H_
#define _REPLICATION_H_

#include "concurrency/synchronization.h"
#include "filesystem/shared.h"
#include "global/global.h"
#include "global/types.h"
#include "global/util.h"
#include "ds/list.h"
#include "ds/stdatomic.h"
#include "agent.h"
#include "ds/circ_buf.h"
#include "concurrency/thpool.h"

#ifdef NIC_SIDE
#define N_RSYNC_THREADS 1
#else
#define N_RSYNC_THREADS 3   // TODO JYKIM Need to increase it? (it was 5 before)
#endif

#ifdef KERNFS
extern struct replication_context *g_sync_ctx[MAX_LIBFS_PROCESSES + g_n_nodes];
#else
#ifdef NIC_OFFLOAD
extern struct replication_context *g_sync_ctx[g_n_nodes];
#else
extern struct replication_context *g_sync_ctx[g_n_replica];
#endif /* NIC_OFFLOAD */
#endif /* KERNFS */
extern int g_rsync_chunk;
extern int g_enable_rpersist;
extern int g_rsync_rf;

uint64_t *libfs_rate_limit_flag; // Used by libfs.

extern threadpool thread_pool_log_prefetch_req; // Used by libfs.

// circular msg buffer size (should be power of 2)
// TODO insufficient size of circular buffer results in severe performance degradation with CPU saturation.
// Better implementation would be to introduce semaphore eliminating busy-waiting on circbuf enqueueing.
// Note that currently, if circbuf is full, build_memcpy_list thread waits until enqueueing succeeds.
#define CIRC_BUF_HOST_MEMCPY_REQ_SIZE (10*1024*1024)
#define CIRC_BUF_COPY_DONE_SIZE (10*1024*1024)

//TODO: [refactor] use fs_log struct for peer (too many common elements between peer_metadata & fs_log).

// metadata for remote peers
typedef struct peer_metadata {
	int id;

	struct peer_id *info;

#ifdef HYPERLOOP
	// Hyperloop replicates following metadata.
	//  - local_start
	//  - start_version
	//  - avail_version
	// Let's make them contiguous so that they can be replicated by one
	// Hyperloop operation.

	// start addr for copying from local log (i.e. last blk synced + 1)
	addr_t local_start;
	uint32_t start_version;
	uint32_t avail_version;
#else
	// start addr for copying from local log (i.e. last blk synced + 1)
	addr_t local_start;
#endif

	// start addr for persisting at remote log (i.e. remote next_avail_header)
	addr_t remote_start;

	// last viable addr at remote log (before wrap around)
	addr_t remote_end;

	// start addr for digesting at remote log
	addr_t start_digest;

	// base addr for remote log
	addr_t base_addr;

#ifndef HYPERLOOP
	uint32_t start_version;

	uint32_t avail_version;
#endif

	// outstanding rsyncs.
	uint8_t outstanding;

	//pthread_t is unsigned long
	unsigned long replication_thread_id;

	// # of log block groups to digest at remote (used for convenience/sanity; can be replaced by n_used_blk)
	atomic_uint n_digest;

	// # of log headers that are pending transmission or inflight
	atomic_uint n_pending;

	// # of log block groups reserved at remote (used for convenience/sanity; can be replaced by n_used_blk)
	// atomic_uint n_used;     // TODO JYKIM Is it used?

	// # of blks reserved at remote log
	// atomic_ulong n_used_blk;    // TODO JYKIM Is it used?

	//1 if peer is currently digesting; 0 otherwise
	int digesting;

	// # of unsynced log block groups (used for convenience/sanity; can be replaced by n_unsync_blk)
	atomic_uint n_unsync;
	atomic_uint n_unsync_upto_end; /* Used in replication (NIC-offloading).
				        * It has a value if there is a wrap around.
				        * It is cleared after replication is done.
					* 0 if no wrap around occurs.
					*/
	// # of unsynced log blocks
	atomic_ulong n_unsync_blk;

	// used for concurrently updating *_start variables
	pthread_mutex_t *shared_rsync_addr_lock;

	// used for concurrently updating n_unsync_* variables
	pthread_mutex_t *shared_rsync_n_lock;

	// Used to prevent from updating n_unsync_blk before it is handled.
	pthread_mutex_t *n_unsync_blk_handle_lock;

	// used to enforce rsync order (acts as an optimization for coalescing; otherwise unused)
	pthread_mutex_t *shared_rsync_order;
	pthread_cond_t *shared_rsync_cond_wait;
	atomic_ulong cond_check;

	atomic_ulong recently_issued_seqn; // Used by LibFS.
	atomic_ulong recently_acked_seqn; // Used by LibFS.
} peer_meta_t;

// DRAM buffer meta in Smart NIC.
// Only nodes in the middle of a replication chain has it. (Ex. Only the second
// node out of three nodes.)
// Replication log is copied to this buffer in background until fsync is called.
// On fsync call, log data in this buffer is flushed to NVM of hosts following
// normal replication chain.
struct nic_log_buf_meta {
    union{
	uintptr_t peer_buf_base; // buffer address of the next node.
	char *base; // local buffer address.
    };
    union{
	uintptr_t peer_cur_addr; // address that the next data is sent to.
	char *cur; // local address that the next data is stored to.
    };
    char *next_start; // Save a local start address for the following
		      // replication request(repreq) when there is a wrap
		      // around. Multi-threading is not considered.

    // pthread_spinlock_t cur_lock;

    // We assume nic_log_buf size is as larger as the log_size. With this
    // assumption we don't need to consider rotation of nic_log_buf.
//    addr_t host_last_read_blknr; // lastly read block number.
//    addr_t host_end_blknr;  // The last block number of the current log. It has
			    // a value only when rotation occurs.
};

#ifdef PROFILE_THPOOL
struct rep_th_stat {
	unsigned long schedule_cnt;
	double schedule_delay_sum;
	double schedule_delay_max;
	double schedule_delay_min;
	double wait_seqn_delay_sum;
	double wait_seqn_delay_max;
	double wait_seqn_delay_min;
};
#endif

// struct of arg for workers.
struct rep_th_arg
{
    int libfs;
    int triggered_by_digest;
    uintptr_t base_addr;
    uint64_t n_unsync_blk;
    uint64_t n_blk_prefetch_requested;
    uint32_t n_unsync;
    uint64_t end;
    uint32_t n_blk;
    int inc_avail_ver;
    uint64_t seqn;
    int ack;
    int sockfd;
    uintptr_t is_meta_updated_p;
    uintptr_t is_ack_received;
#ifdef PROFILE_THPOOL
    struct timespec time_added;
    struct timespec time_dequeued;
    struct timespec time_scheduled;
    int rep_thread_id;
#endif
};

struct repmsg_circbuf {
	struct rpcmsg_replicate *buf;
	unsigned long head;
	unsigned long tail;
};

struct repreq_circbuf {
	struct rep_th_arg *buf;
	unsigned long head;
	unsigned long tail;
};

typedef struct host_memcpy_arg host_memcpy_arg_; // To avoid including
						 // nic/host_memcpy.h header
						 // file.
struct host_memcpy_circbuf {
	host_memcpy_arg_ *buf;
	unsigned long head;
	unsigned long tail;
	pthread_mutex_t produce_mutex;
	pthread_mutex_t consume_mutex;
};
typedef struct host_memcpy_circbuf host_memcpy_circbuf;

typedef struct copy_done_arg copy_done_arg_; // To avoid including
					     // nic/host_memcpy.h header file.
struct copy_done_circbuf {
	copy_done_arg_ *buf;
	unsigned long head;
	unsigned long tail;
	pthread_mutex_t produce_mutex;
	pthread_mutex_t consume_mutex;
};
typedef struct copy_done_circbuf copy_done_circbuf;

struct memcpy_list_batch_meta {
	rdma_meta_t *r_meta;
	uint64_t first_seqn; // seqn of the first entry.
	int memcpy_list_buf_id; // Accessed by single thread.
	addr_t digest_start_blknr;
	uint32_t n_orig_loghdrs;
	uint64_t n_orig_blks;
	int reset_meta;
};
typedef struct memcpy_list_batch_meta memcpy_list_batch_meta_t;

// replication globals
struct replication_context {
	struct peer_id *self;
	addr_t begin;   // log start block number
	addr_t size;    // begin + g_log_size -1 (=end)
	atomic_ulong *end; /* last blknr before wrap around
			    * It points to g_log_sb[]->end.
			    * It is used for digestion.
			    * 0 if there is no wrap around.
			    */
	addr_t base_addr;

	// remote peer metadata; can add as many elements
	// as necessary in case of multiple replicas
	peer_meta_t *peer;

        int next_rep_data_sockfd[2]; /* Sockfd to send log data by RDMA.
                                      * Used in NIC-offloading. In the second
                                      * replica, log data is copied to NVM of
                                      * local host[0] and the next host[1]
                                      * simultaneously.  W/o offloading, it is
                                      * identical to peer->info->sockfd[SOCK_IO].
                                      */

	int next_rep_msg_sockfd[2]; /* sockfd to send replication rpc message.
				     * Used in NIC-offloading. If it is the
				     * second replica or the next replica is the
				     * last one, msg is delivered to libfs[0]
				     * and NIC kernfs of the next replica[1]
				     * simultaneously. W/o offlloading, it is
				     * identical to peer->info->sockfd[SOCK_IO].
				     */

	int next_digest_sockfd; /* sockfd to send digest rpc message.
				 * Used in NIC-offloading. W/o offlloading, it is
				 * identical to peer->info->sockfd[SOCK_BG].
				 */

	int next_loghdr_sockfd; /* sockfd to send log headers and rpc message.
				 * Used in NIC kernfs.
				 */

	//////////////////////////////////////////////
	/////////////// NIC-offloading ///////////////
	//////////////////////////////////////////////
	struct replication_context *host_rctx;	/* Used for replication to host
						 * NVM on the second replica.
						 */
	uintptr_t libfs_rate_limit_addr;

	threadpool thpool_build_memcpy_list;

	host_memcpy_circbuf host_memcpy_req_buf;
	atomic_ulong next_host_memcpy_seqn; // To make request_memcopy tasks serial.
	atomic_ulong next_copy_done_seqn; // To make copy_done serial.
	threadpool thpool_host_memcpy_req;
	copy_done_circbuf local_copy_done_buf; // Copy to local NVM done.

	threadpool thpool_manage_fsync_ack;
	DECLARE_BITMAP(log_persisted_bitmap, COPY_NEXT_BITMAP_SIZE_IN_BIT); // 1M bits. Set bitmap on the finish of copying log to the last replica.
	uint64_t log_persisted_bitmap_start_seqn; // seqn mapped to the first bitmap entry.
	pthread_spinlock_t log_persisted_bitmap_lock;

	memcpy_list_batch_meta_t mcpy_list_batch_meta;
};

// ephemeral sync metadata
struct rdma_meta_entry {
	int local_mr;
	int remote_mr;
	rdma_meta_t *meta;
	struct list_head head;
        int cur_sg;
#ifdef DIGEST_OPT_MERGE_WRITES
        int buf_pos;
        int buf_blk_cnt;
#endif
};

struct sync_list {
	uint32_t count;
	addr_t n_blk;
	addr_t remote_start;
	struct list_head head;
};

typedef struct sync_interval {
	addr_t start;
	addr_t end;
	struct list_head head;
} sync_interval_t;

typedef struct sync_meta {
	int id;
	addr_t local_start;
	addr_t remote_start;
	addr_t remote_end;
	addr_t peer_base_addr;
	addr_t end; //snapshot of last blknr before wrap around
	uint32_t n_unsync; //# of unsynced log hdrs
	uint32_t n_tosync; //# of log hdrs to sync (after coalescing)
	addr_t n_unsync_blk; //# of unsynced log blks
	addr_t n_tosync_blk; //# of log blks to sync (after coalescing)
	uint32_t n_digest;
	struct sync_list *intervals;
	struct list_head rdma_entries;

} sync_meta_t;

/**
 * Common fields of rpc message.
 */
struct rpcmsg_common {
    uint64_t seqn;
};


/*
 * |libfs|
 *    ↓
 * (repreq)
 *    ↓
 * |NIC-kernfs| --(repmsg)-> |NIC-kernfs| --(repmsg)-> |NIC-kernfs| ...
 */
// rpcmsg to relay replication (repmsg)
struct rpcmsg_replicate {
    struct rpcmsg_common common;
    int libfs_id;         // original requester id.
    uint64_t n_log_blks;  // # of log blocks replicated.
    uint32_t n_log_hdrs;  // # of unsynced block groups (loghdrs) Only used in
                          // NIC-offloading.
    int ack;            /* Sends ACK to libfs if ack == 1.
                         * Only used in NIC-offloading.
                         */
    int steps;          // steps in chain.
    int persist;        // persist to NVM.
    bool is_imm;        // Use imm instead of rpc message.
    int inc_avail_ver;  // Increase peer->avail_version if 1.
    addr_t end_blknr;   // The last blknr of the current version of log. Used in
                        // non-offloading case and mlfs_do_rsync_forward() in
                        // nic-offload case.
    uintptr_t ack_bit_p; // Address of ack bit. Set by RDMA WRITE from remote node.
    atomic_bool processed;	// Used in m-to-n circular buffer
#ifdef PROFILE_THPOOL
    struct timespec time_added;
    struct timespec time_dequeued;
    struct timespec time_scheduled;
    int rep_thread_id;
#endif
};

// rpcmsg: replication request from libfs to local NIC kernfs (repreq)
struct rpcmsg_repreq {
    int libfs_id;		// My libfs id
    int triggered_by_digest;	// used for latency breakdown
    uint64_t base_addr;		// base address of log in libfs
    uint64_t n_unsync_blk;	// # of unsynced blocks
    uint64_t n_blk_prefetch_requested;	// # of blocks prefetched.
    uint32_t n_unsync;		// # of unsynced block groups (loghdrs)
    addr_t end_blknr;		// end block number of current version
    uint32_t n_blk;		// # of blocks (used only in async call)
    int inc_avail_ver;		// Increase avail version number if 1
    int ack;			// Send ack if 1
};

struct fsync_acks {
	uint8_t replica1;
	uint8_t replica2;
};

typedef void(*conn_cb_fn)(char* ip);
typedef void(*disc_cb_fn)(char* ip);
typedef void (*signal_cb_fn)(struct app_context *);

#define next_loghdr(x,y) next_loghdr_blknr(x,y->nr_log_blocks,g_sync_ctx[session->id]->begin,session->end)

static inline void set_peer_digesting(peer_meta_t *peer)
{
	while (1) {
		//if (!xchg_8(&g_fs_log->digesting, 1)) 
		if (!cmpxchg(&peer->digesting, 0, 1)) 
			return;

		while (peer->digesting) 
			cpu_relax();
	}
}

static inline void clear_peer_digesting(peer_meta_t *peer)
{
	//while (1) {
		//if (!xchg_8(&g_fs_log->digesting, 1)) 
		if (cmpxchg(&peer->digesting, 1, 0)) 
			return;

		//while (peer->digesting) 
		//	cpu_relax();
	//}
}

void init_replication(int remote_log_id, struct peer_id *peer, addr_t begin,
                      addr_t size, addr_t addr, atomic_ulong *end,
                      int next_rep_data_sockfd[], int next_rep_msg_sockfd[],
		      int next_digest_sockfd, int next_loghdr_sockfd);
void destroy_replication(struct replication_context *rctx);
int make_replication_request_async(uint32_t n_blk);
int make_replication_request_sync(peer_meta_t *peer, uintptr_t local_base_addr,
                                  uintptr_t peer_base_addr, uint32_t n_blk,
                                  int inc_avail_ver, uint64_t seqn, int ack,
                                  uintptr_t is_meta_update_p,
                                  uintptr_t is_ack_received);
int mlfs_do_rsync_forward(struct rpcmsg_replicate *rep);
uint32_t create_rsync(struct replication_context *rctx,
                      struct list_head **rdma_entries, uint32_t n_blk,
                      int force_digest, uintptr_t local_base_addr,
                      uintptr_t peer_base_addr, uint64_t *n_unsync_blk,
                      uint32_t *n_unsync);
void update_peer_sync_state(peer_meta_t *peer, uint64_t nr_log_blocks, uint32_t nr_unsync);
void update_peer_digest_state(peer_meta_t *peer, addr_t start_digest, int n_digested, int rotated);
void wait_on_peer_digesting(peer_meta_t *peer);
void wait_on_peer_replicating(peer_meta_t *peer);
void wait_till_digest_checkpoint(peer_meta_t *peer, int version, addr_t block_nr);

uint32_t addr_to_logblk(int id, addr_t addr);
uint32_t generate_rsync_metadata(int id, uint16_t seq_n, addr_t size, int solicit_ack);
uint32_t generate_rsync_metadata_from_rpcmsg(struct rpcmsg_replicate *rep);
uint32_t next_rsync_metadata(uint32_t meta);
int decode_rsync_metadata(uint32_t meta, uint16_t *seq_n, addr_t *n_log_blk, int *steps,
		int *requester_id, int *sync, int *persist);
// int get_replica_num(void);
peer_meta_t* get_next_peer();
static int start_rsync_session(struct replication_context *rctx, uint32_t n_blk,
                               uintptr_t peer_base_addr);
void end_rsync_session(peer_meta_t *peer);
static void generate_rsync_msgs(struct replication_context *rctx,
                                addr_t sync_size, int loop_mode,
                                uintptr_t local_base_addr);
static void generate_rsync_msgs_using_replay(struct replication_context *rctx,
                                             struct replay_list *replay_list,
                                             int destroy,
                                             uintptr_t local_base_addr);
static addr_t coalesce_log_and_produce_replay(struct replay_list *replay_list, uintptr_t local_base_addr);
static void coalesce_log_entry(addr_t loghdr_blkno, loghdr_t *loghdr, struct replay_list *replay_list);
void move_log_ptr_without_session(int id, addr_t *log_ptr, addr_t lag, addr_t size,
	uintptr_t base_addr, addr_t remaining_blks);
void move_log_ptr(struct replication_context *rctx, addr_t *start,
                  addr_t addr_trans, addr_t size, uintptr_t base_addr);
void reset_log_ptr(struct replication_context *rctx, addr_t *log_ptr);
static void rdma_add_sge(struct replication_context *rctx, addr_t size);
static void rdma_finalize(struct replication_context *rctx,
                          uintptr_t local_base_addr);
addr_t nr_blks_between_ptrs(struct replication_context *rctx, addr_t start,
                            addr_t target, uintptr_t base_addr);
static addr_t nr_blks_before_wrap(struct replication_context *rctx,
                                  addr_t start, addr_t addr_trans, addr_t size,
                                  uintptr_t base_addr);

// NIC_OFFLOAD
int calculate_prefetch_meta(addr_t prefetch_start_blknr,
			     uint64_t n_unsync_blks, uint32_t n_unsync,
			     uint32_t n_unsync_upto_end, addr_t end_blknr,
			     int is_fsync);
static void send_log_prefetch_request(addr_t prefetch_start_blknr,
				      uint64_t n_prefetch_blk,
				      uint32_t n_prefetch, int reset_meta,
				      int is_fsync, uint64_t *fsync_ack_flag);
// print
static void print_peer_meta_struct(peer_meta_t *meta);
static void print_sync_meta_struct(sync_meta_t *meta);
static void print_sync_intervals_list(sync_meta_t *meta);
void print_sync_ctx(void);

#define PR_METADATA(x) print_meta(x, __func__, __LINE__)
void print_meta(struct replication_context *sync_ctx, char const * caller_name, int line);

#endif
