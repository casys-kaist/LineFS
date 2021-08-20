#include "mlfs/mlfs_user.h"
#include "fetch_log.h"
#include "distributed/rpc_interface.h"
#include "storage/storage.h" // for nic_slab
#include "coalesce.h" // thpool_next_stage
#include "copy_log.h"
#include "pipeline_common.h"
#include "global/util.h"
#include "nic/limit_rate.h"

threadpool thpool_log_fetch_from_local_nvm;
threadpool thpool_log_fetch_from_primary_nic_dram;
threadpool thpool_fsync;

// Thread pool of the next pipeline stage.
threadpool thpool_coalesce;
threadpool thpool_copy_to_local_nvm;
threadpool thpool_copy_to_last_replica;

TL_EVENT_TIMER(evt_fetch_from_local_nvm);
TL_EVENT_TIMER(evt_fetch_from_primary_nic);
TL_EVENT_TIMER(evt_read_log_from_primary);

#ifdef PROFILE_REALTIME_FETCH_LOG_FROM_PRIMARY
rt_bw_stat fetch_log_from_primary_bw_stat = {0};
#endif
#ifdef PROFILE_REALTIME_FETCH_LOG_FROM_LOCAL
rt_bw_stat fetch_log_from_local_bw_stat = {0};
#endif

threadpool init_log_fetch_from_local_nvm_thpool(void)
{
	int th_num = mlfs_conf.thread_num_log_prefetch;
	char th_name[] = "lfet_locl_nvm";

	thpool_log_fetch_from_local_nvm = thpool_init(th_num, th_name);

	print_thread_init(th_num, th_name);

	// Init threadpool of the next pipeline stage.
	thpool_coalesce = init_coalesce_thpool();

#ifdef PROFILE_REALTIME_FETCH_LOG_FROM_LOCAL
	init_rt_bw_stat(&fetch_log_from_local_bw_stat, "fetch_log_from_local");
#endif

	return thpool_log_fetch_from_local_nvm;
}

threadpool init_log_fetch_from_primary_nic_dram_thpool(void)
{
	int th_num = mlfs_conf.thread_num_log_fetch;
	char th_name[] = "lfet_pri_nic";

	thpool_log_fetch_from_primary_nic_dram = thpool_init(th_num, th_name);

	print_thread_init(th_num, th_name);

	// Init threadpools of the next pipeline stage.
	thpool_copy_to_local_nvm = init_log_copy_to_local_nvm_thpool();
	thpool_copy_to_last_replica = init_log_copy_to_last_replica_thpool();

#ifdef PROFILE_REALTIME_FETCH_LOG_FROM_PRIMARY
	init_rt_bw_stat(&fetch_log_from_primary_bw_stat, "fetch_log_from_primary");
#endif

	return thpool_log_fetch_from_primary_nic_dram;
}

threadpool init_fsync_thpool(void)
{
	int th_num = mlfs_conf.thread_num_fsync;
	char th_name[] = "fsync";

	thpool_fsync = thpool_init(th_num, th_name);

	print_thread_init(th_num, th_name);

	return thpool_fsync;
}

/**
 * @Synopsis  Return size (in bytes) calculated with start block number and end
 * block number.
 *
 * @Param start_blknr
 * @Param end_blknr
 *
 * @Returns size in bytes
 */
uint64_t get_size(addr_t start_blknr, addr_t end_blknr)
{
	return (end_blknr - start_blknr + 1) << g_block_size_shift;
}

/**
 * @Synopsis  Read log data from primary NVM to primary NIC's DRAM.
 *
 * @Param libfs_id
 * @Param local_addr Address of local NIC DRAM buffer.
 * @Param remote_addr Address where the data is stored in host NVM.
 * @Param size
 *
 * @Returns size read.
 */
static uint64_t read_log_from_local_nvm(int libfs_id, uintptr_t local_addr,
					uintptr_t remote_addr, uint64_t size)
{
	rdma_meta_t *rdma_meta;
	int sock_bg;

	// Use background sockfd to wait for WR completion. A
	// nested polling doesn't work on log wrap around. That
	// is, On wrap around, nested polling thread receives
	// the second rpcmsg earlier than the completion of
	// reading log which breaks an assumption of the current
	// replication design.
	sock_bg = g_peers[libfs_id]->sockfd[SOCK_BG];

	pr_rep("libfs_id=%d local_addr=0x%lx remote_addr=0x%lx "
	       "size=%lu(%lu) sock=%d",
	       libfs_id, local_addr, remote_addr, size,
	       size >> g_block_size_shift, sock_bg);

	// No data to read.
	if (!size) {
		pr_rep("%s", "No data to read. Return.");
		return 0;
	}

	// TODO PROFILE mlfs_zalloc in create_rdma_meta.
	rdma_meta = create_rdma_meta(local_addr, remote_addr, size);

	IBV_WRAPPER_READ_SYNC(sock_bg, rdma_meta, MR_DRAM_BUFFER, MR_NVM_LOG);
	mlfs_free(rdma_meta);

	return (size >> g_block_size_shift);
}

