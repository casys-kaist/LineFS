#include "mlfs/mlfs_user.h"
#include "host_memcpy.h"
#include "global/util.h"
#include "distributed/peer.h"
#include "distributed/rpc_interface.h"
#include "io/device.h"
#include "storage/storage.h"
#include "ds/asm-generic/barrier.h"
// #ifdef CHECK_SANITY_MEMCPY_LIST_BUF
// #include "build_memcpy_list.h"
// #endif

// It is per-libfs threadpool. g_sync_ctx[]->thpool_host_memcpy.
#ifdef PROFILE_REALTIME_MEMCPY_BW
rt_bw_stat memcpy_bw_stat = {0};
#endif

// Thread pool of the next pipeline stage.
threadpool thpool_pipeline_end = NULL;

TL_EVENT_TIMER(evt_host_memcpy_wait_req);
TL_EVENT_TIMER(evt_host_memcpy_wait_copy_done);
TL_EVENT_TIMER(evt_host_memcpy_req);
TL_EVENT_TIMER(evt_end_pipeline);
TL_EVENT_TIMER(evt_enqueue_mcpy_req);
TL_EVENT_TIMER(evt_enqueue_copy_done);

circ_buf_stat host_memcpy_cb_stats[g_n_nodes + MAX_LIBFS_PROCESSES];
circ_buf_stat copy_done_cb_stats[g_n_nodes + MAX_LIBFS_PROCESSES];
uintptr_t host_memcpy_buf_addrs[g_n_nodes + MAX_LIBFS_PROCESSES];

// called on every libfs registration.
threadpool init_host_memcpy_thpool(int libfs_id)
{
	int th_num = 1;
	char tp_name[64] = { 0 };
	threadpool ret;
	sprintf(tp_name, "h_mcpy_req%d", libfs_id);
	ret = thpool_init(th_num, tp_name); // per-libfs thread.

	print_thread_init(th_num, tp_name);

#ifdef PROFILE_CIRCBUF
	init_cb_stats(host_memcpy_cb_stats);
	init_cb_stats(copy_done_cb_stats);
#endif
	if (!thpool_pipeline_end) {
		char th_name[] = "pipeline_end";
		thpool_pipeline_end = thpool_init(mlfs_conf.thread_num_end_pipeline, th_name);

		print_thread_init(th_num, th_name);
	}

	return ret;
}

void init_host_memcpy(void)
{
#ifdef PROFILE_REALTIME_MEMCPY_BW
	init_rt_bw_stat(&memcpy_bw_stat, "rdma_memcpy");
#endif
}

// Host gives NIC a starting address of host memcpy meta buffers which are
// allocated contiguously.
void register_host_memcpy_buf_addrs(uintptr_t buf_start_addr)
{
	memcpy_meta_t *memcpy_meta_p = (memcpy_meta_t *)buf_start_addr;

	host_memcpy_buf_addrs[0] = buf_start_addr;
	// printf("host_memcpy_buf_addr[0]=0x%lx\n", host_memcpy_buf_addrs[0]);
	for (int i = 1; i < g_n_nodes + MAX_LIBFS_PROCESSES; i++) {
		memcpy_meta_p = (memcpy_meta_t *)host_memcpy_buf_addrs[i-1];
		memcpy_meta_p += g_memcpy_meta_cnt; // pointer addition.
		host_memcpy_buf_addrs[i] = (uintptr_t)memcpy_meta_p;
		// printf("host_memcpy_buf_addr[%d]=0x%lx\n", i,
		//        host_memcpy_buf_addrs[i]);
	}
}

static int send_memcpy_req_to_host(void *arg)
{
	host_memcpy_arg *hm_arg = (host_memcpy_arg *)arg;
	char msg[MAX_SOCK_BUF];
	int sockfd;
	uint64_t size;
	uint64_t *ack_bit_p;
	rdma_meta_t *meta;

	ack_bit_p = rpc_alloc_ack_bit();

	// START_TIMER(evt_dc);

	size = hm_arg->array->id; // id stores size of array. (the number of
				  // replay list entries)

	sockfd = g_kernfs_peers[nic_kid_to_host_kid(g_kernfs_id)]
			 ->sockfd[SOCK_BG];

	// Check max size.
	mlfs_assert(size <= g_memcpy_meta_cnt);

	// Send loghdrs vis RDMA write.
	// (RDMA write by NIC to reduce host CPU usage)
	meta = create_rdma_meta((uintptr_t)hm_arg->array->buf,
				host_memcpy_buf_addrs[hm_arg->libfs_id],
				sizeof(memcpy_meta_t) * size);
	IBV_WRAPPER_WRITE_ASYNC(sockfd, meta, MR_DRAM_BUFFER, MR_DRAM_BUFFER);

	sprintf(msg,
		"|" TO_STR(RPC_MEMCPY_REQ) " |%d|%lu|%lu|%lu|%u|%lu|%d|%lu|",
		hm_arg->libfs_id, hm_arg->seqn, size,
		hm_arg->digest_start_blknr, hm_arg->n_orig_loghdrs,
		hm_arg->n_orig_blks, hm_arg->reset_meta, (uintptr_t)ack_bit_p);
	// sprintf(msg,
	//         "|" TO_STR(RPC_MEMCPY_REQ) " |%d|%lu|%lu|%lu|%lu|%u|%lu|%d|%lu|",
	//         hm_arg->libfs_id, hm_arg->seqn, size,
	//         (uintptr_t)hm_arg->array->buf, hm_arg->digest_start_blknr,
	//         hm_arg->n_orig_loghdrs, hm_arg->n_orig_blks, hm_arg->last,
	//         (uintptr_t)ack_bit_p);
#ifndef DIGEST_MEMCPY_NOOP
	rpc_forward_msg_with_per_libfs_seqn(sockfd, msg, hm_arg->libfs_id);

#ifdef BACKUP_RDMA_MEMCPY
	// Check heartbeat. If host is dead, return and do RDMA memcpy instead.
	// Send rpc and wait here for a seconds.
	while(1) {
		if (rpc_check_ack_bit(ack_bit_p)) {
			rpc_free_ack_bit(ack_bit_p);
			break;
		} else if (!host_alive()) {
			printf("Host not responding.\n");
			return -1;
		}
		cpu_relax();
	}
#else
	// TODO OPTIMIZE do not wait it. We can wait it at end_pipeline().
	rpc_wait_ack(ack_bit_p);
#endif
	pr_pipe("Finish waiting host memcpy done. libfs_id=%d seqn=%lu",
		hm_arg->libfs_id, hm_arg->seqn);
#endif

#ifdef CHECK_SANITY_MEMCPY_LIST_BUF
	// Zeroing memcpy_list_buf.
	memset(hm_arg->array->buf, 0, sizeof(memcpy_meta_t) * size);
	// memset(hm_arg.array->buf, 0,
	// sizeof(memcpy_meta_t) * MAX_NUM_LOGHDRS_IN_LOG);
#endif
	nic_slab_free(hm_arg->array->buf);
	mlfs_free(hm_arg->array);
	mlfs_free(meta);
	// END_TIMER(evt_dc);

	return 1;
}

