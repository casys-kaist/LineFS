#include "mlfs/mlfs_user.h"
#include "compress.h"
#include "distributed/rpc_interface.h"
#include "global/util.h"
#include "lz4.h"

threadpool thpool_compress;

TL_EVENT_TIMER(evt_compress);

#ifdef PROFILE_REALTIME_COMPRESS_BW
rt_bw_stat compress_bw_stat = {0};
#endif

void init_compress(void) {

#ifdef PROFILE_REALTIME_COMPRESS_BW
	init_rt_bw_stat(&compress_bw_stat, "compress");
#endif
}

threadpool init_compress_thpool(void)
{
	int th_num = mlfs_conf.thread_num_compress;
	char th_name[] = "compress";

	thpool_compress = thpool_init(th_num, th_name);

	print_thread_init(th_num, th_name);

	init_compress();

	return thpool_compress;
}

void compress_log_bg(void *arg)
{
	char *log_buf;
	uint64_t log_size;
	uint64_t orig_size; // It is different from log_size when data is comrpessed.
	int sockfd;

	START_TL_TIMER(evt_compress);

	compress_arg *c_arg = (compress_arg*)arg;

	print_compress_arg(c_arg);

	// Next pipeline: Send an RPC to the Replica 1.
	if (c_arg->fsync) {
		// Low-lat channel.
		sockfd = c_arg->rctx->next_rep_msg_sockfd[0];
	} else {
		sockfd = c_arg->rctx->next_digest_sockfd; // SOCK_BG channel.
	}

#ifdef COMPRESS_LOG
	if (c_arg->fsync) {
		log_buf = c_arg->log_buf;
		log_size = c_arg->log_size;
		orig_size = log_size;
	} else {
		// Compress log.
		char *input_buf;
		char *output_buf;
		uint64_t input_size;
		uint64_t max_output_size;

		input_buf = c_arg->log_buf;
		input_size = c_arg->log_size;

		max_output_size = LZ4_compressBound(input_size);

		// It is freed in end_pipeline().
		output_buf = (char *)nic_slab_alloc_in_byte(max_output_size);

		////////////////////////////////////////////////////////////////
		// For test: replace compress to memcpy/////////////////////////
		// memcpy(output_buf, input_buf, input_size);
		// log_size = input_size;
		///////////////////////////////////////////////////////////////

		const int actual_output_size = LZ4_compress_default(
			input_buf, output_buf, input_size, max_output_size);

		mlfs_assert(actual_output_size > 0);

#ifdef PROFILE_REALTIME_COMPRESS_BW
		// Throughput of compress module.
		check_rt_bw(&compress_bw_stat, input_size);
#endif

		// mlfs_printf("COMPRESS DONE old_log_buf=%p new_log_buf=%p "
		//             "compressed_size=%d orig_log_size=%lu\n",
		//             input_buf, output_buf, actual_output_size,
		//             input_size);

		log_buf = output_buf;
		log_size = actual_output_size;
		orig_size = input_size;
	}
#else
	log_buf = c_arg->log_buf;
	log_size = c_arg->log_size;
	orig_size = log_size;
#endif

	// TODO
	// Calculate new size of log.
	// 'fetch_start_blknr' of the replicas might be different from that of
	// primary if we coalesce log. Primary needs to maintain and calculate
	// start_blknr of the replicas.

	send_fetch_log_rpc_to_next_replica(c_arg->rctx, c_arg->seqn,
					   (uintptr_t)log_buf,
					   log_size,
					   orig_size,
					   c_arg->fetch_start_blknr,
					   (uintptr_t)c_arg->fetch_log_done_p,
					   c_arg->fsync,
					   c_arg->fsync_ack_addr,
					   sockfd);
	mlfs_free(arg);

	END_TL_TIMER(evt_compress);
}

void decompress_log(char *output_buf, char *input_buf, uint64_t compressed_size,
		    uint64_t original_size)
{
//#ifdef COMPRESS_LOG
//	output_buf = (char*)nic_slab_alloc_in_byte(original_size);
//
//	const int actual_output_size = LZ4_decompress_safe(
//		input_buf, output_buf, compressed_size, original_size);
//
//	// mlfs_printf("DECOMPRESS DONE old_log_buf=%p new_log_buf=%p "
//	//             "compressed_size=%lu decompressed_size=%d\n",
//	//             input_buf, output_buf, compressed_size, actual_output_size);
//
//	mlfs_assert(actual_output_size >= 0);
//	mlfs_assert(actual_output_size == original_size);
//#endif
}

static void send_fetch_log_rpc_to_next_replica(
	struct replication_context *rctx, uint64_t seqn, uintptr_t log_buf_addr,
	uint64_t log_size, uint64_t orig_size, addr_t start_blknr,
	uintptr_t fetch_log_done_addr, int fsync, uintptr_t fsync_ack_addr,
	int sockfd)
{
	char msg[MAX_SIGNAL_BUF];
	int libfs_id = rctx->peer->id;
	sprintf(msg,
		"|" TO_STR(RPC_FETCH_LOG) " |%d|%lu|%lu|%lu|%lu|%lu|%lu|%d|%lu|",
		libfs_id, seqn, log_buf_addr, log_size, orig_size, start_blknr,
		fetch_log_done_addr, fsync, fsync_ack_addr);

	rpc_forward_msg_no_seqn(sockfd, msg);
}

static void print_compress_arg(compress_arg *ar)
{
	pr_pipe("ARG %-30s libfs_id=%d seqn=%lu log_buf=%p log_size=%lu "
		"fetch_start_blknr=%lu fetch_log_done_p=%p fsync=%d "
		"fsync_ack_addr=%lu",
		"compress", ar->rctx->peer->id, ar->seqn, ar->log_buf,
		ar->log_size, ar->fetch_start_blknr, ar->fetch_log_done_p,
		ar->fsync, ar->fsync_ack_addr);
}

void print_compress_thpool_stat(void)
{
#ifdef PROFILE_THPOOL
	print_profile_result(thpool_compress);
#endif
}

/** Print functions registered to each thread. **/
void print_compress_stat(void *arg)
{
	PRINT_TL_TIMER(evt_compress, arg);

	RESET_TL_TIMER(evt_compress);
}

PRINT_ALL_PIPELINE_STAT_FUNC(compress)
