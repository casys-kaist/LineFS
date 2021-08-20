/** Fetch and copy log data. **/
#ifndef _NIC_LOG_FETCHER_H_
#define _NIC_LOG_FETCHER_H_
#include "global/global.h"
#include "global/types.h"
#include "global/defs.h"
#include "pipeline_common.h"
#include "ds/list.h"
#include "concurrency/thpool.h"

/* Thread argument struct */
// Fetching log from primary NVM.
struct log_fetch_from_local_nvm_arg {
	int libfs_id;
	uint64_t seqn;
	addr_t prefetch_start_blknr;
	uint32_t n_to_prefetch_loghdr;
	uint64_t n_to_prefetch_blk;
	uint64_t libfs_base_addr; // base address of log in libfs
	// Pass to LibFS
	int reset_meta;
	// fsync related.
	int fsync;
	uintptr_t fsync_ack_addr;
};
typedef struct log_fetch_from_local_nvm_arg log_fetch_from_local_nvm_arg;

struct log_fetch_from_nic_arg {
	int libfs_id;
	uint64_t seqn;
	uintptr_t remote_log_buf_addr; // Primary NIC's log buffer address.
	uint64_t log_size;
	uint64_t orig_log_size; // Different from log_size when data is compressed.
	uintptr_t fetch_log_done_addr; // To release Primary NIC's log buffer.
	addr_t start_blknr;
	// fsync related.
	int fsync;
	uintptr_t fsync_ack_addr;
};
typedef struct log_fetch_from_nic_arg log_fetch_from_nic_arg;

threadpool init_log_fetch_from_local_nvm_thpool(void);
threadpool init_log_fetch_from_primary_nic_dram_thpool(void);
threadpool init_fsync_thpool(void);

// deprecated.
//uint64_t read_log(int libfs_id, uintptr_t local_addr,
//		  uintptr_t remote_base_addr, addr_t start_blknr,
//		  addr_t end_blknr, int sock_fd);
void fetch_log(void *arg);

// Used by primary.
void fetch_log_from_local_nvm_bg(void *arg);
// void fetch_log_from_local_nvm_sync(void *arg);

// Used by Replica 1.
void fetch_log_from_primary_nic_dram_bg(void *arg);
// void fetch_log_from_primary_nic_dram_sync(void *arg);

// Print
void print_fetch_log_thpool_stat(void);

static void print_log_fetch_from_local_nvm_arg(log_fetch_from_local_nvm_arg *ar,
					       const char *caller);
static void print_log_fetch_from_nic_arg(log_fetch_from_nic_arg *ar,
					 const char *caller);

void print_log_fetch_from_local_nvm_stat(void *arg);
void print_log_fetch_from_primary_nic_dram_stat(void *arg);

/**
 * Macros for the following headers.
 *
 * void print_all_thpool_log_fetch_from_local_nvm_stats(void);
 * void print_all_thpool_log_fetch_from_primary_nic_stats(void);
 **/
PRINT_ALL_PIPELINE_STAT_HEADER(log_fetch_from_local_nvm)
PRINT_ALL_PIPELINE_STAT_HEADER(log_fetch_from_primary_nic_dram)
#endif