static void send_batched_memcpy_req_to_host(struct replication_context *rctx)
{
	memcpy_list_batch_meta_t *batch_meta;
	char msg[MAX_SOCK_BUF];
	int sockfd, libfs_id;
	uint64_t size;
	uint64_t *ack_bit_p;
	rdma_meta_t *r_meta;

	// START_TIMER(evt_dc);
	batch_meta = &rctx->mcpy_list_batch_meta;
	r_meta = batch_meta->r_meta;

	libfs_id = rctx->peer->id;
	size = r_meta->length / sizeof(memcpy_meta_t); // the number of total
						       // memcpy_list entry.
	sockfd = g_kernfs_peers[nic_kid_to_host_kid(g_kernfs_id)]
			 ->sockfd[SOCK_BG];
	ack_bit_p = rpc_alloc_ack_bit();

	// Check max size.
	mlfs_assert(size <= g_memcpy_meta_cnt);

	// Set remote destination address.
	set_rdma_meta_dst(r_meta, host_memcpy_buf_addrs[libfs_id]);

	// Send loghdrs vis RDMA write.
	IBV_WRAPPER_WRITE_ASYNC(sockfd, r_meta, MR_DRAM_BUFFER, MR_DRAM_BUFFER);

	sprintf(msg,
		"|" TO_STR(RPC_MEMCPY_REQ) " |%d|%lu|%lu|%lu|%u|%lu|%d|%lu|%d|",
		libfs_id, batch_meta->first_seqn, size,
		batch_meta->digest_start_blknr, batch_meta->n_orig_loghdrs,
		batch_meta->n_orig_blks, batch_meta->reset_meta, (uintptr_t)ack_bit_p,
		r_meta->sge_count);

#ifndef DIGEST_MEMCPY_NOOP
	rpc_forward_msg_with_per_libfs_seqn(sockfd, msg, libfs_id);

	// TODO OPTIMIZE do not wait it. We can wait it at end_pipeline().
	rpc_wait_ack(ack_bit_p);
	pr_pipe("Finish waiting host memcpy done. libfs_id=%d seqn=%lu cnt=%d",
		libfs_id, batch_meta->first_seqn, r_meta->sge_count);
#endif

#ifdef CHECK_SANITY_MEMCPY_LIST_BUF
	// Zeroing memcpy_list_buf.
	for (int i = 0; i < r_meta->sge_count; i++) {
		memset((void *)r_meta->sge_entries[i].addr, 0,
		       r_meta->sge_entries[i].length);
	}
#endif
	// Free memcpy list buffers.
	for (int i = 0; i < r_meta->sge_count; i++) {
		nic_slab_free((void *)r_meta->sge_entries[i].addr);
	}

	// Reset rdma meta.
	reset_sge_rdma_meta(r_meta);

	// END_TIMER(evt_dc);
}

// Update primary's variable via RDMA. The variable is to limit request rate.
//static void update_complete_seqn(int libfs_id, uint64_t seqn, uintptr_t target_addr)
//{
//	uint64_t *seqn_buf;
//	rdma_meta_t *meta;
//	int sockfd, prev_kernfs_id;
//
//	seqn_buf = (uint64_t *)nic_slab_alloc_in_byte(sizeof(uint64_t));
//	*seqn_buf = seqn;
//
//	pr_pipe("Set primary's seqn_compl to %lu", seqn);
//
//	meta = create_rdma_meta((uintptr_t)seqn_buf, target_addr,
//				sizeof(uint64_t));
//
//	// sockfd = g_peers[libfs_id]->sockfd[SOCK_IO]; // TODO use SOCK_BG
//	prev_kernfs_id = get_prev_kernfs(g_kernfs_id);
//	sockfd = g_peers[prev_kernfs_id]->sockfd[SOCK_BG];
//	IBV_WRAPPER_WRITE_ASYNC(sockfd, meta, MR_DRAM_BUFFER, MR_DRAM_BUFFER);
//
//	nic_slab_free(seqn_buf);
//	mlfs_free(meta); // TODO Is it okay to free after ASYNC ?
//}
// Update previous machine's variable via RDMA.

/**
 * @Synopsis  Free allocated buffers.
 *
 * @Param arg
 */
static void end_pipeline(void *arg)
{
	START_TL_TIMER(evt_end_pipeline);

	pipeline_end_arg *pe_arg = (pipeline_end_arg *)arg;

	print_pipeline_end_arg(pe_arg);

	// Update primary's seqn_completed_in_next_replica.
	// Currently, feedback is only between replica 1 and primary because
	// replica 1 is bottleneck.
//	if (is_middle_in_rep_chain(pe_arg->libfs_id, g_kernfs_id))
//		update_complete_seqn(pe_arg->libfs_id, pe_arg->seqn,
//				     g_sync_ctx[pe_arg->libfs_id]
//					     ->seqn_completed_update_addr);

	//// Free log header buffer.
	// Wait until flag is set to 1 by the next replica.
	pr_pipe("Waiting fetch_loghdr_done_p=%p(%lu) libfs_id=%d seqn=%lu",
		pe_arg->fetch_loghdr_done_p, *pe_arg->fetch_loghdr_done_p,
		pe_arg->libfs_id, pe_arg->seqn);
	while (!(*pe_arg->fetch_loghdr_done_p)) {
		cpu_relax();
	}

	pr_pipe("Freeing loghdr_buf=%p libfs_id=%d seqn=%lu",
		    pe_arg->loghdr_buf, pe_arg->libfs_id, pe_arg->seqn);

	nic_slab_free(pe_arg->loghdr_buf);
	// Free flag.
	nic_slab_free(pe_arg->fetch_loghdr_done_p);

	//// Free log buffer.
	if (is_first_kernfs(pe_arg->libfs_id, g_kernfs_id)) {

		// Wait until flag is set to 1 by the next replica.
		pr_pipe("Start Waiting fetch_log_done_p=%p(%lu) libfs_id=%d "
			    "seqn=%lu",
			    pe_arg->fetch_log_done_p, *pe_arg->fetch_log_done_p,
			    pe_arg->libfs_id, pe_arg->seqn);
		while (!(*pe_arg->fetch_log_done_p)) {
			cpu_relax();
		}
		// mlfs_printf("Finish Waiting fetch_log_done_p=%p(%lu) libfs_id=%d "
		//             "seqn=%lu\n",
		//             pe_arg->fetch_log_done_p, *pe_arg->fetch_log_done_p,
		//             pe_arg->libfs_id, pe_arg->seqn);

		// Free log_buf
#ifdef COMPRESS_LOG
		// The compress output buffer address is stored at
		// fetch_log_done_p via RDMA write by Replica 1.
		char *compress_out_buf;
		compress_out_buf = (char *)(*pe_arg->fetch_log_done_p);

		// compress_out_buf == 1 on fsync. Otherwise, it is an address.
		if ((uintptr_t)compress_out_buf > 1) {
			// mlfs_printf("Freeing compress_out_buf=%p libfs_id=%d seqn=%lu\n",
			//                 (char *)(*pe_arg->fetch_log_done_p), pe_arg->libfs_id,
			//                 pe_arg->seqn);
			nic_slab_free(compress_out_buf);
		}
#endif
		pr_pipe("Freeing log_buf=%p libfs_id=%d seqn=%lu",
			    pe_arg->log_buf, pe_arg->libfs_id, pe_arg->seqn);
		nic_slab_free(pe_arg->log_buf);

		// Free flag.
		nic_slab_free(pe_arg->fetch_log_done_p);
	}

	mlfs_free(arg);

	END_TL_TIMER(evt_end_pipeline);
}

