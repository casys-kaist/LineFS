#include "mlfs/mlfs_user.h"
#include "build_memcpy_list.h"
#include "host_memcpy.h"
#include "global/defs.h"
#include "global/util.h"
#include "filesystem/shared.h"
#include "distributed/peer.h"
#include "distributed/rpc_interface.h" // g_self_id
#include "storage/storage.h"
#include "io/block_io.h"
#include "extents.h"
#include "digest.h"

// TODO to be deleted. for test.
pthread_mutex_t g_publish_lock;

TL_EVENT_TIMER(evt_build_memcpy_list);
TL_EVENT_TIMER(evt_pub_lock_wait);
TL_EVENT_TIMER(evt_digest_logs);
TL_EVENT_TIMER(evt_persist_dirty_nvm);
TL_EVENT_TIMER(evt_part1);
TL_EVENT_TIMER(evt_part3);

uint16_t *inode_version_table = NULL;

static void init_build_memcpy_list(int libfs_id)
{
	if (!inode_version_table) {
		inode_version_table =
			(uint16_t *)mlfs_zalloc(sizeof(uint16_t) * NINODES);

		pthread_mutex_init(&g_publish_lock, NULL);
	}

	// Init memcpy list buffer.
	// g_sync_ctx[libfs_id]->memcpy_meta_list_buf =
	//         (memcpy_meta_t *)nic_slab_alloc_in_byte(sizeof(memcpy_meta_t) *
	//                                                 MAX_NUM_LOGHDRS_IN_LOG);
}

threadpool init_build_memcpy_list_thpool(int libfs_id)
{
	int th_num = 1;
	char th_name[64] = { 0 };
	threadpool ret;

	sprintf(th_name, "b_mcpy_l%d", libfs_id);
	ret = thpool_init(th_num, th_name);
	print_thread_init(th_num, th_name);

	init_build_memcpy_list(libfs_id);

	return ret;
}

static int digest_logs(uint8_t from_dev, int libfs_id, int n_hdrs,
		       addr_t digest_start_blknr, struct logheader *loghdr_buf,
		       struct memcpy_meta_array *memcpy_meta_list)
{
	loghdr_meta_t *loghdr_meta;
	int i, n_digest;
	addr_t current_loghdr_blknr;
	struct replay_list replay_list = {
		.i_digest_hash = NULL,
		.f_digest_hash = NULL,
		.u_digest_hash = NULL,
	};
	INIT_LIST_HEAD(&replay_list.head);

	current_loghdr_blknr = digest_start_blknr;

	// START_TIMER(evt_coal);

	// DEBUG
	// mlfs_printf("DEBUG DIGEST START peer->rsync_start=%lu "
	//             "peer->digest_start=%lu digest_start_blkno=%lu\n",
	//             g_sync_ctx[libfs_id]->peer->local_start,
	//             g_sync_ctx[libfs_id]->peer->start_digest,
	//             digest_start_blknr);

