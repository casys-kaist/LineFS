#include "mlfs/mlfs_user.h"
#include "copy_log.h"
#include "distributed/rpc_interface.h"
#include "host_memcpy.h"
#include "compress.h"


threadpool thpool_copy_to_local_nvm;	// In Replica 1.
threadpool thpool_copy_to_last_replica; // In Replica 1.
threadpool thpool_free_log_buf;		// In Replica 1.
threadpool thpool_handle_copy_done_ack; // In the last replica (Replica 2).

TL_EVENT_TIMER(evt_copy_to_local_nvm);
TL_EVENT_TIMER(evt_copy_to_last_replica);
TL_EVENT_TIMER(evt_free_logbuf);
TL_EVENT_TIMER(evt_handle_copy_done);
TL_EVENT_TIMER(evt_manage_fsync_ack);
TL_EVENT_TIMER(evt_wait_log_persisted_bitmap);
TL_EVENT_TIMER(evt_set_log_persisted_bit);
TL_EVENT_TIMER(evt_copy_log_to_last_wait_wr_compl);
TL_EVENT_TIMER(evt_send_fsync_ack);

#ifdef PROFILE_REALTIME_NET_BW_USAGE
// TODO Replace it with realtime_bw_stat in util.h
// stats about sending log from replica 1 to replica 2.
uint64_t bytes_sent_per_sec = 0;
struct timespec net_bw_start_time = { 0 };
struct timespec net_bw_end_time = { 0 };

// stats about sending log from NIC to local NVM in replica 1.
uint64_t bytes_sent_per_sec_local = 0;
struct timespec net_bw_start_time_local = { 0 };
struct timespec net_bw_end_time_local = { 0 };

pthread_spinlock_t net_bw_lock;
pthread_spinlock_t net_bw_lock_local;
#endif


threadpool init_log_copy_to_local_nvm_thpool(void)
{
	int th_num = mlfs_conf.thread_num_copy_log_to_local_nvm;
	char th_name[] = "lcpy_nvm";

	thpool_copy_to_local_nvm = thpool_init(th_num, th_name);

	print_thread_init(th_num, th_name);

#ifdef PROFILE_REALTIME_NET_BW_USAGE
	pthread_spin_init(&net_bw_lock_local, PTHREAD_PROCESS_PRIVATE);
#endif

	return thpool_copy_to_local_nvm;
}

threadpool init_log_copy_to_last_replica_thpool(void)
{
	int th_num = mlfs_conf.thread_num_copy_log_to_last_replica;
	char th_name[] = "lcpy_last";

	thpool_copy_to_last_replica = thpool_init(th_num, th_name);

	print_thread_init(th_num, th_name);

	// Init threadpool of the next pipeline stage.
	thpool_free_log_buf = init_log_buf_free_thpool();

#ifdef PROFILE_REALTIME_NET_BW_USAGE
	pthread_spin_init(&net_bw_lock, PTHREAD_PROCESS_PRIVATE);
#endif

	return thpool_copy_to_last_replica;
}

threadpool init_log_buf_free_thpool(void)
{
	int th_num = 1;
	char th_name[] = "free_lbf";

	thpool_free_log_buf = thpool_init(th_num, th_name);

	print_thread_init(th_num, th_name);

	return thpool_free_log_buf;
}

threadpool init_copy_done_ack_handle_thpool(void)
{
	int th_num = 1;
	char th_name[] = "cpy_ack";

	thpool_handle_copy_done_ack = thpool_init(th_num, th_name);

	print_thread_init(th_num, th_name);

	return thpool_handle_copy_done_ack;
}

static int manage_fsync_ack_th_num = 2;
threadpool init_manage_fsync_ack_thpool(int libfs_id)
{
	int th_num = manage_fsync_ack_th_num; // We required at least 2 threads:
					      // One is for busy waiting on
					      // fsync and the other is for
					      // setting bits.
	char th_name[64] = {0};
	threadpool ret;
	sprintf(th_name, "fsyn_ack%d", libfs_id);

	ret = thpool_init(th_num, th_name);

	print_thread_init(th_num, th_name);

	return ret;
}