/**
 * @Synopsis  Enqueue one host memcpy request entry to the circular buffer.
 *
 * @Param hm_arg
 */
void enqueue_host_memcpy_req(host_memcpy_arg *hm_arg)
{
	struct replication_context *rctx = g_sync_ctx[hm_arg->libfs_id];
	host_memcpy_circbuf *buffer = &rctx->host_memcpy_req_buf;

	print_host_memcpy_arg(hm_arg);

	pthread_mutex_lock(&buffer->produce_mutex);

	unsigned long head = buffer->head;
	/* The spin_unlock() and next spin_lock() provide needed ordering. */
	unsigned long tail = READ_ONCE(buffer->tail);

	// mlfs_printf("Enqueuing host_memcpy_req: libfs=%d seqn=%lu\n", hm_arg->libfs_id, hm_arg->seqn);
	START_TL_TIMER(evt_enqueue_mcpy_req);

	while (1) {
		if (CIRC_SPACE(head, tail, CIRC_BUF_HOST_MEMCPY_REQ_SIZE) >=
		    1) {
			/* insert one item into the buffer */
			host_memcpy_arg *item = &buffer->buf[head];
			memcpy(item, hm_arg, sizeof(host_memcpy_arg));
#ifdef PRINT_CIRCBUF
			mlfs_printf("HOST_MEMCPY_CIRC_BUF Add item libfs=%d "
				    "seqn=%lu processed=%d(%p) item=%p "
				    "tail=%lu\n",
				    item->libfs_id, item->seqn,
				    atomic_load(&item->processed),
				    &item->processed, item, tail);
#endif
#ifdef PROFILE_CIRCBUF
			// Check added time.
			clock_gettime(CLOCK_MONOTONIC, &item->time_enqueued);
#endif
			smp_store_release(
				&buffer->head,
				(head + 1) &
					(CIRC_BUF_HOST_MEMCPY_REQ_SIZE - 1));

			// We don't need to wake up consumers. We are using
			// mutex.
			/* wake_up() will make sure that the head is committed
			 * before waking anyone up */
			// wake_up(consumer);
			break;
		} else {
			// TODO include reading tail into while loop?
			print_host_memcpy_circbuf(
				buffer,
				atomic_load(&rctx->next_host_memcpy_seqn));
			panic("host memcpy circ_buf full.\n");
		}
	}
	// mlfs_printf("Enqueuing done. host_memcpy_req: libfs=%d seqn=%lu\n", hm_arg->libfs_id, hm_arg->seqn);
	END_TL_TIMER(evt_enqueue_mcpy_req);

	pthread_mutex_unlock(&buffer->produce_mutex);
}

static host_memcpy_arg *
search_next_seqn_host_memcpy_item(host_memcpy_circbuf *buffer,
				  uint64_t next_seqn)
{
	host_memcpy_arg *item;
	unsigned long i;
	unsigned long head;
	unsigned long tail;
	unsigned long cur;

	head = buffer->head;
	tail = buffer->tail;
	cur = tail;
	item = &buffer->buf[tail];

	for (i = 0; i < CIRC_CNT(head, tail, CIRC_BUF_HOST_MEMCPY_REQ_SIZE);
	     i++) {
		// mlfs_printf("HOST_MEMCPY_CIRC_BUF next_seqn=%lu cur_seqn=%lu "
		//             "head=%lu tail=%lu cur=%lu\n",
		//             next_seqn, item->seqn, head, tail, cur);

		if (item->seqn == next_seqn) {
			// mlfs_printf("Search success: HOST_MEMCPY_CIRC_BUF "
			//             "next_seqn=%lu\n",
			//             next_seqn);
			return item;
		}

		// get next item. Search from tail to head.
		cur = (cur + 1) & (CIRC_BUF_HOST_MEMCPY_REQ_SIZE - 1);
		item = &buffer->buf[cur];
	}

	return NULL;
}

/**
 * @Synopsis  Dequeue one host memcpy request entry from the circular buffer.
 *
 * @Param rctx
 * @Param hm_arg Dequeued item.
 *
 * @Returns   1 if an item is dequeued successfully. 0 when no item is dequeued.
 */