/**
 * @Synopsis  A thread worker to fetch log data from host NVM to NIC DRAM in
 * background. It is called by NIC of the primary.
 */
void fetch_log_from_local_nvm_bg(void *arg)
{
	START_TL_TIMER(evt_fetch_from_local_nvm);

	uint64_t size;
	char *log_buf;
	uintptr_t remote_addr;

	log_fetch_from_local_nvm_arg *lf_arg =
		(log_fetch_from_local_nvm_arg *)arg;

	int libfs_id;
	uint64_t n_blks_read;
	addr_t start_blknr, end_blknr;
	struct replication_context *rctx;

	libfs_id = lf_arg->libfs_id;
	rctx = g_sync_ctx[libfs_id];

	print_log_fetch_from_local_nvm_arg(lf_arg, __func__);

	// NOTE We don't need to fetch log in seqn order.

	start_blknr = lf_arg->prefetch_start_blknr;
	end_blknr = start_blknr + lf_arg->n_to_prefetch_blk - 1;
	mlfs_assert(end_blknr <= rctx->size);

	remote_addr =
		lf_arg->libfs_base_addr + (start_blknr << g_block_size_shift);

	size = get_size(start_blknr, end_blknr);

#ifdef PREFETCH_FLOW_CONTROL
	// Limit prefetch rate to deal with out-of-NIC-memory problem.
	limit_prefetch_rate(size);
#endif

	log_buf = (char *)nic_slab_alloc_in_byte(size); // alloc MR to replicate

	n_blks_read = read_log_from_local_nvm(libfs_id, (uintptr_t)log_buf,
					      remote_addr, size);
#ifdef PROFILE_REALTIME_FETCH_LOG_FROM_LOCAL
	check_rt_bw(&fetch_log_from_local_bw_stat, size);
#endif

	// # of read == # of requested.
	mlfs_assert(n_blks_read == lf_arg->n_to_prefetch_blk);

	pr_lpref("[PREFETCH] libfs_id=%d n_to_prefetch(n_blks_read)=%lu "
		 "local_addr=%p libfs_base_addr=0x%lx start_blknr=%lu "
		 "end_blknr=%lu",
		 libfs_id, lf_arg->n_to_prefetch_blk, log_buf,
		 lf_arg->libfs_base_addr, start_blknr, end_blknr);

	// Next pipeline step: validate and coalesce.
	// TODO PROFILE slab may be faster?
	coalesce_arg *c_arg = (coalesce_arg *)mlfs_zalloc(sizeof(coalesce_arg));
	c_arg->seqn = lf_arg->seqn;
	c_arg->rctx = rctx;
	c_arg->log_buf = log_buf;
	c_arg->n_loghdrs = lf_arg->n_to_prefetch_loghdr;
	c_arg->log_size = size;
	c_arg->fetch_start_blknr = lf_arg->prefetch_start_blknr;
	c_arg->reset_meta = lf_arg->reset_meta;
	c_arg->fsync = lf_arg->fsync;
	c_arg->fsync_ack_addr = lf_arg->fsync_ack_addr;

#ifdef NO_PIPELINING
	END_TL_TIMER(evt_fetch_from_local_nvm);
	coalesce_log((void *) c_arg);
#else
	if (lf_arg->fsync) {
		END_TL_TIMER(evt_fetch_from_local_nvm);
		// Call function directly on fsync.
		coalesce_log((void *)c_arg);
	} else {
		thpool_add_work(thpool_coalesce, coalesce_log, (void *)c_arg);
		END_TL_TIMER(evt_fetch_from_local_nvm);
	}
#endif

	mlfs_free(arg);
}

/**
 * @Synopsis  Reads log data from the primary NIC's DRAM.
 *
 * @Param libfs_id
 * @Param local_addr
 * @Param remote_addr
 * @Param size
 * @Param sock
 *
 * @Returns   1 on success. 0 on fail.
 */
static int read_log_from_primary_nic_dram(int libfs_id, uintptr_t local_addr,
					  uintptr_t remote_addr, uint64_t size,
					  int sock)
{
	rdma_meta_t *rdma_meta;

	pr_rep("libfs_id=%d local_addr=0x%lx remote_addr=0x%lx "
	       "size=%lu(%lu) sock=%d",
	       libfs_id, local_addr, remote_addr, size,
	       size >> g_block_size_shift, sock);

	// No data to read.
	if (!size) {
		pr_rep("%s", "No data to read. Return.");
		return 0;
	}

	// TODO PROFILE mlfs_zalloc in create_rdma_meta.
	rdma_meta = create_rdma_meta(local_addr, remote_addr, size);

	IBV_WRAPPER_READ_SYNC(sock, rdma_meta, MR_DRAM_BUFFER, MR_DRAM_BUFFER);
	mlfs_free(rdma_meta);

	return 1;
}

