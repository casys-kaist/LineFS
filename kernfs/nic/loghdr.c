#include "mlfs/mlfs_user.h"
#include "loghdr.h"
#include "build_memcpy_list.h"
#include "storage/storage.h" // nic_slab
#include "distributed/rpc_interface.h"
#include "global/util.h"

threadpool thpool_loghdr_build; // primary.
threadpool thpool_loghdr_fetch; // replica 1.

TL_EVENT_TIMER(evt_build_loghdr);
TL_EVENT_TIMER(evt_fetch_loghdr);

threadpool init_loghdr_build_thpool(void)
{
	int th_num = mlfs_conf.thread_num_loghdr_build;
	char th_name[] = "hdr_build";

	thpool_loghdr_build = thpool_init(th_num, th_name);

	print_thread_init(th_num, th_name);

	return thpool_loghdr_build;
}

threadpool init_loghdr_fetch_thpool(void)
{
	int th_num = mlfs_conf.thread_num_loghdr_fetch;
	char th_name[] = "hdr_fetch";

	thpool_loghdr_fetch = thpool_init(th_num, th_name);

	print_thread_init(th_num, th_name);

	return thpool_loghdr_fetch;
}

void build_loghdr_list(void *arg)
{
	START_TL_TIMER(evt_build_loghdr);

	struct build_loghdrs_arg *bl_arg = (struct build_loghdrs_arg *)arg;
	struct replication_context *rctx;
	struct logheader *loghdr_p;
	struct logheader *loghdr_buf;
	char *new_loghdr;
	uint64_t cnt = 0;
	uint64_t num_blks;
	uint64_t log_size;
	uintptr_t buf_exceed;
	uint32_t i;

	print_build_loghdrs_arg(bl_arg);

	rctx = bl_arg->rctx;
	loghdr_p = (struct logheader *)bl_arg->log_buf;
	log_size = bl_arg->log_size;

	pr_loghdrs("Start building loghdr list: libfs=%d log_buf=%p "
		   "log_size=%lu(%lu)", rctx->peer->id,
		   loghdr_p, log_size, log_size >> g_block_size_shift);

	loghdr_buf = (struct logheader *)nic_slab_alloc_in_byte(
		sizeof(struct logheader) * bl_arg->n_loghdrs);
#ifdef CHECK_LOGHDR_LIST
	// zeroing buffer.
	memset(loghdr_buf, -1, sizeof(struct logheader) * bl_arg->n_loghdrs);
#endif

	pr_loghdrs("loghdr_buf allocated: %p - 0x%lx", loghdr_buf,
		   (uintptr_t)loghdr_buf +
			   (sizeof(struct logheader) * bl_arg->n_loghdrs));

	buf_exceed = (uintptr_t)(bl_arg->log_buf + log_size);

	for (i = 0; i < bl_arg->n_loghdrs; i++) {
		mlfs_assert((uintptr_t)loghdr_p < buf_exceed);

		if (loghdr_p->inuse != LH_COMMIT_MAGIC) {
			uint64_t blk_cnt = 0;
			struct logheader *prev_loghdr_p = 0;

			blk_cnt =
				((uintptr_t)loghdr_p - (uintptr_t)loghdr_buf) >>
				g_block_size_shift;

			if (cnt == 0)
				prev_loghdr_p = 0;
			else
				prev_loghdr_p =
					(struct logheader
						 *)(((char *)loghdr_p) -
						    (num_blks
						     << g_block_size_shift));

			mlfs_printf("[Error] libfs=%d prev_loghdr_p=%p "
				    "cur_loghdr_p=%p loghdr_buf=%p log_buf=%p "
				    "log_size=%lu hdr_cnt=%lu "
				    "blk_cnt=%lu/%lu\n",
				    rctx->peer->id, prev_loghdr_p, loghdr_p,
				    loghdr_buf, bl_arg->log_buf, log_size, cnt,
				    blk_cnt, log_size >> g_block_size_shift);
			panic("loghdr->inuse != LH_COMMIT_MAGIC");
		}

		// DEBUG
		/*
		pr_loghdrs("Copy log header: dst(&loghdr_buf[%u])=%p src=%p "
			   "size=%lu",
			   i, &loghdr_buf[i], loghdr_p,
			   sizeof(struct logheader));
			   */

		memcpy(&loghdr_buf[i], loghdr_p, sizeof(struct logheader));
		cnt++;

		// Set loghdr_p point to the next loghdr.
		num_blks = (uint64_t)loghdr_p->nr_log_blocks;
		new_loghdr =
			((char *)loghdr_p) + (num_blks << g_block_size_shift);
		loghdr_p = (struct logheader *)new_loghdr;
	}

#ifdef CHECK_LOGHDR_LIST
	check_loghdr_list(loghdr_buf, bl_arg->n_loghdrs);
#endif

	mlfs_assert(cnt == bl_arg->n_loghdrs);
	mlfs_assert((uintptr_t)loghdr_p == buf_exceed);

	/*
	for (i = 0; i < bl_arg->n_loghdrs; i++) {
		mlfs_printf("loghdr_buf[%d](%p) n=%hhu nr_log_blks=%hu inuse=%hu\n", i,
		       &loghdr_buf[i], loghdr_buf[i].n,
		       loghdr_buf[i].nr_log_blocks, loghdr_buf[i].inuse);
	}
	*/

	pr_loghdrs("Built loghdr_list: log_start=%p log_size=%lu(%lu) "
		   "loghdr_start=%p hdr_cnt=%lu",
		   bl_arg->log_buf, log_size, log_size >> g_block_size_shift,
		   loghdr_buf, cnt);

	// Allocate loghdr_buf flag to free buffer asynchronously.
	uint64_t *fetch_loghdr_done_p =
		(uint64_t *)nic_slab_alloc_in_byte(sizeof(uint64_t));
	*fetch_loghdr_done_p = 0;

	uint64_t digest_blk_cnt = log_size >> g_block_size_shift;

	// Next pipeline stage build_memcpy_list.
	// 1. build host memcpy list.
	build_memcpy_list_arg *bm_arg = (build_memcpy_list_arg *)mlfs_zalloc(
		sizeof(build_memcpy_list_arg));
	bm_arg->seqn = bl_arg->seqn;
	bm_arg->rctx = rctx;
	bm_arg->digest_blk_cnt = digest_blk_cnt;
	bm_arg->n_loghdrs = bl_arg->n_loghdrs;
	bm_arg->fetch_start_blknr = bl_arg->fetch_start_blknr;
	bm_arg->loghdr_buf = loghdr_buf;
	bm_arg->log_buf = bl_arg->log_buf;
	bm_arg->fetch_loghdr_done_p = fetch_loghdr_done_p;
	bm_arg->fetch_log_done_p = bl_arg->fetch_log_done_p;
	bm_arg->n_orig_loghdrs = bl_arg->n_orig_loghdrs;
	bm_arg->n_orig_blks = bl_arg->n_orig_blks;
	bm_arg->reset_meta = bl_arg->reset_meta;

#ifdef NO_PIPELINING
	// 2. Send an RPC to replica 1.
	send_fetch_loghdr_rpc_to_next_replica(
		rctx, bl_arg->seqn, (uintptr_t)loghdr_buf, bl_arg->n_loghdrs,
		digest_blk_cnt, bl_arg->fetch_start_blknr,
		(uintptr_t)fetch_loghdr_done_p);

	END_TL_TIMER(evt_build_loghdr);

	build_memcpy_list((void *)bm_arg);
#else
	thpool_add_work(rctx->thpool_build_memcpy_list, build_memcpy_list,
			(void *)bm_arg);

	// 2. Send an RPC to replica 1.
	send_fetch_loghdr_rpc_to_next_replica(
		rctx, bl_arg->seqn, (uintptr_t)loghdr_buf, bl_arg->n_loghdrs,
		digest_blk_cnt, bl_arg->fetch_start_blknr,
		(uintptr_t)fetch_loghdr_done_p);

	END_TL_TIMER(evt_build_loghdr);
#endif
	mlfs_free(arg);
}