static int dequeue_host_memcpy_req(struct replication_context *rctx,
				   host_memcpy_arg *hm_arg)
{
	host_memcpy_circbuf *buffer = &rctx->host_memcpy_req_buf;
	int do_handle = 0;

	pthread_mutex_lock(&buffer->consume_mutex);

	/* Read index before reading contents at that index. */
	unsigned long head = smp_load_acquire(&buffer->head);
	unsigned long tail = buffer->tail;
	uint64_t next_seqn;

	if (CIRC_CNT(head, tail, CIRC_BUF_HOST_MEMCPY_REQ_SIZE) >= 1) {
		// Search first
		host_memcpy_arg *item = &buffer->buf[tail];
		next_seqn = atomic_load(&rctx->next_host_memcpy_seqn);
		// mlfs_printf("HOST_MEMCPY_CIRC_BUF Get item next_seqn=%lu : "
		//             "libfs=%d seqn=%lu processed=%d(%p) item=%p "
		//             "tail=%lu\n",
		//             next_seqn, item->libfs_id, item->seqn,
		//             atomic_load(&item->processed), &item->processed,
		//             item, tail);

		if (item->seqn == next_seqn) {
			// We have a proper item at the front of the queue.
			memcpy(hm_arg, item, sizeof(host_memcpy_arg));
			do_handle = 1;
#ifdef PRINT_CIRCBUF
			mlfs_printf("HOST_MEMCPY_CIRC_BUF Handle new item: "
				    "libfs=%d seqn=%lu processed=%d(%p) "
				    "item=%p\n",
				    item->libfs_id, item->seqn,
				    atomic_load(&item->processed),
				    &item->processed, item);
#endif
			/* Finish reading descriptor before incrementing tail.
			 */
			smp_store_release(
				&buffer->tail,
				(tail + 1) &
					(CIRC_BUF_HOST_MEMCPY_REQ_SIZE - 1));

		} else if (item->seqn < next_seqn) {
			// Item has already been processed.
			// mlfs_assert(atomic_load(&item->processed)); // FIXME Sometimes this assertion fails.
			int processed = atomic_load(&item->processed);
			if (!processed) {
				mlfs_printf(
					"[WARN] next_seqn is already ahead "
					"of this item but processed is not "
					"set. libfs=%d "
					"next_seqn=%lu(regain:%lu) "
					"item_seqn=%lu processed=%d(%p) "
					"item=%p tail=%lu\n",
					item->libfs_id, next_seqn,
					atomic_load(
						&rctx->next_host_memcpy_seqn),
					item->seqn,
					atomic_load(&item->processed),
					&item->processed, item, tail);
			}
#ifdef PRINT_CIRCBUF
			mlfs_printf("HOST_MEMCPY_CIRC_BUF Item already "
				    "processed: libfs=%d seqn=%lu "
				    "processed=%d(%p) item=%p tail=%lu\n",
				    item->libfs_id, item->seqn,
				    atomic_load(&item->processed),
				    &item->processed, item, tail);
#endif
			/* Finish reading descriptor before incrementing tail.
			 */
			smp_store_release(
				&buffer->tail,
				(tail + 1) &
					(CIRC_BUF_HOST_MEMCPY_REQ_SIZE - 1));
		} else {
			// Search for an item with the next seqn.
			item = search_next_seqn_host_memcpy_item(buffer,
								 next_seqn);

			if (item && !atomic_load(&item->processed)) {
				// found.
				// mark as processed. This item will be skipped
				// later.
				atomic_store(&item->processed, 1);

				memcpy(hm_arg, item, sizeof(host_memcpy_arg));
				do_handle = 1;
#ifdef PRINT_CIRCBUF
				mlfs_printf("HOST_MEMCPY_CIRC_BUF Set "
					    "processed: libfs=%d seqn=%lu "
					    "processed=%d(%p) item=%p "
					    "tail=%lu\n",
					    item->libfs_id, item->seqn,
					    atomic_load(&item->processed),
					    &item->processed, item, tail);
#endif
			}
		}

		if (do_handle) {
			uint64_t prev_seqn = atomic_fetch_add(
				&rctx->next_host_memcpy_seqn, 1);
			// mlfs_printf("Handling HOST_MEMCPY_seqn=%lu next_host_memcpy_seqn=%lu\n",
			//         prev_seqn, prev_seqn + 1);

#ifdef PROFILE_CIRCBUF
			double duration;
			clock_gettime(CLOCK_MONOTONIC, &item->time_dequeued);
			duration = get_duration(&item->time_enqueued, &item->time_dequeued);
			circ_buf_stat *cb_stat = &host_memcpy_cb_stats[rctx->peer->id];

			cb_stat->schedule_cnt++;
			cb_stat->wait_seqn_delay_sum += duration;
			if (duration > cb_stat->wait_seqn_delay_max)
				cb_stat->wait_seqn_delay_max = duration;
			if (duration < cb_stat->wait_seqn_delay_min)
				cb_stat->wait_seqn_delay_min = duration;
#endif
		}
	}

	pthread_mutex_unlock(&buffer->consume_mutex);

	return do_handle;
}

/**
 * @Synopsis  Enqueue one 'log copy done' entry to the circular buffer.
 *
 * @Param hm_arg
 */
void enqueue_copy_done_item(copy_done_arg *cd_arg)
{
	struct replication_context *rctx = g_sync_ctx[cd_arg->libfs_id];
	copy_done_circbuf *buffer = &rctx->local_copy_done_buf;

	pthread_mutex_lock(&buffer->produce_mutex);

	unsigned long head = buffer->head;
	/* The spin_unlock() and next spin_lock() provide needed ordering. */
	unsigned long tail = READ_ONCE(buffer->tail);

	// mlfs_printf("Enqueuing copy_done: libfs=%d seqn=%lu\n", cd_arg->libfs_id, cd_arg->seqn);

	START_TL_TIMER(evt_enqueue_copy_done);

	while (1) {
		if (CIRC_SPACE(head, tail, CIRC_BUF_COPY_DONE_SIZE) >= 1) {
			/* insert one item into the buffer */
			copy_done_arg *item = &buffer->buf[head];
			memcpy(item, cd_arg, sizeof(copy_done_arg));
#ifdef PRINT_CIRCBUF
			mlfs_printf("COPY_DONE_CIRC_BUF Add item libfs=%d "
				    "seqn=%lu processed=%d(%p) item=%p "
				    "tail=%lu\n",
				    item->libfs_id, item->seqn,
				    atomic_load(&item->processed),
				    &item->processed, item, tail);
#endif
#ifdef PROFILE_CIRCBUF
			// Check added time.
			clock_gettime(CLOCK_MONOTONIC, &item->time_enqueued);
#endif
			smp_store_release(
				&buffer->head,
				(head + 1) & (CIRC_BUF_COPY_DONE_SIZE - 1));

			// We don't need to wake up consumers. We are using
			// mutex.
			/* wake_up() will make sure that the head is committed
			 * before waking anyone up */
			// wake_up(consumer);
			break;
		} else {
			// TODO include reading tail into while loop?
			print_copy_done_circbuf(
				buffer,
				atomic_load(&rctx->next_copy_done_seqn));
			panic("copy done circ_buf full.\n");
		}
	}

	END_TL_TIMER(evt_enqueue_copy_done);

	pthread_mutex_unlock(&buffer->produce_mutex);
}

static copy_done_arg *search_next_seqn_copy_done_item(copy_done_circbuf *buffer,
						      uint64_t next_seqn)
{
	copy_done_arg *item;
	unsigned long i;
	unsigned long head;
	unsigned long tail;
	unsigned long cur;

	head = buffer->head;
	tail = buffer->tail;
	cur = tail;
	item = &buffer->buf[tail];

	for (i = 0; i < CIRC_CNT(head, tail, CIRC_BUF_COPY_DONE_SIZE); i++) {
		// mlfs_printf("COPY_DONE_CIRC_BUF next_seqn=%lu cur_seqn=%lu "
		//             "head=%lu tail=%lu cur=%lu\n",
		//             next_seqn, item->seqn, head, tail, cur);

		if (item->seqn == next_seqn) {
			// mlfs_printf("Search success: COPY_DONE_CIRC_BUF "
			//             "next_seqn=%lu\n",
			//             next_seqn);
			return item;
		}

		// get next item. Search from tail to head.
		cur = (cur + 1) & (CIRC_BUF_COPY_DONE_SIZE - 1);
		item = &buffer->buf[cur];
	}

	return NULL;
}

/**
 * @Synopsis  Dequeue one 'copy done' entry from the circular buffer. It does
 * not return the dequeued item because we don't need it.
 *
 * @Param rctx
 *
 * @Returns   1 if an item is dequeued successfully. 0 when no item is dequeued.
 */
