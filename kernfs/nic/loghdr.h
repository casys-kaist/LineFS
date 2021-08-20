/** Build and copy loghdr list. **/
#ifndef _NIC_LOGHDR_H_
#define _NIC_LOGHDR_H_
#include "global/global.h"
#include "concurrency/thpool.h"
#include "distributed/replication.h"
#include "pipeline_common.h"

struct build_loghdrs_arg {
	uint64_t seqn;
	struct replication_context *rctx;
	char *log_buf;
	uint64_t log_size;
	uint32_t n_loghdrs;
	addr_t fetch_start_blknr; // used in publishing.
	uint64_t *fetch_log_done_p;
	// Pass to LibFS
	uint32_t n_orig_loghdrs;
	uint64_t n_orig_blks;
	int reset_meta;
};
typedef struct build_loghdrs_arg build_loghdrs_arg;

struct fetch_loghdrs_arg {
	int libfs_id;
	uint64_t seqn;
	uint32_t n_loghdrs;
	uint64_t digest_blk_cnt;
	uintptr_t remote_loghdr_buf;
	uintptr_t fetch_loghdr_done_addr; // Remote address.
	addr_t fetch_start_blknr; // used in publishing.
};
typedef struct fetch_loghdrs_arg fetch_loghdrs_arg;

threadpool init_loghdr_build_thpool(void);
threadpool init_loghdr_fetch_thpool(void);

void build_loghdr_list(void *arg);
void fetch_loghdrs(void *arg);
void print_loghdr_thpool_stat(void);
static void send_fetch_loghdr_rpc_to_next_replica(
	struct replication_context *rctx, uint64_t seqn, uintptr_t loghdr_buf,
	uint32_t n_loghdrs, uint64_t digest_blk_cnt, addr_t fetch_start_blknr,
	uintptr_t fetch_loghdr_done_addr);
static void print_build_loghdrs_arg(build_loghdrs_arg *ar);
static void print_fetch_loghdrs_arg(fetch_loghdrs_arg *ar);
void print_loghdr_build_stat(void *arg);
void print_loghdr_fetch_stat(void *arg);

PRINT_ALL_PIPELINE_STAT_HEADER(loghdr_build)
PRINT_ALL_PIPELINE_STAT_HEADER(loghdr_fetch)
#endif