#ifdef PROFILE_REALTIME_NET_BW_USAGE
void check_remote_copy_net_bw(uint64_t sent_bytes)
{
	struct timespec *start = &net_bw_start_time;
	struct timespec *end = &net_bw_end_time;

	pthread_spin_lock(&net_bw_lock);

	if (start->tv_sec == 0) { // Init.
		clock_gettime(CLOCK_MONOTONIC, start);
	}

	clock_gettime(CLOCK_MONOTONIC, end);
	bytes_sent_per_sec += sent_bytes;

	if (get_duration(start, end) > 1.0) {
		printf("[NET_BW_USAGE] remote_copy: %lu MB/s\n",
		       bytes_sent_per_sec >> 20);

		clock_gettime(CLOCK_MONOTONIC, start);
		bytes_sent_per_sec = 0;
	}

	pthread_spin_unlock(&net_bw_lock);
}

void check_local_copy_net_bw(uint64_t sent_bytes)
{
	struct timespec *start = &net_bw_start_time_local;
	struct timespec *end = &net_bw_end_time_local;

	pthread_spin_lock(&net_bw_lock_local);

	if (start->tv_sec == 0) { // Init.
		clock_gettime(CLOCK_MONOTONIC, start);
	}

	clock_gettime(CLOCK_MONOTONIC, end);
	bytes_sent_per_sec_local += sent_bytes;

	if (get_duration(start, end) > 1.0) {
		printf("[NET_BW_USAGE] local_copy: %lu MB/s\n",
		       bytes_sent_per_sec_local >> 20);

		clock_gettime(CLOCK_MONOTONIC, start);
		bytes_sent_per_sec_local = 0;
	}
	pthread_spin_unlock(&net_bw_lock_local);
}
#endif

/**
 * @Synopsis  It copies log data to the local NVM in background. It is called by
 * NIC of replica 1.
 */
void copy_log_to_local_nvm_bg(void *arg)
{
	START_TL_TIMER(evt_copy_to_local_nvm);

	copy_to_local_nvm_arg *cl_arg = (copy_to_local_nvm_arg *)arg;
	struct replication_context *rctx;
	rdma_meta_t *rdma_meta, *rdma_meta_r;
	uintptr_t remote_addr;
	uint32_t wr_id;
	int sock;
	copy_done_arg *cd_arg;
	char *local_buf;

	print_copy_to_local_nvm_arg(cl_arg);

	rctx = cl_arg->rctx;
	remote_addr = rctx->host_rctx->base_addr +
		      (cl_arg->start_blknr << g_block_size_shift);

#ifdef COMPRESS_LOG
	if (cl_arg->fsync) {
		local_buf = cl_arg->log_buf;
	} else {
		// Alloc a buffer for decompressed data.
		local_buf =
			(char *)nic_slab_alloc_in_byte(cl_arg->orig_log_size);
		decompress_log(local_buf, cl_arg->log_buf, cl_arg->log_size,
			       cl_arg->orig_log_size);
	}
#else
	local_buf = cl_arg->log_buf;
#endif

	rdma_meta = create_rdma_meta((uintptr_t)local_buf, remote_addr,
				     cl_arg->log_size);

	// sock = rctx->host_rctx->peer->info->sockfd[SOCK_BG];
	sock = rctx->next_rep_data_sockfd[0];

	wr_id = IBV_WRAPPER_WRITE_ASYNC(sock, rdma_meta, MR_DRAM_BUFFER,
					MR_NVM_LOG);
	// Send READ RDMA to persist data. Read the last 1 byte.
	if (mlfs_conf.persist_nvm_with_rdma_read) {
		uintptr_t last_local_addr =
			((uintptr_t)local_buf) + cl_arg->log_size - 1;
		uintptr_t last_remote_addr = remote_addr + cl_arg->log_size - 1;

		rdma_meta_r =
			create_rdma_meta(last_local_addr, last_remote_addr, 1);

		wr_id = IBV_WRAPPER_READ_ASYNC(sock, rdma_meta_r,
					       MR_DRAM_BUFFER, MR_NVM_LOG);
	}

	// Wait for the WR completion.
	IBV_AWAIT_WORK_COMPLETION(sock, wr_id);

#ifdef COMPRESS_LOG
	// Free decompressed data buffer.
	if (!cl_arg->fsync)
		nic_slab_free(local_buf);
#endif

#ifdef PROFILE_REALTIME_NET_BW_USAGE
	check_local_copy_net_bw(cl_arg->log_size);
#endif

	cd_arg = (copy_done_arg*)mlfs_zalloc(sizeof(copy_done_arg));
	cd_arg->libfs_id = cl_arg->libfs_id;
	cd_arg->seqn = cl_arg->seqn;
	atomic_init(&cd_arg->processed, 0);

	// mlfs_printf("Enqueue copy done: libfs_id=%d seqn=%lu\n",
	//             cd_arg->libfs_id, cd_arg->seqn);
	enqueue_copy_done_item(cd_arg);

#if defined(NO_PIPELINEING) & ! defined(NO_PIPELINING_BG_COPY)
	// log buffer will be freed after copying to last replica.
#else
	// Set flag to free log buffer.
	atomic_store(cl_arg->copy_log_done_p, 1);
#endif

	mlfs_free(cd_arg);
	mlfs_free(rdma_meta);
	if (mlfs_conf.persist_nvm_with_rdma_read)
		mlfs_free(rdma_meta_r);
	mlfs_free(arg);

	END_TL_TIMER(evt_copy_to_local_nvm);
}