static int dequeue_copy_done_item(struct replication_context *rctx)
{
	copy_done_circbuf *buffer = &rctx->local_copy_done_buf;
	int do_handle = 0;

	pthread_mutex_lock(&buffer->consume_mutex);

	/* Read index before reading contents at that index. */
	unsigned long head = smp_load_acquire(&buffer->head);
	unsigned long tail = buffer->tail;
	uint64_t next_seqn;

	if (CIRC_CNT(head, tail, CIRC_BUF_COPY_DONE_SIZE) >= 1) {
		// Search first
		copy_done_arg *item = &buffer->buf[tail];
		next_seqn = atomic_load(&rctx->next_copy_done_seqn);
		// mlfs_printf("COPY_DONE_CIRC_BUF Get item next_seqn=%lu : "
		//             "libfs=%d seqn=%lu processed=%d(%p) item=%p "
		//             "tail=%lu\n",
		//             next_seqn, item->libfs_id, item->seqn,
		//             atomic_load(&item->processed), &item->processed,
		//             item, tail);

		if (item->seqn == next_seqn) {
			// We have a proper item at the front of the queue.
			// memcpy(cd_arg, item, sizeof(copy_done_arg));
			do_handle = 1;
#ifdef PRINT_CIRCBUF
			mlfs_printf("COPY_DONE_CIRC_BUF Handle new item: "
				    "libfs=%d seqn=%lu processed=%d(%p) "
				    "item=%p\n",
				    item->libfs_id, item->seqn,
				    atomic_load(&item->processed),
				    &item->processed, item);
#endif
			/* Finish reading descriptor before incrementing tail.
			 */
			smp_store_release(
				&buffer->tail,
				(tail + 1) & (CIRC_BUF_COPY_DONE_SIZE - 1));

		} else if (item->seqn < next_seqn) {
			// Item has already been processed.
			// mlfs_assert(atomic_load(&item->processed)); // FIXME Sometimes this assertion fails.
			int processed = atomic_load(&item->processed);
			if (!processed) {
				mlfs_printf(
					"[WARN] next_seqn is already ahead "
					"of this item but processed is not "
					"set. libfs=%d "
					"next_seqn=%lu(regain:%lu) "
					"item_seqn=%lu processed=%d(%p) "
					"item=%p tail=%lu\n",
					item->libfs_id, next_seqn,
					atomic_load(&rctx->next_copy_done_seqn),
					item->seqn,
					atomic_load(&item->processed),
					&item->processed, item, tail);
			}

#ifdef PRINT_CIRCBUF
			mlfs_printf("COPY_DONE_CIRC_BUF Item already "
				    "processed: libfs=%d seqn=%lu "
				    "processed=%d(%p) item=%p tail=%lu\n",
				    item->libfs_id, item->seqn,
				    atomic_load(&item->processed),
				    &item->processed, item, tail);
#endif
			/* Finish reading descriptor before incrementing tail.
			 */
			smp_store_release(
				&buffer->tail,
				(tail + 1) & (CIRC_BUF_COPY_DONE_SIZE - 1));
		} else {
			// Search for an item with the next seqn.
			item = search_next_seqn_copy_done_item(buffer,
							       next_seqn);

			if (item && !atomic_load(&item->processed)) {
				// found.
				// mark as processed. This item will be skipped
				// later.
				atomic_store(&item->processed, 1);

				// memcpy(cd_arg, item, sizeof(copy_done_arg));
				do_handle = 1;
#ifdef PRINT_CIRCBUF
				mlfs_printf("COPY_DONE_CIRC_BUF Set "
					    "processed: libfs=%d seqn=%lu "
					    "processed=%d(%p) item=%p "
					    "tail=%lu\n",
					    item->libfs_id, item->seqn,
					    atomic_load(&item->processed),
					    &item->processed, item, tail);
#endif
			}
		}

		if (do_handle) {
			uint64_t prev_seqn = atomic_fetch_add(
				&rctx->next_copy_done_seqn, 1);
			// mlfs_printf("Handling COPY_DONE_seqn=%lu next_copy_done_seqn=%lu\n",
			//         prev_seqn, prev_seqn + 1);

#ifdef PROFILE_CIRCBUF
			double duration;
			clock_gettime(CLOCK_MONOTONIC, &item->time_dequeued);
			duration = get_duration(&item->time_enqueued, &item->time_dequeued);
			circ_buf_stat *cb_stat = &copy_done_cb_stats[rctx->peer->id];

			cb_stat->schedule_cnt++;
			cb_stat->wait_seqn_delay_sum += duration;
			if (duration > cb_stat->wait_seqn_delay_max)
				cb_stat->wait_seqn_delay_max = duration;
			if (duration < cb_stat->wait_seqn_delay_min)
				cb_stat->wait_seqn_delay_min = duration;
#endif
		}
	}

	pthread_mutex_unlock(&buffer->consume_mutex);

	return do_handle;
}

/**
 * @Synopsis  Append memcpy list to rdma meta as an sge entry.
 *
 * @Param rctx
 * @Param hm_arg
 *
 * @Returns   1 if sge list is full. Otherwise, 0.
 */
static int add_memcpy_list_to_batch(struct replication_context *rctx,
				    host_memcpy_arg *hm_arg)
{
	uint64_t size;
	rdma_meta_t *r_meta;

	r_meta = rctx->mcpy_list_batch_meta.r_meta;

	size = sizeof(memcpy_meta_t) * hm_arg->array->id; // id stores size of
							  // array. (the number
							  // of memecpy list
							  // entries)
	add_sge_entry(r_meta, (uintptr_t)hm_arg->array->buf, size);

	// Update meta.
	if (r_meta->sge_count == 1) { // First entry.
		rctx->mcpy_list_batch_meta.first_seqn = hm_arg->seqn;
		rctx->mcpy_list_batch_meta.digest_start_blknr = hm_arg->digest_start_blknr;

		rctx->mcpy_list_batch_meta.n_orig_loghdrs = hm_arg->n_orig_loghdrs;
		rctx->mcpy_list_batch_meta.n_orig_blks = hm_arg->n_orig_blks;
		rctx->mcpy_list_batch_meta.reset_meta = hm_arg->reset_meta;
	} else {
		rctx->mcpy_list_batch_meta.n_orig_loghdrs += hm_arg->n_orig_loghdrs;
		rctx->mcpy_list_batch_meta.n_orig_blks += hm_arg->n_orig_blks;

		// if already 1, just leave it.
		if (!rctx->mcpy_list_batch_meta.reset_meta)
			rctx->mcpy_list_batch_meta.reset_meta =
				hm_arg->reset_meta;
	}

	pr_digest("batch meta: batched_cnt=%d first_seqn=%lu digest_start_blknr=%lu "
		    "n_orig_loghdrs=%u n_orig_blks=%lu reset_meta=%d",
		    r_meta->sge_count,
		    rctx->mcpy_list_batch_meta.first_seqn,
		    rctx->mcpy_list_batch_meta.digest_start_blknr,
		    rctx->mcpy_list_batch_meta.n_orig_loghdrs,
		    rctx->mcpy_list_batch_meta.n_orig_blks,
		    rctx->mcpy_list_batch_meta.reset_meta);

	mlfs_free(hm_arg->array);

	mlfs_assert(r_meta->sge_count <= mlfs_conf.host_memcpy_batch_max);
	return (r_meta->sge_count == mlfs_conf.host_memcpy_batch_max);
}

