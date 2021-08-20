/** Do log validation and log coalescing. **/
#ifndef _NIC_COALESCE_H_
#define _NIC_COALESCE_H_

#include "global/global.h"
#include "concurrency/thpool.h"
#include "pipeline_common.h"

struct coalesce_arg{
	uint64_t seqn;
	struct replication_context *rctx;
	char *log_buf;
	uint64_t log_size;
	uint32_t n_loghdrs;
	addr_t fetch_start_blknr; // used in building memcpy list.
	// Pass to LibFS
	int reset_meta;
	// fsync related.
	int fsync;
	uintptr_t fsync_ack_addr;
};
typedef struct coalesce_arg coalesce_arg;

threadpool init_coalesce_thpool(void);
void coalesce_log(void *arg);
void print_coalesce_thpool_stat(void);

static void print_coalesce_arg (coalesce_arg *ar);
void print_coalesce_stat(void *arg);

PRINT_ALL_PIPELINE_STAT_HEADER(coalesce)

#endif
