/** Pipeline common functions. */
#ifndef _NIC_PIPELINE_COMMON_H_
#define _NIC_PIPELINE_COMMON_H_
#include "global/global.h"
#include "global/types.h"
#include "storage/storage.h" // nic_slab
#include "distributed/rpc_interface.h"

#ifdef PROFILE_PIPELINE
#define PRINT_ALL_PIPELINE_STAT_HEADER(stage)                                  \
	void print_all_thpool_##stage##_stats(int th_num);

#define PRINT_ALL_PIPELINE_STAT_FUNC(stage)                                    \
	void print_all_thpool_##stage##_stats(int th_num)                      \
	{                                                                      \
		for (uint64_t i = 0; i < th_num; i++) {                        \
			thpool_add_work(thpool_##stage, print_##stage##_stat,  \
					(void *)i);                            \
		}                                                              \
		usleep(100000);                                                 \
	}
// Sleep to print in order.

#define PRINT_ALL_PIPELINE_STATS(stage, th_num)                                \
	do {                                                                   \
		print_all_thpool_##stage##_stats(th_num);                      \
	} while (0)
#else
#define PRINT_ALL_PIPELINE_STAT_HEADER(thpool)
#define PRINT_ALL_PIPELINE_STAT_FUNC(thpool)
#define PRINT_ALL_PIPELINE_STATS(thpool)
#endif

extern uint64_t *true_bit;
extern uint64_t *false_bit;

struct host_heartbeat {
	int alive;
	uint64_t seqn;
	struct timespec last_recv_time;
};
typedef struct host_heartbeat host_heartbeat_t;

extern host_heartbeat_t g_host_heartbeat;

struct common_arg {

};

//struct publish_arg {
//	/* log fetch_from_local_nvm_arg */
//	int libfs_id;
//	uint64_t seqn;
//	addr_t prefetch_start_blknr;
//	uint32_t n_to_prefetch_loghdr;
//	uint64_t n_to_prefetch_blk;
//	uint64_t libfs_base_addr; // base address of log in libfs
//	// Pass to LibFS
//	int reset_meta;
//	// fsync related.
//	int fsync;
//	uintptr_t fsync_ack_addr;
//
//
//	/* coalesce */
//	// uint64_t seqn;
//	struct replication_context *rctx;
//	char *log_buf;
//	uint64_t log_size;
//	uint32_t n_loghdrs;
//	addr_t fetch_start_blknr; // used in building memcpy list.
//	// Pass to LibFS
//	int reset_meta;
//	// fsync related.
//	int fsync;
//	uintptr_t fsync_ack_addr;
//
//	/* build loghdr */
//	uint64_t seqn;
//	struct replication_context *rctx;
//	char *log_buf;
//	uint64_t log_size;
//	uint32_t n_loghdrs;
//	addr_t fetch_start_blknr; // used in publishing.
//	uint64_t *fetch_log_done_p;
//	// Pass to LibFS
//	uint32_t n_orig_loghdrs;
//	uint64_t n_orig_blks;
//	int reset_meta;
//
//	/* host memcpy */
//	uint64_t seqn;
//	int libfs_id;
//	memcpy_meta_array_t *array;
//	struct logheader *loghdr_buf;
//	char *log_buf; // to free.
//	uint64_t *fetch_loghdr_done_p;
//	uint64_t *fetch_log_done_p;
//	atomic_bool processed; // 1 if this circ_buf item has been processed.
//	// Pass to LibFS.
//	addr_t digest_start_blknr;
//	uint32_t n_orig_loghdrs;
//	uint64_t n_orig_blks;
//	int reset_meta;
//#ifdef PROFILE_THPOOL
//	struct timespec time_enqueued;
//	struct timespec time_dequeued;
//	struct timespec time_scheduled;
//	int thread_id;
//#endif
//
//};

struct replicate_arg{

};

//struct pipeline_arg {
//	// fetch_log
//
//	// coalesce
//
//};
//typedef struct pipeline_arg pipe_arg;

static inline int host_alive(void)
{
	return g_host_heartbeat.alive;
}

void init_pipeline_common(void);
void set_fetch_done_flag(int libfs_id, uint64_t seqn, uintptr_t fetch_done_addr,
			 int sockfd);
void set_fetch_done_flag_to_val(int libfs_id, uint64_t seqn,
				 uintptr_t fetch_done_addr, uint64_t val,
				 int sockfd);
void print_thread_init(int th_num, char *th_name);
void update_host_heartbeat(uint64_t arrived_seqn);
void check_heartbeat_worker(void *arg);
#endif