// For sanity check.
int check_loghdrs_in_log(struct logheader *buf, uint32_t n_loghdrs)
{
	uint64_t i;
	struct logheader *loghdr_p;
	uint64_t num_blks;
	char *new_loghdr;

	loghdr_p = buf;

	for (i = 0; i < n_loghdrs; i++) {
		if (loghdr_p->inuse != LH_COMMIT_MAGIC) {
			uint64_t blk_cnt = 0;
			struct logheader *prev_loghdr_p = 0;

			blk_cnt = ((uintptr_t)loghdr_p - (uintptr_t)buf) >>
				  g_block_size_shift;

			if (i == 0)
				prev_loghdr_p = 0;
			else
				prev_loghdr_p =
					(struct logheader
						 *)(((char *)loghdr_p) -
						    (num_blks
						     << g_block_size_shift));

			mlfs_printf("[Error] prev_loghdr_p=%p "
				    "cur_loghdr_p=%p loghdr_buf=%p "
				    "hdr_cnt=%lu blk_cnt=%lu\n",
				    prev_loghdr_p, loghdr_p, buf, i, blk_cnt);
			panic("loghdr->inuse != LH_COMMIT_MAGIC");
		}

		num_blks = (uint64_t)loghdr_p->nr_log_blocks;
		new_loghdr =
			((char *)loghdr_p) + (num_blks << g_block_size_shift);
		loghdr_p = (struct logheader *)new_loghdr;
	}

	return 1;
}