/**
 * @Synopsis  It fetches log data from primary NIC's DRAM in background. It is
 * called by NIC of replica 1.
 */
void fetch_log_from_primary_nic_dram_bg(void *arg)
{
	START_TL_TIMER(evt_fetch_from_primary_nic);

	log_fetch_from_nic_arg *lf_arg = (log_fetch_from_nic_arg *)arg;
	char *local_log_buf;
	int sockfd, prev_kernfs_id;
	atomic_bool *copy_log_done_p;

	print_log_fetch_from_nic_arg(lf_arg, __func__);

	// alloc MR to replicate
	local_log_buf = (char *)nic_slab_alloc_in_byte(lf_arg->log_size);

	prev_kernfs_id = get_prev_kernfs(g_kernfs_id);
	// sockfd = g_peers[prev_kernfs_id]->sockfd[SOCK_BG];
	sockfd = g_peers[prev_kernfs_id]->sockfd[SOCK_IO]; // TODO decide socket.

	START_TL_TIMER(evt_read_log_from_primary);

	read_log_from_primary_nic_dram(lf_arg->libfs_id,
				       (uintptr_t)local_log_buf,
				       lf_arg->remote_log_buf_addr,
				       lf_arg->log_size, sockfd);

	END_TL_TIMER(evt_read_log_from_primary);

#ifdef PROFILE_REALTIME_FETCH_LOG_FROM_PRIMARY
	check_rt_bw(&fetch_log_from_primary_bw_stat, lf_arg->log_size);
#endif

	// Send an RPC to the previous NIC to release log buffer.
#ifdef COMPRESS_LOG
	if (lf_arg->fsync) {
		// mlfs_printf("Fetch done(FSYNC). libfs_id=%d seqn=%lu request "
		//             "primary's buffer=0x%lx\n",
		//             lf_arg->libfs_id, lf_arg->seqn,
		//             lf_arg->remote_log_buf_addr);
		set_fetch_done_flag(lf_arg->libfs_id, lf_arg->seqn,
				    lf_arg->fetch_log_done_addr,
				    sockfd); // SOCK_BG
	} else {
		// mlfs_printf("Fetch done. libfs_id=%d seqn=%lu request "
		//             "primary's buffer=0x%lx\n",
		//             lf_arg->libfs_id, lf_arg->seqn,
		//             lf_arg->remote_log_buf_addr);
		set_fetch_done_flag_to_val(lf_arg->libfs_id, lf_arg->seqn,
					   lf_arg->fetch_log_done_addr,
					   lf_arg->remote_log_buf_addr, sockfd);
	}
#else
	set_fetch_done_flag(lf_arg->libfs_id, lf_arg->seqn,
			    lf_arg->fetch_log_done_addr, sockfd); // SOCK_BG
#endif

#if defined(NO_PIPELINEING) & ! defined(NO_PIPELINING_BG_COPY)
	// No allocation.
#else
	// Allocate copy done flag.
	copy_log_done_p = (atomic_bool *)mlfs_alloc(sizeof(atomic_bool));
	atomic_init(copy_log_done_p, 0);
#endif

	// Next pipeline stage 1: copy to next replica.
	copy_to_last_replica_arg *cr_arg =
		(copy_to_last_replica_arg *)mlfs_alloc(
			sizeof(copy_to_last_replica_arg));
	cr_arg->rctx = g_sync_ctx[lf_arg->libfs_id];
	cr_arg->libfs_id = lf_arg->libfs_id;
	cr_arg->seqn = lf_arg->seqn;
	cr_arg->log_buf = local_log_buf;
	cr_arg->log_size = lf_arg->log_size;
	cr_arg->orig_log_size = lf_arg->orig_log_size;
	cr_arg->start_blknr = lf_arg->start_blknr;
	cr_arg->copy_log_done_p = copy_log_done_p;
	cr_arg->fsync = lf_arg->fsync;
	cr_arg->fsync_ack_addr = lf_arg->fsync_ack_addr;

	// Next pipeline stage 2: copy to local NVM.
	copy_to_local_nvm_arg *cl_arg = (copy_to_local_nvm_arg *)mlfs_alloc(
		sizeof(copy_to_local_nvm_arg));
	cl_arg->rctx = g_sync_ctx[lf_arg->libfs_id];
	cl_arg->libfs_id = lf_arg->libfs_id;
	cl_arg->seqn = lf_arg->seqn;
	cl_arg->log_buf = local_log_buf;
	cl_arg->log_size = lf_arg->log_size;
	cl_arg->orig_log_size = lf_arg->orig_log_size;
	cl_arg->start_blknr = lf_arg->start_blknr;
	cl_arg->copy_log_done_p = copy_log_done_p;
	cl_arg->fsync = lf_arg->fsync;


#ifdef NO_PIPELINING
#ifdef NO_PIPELINING_BG_COPY
	// Local copy in background.
	thpool_add_work(thpool_copy_to_local_nvm,
			copy_log_to_local_nvm_bg, (void *)cl_arg);
#else
	// Local copy first. (Serialized)
	copy_log_to_local_nvm_bg((void*)cl_arg);
#endif

	// Remote copy.
	copy_log_to_last_replica_bg((void *)cr_arg);
#else
	if (lf_arg->fsync) {
		// NOTE We don't send ack to libfs on fsync assuming that local copy
		// finishes earlier than remote copy.
		thpool_add_work(thpool_copy_to_local_nvm,
				copy_log_to_local_nvm_bg, (void *)cl_arg);

		// Call function directly on fsync.
		END_TL_TIMER(evt_fetch_from_primary_nic);
		copy_log_to_last_replica_bg((void *)cr_arg);

	} else {
		thpool_add_work(thpool_copy_to_last_replica,
				copy_log_to_last_replica_bg, (void *)cr_arg);

		// NOTE We don't send ack to libfs on fsync assuming that local copy
		// finishes earlier than remote copy.
		thpool_add_work(thpool_copy_to_local_nvm,
				copy_log_to_local_nvm_bg, (void *)cl_arg);

		END_TL_TIMER(evt_fetch_from_primary_nic);
	}
#endif

	mlfs_free(arg);
}