	// digest log entries
	loghdr_meta = (loghdr_meta_t *)mlfs_alloc(sizeof(loghdr_meta_t));
	if (!loghdr_meta)
		panic("cannot allocate logheader\n");
	for (i = 0; i < n_hdrs; i++) {
		// START_TIMER(evt_rlh);
		memset(loghdr_meta, 0, sizeof(loghdr_meta_t));

		INIT_LIST_HEAD(&loghdr_meta->link);

		loghdr_meta->loghdr = &loghdr_buf[i];
		loghdr_meta->hdr_blkno = current_loghdr_blknr;
		loghdr_meta->is_hdr_allocated = 1;

		// DEBUG
		// mlfs_printf("%s", "--------------------------------\n");
		// mlfs_printf("i %d\n", i);
		// mlfs_printf("%d\n", loghdr_meta->loghdr->n);
		// mlfs_printf("ts %ld.%06ld\n", loghdr_meta->loghdr->mtime.tv_sec,
		//             loghdr_meta->loghdr->mtime.tv_usec);
		// mlfs_printf("loghdr_blknr %lu\n", loghdr_meta->hdr_blkno);
		// mlfs_printf("nr_blks %u\n", loghdr_meta->loghdr->nr_log_blocks);
		// mlfs_printf("inuse %x\n", loghdr_meta->loghdr->inuse);
		// for (int i = 0; i < loghdr_meta->loghdr->n; i++) {
		//         mlfs_printf("type[%d]: %u\n", i,
		//                  loghdr_meta->loghdr->type[i]);
		//         mlfs_printf("inum[%d]: %u\n", i,
		//                  loghdr_meta->loghdr->inode_no[i]);
		// }

		// END_TIMER(evt_rlh);

		if (loghdr_meta->loghdr->inuse != LH_COMMIT_MAGIC) {
			mlfs_printf("[ERROR] loghdr_meta->loghdr=%p "
				    "&loghdr_buf[%d]=%p\n",
				    loghdr_meta->loghdr, i, &loghdr_buf[i]);

			hexdump(loghdr_meta->loghdr, 4096);

			mlfs_assert(loghdr_meta->loghdr->inuse == 0);
			mlfs_free(loghdr_meta);
			panic("loghdr_meta->loghdr->inuse != LH_COMMIT_MAGIC");
			break;
		}

#ifdef DIGEST_OPT
		digest_replay_and_optimize(from_dev, loghdr_meta, &replay_list);
#else
		digest_each_log_entries(from_dev, libfs_id, loghdr_meta);
#endif
		current_loghdr_blknr += loghdr_meta->loghdr->nr_log_blocks;

	}
	mlfs_free(loghdr_meta);

	// END_TIMER(evt_coal);

#ifdef DIGEST_OPT
	if (mlfs_conf.digest_opt_host_memcpy) {
		digest_log_from_replay_list(from_dev, libfs_id, &replay_list,
					    memcpy_meta_list);
	} else {
		digest_log_from_replay_list(from_dev, libfs_id, &replay_list,
					    0);
	}
#endif

	n_digest = i;

	if (0) {
		ncx_slab_stat_t slab_stat;
		ncx_slab_stat(mlfs_slab_pool, &slab_stat);
	}

	return n_digest;
}