/**
 * @Synopsis  It copies log data to the last replica's NVM in background. It is
 * called by NIC of replica 1.
 */
void copy_log_to_last_replica_bg(void *arg)
{
	START_TL_TIMER(evt_copy_to_last_replica);

	copy_to_last_replica_arg *c_arg = (copy_to_last_replica_arg*)arg;
	struct replication_context *rctx;
	rdma_meta_t *rdma_meta, *rdma_meta_r;
	uintptr_t remote_addr;
	uint32_t wr_id;
	int sock;

	print_copy_to_last_replica_arg(c_arg);

	rctx = c_arg->rctx;
	remote_addr = rctx->peer->base_addr +
		      (c_arg->start_blknr << g_block_size_shift);
	rdma_meta = create_rdma_meta((uintptr_t)c_arg->log_buf, remote_addr,
				     c_arg->log_size);

	pr_pipe("COPY_LOG_TO_LAST libfs_id=%d seqn=%lu local_addr=%p "
		"remote_addr=0x%lx(base=0x%lx start=0x%lx(%lu))",
		c_arg->libfs_id, c_arg->seqn, c_arg->log_buf, remote_addr,
		rctx->peer->base_addr, c_arg->start_blknr,
		c_arg->start_blknr << g_block_size_shift);

	sock = rctx->next_rep_data_sockfd[1];
	wr_id = IBV_WRAPPER_WRITE_ASYNC(sock, rdma_meta, MR_DRAM_BUFFER,
					MR_NVM_LOG);
	// Replaced with log_persist request to replica 2 host.
	// Send READ RDMA to persist data.
	if (mlfs_conf.persist_nvm_with_rdma_read) {
		uintptr_t last_local_addr =
			((uintptr_t)c_arg->log_buf) + c_arg->log_size - 1;
		uintptr_t last_remote_addr = remote_addr + c_arg->log_size -1;

		rdma_meta_r =
			create_rdma_meta(last_local_addr, last_remote_addr, 1);

		wr_id = IBV_WRAPPER_READ_ASYNC(sock, rdma_meta_r,
					       MR_DRAM_BUFFER, MR_NVM_LOG);
	}

	// Wait for the WR completion.
	START_TL_TIMER(evt_copy_log_to_last_wait_wr_compl);
	IBV_AWAIT_WORK_COMPLETION(sock, wr_id);
	END_TL_TIMER(evt_copy_log_to_last_wait_wr_compl);

#ifdef PROFILE_REALTIME_NET_BW_USAGE
	check_remote_copy_net_bw(c_arg->log_size);
#endif

	if (mlfs_conf.persist_nvm_with_clflush) {
		// Next pipeline stage 1: Request persist_log to replica 2 host.
		if (c_arg->fsync && c_arg->fsync_ack_addr) { // fsync_ack_addr
							     // == 0 on
							     // rotation.
			send_persist_log_req_to_last_replica_host(
				c_arg->libfs_id, c_arg->seqn, sock,
				g_sync_ctx[c_arg->libfs_id]->begin,
				g_sync_ctx[c_arg->libfs_id]->size,
				c_arg->start_blknr,
				c_arg->log_size >> g_block_size_shift,
				c_arg->fsync_ack_addr);
		}
	}

#ifdef NO_PIPELINING  // If NO_PIPELINING, all requests are serialized.
	// Next pipeline stage 2: Send_fsync_ack.
	manage_fsync_ack_arg *mfa_arg = (manage_fsync_ack_arg *)mlfs_alloc(
		sizeof(manage_fsync_ack_arg));
	mfa_arg->libfs_id = rctx->peer->id;
	mfa_arg->seqn = c_arg->seqn;
	mfa_arg->fsync = c_arg->fsync;
	mfa_arg->fsync_ack_addr = c_arg->fsync_ack_addr;

	if (c_arg->fsync)
		manage_fsync_ack((void *)mfa_arg);
#else
	// On fsync, check once for better latency. Send fsync_ack to LibFS.
	// NOTE fsync_ack_addr is 0 on rotation.
	if (c_arg->fsync && c_arg->fsync_ack_addr &&
	    all_previous_log_persisted_bits_set(rctx, c_arg->seqn)) {
		// mlfs_printf("No threading fsync seqn=%lu\n", c_arg->seqn);
		send_fsync_ack(c_arg->libfs_id, c_arg->fsync_ack_addr);
	} else {
		// Next pipeline stage 2: Send_fsync_ack.
		manage_fsync_ack_arg *mfa_arg =
			(manage_fsync_ack_arg *)mlfs_alloc(
				sizeof(manage_fsync_ack_arg));
		mfa_arg->libfs_id = rctx->peer->id;
		mfa_arg->seqn = c_arg->seqn;
		mfa_arg->fsync = c_arg->fsync;
		mfa_arg->fsync_ack_addr = c_arg->fsync_ack_addr;

		thpool_add_work(rctx->thpool_manage_fsync_ack, manage_fsync_ack,
				(void *)mfa_arg);
	}
#endif

	// Next pipeline stage 3: Ack to last replica.
	send_copy_done_ack_to_last_replica(rctx->peer->id, c_arg->seqn,
					   c_arg->start_blknr, c_arg->log_size,
					   c_arg->orig_log_size, c_arg->fsync,
					   rctx->next_digest_sockfd);

	// Next pipeline stage 4: Release log buffer.
	log_buf_free_arg *f_arg =
		(log_buf_free_arg *)mlfs_alloc(sizeof(log_buf_free_arg));
	f_arg->libfs_id = rctx->peer->id;
	f_arg->seqn = c_arg->seqn;
	f_arg->log_buf = c_arg->log_buf;
	f_arg->copy_log_done_p = c_arg->copy_log_done_p;

#ifdef NO_PIPELINING
	END_TL_TIMER(evt_copy_to_last_replica);
	free_log_buf((void*)f_arg);
#else
	thpool_add_work(thpool_free_log_buf, free_log_buf, (void*)f_arg);
	END_TL_TIMER(evt_copy_to_last_replica);
#endif
	mlfs_free(rdma_meta);
	mlfs_free(arg);

}