static void print_memcpy_meta (memcpy_meta_t *meta, uint64_t i)
{
	printf("%lu RDMA_MEMCPY i=%4lu is_single_blk=%d to_dev=%u blk_nr=%lu "
	       "data=%p size=%d offset=%d\n",
	       get_tid(), i, meta->is_single_blk, meta->to_dev, meta->blk_nr,
	       meta->data, meta->size, meta->offset_in_blk);
}

// PCIe memcpy when host dies.
void do_rdma_memcpy(host_memcpy_arg *m_arg)
{
	int i, sockfd;
	uint64_t size;
	memcpy_meta_t *meta;
	memcpy_meta_t *meta_buf;
	rdma_meta_t *r_rdma_meta, *w_rdma_meta;
	uintptr_t remote_addr;
	char *local_buf;
	uint32_t offset_in_blk;


	size = m_arg->array->id; // id stores size of array. (the number of
				 // replay list entries)

	meta_buf = m_arg->array->buf; // get buf start.

	// mlfs_printf("RDMA MEMCPY libfs_id=%d fetch_seqn=%lu n_memcpy=%lu\n",
	// 	m_arg->libfs_id, m_arg->seqn, size);

	sockfd = g_kernfs_peers[nic_kid_to_host_kid(g_kernfs_id)]
			 ->sockfd[SOCK_BG];

#ifdef PROFILE_REALTIME_MEMCPY_BW
	uint64_t total_sent_size = 0;
#endif

	uintptr_t last_byte_addr;
	uintptr_t end_read_addr = 0; // TODO to be deleted. to test merged rdma.
	uintptr_t end_write_addr = 0; // TODO to be deleted. to test merged rdma.

	for (i = 0; i < size; i++) {
		meta = &meta_buf[i];
		// print_memcpy_meta(meta, i);

		local_buf = nic_slab_alloc_in_blk(meta->size);

		// Read from log nvm area.
		remote_addr = (uintptr_t)meta->data;
		// mlfs_printf("RDMA READ local_addr=%p remote_addr=0x%lx\n", local_buf, remote_addr);

		r_rdma_meta = create_rdma_meta((uintptr_t)local_buf, remote_addr, meta->size);
#ifndef DIGEST_MEMCPY_NO_COPY
		IBV_WRAPPER_READ_ASYNC(sockfd, r_rdma_meta, MR_DRAM_BUFFER,
				       MR_NVM_LOG);
#endif
		mlfs_free(r_rdma_meta);

		// TODO to be deleted.
		// if (remote_addr == end_read_addr + 1) {
		//         mlfs_printf("%s", "log read address is consecutive.\n");
		// }
		// end_read_addr = remote_addr + meta->size -1;


		// Write shared nvm area.
		if (meta->is_single_blk)
			offset_in_blk = meta->offset_in_blk;
		else
			offset_in_blk = 0;

		remote_addr = ((uintptr_t)g_bdev[g_root_dev]->map_base_addr) +
			      (meta->blk_nr << g_block_size_shift) +
			      offset_in_blk;
		// mlfs_printf("RDMA WRITE local_addr=%p remote_addr=0x%lx\n", local_buf, remote_addr);
		w_rdma_meta = create_rdma_meta((uintptr_t)local_buf, remote_addr, meta->size);
#ifndef DIGEST_MEMCPY_NO_COPY
		IBV_WRAPPER_WRITE_ASYNC(sockfd, w_rdma_meta, MR_DRAM_BUFFER,
					MR_NVM_SHARED);
#endif
		mlfs_free(w_rdma_meta);


#ifdef PROFILE_REALTIME_MEMCPY_BW
		total_sent_size += meta->size;
#endif
		if (i == size && mlfs_conf.persist_nvm_with_rdma_read) {
			last_byte_addr = remote_addr + meta->size - 1;
			r_rdma_meta = create_rdma_meta((uintptr_t)local_buf, last_byte_addr, 1);
			IBV_WRAPPER_READ_ASYNC(sockfd, r_rdma_meta,
					       MR_DRAM_BUFFER, MR_NVM_SHARED);
		}

		nic_slab_free(local_buf);

		// TODO to be deleted.
		// if (remote_addr == end_write_addr + 1) {
		//         mlfs_printf("%s", "log write address is consecutive.\n");
		// }
		// end_write_addr = remote_addr + meta->size - 1;
	}

	nic_slab_free(m_arg->array->buf);
	mlfs_free(m_arg->array);

#ifdef PROFILE_REALTIME_MEMCPY_BW
	check_rt_bw(&memcpy_bw_stat, total_sent_size);
#endif

	if (is_first_kernfs(m_arg->libfs_id, g_kernfs_id)) {
		mlfs_assert(0); // Currently, BACKUP_RDMA_MEMCPY implemented
				// only for Replica 1.

		// Refer to memcpy worker to implement it.
		// send_publish_ack_to_libfs(m_arg->libfs_id, m_arg->fetch_seqn,
		//                           m_arg->digest_start_blknr,
		//                           m_arg->n_digested,
		//                           m_arg->n_digested_blks,
		//                           m_arg->reset_meta);
	}
}

