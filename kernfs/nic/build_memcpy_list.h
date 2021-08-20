/** Build host memcpy list. **/
#ifndef _NIC_PUBLISHER_H_
#define _NIC_PUBLISHER_H_
#include "global/global.h"
#include "concurrency/thpool.h"
#include "filesystem/shared.h"
#include "loghdr.h"

// #define MAX_NUM_LOGHDRS_IN_LOG (g_log_size / 2) // Worst case.

struct build_memcpy_list_arg {
	uint64_t seqn;
	struct replication_context *rctx;
	uint64_t digest_blk_cnt;
	uint32_t n_loghdrs;
	addr_t fetch_start_blknr; // for print info.
	struct logheader *loghdr_buf;
	char *log_buf; // to free.
	uint64_t *fetch_loghdr_done_p;
	uint64_t *fetch_log_done_p;
	// Pass to LibFS
	uint32_t n_orig_loghdrs;
	uint64_t n_orig_blks;
	int reset_meta;
};
typedef struct build_memcpy_list_arg build_memcpy_list_arg;

threadpool init_build_memcpy_list_thpool(int libfs_id);
void build_memcpy_list(void *arg);
void print_build_memcpy_list_thpool_stat(void);
void print_all_thpool_build_memcpy_list_stats(void);

static void print_build_memcpy_list_arg(build_memcpy_list_arg *ar);
static void print_build_memcpy_list_stat(void *arg);

#endif