/**
 * @brief Check loghdr types are correct.
 *
 * @param loghdr
 * @return int Return 1 if all types of the loghdr are correct. Otherwise, 0.
 */
static int check_loghdr_types(struct logheader *loghdr)
{
	uint16_t nr_entries = loghdr->n;

	for (int i = 0; i < nr_entries; i++) {
		switch (loghdr->type[i]) {
		case L_TYPE_INODE_CREATE:
		case L_TYPE_INODE_UPDATE:
		case L_TYPE_DIR_DEL:
		case L_TYPE_DIR_RENAME:
		case L_TYPE_DIR_ADD:
		case L_TYPE_FILE:
		case L_TYPE_UNLINK:
			return 1;
		default:
			return 0;
		}
	}
}

static void print_loghdr_error(struct logheader *buf, uint64_t i, uint64_t num_blks)
{
	uint64_t blk_cnt = 0;
	struct logheader *prev_loghdr_p = 0;

	blk_cnt = ((uintptr_t)&buf[i] - (uintptr_t)buf) >> g_block_size_shift;

	if (i == 0)
		prev_loghdr_p = 0;
	else
		prev_loghdr_p =
			(struct logheader *)(((char *)&buf[i]) -
					     (num_blks << g_block_size_shift));

	mlfs_printf("[Error] prev_loghdr_p=%p "
		    "cur_loghdr_p=%p loghdr_buf=%p "
		    "hdr_cnt=%lu blk_cnt=%lu\n",
		    prev_loghdr_p, &buf[i], buf, i, blk_cnt);

	hexdump(&buf[i], sizeof(struct logheader));
}

static void print_loghdr(struct logheader *buf, uint64_t i)
{
	// DEBUG
	mlfs_printf("%s", "++++++++++++++++++++++++++++++++\n");
	mlfs_printf("i %lu\n", i);
	mlfs_printf("%d\n", buf[i].n);
	mlfs_printf("ts %ld.%06ld\n", buf[i].mtime.tv_sec,
		    buf[i].mtime.tv_usec);
	mlfs_printf("nr_blks %u\n", buf[i].nr_log_blocks);
	mlfs_printf("inuse %x\n", buf[i].inuse);
	for (int j = 0; j < buf[i].n; j++) {
		mlfs_printf("type[%d]: %u\n", j, buf[i].type[j]);
		mlfs_printf("inum[%d]: %u\n", j, buf[i].inode_no[j]);
	}
}