static void free_log_buf(void *arg) {

	START_TL_TIMER(evt_free_logbuf);

	log_buf_free_arg *f_arg = (log_buf_free_arg*)arg;

	print_log_buf_free_arg(f_arg);

#if defined(NO_PIPELINEING) & ! defined(NO_PIPELINING_BG_COPY)
	// Just free it.
#else
	// TODO PROFILE atomic operation.
	// FIXME Use release and acquire semantic.
	while(!atomic_load(f_arg->copy_log_done_p)) {
		cpu_relax();
	}

	mlfs_free(f_arg->copy_log_done_p);
#endif
	// Free log buffer and flag.
	nic_slab_free(f_arg->log_buf);
	mlfs_free(arg);

	END_TL_TIMER(evt_free_logbuf);
}

static void send_persist_log_req_to_last_replica_host(
	int libfs_id, uint64_t seqn, int sockfd, addr_t log_area_begin_blknr,
	addr_t log_area_end_blknr, addr_t start_blknr, uint64_t n_log_blks,
	uintptr_t fsync_ack_addr)
{
	char msg[MAX_SIGNAL_BUF];
	sprintf(msg,
		"|" TO_STR(RPC_PERSIST_LOG) " |%d|%lu|%lu|%lu|%lu|%lu|%lu|",
		libfs_id, seqn, log_area_begin_blknr, log_area_end_blknr,
		start_blknr, n_log_blks, fsync_ack_addr);

	rpc_forward_msg_no_seqn(sockfd, msg);
}