// NOTE There is no wrap-around because libfs's fetch request is divided on a
// wrap-around.
void build_memcpy_list(void *arg)
{
	START_TL_TIMER(evt_build_memcpy_list);

	struct build_memcpy_list_arg *bm_arg =
		(struct build_memcpy_list_arg *)arg;
	struct replication_context *rctx;
	int dev_id;
	int libfs_id;
	int sock_fd;
	char *response[MAX_SOCK_BUF];
	int lru_updated = 0;
	uint32_t digest_loghdr_cnt, digest_blk_cnt;
	uint32_t n_loghdr_digested;
	struct memcpy_meta_array *memcpy_meta_list;

	print_build_memcpy_list_arg(bm_arg);


	// TO be deleted.
	dev_id = g_log_dev; // all logs have dev_id == 4
	mlfs_assert(dev_id == 4);

	// START_MP_TOTAL_TIMER(evt_dig_req_total);
	// START_MP_PER_TASK_TIMER(evt_dig_req_per_task);
	// START_TIMER(evt_hdr);

	rctx = bm_arg->rctx;
	libfs_id = rctx->peer->id;
	sock_fd = g_peers[libfs_id]->sockfd[SOCK_BG];
	digest_loghdr_cnt = bm_arg->n_loghdrs;
	digest_blk_cnt = bm_arg->digest_blk_cnt;

	pr_digest("[PUBLISH] libfs_id=%d dev_id=%d digest_start=%lu "
		  "digest_loghdr_cnt=%u digest_blk_cnt=%u",
		  libfs_id, dev_id, bm_arg->fetch_start_blknr,
		  digest_loghdr_cnt, digest_blk_cnt);

	PR_METADATA(rctx);

	// update peer digesting state
	// set_peer_digesting(rctx->peer); // TODO do we need it?

	if (mlfs_conf.digest_opt_host_memcpy) {
		// It is freed after host memcpy is done.
		memcpy_meta_list = (struct memcpy_meta_array *)mlfs_zalloc(
			sizeof(struct memcpy_meta_array));

		/* Allocate digest_blk_cnt entries. It is the worst case from
		 * the perspective of memory usage because it includes other
		 * type of blocks like inode or unlink. Besides, the number of
		 * memcpy entries might decrease after coalescing. The exact
		 * number of memcpy is decided after mlfs_ext_get_blocks() call
		 * in digest_file(). So, we cannot know the exact number of
		 * memcopies in advance.
		 */
		memcpy_meta_list->id = 0;

		// Dynamically allocated buffer. Efficient memory usage but high
		// latency.  It is freed after host memcpy is done.
		memcpy_meta_list->buf = (memcpy_meta_t *)nic_slab_alloc_in_byte(
			sizeof(memcpy_meta_t) * digest_blk_cnt);

		// Buffer is allocated at initial time. Low latency but
		// inefficient memory usage.
		// mlfs_assert(digest_blk_cnt <= MAX_NUM_LOGHDRS_IN_LOG);
		// memcpy_meta_list->buf = rctx->memcpy_meta_list_buf;
	}

	START_TL_TIMER(evt_pub_lock_wait);
	pthread_mutex_lock(&g_publish_lock);
	END_TL_TIMER(evt_pub_lock_wait);

	START_TL_TIMER(evt_digest_logs);
	n_loghdr_digested =
		digest_logs(dev_id, libfs_id, digest_loghdr_cnt, bm_arg->fetch_start_blknr,
			    bm_arg->loghdr_buf, memcpy_meta_list);
	END_TL_TIMER(evt_digest_logs);

	pthread_mutex_unlock(&g_publish_lock);

	// At this point, we finished using of loghdr_buf in this pipeline
	// branch. It is freed in the pipeline_end stage as it gets notified by
	// the next replica.

	// Next pipeline stage.
	// Insert memcpy request(arg) to per-libfs circular buffer.
	host_memcpy_arg hm_arg = { 0 };
	hm_arg.libfs_id = libfs_id;
	hm_arg.seqn = bm_arg->seqn;
	hm_arg.array = memcpy_meta_list;
	hm_arg.fetch_loghdr_done_p = bm_arg->fetch_loghdr_done_p;
	hm_arg.fetch_log_done_p = bm_arg->fetch_log_done_p;
	hm_arg.loghdr_buf = bm_arg->loghdr_buf;
	hm_arg.log_buf = bm_arg->log_buf;
	hm_arg.digest_start_blknr = bm_arg->fetch_start_blknr;
	hm_arg.n_orig_loghdrs = bm_arg->n_orig_loghdrs;
	hm_arg.n_orig_blks = bm_arg->n_orig_blks;
	hm_arg.reset_meta =  bm_arg->reset_meta;
	atomic_init(&hm_arg.processed, 0);

#ifndef NO_PIPELINING
	enqueue_host_memcpy_req(&hm_arg);

	// Add new thread job: host_memcpy_request.
	thpool_add_work(rctx->thpool_host_memcpy_req, request_host_memcpy,
			(void *)rctx);
#endif

	mlfs_assert(n_loghdr_digested == digest_loghdr_cnt);

	mlfs_debug(
		"-- Total used block %d\n",
		bitmap_weight((uint64_t *)sb[g_root_dev]->s_blk_bitmap->bitmap,
			      sb[g_root_dev]->ondisk->ndatablocks));

#ifdef MIGRATION
	try_migrate_blocks(g_root_dev, g_ssd_dev, 0, 0, 1);
#endif
	// TODO to be moved to host memcpy worker.
	// flush writes to NVM (only relevant if writes are issued asynchronously using a DMA engine)
	// mlfs_commit(g_root_dev);

	// TODO to be deleted. For test waiting host memcpy done.
	// waiting hostmemcpy.

	START_TL_TIMER(evt_persist_dirty_nvm);
	persist_dirty_objects_nvm(libfs_id); // TODO OPTIMIZE batch update. How to sync cached DRAM
					     // data to NVM? JYKIM
	END_TL_TIMER(evt_persist_dirty_nvm);

#ifdef USE_SSD
	persist_dirty_objects_ssd();
#endif
	//#ifdef USE_HDD
	//		persist_dirty_objects_hdd();
	//#endif

#if MLFS_LEASE // NIC kernfs does not support LEASE yet. TODO JYKIM
	clear_lease_checkpoints(libfs_id, rctx->peer->start_version,
				rctx->peer->start_digest);
#endif

#if MLFS_LEASE
	/*
	sprintf(response, "|NOTIFY |%d|", 1);

	// notify any other local libfs instances
	for(int i=0; i<g_n_hot_rep; i++) {

		if(sockaddr_cmp((struct sockaddr *)&digest_arg->cli_addr,
				       (struct sockaddr *)&all_cli_addr[i])) {
			sendto(sock_fd, response, MAX_SOCK_BUF, 0,
				(struct sockaddr *)&all_cli_addr[i],
	sizeof(struct sockaddr_un));
		}
	}
	*/
#endif

#ifdef MLFS_PRINTF
	// show_storage_stats();
#endif

	// To confirm progress.
	// if (!(bm_arg->seqn % 500)) {
	// 	printf("Data publish done. libfs_id=%d seqn=%lu\n", libfs_id,
	// 	       bm_arg->seqn);
	// 	show_storage_stats();
	// }

	// END_MP_PER_TASK_TIMER(evt_dig_req_per_task);
	// END_MP_TOTAL_TIMER(evt_dig_req_total);
	// END_TIMER(evt_hdr);

	END_TL_TIMER(evt_build_memcpy_list);

#ifdef NO_PIPELINING
	request_host_memcpy_without_pipelining(&hm_arg);
#endif

	mlfs_free(arg);
}

