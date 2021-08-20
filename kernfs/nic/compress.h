#ifndef _NIC_COMPRESS_H_
#define _NIC_COMPRESS_H_

#include "global/global.h"
#include "concurrency/thpool.h"
#include "distributed/replication.h"
#include "pipeline_common.h"

struct compress_arg{
	struct replication_context *rctx;
	uint64_t seqn;
	char *log_buf;
	uint64_t log_size;
	addr_t fetch_start_blknr;
	uint64_t *fetch_log_done_p;
	// fsync related.
	int fsync;
	uintptr_t fsync_ack_addr;
};
typedef struct compress_arg compress_arg;

threadpool init_compress_thpool(void);

void compress_log_bg(void *arg);
void print_compress_thpool_stat(void);
void decompress_log(char *output_buf, char *input_buf, uint64_t compressed_size,
		    uint64_t original_size);

static void send_fetch_log_rpc_to_next_replica(
	struct replication_context *rctx, uint64_t seqn, uintptr_t log_buf_addr,
	uint64_t log_size, uint64_t orig_size, addr_t start_blknr,
	uintptr_t fetch_log_done_addr, int fsync, uintptr_t fsync_ack_addr,
	int sockfd);
static void print_compress_arg(compress_arg *ar);
void print_compress_stat(void *arg);

PRINT_ALL_PIPELINE_STAT_HEADER(compress)
#endif