static void send_copy_done_ack_to_last_replica(int libfs_id, uint64_t seqn,
					       addr_t start_blknr,
					       uint64_t log_size,
					       uint64_t orig_log_size,
					       int fsync, int sockfd)
{
	char msg[MAX_SIGNAL_BUF];
	sprintf(msg, "|" TO_STR(RPC_LOG_COPY_DONE) " |%d|%lu|%lu|%lu|%lu|%d",
		libfs_id, seqn, start_blknr, log_size, orig_log_size, fsync);

	rpc_forward_msg_no_seqn(sockfd, msg);
}

void handle_copy_done_ack(void *arg) {
	START_TL_TIMER(evt_handle_copy_done);

	copy_done_arg *cd_arg = (copy_done_arg*)arg;

	print_copy_done_arg(cd_arg);

#if 0 // Currently, Do not decompress on Replica 2. Just to do experiment.
#ifdef COMPRESS_LOG
	if (!cd_arg->fsync) {
		// Read log from local NVM, decompress it, and write back to
		// NVM.
		rdma_meta_t *rdma_meta;
		char *compressed_buf;
		char *decompressed_buf;
		uintptr_t remote_addr;
		int sock;

		remote_addr =
			g_sync_ctx[cd_arg->libfs_id]->host_rctx->base_addr +
			(cd_arg->start_blknr << g_block_size_shift);

		compressed_buf =
			(char *)nic_slab_alloc_in_byte(cd_arg->log_size);
		rdma_meta = create_rdma_meta((uintptr_t)compressed_buf,
					     remote_addr, cd_arg->log_size);

		sock = g_kernfs_peers[nic_kid_to_host_kid(g_kernfs_id)]
			       ->sockfd[SOCK_BG];

		mlfs_printf("DECOMPRESS Get log data: libfs_id=%d seqn=%lu "
			    "remote_addr=0x%lx local_addr=0x%p size=%lu\n",
			    cd_arg->libfs_id, cd_arg->seqn, remote_addr,
			    compressed_buf, cd_arg->log_size);

		IBV_WRAPPER_READ_SYNC(sock, rdma_meta, MR_DRAM_BUFFER,
				      MR_NVM_LOG);

		decompressed_buf =
			(char *)nic_slab_alloc_in_byte(cd_arg->orig_log_size + 1024);
		decompress_log(decompressed_buf, compressed_buf,
			       cd_arg->log_size, cd_arg->orig_log_size);

		nic_slab_free(compressed_buf);

		reset_single_sge_rdma_meta((uintptr_t)decompressed_buf,
					   remote_addr, cd_arg->orig_log_size,
					   rdma_meta);

		mlfs_printf("DECOMPRESS Send log data: libfs_id=%d seqn=%lu "
			    "remote_addr=0x%lx local_addr=0x%p size=%lu\n",
			    cd_arg->libfs_id, cd_arg->seqn, remote_addr,
			    compressed_buf, cd_arg->log_size);
		IBV_WRAPPER_WRITE_ASYNC(sock, rdma_meta, MR_DRAM_BUFFER,
					MR_NVM_LOG);
		// Send READ RDMA to persist data. Read the last 1 byte.
		if (mlfs_conf.persist_nvm_with_rdma_read) {
			rdma_meta_t *rdma_meta_r;
			uintptr_t last_local_addr;
			uintptr_t last_remote_addr;

			last_local_addr = ((uintptr_t)decompressed_buf) +
					  cd_arg->orig_log_size - 1;
			last_remote_addr =
				remote_addr + cd_arg->orig_log_size - 1;

			rdma_meta_r = create_rdma_meta(last_local_addr,
						       last_remote_addr, 1);

			IBV_WRAPPER_READ_SYNC(sock, rdma_meta_r, MR_DRAM_BUFFER,
					      MR_NVM_LOG);
			mlfs_free(rdma_meta_r);
		}

		nic_slab_free(decompressed_buf);
		mlfs_free(rdma_meta);
	}
#endif
#endif
	// mlfs_printf("Enqueue copy done: libfs_id=%d seqn=%lu\n",
	//             cd_arg->libfs_id, cd_arg->seqn);
	enqueue_copy_done_item(cd_arg);

	mlfs_free(arg);

	END_TL_TIMER(evt_handle_copy_done);
}