static int check_loghdr_list(struct logheader *buf, uint32_t n_loghdrs)
{
	uint64_t i;
	uint64_t num_blks = 0;

	for (i = 0; i < n_loghdrs; i++) {
		print_loghdr(buf, i);
		if (buf[i].inuse != LH_COMMIT_MAGIC) {
			print_loghdr_error(buf, i, num_blks);
			print_loghdr(buf, i);
			panic("[CHECK_LOGHDR] loghdr->inuse != "
			      "LH_COMMIT_MAGIC");
		}

		if (!check_loghdr_types(&buf[i])) {
			print_loghdr_error(buf, i, num_blks);
			print_loghdr(buf, i);
			panic("[CHECK_LOGHDR] unsupported type of operation\n");
		}

		num_blks = (uint64_t)buf[i].nr_log_blocks;
	}

	return 1;
}

void fetch_loghdrs(void *arg)
{
	START_TL_TIMER(evt_fetch_loghdr);

	struct logheader *loghdr_buf;
	rdma_meta_t *loghdr_meta;
	int prev_kernfs_id, sock;
	struct replication_context *rctx;
	int is_last;
	uint64_t *fetch_loghdr_done_p;
	uint32_t wr_id;
	fetch_loghdrs_arg *fl_arg = (fetch_loghdrs_arg *)arg;

	rctx = g_sync_ctx[fl_arg->libfs_id];

	print_fetch_loghdrs_arg(fl_arg);

	is_last = is_last_kernfs(fl_arg->libfs_id, g_kernfs_id);

	loghdr_buf = (struct logheader *)nic_slab_alloc_in_byte(
		sizeof(struct logheader) * fl_arg->n_loghdrs);

#ifdef CHECK_LOGHDR_LIST
	// zeroing buffer.
	memset(loghdr_buf, -1, sizeof(struct logheader) * fl_arg->n_loghdrs);
#endif

	pr_loghdrs("FETCH_LOGHDR libfs=%d n_hdrs=%u remote_addr(src)=0x%lx "
		   "local_addr(dest)=%p",
		   fl_arg->libfs_id, fl_arg->n_loghdrs, fl_arg->remote_loghdr_buf,
		   loghdr_buf);

	// Fetch loghdrs.
	loghdr_meta =
		create_rdma_meta((uintptr_t)loghdr_buf,
				 fl_arg->remote_loghdr_buf,
				 sizeof(struct logheader) * fl_arg->n_loghdrs);

	prev_kernfs_id = get_prev_kernfs(g_kernfs_id);
	sock = g_peers[prev_kernfs_id]->sockfd[SOCK_BG];

	IBV_WRAPPER_READ_SYNC(sock, loghdr_meta, MR_DRAM_BUFFER,
			       MR_DRAM_BUFFER);

	mlfs_free(loghdr_meta);

#ifdef CHECK_LOGHDR_LIST
	check_loghdr_list(loghdr_buf, fl_arg->n_loghdrs);
#endif

	// Set flag to free log header buf.
	set_fetch_done_flag(fl_arg->libfs_id, fl_arg->seqn,
			    fl_arg->fetch_loghdr_done_addr, sock);

	if (!is_last) {
		// Allocate loghdr_buf flags to free buffer asynchronously.
		fetch_loghdr_done_p =
			(uint64_t *)nic_slab_alloc_in_byte(sizeof(uint64_t));
		*fetch_loghdr_done_p = 0;
	}

	// Next pipeline stage.
	// 1. build host memcpy list.
	build_memcpy_list_arg *bm_arg = (build_memcpy_list_arg *)mlfs_zalloc(
		sizeof(build_memcpy_list_arg));
	bm_arg->seqn = fl_arg->seqn;
	bm_arg->rctx = rctx;
	bm_arg->digest_blk_cnt = fl_arg->digest_blk_cnt;
	bm_arg->n_loghdrs = fl_arg->n_loghdrs;
	bm_arg->loghdr_buf = loghdr_buf;
	bm_arg->log_buf = 0; // Not used in Replica 1.
	bm_arg->fetch_start_blknr = fl_arg->fetch_start_blknr;
	bm_arg->fetch_loghdr_done_p = fetch_loghdr_done_p;
	bm_arg->fetch_log_done_p = 0; // Not used in Replica 1.

	// Not used in Replica 1.
	bm_arg->n_orig_loghdrs = 0;
	bm_arg->n_orig_blks = 0;
	bm_arg->reset_meta = 0;

#ifndef NO_PIPELINING
	thpool_add_work(rctx->thpool_build_memcpy_list, build_memcpy_list,
			(void *)bm_arg);
#endif

	if (!is_last) {
		// 2. Send an RPC to the last replica (Replica 2).
		send_fetch_loghdr_rpc_to_next_replica(
			rctx, fl_arg->seqn, (uintptr_t)loghdr_buf,
			fl_arg->n_loghdrs, fl_arg->digest_blk_cnt,
			fl_arg->fetch_start_blknr,
			(uintptr_t)fetch_loghdr_done_p);
	}


	END_TL_TIMER(evt_fetch_loghdr);
#ifdef NO_PIPELINING
	build_memcpy_list(bm_arg);
#endif

	mlfs_free(arg);
}