void request_host_memcpy(void *arg)
{
	struct replication_context *rctx =
		(struct replication_context *)arg;
	host_memcpy_arg hm_arg = { 0 };
	struct timespec dequeue_start_time;
	struct timespec dequeue_end_time;
	int seconds = 0;
	int ret;

	int libfs_id = rctx->peer->id;

	START_TL_TIMER(evt_host_memcpy_wait_req);
	clock_gettime(CLOCK_MONOTONIC, &dequeue_start_time);
	seconds = 0;

	while (!dequeue_host_memcpy_req(rctx, &hm_arg)) {
		clock_gettime(CLOCK_MONOTONIC, &dequeue_end_time);
		if (get_duration(&dequeue_start_time, &dequeue_end_time) >
		    1.0) {
			seconds++;
			mlfs_printf("Waiting for an copy_done_item for "
				    "%d seconds. libfs_id=%d "
				    "rctx->next_seqn=%lu\n",
				    seconds, libfs_id,
				    atomic_load(&rctx->next_host_memcpy_seqn));
			clock_gettime(CLOCK_MONOTONIC, &dequeue_start_time);
		}
		cpu_relax();
	}

	END_TL_TIMER(evt_host_memcpy_wait_req);

	pr_pipe("host_memcpy_req dequeued: libfs_id=%d seqn=%lu",
		hm_arg.libfs_id, hm_arg.seqn);

	if (is_first_kernfs(libfs_id, g_kernfs_id)) {
		// Done.
	} else {
		START_TL_TIMER(evt_host_memcpy_wait_copy_done);
		clock_gettime(CLOCK_MONOTONIC, &dequeue_start_time);

		// Wait for log copy done.
		//  - Replica 1: Copying to local NVM.
		//  - Replica 2: Ack of log copying done (RPC from Replica 1).
		while (!dequeue_copy_done_item(rctx)) {
			clock_gettime(CLOCK_MONOTONIC, &dequeue_end_time);
			if (get_duration(&dequeue_start_time,
					 &dequeue_end_time) > 1.0) {
				seconds++;
				mlfs_printf(
					"Waiting for an copy_done_item for %d "
					"seconds. libfs_id=%d "
					"rctx->next_seqn=%lu\n",
					seconds, libfs_id,
					atomic_load(
						&rctx->next_copy_done_seqn));
				clock_gettime(CLOCK_MONOTONIC,
					      &dequeue_start_time);
			}
			cpu_relax();
		}
		END_TL_TIMER(evt_host_memcpy_wait_copy_done);
		pr_pipe("copy_done dequeued: libfs_id=%d seqn=%lu",
				hm_arg.libfs_id, hm_arg.seqn);
	}

	START_TL_TIMER(evt_host_memcpy_req);

#ifdef BATCH_MEMCPY_LIST
	if (add_memcpy_list_to_batch(rctx, &hm_arg)) {
#ifdef BACKUP_RDMA_MEMCPY
		printf("[Warn] BACKUP RDMA MEMCPY is not implemented with memcpy batching.\n");
#endif
		// Send request once sge list is full.
		send_batched_memcpy_req_to_host(rctx);
	}
#else

#ifdef BACKUP_RDMA_MEMCPY


	// TEST
	// if (is_middle_in_rep_chain(libfs_id, g_kernfs_id)) {
	//         do_rdma_memcpy(&hm_arg);
	// } else {
	//         send_memcpy_req_to_host(&hm_arg);
	// }


	if (host_alive()) {
		ret = send_memcpy_req_to_host(&hm_arg);
		if (ret < 0) // Failed because host is not alive.
			do_rdma_memcpy(&hm_arg);
	} else { // Host is not alive.
		do_rdma_memcpy(&hm_arg);
	}
#else
	send_memcpy_req_to_host(&hm_arg);
#endif

#endif

	if (is_last_kernfs(libfs_id, g_kernfs_id)) {
		// Free loghdr buffer.
		pr_pipe("It is the last kernfs. Freeing loghdr_buf=%p",
			hm_arg.loghdr_buf);
		nic_slab_free(hm_arg.loghdr_buf);
	} else {
		// Next pipeline stage.
		pipeline_end_arg *pe_arg = (pipeline_end_arg *)mlfs_alloc(
			sizeof(pipeline_end_arg));
		pe_arg->libfs_id = rctx->peer->id;
		pe_arg->seqn = hm_arg.seqn;
		pe_arg->loghdr_buf = hm_arg.loghdr_buf;
		pe_arg->log_buf = hm_arg.log_buf;
		pe_arg->fetch_loghdr_done_p = hm_arg.fetch_loghdr_done_p;
		pe_arg->fetch_log_done_p = hm_arg.fetch_log_done_p;

		thpool_add_work(thpool_pipeline_end, end_pipeline,
				(void *)pe_arg);
	}

	// No need to free arg. rctx address is passed to arg.

	END_TL_TIMER(evt_host_memcpy_req);
}

void request_host_memcpy_without_pipelining(host_memcpy_arg *hm_arg)
{
	struct replication_context *rctx = g_sync_ctx[hm_arg->libfs_id];
	struct timespec dequeue_start_time;
	struct timespec dequeue_end_time;
	int seconds = 0;

	int libfs_id = rctx->peer->id;

	START_TL_TIMER(evt_host_memcpy_req);

	pr_pipe("host_memcpy_req libfs_id=%d seqn=%lu", hm_arg->libfs_id,
		hm_arg->seqn);

	if (is_first_kernfs(libfs_id, g_kernfs_id)) {
		// Done.
	} else {
		START_TL_TIMER(evt_host_memcpy_wait_copy_done);
		clock_gettime(CLOCK_MONOTONIC, &dequeue_start_time);

		// Wait for log copy done.
		//  - Replica 1: Copying to local NVM.
		//  - Replica 2: Ack of log copying done (RPC from Replica 1).
		while (!dequeue_copy_done_item(rctx)) {
			clock_gettime(CLOCK_MONOTONIC, &dequeue_end_time);
			if (get_duration(&dequeue_start_time,
					 &dequeue_end_time) > 1.0) {
				seconds++;
				mlfs_printf(
					"Waiting for an copy_done_item for %d "
					"seconds. libfs_id=%d "
					"rctx->next_seqn=%lu\n",
					seconds, libfs_id,
					atomic_load(
						&rctx->next_copy_done_seqn));
				clock_gettime(CLOCK_MONOTONIC,
					      &dequeue_start_time);
			}
			cpu_relax();
		}
		END_TL_TIMER(evt_host_memcpy_wait_copy_done);
		pr_pipe("copy_done dequeued: libfs_id=%d seqn=%lu",
				hm_arg->libfs_id, hm_arg->seqn);
	}

	send_memcpy_req_to_host(hm_arg);

	if (is_last_kernfs(libfs_id, g_kernfs_id)) {
		// Free loghdr buffer.
		pr_pipe("It is the last kernfs. Freeing loghdr_buf=%p", hm_arg->loghdr_buf);
		nic_slab_free(hm_arg->loghdr_buf);
	} else {
		// Next pipeline stage.
		pipeline_end_arg *pe_arg = (pipeline_end_arg *)mlfs_alloc(
			sizeof(pipeline_end_arg));
		pe_arg->libfs_id = rctx->peer->id;
		pe_arg->seqn = hm_arg->seqn;
		pe_arg->loghdr_buf = hm_arg->loghdr_buf;
		pe_arg->log_buf = hm_arg->log_buf;
		pe_arg->fetch_loghdr_done_p = hm_arg->fetch_loghdr_done_p;
		pe_arg->fetch_log_done_p = hm_arg->fetch_log_done_p;

		thpool_add_work(thpool_pipeline_end, end_pipeline,
				(void *)pe_arg);
	}

	END_TL_TIMER(evt_host_memcpy_req);
}

// It flushes all batched host memcpy requests.
void publish_all_remains(struct replication_context *rctx)
{
	send_batched_memcpy_req_to_host(rctx);
}

static void print_host_memcpy_arg(host_memcpy_arg *ar)
{
	pr_pipe("ARG %-30s libfs_id=%d seqn=%lu array->id=%lu array->buf=%p "
		"loghdr_buf=%p log_buf=%p fetch_loghdr_done_p=%p(%lu) "
		"fetch_log_done_p=%p(%lu) processed=%d digest_start_blknr=%lu "
		"n_orig_loghdrs=%u n_orig_blks=%lu reset_meta=%d",
		"host_memcpy", ar->libfs_id, ar->seqn, ar->array->id,
		ar->array->buf, ar->loghdr_buf, ar->log_buf,
		ar->fetch_loghdr_done_p,
		ar->fetch_loghdr_done_p ? *ar->fetch_loghdr_done_p : 0,
		ar->fetch_log_done_p,
		ar->fetch_log_done_p ? *ar->fetch_log_done_p : 0,
		atomic_load(&ar->processed), ar->digest_start_blknr,
		ar->n_orig_loghdrs, ar->n_orig_blks, ar->reset_meta);
}