static void set_log_persisted_bit(struct replication_context *rctx,
				  uint64_t seqn)
{
	unsigned long *bitmap = rctx->log_persisted_bitmap;
	uint64_t start_seqn = rctx->log_persisted_bitmap_start_seqn;

	mlfs_assert(seqn - start_seqn <= COPY_NEXT_BITMAP_SIZE_IN_BIT);

	pr_pipe("Setting log_persisted_bit libfs_id=%d seqn=%lu start_seqn=%lu",
		rctx->peer->id, seqn, start_seqn);

	pthread_spin_lock(&rctx->log_persisted_bitmap_lock);
	bitmap_set(bitmap, seqn - start_seqn, 1);
	pthread_spin_unlock(&rctx->log_persisted_bitmap_lock);
}

static bool
all_previous_log_persisted_bits_set(struct replication_context *rctx,
				    uint64_t fsync_seqn)
{
	// Check bits are set.
	unsigned long *bitmap = rctx->log_persisted_bitmap;
	uint64_t start_seqn = rctx->log_persisted_bitmap_start_seqn;
	int n_bit_set; // It is int.
	bool ret = 0;

	n_bit_set = bitmap_weight(bitmap, fsync_seqn - start_seqn);
	// mlfs_printf("n_bit_set=%d fsync_seqn=%lu start_seqn=%lu\n", n_bit_set,
	//        fsync_seqn, start_seqn);

	ret = (n_bit_set == (int)(fsync_seqn - start_seqn));

	// Reset bitmap and start_seqn.
	if (ret) {
		// mlfs_printf("fsync_seqn matched: libfs_id=%d fsync_seqn=%lu  "
		//             "start_seqn=%lu->%lu n_bit_set=%d\n",
		//             rctx->peer->id, fsync_seqn, start_seqn,
		//             fsync_seqn + 1, n_bit_set);

		pthread_spin_lock(&rctx->log_persisted_bitmap_lock);

		// sizeof(uint8_t)*8: enough length of bits. XXX: Is it enough length?
		bitmap_shift_right(bitmap, bitmap, fsync_seqn - start_seqn + 1,
				   fsync_seqn - start_seqn + 1 +
					   sizeof(uint8_t) * 8);

		rctx->log_persisted_bitmap_start_seqn = fsync_seqn + 1;

		pthread_spin_unlock(&rctx->log_persisted_bitmap_lock);
	}
	return ret;
}