static void print_log_fetch_from_local_nvm_arg(log_fetch_from_local_nvm_arg *ar,
					       const char *caller)
{
	pr_pipe("%-30s ARG %-30s libfs_id=%d seqn=%lu prefetch_start_blknr=%lu "
		"n_to_prefetch_loghdr=%u n_to_prefetch_blk=%lu "
		"libfs_base_addr=%lu(0x%lx) reset_meta=%d fsync=%d "
		"fsync_ack_addr=%lu",
		"log_fetch_from_local_nvm", caller, ar->libfs_id, ar->seqn,
		ar->prefetch_start_blknr, ar->n_to_prefetch_loghdr,
		ar->n_to_prefetch_blk, ar->libfs_base_addr, ar->libfs_base_addr,
		ar->reset_meta, ar->fsync, ar->fsync_ack_addr);
}

static void print_log_fetch_from_nic_arg(log_fetch_from_nic_arg *ar,
					 const char *caller)
{
	pr_pipe("%-30s ARG %-30s libfs_id=%d seqn=%lu "
		"remote_log_buf_addr=0x%lx(%lu) log_size=%lu "
		"fetch_log_done_addr=0x%lx start_blknr=%lu fsync=%d "
		"fsync_ack_addr=%lu",
		"log_fetch_from_nic", caller, ar->libfs_id, ar->seqn,
		ar->remote_log_buf_addr, ar->remote_log_buf_addr, ar->log_size,
		ar->fetch_log_done_addr, ar->start_blknr, ar->fsync,
		ar->fsync_ack_addr);
}

void print_fetch_log_thpool_stat(void)
{
#ifdef PROFILE_THPOOL
	print_profile_result(thpool_log_fetch_from_local_nvm);
	print_profile_result(thpool_log_fetch_from_primary_nic_dram);
#endif
}

/** Print functions registered to each thread. **/
void print_log_fetch_from_local_nvm_stat(void *arg) {
	PRINT_TL_TIMER(evt_fetch_from_local_nvm, arg);
	RESET_TL_TIMER(evt_fetch_from_local_nvm);
}

void print_log_fetch_from_primary_nic_dram_stat(void *arg) {
	PRINT_TL_TIMER(evt_fetch_from_primary_nic, arg);
	// PRINT_TL_TIMER(evt_read_log_from_primary, arg);

	RESET_TL_TIMER(evt_fetch_from_primary_nic);
}

/** Macros for the following functions.
 *
 * void print_all_thpool_log_fetch_from_local_nvm_stats(void) {
 * 	print_per_thread_pipeline_stat(thpool_log_fetch_from_local_nvm);
 * }
 * void print_all_thpool_log_fetch_from_primary_nic_stats(void) {
 * 	print_per_thread_pipeline_stat(thpool_log_fetch_from_primary_nic_dram);
 * }
 **/
PRINT_ALL_PIPELINE_STAT_FUNC(log_fetch_from_local_nvm)
PRINT_ALL_PIPELINE_STAT_FUNC(log_fetch_from_primary_nic_dram)