static void send_fetch_loghdr_rpc_to_next_replica(
	struct replication_context *rctx, uint64_t seqn, uintptr_t loghdr_buf,
	uint32_t n_loghdrs, uint64_t digest_blk_cnt, addr_t fetch_start_blknr,
	uintptr_t fetch_loghdr_done_addr)
{
	char msg[MAX_SIGNAL_BUF];
	int libfs_id = rctx->peer->id;
	sprintf(msg,
		"|" TO_STR(RPC_FETCH_LOGHDR) " |%d|%lu|%lu|%u|%lu|%lu|%lu|",
		libfs_id, seqn, loghdr_buf, n_loghdrs, digest_blk_cnt,
		fetch_start_blknr, fetch_loghdr_done_addr);

	// No need to wait resp. Send it without seqn.
	// TODO Use unsignaled for better performance.
	rpc_forward_msg_no_seqn(rctx->next_digest_sockfd, msg);
}

static void print_build_loghdrs_arg(build_loghdrs_arg *ar)
{
	pr_pipe("ARG %-30s libfs_id=%d seqn=%lu log_buf=%p log_size=%lu "
		"n_loghdrs=%u fetch_start_blknr=%lu fetch_log_done_p=%p "
		"n_orig_loghdrs=%u n_orig_blks=%lu reset_meta=%d",
		"build_loghdrs", ar->rctx->peer->id, ar->seqn, ar->log_buf,
		ar->log_size, ar->n_loghdrs, ar->fetch_start_blknr,
		ar->fetch_log_done_p, ar->n_orig_loghdrs, ar->n_orig_blks,
		ar->reset_meta);
}

static void print_fetch_loghdrs_arg(fetch_loghdrs_arg *ar)
{
	pr_pipe("ARG %-30s libfs_id=%d seqn=%lu n_loghdrs=%u digest_blk_cnt=%lu "
		"remote_loghdr_buf=0x%lx fetch_loghdr_done_addr=0x%lx "
		"fetch_start_blknr=%lu",
		"fetch_loghdrs", ar->libfs_id, ar->seqn, ar->n_loghdrs,
		ar->digest_blk_cnt, ar->remote_loghdr_buf,
		ar->fetch_loghdr_done_addr, ar->fetch_start_blknr);
}

void print_loghdr_thpool_stat(void)
{
#ifdef PROFILE_THPOOL
	print_profile_result(thpool_loghdr_build);
	print_profile_result(thpool_loghdr_fetch);
#endif
}

/** Print functions registered to each thread. **/
void print_loghdr_build_stat(void *arg)
{
	PRINT_TL_TIMER(evt_build_loghdr, arg);
	RESET_TL_TIMER(evt_build_loghdr);
}

void print_loghdr_fetch_stat(void *arg)
{
	PRINT_TL_TIMER(evt_fetch_loghdr, arg);
	RESET_TL_TIMER(evt_fetch_loghdr);
}

PRINT_ALL_PIPELINE_STAT_FUNC(loghdr_build)
PRINT_ALL_PIPELINE_STAT_FUNC(loghdr_fetch)