static void send_fsync_ack(int libfs_id, uintptr_t fsync_ack_addr) {
	START_TL_TIMER(evt_send_fsync_ack);
	// Set LibFS's fsync ack bit to 1.
	// mlfs_printf("Fsync ack to libfs sockfd=%d addr=0x%lx\n",
	//             g_peers[libfs_id]->sockfd[SOCK_IO],
	//             fsync_ack_addr);

	// Send ack to libfs from replica 1.
	uint8_t val = 1;
	// Refer to struct fsync_acks.
	uintptr_t dst_addr = fsync_ack_addr;
	// mlfs_printf("fsync ack val:%hhu RDMA write to libfs_id=%d "
	//             "addr=0x%lx\n",
	//             val, libfs_id,
	//             fsync_ack_addr + 1);
	rpc_write_remote_val8(g_peers[libfs_id]->sockfd[SOCK_IO],
			      dst_addr, val);
	END_TL_TIMER(evt_send_fsync_ack);
}

static void manage_fsync_ack(void *arg)
{
	START_TL_TIMER(evt_manage_fsync_ack);

	manage_fsync_ack_arg *mfa_arg = (manage_fsync_ack_arg *)arg;
	struct replication_context *rctx = g_sync_ctx[mfa_arg->libfs_id];

	// Send fsync_ack to LibFS. NOTE fsync_ack_addr is 0 on rotation.
	if (mfa_arg->fsync && mfa_arg->fsync_ack_addr) {

#ifndef NO_PIPELINING // If NO_PIPELINING, all requests are serialized.
		START_TL_TIMER(evt_wait_log_persisted_bitmap);

		// Wait until all previous bits are set.
		while (!all_previous_log_persisted_bits_set(rctx,
							    mfa_arg->seqn)) {
			cpu_relax();
		}

		END_TL_TIMER(evt_wait_log_persisted_bitmap);
#endif

		// mlfs_printf("In another thread fsync seqn=%lu\n", mfa_arg->seqn);
		send_fsync_ack(mfa_arg->libfs_id, mfa_arg->fsync_ack_addr);

	} else {
#ifndef NO_PIPELINING
		START_TL_TIMER(evt_set_log_persisted_bit);
		set_log_persisted_bit(rctx, mfa_arg->seqn);
		END_TL_TIMER(evt_set_log_persisted_bit);
#endif
	}

	mlfs_free(arg);

	END_TL_TIMER(evt_manage_fsync_ack);
}

static void print_copy_to_local_nvm_arg(copy_to_local_nvm_arg *ar)
{
	pr_pipe("ARG %-30s libfs_id=%d seqn=%lu log_buf=%p log_size=%lu "
		"orig_log_size=%lu start_blknr=%lu copy_log_done=%d",
		"copy_to_local_nvm", ar->libfs_id, ar->seqn, ar->log_buf,
		ar->log_size, ar->orig_log_size, ar->start_blknr,
		atomic_load(ar->copy_log_done_p));
}

static inline void print_copy_to_last_replica_arg(copy_to_last_replica_arg *ar)
{
	pr_pipe("ARG %-30s libfs_id=%d seqn=%lu log_buf=%p log_size=%lu "
		"orig_log_size=%lu start_blknr=%lu copy_log_done=%d fsync=%d "
		"fsync_ack_addr=%lu",
		"copy_to_last_replica", ar->libfs_id, ar->seqn, ar->log_buf,
		ar->log_size, ar->orig_log_size, ar->start_blknr,
		atomic_load(ar->copy_log_done_p), ar->fsync,
		ar->fsync_ack_addr);
}

