/** Copy log data in Replica 1 (Middle of the replication chain.) **/
#ifndef _NIC_COPY_LOG_H_
#define _NIC_COPY_LOG_H_
#include "global/global.h"
#include "global/types.h"
#include "global/defs.h"
#include "pipeline_common.h"
#include "concurrency/thpool.h"

struct log_copy_to_local_nvm_arg {
	struct replication_context *rctx;
	int libfs_id;
	uint64_t seqn;
	char *log_buf;
	uint64_t log_size;
	uint64_t orig_log_size; // For compression.
	addr_t start_blknr;
	atomic_bool *copy_log_done_p;
	int fsync; // Required for compression.
};
typedef struct log_copy_to_local_nvm_arg copy_to_local_nvm_arg;

struct log_copy_to_last_replica_arg {
	struct replication_context *rctx;
	int libfs_id;
	uint64_t seqn;
	char *log_buf;
	uint64_t log_size;
	uint64_t orig_log_size;
	addr_t start_blknr;
	atomic_bool *copy_log_done_p;
	// fsync related.
	int fsync;
	uintptr_t fsync_ack_addr;
};
typedef struct log_copy_to_last_replica_arg copy_to_last_replica_arg;

struct log_buf_free_arg {
	int libfs_id;
	uint64_t seqn;
	char *log_buf;
	atomic_bool *copy_log_done_p;
};
typedef struct log_buf_free_arg log_buf_free_arg;

struct copy_done_arg {
	int libfs_id;
	uint64_t seqn;
	atomic_bool processed; // 1 if this circ_buf item has been processed.

	// Used for compression(COMPRESS_LOG).
	addr_t start_blknr;
	uint64_t log_size;
	uint64_t orig_log_size;
	int fsync;

#ifdef PROFILE_THPOOL
	struct timespec time_enqueued;
	struct timespec time_dequeued;
	struct timespec time_scheduled;
	int thread_id;
#endif
};
typedef struct copy_done_arg copy_done_arg;

struct manage_fsync_ack_arg {
	int libfs_id;
	uint64_t seqn;
	int fsync;
	uintptr_t fsync_ack_addr;
};
typedef struct manage_fsync_ack_arg manage_fsync_ack_arg;

threadpool init_log_copy_to_local_nvm_thpool(void);
threadpool init_log_copy_to_last_replica_thpool(void);
threadpool init_log_buf_free_thpool(void);
threadpool init_copy_done_ack_handle_thpool(void);
threadpool init_manage_fsync_ack_thpool(int libfs_id);

void copy_log_to_local_nvm_bg(void *arg);
void copy_log_to_last_replica_bg(void *arg);
void handle_copy_done_ack(void *arg);
void print_copy_log_thpool_stat(void);
void print_all_thpool_manage_fsync_ack_stats(void);

static void free_log_buf(void *arg);
static void send_persist_log_req_to_last_replica_host(
	int libfs_id, uint64_t seqn, int sockfd, addr_t log_area_begin_blknr,
	addr_t log_area_end_blknr, addr_t start_blknr, uint64_t n_log_blks,
	uintptr_t fsync_ack_addr);
static void send_copy_done_ack_to_last_replica(int libfs_id, uint64_t seqn,
					       addr_t start_blknr,
					       uint64_t log_size,
					       uint64_t orig_log_size,
					       int fsync, int sockfd);
static void manage_fsync_ack (void *arg);

static void print_copy_to_local_nvm_arg(copy_to_local_nvm_arg *ar);
static void print_copy_to_last_replica_arg(copy_to_last_replica_arg *ar);
static void print_log_buf_free_arg(log_buf_free_arg *ar);
static void print_copy_done_arg(copy_done_arg *ar);
void print_copy_to_local_nvm_stat(void *arg);
void print_copy_to_last_replica_stat(void *arg);
void print_free_log_buf_stat(void *arg);
void print_handle_copy_done_ack_stat(void *arg);

PRINT_ALL_PIPELINE_STAT_HEADER(copy_to_local_nvm)
PRINT_ALL_PIPELINE_STAT_HEADER(copy_to_last_replica)
PRINT_ALL_PIPELINE_STAT_HEADER(free_log_buf)
PRINT_ALL_PIPELINE_STAT_HEADER(handle_copy_done_ack)
#endif