static void print_pipeline_end_arg(pipeline_end_arg *ar)
{
	pr_pipe("ARG %-30s libfs_id=%d seqn=%lu loghdr_buf=%p log_buf=%p "
		"fetch_loghdr_done_p=%p(%lu) fetch_log_done_p=%p(%lu)",
		"pipeline_end", ar->libfs_id, ar->seqn, ar->loghdr_buf,
		ar->log_buf, ar->fetch_loghdr_done_p,
		ar->fetch_loghdr_done_p ? *ar->fetch_loghdr_done_p : 0,
		ar->fetch_log_done_p,
		ar->fetch_log_done_p ? *ar->fetch_log_done_p : 0);
}

static void print_host_memcpy_circbuf(host_memcpy_circbuf *buffer, uint64_t next_seqn)
{
	host_memcpy_arg *item;
	unsigned long i;
	unsigned long head;
	unsigned long tail;
	unsigned long cur;

	head = buffer->head;
	tail = buffer->tail;
	cur = tail;
	item = &buffer->buf[tail];

	mlfs_printf("HOST_MEMCPY_CIRC_BUF next_seqn=%lu head=%lu tail=%lu\n",
		    next_seqn, head, tail);

	for (i = 0; i < CIRC_CNT(head, tail, CIRC_BUF_HOST_MEMCPY_REQ_SIZE);
	     i++) {
		mlfs_printf("id=%lu cur_seqn=%lu\n", cur, item->seqn);

		if (item->seqn == next_seqn)
			mlfs_printf("%s", "This item matches next_seqn.\n");

		// get next item. Search from tail to head.
		cur = (cur + 1) & (CIRC_BUF_HOST_MEMCPY_REQ_SIZE - 1);
		item = &buffer->buf[cur];
	}
}

static void print_copy_done_circbuf(copy_done_circbuf *buffer, uint64_t next_seqn)
{
	copy_done_arg *item;
	unsigned long i;
	unsigned long head;
	unsigned long tail;
	unsigned long cur;

	head = buffer->head;
	tail = buffer->tail;
	cur = tail;
	item = &buffer->buf[tail];

	mlfs_printf("COPY_DONE_CIRC_BUF next_seqn=%lu head=%lu tail=%lu\n",
		    next_seqn, head, tail);

	for (i = 0; i < CIRC_CNT(head, tail, CIRC_BUF_COPY_DONE_SIZE); i++) {
		mlfs_printf("id=%lu cur_seqn=%lu\n", cur, item->seqn);

		if (item->seqn == next_seqn)
			mlfs_printf("%s", "This item matches next_seqn.\n");

		// get next item. Search from tail to head.
		cur = (cur + 1) & (CIRC_BUF_COPY_DONE_SIZE - 1);
		item = &buffer->buf[cur];
	}
}

void print_host_memcpy_thpool_stat(void)
{
#ifdef PROFILE_THPOOL
	print_profile_result(thpool_pipeline_end);
#endif
}

/** Print functions registered to each thread. **/
static void print_host_memcpy_stat(void *arg)
{
	PRINT_TL_TIMER(evt_enqueue_mcpy_req, arg);
	PRINT_TL_TIMER(evt_enqueue_copy_done, arg);
	PRINT_TL_TIMER(evt_host_memcpy_wait_req, arg);
	PRINT_TL_TIMER(evt_host_memcpy_wait_copy_done, arg);
	PRINT_TL_TIMER(evt_host_memcpy_req, arg);

	RESET_TL_TIMER(evt_enqueue_mcpy_req);
	RESET_TL_TIMER(evt_enqueue_copy_done);
	RESET_TL_TIMER(evt_host_memcpy_wait_req);
	RESET_TL_TIMER(evt_host_memcpy_wait_copy_done);
	RESET_TL_TIMER(evt_host_memcpy_req);
}

void print_pipeline_end_stat(void *arg)
{
	PRINT_TL_TIMER(evt_end_pipeline, arg);
	RESET_TL_TIMER(evt_end_pipeline);
}

// PRINT_ALL_PIPELINE_STAT_FUNC(thpool_pipeline_end)
PRINT_ALL_PIPELINE_STAT_FUNC(pipeline_end)

void print_all_thpool_host_memcpy_req_stats(void)
{
	for (int i = g_n_nodes; i < g_n_nodes + MAX_LIBFS_PROCESSES; i++) {
		if (g_sync_ctx[i]) {
			// print_per_thread_pipeline_stat(tp);
			printf("libfs_%d\n", i);
			thpool_add_work(g_sync_ctx[i]->thpool_host_memcpy_req,
					print_host_memcpy_stat,
					0); // 1 thread per libfs.
			usleep(10000); // To print in order.
		}
	}
}

void print_all_circ_buf_stats(void)
{
	printf("==========_Circbuf_Stat ==========\n");
	print_circ_buf_stat(host_memcpy_cb_stats, "host_memcpy");
	print_circ_buf_stat(copy_done_cb_stats, "copy_done");
}

static void init_cb_stats(circ_buf_stat *cb_stats)
{
	for (int i = 0; i < g_n_nodes + MAX_LIBFS_PROCESSES; i++) {
		cb_stats[i].schedule_cnt = 0;
		cb_stats[i].wait_seqn_delay_sum = 0.0;
		cb_stats[i].wait_seqn_delay_max = 0.0;
		cb_stats[i].wait_seqn_delay_min = 99999.99; // large value.
	}
}

static void print_circ_buf_stat(circ_buf_stat *cb_stat, char *cb_name)
{
	int n;
	double wait_seqn_sum = 0.0, wait_seqn_avg = 0.0, wait_seqn_max = 0.0;
	double wait_seqn_min = 99999.99; //sufficiently large value for min.
	unsigned long cnt = 0;

	if (!cb_stat)
		return;

	for (n = 0; n < g_n_nodes + MAX_LIBFS_PROCESSES; n++) {
		if (!cb_stat[n].schedule_cnt)
			continue;

		cnt += cb_stat[n].schedule_cnt;
		wait_seqn_sum += cb_stat[n].wait_seqn_delay_sum;

		if (cb_stat[n].wait_seqn_delay_max > wait_seqn_max)
			wait_seqn_max = cb_stat[n].wait_seqn_delay_max;

		if (cb_stat[n].wait_seqn_delay_min &&
		    cb_stat[n].wait_seqn_delay_min < wait_seqn_min)
			wait_seqn_min = cb_stat[n].wait_seqn_delay_min;
	}

	if (cnt) {
		wait_seqn_avg = (double)wait_seqn_sum / cnt;
		printf("Circbuf_name %30s\n", cb_name);
		printf("Total_scheduled_count %19lu\n", cnt);
		printf("Seqn_wait_avg(us) %16.4f\n", wait_seqn_avg * 1000000.0);
		printf("Seqn_wait_min(us) %16.4f\n", wait_seqn_min * 1000000.0);
		printf("Seqn_wait_max(us) %16.4f\n", wait_seqn_max * 1000000.0);
		printf("--------------------- ---------\n");
	} else {
		// printf("Thread name: rep\n");
		// printf("Total scheduled cnt: %lu\n", cnt);
		// printf("----------------------------------------------\n");
	}
}