static void print_build_memcpy_list_arg(build_memcpy_list_arg *ar)
{
	pr_pipe("ARG %-30s libfs_id=%d seqn=%lu digest_blk_cnt=%lu n_loghdrs=%u "
		"fetch_start_blknr=%lu loghdr_buf=%p log_buf=%p "
		"fetch_loghdr_done_p=%p fetch_log_done_p=%p n_orig_loghdrs=%u "
		"n_orig_blks=%lu reset_meta=%d",
		"build_memcpy_list", ar->rctx->peer->id, ar->seqn,
		ar->digest_blk_cnt, ar->n_loghdrs, ar->fetch_start_blknr,
		ar->loghdr_buf, ar->log_buf, ar->fetch_loghdr_done_p,
		ar->fetch_log_done_p, ar->n_orig_loghdrs, ar->n_orig_blks,
		ar->reset_meta);
}

void print_build_memcpy_list_thpool_stat(void)
{
#ifdef PROFILE_THPOOL
	// Add here...
#endif
}

/** Print functions registered to each thread. **/
static void print_build_memcpy_list_stat(void *arg)
{

	PRINT_TL_TIMER(evt_build_memcpy_list, arg);
	// PRINT_TL_TIMER(evt_part1, arg);
	// PRINT_TL_TIMER(evt_pub_lock_wait, arg);
	PRINT_TL_TIMER(evt_digest_logs, arg);
	PRINT_TL_TIMER(evt_persist_dirty_nvm, arg);
	// PRINT_TL_TIMER(evt_part3, arg);

	RESET_TL_TIMER(evt_build_memcpy_list);
	RESET_TL_TIMER(evt_digest_logs);
	RESET_TL_TIMER(evt_persist_dirty_nvm);
	RESET_TL_TIMER(evt_part1);
	RESET_TL_TIMER(evt_pub_lock_wait);
	RESET_TL_TIMER(evt_part3);
}

void print_all_thpool_build_memcpy_list_stats(void)
{
	for (int i = g_n_nodes; i < g_n_nodes + MAX_LIBFS_PROCESSES; i++) {
		if (g_sync_ctx[i]) {
			// print_per_thread_pipeline_stat(tp);
			printf("libfs_%d\n", i);
			thpool_add_work(g_sync_ctx[i]->thpool_build_memcpy_list,
					print_build_memcpy_list_stat,
					0); // 1 thread per libfs.
			usleep(10000); // To print in order.
		}
	}
}