static void print_log_buf_free_arg(log_buf_free_arg *ar)
{
	pr_pipe("ARG %-30s libfs_id=%d seqn=%lu log_buf=%p copy_log_done_p=%d",
		"log_buf_free", ar->libfs_id, ar->seqn, ar->log_buf,
		atomic_load(ar->copy_log_done_p));
}

static void print_copy_done_arg(copy_done_arg *ar)
{
	pr_pipe("ARG %-30s libfs_id=%d seqn=%lu start_blknr=%lu log_size=%lu "
		"orig_log_size=%lu processed=%d",
		"copy_done", ar->libfs_id, ar->seqn, ar->start_blknr,
		ar->log_size, ar->orig_log_size, atomic_load(&ar->processed));
}

void print_copy_log_thpool_stat(void)
{
#ifdef PROFILE_THPOOL
	print_profile_result(thpool_copy_to_local_nvm);
	print_profile_result(thpool_copy_to_last_replica);
	print_profile_result(thpool_free_log_buf);
	print_profile_result(thpool_handle_copy_done_ack);
#endif
}

/** Print functions registered to each thread. **/
void print_copy_to_local_nvm_stat(void *arg)
{
	PRINT_TL_TIMER(evt_copy_to_local_nvm, arg);
	RESET_TL_TIMER(evt_copy_to_local_nvm);
}
void print_copy_to_last_replica_stat(void *arg)
{
	PRINT_TL_TIMER(evt_copy_to_last_replica, arg);
	PRINT_TL_TIMER(evt_copy_log_to_last_wait_wr_compl, arg);
	PRINT_TL_TIMER(evt_send_fsync_ack, arg);

	RESET_TL_TIMER(evt_copy_to_last_replica);
	RESET_TL_TIMER(evt_copy_log_to_last_wait_wr_compl);
	RESET_TL_TIMER(evt_send_fsync_ack);
}
void print_free_log_buf_stat(void *arg)
{
	PRINT_TL_TIMER(evt_free_logbuf, arg);
	RESET_TL_TIMER(evt_free_logbuf);
}
void print_handle_copy_done_ack_stat(void *arg)
{
	PRINT_TL_TIMER(evt_handle_copy_done, arg);
	RESET_TL_TIMER(evt_handle_copy_done);
}
static void print_manage_fsync_ack_stat(void *arg) {
	PRINT_TL_TIMER(evt_manage_fsync_ack, arg);
	PRINT_TL_TIMER(evt_set_log_persisted_bit, arg);
	PRINT_TL_TIMER(evt_wait_log_persisted_bitmap, arg);

	RESET_TL_TIMER(evt_manage_fsync_ack);
	RESET_TL_TIMER(evt_set_log_persisted_bit);
	RESET_TL_TIMER(evt_wait_log_persisted_bitmap);
}

PRINT_ALL_PIPELINE_STAT_FUNC(copy_to_local_nvm)
PRINT_ALL_PIPELINE_STAT_FUNC(copy_to_last_replica)
PRINT_ALL_PIPELINE_STAT_FUNC(free_log_buf)
PRINT_ALL_PIPELINE_STAT_FUNC(handle_copy_done_ack)

void print_all_thpool_manage_fsync_ack_stats(void)
{
	for (int i = g_n_nodes; i < g_n_nodes + MAX_LIBFS_PROCESSES; i++) {
		if (g_sync_ctx[i]) {
			for (uint64_t j = 0; j < manage_fsync_ack_th_num; j++) {
				// print_per_thread_pipeline_stat(tp);
				thpool_add_work(
					g_sync_ctx[i]->thpool_manage_fsync_ack,
					print_manage_fsync_ack_stat, (void *)j);
				usleep(10000); // To print in order.
			}
		}
	}
}
