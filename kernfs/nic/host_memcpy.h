/** Request host memcpy to host kernel worker. **/
#ifndef _NIC_HOST_MEMCPY_H_
#define _NIC_HOST_MEMCPY_H_

#include "global/global.h"
#include "concurrency/thpool.h"
#include "digest.h"
#include "loghdr.h"
#include "copy_log.h"
#include "distributed/replication.h"

struct host_memcpy_arg {
	uint64_t seqn;
	int libfs_id;
	memcpy_meta_array_t *array;
	struct logheader *loghdr_buf;
	char *log_buf; // to free.
	uint64_t *fetch_loghdr_done_p;
	uint64_t *fetch_log_done_p;
	atomic_bool processed; // 1 if this circ_buf item has been processed.
	// Pass to LibFS.
	addr_t digest_start_blknr;
	uint32_t n_orig_loghdrs;
	uint64_t n_orig_blks;
	int reset_meta;

#ifdef PROFILE_THPOOL
	struct timespec time_enqueued;
	struct timespec time_dequeued;
	struct timespec time_scheduled;
	int thread_id;
#endif
};
typedef struct host_memcpy_arg host_memcpy_arg;

struct pipeline_end_arg {
	int libfs_id;
	uint64_t seqn;
	struct logheader *loghdr_buf;
	char *log_buf;
	uint64_t *fetch_loghdr_done_p;
	uint64_t *fetch_log_done_p; // compress output_buf address is stored by Replica 1.
};
typedef struct pipeline_end_arg pipeline_end_arg;

typedef struct circ_buf_stat {
	unsigned long schedule_cnt;
	double wait_seqn_delay_sum;
	double wait_seqn_delay_max;
	double wait_seqn_delay_min;
} circ_buf_stat;

threadpool init_host_memcpy_thpool(int libfs_id);
void init_host_memcpy(void);
void enqueue_host_memcpy_req(host_memcpy_arg *hm_arg);
void enqueue_copy_done_item(copy_done_arg *cd_arg);
void request_host_memcpy(void *arg);
void request_host_memcpy_without_pipelining(host_memcpy_arg *hm_arg);
void print_host_memcpy_thpool_stat(void);
void register_host_memcpy_buf_addrs(uintptr_t buf_start_addr);
void publish_all_remains(struct replication_context *rctx);

static int send_memcpy_req_to_host(void *arg);
static void end_pipeline(void *arg);
// static void schedule_host_memcpy_threads(void *arg);
static int dequeue_host_memcpy_req(struct replication_context *rctx,
				   host_memcpy_arg *hm_arg);
static int dequeue_copy_done_item(struct replication_context *rctx);
static host_memcpy_arg *
search_next_seqn_host_memcpy_item(host_memcpy_circbuf *buffer,
				  uint64_t next_seqn);
static copy_done_arg *search_next_seqn_copy_done_item(copy_done_circbuf *buffer,
						      uint64_t next_seqn);
static void print_host_memcpy_arg(host_memcpy_arg *ar);
static void print_pipeline_end_arg(pipeline_end_arg *ar);
static void print_host_memcpy_circbuf(host_memcpy_circbuf *buffer, uint64_t next_seqn);
static void print_copy_done_circbuf(copy_done_circbuf *buffer, uint64_t next_seqn);
static void print_host_memcpy_stat(void *arg);
void print_pipeline_end_stat(void *arg);
static void init_cb_stats(circ_buf_stat *cb_stats);
static void print_circ_buf_stat(circ_buf_stat *cb_stat, char *cb_name);

// PRINT_ALL_PIPELINE_STAT_HEADER(thpool_pipeline_end)
PRINT_ALL_PIPELINE_STAT_HEADER(pipeline_end)
void print_all_thpool_host_memcpy_req_stats(void);
void print_all_circ_buf_stats(void);

#endif
